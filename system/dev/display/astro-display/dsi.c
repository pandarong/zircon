// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "astro-display.h"

#define CMD_TIMEOUT_CNT                 3000


void lcd_mipi_phy_set(astro_display_t* display, bool enable) {
    uint32_t phy_bit, phy_width;
    uint32_t lane_cnt;

    if (enable) {
        /* HHI_MIPI_CNTL0 */
        /* DIF_REF_CTL1:31-16bit, DIF_REF_CTL0:15-0bit */
        WRITE32_REG(HHI, HHI_MIPI_CNTL0, (0xa487 << 16) | (0x8 << 0));

        /* HHI_MIPI_CNTL1 */
        /* DIF_REF_CTL2:15-0bit; bandgap bit16 */
        WRITE32_REG(HHI, HHI_MIPI_CNTL1, (0x1 << 16) | (0x002e << 0));

        /* HHI_MIPI_CNTL2 */
        /* DIF_TX_CTL1:31-16bit, DIF_TX_CTL0:15-0bit */
        WRITE32_REG(HHI, HHI_MIPI_CNTL2, (0x2680 << 16) | (0x45a << 0));

        phy_bit = MIPI_PHY_LANE_BIT;
        phy_width = MIPI_PHY_LANE_WIDTH;
        switch (display->dsi_cfg->lane_num) {
        case 1:
            lane_cnt = DSI_LANE_COUNT_1;
            break;
        case 2:
            lane_cnt = DSI_LANE_COUNT_2;
            break;
        case 3:
            lane_cnt = DSI_LANE_COUNT_3;
            break;
        case 4:
            lane_cnt = DSI_LANE_COUNT_4;
            break;
        default:
            lane_cnt = 0;
            break;
        }
        SET_BIT32(HHI, HHI_MIPI_CNTL2, lane_cnt, phy_bit, phy_width);
    } else {
        WRITE32_REG(HHI, HHI_MIPI_CNTL0, 0);
        WRITE32_REG(HHI, HHI_MIPI_CNTL1, 0);
        WRITE32_REG(HHI, HHI_MIPI_CNTL2, 0);
    }
}

#define max(x, y) ((x > y)? x : y)

static void mipi_dsi_phy_config(astro_display_t* display, uint32_t dsi_ui) {
    uint32_t temp, t_ui, t_req_min, t_req_max, t_req, n;

    t_ui = (1000000 * 100) / (dsi_ui / 1000); /* 0.01ns*100 */
    temp = t_ui * 8; /* lane_byte cycle time */

    display->dsi_phy_cfg->lp_tesc = ((DPHY_TIME_LP_TESC(t_ui) + temp - 1) / temp) & 0xff;
    display->dsi_phy_cfg->lp_lpx = ((DPHY_TIME_LP_LPX(t_ui) + temp - 1) / temp) & 0xff;
    display->dsi_phy_cfg->lp_ta_sure = ((DPHY_TIME_LP_TA_SURE(t_ui) + temp - 1) / temp) &
            0xff;
    display->dsi_phy_cfg->lp_ta_go = ((DPHY_TIME_LP_TA_GO(t_ui) + temp - 1) / temp) & 0xff;
    display->dsi_phy_cfg->lp_ta_get = ((DPHY_TIME_LP_TA_GETX(t_ui) + temp - 1) / temp) &
            0xff;
    display->dsi_phy_cfg->hs_exit = ((DPHY_TIME_HS_EXIT(t_ui) + temp - 1) / temp) & 0xff;
    display->dsi_phy_cfg->clk_prepare = ((DPHY_TIME_CLK_PREPARE(t_ui) + temp - 1) / temp) &
            0xff;
    display->dsi_phy_cfg->clk_zero = ((DPHY_TIME_CLK_ZERO(t_ui) + temp - 1) / temp) & 0xff;
    display->dsi_phy_cfg->clk_pre = ((DPHY_TIME_CLK_PRE(t_ui) + temp - 1) / temp) & 0xff;
    display->dsi_phy_cfg->init = (DPHY_TIME_INIT(t_ui) + temp - 1) / temp;
    display->dsi_phy_cfg->wakeup = (DPHY_TIME_WAKEUP(t_ui) + temp - 1) / temp;

    t_req_max = ((105 * 100 + 12 * t_ui) / 100);
    for (n = 0; n <= 0xff; n++) {
        display->dsi_phy_cfg->clk_trail = n;
        if (((temp * display->dsi_phy_cfg->clk_trail / 100) > 70) &&
            ((temp * display->dsi_phy_cfg->clk_trail / 100) < t_req_max)) {
            DISP_INFO("t_ui=%d, t_req=%d\n", t_ui, t_req_max);
            DISP_INFO("clk_trail=%d, n=%d\n", display->dsi_phy_cfg->clk_trail, n);

            break;
        }
    }

    t_req_min = 2 * ((60 * 100 + 52 * t_ui) / 100);
    for (n = 0; n <= 0xff; n++) {
        display->dsi_phy_cfg->clk_post = n;
        if ((temp * display->dsi_phy_cfg->clk_post / 100) >= t_req_min) {
            DISP_INFO("t_ui=%d, t_req_min=%d\n", t_ui, t_req_min);
            DISP_INFO("clk_post=%d, n=%d\n", display->dsi_phy_cfg->clk_post, n);

            break;
        }
    }

    t_req_min = max(8 * t_ui / 100, (60 * 100 + 4 * t_ui) / 100) + 10;
    t_req_max = ((105 * 100 + 12 * t_ui) / 100);
    for (n = 0; n <= 0xff; n++) {
        display->dsi_phy_cfg->hs_trail = n;
        if (((temp * display->dsi_phy_cfg->hs_trail / 100) > t_req_min) &&
            ((temp * display->dsi_phy_cfg->hs_trail / 100) < t_req_max)) {
            DISP_INFO("t_req_min=%d, t_req_max=%d\n", t_req_min, t_req_max);
            DISP_INFO("t_ui=%d, hs_trail=%d, n=%d\n", t_ui, display->dsi_phy_cfg->hs_trail, n);

            break;
        }
    }

    t_req_min = (40 * 100 + 4 * t_ui) / 100;
    t_req_max = ((85 * 100 + 6 * t_ui) / 100);
    for (n = 0; n <= 0xff; n++) {
        display->dsi_phy_cfg->hs_prepare = n;
        if (((temp * display->dsi_phy_cfg->hs_prepare / 100) > t_req_min) &&
            ((temp * display->dsi_phy_cfg->hs_prepare / 100) < t_req_max)) {
            DISP_INFO("t_req_min=%d, t_req_max=%d\n", t_req_min, t_req_max);
            DISP_INFO("t_ui=%d, hs_prepare=%d, n=%d\n", t_ui, display->dsi_phy_cfg->hs_prepare, n);

            break;
        }
    }

    t_req_min = ((145 * 100 + 10 * t_ui) / 100) - ((40 * 100 + 4 * t_ui) / 100);
    for (n = 0; n <= 0xff; n++) {
        display->dsi_phy_cfg->hs_zero = n;
        if ((temp * display->dsi_phy_cfg->hs_zero / 100) > t_req_min) {
            DISP_INFO("t_ui=%d, t_req_min=%d\n", t_ui, t_req_min);
            DISP_INFO("hs_zero=%d, n=%d\n", display->dsi_phy_cfg->hs_zero, n);

            break;
        }
    }

    /* check dphy spec: (unit: ns) */
    if ((temp * display->dsi_phy_cfg->lp_tesc / 100) <= 100)
        DISP_ERROR("lp_tesc timing error\n");
    if ((temp * display->dsi_phy_cfg->lp_lpx / 100) <= 50)
        DISP_ERROR("lp_lpx timing error\n");
    if ((temp * display->dsi_phy_cfg->hs_exit / 100) <= 100)
        DISP_ERROR("hs_exit timing error\n");
    t_req = ((t_ui > (60 * 100 / 4)) ?
        (8 * t_ui) : ((60 * 100) + 4 * t_ui));
    if ((temp * display->dsi_phy_cfg->hs_trail / 100) <= ((t_req + 50) / 100))
        DISP_ERROR("hs_trail timing error\n");
    t_req = temp * display->dsi_phy_cfg->hs_prepare / 100;
    if ((t_req <= (40 + (t_ui * 4 / 100))) ||
        (t_req >= (85 + (t_ui * 6 / 100))))
        DISP_ERROR("hs_prepare timing error\n");
    t_req = 145 + (t_ui * 10 / 100);
    if (((temp * display->dsi_phy_cfg->hs_zero / 100) +
        (temp * display->dsi_phy_cfg->hs_prepare / 100)) <= t_req)
        DISP_ERROR("hs_zero timing error\n");
    if ((temp * display->dsi_phy_cfg->init / 100) <= 100000)
        DISP_ERROR("init timing error\n");
    if ((temp * display->dsi_phy_cfg->wakeup / 100) <= 1000000)
        DISP_ERROR("wakeup timing error\n");

    DISP_INFO("%s:\n"
        "lp_tesc     = 0x%02x\n"
        "lp_lpx      = 0x%02x\n"
        "lp_ta_sure  = 0x%02x\n"
        "lp_ta_go    = 0x%02x\n"
        "lp_ta_get   = 0x%02x\n"
        "hs_exit     = 0x%02x\n"
        "hs_trail    = 0x%02x\n"
        "hs_zero     = 0x%02x\n"
        "hs_prepare  = 0x%02x\n"
        "clk_trail   = 0x%02x\n"
        "clk_post    = 0x%02x\n"
        "clk_zero    = 0x%02x\n"
        "clk_prepare = 0x%02x\n"
        "clk_pre     = 0x%02x\n"
        "init        = 0x%02x\n"
        "wakeup      = 0x%02x\n\n",
        __func__,
        display->dsi_phy_cfg->lp_tesc, display->dsi_phy_cfg->lp_lpx, display->dsi_phy_cfg->lp_ta_sure,
        display->dsi_phy_cfg->lp_ta_go, display->dsi_phy_cfg->lp_ta_get, display->dsi_phy_cfg->hs_exit,
        display->dsi_phy_cfg->hs_trail, display->dsi_phy_cfg->hs_zero, display->dsi_phy_cfg->hs_prepare,
        display->dsi_phy_cfg->clk_trail, display->dsi_phy_cfg->clk_post,
        display->dsi_phy_cfg->clk_zero, display->dsi_phy_cfg->clk_prepare, display->dsi_phy_cfg->clk_pre,
        display->dsi_phy_cfg->init, display->dsi_phy_cfg->wakeup);
}

static void mipi_dsi_video_config(astro_display_t* display)
{
    uint32_t h_period, hs_width, hs_bp;
    uint32_t den, num;
    uint32_t v_period, v_active, vs_width, vs_bp;

    h_period = display->lcd_timing->h_period;
    hs_width = display->lcd_timing->hSync_width;
    hs_bp = display->lcd_timing->hSync_backPorch;
    den = display->dsi_cfg->factor_denominator;
    num = display->dsi_cfg->factor_numerator;

    display->dsi_vid->hline = (h_period * den + num - 1) / num;
    display->dsi_vid->hsa = (hs_width * den + num - 1) / num;
    display->dsi_vid->hbp = (hs_bp * den + num - 1) / num;

    v_period = display->lcd_timing->v_period;
    v_active = display->lcd_timing->v_active;
    vs_width = display->lcd_timing->vSync_width;
    vs_bp = display->lcd_timing->vSync_backPorch;
    display->dsi_vid->vsa = vs_width;
    display->dsi_vid->vbp = vs_bp;
    display->dsi_vid->vfp = v_period - v_active - vs_bp - vs_width;
    display->dsi_vid->vact = v_active;

    DISP_INFO("MIPI DSI video timing:\n"
        "  HLINE     = %d\n"
        "  HSA       = %d\n"
        "  HBP       = %d\n"
        "  VSA       = %d\n"
        "  VBP       = %d\n"
        "  VFP       = %d\n"
        "  VACT      = %d\n\n",
        display->dsi_vid->hline, display->dsi_vid->hsa, display->dsi_vid->hbp,
        display->dsi_vid->vsa, display->dsi_vid->vbp, display->dsi_vid->vfp, display->dsi_vid->vact);
}

static void mipi_dsi_vid_mode_config(astro_display_t* display)
{
    if (display->dsi_cfg->video_mode_type == BURST_MODE) {
        display->dsi_vid->pixel_per_chunk = display->lcd_timing->h_active;
        display->dsi_vid->vid_num_chunks = 0;
        display->dsi_vid->vid_null_size = 0;
    } else {
        DISP_ERROR("Non-Burst mode is not supported\n");
        while(1);
    }

    mipi_dsi_video_config(display);
}

static void mipi_dsi_config_post(astro_display_t* display)
{
    uint32_t pclk, lanebyteclk;
    uint32_t den, num;

    pclk = display->lcd_timing->lcd_clock / 1000;

    /* pclk lanebyteclk factor */
    if (display->dsi_cfg->factor_numerator == 0) {
        lanebyteclk = display->dsi_cfg->bit_rate / 8 / 1000;
        DISP_INFO("pixel_clk = %d.%03dMHz, bit_rate = %d.%03dMHz, lanebyteclk = %d.%03dMHz\n",
            (pclk / 1000), (pclk % 1000),
            (display->dsi_cfg->bit_rate / 1000000),
            ((display->dsi_cfg->bit_rate / 1000) % 1000),
            (lanebyteclk / 1000), (lanebyteclk % 1000));
        display->dsi_cfg->factor_numerator = 8;
        display->dsi_cfg->factor_denominator = display->dsi_cfg->clock_factor;
    }
    num = display->dsi_cfg->factor_numerator;
    den = display->dsi_cfg->factor_denominator;
    DISP_INFO("num=%d, den=%d, factor=%d.%02d\n",
        num, den, (den / num), ((den % num) * 100 / num));

    if (display->dsi_cfg->opp_mode_display == OPERATION_VIDEO_MODE) {
        mipi_dsi_vid_mode_config(display);
    }

    /* phy config */
    mipi_dsi_phy_config(display, display->dsi_cfg->bit_rate);
}

static void mipi_dcs_set(astro_display_t* display, int trans_type, int req_ack, int tear_en)
{
    WRITE32_REG(MIPI_DSI, DW_DSI_CMD_MODE_CFG,
        (trans_type << BIT_MAX_RD_PKT_SIZE) |
        (trans_type << BIT_DCS_LW_TX)    |
        (trans_type << BIT_DCS_SR_0P_TX) |
        (trans_type << BIT_DCS_SW_1P_TX) |
        (trans_type << BIT_DCS_SW_0P_TX) |
        (trans_type << BIT_GEN_LW_TX)    |
        (trans_type << BIT_GEN_SR_2P_TX) |
        (trans_type << BIT_GEN_SR_1P_TX) |
        (trans_type << BIT_GEN_SR_0P_TX) |
        (trans_type << BIT_GEN_SW_2P_TX) |
        (trans_type << BIT_GEN_SW_1P_TX) |
        (trans_type << BIT_GEN_SW_0P_TX) |
        (req_ack << BIT_ACK_RQST_EN)     |
        (tear_en << BIT_TEAR_FX_EN));

    if (tear_en == MIPI_DCS_ENABLE_TEAR) {
        /* Enable Tear Interrupt if tear_en is valid */
        SET_BIT32(MIPI_DSI, MIPI_DSI_TOP_INTR_CNTL_STAT, 0x1, BIT_EDPITE_INT_EN, 1);
        /* Enable Measure Vsync */
        SET_BIT32(MIPI_DSI, MIPI_DSI_TOP_MEAS_CNTL, 0x1, BIT_VSYNC_MEAS_EN, 1);
        SET_BIT32(MIPI_DSI, MIPI_DSI_TOP_MEAS_CNTL, 0x1, BIT_TE_MEAS_EN, 1);
    }

    /* Packet header settings */
    WRITE32_REG(MIPI_DSI, DW_DSI_PCKHDL_CFG,
        (1 << BIT_CRC_RX_EN)  |
        (1 << BIT_ECC_RX_EN)  |
        (req_ack << BIT_BTA_EN)     |
        (0 << BIT_EOTP_RX_EN) |
        (0 << BIT_EOTP_TX_EN));
}


static void set_mipi_dsi_host(astro_display_t* display, uint32_t vcid, uint32_t chroma_subsample,
        uint32_t operation_mode)
{
    uint32_t dpi_data_format, venc_data_width;
    uint32_t lane_num, vid_mode_type;
    uint32_t temp;

    venc_data_width = display->dsi_cfg->venc_data_width;
    dpi_data_format = display->dsi_cfg->dpi_data_format;
    lane_num        = (uint32_t)(display->dsi_cfg->lane_num);
    vid_mode_type   = (uint32_t)(display->dsi_cfg->video_mode_type);

    /* ----------------------------------------------------- */
    /* Standard Configuration for Video Mode Operation */
    /* ----------------------------------------------------- */
    /* 1,    Configure Lane number and phy stop wait time */
    if (display->dsi_phy_cfg->state_change == 2) {
        WRITE32_REG(MIPI_DSI, DW_DSI_PHY_IF_CFG,
            (0x28 << BIT_PHY_STOP_WAIT_TIME) |
            ((lane_num-1) << BIT_N_LANES));
    } else {
        WRITE32_REG(MIPI_DSI, DW_DSI_PHY_IF_CFG,
            (1 << BIT_PHY_STOP_WAIT_TIME) |
            ((lane_num-1) << BIT_N_LANES));
    }

    /* 2.1,  Configure Virtual channel settings */
    WRITE32_REG(MIPI_DSI, DW_DSI_DPI_VCID, vcid);
    /* 2.2,  Configure Color format */
    WRITE32_REG(MIPI_DSI, DW_DSI_DPI_COLOR_CODING,
        (((dpi_data_format == COLOR_18BIT_CFG_2) ?
            1 : 0) << BIT_LOOSELY18_EN) |
        (dpi_data_format << BIT_DPI_COLOR_CODING));
    /* 2.2.1 Configure Set color format for DPI register */
    temp = (READ32_REG(MIPI_DSI, MIPI_DSI_TOP_CNTL) &
        ~(0xf<<BIT_DPI_COLOR_MODE) &
        ~(0x7<<BIT_IN_COLOR_MODE) &
        ~(0x3<<BIT_CHROMA_SUBSAMPLE));
    WRITE32_REG(MIPI_DSI, MIPI_DSI_TOP_CNTL,
        (temp |
        (dpi_data_format  << BIT_DPI_COLOR_MODE)  |
        (venc_data_width  << BIT_IN_COLOR_MODE)   |
        (chroma_subsample << BIT_CHROMA_SUBSAMPLE)));
    /* 2.3   Configure Signal polarity */
    WRITE32_REG(MIPI_DSI, DW_DSI_DPI_CFG_POL,
        (0x0 << BIT_COLORM_ACTIVE_LOW) |
        (0x0 << BIT_SHUTD_ACTIVE_LOW)  |
        (0 << BIT_HSYNC_ACTIVE_LOW)    |
        (0 << BIT_VSYNC_ACTIVE_LOW)    |
        (0x0 << BIT_DATAEN_ACTIVE_LOW));

    if (operation_mode == OPERATION_VIDEO_MODE) {
        /* 3.1   Configure Low power and video mode type settings */
        WRITE32_REG(MIPI_DSI, DW_DSI_VID_MODE_CFG,
            (1 << BIT_LP_HFP_EN)  |       /* enalbe lp */
            (1 << BIT_LP_HBP_EN)  |       /* enalbe lp */
            (1 << BIT_LP_VCAT_EN) |       /* enalbe lp */
            (1 << BIT_LP_VFP_EN)  |       /* enalbe lp */
            (1 << BIT_LP_VBP_EN)  |       /* enalbe lp */
            (1 << BIT_LP_VSA_EN)  |       /* enalbe lp */
            (0 << BIT_FRAME_BTA_ACK_EN) |
               /* enable BTA after one frame, TODO, need check */
            /* (1 << BIT_LP_CMD_EN) |  */
               /* enable the command transmission only in lp mode */
            (vid_mode_type << BIT_VID_MODE_TYPE));
                    /* burst non burst mode */
        /* [23:16]outvact, [7:0]invact */
        WRITE32_REG(MIPI_DSI, DW_DSI_DPI_LP_CMD_TIM,
            (4 << 16) | (4 << 0));
        /* 3.2   Configure video packet size settings */
        WRITE32_REG(MIPI_DSI, DW_DSI_VID_PKT_SIZE,
            display->dsi_vid->pixel_per_chunk);
        WRITE32_REG(MIPI_DSI, DW_DSI_VID_NUM_CHUNKS,
            display->dsi_vid->vid_num_chunks);
        WRITE32_REG(MIPI_DSI, DW_DSI_VID_NULL_SIZE,
            display->dsi_vid->vid_null_size);
        /* 4 Configure the video relative parameters according to
         *     the output type
         */
        /* include horizontal timing and vertical line */
        WRITE32_REG(MIPI_DSI, DW_DSI_VID_HLINE_TIME, display->dsi_vid->hline);
        WRITE32_REG(MIPI_DSI, DW_DSI_VID_HSA_TIME, display->dsi_vid->hsa);
        WRITE32_REG(MIPI_DSI, DW_DSI_VID_HBP_TIME, display->dsi_vid->hbp);
        WRITE32_REG(MIPI_DSI, DW_DSI_VID_VSA_LINES, display->dsi_vid->vsa);
        WRITE32_REG(MIPI_DSI, DW_DSI_VID_VBP_LINES, display->dsi_vid->vbp);
        WRITE32_REG(MIPI_DSI, DW_DSI_VID_VFP_LINES, display->dsi_vid->vfp);
        WRITE32_REG(MIPI_DSI, DW_DSI_VID_VACTIVE_LINES,
            display->dsi_vid->vact);
    }  /* operation_mode == OPERATION_VIDEO_MODE */

    /* ----------------------------------------------------- */
    /* Finish Configuration */
    /* ----------------------------------------------------- */

    /* Inner clock divider settings */
    WRITE32_REG(MIPI_DSI, DW_DSI_CLKMGR_CFG,
        (0x1 << BIT_TO_CLK_DIV) |
        (display->dsi_phy_cfg->lp_tesc << BIT_TX_ESC_CLK_DIV));
    /* Packet header settings  //move to mipi_dcs_set */
    /* WRITE32_REG(MIPI_DSI,  DW_DSI_PCKHDL_CFG,
     *  (1 << BIT_CRC_RX_EN) |
     *  (1 << BIT_ECC_RX_EN) |
     *  (0 << BIT_BTA_EN) |
     *  (0 << BIT_EOTP_RX_EN) |
     *  (0 << BIT_EOTP_TX_EN) );
     */
    /* operation mode setting: video/command mode */
    WRITE32_REG(MIPI_DSI, DW_DSI_MODE_CFG, operation_mode);

    /* Phy Timer */
    if (display->dsi_phy_cfg->state_change == 2)
        WRITE32_REG(MIPI_DSI, DW_DSI_PHY_TMR_CFG, 0x03320000);
    else
        WRITE32_REG(MIPI_DSI, DW_DSI_PHY_TMR_CFG, 0x090f0000);

    if (display->dsi_phy_cfg->state_change == 2)
        WRITE32_REG(MIPI_DSI, DW_DSI_PHY_TMR_LPCLK_CFG, 0x870025);
    else
        WRITE32_REG(MIPI_DSI, DW_DSI_PHY_TMR_LPCLK_CFG, 0x260017);
}

static void dsi_dump_host(astro_display_t* display) {
    DISP_INFO("%s: DUMPING DSI HOST REGS\n", __func__);
    DISP_INFO("DW_DSI_VERSION = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_VERSION));
    DISP_INFO("DW_DSI_PWR_UP = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_PWR_UP));
    DISP_INFO("DW_DSI_CLKMGR_CFG = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_CLKMGR_CFG));
    DISP_INFO("DW_DSI_DPI_VCID = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_DPI_VCID));
    DISP_INFO("DW_DSI_DPI_COLOR_CODING = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_DPI_COLOR_CODING));
    DISP_INFO("DW_DSI_DPI_CFG_POL = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_DPI_CFG_POL));
    DISP_INFO("DW_DSI_DPI_LP_CMD_TIM = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_DPI_LP_CMD_TIM));
    DISP_INFO("DW_DSI_DBI_VCID = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_DBI_VCID));
    DISP_INFO("DW_DSI_DBI_CFG = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_DBI_CFG));
    DISP_INFO("DW_DSI_DBI_PARTITIONING_EN = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_DBI_PARTITIONING_EN));
    DISP_INFO("DW_DSI_DBI_CMDSIZE = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_DBI_CMDSIZE));
    DISP_INFO("DW_DSI_PCKHDL_CFG = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_PCKHDL_CFG));
    DISP_INFO("DW_DSI_GEN_VCID = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_GEN_VCID));
    DISP_INFO("DW_DSI_MODE_CFG = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_MODE_CFG));
    DISP_INFO("DW_DSI_VID_MODE_CFG = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_VID_MODE_CFG));
    DISP_INFO("DW_DSI_VID_PKT_SIZE = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_VID_PKT_SIZE));
    DISP_INFO("DW_DSI_VID_NUM_CHUNKS = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_VID_NUM_CHUNKS));
    DISP_INFO("DW_DSI_VID_NULL_SIZE = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_VID_NULL_SIZE));
    DISP_INFO("DW_DSI_VID_HSA_TIME = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_VID_HSA_TIME));
    DISP_INFO("DW_DSI_VID_HBP_TIME = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_VID_HBP_TIME));
    DISP_INFO("DW_DSI_VID_HLINE_TIME = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_VID_HLINE_TIME));
    DISP_INFO("DW_DSI_VID_VSA_LINES = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_VID_VSA_LINES));
    DISP_INFO("DW_DSI_VID_VBP_LINES = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_VID_VBP_LINES));
    DISP_INFO("DW_DSI_VID_VFP_LINES = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_VID_VFP_LINES));
    DISP_INFO("DW_DSI_VID_VACTIVE_LINES = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_VID_VACTIVE_LINES));
    DISP_INFO("DW_DSI_EDPI_CMD_SIZE = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_EDPI_CMD_SIZE));
    DISP_INFO("DW_DSI_CMD_MODE_CFG = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_CMD_MODE_CFG));
    DISP_INFO("DW_DSI_GEN_HDR = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_GEN_HDR));
    DISP_INFO("DW_DSI_GEN_PLD_DATA = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_GEN_PLD_DATA));
    DISP_INFO("DW_DSI_CMD_PKT_STATUS = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_CMD_PKT_STATUS));
    DISP_INFO("DW_DSI_TO_CNT_CFG = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_TO_CNT_CFG));
    DISP_INFO("DW_DSI_HS_RD_TO_CNT = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_HS_RD_TO_CNT));
    DISP_INFO("DW_DSI_LP_RD_TO_CNT = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_LP_RD_TO_CNT));
    DISP_INFO("DW_DSI_HS_WR_TO_CNT = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_HS_WR_TO_CNT));
    DISP_INFO("DW_DSI_LP_WR_TO_CNT = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_LP_WR_TO_CNT));
    DISP_INFO("DW_DSI_BTA_TO_CNT = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_BTA_TO_CNT));
    DISP_INFO("DW_DSI_SDF_3D = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_SDF_3D));
    DISP_INFO("DW_DSI_LPCLK_CTRL = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_LPCLK_CTRL));
    DISP_INFO("DW_DSI_PHY_TMR_LPCLK_CFG = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_PHY_TMR_LPCLK_CFG));
    DISP_INFO("DW_DSI_PHY_TMR_CFG = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_PHY_TMR_CFG));
    DISP_INFO("DW_DSI_PHY_RSTZ = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_PHY_RSTZ));
    DISP_INFO("DW_DSI_PHY_IF_CFG = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_PHY_IF_CFG));
    DISP_INFO("DW_DSI_PHY_ULPS_CTRL = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_PHY_ULPS_CTRL));
    DISP_INFO("DW_DSI_PHY_TX_TRIGGERS = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_PHY_TX_TRIGGERS));
    DISP_INFO("DW_DSI_PHY_STATUS = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_PHY_STATUS));
    DISP_INFO("DW_DSI_PHY_TST_CTRL0 = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_PHY_TST_CTRL0));
    DISP_INFO("DW_DSI_PHY_TST_CTRL1 = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_PHY_TST_CTRL1));
    DISP_INFO("DW_DSI_INT_ST0 = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_INT_ST0));
    DISP_INFO("DW_DSI_INT_ST1 = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_INT_ST1));
    DISP_INFO("DW_DSI_INT_MSK0 = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_INT_MSK0));
    DISP_INFO("DW_DSI_INT_MSK1 = 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_INT_MSK1));
}

static void dsi_phy_dump(astro_display_t* display) {
    DISP_INFO("%s: DUMPING PHY REGS\n", __func__);

    DISP_INFO("MIPI_DSI_PHY_CTRL = 0x%x\n", READ32_REG(DSI_PHY, MIPI_DSI_PHY_CTRL));
    DISP_INFO("MIPI_DSI_CHAN_CTRL = 0x%x\n", READ32_REG(DSI_PHY, MIPI_DSI_CHAN_CTRL));
    DISP_INFO("MIPI_DSI_CHAN_STS = 0x%x\n", READ32_REG(DSI_PHY, MIPI_DSI_CHAN_STS));
    DISP_INFO("MIPI_DSI_CLK_TIM = 0x%x\n", READ32_REG(DSI_PHY, MIPI_DSI_CLK_TIM));
    DISP_INFO("MIPI_DSI_HS_TIM = 0x%x\n", READ32_REG(DSI_PHY, MIPI_DSI_HS_TIM));
    DISP_INFO("MIPI_DSI_LP_TIM = 0x%x\n", READ32_REG(DSI_PHY, MIPI_DSI_LP_TIM));
    DISP_INFO("MIPI_DSI_ANA_UP_TIM = 0x%x\n", READ32_REG(DSI_PHY, MIPI_DSI_ANA_UP_TIM));
    DISP_INFO("MIPI_DSI_INIT_TIM = 0x%x\n", READ32_REG(DSI_PHY, MIPI_DSI_INIT_TIM));
    DISP_INFO("MIPI_DSI_WAKEUP_TIM = 0x%x\n", READ32_REG(DSI_PHY, MIPI_DSI_WAKEUP_TIM));
    DISP_INFO("MIPI_DSI_LPOK_TIM = 0x%x\n", READ32_REG(DSI_PHY, MIPI_DSI_LPOK_TIM));
    DISP_INFO("MIPI_DSI_LP_WCHDOG = 0x%x\n", READ32_REG(DSI_PHY, MIPI_DSI_LP_WCHDOG));
    DISP_INFO("MIPI_DSI_ANA_CTRL = 0x%x\n", READ32_REG(DSI_PHY, MIPI_DSI_ANA_CTRL));
    DISP_INFO("MIPI_DSI_CLK_TIM1 = 0x%x\n", READ32_REG(DSI_PHY, MIPI_DSI_CLK_TIM1));
    DISP_INFO("MIPI_DSI_TURN_WCHDOG = 0x%x\n", READ32_REG(DSI_PHY, MIPI_DSI_TURN_WCHDOG));
    DISP_INFO("MIPI_DSI_ULPS_CHECK = 0x%x\n", READ32_REG(DSI_PHY, MIPI_DSI_ULPS_CHECK));
    DISP_INFO("MIPI_DSI_TEST_CTRL0 = 0x%x\n", READ32_REG(DSI_PHY, MIPI_DSI_TEST_CTRL0));
    DISP_INFO("MIPI_DSI_TEST_CTRL1 = 0x%x\n", READ32_REG(DSI_PHY, MIPI_DSI_TEST_CTRL1));

    DISP_INFO("\n");

}

static void dsi_phy_init(astro_display_t* display, uint32_t lane_num)
{
    /* enable phy clock. */
    WRITE32_REG(DSI_PHY, MIPI_DSI_PHY_CTRL,  0x1); /* enable DSI top clock. */
    WRITE32_REG(DSI_PHY, MIPI_DSI_PHY_CTRL,
        (1 << 0)  | /* enable the DSI PLL clock . */
        (1 << 7)  |
            /* enable pll clock which connected to
             * DDR clock path
             */
        (1 << 8)  | /* enable the clock divider counter */
        (0 << 9)  | /* enable the divider clock out */
        (0 << 10) | /* clock divider. 1: freq/4, 0: freq/2 */
        (0 << 11) |
            /* 1: select the mipi DDRCLKHS from clock divider,
             * 0: from PLL clock
             */
        (0 << 12)); /* enable the byte clock generateion. */
    /* enable the divider clock out */
    SET_BIT32(DSI_PHY, MIPI_DSI_PHY_CTRL,  1, 9, 1);
    /* enable the byte clock generateion. */
    SET_BIT32(DSI_PHY, MIPI_DSI_PHY_CTRL,  1, 12, 1);
    SET_BIT32(DSI_PHY, MIPI_DSI_PHY_CTRL,  1, 31, 1);
    SET_BIT32(DSI_PHY, MIPI_DSI_PHY_CTRL,  0, 31, 1);

    /* 0x05210f08);//0x03211c08 */
    WRITE32_REG(DSI_PHY, MIPI_DSI_CLK_TIM,
        (display->dsi_phy_cfg->clk_trail | (display->dsi_phy_cfg->clk_post << 8) |
        (display->dsi_phy_cfg->clk_zero << 16) | (display->dsi_phy_cfg->clk_prepare << 24)));
    WRITE32_REG(DSI_PHY, MIPI_DSI_CLK_TIM1, display->dsi_phy_cfg->clk_pre); /* ?? */
    /* 0x050f090d */
    WRITE32_REG(DSI_PHY, MIPI_DSI_HS_TIM,
        (display->dsi_phy_cfg->hs_exit | (display->dsi_phy_cfg->hs_trail << 8) |
        (display->dsi_phy_cfg->hs_zero << 16) | (display->dsi_phy_cfg->hs_prepare << 24)));
    /* 0x4a370e0e */
    WRITE32_REG(DSI_PHY, MIPI_DSI_LP_TIM,
        (display->dsi_phy_cfg->lp_lpx | (display->dsi_phy_cfg->lp_ta_sure << 8) |
        (display->dsi_phy_cfg->lp_ta_go << 16) | (display->dsi_phy_cfg->lp_ta_get << 24)));
    /* ?? //some number to reduce sim time. */
    WRITE32_REG(DSI_PHY, MIPI_DSI_ANA_UP_TIM, 0x0100);
    /* 0xe20   //30d4 -> d4 to reduce sim time. */
    WRITE32_REG(DSI_PHY, MIPI_DSI_INIT_TIM, display->dsi_phy_cfg->init);
    /* 0x8d40  //1E848-> 48 to reduct sim time. */
    WRITE32_REG(DSI_PHY, MIPI_DSI_WAKEUP_TIM, display->dsi_phy_cfg->wakeup);
    /* wait for the LP analog ready. */
    WRITE32_REG(DSI_PHY, MIPI_DSI_LPOK_TIM,  0x7C);
    /* 1/3 of the tWAKEUP. */
    WRITE32_REG(DSI_PHY, MIPI_DSI_ULPS_CHECK,  0x927C);
    /* phy TURN watch dog. */
    WRITE32_REG(DSI_PHY, MIPI_DSI_LP_WCHDOG,  0x1000);
    /* phy ESC command watch dog. */
    WRITE32_REG(DSI_PHY, MIPI_DSI_TURN_WCHDOG,  0x1000);

    /* Powerup the analog circuit. */
    switch (lane_num) {
    case 1:
        WRITE32_REG(DSI_PHY, MIPI_DSI_CHAN_CTRL, 0x0e);
        break;
    case 2:
        WRITE32_REG(DSI_PHY, MIPI_DSI_CHAN_CTRL, 0x0c);
        break;
    case 3:
        WRITE32_REG(DSI_PHY, MIPI_DSI_CHAN_CTRL, 0x08);
        break;
    case 4:
    default:
        WRITE32_REG(DSI_PHY, MIPI_DSI_CHAN_CTRL, 0);
        break;
    }
}

#define DPHY_TIMEOUT    200000
static void check_phy_status(astro_display_t* display)
{
    int i = 0;

    while (GET_BIT32(MIPI_DSI, DW_DSI_PHY_STATUS,
        BIT_PHY_LOCK, 1) == 0) {
        if (i++ >= DPHY_TIMEOUT) {
            DISP_ERROR("phy_lock timeout 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_PHY_STATUS));
            break;
        }
        usleep(6);
    }

    i = 0;
    usleep(10);
    while (GET_BIT32(MIPI_DSI, DW_DSI_PHY_STATUS,
        BIT_PHY_STOPSTATECLKLANE, 1) == 0) {
        if (i == 0) {
            DISP_INFO(" Waiting STOP STATE LANE\n");
        }
        if (i++ >= DPHY_TIMEOUT) {
            DISP_ERROR("lane_state timeout 0x%x\n", READ32_REG(MIPI_DSI, DW_DSI_PHY_STATUS));
            break;
        }
        usleep(6);
    }
}

static void set_dsi_phy_config(astro_display_t* display)
{
    /* Digital */
    /* Power up DSI */
    WRITE32_REG(MIPI_DSI, DW_DSI_PWR_UP, 1);

    /* Setup Parameters of DPHY */
    WRITE32_REG(MIPI_DSI, DW_DSI_PHY_TST_CTRL1, 0x00010044);/*testcode*/
    WRITE32_REG(MIPI_DSI, DW_DSI_PHY_TST_CTRL0, 0x2);
    WRITE32_REG(MIPI_DSI, DW_DSI_PHY_TST_CTRL0, 0x0);
    WRITE32_REG(MIPI_DSI, DW_DSI_PHY_TST_CTRL1, 0x00000074);/*testwrite*/
    WRITE32_REG(MIPI_DSI, DW_DSI_PHY_TST_CTRL0, 0x2);
    WRITE32_REG(MIPI_DSI, DW_DSI_PHY_TST_CTRL0, 0x0);

    /* Power up D-PHY */
    WRITE32_REG(MIPI_DSI, DW_DSI_PHY_RSTZ, 0xf);

    /* Analog */
    dsi_phy_init(display, display->dsi_cfg->lane_num);

    /* Check the phylock/stopstateclklane to decide if the DPHY is ready */
    check_phy_status(display);

    /* Trigger a sync active for esc_clk */
    SET_BIT32(DSI_PHY, MIPI_DSI_PHY_CTRL, 1, 1, 1);

    /* Startup transfer, default lpclk */
    WRITE32_REG(MIPI_DSI, DW_DSI_LPCLK_CTRL,
            (0x1 << BIT_AUTOCLKLANE_CTRL) |
            (0x1 << BIT_TXREQUESTCLKHS));
}



/* MIPI DSI Specific functions */
static inline void print_mipi_cmd_status(astro_display_t* display, int cnt, uint32_t status)
{
    if (cnt == 0)
{        DISP_ERROR("cmd error: status=0x%04x, int0=0x%06x, int1=0x%06x\n",
            status,
            READ32_REG(MIPI_DSI, DW_DSI_INT_ST0),
            READ32_REG(MIPI_DSI, DW_DSI_INT_ST1));
    }
}

static void dsi_bta_control(astro_display_t* display, int en)
{
    if (en) {
        SET_BIT32(MIPI_DSI, DW_DSI_CMD_MODE_CFG,
            MIPI_DSI_DCS_REQ_ACK, BIT_ACK_RQST_EN, 1);
        SET_BIT32(MIPI_DSI, DW_DSI_PCKHDL_CFG,
            MIPI_DSI_DCS_REQ_ACK, BIT_BTA_EN, 1);
    } else {
        SET_BIT32(MIPI_DSI, DW_DSI_PCKHDL_CFG,
            MIPI_DSI_DCS_NO_ACK, BIT_BTA_EN, 1);
        SET_BIT32(MIPI_DSI, DW_DSI_CMD_MODE_CFG,
            MIPI_DSI_DCS_NO_ACK, BIT_ACK_RQST_EN, 1);
    }
}


static int wait_bta_ack(astro_display_t* display)
{
    uint32_t phy_status;
    int i;

    /* Check if phydirection is RX */
    i = CMD_TIMEOUT_CNT;
    do {
        usleep(10);
        i--;
        phy_status = READ32_REG(MIPI_DSI, DW_DSI_PHY_STATUS);
    } while ((((phy_status & 0x2) >> BIT_PHY_DIRECTION) == 0x0) && (i > 0));
    if (i == 0) {
        DISP_ERROR("phy direction error: RX\n");
        return -1;
    }

    /* Check if phydirection is return to TX */
    i = CMD_TIMEOUT_CNT;
    do {
        usleep(10);
        i--;
        phy_status = READ32_REG(MIPI_DSI, DW_DSI_PHY_STATUS);
    } while ((((phy_status & 0x2) >> BIT_PHY_DIRECTION) == 0x1) && (i > 0));
    if (i == 0) {
        DISP_ERROR("phy direction error: TX\n");
        return -1;
    }

    return 0;
}

static void wait_cmd_fifo_empty(astro_display_t* display)
{
    uint32_t cmd_status;
    int i = CMD_TIMEOUT_CNT;

    do {
        usleep(10);
        i--;
        cmd_status = READ32_REG(MIPI_DSI, DW_DSI_CMD_PKT_STATUS);
    } while ((((cmd_status >> BIT_GEN_CMD_EMPTY) & 0x1) != 0x1) && (i > 0));
    print_mipi_cmd_status(display, i, cmd_status);
}
static unsigned short dsi_rx_n;

static void dsi_set_max_return_pkt_size(astro_display_t* display, struct dsi_cmd_request_s *req)
{
    uint32_t d_para[2];

    d_para[0] = (uint32_t)(req->payload[2] & 0xff);
    d_para[1] = (uint32_t)(req->payload[3] & 0xff);
    dsi_rx_n = (unsigned short)((d_para[1] << 8) | d_para[0]);
    generic_if_wr(display, DW_DSI_GEN_HDR,
        ((d_para[1] << BIT_GEN_WC_MSBYTE)          |
        (d_para[0] << BIT_GEN_WC_LSBYTE)           |
        (((uint32_t)req->vc_id) << BIT_GEN_VC) |
        (DT_SET_MAX_RET_PKT_SIZE << BIT_GEN_DT)));
    if (req->req_ack == MIPI_DSI_DCS_REQ_ACK) {
        wait_bta_ack(display);
    } else if (req->req_ack == MIPI_DSI_DCS_NO_ACK) {
        wait_cmd_fifo_empty(display);
    }
}


static int dsi_generic_read_packet(astro_display_t* display, struct dsi_cmd_request_s *req,
        unsigned char *r_data)
{
    unsigned int d_para[2], read_data;
    unsigned int i, j, done;
    int ret = 0;

    switch (req->data_type) {
    case DT_GEN_RD_1:
        d_para[0] = (req->pld_count == 0) ?
            0 : (((unsigned int)req->payload[2]) & 0xff);
        d_para[1] = 0;
        break;
    case DT_GEN_RD_2:
        d_para[0] = (req->pld_count == 0) ?
            0 : (((unsigned int)req->payload[2]) & 0xff);
        d_para[1] = (req->pld_count < 2) ?
            0 : (((unsigned int)req->payload[3]) & 0xff);
        break;
    case DT_GEN_RD_0:
    default:
        d_para[0] = 0;
        d_para[1] = 0;
        break;
    }

    if (MIPI_DSI_DCS_ACK_TYPE == MIPI_DSI_DCS_NO_ACK)
        dsi_bta_control(display, 1);
    generic_if_wr(display, DW_DSI_GEN_HDR,
        ((d_para[1] << BIT_GEN_WC_MSBYTE)          |
        (d_para[0] << BIT_GEN_WC_LSBYTE)           |
        (((unsigned int)req->vc_id) << BIT_GEN_VC) |
        (((unsigned int)req->data_type) << BIT_GEN_DT)));
    ret = wait_bta_ack(display);
    if (ret)
        return -1;

    i = 0;
    done = 0;
    while (done == 0) {
        read_data = generic_if_rd(display, DW_DSI_GEN_PLD_DATA);
        for (j = 0; j < 4; j++) {
            if (i < dsi_rx_n) {
                r_data[i] = (unsigned char)
                    ((read_data >> (j*8)) & 0xff);
                i++;
            } else {
                done = 1;
                break;
            }
        }
    }
    if (MIPI_DSI_DCS_ACK_TYPE == MIPI_DSI_DCS_NO_ACK)
        dsi_bta_control(display, 0);

    return dsi_rx_n;
}

static int dsi_dcs_read_packet(astro_display_t* display, struct dsi_cmd_request_s *req,
        unsigned char *r_data)
{
    unsigned int d_command, read_data;
    unsigned int i, j, done;
    int ret = 0;

    d_command = ((unsigned int)req->payload[2]) & 0xff;

    if (MIPI_DSI_DCS_ACK_TYPE == MIPI_DSI_DCS_NO_ACK)
        dsi_bta_control(display, 1);
    generic_if_wr(display, DW_DSI_GEN_HDR,
        ((0 << BIT_GEN_WC_MSBYTE)                  |
        (d_command << BIT_GEN_WC_LSBYTE)           |
        (((unsigned int)req->vc_id) << BIT_GEN_VC) |
        (((unsigned int)req->data_type) << BIT_GEN_DT)));
    ret = wait_bta_ack(display);
    if (ret)
        return -1;

    i = 0;
    done = 0;
    while (done == 0) {
        read_data = generic_if_rd(display, DW_DSI_GEN_PLD_DATA);
        for (j = 0; j < 4; j++) {
            if (i < dsi_rx_n) {
                r_data[i] = (unsigned char)
                    ((read_data >> (j*8)) & 0xff);
                i++;
            } else {
                done = 1;
                break;
            }
        }
    }

    if (MIPI_DSI_DCS_ACK_TYPE == MIPI_DSI_DCS_NO_ACK)
        dsi_bta_control(display, 0);

    return dsi_rx_n;
}


/* *************************************************************
 * Function: dcs_write_short_packet
 * DCS Write Short Packet with Generic Interface
 * Supported Data Type: DT_DCS_SHORT_WR_0, DT_DCS_SHORT_WR_1,
 */
static void dsi_dcs_write_short_packet(astro_display_t* display, struct dsi_cmd_request_s *req)
{
    unsigned int d_command, d_para;

    d_command = ((unsigned int)req->payload[2]) & 0xff;
    d_para = (req->pld_count < 2) ?
        0 : (((unsigned int)req->payload[3]) & 0xff);

    generic_if_wr(display, DW_DSI_GEN_HDR,
        ((d_para << BIT_GEN_WC_MSBYTE)             |
        (d_command << BIT_GEN_WC_LSBYTE)           |
        (((unsigned int)req->vc_id) << BIT_GEN_VC) |
        (((unsigned int)req->data_type) << BIT_GEN_DT)));
    if (req->req_ack == MIPI_DSI_DCS_REQ_ACK)
        wait_bta_ack(display);
    else if (req->req_ack == MIPI_DSI_DCS_NO_ACK)
        wait_cmd_fifo_empty(display);
}

/* *************************************************************
 * Function: dsi_write_long_packet
 * Write Long Packet with Generic Interface
 * Supported Data Type: DT_GEN_LONG_WR, DT_DCS_LONG_WR
 */
static void dsi_write_long_packet(astro_display_t* display, struct dsi_cmd_request_s *req)
{
    unsigned int d_command, payload_data, header_data;
    unsigned int cmd_status;
    unsigned int i, j, data_index, n, temp;

    /* payload[2] start (payload[0]: data_type, payload[1]: data_cnt) */
    data_index = DSI_CMD_SIZE_INDEX + 1;
    d_command = ((unsigned int)req->payload[data_index]) & 0xff;

    /* Write Payload Register First */
    n = (req->pld_count+3)/4;
    for (i = 0; i < n; i++) {
        payload_data = 0;
        if (i < (req->pld_count/4))
            temp = 4;
        else
            temp = req->pld_count % 4;
        for (j = 0; j < temp; j++) {
            payload_data |= (((unsigned int)
                req->payload[data_index+(i*4)+j]) << (j*8));
        }

        /* Check the pld fifo status before write to it,
         * do not need check every word
         */
        if ((i == (n/3)) || (i == (n/2))) {
            j = CMD_TIMEOUT_CNT;
            do {
                usleep(10);
                j--;
                cmd_status = READ32_REG(MIPI_DSI,
                        DW_DSI_CMD_PKT_STATUS);
            } while ((((cmd_status >> BIT_GEN_PLD_W_FULL) & 0x1) ==
                0x1) && (j > 0));
            print_mipi_cmd_status(display, j, cmd_status);
        }
        /* Use direct memory write to save time when in
         * WRITE_MEMORY_CONTINUE
         */
        if (d_command == DCS_WRITE_MEMORY_CONTINUE) {
            WRITE32_REG(MIPI_DSI, DW_DSI_GEN_PLD_DATA,
                payload_data);
        } else {
            generic_if_wr(display, DW_DSI_GEN_PLD_DATA,
                payload_data);
        }
    }

    /* Check cmd fifo status before write to it */
    j = CMD_TIMEOUT_CNT;
    do {
        usleep(10);
        j--;
        cmd_status = READ32_REG(MIPI_DSI, DW_DSI_CMD_PKT_STATUS);
    } while ((((cmd_status >> BIT_GEN_CMD_FULL) & 0x1) == 0x1) && (j > 0));
    print_mipi_cmd_status(display, j, cmd_status);
    /* Write Header Register */
    /* include command */
    header_data = ((((unsigned int)req->pld_count) << BIT_GEN_WC_LSBYTE) |
            (((unsigned int)req->vc_id) << BIT_GEN_VC)           |
            (((unsigned int)req->data_type) << BIT_GEN_DT));
    generic_if_wr(display, DW_DSI_GEN_HDR, header_data);
    if (req->req_ack == MIPI_DSI_DCS_REQ_ACK)
        wait_bta_ack(display);
    else if (req->req_ack == MIPI_DSI_DCS_NO_ACK)
        wait_cmd_fifo_empty(display);
}

int dsi_read_single(astro_display_t* display, unsigned char *payload, unsigned char *rd_data,
        unsigned int rd_byte_len)
{
    int num = 0;
    unsigned char temp[4];
    unsigned char vc_id = MIPI_DSI_VIRTUAL_CHAN_ID;
    unsigned int req_ack;
    struct dsi_cmd_request_s dsi_cmd_req;

    req_ack = MIPI_DSI_DCS_ACK_TYPE;
    dsi_cmd_req.data_type = DT_SET_MAX_RET_PKT_SIZE;
    dsi_cmd_req.vc_id = (vc_id & 0x3);
    temp[0] = dsi_cmd_req.data_type;
    temp[1] = 2;
    temp[2] = (unsigned char)((rd_byte_len >> 0) & 0xff);
    temp[3] = (unsigned char)((rd_byte_len >> 8) & 0xff);
    dsi_cmd_req.payload = &temp[0];
    dsi_cmd_req.pld_count = 2;
    dsi_cmd_req.req_ack = req_ack;
    dsi_set_max_return_pkt_size(display, &dsi_cmd_req);

    /* payload struct: */
    /* data_type, data_cnt, command, parameters... */
    req_ack = MIPI_DSI_DCS_REQ_ACK; /* need BTA ack */
    dsi_cmd_req.data_type = payload[0];
    dsi_cmd_req.vc_id = (vc_id & 0x3);
    dsi_cmd_req.payload = &payload[0];
    dsi_cmd_req.pld_count = payload[DSI_CMD_SIZE_INDEX];
    dsi_cmd_req.req_ack = req_ack;
    switch (dsi_cmd_req.data_type) {/* analysis data_type */
    case DT_GEN_RD_0:
    case DT_GEN_RD_1:
    case DT_GEN_RD_2:
        num = dsi_generic_read_packet(display, &dsi_cmd_req, rd_data);
        break;
    case DT_DCS_RD_0:
        num = dsi_dcs_read_packet(display, &dsi_cmd_req, rd_data);
        break;
    default:
        DISP_ERROR("read un-support data_type: 0x%02x\n",
            dsi_cmd_req.data_type);
        break;
    }

    if (num < 0) {
        DISP_ERROR("mipi-dsi read error\n");
    }

    return num;
}

static void mipi_dsi_check_state(astro_display_t* display, unsigned char reg, unsigned char cnt)
{
    int ret = 0, i;
    unsigned char *rd_data;
    unsigned char payload[3] = {DT_GEN_RD_1, 1, 0x04};

    if (display->dsi_cfg->check_en == 0) {
        return;
    }

    display->dsi_cfg->check_state = 0;
    rd_data = (unsigned char *)malloc(sizeof(unsigned char) * cnt);
    if (rd_data == NULL) {
        DISP_ERROR("%s: rd_data malloc error\n", __func__);
        return;
    }

    payload[2] = reg;
    ret = dsi_read_single(display, payload, rd_data, cnt);
    if (ret < 0) {
        display->dsi_cfg->check_state = 0;
        SET_BIT32(VPU, L_VCOM_VS_ADDR, 0, 12, 1);
        free(rd_data);
        return;
    }
    if (ret > cnt) {
        DISP_ERROR("%s: read back cnt is wrong\n", __func__);
        free(rd_data);
        return;
    }

    display->dsi_cfg->check_state = 1;
    SET_BIT32(VPU, L_VCOM_VS_ADDR, 1, 12, 1);
    DISP_INFO("read reg 0x%02x: ", reg);
    for (i = 0; i < ret; i++) {
        if (i == 0)
            DISP_INFO("0x%02x", rd_data[i]);
        else
            DISP_INFO(",0x%02x", rd_data[i]);
    }
    DISP_INFO("\n");

    free(rd_data);
}

#if 0
int dsi_write_cmd(unsigned char *payload)
{
    int i = 0, j = 0, num = 0;
    int k = 0, n = 0;
    unsigned char rd_data[100];
    struct dsi_cmd_request_s dsi_cmd_req;
    unsigned char vc_id = MIPI_DSI_VIRTUAL_CHAN_ID;
    uint32_t req_ack = MIPI_DSI_DCS_ACK_TYPE;
    struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
    struct lcd_power_ctrl_s *lcd_power;
    char *str;
    int gpio;

    /* mipi command(payload) */
    /* format:  data_type, cmd_size, data.... */
    /* special: data_type=0xff,
     *      cmd_size<0xff means delay ms,
     *      cmd_size=0xff means ending.
     *      data_type=0xf0,
     *      data0=gpio_index, data1=gpio_value, data2=delay.
     */
    while (i < DSI_CMD_SIZE_MAX) {
        if (payload[i] == 0xff) {
            j = 2;
            if (payload[i+1] == 0xff)
                break;
            else
                mdelay(payload[i+1]);
        } else if (payload[i] == 0xf0) { /* gpio */
            j = (DSI_CMD_SIZE_INDEX + 1) +
                payload[i+DSI_CMD_SIZE_INDEX];
            if (payload[i+DSI_CMD_SIZE_INDEX] < 3) {
                DISP_ERROR("wrong cmd_size %d for gpio\n",
                    payload[i+DSI_CMD_SIZE_INDEX]);
                break;
            }
            lcd_power = lcd_drv->lcd_config->lcd_power;
            str = lcd_power->cpu_gpio[payload[i+DSI_GPIO_INDEX]];
            gpio = aml_lcd_gpio_name_map_num(str);
            aml_lcd_gpio_set(gpio, payload[i+DSI_GPIO_INDEX+1]);
            if (payload[i+DSI_GPIO_INDEX+2])
                mdelay(payload[i+DSI_GPIO_INDEX+2]);
        } else if (payload[i] == 0xfc) { /* check state */
            j = (DSI_CMD_SIZE_INDEX + 1) +
                payload[i+DSI_CMD_SIZE_INDEX];
            if (payload[i+DSI_CMD_SIZE_INDEX] < 2) {
                DISP_ERROR("wrong cmd_size %d for check state\n",
                    payload[i+DSI_CMD_SIZE_INDEX]);
                break;
            }
            if (payload[i+DSI_GPIO_INDEX+2] > 0) {
                mipi_dsi_check_state(
                    payload[i+DSI_GPIO_INDEX],
                    payload[i+DSI_GPIO_INDEX+1]);
            }
        } else if ((payload[i] & 0xf) == 0x0) {
                DISP_ERROR("data_type: 0x%02x\n", payload[i]);
                break;
        } else {
            /* payload[i+DSI_CMD_SIZE_INDEX] is data count */
            j = (DSI_CMD_SIZE_INDEX + 1) +
                payload[i+DSI_CMD_SIZE_INDEX];
            dsi_cmd_req.data_type = payload[i];
            dsi_cmd_req.vc_id = (vc_id & 0x3);
            dsi_cmd_req.payload = &payload[i];
            dsi_cmd_req.pld_count = payload[i+DSI_CMD_SIZE_INDEX];
            dsi_cmd_req.req_ack = req_ack;
            switch (dsi_cmd_req.data_type) {/* analysis data_type */
            case DT_GEN_SHORT_WR_0:
            case DT_GEN_SHORT_WR_1:
            case DT_GEN_SHORT_WR_2:
                dsi_generic_write_short_packet(&dsi_cmd_req);
                break;
            case DT_DCS_SHORT_WR_0:
            case DT_DCS_SHORT_WR_1:
                dsi_dcs_write_short_packet(&dsi_cmd_req);
                break;
            case DT_DCS_LONG_WR:
            case DT_GEN_LONG_WR:
                dsi_write_long_packet(&dsi_cmd_req);
                break;
            case DT_TURN_ON:
                SET_BIT32(MIPI_DSI, MIPI_DSI_TOP_CNTL, 1, 2, 1);
                mdelay(20); /* wait for vsync trigger */
                SET_BIT32(MIPI_DSI, MIPI_DSI_TOP_CNTL, 0, 2, 1);
                mdelay(20); /* wait for vsync trigger */
                break;
            case DT_SHUT_DOWN:
                SET_BIT32(MIPI_DSI, MIPI_DSI_TOP_CNTL, 1, 2, 1);
                mdelay(20); /* wait for vsync trigger */
                break;
            case DT_SET_MAX_RET_PKT_SIZE:
                dsi_set_max_return_pkt_size(&dsi_cmd_req);
                break;
#ifdef DSI_CMD_READ_VALID
            case DT_GEN_RD_0:
            case DT_GEN_RD_1:
            case DT_GEN_RD_2:
                /* need BTA ack */
                dsi_cmd_req.req_ack = MIPI_DSI_DCS_REQ_ACK;
                dsi_cmd_req.pld_count =
                    (dsi_cmd_req.pld_count > 2) ?
                    2 : dsi_cmd_req.pld_count;
                n = dsi_generic_read_packet(&dsi_cmd_req,
                        &rd_data[0]);
                DISP_ERROR("generic read data");
                for (k = 0; k < dsi_cmd_req.pld_count; k++) {
                    DISP_INFO(" 0x%02x",
                        dsi_cmd_req.payload[k+2]);
                }
                for (k = 0; k < n; k++)
                    DISP_INFO("0x%02x ", rd_data[k]);
                DISP_INFO("\n");
                break;
            case DT_DCS_RD_0:
                /* need BTA ack */
                dsi_cmd_req.req_ack = MIPI_DSI_DCS_REQ_ACK;
                n = dsi_dcs_read_packet(&dsi_cmd_req,
                    &rd_data[0]);
                DISP_INFO("dcs read data 0x%02x:\n",
                    dsi_cmd_req.payload[2]);
                for (k = 0; k < n; k++)
                    DISP_INFO("0x%02x ", rd_data[k]);
                DISP_INFO("\n");
                break;
#endif
            default:
                DISP_ERROR("[warning]un-support data_type: 0x%02x\n",
                    dsi_cmd_req.data_type);

                break;
            }
        }
        i += j;
        num++;
    }

    return num;
}
#endif

static void set_mipi_dsi_lpclk_ctrl(astro_display_t* display, uint32_t operation_mode) {
    uint32_t lpclk = 1;

    if (operation_mode == OPERATION_VIDEO_MODE) {
        if (display->dsi_cfg->clk_always_hs) { /* clk always hs */
            lpclk = 0;
        } else { /* enable clk lp state */
            lpclk = 1;
        }
    } else { /* enable clk lp state */
        lpclk = 1;
    }
    SET_BIT32(MIPI_DSI, DW_DSI_LPCLK_CTRL, lpclk, BIT_AUTOCLKLANE_CTRL, 1);
}

static void mipi_dsi_link_on(astro_display_t* display) {

    set_mipi_dsi_lpclk_ctrl(display, display->dsi_cfg->opp_mode_init);

    // dsi_write_cmd(dconf->dsi_init_on);

    // dsi_write_cmd(lcd_ext->config->table_init_on);

    set_mipi_dsi_host(display, MIPI_DSI_VIRTUAL_CHAN_ID,
        0, /* Chroma sub sample, only for
            * YUV 422 or 420, even or odd
            */
        display->dsi_cfg->opp_mode_display);
    set_mipi_dsi_lpclk_ctrl(display, display->dsi_cfg->opp_mode_display);
}

static void startup_mipi_dsi_host(astro_display_t* display)
{

    /* Enable dwc mipi_dsi_host's clock */
    SET_BIT32(MIPI_DSI, MIPI_DSI_TOP_CNTL, 0x3, 4, 2);
    /* mipi_dsi_host's reset */
    SET_BIT32(MIPI_DSI, MIPI_DSI_TOP_SW_RESET, 0xf, 0, 4);
    /* Release mipi_dsi_host's reset */
    SET_BIT32(MIPI_DSI, MIPI_DSI_TOP_SW_RESET, 0x0, 0, 4);
    /* Enable dwc mipi_dsi_host's clock */
    SET_BIT32(MIPI_DSI, MIPI_DSI_TOP_CLK_CNTL, 0x3, 0, 2);

    WRITE32_REG(MIPI_DSI, MIPI_DSI_TOP_MEM_PD, 0);

    usleep(10000);
}


zx_status_t aml_dsi_host_on(astro_display_t* display) {

    ZX_DEBUG_ASSERT(display);

    display->dsi_vid = calloc(1, sizeof(dsi_video_t));
    if (!display->dsi_vid) {
        DISP_ERROR("Could not allocate dsi_video_t\n");
        return ZX_ERR_NO_MEMORY;
    }

    mipi_dsi_config_post(display);

    startup_mipi_dsi_host(display);

    mipi_dcs_set(display, MIPI_DSI_CMD_TRANS_TYPE, /* 0: high speed, 1: low power */
        MIPI_DSI_DCS_ACK_TYPE,        /* if need bta ack check */
        MIPI_DSI_TEAR_SWITCH);        /* enable tear ack */

    set_mipi_dsi_host(display, MIPI_DSI_VIRTUAL_CHAN_ID,   /* Virtual channel id */
        0, /* Chroma sub sample, only for YUV 422 or 420, even or odd */
        display->dsi_cfg->opp_mode_init); /* DSI operation mode, video or command */
    set_dsi_phy_config(display);

    mipi_dsi_link_on(display);


    return ZX_OK;
}