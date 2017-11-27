// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <threads.h>
#include <unistd.h>


#include <bits/limits.h>
#include <ddk/debug.h>
#include <hw/reg.h>

#include <zircon/assert.h>
#include <zircon/types.h>

#include <soc/aml-a113/a113-clocks.h>


#define SDM_FRACTIONALITY 16384
#define A113_FIXED_PLL_RATE 2000000000

#define DIV_ROUND_UP(n,d) ((n + d - 1) / d)
/* create instance of a113_clock_t and do basic initialization.
*/
zx_status_t a113_clk_init(a113_clk_dev_t **device, a113_bus_t *host_bus) {

    *device = calloc(1, sizeof(a113_clk_dev_t));
    if (!(*device)) {
        return ZX_ERR_NO_MEMORY;
    }

    (*device)->host_bus = host_bus;  // TODO - might not need this

    zx_handle_t resource = get_root_resource();
    zx_status_t status;

    status = io_buffer_init_physical(&(*device)->regs_iobuff, A113_CLOCKS_BASE_PHYS,
                                     PAGE_SIZE, resource, ZX_CACHE_POLICY_UNCACHED_DEVICE);

    if (status != ZX_OK) {
        zxlogf(ERROR, "a113_clk_init: io_buffer_init_physical failed %d\n", status);
        goto init_fail;
    }

    (*device)->virt_regs = (zx_vaddr_t)(io_buffer_virt(&(*device)->regs_iobuff));

    return ZX_OK;

init_fail:
    if (*device) {
        io_buffer_release(&(*device)->regs_iobuff);
        free(*device);
     };
    return status;
}

static void a113_clk_update_reg(a113_clk_dev_t *dev, uint32_t offset,
                                                     uint32_t pos,
                                                     uint32_t bits,
                                                     uint32_t value) {
    uint32_t reg = a113_clk_get_reg(dev,offset);
    reg &= ~(((1 << bits) - 1) << pos);
    reg |=  (value & ((1 << bits) - 1)) << pos;
    a113_clk_set_reg(dev,offset,reg);
}

zx_status_t a113_clk_set_mpll2(a113_clk_dev_t *device, uint64_t rate) {

    uint64_t n = A113_FIXED_PLL_RATE/rate;  //calculate the integer ratio;
    printf("Integer divider = %ld\n",n);

    uint64_t sdm = DIV_ROUND_UP((A113_FIXED_PLL_RATE - n * rate) * SDM_FRACTIONALITY, rate);
    printf("Fractional divider = %ld\n",sdm);

    a113_clk_update_reg(device, 0xa8, 0, 14, (uint32_t)sdm);
    a113_clk_update_reg(device, 0xa8, 16, 9, (uint32_t)n);
    a113_clk_update_reg(device, 0xa8, 15, 1, 1);
    a113_clk_update_reg(device, 0xa8, 14, 1, 1);

    a113_clk_update_reg(device, 0xba, 2, 1, 1);


    return ZX_OK;
}
