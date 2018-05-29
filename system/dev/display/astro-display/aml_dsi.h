// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

/* TOP MIPI_DSI AML Registers */
#define MIPI_DSI_TOP_SW_RESET                               (0xF0 << 2)
#define MIPI_DSI_TOP_CLK_CNTL                               (0xF1 << 2)
#define MIPI_DSI_TOP_CNTL                                   (0xF2 << 2)
#define MIPI_DSI_TOP_SUSPEND_CNTL                           (0xF3 << 2)
#define MIPI_DSI_TOP_SUSPEND_LINE                           (0xF4 << 2)
#define MIPI_DSI_TOP_SUSPEND_PIX                            (0xF5 << 2)
#define MIPI_DSI_TOP_MEAS_CNTL                              (0xF6 << 2)
#define MIPI_DSI_TOP_STAT                                   (0xF7 << 2)
#define MIPI_DSI_TOP_MEAS_STAT_TE0                          (0xF8 << 2)
#define MIPI_DSI_TOP_MEAS_STAT_TE1                          (0xF9 << 2)
#define MIPI_DSI_TOP_MEAS_STAT_VS0                          (0xFA << 2)
#define MIPI_DSI_TOP_MEAS_STAT_VS1                          (0xFB << 2)
#define MIPI_DSI_TOP_INTR_CNTL_STAT                         (0xFC << 2)
#define MIPI_DSI_TOP_MEM_PD                                 (0xFD << 2)

#define MIPI_DSI_PHY_CTRL                                   (0x000 << 2)
#define MIPI_DSI_CHAN_CTRL                                  (0x001 << 2)
#define MIPI_DSI_CHAN_STS                                   (0x002 << 2)
#define MIPI_DSI_CLK_TIM                                    (0x003 << 2)
#define MIPI_DSI_HS_TIM                                     (0x004 << 2)
#define MIPI_DSI_LP_TIM                                     (0x005 << 2)
#define MIPI_DSI_ANA_UP_TIM                                 (0x006 << 2)
#define MIPI_DSI_INIT_TIM                                   (0x007 << 2)
#define MIPI_DSI_WAKEUP_TIM                                 (0x008 << 2)
#define MIPI_DSI_LPOK_TIM                                   (0x009 << 2)
#define MIPI_DSI_LP_WCHDOG                                  (0x00a << 2)
#define MIPI_DSI_ANA_CTRL                                   (0x00b << 2)
#define MIPI_DSI_CLK_TIM1                                   (0x00c << 2)
#define MIPI_DSI_TURN_WCHDOG                                (0x00d << 2)
#define MIPI_DSI_ULPS_CHECK                                 (0x00e << 2)
#define MIPI_DSI_TEST_CTRL0                                 (0x00f << 2)
#define MIPI_DSI_TEST_CTRL1                                 (0x010 << 2)
/* *************************************************************
 * Define MIPI DSI Default config
 */
/* Range [0,3] */
#define MIPI_DSI_VIRTUAL_CHAN_ID        0
/* Define DSI command transfer type: high speed or low power */
#define MIPI_DSI_CMD_TRANS_TYPE         DCS_TRANS_LP
/* Define if DSI command need ack: req_ack or no_ack */
#define MIPI_DSI_DCS_ACK_TYPE           MIPI_DSI_DCS_NO_ACK

#define MIPI_DSI_TEAR_SWITCH            MIPI_DCS_DISABLE_TEAR

/* PLL_CNTL bit: GP0 */
#define LCD_PLL_LOCK_GP0_G12A           (31)
#define LCD_PLL_EN_GP0_G12A             (28)
#define LCD_PLL_RST_GP0_G12A            (29)
#define LCD_PLL_OD_GP0_G12A             (16)
#define LCD_PLL_N_GP0_G12A              (10)
#define LCD_PLL_M_GP0_G12A              (0)

/* ******** frequency limit (unit: kHz) ******** */
#define PLL_FRAC_OD_FB_GP0_G12A         0
#define SS_LEVEL_MAX_GP0_G12A           5
#define PLL_FRAC_RANGE_GP0_G12A         (1 << 17)
#define PLL_OD_SEL_MAX_GP0_G12A         5
#define PLL_VCO_MIN_GP0_G12A            (3000 * 1000)
#define PLL_VCO_MAX_GP0_G12A            (6000 * 1000)

/* PLL_CNTL bit: hpll */
#define LCD_PLL_LOCK_HPLL_G12A          (31)
#define LCD_PLL_EN_HPLL_G12A            (28)
#define LCD_PLL_RST_HPLL_G12A           (29)
#define LCD_PLL_N_HPLL_G12A             (10)
#define LCD_PLL_M_HPLL_G12A             (0)

#define LCD_PLL_OD3_HPLL_G12A           (20)
#define LCD_PLL_OD2_HPLL_G12A           (18)
#define LCD_PLL_OD1_HPLL_G12A           (16)

/* ******** frequency limit (unit: kHz) ******** */
#define PLL_FRAC_OD_FB_HPLL_G12A        0
#define SS_LEVEL_MAX_HPLL_G12A          1
#define PLL_FRAC_RANGE_HPLL_G12A        (1 << 17)
#define PLL_OD_SEL_MAX_HPLL_G12A        3
#define PLL_VCO_MIN_HPLL_G12A           (3000 * 1000)
#define PLL_VCO_MAX_HPLL_G12A           (6000 * 1000)

/* video */
#define PLL_M_MIN_G12A                  2
#define PLL_M_MAX_G12A                  511
#define PLL_N_MIN_G12A                  1
#define PLL_N_MAX_G12A                  1
#define PLL_FREF_MIN_G12A               (5 * 1000)
#define PLL_FREF_MAX_G12A               (25 * 1000)
#define CRT_VID_CLK_IN_MAX_G12A         (6000 * 1000)
#define ENCL_CLK_IN_MAX_G12A            (200 * 1000)

#define MIPI_DSI_COLOR_24BIT            (0x5)

#define MIPI_PHY_CLK_MAX                (1000)
#define MAX_ERROR                       (2 * 1000)
#define CRT_VID_DIV_MAX                 (255)

/* **********************************
 * clk parameter bit define
 * pll_ctrl, div_ctrl, clk_ctrl
 * ********************************** */
/* ******** pll_ctrl ******** */
#define PLL_CTRL_OD3                20 /* [21:20] */
#define PLL_CTRL_OD2                18 /* [19:18] */
#define PLL_CTRL_OD1                16 /* [17:16] */
#define PLL_CTRL_N                  9  /* [13:9] */
#define PLL_CTRL_M                  0  /* [8:0] */

/* ******** div_ctrl ******** */
#define DIV_CTRL_EDP_DIV1           24 /* [26:24] */
#define DIV_CTRL_EDP_DIV0           20 /* [23:20] */
#define DIV_CTRL_DIV_SEL            8 /* [15:8] */
#define DIV_CTRL_XD                 0 /* [7:0] */

/* ******** clk_ctrl ******** */
#define CLK_CTRL_LEVEL              28 /* [30:28] */
#define CLK_CTRL_FRAC               0  /* [18:0] */

/* ******** MIPI_DSI_PHY ******** */
/* bit[15:11] */
#define MIPI_PHY_LANE_BIT        11
#define MIPI_PHY_LANE_WIDTH      5

/* MIPI-DSI */
#define DSI_LANE_0              (1 << 4)
#define DSI_LANE_1              (1 << 3)
#define DSI_LANE_CLK            (1 << 2)
#define DSI_LANE_2              (1 << 1)
#define DSI_LANE_3              (1 << 0)
#define DSI_LANE_COUNT_1        (DSI_LANE_CLK | DSI_LANE_0)
#define DSI_LANE_COUNT_2        (DSI_LANE_CLK | DSI_LANE_0 | DSI_LANE_1)
#define DSI_LANE_COUNT_3        (DSI_LANE_CLK | DSI_LANE_0 |\
                    DSI_LANE_1 | DSI_LANE_2)
#define DSI_LANE_COUNT_4        (DSI_LANE_CLK | DSI_LANE_0 |\
                    DSI_LANE_1 | DSI_LANE_2 | DSI_LANE_3)

#define OPERATION_VIDEO_MODE     0
#define OPERATION_COMMAND_MODE   1

#define SYNC_PULSE               0x0
#define SYNC_EVENT               0x1
#define BURST_MODE               0x2

/* command config */
#define DSI_CMD_SIZE_INDEX       1  /* byte[1] */
#define DSI_GPIO_INDEX           2  /* byte[2] */

#define DSI_INIT_ON_MAX          100
#define DSI_INIT_OFF_MAX         30

/* **** DPHY timing parameter       Value (unit: 0.01ns) **** */
/* >100ns (4M) */
#define DPHY_TIME_LP_TESC(ui)       (250 * 100)
/* >50ns */
#define DPHY_TIME_LP_LPX(ui)        (100 * 100)
/* (lpx, 2*lpx) */
#define DPHY_TIME_LP_TA_SURE(ui)    DPHY_TIME_LP_LPX(ui)
/* 4*lpx */
#define DPHY_TIME_LP_TA_GO(ui)      (4 * DPHY_TIME_LP_LPX(ui))
/* 5*lpx */
#define DPHY_TIME_LP_TA_GETX(ui)    (5 * DPHY_TIME_LP_LPX(ui))
/* >100ns */
#define DPHY_TIME_HS_EXIT(ui)       (110 * 100)
/* max(8*ui, 60+4*ui), (teot)<105+12*ui */
#define DPHY_TIME_HS_TRAIL(ui)      ((ui > (60 * 100 / 4)) ? \
                    (8 * ui) : ((60 * 100) + 4 * ui))
/* (40+4*ui, 85+6*ui) */
#define DPHY_TIME_HS_PREPARE(ui)    (50 * 100 + 4 * ui)
/* hs_prepare+hs_zero >145+10*ui */
#define DPHY_TIME_HS_ZERO(ui)       (160 * 100 + 10 * ui - \
                    DPHY_TIME_HS_PREPARE(ui))
/* >60ns, (teot)<105+12*ui */
#define DPHY_TIME_CLK_TRAIL(ui)     (70 * 100)
/* >60+52*ui */
#define DPHY_TIME_CLK_POST(ui)      (2 * (60 * 100 + 52 * ui))
/* (38, 95) */
#define DPHY_TIME_CLK_PREPARE(ui)   (50 * 100)
/* clk_prepare+clk_zero > 300 */
#define DPHY_TIME_CLK_ZERO(ui)      (320 * 100 - DPHY_TIME_CLK_PREPARE(ui))
/* >8*ui */
#define DPHY_TIME_CLK_PRE(ui)       (10 * ui)
/* >100us */
#define DPHY_TIME_INIT(ui)          (110 * 1000 * 100)
/* >1ms */
#define DPHY_TIME_WAKEUP(ui)        (1020 * 1000 * 100)

/*  MIPI DSI Relative REGISTERs Definitions */
/* For MIPI_DSI_TOP_CNTL */
#define BIT_DPI_COLOR_MODE        20
#define BIT_IN_COLOR_MODE         16
#define BIT_CHROMA_SUBSAMPLE      14
#define BIT_COMP2_SEL             12
#define BIT_COMP1_SEL             10
#define BIT_COMP0_SEL              8
#define BIT_DE_POL                 6
#define BIT_HSYNC_POL              5
#define BIT_VSYNC_POL              4
#define BIT_DPICOLORM              3
#define BIT_DPISHUTDN              2
#define BIT_EDPITE_INTR_PULSE      1
#define BIT_ERR_INTR_PULSE         0

/* For MIPI_DSI_DWC_CLKMGR_CFG_OS */
#define BIT_TO_CLK_DIV            8
#define BIT_TX_ESC_CLK_DIV        0

/* For MIPI_DSI_DWC_PCKHDL_CFG_OS */
#define BIT_CRC_RX_EN             4
#define BIT_ECC_RX_EN             3
#define BIT_BTA_EN                2
#define BIT_EOTP_RX_EN            1
#define BIT_EOTP_TX_EN            0

/* For MIPI_DSI_DWC_VID_MODE_CFG_OS */
#define BIT_LP_CMD_EN            15
#define BIT_FRAME_BTA_ACK_EN     14
#define BIT_LP_HFP_EN            13
#define BIT_LP_HBP_EN            12
#define BIT_LP_VCAT_EN           11
#define BIT_LP_VFP_EN            10
#define BIT_LP_VBP_EN             9
#define BIT_LP_VSA_EN             8
#define BIT_VID_MODE_TYPE         0

/* For MIPI_DSI_DWC_PHY_STATUS_OS */
#define BIT_PHY_ULPSACTIVENOT3LANE 12
#define BIT_PHY_STOPSTATE3LANE     11
#define BIT_PHY_ULPSACTIVENOT2LANE 10
#define BIT_PHY_STOPSTATE2LANE      9
#define BIT_PHY_ULPSACTIVENOT1LANE  8
#define BIT_PHY_STOPSTATE1LANE      7
#define BIT_PHY_RXULPSESC0LANE      6
#define BIT_PHY_ULPSACTIVENOT0LANE  5
#define BIT_PHY_STOPSTATE0LANE      4
#define BIT_PHY_ULPSACTIVENOTCLK    3
#define BIT_PHY_STOPSTATECLKLANE    2
#define BIT_PHY_DIRECTION           1
#define BIT_PHY_LOCK                0

/* For MIPI_DSI_DWC_PHY_IF_CFG_OS */
#define BIT_PHY_STOP_WAIT_TIME      8
#define BIT_N_LANES                 0

/* For MIPI_DSI_DWC_DPI_COLOR_CODING_OS */
#define BIT_LOOSELY18_EN            8
#define BIT_DPI_COLOR_CODING        0

/* For MIPI_DSI_DWC_GEN_HDR_OS */
#define BIT_GEN_WC_MSBYTE          16
#define BIT_GEN_WC_LSBYTE           8
#define BIT_GEN_VC                  6
#define BIT_GEN_DT                  0

/* For MIPI_DSI_DWC_LPCLK_CTRL_OS */
#define BIT_AUTOCLKLANE_CTRL        1
#define BIT_TXREQUESTCLKHS          0

/* For MIPI_DSI_DWC_DPI_CFG_POL_OS */
#define BIT_COLORM_ACTIVE_LOW       4
#define BIT_SHUTD_ACTIVE_LOW        3
#define BIT_HSYNC_ACTIVE_LOW        2
#define BIT_VSYNC_ACTIVE_LOW        1
#define BIT_DATAEN_ACTIVE_LOW       0

/* For MIPI_DSI_DWC_CMD_MODE_CFG_OS */
#define BIT_MAX_RD_PKT_SIZE        24
#define BIT_DCS_LW_TX              19
#define BIT_DCS_SR_0P_TX           18
#define BIT_DCS_SW_1P_TX           17
#define BIT_DCS_SW_0P_TX           16
#define BIT_GEN_LW_TX              14
#define BIT_GEN_SR_2P_TX           13
#define BIT_GEN_SR_1P_TX           12
#define BIT_GEN_SR_0P_TX           11
#define BIT_GEN_SW_2P_TX           10
#define BIT_GEN_SW_1P_TX            9
#define BIT_GEN_SW_0P_TX            8
#define BIT_ACK_RQST_EN             1
#define BIT_TEAR_FX_EN              0

/* For MIPI_DSI_DWC_CMD_PKT_STATUS_OS */
/* For DBI no use full */
#define BIT_DBI_RD_CMD_BUSY        14
#define BIT_DBI_PLD_R_FULL         13
#define BIT_DBI_PLD_R_EMPTY        12
#define BIT_DBI_PLD_W_FULL         11
#define BIT_DBI_PLD_W_EMPTY        10
#define BIT_DBI_CMD_FULL            9
#define BIT_DBI_CMD_EMPTY           8
/* For Generic interface */
#define BIT_GEN_RD_CMD_BUSY         6
#define BIT_GEN_PLD_R_FULL          5
#define BIT_GEN_PLD_R_EMPTY         4
#define BIT_GEN_PLD_W_FULL          3
#define BIT_GEN_PLD_W_EMPTY         2
#define BIT_GEN_CMD_FULL            1
#define BIT_GEN_CMD_EMPTY           0

/* For MIPI_DSI_TOP_MEAS_CNTL */
/* measure vsync control */
#define BIT_CNTL_MEAS_VSYNC        10
/* tear measure enable */
#define BIT_EDPITE_MEAS_EN          9
/* not clear the counter */
#define BIT_EDPITE_ACCUM_MEAS_EN    8
#define BIT_EDPITE_VSYNC_SPAN       0

/* For MIPI_DSI_TOP_STAT */
/* signal from halt */
#define BIT_STAT_EDPIHALT          31
/* line number when edpite pulse */
#define BIT_STAT_TE_LINE           16
/* pixel number when edpite pulse */
#define BIT_STAT_TE_PIXEL           0

/* For MIPI_DSI_TOP_INTR_CNTL_STAT */
/* State/Clear for pic_eof */
#define BIT_STAT_CLR_DWC_PIC_EOF   21
/* State/Clear for de_fall */
#define BIT_STAT_CLR_DWC_DE_FALL   20
/* State/Clear for de_rise */
#define BIT_STAT_CLR_DWC_DE_RISE   19
/* State/Clear for vs_fall */
#define BIT_STAT_CLR_DWC_VS_FALL   18
/* State/Clear for vs_rise */
#define BIT_STAT_CLR_DWC_VS_RISE   17
/* State/Clear for edpite */
#define BIT_STAT_CLR_DWC_EDPITE    16
/* end of picture */
#define BIT_PIC_EOF                 5
/* data enable fall */
#define BIT_DE_FALL                 4
/* data enable rise */
#define BIT_DE_RISE                 3
/* vsync fall */
#define BIT_VS_FALL                 2
/* vsync rise */
#define BIT_VS_RISE                 1
/* edpite int enable */
#define BIT_EDPITE_INT_EN           0

/* For MIPI_DSI_TOP_MEAS_CNTL */
/* vsync measure enable */
#define BIT_VSYNC_MEAS_EN          19
/* vsync accumulate measure */
#define BIT_VSYNC_ACCUM_MEAS_EN    18
/* vsync span */
#define BIT_VSYNC_SPAN             10
/* tearing measure enable */
#define BIT_TE_MEAS_EN              9
/* tearing accumulate measure */
#define BIT_TE_ACCUM_MEAS_EN        8
/* tearing span */
#define BIT_TE_SPAN                 0

/* For MIPI_DSI_DWC_INT_ST0_OS */
/* LP1 contention error from lane0 */
#define BIT_DPHY_ERR_4             20
/* LP0 contention error from lane0 */
#define BIT_DPHY_ERR_3             19
/* ErrControl error from lane0 */
#define BIT_DPHY_ERR_2             18
/* ErrSyncEsc error from lane0 */
#define BIT_DPHY_ERR_1             17
/* ErrEsc escape error lane0 */
#define BIT_DPHY_ERR_0             16
#define BIT_ACK_ERR_15             15
#define BIT_ACK_ERR_14             14
#define BIT_ACK_ERR_13             13
#define BIT_ACK_ERR_12             12
#define BIT_ACK_ERR_11             11
#define BIT_ACK_ERR_10             10
#define BIT_ACK_ERR_9               9
#define BIT_ACK_ERR_8               8
#define BIT_ACK_ERR_7               7
#define BIT_ACK_ERR_6               6
#define BIT_ACK_ERR_5               5
#define BIT_ACK_ERR_4               4
#define BIT_ACK_ERR_3               3
#define BIT_ACK_ERR_2               2
#define BIT_ACK_ERR_1               1
#define BIT_ACK_ERR_0               0

/* Command transfer type in command mode */
#define DCS_TRANS_HS                0
#define DCS_TRANS_LP                1

#define MIPI_DSI_DCS_NO_ACK         0
#define MIPI_DSI_DCS_REQ_ACK        1

/* DSI Tear Defines */
#define MIPI_DCS_SET_TEAR_ON_MODE_0         0
#define MIPI_DCS_SET_TEAR_ON_MODE_1         1
#define MIPI_DCS_ENABLE_TEAR                1
#define MIPI_DCS_DISABLE_TEAR               0


/*  MIPI DCS Pixel-to-Byte Format */
#define DCS_PF_RSVD                  0x0
#define DCS_PF_3BIT                  0x1
#define DCS_PF_8BIT                  0x2
#define DCS_PF_12BIT                 0x3
#define DCS_PF_16BIT                 0x5
#define DCS_PF_18BIT                 0x6
#define DCS_PF_24BIT                 0x7

/*  MIPI DSI/VENC Color Format Definitions */
#define MIPI_DSI_VENC_COLOR_30B   0x0
#define MIPI_DSI_VENC_COLOR_24B   0x1
#define MIPI_DSI_VENC_COLOR_18B   0x2
#define MIPI_DSI_VENC_COLOR_16B   0x3

#define COLOR_16BIT_CFG_1         0x0
#define COLOR_16BIT_CFG_2         0x1
#define COLOR_16BIT_CFG_3         0x2
#define COLOR_18BIT_CFG_1         0x3
#define COLOR_18BIT_CFG_2         0x4
#define COLOR_24BIT               0x5
#define COLOR_20BIT_LOOSE         0x6
#define COLOR_24_BIT_YCBCR        0x7
#define COLOR_16BIT_YCBCR         0x8
#define COLOR_30BIT               0x9
#define COLOR_36BIT               0xa
#define COLOR_12BIT               0xb
#define COLOR_RGB_111             0xc
#define COLOR_RGB_332             0xd
#define COLOR_RGB_444             0xe


enum div_sel_e {
    CLK_DIV_SEL_1 = 0,
    CLK_DIV_SEL_2,    /* 1 */
    CLK_DIV_SEL_3,    /* 2 */
    CLK_DIV_SEL_3p5,  /* 3 */
    CLK_DIV_SEL_3p75, /* 4 */
    CLK_DIV_SEL_4,    /* 5 */
    CLK_DIV_SEL_5,    /* 6 */
    CLK_DIV_SEL_6,    /* 7 */
    CLK_DIV_SEL_6p25, /* 8 */
    CLK_DIV_SEL_7,    /* 9 */
    CLK_DIV_SEL_7p5,  /* 10 */
    CLK_DIV_SEL_12,   /* 11 */
    CLK_DIV_SEL_14,   /* 12 */
    CLK_DIV_SEL_15,   /* 13 */
    CLK_DIV_SEL_2p5,  /* 14 */
    CLK_DIV_SEL_MAX,
};


typedef struct {
    uint32_t lcd_bits;
    uint32_t h_active;
    uint32_t v_active;
    uint32_t h_period;
    uint32_t v_period;

    uint32_t fr_adj_type;
    uint32_t ss_level;
    uint32_t clk_auto_gen;
    uint32_t lcd_clock;

    uint32_t clk_ctrl;
    uint32_t pll_ctrl;
    uint32_t div_ctrl;
    uint32_t clk_change;

    uint32_t lcd_clk_dft;
    uint32_t hPeriod_dft;
    uint32_t vPeriod_dft;

    uint32_t sync_duration_numerator;
    uint32_t sync_duration_denominator;

    uint32_t vid_pixel_on;
    uint32_t vid_line_on;

    uint32_t hSync_width;
    uint32_t hSync_backPorch;
    uint32_t hSync_pol;
    uint32_t vSync_width;
    uint32_t vSync_backPorch;
    uint32_t vSync_pol;

    uint32_t hOffset;
    uint32_t vOffset;

    uint32_t de_hs_addr;
    uint32_t de_he_addr;
    uint32_t de_vs_addr;
    uint32_t de_ve_addr;

    uint32_t hs_hs_addr;
    uint32_t hs_he_addr;
    uint32_t hs_vs_addr;
    uint32_t hs_ve_addr;

    uint32_t vs_hs_addr;
    uint32_t vs_he_addr;
    uint32_t vs_vs_addr;
    uint32_t vs_ve_addr;
} lcd_timing_t;

typedef struct {
    uint32_t lane_num;
    uint32_t bit_rate_max;
    uint32_t bit_rate_min;
    uint32_t bit_rate;
    uint32_t clock_factor;
    uint32_t factor_numerator;
    uint32_t factor_denominator;
    uint32_t opp_mode_init;
    uint32_t opp_mode_display;
    uint32_t video_mode_type;
    uint32_t clk_always_hs;
    uint32_t phy_switch;
    uint32_t venc_data_width;
    uint32_t dpi_data_format;

    uint32_t hLine;              // Overall time for each video line
    uint32_t hSyncActive;        // Horizontal Sync Active Period
    uint32_t hBackporch;         // Horizontal Back Porch Period
    uint32_t vSyncActive;        // Vertical Sync Active Period
    uint32_t vBackporch;         // Vertical Back Porch Period
    uint32_t vFrontporch;        // Vertical Front Porch Period
    uint32_t vActiveLines;       // Vertical Active Period

    unsigned char check_en;
    unsigned char check_reg;
    unsigned char check_cnt;
    unsigned char check_state;

} dsi_config_t;

typedef struct {
    uint32_t lp_tesc;
    uint32_t lp_lpx;
    uint32_t lp_ta_sure;
    uint32_t lp_ta_go;
    uint32_t lp_ta_get;
    uint32_t hs_exit;
    uint32_t hs_trail;
    uint32_t hs_zero;
    uint32_t hs_prepare;
    uint32_t clk_trail;
    uint32_t clk_post;
    uint32_t clk_zero;
    uint32_t clk_prepare;
    uint32_t clk_pre;
    uint32_t init;
    uint32_t wakeup;
    uint32_t state_change;
} dsi_phy_config_t;

typedef struct {
    int         data_bits;
    int         vid_num_chunks;
    int         pixel_per_chunk; /* pkt_size */
    int         vid_null_size;
    int         byte_per_chunk; /* internal usage */
    int         multi_pkt_en;   /* internal usage */

    /* vid timing */
    uint32_t    hline;
    uint32_t    hsa;
    uint32_t    hbp;
    uint32_t    vsa;
    uint32_t    vbp;
    uint32_t    vfp;
    uint32_t    vact;
} dsi_video_t;

struct dsi_cmd_request_s {
    unsigned char data_type;
    unsigned char vc_id;
    unsigned char *payload;
    unsigned short pld_count;
    unsigned int req_ack;
};




enum mipi_dsi_data_type_host_e {
    DT_VSS                  = 0x01,
    DT_VSE                  = 0x11,
    DT_HSS                  = 0x21,
    DT_HSE                  = 0x31,
    DT_EOTP                 = 0x08,
    DT_CMOFF                = 0x02,
    DT_CMON                 = 0x12,
    DT_SHUT_DOWN            = 0x22,
    DT_TURN_ON              = 0x32,
    DT_GEN_SHORT_WR_0       = 0x03,
    DT_GEN_SHORT_WR_1       = 0x13,
    DT_GEN_SHORT_WR_2       = 0x23,
    DT_GEN_RD_0             = 0x04,
    DT_GEN_RD_1             = 0x14,
    DT_GEN_RD_2             = 0x24,
    DT_DCS_SHORT_WR_0       = 0x05,
    DT_DCS_SHORT_WR_1       = 0x15,
    DT_DCS_RD_0             = 0x06,
    DT_SET_MAX_RET_PKT_SIZE = 0x37,
    DT_NULL_PKT             = 0x09,
    DT_BLANK_PKT            = 0x19,
    DT_GEN_LONG_WR          = 0x29,
    DT_DCS_LONG_WR          = 0x39,
    DT_20BIT_LOOSE_YCBCR    = 0x0c,
    DT_24BIT_YCBCR          = 0x1c,
    DT_16BIT_YCBCR          = 0x2c,
    DT_30BIT_RGB_101010     = 0x0d,
    DT_36BIT_RGB_121212     = 0x1d,
    DT_12BIT_YCBCR          = 0x3d,
    DT_16BIT_RGB_565        = 0x0e,
    DT_18BIT_RGB_666        = 0x1e,
    DT_18BIT_LOOSE_RGB_666  = 0x2e,
    DT_24BIT_RGB_888        = 0x3e
};

/* DCS Command List */
#define DCS_ENTER_IDLE_MODE          0x39
#define DCS_ENTER_INVERT_MODE        0x21
#define DCS_ENTER_NORMAL_MODE        0x13
#define DCS_ENTER_PARTIAL_MODE       0x12
#define DCS_ENTER_SLEEP_MODE         0x10
#define DCS_EXIT_IDLE_MODE           0x38
#define DCS_EXIT_INVERT_MODE         0x20
#define DCS_EXIT_SLEEP_MODE          0x11
#define DCS_GET_3D_CONTROL           0x3f
#define DCS_GET_ADDRESS_MODE         0x0b
#define DCS_GET_BLUE_CHANNEL         0x08
#define DCS_GET_DIAGNOSTIC_RESULT    0x0f
#define DCS_GET_DISPLAY_MODE         0x0d
#define DCS_GET_GREEN_CHANNEL        0x07
#define DCS_GET_PIXEL_FORMAT         0x0c
#define DCS_GET_POWER_MODE           0x0a
#define DCS_GET_RED_CHANNEL          0x06
#define DCS_GET_SCANLINE             0x45
#define DCS_GET_SIGNAL_MODE          0x0e
#define DCS_NOP                      0x00
#define DCS_READ_DDB_CONTINUE        0xa8
#define DCS_READ_DDB_START           0xa1
#define DCS_READ_MEMORY_CONTINUE     0x3e
#define DCS_READ_MEMORY_START        0x2e
#define DCS_SET_3D_CONTROL           0x3d
#define DCS_SET_ADDRESS_MODE         0x36
#define DCS_SET_COLUMN_ADDRESS       0x2a
#define DCS_SET_DISPLAY_OFF          0x28
#define DCS_SET_DISPLAY_ON           0x29
#define DCS_SET_GAMMA_CURVE          0x26
#define DCS_SET_PAGE_ADDRESS         0x2b
#define DCS_SET_PARTIAL_COLUMNS      0x31
#define DCS_SET_PARTIAL_ROWS         0x30
#define DCS_SET_PIXEL_FORMAT         0x3a
#define DCS_SET_SCROLL_AREA          0x33
#define DCS_SET_SCROLL_START         0x37
#define DCS_SET_TEAR_OFF             0x34
#define DCS_SET_TEAR_ON              0x35
#define DCS_SET_TEAR_SCANLINE        0x44
#define DCS_SET_VSYNC_TIMING         0x40
#define DCS_SOFT_RESET               0x01
#define DCS_WRITE_LUT                0x2d
#define DCS_WRITE_MEMORY_CONTINUE    0x3c
#define DCS_WRITE_MEMORY_START       0x2c



enum mipi_dsi_data_type_peripheral_e {
    DT_RESP_TE             = 0xba,
    DT_RESP_ACK            = 0x84,
    DT_RESP_ACK_ERR        = 0x02,
    DT_RESP_EOT            = 0x08,
    DT_RESP_GEN_READ_1     = 0x11,
    DT_RESP_GEN_READ_2     = 0x12,
    DT_RESP_GEN_READ_LONG  = 0x1a,
    DT_RESP_DCS_READ_LONG  = 0x1c,
    DT_RESP_DCS_READ_1     = 0x21,
    DT_RESP_DCS_READ_2     = 0x22,
};






































#if 0
//------------------------------------------------------------------------------
// Top-level registers: AmLogic proprietary
//------------------------------------------------------------------------------
// 31: 4    Reserved.                                                                           Default 0.
//     3 RW ~tim_rst_n:  1=Assert SW reset on mipi_dsi_host_timing block.   0=Release reset.    Default 1.
//     2 RW ~dpi_rst_n:  1=Assert SW reset on mipi_dsi_host_dpi block.      0=Release reset.    Default 1.
//     1 RW ~intr_rst_n: 1=Assert SW reset on mipi_dsi_host_intr block.     0=Release reset.    Default 1.
//     0 RW ~dwc_rst_n:  1=Assert SW reset on IP core.                      0=Release reset.    Default 1.
#define MIPI_DSI_TOP_SW_RESET                      (0x1cf0)
// 31: 5    Reserved.                                                                                                       Default 0.
//     4 RW manual_edpihalt:  1=Manual suspend VencL; 0=do not suspend VencL.                                               Default 0.
//     3 RW auto_edpihalt_en: 1=Enable IP's edpihalt signal to suspend VencL; 0=IP's edpihalt signal does not affect VencL. Default 0.
//     2 RW clock_freerun: Apply to auto-clock gate only.                                                                   Default 0.
//                          0=Default, use auto-clock gating to save power;
//                          1=use free-run clock, disable auto-clock gating, for debug mode.
//     1 RW enable_pixclk: A manual clock gate option, due to DWC IP does not have auto-clock gating. 1=Enable pixclk.      Default 0.
//     0 RW enable_sysclk: A manual clock gate option, due to DWC IP does not have auto-clock gating. 1=Enable sysclk.      Default 0.
#define MIPI_DSI_TOP_CLK_CNTL                      (0x1cf1)
// 31:27    Reserved.                                                                       Default 0.
//    26 RW de_dpi_pol:     1= Invert DE polarity from mipi_dsi_host_dpi.                   Default 0.
//    25 RW hsync_dpi_pol:  1= Invert HS polarity from mipi_dsi_host_dpi.                   Default 0.
//    24 RW vsync_dpi_pol:  1= Invert VS polarity from mipi_dsi_host_dpi.                   Default 0.
// 23:20 RW dpi_color_mode: Define DPI pixel format.                                        Default 0.
//                           0=16-bit RGB565 config 1;
//                           1=16-bit RGB565 config 2;
//                           2=16-bit RGB565 config 3;
//                           3=18-bit RGB666 config 1;
//                           4=18-bit RGB666 config 2;
//                           5=24-bit RGB888;
//                           6=20-bit YCbCr 4:2:2;
//                           7=24-bit YCbCr 4:2:2;
//                           8=16-bit YCbCr 4:2:2;
//                           9=30-bit RGB;
//                          10=36-bit RGB;
//                          11=12-bit YCbCr 4:2:0.
//    19    Reserved.                                                                       Default 0.
// 18:16 RW in_color_mode:  Define VENC data width.                                         Default 0.
//                          0=30-bit pixel;
//                          1=24-bit pixel;
//                          2=18-bit pixel, RGB666;
//                          3=16-bit pixel, RGB565.
// 15:14 RW chroma_subsample: Define method of chroma subsampling.                          Default 0.
//                            Applicable to YUV422 or YUV420 only.
//                            0=Use even pixel's chroma;
//                            1=Use odd pixel's chroma;
//                            2=Use averaged value between even and odd pair.
// 13:12 RW comp2_sel:  Select which component to be Cr or B: 0=comp0; 1=comp1; 2=comp2.    Default 2.
// 11:10 RW comp1_sel:  Select which component to be Cb or G: 0=comp0; 1=comp1; 2=comp2.    Default 1.
//  9: 8 RW comp0_sel:  Select which component to be Y  or R: 0=comp0; 1=comp1; 2=comp2.    Default 0.
//     7    Reserved.                                                                       Default 0.
//     6 RW de_venc_pol:    1= Invert DE polarity from VENC.                                Default 0.
//     5 RW hsync_venc_pol: 1= Invert HS polarity from VENC.                                Default 0.
//     4 RW vsync_venc_pol: 1= Invert VS polarity from VENC.                                Default 0.
//     3 RW dpicolorm:      Signal to IP.                                                   Default 0.
//     2 RW dpishutdn:      Signal to IP.                                                   Default 0.
//     1    Reserved.                                                                       Default 0.
//     0    Reserved.                                                                       Default 0.
#define MIPI_DSI_TOP_CNTL                          (0x1cf2)
// 31:16    Reserved.                                                                                                           Default 0.
// 15: 8 RW suspend_frame_rate: Define rate of timed-suspend.                                                                   Default 0.
//                              0=Execute suspend every frame; 1=Every other frame; ...; 255=Every 256 frame.
//  7: 3    Reserved.                                                                                                           Default 0.
//     2 RW timed_suspend_en:   1=Enable timed suspend VencL. 0=Disable timed suspend.                                          Default 0.
//     1 RW manual_suspend_en:  1=Enable manual suspend VencL. 1=Cancel manual suspend VencL.                                   Default 0.
//     0 RW suspend_on_edpihalt:1=Enable IP's edpihalt signal to suspend VencL; 0=IP's edpihalt signal does not affect VencL.   Default 1.
#define MIPI_DSI_TOP_SUSPEND_CNTL                  (0x1cf3)
// 31:29    Reserved.                                                                                                           Default 0.
// 28:16 RW suspend_line_end:   Define timed-suspend region. Suspend from [pix_start,line_start] to [pix_end,line_end].         Default 0.
// 15:13    Reserved.                                                                                                           Default 0.
// 12: 0 RW suspend_line_start: Define timed-suspend region. Suspend from [pix_start,line_start] to [pix_end,line_end].         Default 0.
#define MIPI_DSI_TOP_SUSPEND_LINE                  (0x1cf4)
// 31:29    Reserved.                                                                                                           Default 0.
// 28:16 RW suspend_pix_end:    Define timed-suspend region. Suspend from [pix_start,line_start] to [pix_end,line_end].         Default 0.
// 15:13    Reserved.                                                                                                           Default 0.
// 12: 0 RW suspend_pix_start:  Define timed-suspend region. Suspend from [pix_start,line_start] to [pix_end,line_end].         Default 0.
#define MIPI_DSI_TOP_SUSPEND_PIX                   (0x1cf5)
// 31:20    Reserved.                                                                                                           Default 0.
// 19:10 RW meas_vsync:     Control on measuring Host Controller's vsync.                                                       Default 0.
//                          [   19] meas_en:        1=Enable measurement
//                          [   18] accum_meas_en:  0=meas_count is cleared at the end of each measure;
//                                                  1=meas_count is accumulated at the end of each measure.
//                          [17:10] vsync_span:     Define the duration of a measure is to last for how many Vsyncs.
//  9: 0 RW meas_edpite:    Control on measuring Display Slave's edpite.                                                        Default 0.
//                          [    9] meas_en:        1=Enable measurement
//                          [    8] accum_meas_en:  0=meas_count is cleared at the end of each measure;
//                                                  1=meas_count is accumulated at the end of each measure.
//                          [ 7: 0] edpite_span:    Define the duration of a measure is to last for how many edpite.
#define MIPI_DSI_TOP_MEAS_CNTL                     (0x1cf6)
//    31 R  stat_edpihalt:  status of edpihalt signal from IP.              Default 0.
// 30:29    Reserved.                                                       Default 0.
// 28:16 R  stat_te_line:   Snapshot of Host's line position at edpite.     Default 0.
// 15:13    Reserved.                                                       Default 0.
// 12: 0 R  stat_te_pix:    Snapshot of Host's pixel position at edpite.    Default 0.
#define MIPI_DSI_TOP_STAT                          (0x1cf7)
// To measure display slave's frame rate, we can use a reference clock to measure the duration of one of more edpite pulse(s).
// Measurement control is by register MIPI_DSI_TOP_MEAS_CNTL bit[9:0].
// Reference clock comes from clk_rst_tst.cts_dsi_meas_clk, and is defined by HIU register HHI_VDIN_MEAS_CLK_CNTL bit[23:12].
// Mesurement result is in MIPI_DSI_TOP_MEAS_STAT_TE0 and MIPI_DSI_TOP_MEAS_STAT_TE1, as below:
// edpite_meas_count[47:0]: Number of reference clock cycles counted during one measure period (non-incremental measure), or
//                          during all measure periods so far (incremental measure).
// edpite_meas_count_n[3:0]:Number of measure periods has been done. Number can wrap over.
//
// 31: 0 R  edpite_meas_count[31:0].    Default 0.
#define MIPI_DSI_TOP_MEAS_STAT_TE0                 (0x1cf8)
// 19:16 R  edpite_meas_count_n.        Default 0.
// 15: 0 R  edpite_meas_count[47:32].   Default 0.
#define MIPI_DSI_TOP_MEAS_STAT_TE1                 (0x1cf9)
// To measure Host's frame rate, we can use a reference clock to measure the duration of one of more Vsync pulse(s).
// Measurement control is by register MIPI_DSI_TOP_MEAS_CNTL bit[19:10].
// Reference clock comes from clk_rst_tst.cts_dsi_meas_clk, and is defined by HIU register HHI_VDIN_MEAS_CLK_CNTL bit[23:12].
// Mesurement result is in MIPI_DSI_TOP_MEAS_STAT_VS0 and MIPI_DSI_TOP_MEAS_STAT_VS1, as below:
// vsync_meas_count[47:0]:  Number of reference clock cycles counted during one measure period (non-incremental measure), or
//                          during all measure periods so far (incremental measure).
// vsync_meas_count_n[3:0]: Number of measure periods has been done. Number can wrap over.
//
// 31: 0 R  vsync_meas_count[31:0].     Default 0.
#define MIPI_DSI_TOP_MEAS_STAT_VS0                 (0x1cfa)
// 19:16 R  vsync_meas_count_n.         Default 0.
// 15: 0 R  vsync_meas_count[47:32].    Default 0.
#define MIPI_DSI_TOP_MEAS_STAT_VS1                 (0x1cfb)
// 31:16 RW intr_stat/clr. For each bit, read as this interrupt level status, write 1 to clear. Default 0.
//                         Note: To clear the interrupt level, simply write 1 to the specific bit, no need to write 0 afterwards.
//          [31:22] Reserved
//          [   21] stat/clr of EOF interrupt
//          [   20] stat/clr of de_fall interrupt
//          [   19] stat/clr of de_rise interrupt
//          [   18] stat/clr of vs_fall interrupt
//          [   17] stat/clr of vs_rise interrupt
//          [   16] stat/clr of dwc_edpite interrupt
// 15: 0 RW intr_enable. For each bit, 1=enable this interrupt, 0=disable.                      Default 0.
//          [15: 6] Reserved
//          [    5] EOF (End_Of_Field) interrupt
//          [    4] de_fall interrupt
//          [    3] de_rise interrupt
//          [    2] vs_fall interrupt
//          [    1] vs_rise interrupt
//          [    0] dwc_edpite interrupt
#define MIPI_DSI_TOP_INTR_CNTL_STAT                (0x1cfc)
// 31: 2    Reserved.   Default 0.
//  1: 0 RW mem_pd.     Default 3.
#define MIPI_DSI_TOP_MEM_PD                        (0x1cfd)
#endif