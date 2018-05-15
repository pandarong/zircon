// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/debug.h>
#include <ddk/metadata.h>
#include <ddk/protocol/platform-defs.h>
#include <hw/reg.h>
#include <soc/aml-s912/s912-hw.h>
#include <soc/aml-a113/a113-hw.h>
#include <soc/aml-s912/s912-gpio.h>

#include "vim.h"

#define BIT_MASK(start, count) (((1 << (count)) - 1) << (start))
#define SET_BITS(dest, start, count, value) \
        ((dest & ~BIT_MASK(start, count)) | (((value) << (start)) & BIT_MASK(start, count)))


/*static const pbus_mmio_t sd_mmios[] = {
    {
        .base = 0xD0072000,
        .length = 0x2000,
    }
};*/

/*static const pbus_irq_t sd_irqs[] = {
    {
        .irq = 249,
    },
};*/

static const pbus_mmio_t sdio_mmios[] = {
    {
        .base = 0xD0070000,
        .length = 0x2000,
    }
};

static const pbus_irq_t sdio_irqs[] = {
    {
        .irq = 248,
    },
};

static const pbus_bti_t sdio_btis[] = {
    {
        .iommu_index = 0,
        .bti_id = BTI_SDIO,
    },
};

static const pbus_gpio_t sdio_gpios[] = {
    {
        .gpio = S912_GPIOX(6),
    },
};

static const pbus_dev_t sdio_dev = {
    .name = "vim_sdio",
    .vid = PDEV_VID_AMLOGIC,
    .pid = PDEV_PID_GENERIC,
    .did = PDEV_DID_AMLOGIC_SD_EMMC,
    .mmios = sdio_mmios,
    .mmio_count = countof(sdio_mmios),
    .irqs = sdio_irqs,
    .irq_count = countof(sdio_irqs),
    .btis = sdio_btis,
    .bti_count = countof(sdio_btis),
    .gpios = sdio_gpios,
    .gpio_count = countof(sdio_gpios),
};

zx_status_t vim_sdio_init(vim_bus_t* bus) {
    zx_status_t status;

    // set alternate functions to enable EMMC
    gpio_set_alt_function(&bus->gpio, S912_WIFI_SDIO_D0, S912_WIFI_SDIO_D0_FN);
    gpio_set_alt_function(&bus->gpio, S912_WIFI_SDIO_D1, S912_WIFI_SDIO_D1_FN);
    gpio_set_alt_function(&bus->gpio, S912_WIFI_SDIO_D2, S912_WIFI_SDIO_D2_FN);
    gpio_set_alt_function(&bus->gpio, S912_WIFI_SDIO_D3, S912_WIFI_SDIO_D3_FN);
    gpio_set_alt_function(&bus->gpio, S912_WIFI_SDIO_CLK, S912_WIFI_SDIO_CLK_FN);
    gpio_set_alt_function(&bus->gpio, S912_WIFI_SDIO_CMD, S912_WIFI_SDIO_CMD_FN);

    if ((status = pbus_device_add(&bus->pbus, &sdio_dev, 0)) != ZX_OK) {
        zxlogf(ERROR, "vim_sd_emmc_init could not add emmc_dev: %d\n", status);
        return status;
    }

    return ZX_OK;
}
