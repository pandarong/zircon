// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zircon/assert.h>
#include <ddk/protocol/gpio.h>
#include <fbl/macros.h>
#include <lib/zx/interrupt.h>


namespace ddk {

class GpioPin {

public:
    friend class Pdev;

    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(GpioPin);
    DISALLOW_NEW;

    GpioPin() : gpio_({nullptr, nullptr}) {}

    // Allow assignment from an rvalue
    GpioPin& operator=(GpioPin&& other) {
        transfer(other);
        return *this;
    }

    zx_status_t Read(uint8_t* out) {
        if (!is_valid()) {
            return ZX_ERR_BAD_STATE;
        }
        return gpio_read(&gpio_, pdev_index_, out);
    }

    zx_status_t Write(uint8_t val) {
        if (!is_valid()) {
            return ZX_ERR_BAD_STATE;
        }
        return gpio_write(&gpio_, pdev_index_, val);
    }

    zx_status_t Config(uint32_t flags) {
        if (!is_valid()){
            return ZX_ERR_BAD_STATE;
        }
        return gpio_config(&gpio_, pdev_index_, flags);
    }

    zx_status_t SetFunction(uint64_t function){
        if (!is_valid()){
            return ZX_ERR_BAD_STATE;
        }
        return gpio_set_alt_function(&gpio_, pdev_index_, function);
    }

    zx_status_t GetInterrupt(uint32_t flags, zx::interrupt* out){
        if (!is_valid()){
            return ZX_ERR_BAD_STATE;
        }
        return gpio_get_interrupt(&gpio_, pdev_index_, flags,
                                  out->reset_and_get_address());
    }

    zx_status_t SetPolarity(uint32_t polarity) {
        if (!is_valid()){
            return ZX_ERR_BAD_STATE;
        }
        return gpio_set_polarity(&gpio_, pdev_index_, polarity);
    }


    // Check to determine if this object is intiialized
    bool is_valid(void) {
        return  (gpio_.ops && gpio_.ctx);
    }

    GpioPin(GpioPin&&) = default;

    ~GpioPin() {}

private:
    void transfer(GpioPin& other) {
            gpio_.ops = other.gpio_.ops;
            gpio_.ctx = other.gpio_.ctx;
            pdev_index_ = other.pdev_index_;
            other.Invalidate();
    }

    GpioPin(GpioPin& other) {
        transfer(other);
    }
    GpioPin(uint32_t index, gpio_protocol_t gpio) : pdev_index_(index),
                    gpio_(gpio) {
    }

    void Invalidate() {
        gpio_ = {0, 0};
    }

    uint32_t pdev_index_;

    gpio_protocol_t gpio_ ;

};

} //namespace ddk