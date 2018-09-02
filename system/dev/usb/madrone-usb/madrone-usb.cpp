// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "madrone-usb.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/platform-defs.h>
#include <ddk/protocol/platform-device.h>
#include <ddktl/protocol/gpio.h>
#include <fbl/unique_ptr.h>

namespace madrone_usb {

zx_status_t MadroneUsb::Create(zx_device_t* parent) {
    fbl::AllocChecker ac;
    auto bus = fbl::make_unique_checked<MadroneUsb>(&ac, parent);
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    auto status = bus->Init();
    if (status != ZX_OK) {
        return status;
    }

    // devmgr is now in charge of the device.
    __UNUSED auto* dummy = bus.release();
    return ZX_OK;
}

zx_status_t MadroneUsb::Init() {
    platform_device_protocol_t pdev;

    auto status = device_get_protocol(parent(), ZX_PROTOCOL_PLATFORM_DEV, &pdev);
    if (status != ZX_OK) {
        return status;
    }

    status = device_get_protocol(parent(), ZX_PROTOCOL_GPIO, &gpio_);
    if (status != ZX_OK) {
        return status;
    }

    ddk::GpioProtocolProxy gpio(&gpio_);
    if (status != ZX_OK) {
        return status;
    }
    status = gpio.Config(0, GPIO_DIR_IN | GPIO_TRIGGER_RISING | GPIO_TRIGGER_FALLING);

    status = gpio.GetInterrupt(0, ZX_INTERRUPT_MODE_EDGE_BOTH, interrupt_.reset_and_get_address());
    if (status != ZX_OK) {
        return status;
    }

    zx_device_prop_t props[] = {
        {BIND_PLATFORM_DEV_VID, 0, PDEV_VID_GENERIC},
        {BIND_PLATFORM_DEV_PID, 0, PDEV_PID_GENERIC},
        {BIND_PLATFORM_DEV_DID, 0, PDEV_DID_USB_DWC3},
    };

    device_add_args_t args = {};
    args.version = DEVICE_ADD_ARGS_VERSION;
    args.name = "dwc3";
    args.ctx = this;
    args.ops = &ddk_device_proto_;
    args.props = props;
    args.prop_count = countof(props);
    args.proto_id = ddk_proto_id_;
    args.proto_ops = ddk_proto_ops_;

    status = pdev_device_add(&pdev, 0, &args, &zxdev_);
    if (status != ZX_OK) {
        return status;
    }

    auto thunk = [](void* arg) { return static_cast<MadroneUsb*>(arg)->InterruptThread(); };
    thrd_create_with_name(&interrupt_thread_, thunk, this, "MadroneUsb::InterruptThread");
    return ZX_OK;
}

int MadroneUsb::InterruptThread() {
    while (true) {
        zx::time timestamp;
        auto status = interrupt_.wait(&timestamp);
        printf("MadroneUsb::InterruptThread got %d\n", status);
    }
    return 0;
}

zx_status_t MadroneUsb::UmsSetMode(usb_mode_t mode) {
/*
    if (mode == usb_mode_) {
        return ZX_OK;
    }
    if (mode == USB_MODE_OTG) {
        return ZX_ERR_NOT_SUPPORTED;
    }

    ddk::GpioProtocolProxy gpio(&gpio_);
    gpio.Write(HUB_VDD33_EN, mode == USB_MODE_HOST);
    gpio.Write(VBUS_TYPEC, mode == USB_MODE_HOST);
    gpio.Write(USBSW_SW_SEL, mode == USB_MODE_HOST);

    usb_mode_ = mode;
*/
    return ZX_OK;
}

void MadroneUsb::DdkRelease() {
    delete this;
}

} // namespace madrone_usb

zx_status_t madrone_usb_bind(void* ctx, zx_device_t* parent) {
    return madrone_usb::MadroneUsb::Create(parent);
}
