// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zircon/assert.h>
#include <ddk/protocol/i2c.h>
#include <fbl/macros.h>

namespace ddk {

class I2cChannel {

public:
    friend class Pdev;

    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(I2cChannel);
    DISALLOW_NEW;

    I2cChannel() : i2c_({0,0}) {}

    // Allow assignment from an rvalue
    I2cChannel& operator=(I2cChannel&& other) {
        transfer(other);
        return *this;
    }

    I2cChannel release() {
        return I2cChannel(*this);
    }
    // Performs typical i2c Read: writes device register address (1 byte) followed
    //  by len reads into buf.
    zx_status_t Read(uint8_t addr, uint8_t* buf, uint8_t len) {
        return Transact(&addr, 1, buf, len);
    }

    // Writes len bytes from buffer with no trailing read
    zx_status_t Write(uint8_t* buf, uint8_t len) {
        return Transact(buf, len, nullptr, 0);
    }

    zx_status_t Transact(uint8_t* tx_buf, uint8_t tx_len,
                         uint8_t* rx_buf, uint8_t rx_len) {
        ZX_DEBUG_ASSERT(is_valid());
        return i2c_transact_sync(&i2c_, pdev_index_, tx_buf, tx_len,
                                 rx_buf, rx_len);
    }

    // Check to determine if this object is intiialized
    bool is_valid(void) {
        return  (i2c_.ops && i2c_.ctx);
    }

    I2cChannel(I2cChannel&&) = default;

    ~I2cChannel() {}

private:
    void transfer(I2cChannel& other) {
            i2c_.ops = other.i2c_.ops;
            i2c_.ctx = other.i2c_.ctx;
            pdev_index_ = other.pdev_index_;
            other.Invalidate();
    }

    I2cChannel(I2cChannel& other) {
        transfer(other);
    }
    I2cChannel(uint32_t index, i2c_protocol_t i2c) : pdev_index_(index),
                    i2c_(i2c) {
    }

    void Invalidate() {
        i2c_ = {0, 0};
    }

    uint32_t pdev_index_;

    i2c_protocol_t i2c_ ;

};

} //namespace ddk