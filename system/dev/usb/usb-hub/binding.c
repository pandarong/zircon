// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/binding.h>

#include <zircon/hw/usb.h>

extern zx_status_t usb_hub_bind(void* ctx, zx_device_t* device);

static zx_driver_ops_t usb_hub_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = usb_hub_bind,
};

ZIRCON_DRIVER_BEGIN(usb_hub, usb_hub_driver_ops, "zircon", "0.1", 2)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_USB),
    BI_MATCH_IF(EQ, BIND_USB_CLASS, USB_CLASS_HUB),
ZIRCON_DRIVER_END(usb_hub)
