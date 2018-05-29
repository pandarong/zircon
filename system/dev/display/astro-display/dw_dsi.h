// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#define DW_DSI_VERSION                  (0x00 << 2)    /* contains the vers of the DSI host controller */
#define DW_DSI_PWR_UP                   (0x01 << 2)    /* controls the power up of the core */
#define DW_DSI_CLKMGR_CFG               (0x02 << 2)    /* configs the factor for internal dividers */
#define DW_DSI_DPI_VCID                 (0x03 << 2)    /* configs the Virt Chan ID for DPI traffic */
#define DW_DSI_DPI_COLOR_CODING         (0x04 << 2)    /* configs DPI color coding */
#define DW_DSI_DPI_CFG_POL              (0x05 << 2)    /* configs the polarity of DPI signals */
#define DW_DSI_DPI_LP_CMD_TIM           (0x06 << 2)    /* configs the timing for lp cmds (in vid mode) */
#define DW_DSI_DBI_VCID                 (0x07 << 2)    /* configs Virtual Channel ID for DBI traffic */
#define DW_DSI_DBI_CFG                  (0x08 << 2)    /* configs the bit width of pixels for DBI */
#define DW_DSI_DBI_PARTITIONING_EN      (0x09 << 2)    /* host partition DBI traffic automatically */
#define DW_DSI_DBI_CMDSIZE              (0x0A << 2)    /* cmd size for auto partitioning of DBI */
#define DW_DSI_PCKHDL_CFG               (0x0B << 2)    /* how EoTp, BTA, CRC and ECC are to be used */
#define DW_DSI_GEN_VCID                 (0x0C << 2)    /* Virtual Channel ID of READ responses to store */
#define DW_DSI_MODE_CFG                 (0x0D << 2)    /* mode of op between Video or Command Mode */
#define DW_DSI_VID_MODE_CFG             (0x0E << 2)    /* Video mode operation config */
#define DW_DSI_VID_PKT_SIZE             (0x0F << 2)    /* video packet size */
#define DW_DSI_VID_NUM_CHUNKS           (0x10 << 2)    /* number of chunks to use  */
#define DW_DSI_VID_NULL_SIZE            (0x11 << 2)    /* configs the size of null packets */
#define DW_DSI_VID_HSA_TIME             (0x12 << 2)    /* configs the video HSA time */
#define DW_DSI_VID_HBP_TIME             (0x13 << 2)    /* configs the video HBP time */
#define DW_DSI_VID_HLINE_TIME           (0x14 << 2)    /* configs the overall time for each video line */
#define DW_DSI_VID_VSA_LINES            (0x15 << 2)    /* configs the VSA period */
#define DW_DSI_VID_VBP_LINES            (0x16 << 2)    /* configs the VBP period */
#define DW_DSI_VID_VFP_LINES            (0x17 << 2)    /* configs the VFP period */
#define DW_DSI_VID_VACTIVE_LINES        (0x18 << 2)    /* configs the vertical resolution of video */
#define DW_DSI_EDPI_CMD_SIZE            (0x19 << 2)    /* configs the size of eDPI packets */
#define DW_DSI_CMD_MODE_CFG             (0x1A << 2)    /* command mode operation config */
#define DW_DSI_GEN_HDR                  (0x1B << 2)    /* header for new packets */
#define DW_DSI_GEN_PLD_DATA             (0x1C << 2)    /* payload for packets sent using the Gen i/f */
#define DW_DSI_CMD_PKT_STATUS           (0x1D << 2)    /* info about FIFOs related to DBI and Gen i/f */
#define DW_DSI_TO_CNT_CFG               (0x1E << 2)    /* counters that trig timeout errors */
#define DW_DSI_HS_RD_TO_CNT             (0x1F << 2)    /* Peri Resp timeout after HS Rd operations */
#define DW_DSI_LP_RD_TO_CNT             (0x20 << 2)    /* Peri Resp timeout after LP Rd operations */
#define DW_DSI_HS_WR_TO_CNT             (0x21 << 2)    /* Peri Resp timeout after HS Wr operations */
#define DW_DSI_LP_WR_TO_CNT             (0x22 << 2)    /* Peri Resp timeout after LP Wr operations */
#define DW_DSI_BTA_TO_CNT               (0x23 << 2)    /* Peri Resp timeout after Bus Turnaround comp */
#define DW_DSI_SDF_3D                   (0x24 << 2)    /* 3D cntrl info for VSS packets in video mode. */
#define DW_DSI_LPCLK_CTRL               (0x25 << 2)    /* non continuous clock in the clock lane. */
#define DW_DSI_PHY_TMR_LPCLK_CFG        (0x26 << 2)    /* time for the clock lane  */
#define DW_DSI_PHY_TMR_CFG              (0x27 << 2)    /* time for the data lanes  */
#define DW_DSI_PHY_RSTZ                 (0x28 << 2)    /* controls resets and the PLL of the D-PHY. */
#define DW_DSI_PHY_IF_CFG               (0x29 << 2)    /* number of active lanes  */
#define DW_DSI_PHY_ULPS_CTRL            (0x2A << 2)    /* entering and leaving ULPS in the D- PHY. */
#define DW_DSI_PHY_TX_TRIGGERS          (0x2B << 2)    /* pins that activate triggers in the D-PHY */
#define DW_DSI_PHY_STATUS               (0x2C << 2)    /* contains info about the status of the D- PHY */
#define DW_DSI_PHY_TST_CTRL0            (0x2D << 2)    /* controls clock and clear pins of the D-PHY */
#define DW_DSI_PHY_TST_CTRL1            (0x2E << 2)    /* controls data and enable pins of the D-PHY */
#define DW_DSI_INT_ST0                  (0x3F << 2)    /* status of intr from ack and D-PHY */
#define DW_DSI_INT_ST1                  (0x30 << 2)    /* status of intr related to timeout, ECC, etc */
#define DW_DSI_INT_MSK0                 (0x31 << 2)    /* masks interrupts that affect the INT_ST0 reg */
#define DW_DSI_INT_MSK1                 (0x32 << 2)    /* masks interrupts that affect the INT_ST1 reg */
