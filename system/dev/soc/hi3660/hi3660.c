// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
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
#include <hw/reg.h>

#include <zircon/process.h>
#include <zircon/syscalls.h>
#include <zircon/assert.h>

#include <gpio/pl061/pl061.h>
#include <soc/hi3660/hi3660.h>
#include <soc/hi3660/hi3660-hw.h>

zx_status_t hi3660_init(zx_handle_t resource, hi3660_t** out) {
    hi3660_t* hi3660 = calloc(1, sizeof(hi3660_t));
    if (!hi3660) {
        return ZX_ERR_NO_MEMORY;
    }
    list_initialize(&hi3660->gpios);

    zx_status_t status;
    if ((status = io_buffer_init_physical(&hi3660->usb3otg_bc, MMIO_USB3OTG_BC_BASE,
                                          MMIO_USB3OTG_BC_LENGTH, resource,
                                          ZX_CACHE_POLICY_UNCACHED_DEVICE)) != ZX_OK ||
         (status = io_buffer_init_physical(&hi3660->peri_crg, MMIO_PERI_CRG_BASE,
                                           MMIO_PERI_CRG_LENGTH, resource,
                                           ZX_CACHE_POLICY_UNCACHED_DEVICE)) != ZX_OK ||
         (status = io_buffer_init_physical(&hi3660->pctrl, MMIO_PCTRL_BASE, MMIO_PCTRL_LENGTH,
                                           resource, ZX_CACHE_POLICY_UNCACHED_DEVICE)) != ZX_OK) {
        goto fail;
    }

    status = hi3660_gpio_init(hi3660);
    if (status != ZX_OK) {
        goto fail;
    }
    status = hi3660_usb_init(hi3660);
    if (status != ZX_OK) {
        goto fail;
    }

/*
[    3.067721] CCC clk_prepare_enable biu_clk
[    3.067733] CCC clkgate_separated_enable 40000000 ffffff8008005000
[    3.067746] CCC clk_prepare_enable ciu_clk
[    3.067755] CCC clkgate_separated_enable 20000 ffffff8008005040
[    3.067843] CCC dw_mci_hs_set_timing 0 -1
[    3.095964] CCC dw_mci_hs_set_timing 0 -1
[    3.117050] CCC clk_prepare_enable biu_clk
[    3.117061] CCC clkgate_separated_enable 200000 ffffff8008005000
[    3.117073] CCC clk_prepare_enable ciu_clk
[    3.117083] CCC clkgate_separated_enable 80000 ffffff8008005040
*/

// SD card
    volatile void* peri_crg = io_buffer_virt(&hi3660->peri_crg);
    uint32_t temp;


    writel(0x40000, peri_crg + 0x94);
    usleep(50);
    writel(0x40000, peri_crg + 0x94 + 4);


    temp = readl(peri_crg + 0 + 8);
    printf("HI3660_HCLK_GATE_SD status %x\n", temp);
    // enable HI3660_HCLK_GATE_SD
    writel(0x40000000, peri_crg + 0);
    temp = readl(peri_crg + 0 + 8);
    printf("HI3660_HCLK_GATE_SD status %x\n", temp);

    temp = readl(peri_crg + 0x40 + 8);
    printf("HI3660_CLK_GATE_SD status %x\n", temp);
    // enable HI3660_CLK_GATE_SD
    writel(0x20000, peri_crg + 0x40);
    temp = readl(peri_crg + 0x40 + 8);
    printf("HI3660_CLK_GATE_SD status %x\n", temp);

    *out = hi3660;
    return ZX_OK;

fail:
    zxlogf(ERROR, "hi3660_init failed %d\n", status);
    hi3660_release(hi3660);
    return status;
}

zx_status_t hi3660_get_protocol(hi3660_t* hi3660, uint32_t proto_id, void* out) {
    switch (proto_id) {
    case ZX_PROTOCOL_GPIO: {
        memcpy(out, &hi3660->gpio, sizeof(hi3660->gpio));
        return ZX_OK;
    }
    default:
        return ZX_ERR_NOT_SUPPORTED;
    }
}

void hi3660_release(hi3660_t* hi3660) {
    hi3660_gpio_release(hi3660);
    io_buffer_release(&hi3660->usb3otg_bc);
    io_buffer_release(&hi3660->peri_crg);
    io_buffer_release(&hi3660->pctrl);
    free(hi3660);
}
