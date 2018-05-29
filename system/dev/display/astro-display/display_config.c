// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "astro-display.h"

static const unsigned int od_fb_table[2] = {1, 2};
static const unsigned int od_table[6] = {
    1, 2, 4, 8, 16, 32
};

static int check_pll(struct lcd_clk_config *cConf,
        unsigned int pll_fout)
{
    unsigned int m, n;
    unsigned int od1_sel, od2_sel, od3_sel, od1, od2, od3;
    unsigned int pll_fod2_in, pll_fod3_in, pll_fvco;
    unsigned int od_fb = 0, pll_frac;
    int done;

    done = 0;
    if ((pll_fout > cConf->pll_out_fmax) ||
        (pll_fout < cConf->pll_out_fmin)) {
        return done;
    }
    for (od3_sel = cConf->pll_od_sel_max; od3_sel > 0; od3_sel--) {
        od3 = od_table[od3_sel - 1];
        pll_fod3_in = pll_fout * od3;
        for (od2_sel = od3_sel; od2_sel > 0; od2_sel--) {
            od2 = od_table[od2_sel - 1];
            pll_fod2_in = pll_fod3_in * od2;
            for (od1_sel = od2_sel; od1_sel > 0; od1_sel--) {
                od1 = od_table[od1_sel - 1];
                pll_fvco = pll_fod2_in * od1;
                if ((pll_fvco < cConf->pll_vco_fmin) ||
                    (pll_fvco > cConf->pll_vco_fmax)) {
                    continue;
                }
                cConf->pll_od1_sel = od1_sel - 1;
                cConf->pll_od2_sel = od2_sel - 1;
                cConf->pll_od3_sel = od3_sel - 1;
                cConf->pll_fout = pll_fout;
                cConf->pll_fvco = pll_fvco;
                n = 1;
                od_fb = cConf->od_fb;
                pll_fvco = pll_fvco / od_fb_table[od_fb];
                m = pll_fvco / cConf->fin;
                pll_frac = (pll_fvco % cConf->fin) *
                    cConf->pll_frac_range / cConf->fin;
                cConf->pll_m = m;
                cConf->pll_n = n;
                cConf->pll_frac = pll_frac;
                done = 1;
                break;
            }
        }
    }
    return done;
}

zx_status_t astro_dsi_generate_hpll(astro_display_t* display) {
    uint32_t dsi_bit_rate_min = 0;
    uint32_t dsi_bit_rate_max = 0;
    uint32_t tmp;
    uint32_t pll_fout;
    uint32_t clk_div_sel, xd;
    int done;

    ZX_DEBUG_ASSERT(display);
    ZX_DEBUG_ASSERT(display->dsi_cfg);
    ZX_DEBUG_ASSERT(display->lcd_clk_cfg);

    display->lcd_clk_cfg->fout = display->lcd_timing->lcd_clock / 1000; // KHz
    display->lcd_clk_cfg->err_fmin = MAX_ERROR;
    if (display->lcd_clk_cfg->fout > display->lcd_clk_cfg->xd_out_fmax) {
        DISP_ERROR("Invalid LCD Clock value %dKHz\n", display->lcd_clk_cfg->fout);
        return ZX_ERR_OUT_OF_RANGE;
    }

    display->lcd_clk_cfg->xd_max = CRT_VID_DIV_MAX;
    tmp = display->dsi_cfg->bit_rate_max;
    dsi_bit_rate_max = tmp * 1000; // change to KHz
    dsi_bit_rate_min = dsi_bit_rate_max - display->lcd_clk_cfg->fout;

    clk_div_sel = CLK_DIV_SEL_1;
    for (xd = 1; xd <= display->lcd_clk_cfg->xd_max; xd++) {
        pll_fout = display->lcd_clk_cfg->fout * xd;
        if ((pll_fout > dsi_bit_rate_max) || (pll_fout < dsi_bit_rate_min)) {
            continue;
        }
        display->dsi_cfg->bit_rate = pll_fout * 1000;
        display->dsi_cfg->clock_factor = xd;
        display->lcd_clk_cfg->xd = xd;
        display->lcd_clk_cfg->div_sel = clk_div_sel;
        done = check_pll(display->lcd_clk_cfg, pll_fout);
        if (done) {
            goto generate_clk_done;
        }
    }

generate_clk_done:
    if (done) {
        display->lcd_timing->pll_ctrl =
            (display->lcd_clk_cfg->pll_od1_sel << PLL_CTRL_OD1) |
            (display->lcd_clk_cfg->pll_od2_sel << PLL_CTRL_OD2) |
            (display->lcd_clk_cfg->pll_od3_sel << PLL_CTRL_OD3) |
            (display->lcd_clk_cfg->pll_n << PLL_CTRL_N)         |
            (display->lcd_clk_cfg->pll_m << PLL_CTRL_M);
        display->lcd_timing->div_ctrl =
            (display->lcd_clk_cfg->div_sel << DIV_CTRL_DIV_SEL) |
            (display->lcd_clk_cfg->xd << DIV_CTRL_XD);
        display->lcd_timing->clk_ctrl = (display->lcd_clk_cfg->pll_frac << CLK_CTRL_FRAC);
    } else {
        display->lcd_timing->pll_ctrl =
            (1 << PLL_CTRL_OD1) |
            (1 << PLL_CTRL_OD2) |
            (1 << PLL_CTRL_OD3) |
            (1 << PLL_CTRL_N)   |
            (50 << PLL_CTRL_M);
        display->lcd_timing->div_ctrl =
            (CLK_DIV_SEL_1 << DIV_CTRL_DIV_SEL) |
            (7 << DIV_CTRL_XD);
        display->lcd_timing->clk_ctrl = (0 << CLK_CTRL_FRAC);
        DISP_ERROR("Out of clock range, reset to default setting\n");
    }

    display->lcd_clk_cfg->ss_level =
                (display->lcd_timing->ss_level > display->lcd_clk_cfg->ss_level_max)?
                0 : display->lcd_timing->ss_level;

    return ZX_OK;
}

zx_status_t astro_dsi_load_config(astro_display_t* display) {
    zx_status_t status = ZX_OK;

    ZX_DEBUG_ASSERT(display);

    if (!display->disp_setting) {
        DISP_ERROR("Display Configuration has not been populated! Exiting.\n");
        return ZX_ERR_UNAVAILABLE;
    }

    if (!display->lcd_clk_cfg) {
        DISP_ERROR("LCD Clock/PLL Configuration has not been populated! Exiting.\n");
        return ZX_ERR_UNAVAILABLE;
    }

    // lcd timing should already be populated
    if(!display->lcd_timing) {
        DISP_ERROR("LCD Timing structure has not been populated! Exiting.\n");
        return ZX_ERR_UNAVAILABLE;
    }

    // allocate dsi config structure
    if (display->dsi_cfg != NULL) {
        DISP_INFO("Re-Populating MIPI DSI Config parameters\n");
        memset(display->dsi_cfg, 0, sizeof(dsi_config_t));
    } else {
        display->dsi_cfg = calloc(1, sizeof(dsi_config_t));
        if (!display->dsi_cfg) {
            DISP_ERROR("Could not allocate dsi_config structure\n");
            return ZX_ERR_NO_MEMORY;
        }
    }

    // allocate dsi phy config here as well
    if (display->dsi_phy_cfg != NULL) {
        DISP_INFO("Re-Populating MIPI DSI PHY Config parameters\n");
        memset(display->dsi_phy_cfg, 0, sizeof(dsi_phy_config_t));
    } else {
        display->dsi_phy_cfg = calloc(1, sizeof(dsi_phy_config_t));
        if (!display->dsi_phy_cfg) {
            DISP_ERROR("Could not allocate dsi_phy_cfg structure\n");
            return ZX_ERR_NO_MEMORY;
        }
    }

    display->dsi_cfg->lane_num = display->disp_setting->lane_num;
    display->dsi_cfg->bit_rate_max = display->disp_setting->bit_rate_max;
    display->dsi_cfg->factor_numerator = display->disp_setting->factor_numerator;
    display->dsi_cfg->opp_mode_init = display->disp_setting->opp_mode_init;
    display->dsi_cfg->opp_mode_display = display->disp_setting->opp_mode_display;
    display->dsi_cfg->video_mode_type = display->disp_setting->video_mode_type;
    display->dsi_cfg->clk_always_hs = display->disp_setting->clk_always_hs;
    display->dsi_cfg->phy_switch = display->disp_setting->phy_switch;

    if (display->lcd_timing->lcd_bits == 8) {
        display->dsi_cfg->venc_data_width = MIPI_DSI_VENC_COLOR_24B;
        display->dsi_cfg->dpi_data_format = MIPI_DSI_COLOR_24BIT;
    } else {
        DISP_ERROR("Unsupported LCD Bits\n");
        return ZX_ERR_NOT_SUPPORTED;
    }


    if (display->dsi_cfg->bit_rate_max == 0) {
        DISP_ERROR("Auto bit rate calculation not supported\n");
        return ZX_ERR_NOT_SUPPORTED;
    }

    if (display->dsi_cfg->bit_rate_max < (display->lcd_clk_cfg->pll_out_fmin/1000)) {
        DISP_ERROR("Bit rate of %dMHz not supported (min = %dMHz)\n",
            display->dsi_cfg->bit_rate_max, (display->lcd_clk_cfg->pll_out_fmin/1000));
        return ZX_ERR_OUT_OF_RANGE;
    }

    if (display->dsi_cfg->bit_rate_max > MIPI_PHY_CLK_MAX) {
        DISP_ERROR("Bit rate of %dMHz out of range (max = %dMHz)\n",
            display->dsi_cfg->bit_rate_max, MIPI_PHY_CLK_MAX);
        return ZX_ERR_OUT_OF_RANGE;
    }

    /* Venc resolution format */
    switch (display->disp_setting->phy_switch) {
        case 1: /* standard */
            display->dsi_phy_cfg->state_change = 1;
            break;
        case 2: /* slow */
            display->dsi_phy_cfg->state_change = 2;
            break;
        case 0: /* auto */
        default:
            if ((display->lcd_timing->h_active != 240) &&
                (display->lcd_timing->h_active != 768) &&
                (display->lcd_timing->h_active != 1920) &&
                (display->lcd_timing->h_active != 2560)) {
                display->dsi_phy_cfg->state_change = 2;
            } else {
                display->dsi_phy_cfg->state_change = 1;
            }
            break;
    }

    return status;
}

zx_status_t astro_lcd_timing(astro_display_t* display) {
    zx_status_t status = ZX_OK;
    uint32_t de_hstart, de_vstart;
    uint32_t hstart, hend, vstart, vend;
    uint32_t hPeriod, vPeriod, hActive, vActive;
    uint32_t hSync_backPorch, hSync_width, vSync_width, vSync_backPorch;
    uint32_t sync_duration;
    uint32_t clk;
    ZX_DEBUG_ASSERT(display);

    if (!display->disp_setting) {
        DISP_ERROR("Display Configuration has not been populated! Exiting.\n");
        return ZX_ERR_UNAVAILABLE;
    }

    // allocate lcd_timing structure
    if (display->lcd_timing != NULL) {
        DISP_INFO("Re-Populating LCD Timing parameters\n");
        memset(display->lcd_timing, 0, sizeof(lcd_timing_t));
    } else {
        display->lcd_timing = calloc(1, sizeof(lcd_timing_t));
        if (!display->lcd_timing) {
            DISP_ERROR("Could not allocate lcd_timing structure\n");
            return ZX_ERR_NO_MEMORY;
        }
    }

    // populate values that match 1:1
    display->lcd_timing->fr_adj_type = display->disp_setting->fr_adj_type;
    display->lcd_timing->ss_level = display->disp_setting->ss_level;
    display->lcd_timing->clk_auto_gen = display->disp_setting->clk_auto_gen;
    display->lcd_timing->lcd_clock = display->disp_setting->lcd_clock;

    display->lcd_timing->hSync_width = display->disp_setting->hSync_width;
    display->lcd_timing->hSync_backPorch = display->disp_setting->hSync_backPorch;
    display->lcd_timing->hSync_pol = display->disp_setting->hSync_pol;
    display->lcd_timing->vSync_width = display->disp_setting->vSync_width;
    display->lcd_timing->vSync_backPorch = display->disp_setting->vSync_backPorch;
    display->lcd_timing->vSync_pol = display->disp_setting->vSync_pol;

    display->lcd_timing->lcd_bits = display->disp_setting->lcd_bits;
    display->lcd_timing->h_active = display->disp_setting->hActive;
    display->lcd_timing->v_active = display->disp_setting->vActive;
    display->lcd_timing->h_period = display->disp_setting->hPeriod;
    display->lcd_timing->v_period = display->disp_setting->vPeriod;

    clk = display->lcd_timing->lcd_clock;

    if (clk < 200) {
        sync_duration = clk * 100;
        display->lcd_timing->lcd_clock = clk * display->lcd_timing->h_period *
                                                display->lcd_timing->v_period;
    } else {
        sync_duration = ((clk / display->lcd_timing->h_period) * 100) /
                                                    display->lcd_timing->v_period;
    }

    display->lcd_timing->lcd_clk_dft = display->lcd_timing->lcd_clock;
    display->lcd_timing->hPeriod_dft = display->lcd_timing->h_period;
    display->lcd_timing->vPeriod_dft = display->lcd_timing->v_period;
    display->lcd_timing->sync_duration_numerator = sync_duration;
    display->lcd_timing->sync_duration_denominator = 100;


    hPeriod = display->lcd_timing->h_period;
    vPeriod = display->lcd_timing->v_period;
    hActive = display->lcd_timing->h_active;
    vActive = display->lcd_timing->v_active;

    hSync_width = display->lcd_timing->hSync_width;
    hSync_backPorch = display->lcd_timing->hSync_backPorch;
    vSync_width = display->lcd_timing->vSync_width;
    vSync_backPorch = display->lcd_timing->vSync_backPorch;

    de_hstart = hPeriod - hActive - 1;
    de_vstart = vPeriod - vActive;

    display->lcd_timing->vid_pixel_on = de_hstart;
    display->lcd_timing->vid_line_on = de_vstart;

    display->lcd_timing->de_hs_addr = de_hstart;
    display->lcd_timing->de_he_addr = de_hstart + hActive;
    display->lcd_timing->de_vs_addr = de_vstart;
    display->lcd_timing->de_ve_addr = de_vstart + vActive - 1;


    hstart = (de_hstart + hPeriod - hSync_backPorch - hSync_width) % hPeriod;
    hend = (de_hstart + hPeriod - hSync_backPorch) % hPeriod;
    display->lcd_timing->hs_hs_addr = hstart;
    display->lcd_timing->hs_he_addr = hend;
    display->lcd_timing->hs_vs_addr = 0;
    display->lcd_timing->hs_ve_addr = vPeriod - 1;

    display->lcd_timing->vs_hs_addr = (hstart + hPeriod) % hPeriod;
    display->lcd_timing->vs_he_addr = display->lcd_timing->vs_hs_addr;
    vstart = (de_vstart + vPeriod - vSync_backPorch - vSync_width) % vPeriod;
    vend = (de_vstart + vPeriod - vSync_backPorch) % vPeriod;
    display->lcd_timing->vs_vs_addr = vstart;
    display->lcd_timing->vs_ve_addr = vend;

    return status;
}