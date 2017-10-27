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

#define REGDUMP(regval) \
    printf(#regval " = 0x%08x\n",device->virt_regs->regval);



/* create instance of aml_i2c_t and do basic initialization.  There will
be one of these instances for each of the soc i2c ports.
*/
zx_status_t aml_tdm_init(aml_tdm_dev_t *device, a113_bus_t *host_bus) {

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

    device->virt_regs = (aml_tdm_regs_t*)(io_buffer_virt(&device->regs_iobuff));
/*
    status = zx_interrupt_create(resource, dev_desc->irqnum, ZX_INTERRUPT_MODE_LEVEL_HIGH, &(*device)->irq);
    if (status != ZX_OK) {
        goto init_fail;
    }
*/
    REGDUMP(clk_gate_en)
    REGDUMP(mclk_c_ctl)
    REGDUMP(mst_c_sclk_ctl0)
    REGDUMP(mst_c_sclk_ctl1)

    aml_tdm_regs_t *reg = device->virt_regs;

    // enable mclk c, select fclk_div4 as source, divide by 5208 to get 48kHz
    reg->mclk_c_ctl = (1 << 31) | (6 << 24) | (5208);

    // configure mst_sclk_gen
    reg->mst_c_sclk_ctl0 = (0x03 << 30) | (1 << 20) | (4 << 10) | 32;
    reg->mst_c_sclk_ctl1 = 0x11111111;


    reg->clk_tdmout_c_ctl = (0x03 << 30) | (2 << 24) | (2 << 20);

    reg->clk_gate_en = (1 << 8);


/*


    thrd_t thrd;
    thrd_create_with_name(&thrd, aml_i2c_thread, *device, "i2c_thread");
    thrd_t irqthrd;
    thrd_create_with_name(&irqthrd, aml_i2c_irq_thread, *device, "i2c_irq_thread");
*/
    return ZX_OK;

init_fail:
    if (device) {
        io_buffer_release(&device->regs_iobuff);
        if (device->irq != ZX_HANDLE_INVALID)
            zx_handle_close(device->irq);
     };
    return status;
}
