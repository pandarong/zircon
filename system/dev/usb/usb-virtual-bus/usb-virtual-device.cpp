// Copyright 2017 The Fuchsia Authors. All riusghts reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/usb.h>
#include <ddk/protocol/usb-dci.h>
#include <ddk/protocol/usb-function.h>

#include <zircon/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "usb-virtual-bus.h"

typedef struct usb_virtual_device {
    zx_device_t* zxdev;
    usb_virtual_bus_t* bus;
    usb_dci_interface_t dci_intf;
} usb_virtual_device_t;

void usb_virtual_device_control(usb_virtual_device_t* device, usb_request_t* req) {
    usb_setup_t* setup = &req->setup;
    zx_status_t status;
    size_t length = le16toh(setup->wLength);
    size_t actual = 0;

//    printf("usb_virtual_device_control type: 0x%02X req: %d value: %d index: %d length: %zu\n",
//           setup->bmRequestType, setup->bRequest, le16toh(setup->wValue), le16toh(setup->wIndex),
//           length);

    if (device->dci_intf.ops) {
        void* buffer = nullptr;

        if (length > 0) {
            usb_request_mmap(req, &buffer);
        }

        if ((setup->bmRequestType & USB_ENDPOINT_DIR_MASK) == USB_ENDPOINT_IN) {
            status = usb_dci_interface_control(&device->dci_intf, setup, nullptr, 0, buffer, length,
                                               &actual);
        } else {
            status = usb_dci_interface_control(&device->dci_intf, setup, buffer, length, nullptr, 0,
                                               nullptr);
        }
    } else {
        status = ZX_ERR_UNAVAILABLE;
    }

    auto* req_int = USB_REQ_TO_INTERNAL(req);
    usb_request_complete(req, status, actual, req_int->complete_cb, req_int->cookie);
}

static void device_request_queue(void* ctx, usb_request_t* req, usb_request_complete_cb cb,
                                 void* cookie) {
    auto* device = static_cast<usb_virtual_device_t*>(ctx);
    usb_virtual_bus_device_queue(device->bus, req, cb, cookie);
}

static zx_status_t device_set_interface(void* ctx, const usb_dci_interface_t* dci_intf) {
    auto* device = static_cast<usb_virtual_device_t*>(ctx);
    memcpy(&device->dci_intf, dci_intf, sizeof(device->dci_intf));
    return ZX_OK;
}

static zx_status_t device_config_ep(void* ctx, const usb_endpoint_descriptor_t* ep_desc,
                                    const usb_ss_ep_comp_descriptor_t* ss_comp_desc) {
    return ZX_OK;
}

static zx_status_t device_disable_ep(void* ctx, uint8_t ep_addr) {
    return ZX_OK;
}

static zx_status_t device_ep_set_stall(void* ctx, uint8_t ep_address) {
    auto* device = static_cast<usb_virtual_device_t*>(ctx);
    return usb_virtual_bus_set_stall(device->bus, ep_address, true);
}

static zx_status_t device_ep_clear_stall(void* ctx, uint8_t ep_address) {
    auto* device = static_cast<usb_virtual_device_t*>(ctx);
    return usb_virtual_bus_set_stall(device->bus, ep_address, false);
}

static zx_status_t device_get_bti(void* ctx, zx_handle_t* out_bti) {
    // FIXME
    *out_bti = ZX_HANDLE_INVALID;
    return ZX_OK;
}

static size_t device_get_request_size(void* ctx) {
    return sizeof(usb_request_t) + sizeof(virt_usb_req_internal_t);
}

usb_dci_protocol_ops_t virt_device_dci_protocol = {
    .request_queue = device_request_queue,
    .set_interface = device_set_interface,
    .config_ep = device_config_ep,
    .disable_ep = device_disable_ep,
    .ep_set_stall = device_ep_set_stall,
    .ep_clear_stall = device_ep_clear_stall,
    .get_bti = device_get_bti,
    .get_request_size = device_get_request_size,
};

static zx_status_t virt_device_open(void* ctx, zx_device_t** dev_out, uint32_t flags) {
    return ZX_OK;
}

static void virt_device_unbind(void* ctx) {
    auto* device = static_cast<usb_virtual_device_t*>(ctx);
    device_remove(device->zxdev);
}

static void virt_device_release(void* ctx) {
    auto* device = static_cast<usb_virtual_device_t*>(ctx);
    free(device);
}

static zx_protocol_device_t usb_virtual_device_device_proto = [](){
    zx_protocol_device_t proto;
    proto.version = DEVICE_OPS_VERSION;
    proto.open = virt_device_open;
    proto.unbind = virt_device_unbind;
    proto.release = virt_device_release;
    return proto;
}();

zx_status_t usb_virtual_device_add(usb_virtual_bus_t* bus, usb_virtual_device_t** out_device) {
    auto* device = static_cast<usb_virtual_device_t*>(calloc(1, sizeof(usb_virtual_device_t)));
    if (!device) {
        return ZX_ERR_NO_MEMORY;
    }
    device->bus = bus;

    device_add_args_t args = {};
    args.version = DEVICE_ADD_ARGS_VERSION;
    args.name = "usb-virtual-device";
    args.ctx = device;
    args.ops = &usb_virtual_device_device_proto;
    args.proto_id = ZX_PROTOCOL_USB_DCI;
    args.proto_ops = &virt_device_dci_protocol;

    zx_status_t status = device_add(device->bus->zxdev, &args, &device->zxdev);

    if (status != ZX_OK) {
        free(device);
        return status;
    }

    *out_device = device;
    return ZX_OK;
}

void usb_virtual_device_release(usb_virtual_device_t* device) {
    device_remove(device->zxdev);
}
