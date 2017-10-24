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

#include "a113-clocks.h"

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
        dprintf(ERROR, "a113_clk_init: io_buffer_init_physical failed %d\n", status);
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


