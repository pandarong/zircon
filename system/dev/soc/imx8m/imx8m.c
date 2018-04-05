// Copyright 2018 The Fuchsia Authors. All rights reserved.
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

#include <zircon/process.h>
#include <zircon/syscalls.h>
#include <zircon/assert.h>

#include <soc/imx8m/imx8m.h>
#include <soc/imx8m/imx8m-hw.h>
#include <hw/reg.h>

void imx8m_release(imx8m_t* imx8m)
{
    if (imx8m) {
        io_buffer_release(&imx8m->iomuxc_base);
    }
    free(imx8m);
}

zx_status_t imx8m_init(zx_handle_t resource, zx_handle_t bti, imx8m_t** out)
{
    zx_status_t status;
    imx8m_t* imx8m = calloc(1, sizeof(imx8m_t));
    if (!imx8m) {
        return ZX_ERR_NO_MEMORY;
    }

    if ( (status = io_buffer_init_physical(&imx8m->iomuxc_base, bti, IMX8M_AIPS_IOMUXC_BASE,
                                            IMX8M_AIPS_LENGTH, resource,
                                            ZX_CACHE_POLICY_UNCACHED_DEVICE)) != ZX_OK) {
        goto fail;
    }

    *out = imx8m;
    return ZX_OK;

fail:
    zxlogf(ERROR, "%s: failed %d\n", __FUNCTION__, status);
    imx8m_release(imx8m);
    return status;
}

