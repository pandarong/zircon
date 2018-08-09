// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/binding.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/platform-defs.h>

extern zx_status_t i2c_bind(void* ctx, zx_device_t* parent);

static zx_driver_ops_t i2c_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = i2c_bind,
};

ZIRCON_DRIVER_BEGIN(i2c, i2c_driver_ops, "zircon", "0.1", 1)
    BI_MATCH_IF(EQ, BIND_PROTOCOL, ZX_PROTOCOL_I2C_IMPL),
ZIRCON_DRIVER_END(i2c)
