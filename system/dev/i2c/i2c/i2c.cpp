// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "i2c.h"

#include <stdint.h>
#include <string.h>

#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/protocol/i2c-impl.h>
#include <ddk/protocol/platform-bus.h>
#include <ddktl/device.h>
#include <fbl/alloc_checker.h>

#include "i2c-bus.h"

namespace i2c {

zx_status_t I2cDevice::Create(zx_device_t* parent) {
    i2c_impl_protocol_t proto;

    auto status = device_get_protocol(parent, ZX_PROTOCOL_I2C_IMPL, &proto);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: ZX_PROTOCOL_I2C_IMPL not available\n", __func__);
        return status;
    }

    fbl::AllocChecker ac;
    fbl::unique_ptr<i2c::I2cDevice> i2c(new (&ac) I2cDevice(parent, &proto));
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    status = i2c->Init();
    if (status != ZX_OK) {
        return status;
    }

    // devmgr is now in charge of the device.
    __UNUSED auto* dummy = i2c.release();
    return ZX_OK;
}

zx_status_t I2cDevice::Init() {
    uint32_t bus_count = i2c_impl_.GetBusCount();
    if (!bus_count) {
        return ZX_ERR_NOT_SUPPORTED;
    }

    fbl::AllocChecker ac;
    i2c_buses_.reserve(bus_count, &ac);
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    for (uint32_t i = 0; i < bus_count; i++) {
        fbl::unique_ptr<I2cBus> i2c_bus(new (&ac) I2cBus(i2c_impl_, i));
        if (!ac.check()) {
            return ZX_ERR_NO_MEMORY;
        }

        auto status = i2c_bus->Start();
        if (status != ZX_OK) {
            return status;
        }

        i2c_buses_.push_back(fbl::move(i2c_bus));
    }

    // If our grand parent supports ZX_PROTOCOL_PLATFORM_BUS, then we need to register our protocol
    // with the platform bus.
    zx_device_t* grand_parent = device_get_parent(parent());
    if (grand_parent) {
        platform_bus_protocol_t pbus;
        if (device_get_protocol(grand_parent, ZX_PROTOCOL_PLATFORM_BUS, &pbus) == ZX_OK) {
            i2c_protocol_t i2c;
            i2c.ctx = this;
            i2c.ops = &i2c_proto_ops_;
            pbus_set_protocol(&pbus, ZX_PROTOCOL_I2C, &i2c);

            // If we are implementing our protocol for the platform bus, we should not be bindable.
            return DdkAdd("i2c", DEVICE_ADD_NON_BINDABLE);
        }
    }

    // Otherwise, publish a normal device.
    return DdkAdd("i2c");
}

zx_status_t I2cDevice::I2cTransact(uint32_t index, const void* write_buf, size_t write_length,
                                   size_t read_length, i2c_complete_cb complete_cb, void* cookie) {
    if (index >= i2c_buses_.size()) {
        return ZX_ERR_OUT_OF_RANGE;
    }

    return i2c_buses_[index]->Transact(write_buf, write_length, read_length, complete_cb, cookie);
}

zx_status_t I2cDevice::I2cGetMaxTransferSize(uint32_t index, size_t* out_size) {
    if (index >= i2c_buses_.size()) {
        return ZX_ERR_OUT_OF_RANGE;
    }

    *out_size = i2c_buses_[index]->GetMaxTransferSize();
    return ZX_OK;
}

} // namespace i2c

extern "C" zx_status_t i2c_bind(void* ctx, zx_device_t* parent) {
    return i2c::I2cDevice::Create(parent);
}
