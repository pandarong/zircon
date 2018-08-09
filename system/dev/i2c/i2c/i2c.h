// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <threads.h>

#include <ddktl/device.h>
#include <ddktl/protocol/i2c.h>
#include <ddktl/protocol/i2c-impl.h>
#include <fbl/vector.h>
#include <fbl/unique_ptr.h>

namespace i2c {

class I2cBus;
class I2cDevice;
using I2cDeviceType = ddk::Device<I2cDevice, ddk::Unbindable>;

class I2cDevice : public I2cDeviceType, public ddk::I2cProtocol<I2cDevice> {
public:
    static zx_status_t Create(zx_device_t* parent);

    // Device protocol implementation.
    void DdkUnbind() {
        DdkRemove();
    }
    void DdkRelease() {
        delete this;
    }

    // I2c protocol implementation.
    zx_status_t I2cTransact(uint32_t index, const void* write_buf, size_t write_length,
                            size_t read_length, i2c_complete_cb complete_cb, void* cookie);
    zx_status_t I2cGetMaxTransferSize(uint32_t index, size_t* out_size);

private:
    explicit I2cDevice(zx_device_t* parent, i2c_impl_protocol_t* i2c_impl)
        : I2cDeviceType(parent), i2c_impl_(i2c_impl) {}


    zx_status_t Init();

    // Lower level I2C protocol.
    ddk::I2cImplProtocolProxy i2c_impl_;

    // List of I2C buses.
    fbl::Vector<fbl::unique_ptr<I2cBus>> i2c_buses_;
};

} // namespace i2c
