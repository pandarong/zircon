// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddk/protocol/platform-device.h>
#include <ddk/device.h>
#include <ddktl/gpio_pin.h>
#include <ddktl/i2c_channel.h>
#include <ddktl/mmio.h>

#include <fbl/ref_counted.h>
#include <fbl/ref_ptr.h>
#include <fbl/vector.h>
#include <lib/zx/interrupt.h>
#include <lib/zx/bti.h>



namespace ddk {

class Pdev : public fbl::RefCounted<Pdev> {

public:
    static fbl::RefPtr<Pdev> Create(zx_device_t* parent);
    void ShowInfo();

    MmioBlock GetMmio(uint32_t index);

    zx_status_t MapInterrupt(uint32_t index, uint32_t flags, zx::interrupt* out) {
        return pdev_get_interrupt(&pdev_, index, flags,
            out->reset_and_get_address());
    }

    zx_status_t MapInterrupt(uint32_t index, zx::interrupt* out) {
        return MapInterrupt(index, 0, out);
    }

    zx_status_t GetBti(uint32_t index, zx::bti* out){
        return pdev_get_bti(&pdev_, index, out->reset_and_get_address());
    }

    zx_status_t GetInfo(uint32_t index, pdev_device_info_t* out){
        return pdev_get_device_info(&pdev_, out);
    }

    I2cChannel GetI2cChan(uint32_t index);
    GpioPin GetGpio(uint32_t index);

private:

    Pdev(zx_device_t* parent) : parent_(parent) {};

    zx_device_t* parent_;
    platform_device_protocol_t pdev_;

    pdev_device_info_t pdev_info_;

    uint32_t mmio_count_ = 0;
    fbl::Vector<MmioBlock> mmio_;
};


} //namespace ddk


