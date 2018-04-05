// Copyright 2018 The Fuchsia Authors. All rights reserved.
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

#include <soc/imx8m/imx8m.h>
#include <soc/imx8m/imx8m-hw.h>
#include <soc/imx8m/imx8m-iomux.h>

zx_status_t imx8m_config_pin(imx8m_t *dev, iomux_cfg_struct* s_cfg, int size)
{
    int i;
    volatile void* iomux = io_buffer_virt(&dev->iomuxc_base);

    if (!dev || !(iomux) || !(s_cfg)) {
        return ZX_ERR_INVALID_ARGS;
    }

    for (i = 0; i < size; i++) {
        zxlogf(SPEW, "0x%lx\n", s_cfg[i]);
        zxlogf(SPEW, "val = 0x%lx, reg = %p\n",
                    IOMUX_CFG_MUX_MODE_VAL(GET_MUX_MODE_VAL(s_cfg[i])) |
                    IOMUX_CFG_SION_VAL(GET_SION_VAL(s_cfg[i])),
                    iomux + GET_MUX_CTL_OFF_VAL(s_cfg[i]));
        zxlogf(SPEW, "val = 0x%lx, reg = %p\n",
                    IOMUX_CFG_DSE_VAL(GET_DSE_VAL(s_cfg[i])) |
                    IOMUX_CFG_SRE_VAL(GET_SRE_VAL(s_cfg[i])) |
                    IOMUX_CFG_ODE_VAL(GET_ODE_VAL(s_cfg[i])) |
                    IOMUX_CFG_PUE_VAL(GET_PUE_VAL(s_cfg[i])) |
                    IOMUX_CFG_HYS_VAL(GET_HYS_VAL(s_cfg[i])) |
                    IOMUX_CFG_LVTTL_VAL(GET_LVTTL_VAL(s_cfg[i])) |
                    IOMUX_CFG_VSEL_VAL(GET_VSEL_VAL(s_cfg[i])),
                    iomux + GET_PAD_CTL_OFF_VAL(s_cfg[i]));
        zxlogf(SPEW, "val = 0x%lx, reg = %p\n",
                    IOMUX_CFG_DAISY_VAL(GET_DAISY_VAL(s_cfg[i])),
                    iomux + GET_SEL_INP_OFF_VAL(s_cfg[i]));

        if (GET_MUX_CTL_OFF_VAL(s_cfg[i])) {
            writel(
                    IOMUX_CFG_MUX_MODE_VAL(GET_MUX_MODE_VAL(s_cfg[i])) |
                    IOMUX_CFG_SION_VAL(GET_SION_VAL(s_cfg[i])),
                        iomux + GET_MUX_CTL_OFF_VAL(s_cfg[i]));
        }
        if (GET_PAD_CTL_OFF_VAL(s_cfg[i])) {
            writel(
                    IOMUX_CFG_DSE_VAL(GET_DSE_VAL(s_cfg[i])) |
                    IOMUX_CFG_SRE_VAL(GET_SRE_VAL(s_cfg[i])) |
                    IOMUX_CFG_ODE_VAL(GET_ODE_VAL(s_cfg[i])) |
                    IOMUX_CFG_PUE_VAL(GET_PUE_VAL(s_cfg[i])) |
                    IOMUX_CFG_HYS_VAL(GET_HYS_VAL(s_cfg[i])) |
                    IOMUX_CFG_LVTTL_VAL(GET_LVTTL_VAL(s_cfg[i])) |
                    IOMUX_CFG_VSEL_VAL(GET_VSEL_VAL(s_cfg[i])),
                        iomux + GET_PAD_CTL_OFF_VAL(s_cfg[i]));
        }
        if (GET_SEL_INP_OFF_VAL(s_cfg[i])) {
            writel(IOMUX_CFG_DAISY_VAL(GET_DAISY_VAL(s_cfg[i])),
                        iomux + GET_SEL_INP_OFF_VAL(s_cfg[i]));
        }
    }

    return ZX_OK;
}