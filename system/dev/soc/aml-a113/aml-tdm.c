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
#include "sin.h"
#include "a113-bus.h"


static void aml_tdmout_fifo_reset(aml_tdmout_dev_t *device, uint32_t index) {

    device->regs->tdmout[index].ctl0 &= ~(3 <<28);
    device->regs->tdmout[index].ctl0 |= (1 << 29);
    device->regs->tdmout[index].ctl0 |= (1 << 28);
}

static void aml_tdmout_enable(aml_tdmout_dev_t *device, uint32_t index) {
    device->regs->tdmout[index].ctl0 |= (1 << 31);
}

static void aml_tdm_frddr_enable(aml_tdmout_dev_t *device, uint32_t index) {
    device->regs->frddr[index].ctl0 |= (1 << 31);
}

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
    status = io_buffer_init(&device->ring_buff, 4096, IO_BUFFER_CONTIG | IO_BUFFER_RW);
    if (status != ZX_OK) {
        printf("iobuffer init failed with %d\n",status);
    }
    int32_t *ring = (int32_t*)io_buffer_virt(&device->ring_buff);
#if 1
    for (int i=0; i<1024; i=i+4) {
        ring[i]   = sintable[i/4]/20;
        ring[i+1] = sintable[i/4]/20;
        ring[i+2] = sintable[i/4]/20;
        ring[i+3] = sintable[i/4]/20;
    }
    io_buffer_cache_op(&device->ring_buff, ZX_VMO_OP_CACHE_CLEAN, 0, 4096);
#endif
    aml_tdm_regs_t *reg = device->regs;


    //todo - need to configure the mpll and use it as clock source

    // enable mclk c, select fclk_div4 as source, divide by 5208 to get 48kHz
    reg->mclk_ctl[MCLK_C] = (1 << 31) | (6 << 24) | (19);

    // configure mst_sclk_gen
    reg->sclk_ctl[MCLK_C].ctl0 = (0x03 << 30) | (1 << 20) | (0 << 10) | 255;
    reg->sclk_ctl[MCLK_C].ctl1 = 0x00000001;

    reg->clk_tdmout_ctl[TDM_OUT_C] = (0x03 << 30) | (2 << 24) | (2 << 20);

    // assign fclk/4 (500MHz) to the pdm clocks and divide to get ~48khz
    //reg->clk_pdmin_ctl0 = ( 1 << 31) | (6 << 24) | (10416/128);
    //reg->clk_pdmin_ctl1 = ( 1 << 31) | (6 << 24) | (2);

    // Enable clock gates for PDM and TDM blocks
    reg->clk_gate_en = (1 << 8) | (1 << 11) | 1;

    reg->arb_ctl |= (1 << 31) | (1 << 6);
    reg->frddr[2].ctl0 = (2 << 0);

    reg->frddr[2].ctl1 = ((0x40 - 1) << 24) | ((0x20 -1) << 16) | (2 << 8);
    reg->frddr[2].start_addr = io_buffer_phys(&device->ring_buff);
    reg->frddr[2].finish_addr = io_buffer_phys(&device->ring_buff) + (4096 - 8);

    reg->tdmout[TDM_OUT_C].ctl0 =  (1 << 15) | (7 << 5 ) | (31 << 0);

    reg->tdmout[TDM_OUT_C].ctl1 =  (23 << 8) | (2 << 24) | (4  << 4);

    reg->tdmout[TDM_OUT_C].mask[0]=0x0000000f;
    reg->tdmout[TDM_OUT_C].swap = 0x00000010;
    reg->tdmout[TDM_OUT_C].mask_val=0x0f0f0f0f;
    reg->tdmout[TDM_OUT_C].mute_val=0xaaaaaaaa;

    aml_tdmout_fifo_reset(device, TDM_OUT_C);
    aml_tdm_frddr_enable(device, 2);        //enable frddr_c
    aml_tdmout_enable(device, TDM_OUT_C);

    uint64_t tempt = (uint64_t)&reg->tdmout[0] - (uint64_t)reg;
    printf("tdmout offset = %lx\n",tempt);

    printf("stat= %08x\n",reg->tdmout[TDM_OUT_C].stat);
    printf("frddr status1 = %08x\n",reg->frddr[2].status1);
    printf("frddr status2 = %08x\n",reg->frddr[2].status2);

    printf("frddr finish addr = %08x\n",reg->frddr[2].finish_addr);

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
