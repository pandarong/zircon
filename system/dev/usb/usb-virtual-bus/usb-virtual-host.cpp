// Copyright 2017 The Fuchsia Authors. All riusghts reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/usb/bus.h>
#include <ddk/protocol/usb-hci.h>
#include <ddk/protocol/usb.h>

#include <zircon/listnode.h>
#include <zircon/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "usb-virtual-bus.h"

#define CLIENT_SLOT_ID  0
#define CLIENT_HUB_ID   0
#define CLIENT_SPEED    USB_SPEED_HIGH

typedef struct usb_virtual_host {
    zx_device_t* zxdev;
    usb_virtual_bus_t* bus;
    usb_bus_interface_t bus_intf;

    fbl::Mutex lock;
    sync_completion_t completion;
    bool connected;
} usb_virtual_host_t;

static void virt_host_request_queue(void* ctx, usb_request_t* req, usb_request_complete_cb cb,
                                    void* cookie) {
    auto* host = static_cast<usb_virtual_host_t*>(ctx);
    usb_virtual_bus_host_queue(host->bus, req, cb, cookie);
}

static void virt_host_set_bus_interface(void* ctx, usb_bus_interface_t* bus_intf) {
    auto* host = static_cast<usb_virtual_host_t*>(ctx);

    if (bus_intf) {
        memcpy(&host->bus_intf, bus_intf, sizeof(host->bus_intf));

        host->lock.Acquire();
        bool connected = host->connected;
        host->lock.Release();

        if (connected) {
            usb_bus_interface_add_device(&host->bus_intf, CLIENT_SLOT_ID, CLIENT_HUB_ID,
                                         CLIENT_SPEED);
        }
    } else {
        memset(&host->bus_intf, 0, sizeof(host->bus_intf));
    }
}

static size_t virt_host_get_max_device_count(void* ctx) {
    return 1;
}

static zx_status_t virt_host_enable_ep(void* ctx, uint32_t device_id,
                                       usb_endpoint_descriptor_t* ep_desc,
                                       usb_ss_ep_comp_descriptor_t* ss_comp_desc, bool enable) {
    return ZX_OK;
}

static uint64_t virt_host_get_frame(void* ctx) {
    return 0;
}

zx_status_t virt_host_config_hub(void* ctx, uint32_t device_id, usb_speed_t speed,
                                 usb_hub_descriptor_t* descriptor) {
    return ZX_OK;
}

zx_status_t virt_host_hub_device_added(void* ctx, uint32_t hub_address, int port,
                                       usb_speed_t speed) {
    return ZX_OK;
}

zx_status_t virt_host_hub_device_removed(void* ctx, uint32_t hub_address, int port) {
    return ZX_OK;
}

zx_status_t virt_host_reset_endpoint(void* ctx, uint32_t device_id, uint8_t ep_address) {
    return ZX_ERR_NOT_SUPPORTED;
}

size_t virt_host_get_max_transfer_size(void* ctx, uint32_t device_id, uint8_t ep_address) {
    return 65536;
}

static zx_status_t virt_host_cancel_all(void* ctx, uint32_t device_id, uint8_t ep_address) {
    return ZX_ERR_NOT_SUPPORTED;
}

static zx_status_t virt_host_get_bti(void* ctx, zx_handle_t* out_bti) {
    // FIXME
    *out_bti = ZX_HANDLE_INVALID;
    return ZX_OK;
}

static size_t virt_host_get_request_size(void* ctx) {
    return sizeof(usb_request_t) + sizeof(virt_usb_req_internal_t);
}

static usb_hci_protocol_ops_t virtual_host_protocol = {
    .request_queue = virt_host_request_queue,
    .set_bus_interface = virt_host_set_bus_interface,
    .get_max_device_count = virt_host_get_max_device_count,
    .enable_endpoint = virt_host_enable_ep,
    .get_current_frame = virt_host_get_frame,
    .configure_hub = virt_host_config_hub,
    .hub_device_added = virt_host_hub_device_added,
    .hub_device_removed = virt_host_hub_device_removed,
    .reset_endpoint = virt_host_reset_endpoint,
    .get_max_transfer_size = virt_host_get_max_transfer_size,
    .cancel_all = virt_host_cancel_all,
    .get_bti = virt_host_get_bti,
    .get_request_size = virt_host_get_request_size,
};

static void virt_host_unbind(void* ctx) {
    printf("virt_host_unbind\n");
    auto* host = static_cast<usb_virtual_host_t*>(ctx);

    device_remove(host->zxdev);
}

static void virt_host_release(void* ctx) {
    free(ctx);
}

static zx_protocol_device_t virt_host_device_proto = [](){
    zx_protocol_device_t proto;
    proto.version = DEVICE_OPS_VERSION;
    proto.unbind = virt_host_unbind;
    proto.release = virt_host_release;
    return proto;
}();

zx_status_t usb_virtual_host_add(usb_virtual_bus_t* bus, usb_virtual_host_t** out_host) {
    auto* host = static_cast<usb_virtual_host_t*>(calloc(1, sizeof(usb_virtual_host_t)));
    if (!host) {
        return ZX_ERR_NO_MEMORY;
    }

    sync_completion_reset(&host->completion);
    host->bus = bus;

    device_add_args_t args = {};
    args.version = DEVICE_ADD_ARGS_VERSION;
    args.name = "usb-virtual-host";
    args.ctx = host;
    args.ops = &virt_host_device_proto;
    args.proto_id = ZX_PROTOCOL_USB_HCI;
    args.proto_ops = &virtual_host_protocol;

    zx_status_t status = device_add(host->bus->zxdev, &args, &host->zxdev);
    if (status != ZX_OK) {
        free(host);
        return status;
    }

    *out_host = host;
    return ZX_OK;
}

void usb_virtual_host_release(usb_virtual_host_t* host) {
    device_remove(host->zxdev);
}

void usb_virtual_host_set_connected(usb_virtual_host_t* host, bool connected) {
    host->lock.Acquire();
    bool connect = connected && !host->connected;
    bool disconnect = !connected && host->connected;
    host->connected = connected;
    host->lock.Release();

    if (host->bus_intf.ops) {
        if (connect) {
            usb_bus_interface_add_device(&host->bus_intf, CLIENT_SLOT_ID, CLIENT_HUB_ID, CLIENT_SPEED);
        } else if (disconnect) {
            usb_bus_interface_remove_device(&host->bus_intf, CLIENT_SLOT_ID);
        }
    }
}
