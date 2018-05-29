// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "astro-display.h"

void dump_display_info(astro_display_t* display) {

    if (display->lcd_clk_cfg) {
        DISP_INFO("#############################\n");
        DISP_INFO("Dumping lcd_clk_cfg structure:\n");
        DISP_INFO("#############################\n");
        DISP_INFO("fin = 0x%x (%u)\n", display->lcd_clk_cfg->fin,display->lcd_clk_cfg->fin);
        DISP_INFO("fout = 0x%x (%u)\n", display->lcd_clk_cfg->fout,display->lcd_clk_cfg->fout);
        DISP_INFO("pll_mode = 0x%x (%u)\n", display->lcd_clk_cfg->pll_mode,display->lcd_clk_cfg->pll_mode);
        DISP_INFO("od_fb = 0x%x (%u)\n", display->lcd_clk_cfg->od_fb,display->lcd_clk_cfg->od_fb);
        DISP_INFO("pll_m = 0x%x (%u)\n", display->lcd_clk_cfg->pll_m,display->lcd_clk_cfg->pll_m);
        DISP_INFO("pll_n = 0x%x (%u)\n", display->lcd_clk_cfg->pll_n,display->lcd_clk_cfg->pll_n);
        DISP_INFO("pll_fvco = 0x%x (%u)\n", display->lcd_clk_cfg->pll_fvco,display->lcd_clk_cfg->pll_fvco);
        DISP_INFO("pll_od1_sel = 0x%x (%u)\n", display->lcd_clk_cfg->pll_od1_sel,display->lcd_clk_cfg->pll_od1_sel);
        DISP_INFO("pll_od2_sel = 0x%x (%u)\n", display->lcd_clk_cfg->pll_od2_sel,display->lcd_clk_cfg->pll_od2_sel);
        DISP_INFO("pll_od3_sel = 0x%x (%u)\n", display->lcd_clk_cfg->pll_od3_sel,display->lcd_clk_cfg->pll_od3_sel);
        DISP_INFO("pll_pi_div_sel = 0x%x (%u)\n", display->lcd_clk_cfg->pll_pi_div_sel,display->lcd_clk_cfg->pll_pi_div_sel);
        DISP_INFO("pll_level = 0x%x (%u)\n", display->lcd_clk_cfg->pll_level,display->lcd_clk_cfg->pll_level);
        DISP_INFO("pll_frac = 0x%x (%u)\n", display->lcd_clk_cfg->pll_frac,display->lcd_clk_cfg->pll_frac);
        DISP_INFO("pll_fout = 0x%x (%u)\n", display->lcd_clk_cfg->pll_fout,display->lcd_clk_cfg->pll_fout);
        DISP_INFO("ss_level = 0x%x (%u)\n", display->lcd_clk_cfg->ss_level,display->lcd_clk_cfg->ss_level);
        DISP_INFO("div_sel = 0x%x (%u)\n", display->lcd_clk_cfg->div_sel,display->lcd_clk_cfg->div_sel);
        DISP_INFO("xd = 0x%x (%u)\n", display->lcd_clk_cfg->xd,display->lcd_clk_cfg->xd);
        DISP_INFO("ss_level_max = 0x%x (%u)\n", display->lcd_clk_cfg->ss_level_max,display->lcd_clk_cfg->ss_level_max);
        DISP_INFO("pll_m_max = 0x%x (%u)\n", display->lcd_clk_cfg->pll_m_max,display->lcd_clk_cfg->pll_m_max);
        DISP_INFO("pll_m_min = 0x%x (%u)\n", display->lcd_clk_cfg->pll_m_min,display->lcd_clk_cfg->pll_m_min);
        DISP_INFO("pll_n_max = 0x%x (%u)\n", display->lcd_clk_cfg->pll_n_max,display->lcd_clk_cfg->pll_n_max);
        DISP_INFO("pll_n_min = 0x%x (%u)\n", display->lcd_clk_cfg->pll_n_min,display->lcd_clk_cfg->pll_n_min);
        DISP_INFO("pll_frac_range = 0x%x (%u)\n", display->lcd_clk_cfg->pll_frac_range,display->lcd_clk_cfg->pll_frac_range);
        DISP_INFO("pll_od_sel_max = 0x%x (%u)\n", display->lcd_clk_cfg->pll_od_sel_max,display->lcd_clk_cfg->pll_od_sel_max);
        DISP_INFO("div_sel_max = 0x%x (%u)\n", display->lcd_clk_cfg->div_sel_max,display->lcd_clk_cfg->div_sel_max);
        DISP_INFO("xd_max = 0x%x (%u)\n", display->lcd_clk_cfg->xd_max,display->lcd_clk_cfg->xd_max);
        DISP_INFO("pll_ref_fmax = 0x%x (%u)\n", display->lcd_clk_cfg->pll_ref_fmax,display->lcd_clk_cfg->pll_ref_fmax);
        DISP_INFO("pll_ref_fmin = 0x%x (%u)\n", display->lcd_clk_cfg->pll_ref_fmin,display->lcd_clk_cfg->pll_ref_fmin);
        DISP_INFO("pll_vco_fmax = 0x%x (%u)\n", display->lcd_clk_cfg->pll_vco_fmax,display->lcd_clk_cfg->pll_vco_fmax);
        DISP_INFO("pll_vco_fmin = 0x%x (%u)\n", display->lcd_clk_cfg->pll_vco_fmin,display->lcd_clk_cfg->pll_vco_fmin);
        DISP_INFO("pll_out_fmax = 0x%x (%u)\n", display->lcd_clk_cfg->pll_out_fmax,display->lcd_clk_cfg->pll_out_fmax);
        DISP_INFO("pll_out_fmin = 0x%x (%u)\n", display->lcd_clk_cfg->pll_out_fmin,display->lcd_clk_cfg->pll_out_fmin);
        DISP_INFO("div_in_fmax = 0x%x (%u)\n", display->lcd_clk_cfg->div_in_fmax,display->lcd_clk_cfg->div_in_fmax);
        DISP_INFO("div_out_fmax = 0x%x (%u)\n", display->lcd_clk_cfg->div_out_fmax,display->lcd_clk_cfg->div_out_fmax);
        DISP_INFO("xd_out_fmax = 0x%x (%u)\n", display->lcd_clk_cfg->xd_out_fmax,display->lcd_clk_cfg->xd_out_fmax);
        DISP_INFO("err_fmin = 0x%x (%u)\n", display->lcd_clk_cfg->err_fmin,display->lcd_clk_cfg->err_fmin);
    }

    if (display->disp_setting) {
        DISP_INFO("#############################\n");
        DISP_INFO("Dumping disp_setting structure:\n");
        DISP_INFO("#############################\n");
        DISP_INFO("lcd_bits = 0x%x (%u)\n", display->disp_setting->lcd_bits, display->disp_setting->lcd_bits);
        DISP_INFO("hActive = 0x%x (%u)\n", display->disp_setting->hActive, display->disp_setting->hActive);
        DISP_INFO("vActive = 0x%x (%u)\n", display->disp_setting->vActive, display->disp_setting->vActive);
        DISP_INFO("hPeriod = 0x%x (%u)\n", display->disp_setting->hPeriod, display->disp_setting->hPeriod);
        DISP_INFO("vPeriod = 0x%x (%u)\n", display->disp_setting->vPeriod, display->disp_setting->vPeriod);
        DISP_INFO("hSync_width = 0x%x (%u)\n", display->disp_setting->hSync_width, display->disp_setting->hSync_width);
        DISP_INFO("hSync_backPorch = 0x%x (%u)\n", display->disp_setting->hSync_backPorch, display->disp_setting->hSync_backPorch);
        DISP_INFO("hSync_pol = 0x%x (%u)\n", display->disp_setting->hSync_pol, display->disp_setting->hSync_pol);
        DISP_INFO("vSync_width = 0x%x (%u)\n", display->disp_setting->vSync_width, display->disp_setting->vSync_width);
        DISP_INFO("vSync_backPorch = 0x%x (%u)\n", display->disp_setting->vSync_backPorch, display->disp_setting->vSync_backPorch);
        DISP_INFO("vSync_pol = 0x%x (%u)\n", display->disp_setting->vSync_pol, display->disp_setting->vSync_pol);
        DISP_INFO("fr_adj_type = 0x%x (%u)\n", display->disp_setting->fr_adj_type, display->disp_setting->fr_adj_type);
        DISP_INFO("ss_level = 0x%x (%u)\n", display->disp_setting->ss_level, display->disp_setting->ss_level);
        DISP_INFO("clk_auto_gen = 0x%x (%u)\n", display->disp_setting->clk_auto_gen, display->disp_setting->clk_auto_gen);
        DISP_INFO("lcd_clock = 0x%x (%u)\n", display->disp_setting->lcd_clock, display->disp_setting->lcd_clock);
        DISP_INFO("lane_num = 0x%x (%u)\n", display->disp_setting->lane_num, display->disp_setting->lane_num);
        DISP_INFO("bit_rate_max = 0x%x (%u)\n", display->disp_setting->bit_rate_max, display->disp_setting->bit_rate_max);
        DISP_INFO("factor_numerator = 0x%x (%u)\n", display->disp_setting->factor_numerator, display->disp_setting->factor_numerator);
        DISP_INFO("opp_mode_init = 0x%x (%u)\n", display->disp_setting->opp_mode_init, display->disp_setting->opp_mode_init);
        DISP_INFO("opp_mode_display = 0x%x (%u)\n", display->disp_setting->opp_mode_display, display->disp_setting->opp_mode_display);
        DISP_INFO("video_mode_type = 0x%x (%u)\n", display->disp_setting->video_mode_type, display->disp_setting->video_mode_type);
        DISP_INFO("clk_always_hs = 0x%x (%u)\n", display->disp_setting->clk_always_hs, display->disp_setting->clk_always_hs);
        DISP_INFO("phy_switch = 0x%x (%u)\n", display->disp_setting->phy_switch, display->disp_setting->phy_switch);
    }

    if (display->lcd_timing) {
        DISP_INFO("#############################\n");
        DISP_INFO("Dumping lcd_timing structure:\n");
        DISP_INFO("#############################\n");
        DISP_INFO("lcd_bits = 0x%x (%u)\n", display->lcd_timing->lcd_bits, display->lcd_timing->lcd_bits);
        DISP_INFO("h_active = 0x%x (%u)\n", display->lcd_timing->h_active, display->lcd_timing->h_active);
        DISP_INFO("v_active = 0x%x (%u)\n", display->lcd_timing->v_active, display->lcd_timing->v_active);
        DISP_INFO("h_period = 0x%x (%u)\n", display->lcd_timing->h_period, display->lcd_timing->h_period);
        DISP_INFO("v_period = 0x%x (%u)\n", display->lcd_timing->v_period, display->lcd_timing->v_period);
        DISP_INFO("fr_adj_type = 0x%x (%u)\n", display->lcd_timing->fr_adj_type, display->lcd_timing->fr_adj_type);
        DISP_INFO("ss_level = 0x%x (%u)\n", display->lcd_timing->ss_level, display->lcd_timing->ss_level);
        DISP_INFO("clk_auto_gen = 0x%x (%u)\n", display->lcd_timing->clk_auto_gen, display->lcd_timing->clk_auto_gen);
        DISP_INFO("lcd_clock = 0x%x (%u)\n", display->lcd_timing->lcd_clock, display->lcd_timing->lcd_clock);
        DISP_INFO("clk_ctrl = 0x%x (%u)\n", display->lcd_timing->clk_ctrl, display->lcd_timing->clk_ctrl);
        DISP_INFO("pll_ctrl = 0x%x (%u)\n", display->lcd_timing->pll_ctrl, display->lcd_timing->pll_ctrl);
        DISP_INFO("div_ctrl = 0x%x (%u)\n", display->lcd_timing->div_ctrl, display->lcd_timing->div_ctrl);
        DISP_INFO("clk_change = 0x%x (%u)\n", display->lcd_timing->clk_change, display->lcd_timing->clk_change);
        DISP_INFO("lcd_clk_dft = 0x%x (%u)\n", display->lcd_timing->lcd_clk_dft, display->lcd_timing->lcd_clk_dft);
        DISP_INFO("hPeriod_dft = 0x%x (%u)\n", display->lcd_timing->hPeriod_dft, display->lcd_timing->hPeriod_dft);
        DISP_INFO("vPeriod_dft = 0x%x (%u)\n", display->lcd_timing->vPeriod_dft, display->lcd_timing->vPeriod_dft);
        DISP_INFO("sync_duration_numerator = 0x%x (%u)\n", display->lcd_timing->sync_duration_numerator, display->lcd_timing->sync_duration_numerator);
        DISP_INFO("sync_duration_denominator = 0x%x (%u)\n", display->lcd_timing->sync_duration_denominator, display->lcd_timing->sync_duration_denominator);
        DISP_INFO("vid_pixel_on = 0x%x (%u)\n", display->lcd_timing->vid_pixel_on, display->lcd_timing->vid_pixel_on);
        DISP_INFO("vid_line_on = 0x%x (%u)\n", display->lcd_timing->vid_line_on, display->lcd_timing->vid_line_on);
        DISP_INFO("hSync_width = 0x%x (%u)\n", display->lcd_timing->hSync_width, display->lcd_timing->hSync_width);
        DISP_INFO("hSync_backPorch = 0x%x (%u)\n", display->lcd_timing->hSync_backPorch, display->lcd_timing->hSync_backPorch);
        DISP_INFO("hSync_pol = 0x%x (%u)\n", display->lcd_timing->hSync_pol, display->lcd_timing->hSync_pol);
        DISP_INFO("vSync_width = 0x%x (%u)\n", display->lcd_timing->vSync_width, display->lcd_timing->vSync_width);
        DISP_INFO("vSync_backPorch = 0x%x (%u)\n", display->lcd_timing->vSync_backPorch, display->lcd_timing->vSync_backPorch);
        DISP_INFO("vSync_pol = 0x%x (%u)\n", display->lcd_timing->vSync_pol, display->lcd_timing->vSync_pol);
        DISP_INFO("hOffset = 0x%x (%u)\n", display->lcd_timing->hOffset, display->lcd_timing->hOffset);
        DISP_INFO("vOffset = 0x%x (%u)\n", display->lcd_timing->vOffset, display->lcd_timing->vOffset);
        DISP_INFO("de_hs_addr = 0x%x (%u)\n", display->lcd_timing->de_hs_addr, display->lcd_timing->de_hs_addr);
        DISP_INFO("de_he_addr = 0x%x (%u)\n", display->lcd_timing->de_he_addr, display->lcd_timing->de_he_addr);
        DISP_INFO("de_vs_addr = 0x%x (%u)\n", display->lcd_timing->de_vs_addr, display->lcd_timing->de_vs_addr);
        DISP_INFO("de_ve_addr = 0x%x (%u)\n", display->lcd_timing->de_ve_addr, display->lcd_timing->de_ve_addr);
        DISP_INFO("hs_hs_addr = 0x%x (%u)\n", display->lcd_timing->hs_hs_addr, display->lcd_timing->hs_hs_addr);
        DISP_INFO("hs_he_addr = 0x%x (%u)\n", display->lcd_timing->hs_he_addr, display->lcd_timing->hs_he_addr);
        DISP_INFO("hs_vs_addr = 0x%x (%u)\n", display->lcd_timing->hs_vs_addr, display->lcd_timing->hs_vs_addr);
        DISP_INFO("hs_ve_addr = 0x%x (%u)\n", display->lcd_timing->hs_ve_addr, display->lcd_timing->hs_ve_addr);
        DISP_INFO("vs_hs_addr = 0x%x (%u)\n", display->lcd_timing->vs_hs_addr, display->lcd_timing->vs_hs_addr);
        DISP_INFO("vs_he_addr = 0x%x (%u)\n", display->lcd_timing->vs_he_addr, display->lcd_timing->vs_he_addr);
        DISP_INFO("vs_vs_addr = 0x%x (%u)\n", display->lcd_timing->vs_vs_addr, display->lcd_timing->vs_vs_addr);
        DISP_INFO("vs_ve_addr = 0x%x (%u)\n", display->lcd_timing->vs_ve_addr, display->lcd_timing->vs_ve_addr);
    }

    if (display->dsi_cfg) {
        DISP_INFO("#############################\n");
        DISP_INFO("Dumping dsi_cfg structure:\n");
        DISP_INFO("#############################\n");
        DISP_INFO("lane_num = 0x%x (%u)\n", display->dsi_cfg->lane_num ,display->dsi_cfg->lane_num);
        DISP_INFO("bit_rate_max = 0x%x (%u)\n", display->dsi_cfg->bit_rate_max ,display->dsi_cfg->bit_rate_max);
        DISP_INFO("bit_rate_min = 0x%x (%u)\n", display->dsi_cfg->bit_rate_min ,display->dsi_cfg->bit_rate_min);
        DISP_INFO("bit_rate = 0x%x (%u)\n", display->dsi_cfg->bit_rate ,display->dsi_cfg->bit_rate);
        DISP_INFO("clock_factor = 0x%x (%u)\n", display->dsi_cfg->clock_factor ,display->dsi_cfg->clock_factor);
        DISP_INFO("factor_numerator = 0x%x (%u)\n", display->dsi_cfg->factor_numerator ,display->dsi_cfg->factor_numerator);
        DISP_INFO("factor_denominator = 0x%x (%u)\n", display->dsi_cfg->factor_denominator ,display->dsi_cfg->factor_denominator);
        DISP_INFO("opp_mode_init = 0x%x (%u)\n", display->dsi_cfg->opp_mode_init ,display->dsi_cfg->opp_mode_init);
        DISP_INFO("opp_mode_display = 0x%x (%u)\n", display->dsi_cfg->opp_mode_display ,display->dsi_cfg->opp_mode_display);
        DISP_INFO("video_mode_type = 0x%x (%u)\n", display->dsi_cfg->video_mode_type ,display->dsi_cfg->video_mode_type);
        DISP_INFO("clk_always_hs = 0x%x (%u)\n", display->dsi_cfg->clk_always_hs ,display->dsi_cfg->clk_always_hs);
        DISP_INFO("phy_switch = 0x%x (%u)\n", display->dsi_cfg->phy_switch ,display->dsi_cfg->phy_switch);
        DISP_INFO("venc_data_width = 0x%x (%u)\n", display->dsi_cfg->venc_data_width ,display->dsi_cfg->venc_data_width);
        DISP_INFO("dpi_data_format = 0x%x (%u)\n", display->dsi_cfg->dpi_data_format ,display->dsi_cfg->dpi_data_format);
        DISP_INFO("hLine = 0x%x (%u)\n", display->dsi_cfg->hLine ,display->dsi_cfg->hLine);
        DISP_INFO("hSyncActive = 0x%x (%u)\n", display->dsi_cfg->hSyncActive ,display->dsi_cfg->hSyncActive);
        DISP_INFO("hBackporch = 0x%x (%u)\n", display->dsi_cfg->hBackporch ,display->dsi_cfg->hBackporch);
        DISP_INFO("vSyncActive = 0x%x (%u)\n", display->dsi_cfg->vSyncActive ,display->dsi_cfg->vSyncActive);
        DISP_INFO("vBackporch = 0x%x (%u)\n", display->dsi_cfg->vBackporch ,display->dsi_cfg->vBackporch);
        DISP_INFO("vFrontporch = 0x%x (%u)\n", display->dsi_cfg->vFrontporch ,display->dsi_cfg->vFrontporch);
        DISP_INFO("vActiveLines = 0x%x (%u)\n", display->dsi_cfg->vActiveLines ,display->dsi_cfg->vActiveLines);
    }

    if (display->dsi_phy_cfg) {
        DISP_INFO("#############################\n");
        DISP_INFO("Dumping dsi_phy_cfg structure:\n");
        DISP_INFO("#############################\n");
        DISP_INFO("lp_tesc = 0x%x (%u)\n", display->dsi_phy_cfg->lp_tesc, display->dsi_phy_cfg->lp_tesc);
        DISP_INFO("lp_lpx = 0x%x (%u)\n", display->dsi_phy_cfg->lp_lpx, display->dsi_phy_cfg->lp_lpx);
        DISP_INFO("lp_ta_sure = 0x%x (%u)\n", display->dsi_phy_cfg->lp_ta_sure, display->dsi_phy_cfg->lp_ta_sure);
        DISP_INFO("lp_ta_go = 0x%x (%u)\n", display->dsi_phy_cfg->lp_ta_go, display->dsi_phy_cfg->lp_ta_go);
        DISP_INFO("lp_ta_get = 0x%x (%u)\n", display->dsi_phy_cfg->lp_ta_get, display->dsi_phy_cfg->lp_ta_get);
        DISP_INFO("hs_exit = 0x%x (%u)\n", display->dsi_phy_cfg->hs_exit, display->dsi_phy_cfg->hs_exit);
        DISP_INFO("hs_trail = 0x%x (%u)\n", display->dsi_phy_cfg->hs_trail, display->dsi_phy_cfg->hs_trail);
        DISP_INFO("hs_zero = 0x%x (%u)\n", display->dsi_phy_cfg->hs_zero, display->dsi_phy_cfg->hs_zero);
        DISP_INFO("hs_prepare = 0x%x (%u)\n", display->dsi_phy_cfg->hs_prepare, display->dsi_phy_cfg->hs_prepare);
        DISP_INFO("clk_trail = 0x%x (%u)\n", display->dsi_phy_cfg->clk_trail, display->dsi_phy_cfg->clk_trail);
        DISP_INFO("clk_post = 0x%x (%u)\n", display->dsi_phy_cfg->clk_post, display->dsi_phy_cfg->clk_post);
        DISP_INFO("clk_zero = 0x%x (%u)\n", display->dsi_phy_cfg->clk_zero, display->dsi_phy_cfg->clk_zero);
        DISP_INFO("clk_prepare = 0x%x (%u)\n", display->dsi_phy_cfg->clk_prepare, display->dsi_phy_cfg->clk_prepare);
        DISP_INFO("clk_pre = 0x%x (%u)\n", display->dsi_phy_cfg->clk_pre, display->dsi_phy_cfg->clk_pre);
        DISP_INFO("init = 0x%x (%u)\n", display->dsi_phy_cfg->init, display->dsi_phy_cfg->init);
        DISP_INFO("wakeup = 0x%x (%u)\n", display->dsi_phy_cfg->wakeup, display->dsi_phy_cfg->wakeup);
        DISP_INFO("state_change = 0x%x (%u)\n", display->dsi_phy_cfg->state_change, display->dsi_phy_cfg->state_change);
    }

    if (display->dsi_vid) {
        DISP_INFO("#############################\n");
        DISP_INFO("Dumping dsi_vid structure:\n");
        DISP_INFO("#############################\n");
        DISP_INFO("data_bits = 0x%x (%d)\n", display->dsi_vid->data_bits, display->dsi_vid->data_bits);
        DISP_INFO("vid_num_chunks = 0x%x (%d)\n", display->dsi_vid->vid_num_chunks, display->dsi_vid->vid_num_chunks);
        DISP_INFO("pixel_per_chunk = 0x%x (%d)\n", display->dsi_vid->pixel_per_chunk, display->dsi_vid->pixel_per_chunk);
        DISP_INFO("vid_null_size = 0x%x (%d)\n", display->dsi_vid->vid_null_size, display->dsi_vid->vid_null_size);
        DISP_INFO("byte_per_chunk = 0x%x (%d)\n", display->dsi_vid->byte_per_chunk, display->dsi_vid->byte_per_chunk);
        DISP_INFO("multi_pkt_en = 0x%x (%d)\n", display->dsi_vid->multi_pkt_en, display->dsi_vid->multi_pkt_en);
        DISP_INFO("hline = 0x%x (%u)\n", display->dsi_vid->hline, display->dsi_vid->hline);
        DISP_INFO("hsa = 0x%x (%u)\n", display->dsi_vid->hsa, display->dsi_vid->hsa);
        DISP_INFO("hbp = 0x%x (%u)\n", display->dsi_vid->hbp, display->dsi_vid->hbp);
        DISP_INFO("vsa = 0x%x (%u)\n", display->dsi_vid->vsa, display->dsi_vid->vsa);
        DISP_INFO("vbp = 0x%x (%u)\n", display->dsi_vid->vbp, display->dsi_vid->vbp);
        DISP_INFO("vfp = 0x%x (%u)\n", display->dsi_vid->vfp, display->dsi_vid->vfp);
        DISP_INFO("vact = 0x%x (%u)\n", display->dsi_vid->vact, display->dsi_vid->vact);
    }
}
