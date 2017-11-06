// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#include <bits/limits.h>
#include <ddk/debug.h>
#include <hw/reg.h>

#include <zircon/assert.h>
#include <zircon/types.h>

#include "aml-tdm.h"
#include "a113-bus.h"





/* create instance of aml_i2c_t and do basic initialization.  There will
be one of these instances for each of the soc i2c ports.
*/
zx_status_t aml_tdmout_init(aml_tdmout_dev_t *device, a113_bus_t *host_bus) {

    ZX_DEBUG_ASSERT(device);
    ZX_DEBUG_ASSERT(host_bus);

    device->host_bus = host_bus;  // TODO - might not need this

    zx_handle_t resource = get_root_resource();
    zx_status_t status;

    status = io_buffer_init_physical(&device->regs_iobuff,
                                        AML_TDM_PHYS_BASE,
                                        PAGE_SIZE, resource,
                                        ZX_CACHE_POLICY_UNCACHED_DEVICE);

    if (status != ZX_OK) {
        dprintf(ERROR, "aml_tdm_init: io_buffer_init_physical failed %d\n", status);
        goto init_fail;
    }

    device->regs = (aml_tdm_regs_t*)(io_buffer_virt(&device->regs_iobuff));
/*
    status = zx_interrupt_create(resource, dev_desc->irqnum, ZX_INTERRUPT_MODE_LEVEL_HIGH, &(*device)->irq);
    if (status != ZX_OK) {
        goto init_fail;
    }
*/

    //in the fuchsia audio interface, a ring buffer vmo will be handed
    // to us by our client, but for testing we will make our own now
    status = io_buffer_init(&device->ring_buff, 4096, IO_BUFFER_CONTIG);

    aml_tdm_regs_t *reg = device->regs;

    //todo - need to configure the mpll and use it as clock source

    // enable mclk c, select fclk_div4 as source, divide by 5208 to get 48kHz
    reg->mclk_ctl[MCLK_C] = (1 << 31) | (6 << 24) | (5208);

    // configure mst_sclk_gen
    reg->sclk_ctl[MCLK_C].ctl0 = (0x03 << 30) | (1 << 20) | (4 << 10) | 31;
    reg->sclk_ctl[MCLK_C].ctl1 = 0x11111111;

    reg->clk_tdmout_ctl[TDM_OUT_C] = (0x03 << 30) | (2 << 24) | (2 << 20);

    // assign fclk/4 (500MHz) to the pdm clocks and divide to get ~48khz
    reg->clk_pdmin_ctl0 = ( 1 << 31) | (6 << 24) | (10416/128);
    reg->clk_pdmin_ctl1 = ( 1 << 31) | (6 << 24) | (2);

    // Enable clock gates for PDM and TDM blocks
    reg->clk_gate_en = (1 << 8) | (1 << 1);

    return ZX_OK;

init_fail:
    if (device) {
        io_buffer_release(&device->ring_buff);
        io_buffer_release(&device->regs_iobuff);
        if (device->irq != ZX_HANDLE_INVALID)
            zx_handle_close(device->irq);
     };
    return status;
}
