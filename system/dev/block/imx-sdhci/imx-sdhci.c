// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/sdhci.h>
#include <ddk/protocol/platform-bus.h>
#include <ddk/protocol/platform-defs.h>
#include <ddk/protocol/platform-device.h>
#include <soc/imx8m/imx8m.h>
#include <soc/imx8m/imx8m-hw.h>
#include <soc/imx8m/imx8m-iomux.h>
#include <soc/imx8m/imx8m-gpio.h>
#include <hw/reg.h>
#include <zircon/assert.h>
#include <zircon/types.h>

#include <hw/sdhci.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <threads.h>
#include <unistd.h>

typedef struct imx_sdhci_device {
    platform_device_protocol_t  pdev;
    platform_bus_protocol_t     pbus;
    zx_device_t*                zxdev;
    io_buffer_t                 mmios;

    volatile sdhci_regs_t*      regs;
    uint64_t                    regs_size;
    zx_handle_t                 regs_handle;
    zx_handle_t                 bti_handle;
} imx_sdhci_device_t;

static zx_status_t imx_sdhci_get_interrupt(void* ctx, zx_handle_t* handle_out) {
    imx_sdhci_device_t* dev = ctx;
    return pdev_map_interrupt(&dev->pdev, 0, handle_out);
}

static zx_status_t imx_sdhci_get_mmio(void* ctx, volatile sdhci_regs_t** out) {
    imx_sdhci_device_t* dev = ctx;
    dev->regs = (void *)io_buffer_virt(&dev->mmios);
    *out = dev->regs;
    return ZX_OK;
}

static zx_status_t imx_sdhci_get_bti(void* ctx, uint32_t index, zx_handle_t* out_handle) {
    imx_sdhci_device_t* dev = ctx;
    return zx_handle_duplicate(dev->bti_handle, ZX_RIGHT_SAME_RIGHTS, out_handle);
}

static uint32_t imx_sdhci_get_base_clock(void* ctx) {
    zxlogf(ERROR, "%s\n", __FUNCTION__);
    return ZX_OK;
}

static uint64_t imx_sdhci_get_quirks(void* ctx) {
    zxlogf(ERROR, "%s\n", __FUNCTION__);
    return ZX_OK;
}

static void imx_sdhci_hw_reset(void* ctx) {
    zxlogf(ERROR, "%s\n", __FUNCTION__);
}

static sdhci_protocol_ops_t imx_sdhci_sdhci_proto = {
    .get_interrupt = imx_sdhci_get_interrupt,
    .get_mmio = imx_sdhci_get_mmio,
    .get_bti = imx_sdhci_get_bti,
    .get_base_clock = imx_sdhci_get_base_clock,
    .get_quirks = imx_sdhci_get_quirks,
    .hw_reset = imx_sdhci_hw_reset,
};

static void imx_sdhci_release(void* ctx) {
    imx_sdhci_device_t* dev = ctx;
    if (dev->regs != NULL) {
        zx_handle_close(dev->regs_handle);
    }
    zx_handle_close(dev->bti_handle);
    free(dev);
}

static zx_protocol_device_t imx_sdhci_device_proto = {
    .version = DEVICE_OPS_VERSION,
    .release = imx_sdhci_release,
};


static zx_status_t imx_sdhci_bind(void* ctx, zx_device_t* parent) {
    zx_status_t status;

    imx_sdhci_device_t* dev = calloc(1, sizeof(imx_sdhci_device_t));
    if (!dev) {
        return ZX_ERR_NO_MEMORY;
    }

    status = device_get_protocol(parent, ZX_PROTOCOL_PLATFORM_DEV, &dev->pdev);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: ZX_PROTOCOL_PLATFORM_DEV not available %d \n", __FUNCTION__, status);
        goto fail;
    }

    status = pdev_map_mmio_buffer(&dev->pdev, 0, ZX_CACHE_POLICY_UNCACHED_DEVICE,
                                    &dev->mmios);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: pdev_map_mmio_buffer failed %d\n", __FUNCTION__, status);
        goto fail;
    }

    status = pdev_get_bti(&dev->pdev, 0, &dev->bti_handle);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: Could not get BTI handle %d\n", __FUNCTION__, status);
        goto fail;
    }

    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "imx-sdhci",
        .ctx = dev,
        .ops = &imx_sdhci_device_proto,
        .proto_id = ZX_PROTOCOL_SDHCI,
        .proto_ops = &imx_sdhci_sdhci_proto,
    };

    status = device_add(parent, &args, &dev->zxdev);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: device_add failed %d\n", __FUNCTION__, status);
        goto fail;
    }

    return ZX_OK;

fail:
    imx_sdhci_release(dev);
    return status;
}

static zx_driver_ops_t imx_sdhci_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = imx_sdhci_bind,
};

ZIRCON_DRIVER_BEGIN(imx_sdhci, imx_sdhci_driver_ops, "zircon", "0.1", 4)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_PLATFORM_DEV),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_NXP),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_DID, PDEV_DID_IMX_SDHCI),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_PID, PDEV_PID_IMX8MEVK),
ZIRCON_DRIVER_END(imx_sdhci)
