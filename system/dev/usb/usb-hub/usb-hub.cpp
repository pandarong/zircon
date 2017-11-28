// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/usb.h>
#include <ddk/protocol/usb-bus.h>
#include <ddk/usb-request.h>
#include <driver/usb.h>
#include <zircon/hw/usb-hub.h>
#include <zx/time.h>

#include <sync/completion.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "usb-hub.h"

namespace usb {
namespace hub {

UsbHub::UsbHub(zx_device_t* device) : ddk::Device<UsbHub, ddk::Unbindable>(device) {
}

UsbHub::~UsbHub() {
    if (status_request_) {
        usb_request_release(status_request_);
    }
    free(port_status_);
}

zx_status_t UsbHub::Init() {
    zx_status_t status = device_get_protocol(parent_, ZX_PROTOCOL_USB,
                                             reinterpret_cast<void*>(&usb_));
    if (status != ZX_OK) {
        return status;
    }

    // search for the bus device
    zx_device_t* bus_device = device_get_parent(parent_);
    while (bus_device && !bus_.ops) {
        if (device_get_protocol(bus_device, ZX_PROTOCOL_USB_BUS, &bus_) == ZX_OK) {
            break;
        }
        bus_device = device_get_parent(bus_device);
    }
    if (!bus_device || !bus_.ops) {
        zxlogf(ERROR, "usb_hub_bind could not find bus device\n");
        return ZX_ERR_NOT_SUPPORTED;
    }

    // find our interrupt endpoint
    usb_desc_iter_t iter;
    status = usb_desc_iter_init(&usb_, &iter);
    if (status < 0) return status;

    usb_interface_descriptor_t* intf = usb_desc_iter_next_interface(&iter, true);
    if (!intf || intf->bNumEndpoints != 1) {
        usb_desc_iter_release(&iter);
        return ZX_ERR_NOT_SUPPORTED;
    }

    uint8_t ep_addr = 0;
    uint16_t max_packet_size = 0;
    usb_endpoint_descriptor_t* endp = usb_desc_iter_next_endpoint(&iter);
    if (endp && usb_ep_type(endp) == USB_ENDPOINT_INTERRUPT) {
        ep_addr = endp->bEndpointAddress;
        max_packet_size = usb_ep_max_packet(endp);
    }
    usb_desc_iter_release(&iter);

    if (!ep_addr) {
        return ZX_ERR_NOT_SUPPORTED;
    }

    hub_speed_ = usb_get_speed(&usb_);
    bus_device_ = bus_device;

    usb_request_t* req;
    status = usb_request_alloc(&req, max_packet_size, ep_addr);
    if (status != ZX_OK) {
        return status;
    }
    req->complete_cb = InterruptComplete;
    req->cookie = this;
    status_request_ = req;

    return ZX_OK;
}

zx_status_t UsbHub::StartThread() {
    int ret = thrd_create_with_name(&thread_, ThreadEntry, reinterpret_cast<void*>(this),
                                    "usb_hub_thread");
    if (ret != thrd_success) {
        return ZX_ERR_NO_MEMORY;
    }
    return ZX_OK;
}

int UsbHub::ThreadEntry(void* arg) {
    UsbHub* hub = reinterpret_cast<UsbHub*>(arg);
    hub->Thread();
    return 0;
}

void UsbHub::Thread() {
    usb_request_t* req = status_request_;
    usb_hub_descriptor_t desc;
    size_t out_length;
    size_t min_length;
    size_t max_length;

    uint16_t desc_type = (hub_speed_ == USB_SPEED_SUPER ? USB_HUB_DESC_TYPE_SS : USB_HUB_DESC_TYPE);
    zx_status_t result = usb_get_descriptor(&usb_, USB_TYPE_CLASS | USB_RECIP_DEVICE,
                                            desc_type, 0, &desc, sizeof(desc), ZX_TIME_INFINITE,
                                            &out_length);
    if (result < 0) {
        zxlogf(ERROR, "get hub descriptor failed: %d\n", result);
        goto fail;
    }
    // The length of the descriptor varies depending on whether it is USB 2.0 or 3.0,
    // and how many ports it has.
    min_length = 7;
    max_length = sizeof(desc);
    if (out_length < min_length || out_length > max_length) {
        zxlogf(ERROR, "get hub descriptor got length %lu, want length between %lu and %lu\n",
                out_length, min_length, max_length);
        result = ZX_ERR_BAD_STATE;
        goto fail;
    }

    result = usb_bus_configure_hub(&bus_, parent_, hub_speed_, &desc);
    if (result < 0) {
        zxlogf(ERROR, "configure_hub failed: %d\n", result);
        goto fail;
    }

    num_ports_ = desc.bNbrPorts;
    port_status_ = (port_status_t *)calloc(num_ports_ + 1, sizeof(port_status_t));
    if (!port_status_) {
        result = ZX_ERR_NO_MEMORY;
        goto fail;
    }

    // power on delay in microseconds
    power_on_delay_ = desc.bPowerOn2PwrGood * 2 * 1000;
    if (power_on_delay_ < 100 * 1000) {
        // USB 2.0 spec section 9.1.2 recommends atleast 100ms delay after power on
        power_on_delay_ = 100 * 1000;
    }

    for (port_t i = 1; i <= num_ports_; i++) {
        PowerOnPort(i);
    }

    DdkMakeVisible();

    // bit field for port status bits
    uint8_t status_buf[128 / 8];
    memset(status_buf, 0, sizeof(status_buf));

    // This loop handles events from our interrupt endpoint
    while (1) {
        completion_reset(&completion_);
        usb_request_queue(&usb_, req);
        completion_wait(&completion_, ZX_TIME_INFINITE);
        if (req->response.status != ZX_OK || thread_done_) {
            break;
        }

        usb_request_copyfrom(req, status_buf, req->response.actual, 0);
        uint8_t* bitmap = status_buf;
        uint8_t* bitmap_end = bitmap + req->response.actual;

        // bit zero is hub status
        if (bitmap[0] & 1) {
            // what to do here?
            zxlogf(ERROR, "usb_hub_interrupt_complete hub status changed\n");
        }

        port_t port = 1;
        int bit = 1;
        while (bitmap < bitmap_end && port <= num_ports_) {
            if (*bitmap & (1 << bit)) {
                port_status_t status;
                zx_status_t result = GetPortStatus(port, &status);
                if (result == ZX_OK) {
                    HandlePortStatus(port, status);
                }
            }
            port++;
            if (++bit == 8) {
                bitmap++;
                bit = 0;
            }
        }
    }

    return;

fail:
    DdkRemove();
}

zx_status_t UsbHub::GetPortStatus(port_t port, port_status_t* out_status) {
    usb_port_status_t status;

    size_t out_length;
    zx_status_t result = usb_get_status(&usb_, USB_RECIP_PORT, port, &status, sizeof(status),
                                        ZX_TIME_INFINITE, &out_length);
    if (result != ZX_OK) {
        return result;
    }
    if (out_length != sizeof(status)) {
        return ZX_ERR_BAD_STATE;
    }

    zxlogf(TRACE, "UsbHub::GetPortStatus port %d ", port);

    uint16_t port_change = status.wPortChange;
    if (port_change & USB_C_PORT_CONNECTION) {
        zxlogf(TRACE, "USB_C_PORT_CONNECTION ");
        usb_clear_feature(&usb_, USB_RECIP_PORT, USB_FEATURE_C_PORT_CONNECTION, port,
                          ZX_TIME_INFINITE);
    }
    if (port_change & USB_C_PORT_ENABLE) {
        zxlogf(TRACE, "USB_C_PORT_ENABLE ");
        usb_clear_feature(&usb_, USB_RECIP_PORT, USB_FEATURE_C_PORT_ENABLE, port,
                          ZX_TIME_INFINITE);
    }
    if (port_change & USB_C_PORT_SUSPEND) {
        zxlogf(TRACE, "USB_C_PORT_SUSPEND ");
        usb_clear_feature(&usb_, USB_RECIP_PORT, USB_FEATURE_C_PORT_SUSPEND, port,
                          ZX_TIME_INFINITE);
    }
    if (port_change & USB_C_PORT_OVER_CURRENT) {
        zxlogf(TRACE, "USB_C_PORT_OVER_CURRENT ");
        usb_clear_feature(&usb_, USB_RECIP_PORT, USB_FEATURE_C_PORT_OVER_CURRENT, port,
                          ZX_TIME_INFINITE);
    }
    if (port_change & USB_C_PORT_RESET) {
        zxlogf(TRACE, "USB_C_PORT_RESET");
        usb_clear_feature(&usb_, USB_RECIP_PORT, USB_FEATURE_C_PORT_RESET, port,
                          ZX_TIME_INFINITE);
    }
    if (port_change & USB_C_BH_PORT_RESET) {
        zxlogf(TRACE, "USB_C_BH_PORT_RESET");
        usb_clear_feature(&usb_, USB_RECIP_PORT, USB_FEATURE_C_BH_PORT_RESET, port,
                          ZX_TIME_INFINITE);
    }
    if (port_change & USB_C_PORT_LINK_STATE) {
        zxlogf(TRACE, "USB_C_PORT_LINK_STATE");
        usb_clear_feature(&usb_, USB_RECIP_PORT, USB_FEATURE_C_PORT_LINK_STATE, port,
                          ZX_TIME_INFINITE);
    }
    if (port_change & USB_C_PORT_CONFIG_ERROR) {
        zxlogf(TRACE, "USB_C_PORT_CONFIG_ERROR");
        usb_clear_feature(&usb_, USB_RECIP_PORT, USB_FEATURE_C_PORT_CONFIG_ERROR, port,
                          ZX_TIME_INFINITE);
    }
    zxlogf(TRACE, "\n");

    *out_status = status.wPortStatus;
    return ZX_OK;
}

zx_status_t UsbHub::WaitForPort(port_t port, port_status_t* out_status, port_status_t status_bits,
                                port_status_t status_mask, zx_time_t stable_time) {
    const zx_time_t timeout = ZX_SEC(2);        // 2 second total timeout
    const zx_time_t poll_delay = ZX_MSEC(25);   // poll every 25 milliseconds
    zx_time_t total = 0;
    zx_time_t stable = 0;

    while (total < timeout) {
        zx_nanosleep(zx_deadline_after(poll_delay));
        total += poll_delay;

        zx_status_t result = GetPortStatus(port, out_status);
        if (result != ZX_OK) {
            return result;
        }
        port_status_[port] = *out_status;

        if ((*out_status & status_mask) == status_bits) {
            stable += poll_delay;
            if (stable >= stable_time) {
                return ZX_OK;
            }
        } else {
            stable = 0;
        }
    }

    return ZX_ERR_TIMED_OUT;
}

void UsbHub::InterruptComplete(usb_request_t* request, void* cookie) {
    zxlogf(TRACE, "UsbHub::InterruptComplete got %d %" PRIu64 "\n", request->response.status,
           request->response.actual);
    auto hub = static_cast<UsbHub*>(cookie);
    completion_signal(&hub->completion_);
}

void UsbHub::PowerOnPort(port_t port) {
    usb_set_feature(&usb_, USB_RECIP_PORT, USB_FEATURE_PORT_POWER, port, ZX_TIME_INFINITE);

    zx::nanosleep(zx::deadline_after(ZX_USEC(power_on_delay_)));
}

void UsbHub::PortEnabled(port_t port) {
    port_status_t status;

    zxlogf(TRACE, "UsbHub::PortEnabled: port %u\n", port);

    // USB 2.0 spec section 9.1.2 recommends 100ms delay before enumerating
    // wait for USB_PORT_ENABLE == 1 and USB_PORT_RESET == 0
    if (WaitForPort(port, &status, USB_PORT_ENABLE, USB_PORT_ENABLE | USB_PORT_RESET,
                    ZX_MSEC(100)) != ZX_OK) {
        zxlogf(ERROR, "UsbHub::WaitForPort USB_PORT_RESET failed for USB hub, port %d\n", port);
        return;
    }

    usb_speed_t speed;
    if (hub_speed_ == USB_SPEED_SUPER) {
        speed = USB_SPEED_SUPER;
    } else if (status & USB_PORT_LOW_SPEED) {
        speed = USB_SPEED_LOW;
    } else if (status & USB_PORT_HIGH_SPEED) {
        speed = USB_SPEED_HIGH;
    } else {
        speed = USB_SPEED_FULL;
    }

    zxlogf(TRACE, "call hub_device_added for port %d\n", port);
    usb_bus_hub_device_added(&bus_, parent_, port, speed);
    SetPortAttached(port, true);
}

void UsbHub::PortConnected(port_t port) {
    port_status_t status;

    zxlogf(TRACE, "port %d UsbHub::PortConnected\n", port);

    // USB 2.0 spec section 7.1.7.3 recommends 100ms between connect and reset
    if (WaitForPort(port, &status, USB_PORT_CONNECTION, USB_PORT_CONNECTION,
                              ZX_MSEC(100)) != ZX_OK) {
        zxlogf(ERROR, "usb_hub_wait_for_port USB_PORT_CONNECTION failed for USB hub, port %d\n", port);
        return;
    }

    usb_set_feature(&usb_, USB_RECIP_PORT, USB_FEATURE_PORT_RESET, port, ZX_TIME_INFINITE);
    PortEnabled(port);
}

void UsbHub::PortDisconnected(port_t port) {
    zxlogf(TRACE, "port %d UsbHub::PortDisconnected\n", port);
    usb_bus_hub_device_removed(&bus_, parent_, port);
    SetPortAttached(port, false);
}

void UsbHub::HandlePortStatus(port_t port, port_status_t status) {
    port_status_t old_status = port_status_[port];

    zxlogf(TRACE, "usb_hub_handle_port_status port: %d status: %04X old_status: %04X\n", port, status,
            old_status);

    port_status_[port] = status;

    if ((status & USB_PORT_CONNECTION) && !(status & USB_PORT_ENABLE)) {
        // Handle race condition where device is quickly disconnected and reconnected.
        // This happens when Android devices switch USB configurations.
        // In this case, any change to the connect state should trigger a disconnect
        // before handling a connect event.
        if (IsPortAttached(port)) {
            PortDisconnected(port);
            old_status &= (port_status_t)~USB_PORT_CONNECTION;
        }
    }
    if ((status & USB_PORT_CONNECTION) && !(old_status & USB_PORT_CONNECTION)) {
        PortConnected(port);
    } else if (!(status & USB_PORT_CONNECTION) && (old_status & USB_PORT_CONNECTION)) {
        PortDisconnected(port);
    } else if ((status & USB_PORT_ENABLE) && !(old_status & USB_PORT_ENABLE)) {
        PortEnabled(port);
    }
}

void UsbHub::DdkUnbind() {
    for (port_t port = 1; port <= num_ports_; port++) {
        if (IsPortAttached(port)) {
            PortDisconnected(port);
        }
    }
    DdkRemove();
}

void UsbHub::DdkRelease() {
    thread_done_ = true;
    completion_signal(&completion_);
    thrd_join(thread_, nullptr);
    delete this;
}

}  // namespace hub
}  // namespace usb

extern "C" zx_status_t usb_hub_bind(void* ctx, zx_device_t* device) {
    auto dev = fbl::unique_ptr<usb::hub::UsbHub>(new usb::hub::UsbHub(device));
    zx_status_t status = dev->Init();
    if (status != ZX_OK) {
        return status;
    }

    status = dev->DdkAdd("usb-hub", DEVICE_ADD_NON_BINDABLE | DEVICE_ADD_INVISIBLE);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: could not add device: %d\n", __func__, status);
        return status;     
    }

    status = dev->StartThread();
    if (status != ZX_OK) {
        dev->DdkRemove();
    }

    // devmgr owns the memory now
    dev.release();
    return status;
}
