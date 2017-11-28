// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddk/device.h>
#include <ddktl/device.h>
#include <ddktl/protocol/hidbus.h>
#include <fbl/mutex.h>
#include <fbl/unique_ptr.h>
#include <threads.h>
#include <zircon/compiler.h>
#include <zircon/device/hidctl.h>
#include <zircon/types.h>
#include <zx/socket.h>

namespace usb {
namespace hub {

class UsbHub : public ddk::Device<UsbHub, ddk::Unbindable> {
  public:
    UsbHub(zx_device_t* device);
    virtual ~UsbHub();
    zx_status_t Init();
    zx_status_t StartThread();

    void DdkRelease();
    void DdkUnbind();

  private:
    typedef uint16_t port_t;
    typedef uint16_t port_status_t;
    static constexpr port_t kMaxPort = 128;
  
    static int ThreadEntry(void* arg);
    void Thread();

   static void InterruptComplete(usb_request_t* request, void* cookie);

    zx_status_t GetPortStatus(port_t port, port_status_t* out_status);
    zx_status_t WaitForPort(port_t port, port_status_t* out_status, port_status_t status_bits,
                            port_status_t status_mask, zx_time_t stable_time);
    void PowerOnPort(port_t port);
    void PortEnabled(port_t port);
    void PortConnected(port_t port);
    void PortDisconnected(port_t port);
    void HandlePortStatus(port_t port, port_status_t status);

    inline bool IsPortAttached(int port) {
        return (attached_ports_[port / 8] & (1 << (port % 8))) != 0;
    }

    inline void SetPortAttached(int port, bool attached) {
        if (attached) {
            attached_ports_[port / 8] |= (uint8_t)(1 << (port % 8));
        } else {
            attached_ports_[port / 8] &= (uint8_t)(~(1 << (port % 8)));
        }
    }

    usb_protocol_t usb_;

    zx_device_t* bus_device_;
    usb_bus_protocol_t bus_;

    usb_speed_t hub_speed_;
    port_t num_ports_;
    // delay after port power in microseconds
    zx_time_t power_on_delay_;

    usb_request_t* status_request_;
    completion_t completion_;

    thrd_t thread_;
    bool thread_done_;

    // port status values for our ports
    // length is num_ports
    port_status_t* port_status_;

    // bit field indicating which ports have devices attached
    uint8_t attached_ports_[kMaxPort / 8];
};

}  // namespace hub
}  // namespace usb
