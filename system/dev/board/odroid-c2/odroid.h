// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddk/device.h>
#include <ddk/protocol/gpio.h>
#include <ddk/protocol/platform-bus.h>

typedef struct {
    platform_bus_protocol_t pbus;
    gpio_protocol_t gpio;
    zx_device_t* parent;
} odroid_t;

// odroid-gpio.c
zx_status_t odroid_gpio_init(odroid_t* odroid);

// odroid-usb.c
zx_status_t odroid_usb_init(odroid_t* odroid);
