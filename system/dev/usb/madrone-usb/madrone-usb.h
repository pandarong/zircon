// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <threads.h>

#include <ddk/device.h>
#include <ddktl/device.h>
#include <ddktl/protocol/gpio.h>
#include <ddktl/protocol/usb-mode-switch.h>
#include <fbl/macros.h>
#include <lib/zx/interrupt.h>

namespace madrone_usb {

class MadroneUsb;
using MadroneUsbType = ddk::Device<MadroneUsb>;

// This is the main class for the platform bus driver.
class MadroneUsb : public MadroneUsbType, public ddk::UmsProtocol<MadroneUsb> {
public:
    explicit MadroneUsb(zx_device_t* parent)
        : MadroneUsbType(parent), usb_mode_(USB_MODE_NONE) {}

    static zx_status_t Create(zx_device_t* parent);

    // Device protocol implementation.
    void DdkRelease();

    // USB mode switch protocol implementation.
    zx_status_t UmsSetMode(usb_mode_t mode);

private:
    DISALLOW_COPY_ASSIGN_AND_MOVE(MadroneUsb);

    zx_status_t Init();

    int InterruptThread();

    gpio_protocol_t gpio_;
    usb_mode_t usb_mode_;

    thrd_t interrupt_thread_;
    zx::interrupt interrupt_;
};

} // namespace madrone_usb

__BEGIN_CDECLS
zx_status_t madrone_usb_bind(void* ctx, zx_device_t* parent);
__END_CDECLS
