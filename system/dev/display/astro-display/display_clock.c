// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "astro-display.h"

/************************************************************************************************/
/*                              VENC Related Configs                                            */
/************************************************************************************************/

#define STV2_SEL         5
#define STV1_SEL         4
static void lcd_encl_tcon_set(astro_display_t* display)
{

    WRITE32_REG(VPU, L_RGB_BASE_ADDR, 0);
    WRITE32_REG(VPU, L_RGB_COEFF_ADDR, 0x400);

    switch (display->lcd_timing->lcd_bits) {
    case 6:
        WRITE32_REG(VPU, L_DITH_CNTL_ADDR,  0x600);
        break;
    case 8:
        WRITE32_REG(VPU, L_DITH_CNTL_ADDR,  0x400);
        break;
    case 10:
    default:
        WRITE32_REG(VPU, L_DITH_CNTL_ADDR,  0x0);
        break;
    }

    /* DE signal for TTL m8,m8m2 */
    WRITE32_REG(VPU, L_OEH_HS_ADDR, display->lcd_timing->de_hs_addr);
    WRITE32_REG(VPU, L_OEH_HE_ADDR, display->lcd_timing->de_he_addr);
    WRITE32_REG(VPU, L_OEH_VS_ADDR, display->lcd_timing->de_vs_addr);
    WRITE32_REG(VPU, L_OEH_VE_ADDR, display->lcd_timing->de_ve_addr);
    /* DE signal for TTL m8b */
    WRITE32_REG(VPU, L_OEV1_HS_ADDR,  display->lcd_timing->de_hs_addr);
    WRITE32_REG(VPU, L_OEV1_HE_ADDR,  display->lcd_timing->de_he_addr);
    WRITE32_REG(VPU, L_OEV1_VS_ADDR,  display->lcd_timing->de_vs_addr);
    WRITE32_REG(VPU, L_OEV1_VE_ADDR,  display->lcd_timing->de_ve_addr);

    /* Hsync signal for TTL m8,m8m2 */
    if (display->lcd_timing->hSync_pol == 0) {
        WRITE32_REG(VPU, L_STH1_HS_ADDR, display->lcd_timing->hs_he_addr);
        WRITE32_REG(VPU, L_STH1_HE_ADDR, display->lcd_timing->hs_hs_addr);
    } else {
        WRITE32_REG(VPU, L_STH1_HS_ADDR, display->lcd_timing->hs_hs_addr);
        WRITE32_REG(VPU, L_STH1_HE_ADDR, display->lcd_timing->hs_he_addr);
    }
    WRITE32_REG(VPU, L_STH1_VS_ADDR, display->lcd_timing->hs_vs_addr);
    WRITE32_REG(VPU, L_STH1_VE_ADDR, display->lcd_timing->hs_ve_addr);

    /* Vsync signal for TTL m8,m8m2 */
    WRITE32_REG(VPU, L_STV1_HS_ADDR, display->lcd_timing->vs_hs_addr);
    WRITE32_REG(VPU, L_STV1_HE_ADDR, display->lcd_timing->vs_he_addr);
    if (display->lcd_timing->vSync_pol == 0) {
        WRITE32_REG(VPU, L_STV1_VS_ADDR, display->lcd_timing->vs_ve_addr);
        WRITE32_REG(VPU, L_STV1_VE_ADDR, display->lcd_timing->vs_vs_addr);
    } else {
        WRITE32_REG(VPU, L_STV1_VS_ADDR, display->lcd_timing->vs_vs_addr);
        WRITE32_REG(VPU, L_STV1_VE_ADDR, display->lcd_timing->vs_ve_addr);
    }

    /* DE signal */
    WRITE32_REG(VPU, L_DE_HS_ADDR,    display->lcd_timing->de_hs_addr);
    WRITE32_REG(VPU, L_DE_HE_ADDR,    display->lcd_timing->de_he_addr);
    WRITE32_REG(VPU, L_DE_VS_ADDR,    display->lcd_timing->de_vs_addr);
    WRITE32_REG(VPU, L_DE_VE_ADDR,    display->lcd_timing->de_ve_addr);

    /* Hsync signal */
    WRITE32_REG(VPU, L_HSYNC_HS_ADDR,  display->lcd_timing->hs_hs_addr);
    WRITE32_REG(VPU, L_HSYNC_HE_ADDR,  display->lcd_timing->hs_he_addr);
    WRITE32_REG(VPU, L_HSYNC_VS_ADDR,  display->lcd_timing->hs_vs_addr);
    WRITE32_REG(VPU, L_HSYNC_VE_ADDR,  display->lcd_timing->hs_ve_addr);

    /* Vsync signal */
    WRITE32_REG(VPU, L_VSYNC_HS_ADDR,  display->lcd_timing->vs_hs_addr);
    WRITE32_REG(VPU, L_VSYNC_HE_ADDR,  display->lcd_timing->vs_he_addr);
    WRITE32_REG(VPU, L_VSYNC_VS_ADDR,  display->lcd_timing->vs_vs_addr);
    WRITE32_REG(VPU, L_VSYNC_VE_ADDR,  display->lcd_timing->vs_ve_addr);

    WRITE32_REG(VPU, L_INV_CNT_ADDR, 0);
    WRITE32_REG(VPU, L_TCON_MISC_SEL_ADDR, ((1 << STV1_SEL) | (1 << STV2_SEL)));

    WRITE32_REG(VPU, VPP_MISC, READ32_REG(VPU, VPP_MISC) & ~(VPP_OUT_SATURATE));
}

static void lcd_venc_set(astro_display_t* display)
{
    unsigned int h_active, v_active;
    unsigned int video_on_pixel, video_on_line;

    h_active = display->lcd_timing->h_active;
    v_active = display->lcd_timing->v_active;
    video_on_pixel = display->lcd_timing->vid_pixel_on;
    video_on_line = display->lcd_timing->vid_line_on;

    WRITE32_REG(VPU, ENCL_VIDEO_EN, 0);

    WRITE32_REG(VPU, VPU_VIU_VENC_MUX_CTRL, (0 << 0) | (0 << 2));    // viu1 select encl | viu2 select encl
    WRITE32_REG(VPU, ENCL_VIDEO_MODE, 0x8000); // bit[15] shadown en
    WRITE32_REG(VPU, ENCL_VIDEO_MODE_ADV, 0x0418); // Sampling rate: 1

    // bypass filter
    WRITE32_REG(VPU, ENCL_VIDEO_FILT_CTRL, 0x1000);
    WRITE32_REG(VPU, ENCL_VIDEO_MAX_PXCNT, display->lcd_timing->h_period - 1);
    WRITE32_REG(VPU, ENCL_VIDEO_MAX_LNCNT, display->lcd_timing->v_period - 1);
    WRITE32_REG(VPU, ENCL_VIDEO_HAVON_BEGIN, video_on_pixel);
    WRITE32_REG(VPU, ENCL_VIDEO_HAVON_END,   h_active - 1 + video_on_pixel);
    WRITE32_REG(VPU, ENCL_VIDEO_VAVON_BLINE, video_on_line);
    WRITE32_REG(VPU, ENCL_VIDEO_VAVON_ELINE, v_active - 1  + video_on_line);

    WRITE32_REG(VPU, ENCL_VIDEO_HSO_BEGIN, display->lcd_timing->hs_hs_addr);
    WRITE32_REG(VPU, ENCL_VIDEO_HSO_END,   display->lcd_timing->hs_he_addr);
    WRITE32_REG(VPU, ENCL_VIDEO_VSO_BEGIN, display->lcd_timing->vs_hs_addr);
    WRITE32_REG(VPU, ENCL_VIDEO_VSO_END,   display->lcd_timing->vs_he_addr);
    WRITE32_REG(VPU, ENCL_VIDEO_VSO_BLINE, display->lcd_timing->vs_vs_addr);
    WRITE32_REG(VPU, ENCL_VIDEO_VSO_ELINE, display->lcd_timing->vs_ve_addr);
    WRITE32_REG(VPU, ENCL_VIDEO_RGBIN_CTRL, 3);

    WRITE32_REG(VPU, ENCL_VIDEO_EN, 1);
}


/************************************************************************************************/
/*                              PLL / CLOCK Related Configs                                     */
/************************************************************************************************/
static unsigned int lcd_clk_div_g9_gxtvbb[][3] = {
    /* divider,        shift_val,  shift_sel */
    {CLK_DIV_SEL_1,    0xffff,     0,},
    {CLK_DIV_SEL_2,    0x0aaa,     0,},
    {CLK_DIV_SEL_3,    0x0db6,     0,},
    {CLK_DIV_SEL_3p5,  0x36cc,     1,},
    {CLK_DIV_SEL_3p75, 0x6666,     2,},
    {CLK_DIV_SEL_4,    0x0ccc,     0,},
    {CLK_DIV_SEL_5,    0x739c,     2,},
    {CLK_DIV_SEL_6,    0x0e38,     0,},
    {CLK_DIV_SEL_6p25, 0x0000,     3,},
    {CLK_DIV_SEL_7,    0x3c78,     1,},
    {CLK_DIV_SEL_7p5,  0x78f0,     2,},
    {CLK_DIV_SEL_12,   0x0fc0,     0,},
    {CLK_DIV_SEL_14,   0x3f80,     1,},
    {CLK_DIV_SEL_15,   0x7f80,     2,},
    {CLK_DIV_SEL_2p5,  0x5294,     2,},
    {CLK_DIV_SEL_MAX,  0xffff,     0,},
};

static void lcd_set_vclk_crt(astro_display_t* display)
{
    struct lcd_clk_config *cConf = display->lcd_clk_cfg;

    /* setup the XD divider value */
    SET_BIT32(HHI, HHI_VIID_CLK_DIV, (cConf->xd-1), VCLK2_XD, 8);
    usleep(5);

    /* select vid_pll_clk */
    SET_BIT32(HHI, HHI_VIID_CLK_CNTL, 0, VCLK2_CLK_IN_SEL, 3);
    SET_BIT32(HHI, HHI_VIID_CLK_CNTL, 1, VCLK2_EN, 1);
    usleep(2);

    /* [15:12] encl_clk_sel, select vclk2_div1 */
    SET_BIT32(HHI, HHI_VIID_CLK_DIV, 8, ENCL_CLK_SEL, 4);
    /* release vclk2_div_reset and enable vclk2_div */
    SET_BIT32(HHI, HHI_VIID_CLK_DIV, 1, VCLK2_XD_EN, 2);
    usleep(5);

    SET_BIT32(HHI, HHI_VIID_CLK_CNTL, 1, VCLK2_DIV1_EN, 1);
    SET_BIT32(HHI, HHI_VIID_CLK_CNTL, 1, VCLK2_SOFT_RST, 1);
    usleep(10);
    SET_BIT32(HHI, HHI_VIID_CLK_CNTL, 0, VCLK2_SOFT_RST, 1);
    usleep(5);

    /* enable CTS_ENCL clk gate */
    SET_BIT32(HHI, HHI_VID_CLK_CNTL2, 1, ENCL_GATE_VCLK, 1);
}

static void lcd_set_vid_pll_div(astro_display_t* display)
{
    unsigned int shift_val, shift_sel;
    int i;
    struct lcd_clk_config *cConf = display->lcd_clk_cfg;

    SET_BIT32(HHI, HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);
    usleep(5);

    /* Disable the div output clock */
    SET_BIT32(HHI, HHI_VID_PLL_CLK_DIV, 0, 19, 1);
    SET_BIT32(HHI, HHI_VID_PLL_CLK_DIV, 0, 15, 1);

    i = 0;
    while (lcd_clk_div_g9_gxtvbb[i][0] != CLK_DIV_SEL_MAX) {
        if (cConf->div_sel == lcd_clk_div_g9_gxtvbb[i][0])
            break;
        i++;
    }
    if (lcd_clk_div_g9_gxtvbb[i][0] == CLK_DIV_SEL_MAX)
        DISP_ERROR("invalid clk divider\n");
    shift_val = lcd_clk_div_g9_gxtvbb[i][1];
    shift_sel = lcd_clk_div_g9_gxtvbb[i][2];

    if (shift_val == 0xffff) { /* if divide by 1 */
        SET_BIT32(HHI, HHI_VID_PLL_CLK_DIV, 1, 18, 1);
    } else {
        SET_BIT32(HHI, HHI_VID_PLL_CLK_DIV, 0, 18, 1);
        SET_BIT32(HHI, HHI_VID_PLL_CLK_DIV, 0, 16, 2);
        SET_BIT32(HHI, HHI_VID_PLL_CLK_DIV, 0, 15, 1);
        SET_BIT32(HHI, HHI_VID_PLL_CLK_DIV, 0, 0, 14);

        SET_BIT32(HHI, HHI_VID_PLL_CLK_DIV, shift_sel, 16, 2);
        SET_BIT32(HHI, HHI_VID_PLL_CLK_DIV, 1, 15, 1);
        SET_BIT32(HHI, HHI_VID_PLL_CLK_DIV, shift_val, 0, 14);
        SET_BIT32(HHI, HHI_VID_PLL_CLK_DIV, 0, 15, 1);
    }
    /* Enable the final output clock */
    SET_BIT32(HHI, HHI_VID_PLL_CLK_DIV, 1, 19, 1);
}

#define PLL_WAIT_LOCK_CNT_G12A    1000
static int lcd_pll_wait_lock_g12a(astro_display_t* display)
{
    // uint32_t pll_ctrl, pll_ctrl3, pll_ctrl6;
    uint32_t pll_lock;
    int wait_loop = PLL_WAIT_LOCK_CNT_G12A; /* 200 */
    zx_status_t status = ZX_OK;

    // pll_ctrl = HHI_HDMI_PLL_CNTL0;
    // pll_ctrl3 = HHI_HDMI_PLL_CNTL3;
    // pll_ctrl6 = HHI_HDMI_PLL_CNTL6;
    do {
        usleep(50);
        pll_lock = GET_BIT32(HHI, HHI_HDMI_PLL_CNTL0, 31, 1);
        wait_loop--;
    } while ((pll_lock != 1) && (wait_loop > 0));

    if (pll_lock == 1) {
        goto pll_lock_end_g12a;
    } else {
        DISP_ERROR("pll try 1, lock: %d\n", pll_lock);
        SET_BIT32(HHI, HHI_HDMI_PLL_CNTL3, 1, 31, 1);
        wait_loop = PLL_WAIT_LOCK_CNT_G12A;
        do {
            usleep(50);
            pll_lock = GET_BIT32(HHI, HHI_HDMI_PLL_CNTL0, 31, 1);
            wait_loop--;
        } while ((pll_lock != 1) && (wait_loop > 0));
    }

    if (pll_lock == 1) {
        goto pll_lock_end_g12a;
    } else {
        DISP_ERROR("pll try 2, lock: %d\n", pll_lock);
        WRITE32_REG(HHI, HHI_HDMI_PLL_CNTL6, 0x55540000);
        wait_loop = PLL_WAIT_LOCK_CNT_G12A;
        do {
            usleep(50);
            pll_lock = GET_BIT32(HHI, HHI_HDMI_PLL_CNTL0, 31, 1);
            wait_loop--;
        } while ((pll_lock != 1) && (wait_loop > 0));
    }

    if (pll_lock != 1)
        status = ZX_ERR_CALL_FAILED;

pll_lock_end_g12a:
    DISP_ERROR("pll_lock=%d, wait_loop=%d\n", pll_lock, (PLL_WAIT_LOCK_CNT_G12A - wait_loop));
    return status;
}

zx_status_t display_clock_init(astro_display_t* display) {
    zx_status_t status = ZX_OK;
    uint32_t pll_ctrl, pll_ctrl1, pll_ctrl3, pll_ctrl4, pll_ctrl6;
    struct lcd_clk_config *cConf = display->lcd_clk_cfg;

    pll_ctrl = ((1 << LCD_PLL_EN_HPLL_G12A) |
        (1 << 25) | /* clk out gate */
        (cConf->pll_n << LCD_PLL_N_HPLL_G12A) |
        (cConf->pll_m << LCD_PLL_M_HPLL_G12A) |
        (cConf->pll_od1_sel << LCD_PLL_OD1_HPLL_G12A) |
        (cConf->pll_od2_sel << LCD_PLL_OD2_HPLL_G12A) |
        (cConf->pll_od3_sel << LCD_PLL_OD3_HPLL_G12A));
    pll_ctrl1 = (cConf->pll_frac << 0);
    if (cConf->pll_frac) {
        pll_ctrl |= (1 << 27);
        pll_ctrl3 = 0x6a285c00;
        pll_ctrl4 = 0x65771290;
        pll_ctrl6 = 0x56540000;
    } else {
        pll_ctrl3 = 0x48681c00;
        pll_ctrl4 = 0x33771290;
        pll_ctrl6 = 0x56540000;
    }

    WRITE32_REG(HHI, HHI_HDMI_PLL_CNTL0, pll_ctrl);
    WRITE32_REG(HHI, HHI_HDMI_PLL_CNTL1, pll_ctrl1);
    WRITE32_REG(HHI, HHI_HDMI_PLL_CNTL2, 0x00);
    WRITE32_REG(HHI, HHI_HDMI_PLL_CNTL3, pll_ctrl3);
    WRITE32_REG(HHI, HHI_HDMI_PLL_CNTL4, pll_ctrl4);
    WRITE32_REG(HHI, HHI_HDMI_PLL_CNTL5, 0x39272000);
    WRITE32_REG(HHI, HHI_HDMI_PLL_CNTL6, pll_ctrl6);
    SET_BIT32(HHI, HHI_HDMI_PLL_CNTL0, 1, LCD_PLL_RST_HPLL_G12A, 1);
    usleep(100);
    SET_BIT32(HHI, HHI_HDMI_PLL_CNTL0, 0, LCD_PLL_RST_HPLL_G12A, 1);

    status = lcd_pll_wait_lock_g12a(display);
    if (status != ZX_OK) {
        DISP_ERROR("hpll lock failed\n");
        goto exit;
    }

    lcd_set_vid_pll_div(display);

    SET_BIT32(HHI, HHI_VDIN_MEAS_CLK_CNTL, 0, 21, 3);
    SET_BIT32(HHI, HHI_VDIN_MEAS_CLK_CNTL, 0, 12, 7);
    SET_BIT32(HHI, HHI_VDIN_MEAS_CLK_CNTL, 1, 20, 1);

    SET_BIT32(HHI, HHI_MIPIDSI_PHY_CLK_CNTL, 0, 12, 3);
    SET_BIT32(HHI, HHI_MIPIDSI_PHY_CLK_CNTL, 1, 8, 1);
    SET_BIT32(HHI, HHI_MIPIDSI_PHY_CLK_CNTL, 0, 0, 7);

    lcd_set_vclk_crt(display);
    usleep(10000);

    lcd_venc_set(display);
    lcd_encl_tcon_set(display);

exit:
    return status;
}