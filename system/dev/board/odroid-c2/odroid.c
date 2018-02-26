// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/platform-defs.h>


#include <zircon/assert.h>
#include <zircon/process.h>
#include <zircon/syscalls.h>
#include <zircon/threads.h>

#include "odroid.h"

static void odroid_release(void* ctx) {
    odroid_t* odroid = ctx;
    free(odroid);
}

static zx_protocol_device_t odroid_device_protocol = {
    .version = DEVICE_OPS_VERSION,
    .release = odroid_release,
};

static int odroid_start_thread(void* arg) {
    odroid_t* odroid = arg;
    zx_status_t status;

    if ((status = odroid_gpio_init(odroid)) != ZX_OK) {
        zxlogf(ERROR, "odroid_gpio_init failed: %d\n", status);
        goto fail;
    }
    if ((status = odroid_usb_init(odroid)) != ZX_OK) {
        zxlogf(ERROR, "odroid_usb_init failed: %d\n", status);
        goto fail;
    }

    return ZX_OK;
fail:
    zxlogf(ERROR, "odroid_start_thread failed, not all devices have been initialized\n");
    return status;
}

static zx_status_t odroid_bind(void* ctx, zx_device_t* parent) {
    zx_status_t status;

    odroid_t* odroid = calloc(1, sizeof(odroid_t));
    if (!odroid) {
        return ZX_ERR_NO_MEMORY;
    }
    odroid->parent = parent;

    if ((status = device_get_protocol(parent, ZX_PROTOCOL_PLATFORM_BUS, &odroid->pbus)) != ZX_OK) {
        goto fail;
    }

    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "odroid",
        .ctx = odroid,
        .ops = &odroid_device_protocol,
        .flags = DEVICE_ADD_NON_BINDABLE,
    };

    status = device_add(parent, &args, NULL);
    if (status != ZX_OK) {
        goto fail;
    }

    thrd_t t;
    int thrd_rc = thrd_create_with_name(&t, odroid_start_thread, odroid, "odroid_start_thread");
    if (thrd_rc != thrd_success) {
        status = thrd_status_to_zx_status(thrd_rc);
        goto fail;
    }
    return ZX_OK;

fail:
    zxlogf(ERROR, "odroid_bind failed %d\n", status);
    odroid_release(odroid);
    return status;
}

static zx_driver_ops_t odroid_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = odroid_bind,
};

ZIRCON_DRIVER_BEGIN(odroid, odroid_driver_ops, "zircon", "0.1", 3)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_PLATFORM_BUS),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_HARDKERNEL),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_PID, PDEV_PID_ODROID_C2),
ZIRCON_DRIVER_END(odroid)
