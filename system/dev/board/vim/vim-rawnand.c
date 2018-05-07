// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/io-buffer.h>
#include <ddk/protocol/gpio.h>
#include <ddk/protocol/platform-bus.h>
#include <ddk/protocol/platform-defs.h>
#include <hw/reg.h>
#include <soc/aml-s912/s912-gpio.h>
#include <soc/aml-s912/s912-hw.h>
#include <unistd.h>

#include "vim.h"

static const pbus_mmio_t rawnand_mmios[] = {
    {
        .base = 0xffe07000,
        .length = 0x2000,
    }
};

static const pbus_irq_t rawnand_irqs[] = {
    {
        .irq = 34,
    },
};

static const pbus_dev_t rawnand_dev = {
    .name = "aml_rawnand",
    .vid = PDEV_VID_AMLOGIC,
    .pid = PDEV_PID_GENERIC,
    .did = PDEV_DID_AMLOGIC_RAWNAND,
    .mmios = rawnand_mmios,
    .mmio_count = countof(rawnand_mmios),
    .irqs = rawnand_irqs,
    .irq_count = countof(rawnand_irqs),
};


zx_status_t vim_rawnand_init(vim_bus_t* bus) 
{
    zx_status_t status;

    zxlogf(ERROR, "*** MOHAN *** Entered rawnand init\n");
    return ZX_OK;

    // set alternate functions to enable rawnand
    status = gpio_set_alt_function(&bus->gpio, S912_RAWNAND_CE0,
                                   S912_RAWNAND_CE0_FN);
    if (status != ZX_OK)
        return status;    
    status = gpio_set_alt_function(&bus->gpio, S912_RAWNAND_CE1,
                                   S912_RAWNAND_CE1_FN);
    if (status != ZX_OK)
        return status;
    status = gpio_set_alt_function(&bus->gpio, S912_RAWNAND_RB0,
                                   S912_RAWNAND_RB0_FN);
    if (status != ZX_OK)
        return status;
    status = gpio_set_alt_function(&bus->gpio, S912_RAWNAND_ALE,
                                   S912_RAWNAND_ALE_FN);
    if (status != ZX_OK)
        return status;
    status = gpio_set_alt_function(&bus->gpio, S912_RAWNAND_CLE,
                                   S912_RAWNAND_CLE_FN);
    if (status != ZX_OK)
        return status;
    status = gpio_set_alt_function(&bus->gpio, S912_RAWNAND_WEN_CLK,
                                   S912_RAWNAND_WEN_CLK_FN);
    if (status != ZX_OK)
        return status;
    status = gpio_set_alt_function(&bus->gpio, S912_RAWNAND_REN_WR,
                                   S912_RAWNAND_REN_WR_FN);
    if (status != ZX_OK)
        return status;
    status = gpio_set_alt_function(&bus->gpio, S912_RAWNAND_NAND_DQS,
                                   S912_RAWNAND_NAND_DQS_FN);
    if (status != ZX_OK)
        return status;    

    status = pbus_device_add(&bus->pbus, &rawnand_dev, 0);
    if (status != ZX_OK) {
        zxlogf(ERROR, "vim_rawnand_init: pbus_device_add failed: %d\n",
               status);
        return status;
    }

    return ZX_OK;
}


