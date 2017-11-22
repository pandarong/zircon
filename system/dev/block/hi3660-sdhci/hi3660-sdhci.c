// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/platform-defs.h>
#include <ddk/protocol/platform-device.h>
#include <ddk/protocol/sdhci.h>

#include <hw/reg.h>
#include <hw/sdhci.h>

#include <stdlib.h>

#define SDHCI_MMIO 0
#define SDHCI_PINMUX 1

#define SDHCI_IRQ 0


typedef struct {
    zx_device_t* zxdev;
    platform_device_protocol_t pdev;
    pdev_vmo_buffer_t mmio;
    pdev_vmo_buffer_t pinmux;
    

    volatile sdhci_regs_t* regs;
    uint64_t regs_size;
} hisi_sdhci_device_t;

static zx_status_t hisi_sdhci_get_interrupt(void* ctx, zx_handle_t* handle_out) {
    hisi_sdhci_device_t* dev = ctx;

    return pdev_map_interrupt(&dev->pdev, SDHCI_IRQ, handle_out);
}

static zx_status_t hisi_sdhci_get_mmio(void* ctx, volatile sdhci_regs_t** out) {
    hisi_sdhci_device_t* dev = ctx;

    *out = (sdhci_regs_t *)dev->mmio.vaddr;
    return ZX_OK;
}

static uint32_t hisi_sdhci_get_base_clock(void* ctx) {
    return 0;
}

static zx_paddr_t hisi_sdhci_get_dma_offset(void* ctx) {
    return 0;
}

static uint64_t hisi_sdhci_get_quirks(void* ctx) {
    return 0;
}

static void hisi_sdhci_hw_reset(void* ctx) {
}

static sdhci_protocol_ops_t hisi_sdhci_sdhci_proto = {
    .get_interrupt = hisi_sdhci_get_interrupt,
    .get_mmio = hisi_sdhci_get_mmio,
    .get_base_clock = hisi_sdhci_get_base_clock,
    .get_dma_offset = hisi_sdhci_get_dma_offset,
    .get_quirks = hisi_sdhci_get_quirks,
    .hw_reset = hisi_sdhci_hw_reset,
};

static void hisi_sdhci_unbind(void* ctx) {
    hisi_sdhci_device_t* dev = ctx;
    device_remove(dev->zxdev);
}

static void hisi_sdhci_release(void* ctx) {
    hisi_sdhci_device_t* dev = ctx;

    pdev_vmo_buffer_release(&dev->mmio);
    pdev_vmo_buffer_release(&dev->pinmux);
    free(dev);
}

static zx_protocol_device_t hisi_sdhci_device_proto = {
    .version = DEVICE_OPS_VERSION,
    .unbind = hisi_sdhci_unbind,
    .release = hisi_sdhci_release,
};

static zx_status_t hisi_sdhci_bind(void* ctx, zx_device_t* parent) {
    zxlogf(INFO, "hisi_sdhci_bind\n");

    hisi_sdhci_device_t* dev = calloc(1, sizeof(hisi_sdhci_device_t));
    if (!dev) {
        return ZX_ERR_NO_MEMORY;
    }

    zx_status_t status = device_get_protocol(parent, ZX_PROTOCOL_PLATFORM_DEV, &dev->pdev);
    if (status != ZX_OK) {
        goto fail;
    }
    status = pdev_map_mmio_buffer(&dev->pdev, SDHCI_MMIO, ZX_CACHE_POLICY_UNCACHED_DEVICE,
                                  &dev->mmio);
    if (status != ZX_OK) {
        goto fail;
    }
    status = pdev_map_mmio_buffer(&dev->pdev, SDHCI_PINMUX, ZX_CACHE_POLICY_UNCACHED_DEVICE,
                                  &dev->pinmux);
    if (status != ZX_OK) {
        goto fail;
    }

    // Configure pins for the SD controller
    volatile uint32_t* pinmux = dev->pinmux.vaddr;
    writel(1, pinmux + 0);  // SD_CLK
    writel(1, pinmux + 1);  // SD_CMD
    writel(1, pinmux + 2);  // SD_DATA0
    writel(1, pinmux + 3);  // SD_DATA1
    writel(1, pinmux + 4);  // SD_DATA2
    writel(1, pinmux + 5);  // SD_DATA3

    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "hi3660-sdhci",
        .ctx = dev,
        .ops = &hisi_sdhci_device_proto,
        .proto_id = ZX_PROTOCOL_SDHCI,
        .proto_ops = &hisi_sdhci_sdhci_proto,
    };

    status = device_add(parent, &args, &dev->zxdev);
    if (status != ZX_OK) {
        goto fail;
    }

    return ZX_OK;

fail:
    zxlogf(ERROR, "hisi_sdhci_bind failed: %d\n", status);
    hisi_sdhci_release(dev);
    return status;
}

static zx_driver_ops_t hisi_sdhci_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = hisi_sdhci_bind,
};

// clang-format off
ZIRCON_DRIVER_BEGIN(hisi_sdhci, hisi_sdhci_driver_ops, "zircon", "0.1", 3)
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_HISILICON),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_PID, PDEV_PID_HI3660),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_DID, PDEV_DID_HI3660_SDHCI),
ZIRCON_DRIVER_END(hi3660_sdhci)
