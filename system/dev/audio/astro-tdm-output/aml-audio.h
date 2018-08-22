// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#define    EE_AUDIO_MCLK_A     0
#define    EE_AUDIO_MCLK_B     1
#define    EE_AUDIO_MCLK_C     2
#define    EE_AUDIO_MCLK_D     3
#define    EE_AUDIO_MCLK_E     4
#define    EE_AUDIO_MCLK_F     5

#define    EE_AUDIO_TDMOUTA    0
#define    EE_AUDIO_TDMOUTB    1
#define    EE_AUDIO_TDMOUTC    2


typedef enum {
    MP0_PLL = 0,
    MP1_PLL = 1,
    MP2_PLL = 2,
    MP3_PLL = 3,
    HIFI_PLL = 4,
    FCLK_DIV3 = 5,
    FCLK_DIV4 = 6,
    GP0_PLL = 7
} ee_audio_mclk_src_t;


#define EE_AUDIO_NUM_MCLK_CHANNELS     6
#define EE_AUDIO_MCLK_ENA            (1 << 31)


#define EE_AUDIO_CLK_GATE_EN        0x0000
#define EE_AUDIO_MCLK_A_CTRL        0x0004
#define EE_AUDIO_MCLK_B_CTRL        0x0008
#define EE_AUDIO_MCLK_C_CTRL        0x000C
#define EE_AUDIO_MCLK_D_CTRL        0x0010
#define EE_AUDIO_MCLK_E_CTRL        0x0014
#define EE_AUDIO_MCLK_F_CTRL        0x0018

#define EE_AUDIO_MST_A_SCLK_CTRL0     0x0040
#define EE_AUDIO_MST_A_SCLK_CTRL1     0x0044
#define EE_AUDIO_MST_B_SCLK_CTRL0     0x0048
#define EE_AUDIO_MST_B_SCLK_CTRL1     0x004C
#define EE_AUDIO_MST_C_SCLK_CTRL0     0x0050
#define EE_AUDIO_MST_C_SCLK_CTRL1     0x0054
#define EE_AUDIO_MST_D_SCLK_CTRL0     0x0058
#define EE_AUDIO_MST_D_SCLK_CTRL1     0x005C
#define EE_AUDIO_MST_E_SCLK_CTRL0     0x0060
#define EE_AUDIO_MST_E_SCLK_CTRL1     0x0064
#define EE_AUDIO_MST_F_SCLK_CTRL0     0x0068
#define EE_AUDIO_MST_F_SCLK_CTRL1     0x006c


#define EE_AUDIO_CLK_TDMOUT_A_CTL     0x0090
#define EE_AUDIO_CLK_TDMOUT_B_CTL     0x0094
#define EE_AUDIO_CLK_TDMOUT_C_CTL     0x0098


//Audio clock gating masks
#define EE_AUDIO_CLK_GATE_PDM        (1 << 1)
#define EE_AUDIO_CLK_GATE_TDMINA     (1 << 2)
#define EE_AUDIO_CLK_GATE_TDMINB     (1 << 3)
#define EE_AUDIO_CLK_GATE_TDMINC     (1 << 4)
#define EE_AUDIO_CLK_GATE_TDMOUTA    (1 << 6)
#define EE_AUDIO_CLK_GATE_TDMOUTB    (1 << 7)
#define EE_AUDIO_CLK_GATE_TDMOUTC    (1 << 8)
#define EE_AUDIO_CLK_GATE_FRDDRA     (1 << 9)
#define EE_AUDIO_CLK_GATE_FRDDRB     (1 << 10)
#define EE_AUDIO_CLK_GATE_FRDDRC     (1 << 11)