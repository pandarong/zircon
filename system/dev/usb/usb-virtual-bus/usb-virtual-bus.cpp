// Copyright 2017 The Fuchsia Authors. All riusghts reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/binding.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/usb.h>
#include <ddk/protocol/usb-dci.h>
#include <ddk/protocol/usb-function.h>
#include <fbl/auto_lock.h>
#include <fuchsia/usb/virtualbus/c/fidl.h>
#include <stdlib.h>
#include <stdio.h>

#include "usb-virtual-bus.h"

// for mapping bEndpointAddress value to/from index in range 0 - 31
// OUT endpoints are in range 1 - 15, IN endpoints are in range 17 - 31
#define ep_address_to_index(addr) (uint8_t)(((addr) & 0xF) | (((addr) & 0x80) >> 3))
#define ep_index_to_address(index) (uint8_t)(((index) & 0xF) | (((index) & 0x10) << 3))
#define OUT_EP_START    1
#define OUT_EP_END      15
#define IN_EP_START     17
#define IN_EP_END       31

static int usb_virtual_bus_thread(void* arg) {
    auto* bus = static_cast<usb_virtual_bus_t*>(arg);

    // FIXME how to exit this thread
    while (1) {
        sync_completion_wait(&bus->completion, ZX_TIME_INFINITE);
        sync_completion_reset(&bus->completion);

        fbl::AutoLock lock(&bus->lock);

        // special case endpoint zero
        virt_usb_req_internal_t* req_int;
        req_int = list_remove_head_type(&bus->eps[0].host_reqs, virt_usb_req_internal_t, node);
        if (req_int) {
            usb_virtual_device_control(bus->device, INTERNAL_TO_USB_REQ(req_int));
        }

        for (unsigned i = 1; i < USB_MAX_EPS; i++) {
            usb_virtual_ep_t* ep = &bus->eps[i];
            bool out = (i < IN_EP_START);

            while ((req_int = list_peek_head_type(&ep->host_reqs, virt_usb_req_internal_t, node))
                    != nullptr) {
                virt_usb_req_internal_t* device_req_int;
                device_req_int = list_remove_head_type(&ep->device_reqs, virt_usb_req_internal_t, node);

                if (device_req_int) {
                    usb_request_t* req = INTERNAL_TO_USB_REQ(req_int);
                    usb_request_t* device_req = INTERNAL_TO_USB_REQ(device_req_int);
                    zx_off_t offset = ep->req_offset;
                    size_t length = req->header.length - offset;
                    if (length > device_req->header.length) {
                        length = device_req->header.length;
                    }

                    void* device_buffer;
                    usb_request_mmap(device_req, &device_buffer);

                    if (out) {
                        usb_request_copy_from(req, device_buffer, length, offset);
                    } else {
                        usb_request_copy_to(req, device_buffer, length, offset);
                    }
                    usb_request_complete(device_req, ZX_OK, length, device_req_int->complete_cb,
                                         device_req_int->cookie);

                    offset += length;
                    if (offset < req->header.length) {
                        ep->req_offset = offset;
                    } else {
                        list_delete(&req_int->node);
                        usb_request_t* req = INTERNAL_TO_USB_REQ(req_int);
                        usb_request_complete(req, ZX_OK, length, req_int->complete_cb, req_int->cookie);
                        ep->req_offset = 0;
                    }
                } else {
                    break;
                }
            }
        }
    }
    return 0;
}

zx_status_t usb_virtual_bus_set_stall(usb_virtual_bus_t* bus, uint8_t ep_address, bool stall) {
    uint8_t index = ep_address_to_index(ep_address);
    if (index >= USB_MAX_EPS) {
        return ZX_ERR_INVALID_ARGS;
    }

    bus->lock.Acquire();
    usb_virtual_ep_t* ep = &bus->eps[index];
    ep->stalled = stall;

    virt_usb_req_internal_t* req_int = nullptr;
    if (stall) {
        req_int = list_remove_head_type(&ep->host_reqs, virt_usb_req_internal_t, node);
    }
    bus->lock.Release();

    if (req_int) {
        usb_request_t* req = INTERNAL_TO_USB_REQ(req_int);
        usb_request_complete(req, ZX_ERR_IO_REFUSED, 0, req_int->complete_cb, req_int->cookie);
    }

    return ZX_OK;
}

void usb_virtual_bus_device_queue(usb_virtual_bus_t* bus, usb_request_t* req,
                                  usb_request_complete_cb cb, void* cookie) {
    auto* req_int = USB_REQ_TO_INTERNAL(req);
    req_int->complete_cb = cb;
    req_int->cookie = cookie;

    uint8_t index = ep_address_to_index(req->header.ep_address);
    if (index == 0 || index >= USB_MAX_EPS) {
        printf("usb_virtual_bus_device_queue bad endpoint %u\n", req->header.ep_address);
        usb_request_complete(req, ZX_ERR_INVALID_ARGS, 0, cb, cookie);
        return;
    }

    usb_virtual_ep_t* ep = &bus->eps[index];

    list_add_tail(&ep->device_reqs, &req_int->node);

    sync_completion_signal(&bus->completion);
}

void usb_virtual_bus_host_queue(usb_virtual_bus_t* bus, usb_request_t* req,
                                usb_request_complete_cb cb, void* cookie) {
    auto* req_int = USB_REQ_TO_INTERNAL(req);
    req_int->complete_cb = cb;
    req_int->cookie = cookie;

    uint8_t index = ep_address_to_index(req->header.ep_address);
    if (index >= USB_MAX_EPS) {
        printf("usb_virtual_bus_host_queue bad endpoint %u\n", req->header.ep_address);
        usb_request_complete(req, ZX_ERR_INVALID_ARGS, 0, cb, cookie);
        return;
    }

    usb_virtual_ep_t* ep = &bus->eps[index];

    if (ep->stalled) {
        usb_request_complete(req, ZX_ERR_IO_REFUSED, 0, cb, cookie);
        return;
    }
    list_add_tail(&ep->host_reqs, &req_int->node);

    sync_completion_signal(&bus->completion);
}

static zx_status_t usb_bus_enable(void* ctx, fidl_txn_t* txn) {
    auto* bus = static_cast<usb_virtual_bus_t*>(ctx);

    fbl::AutoLock lock(&bus->lock);

    zx_status_t status = ZX_OK;
    if (!bus->host) {
        status = usb_virtual_host_add(bus, &bus->host);
        if (status != ZX_OK) {
            return status;
        }
    }
    if (status == ZX_OK && !bus->device) {
        auto status = usb_virtual_device_add(bus, &bus->device);
        if (status != ZX_OK) {
            return status;
        }
    }

    return fuchsia_usb_virtualbus_BusEnable_reply(txn, status);
}

static zx_status_t usb_bus_disable(void* ctx, fidl_txn_t* txn) {
    auto* bus = static_cast<usb_virtual_bus_t*>(ctx);

    fbl::AutoLock lock(&bus->lock);

    if (bus->host) {
        usb_virtual_host_release(bus->host);
        bus->host = nullptr;
    }
    if (bus->device) {
        usb_virtual_device_release(bus->device);
        bus->device = nullptr;
    }

    return fuchsia_usb_virtualbus_BusDisable_reply(txn, ZX_OK);
}

static zx_status_t usb_bus_connect(void* ctx, fidl_txn_t* txn) {
    auto* bus = static_cast<usb_virtual_bus_t*>(ctx);

    fbl::AutoLock lock(&bus->lock);

    if (!bus->host || !bus->device) {
        return fuchsia_usb_virtualbus_BusConnect_reply(txn, ZX_ERR_BAD_STATE);
    }

    usb_virtual_host_set_connected(bus->host, true);

    return fuchsia_usb_virtualbus_BusConnect_reply(txn, ZX_OK);
}

static zx_status_t usb_bus_disconnect(void* ctx, fidl_txn_t* txn) {
    auto* bus = static_cast<usb_virtual_bus_t*>(ctx);

    fbl::AutoLock lock(&bus->lock);

    if (!bus->host || !bus->device) {
        return fuchsia_usb_virtualbus_BusConnect_reply(txn, ZX_ERR_BAD_STATE);
    }

    usb_virtual_host_set_connected(bus->host, false);

    return fuchsia_usb_virtualbus_BusConnect_reply(txn, ZX_OK);
}

static fuchsia_usb_virtualbus_Bus_ops_t fidl_ops = {
    .Enable = usb_bus_enable,
    .Disable = usb_bus_disable,
    .Connect = usb_bus_connect,
    .Disconnect = usb_bus_disconnect,
};

static zx_status_t usb_bus_message(void* ctx, fidl_msg_t* msg, fidl_txn_t* txn) {
    return fuchsia_usb_virtualbus_Bus_dispatch(ctx, txn, msg, &fidl_ops);
}

static void usb_bus_unbind(void* ctx) {
    auto* bus = static_cast<usb_virtual_bus_t*>(ctx);
    device_remove(bus->zxdev);
}

static void usb_bus_release(void* ctx) {
    auto* bus = static_cast<usb_virtual_bus_t*>(ctx);
    free(bus);
}

static zx_protocol_device_t usb_virtual_bus_proto = [](){
    zx_protocol_device_t proto;
    proto.version = DEVICE_OPS_VERSION;
    proto.message = usb_bus_message;
    proto.unbind = usb_bus_unbind;
    proto.release = usb_bus_release;
    return proto;
}();

static zx_status_t usb_virtual_bus_bind(void* ctx, zx_device_t* parent) {
printf("usb_virtual_bus_bind\n");
    auto* bus = static_cast<usb_virtual_bus_t*>(calloc(1, sizeof(usb_virtual_bus_t)));
    if (!bus) {
        return ZX_ERR_NO_MEMORY;
    }

    for (unsigned i = 0; i < USB_MAX_EPS; i++) {
        usb_virtual_ep_t* ep = &bus->eps[i];
        list_initialize(&ep->host_reqs);
        list_initialize(&ep->device_reqs);
    }
    sync_completion_reset(&bus->completion);

    device_add_args_t args = {};
    args.version = DEVICE_ADD_ARGS_VERSION;
    args.name = "usb-virtual-bus";
    args.ctx = bus;
    args.ops = &usb_virtual_bus_proto;
    args.flags = DEVICE_ADD_NON_BINDABLE;

    zx_status_t status = device_add(parent, &args, &bus->zxdev);
    if (status != ZX_OK) {
        free(bus);
        return status;
    }

    thrd_t thread;
    thrd_create_with_name(&thread, usb_virtual_bus_thread, bus, "usb-virtual-bus-thread");
    thrd_detach(thread);


    return ZX_OK;
}

static zx_driver_ops_t bus_driver_ops = [](){
    zx_driver_ops_t ops;
    ops.version = DRIVER_OPS_VERSION;
    ops.bind = usb_virtual_bus_bind;
    return ops;
}();

// clang-format off
ZIRCON_DRIVER_BEGIN(usb_virtual_bus, bus_driver_ops, "zircon", "0.1", 1)
    BI_MATCH_IF(EQ, BIND_PROTOCOL, ZX_PROTOCOL_MISC_PARENT),
ZIRCON_DRIVER_END(usb_virtual_bus)
