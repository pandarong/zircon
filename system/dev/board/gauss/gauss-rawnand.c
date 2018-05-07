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
#include <soc/aml-a113/a113-hw.h>
#include <unistd.h>

#include "gauss.h"
#include "gauss-hw.h"

static const pbus_mmio_t rawnand_mmios[] = {
    {   /* nandreg : Registers for NAND controller */
        /* 
         * From the Linux devicetree this seems the right
         * address. The data sheet is WRONG.
         */
        .base = 0xffe07800,
        .length = 0x2000,
    },
    {   /* clockreg : Clock Register for NAND controller */
        /*
         * From the Linux devicetree. This is the base SD_EMMC_CLOCK
         * register (for port C)
         */
        .base = 0xffe07000,
        .length = 0x4,  /* Just 4 bytes */
    },
};

static const pbus_irq_t rawnand_irqs[] = {
    {
        .irq = 66,
    },
};

static const pbus_bti_t rawnand_btis[] = {
    {
        .iommu_index = 0,
        .bti_id = BTI_AML_RAWNAND,
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
    .btis = rawnand_btis,
    .bti_count = countof(rawnand_btis),
};


zx_status_t gauss_rawnand_init(gauss_bus_t* bus) 
{
    zx_status_t status;

    // set alternate functions to enable rawnand
    status = gpio_set_alt_function(&bus->gpio, A113_GPIOBOOT(8),
                                   2);
    if (status != ZX_OK)
        return status;    
    status = gpio_set_alt_function(&bus->gpio, A113_GPIOBOOT(9),
                                   2);
    if (status != ZX_OK)
        return status;
    status = gpio_set_alt_function(&bus->gpio, A113_GPIOBOOT(10),
                                   2);    
    if (status != ZX_OK)
        return status;
    status = gpio_set_alt_function(&bus->gpio, A113_GPIOBOOT(11),
                                   2);        
    if (status != ZX_OK)
        return status;
    status = gpio_set_alt_function(&bus->gpio, A113_GPIOBOOT(12),
                                   2);            
    if (status != ZX_OK)
        return status;
    status = gpio_set_alt_function(&bus->gpio, A113_GPIOBOOT(13),
                                   2);                
    if (status != ZX_OK)
        return status;

    status = pbus_device_add(&bus->pbus, &rawnand_dev, 0);
    if (status != ZX_OK) {
        zxlogf(ERROR, "gauss_rawnand_init: pbus_device_add failed: %d\n",
               status);
        return status;
    }

    return ZX_OK;
}


