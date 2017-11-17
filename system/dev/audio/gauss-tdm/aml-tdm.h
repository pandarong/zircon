// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddk/protocol/platform-bus.h>
#include <ddk/io-buffer.h>

#include <sync/completion.h>

//#include "a113-bus.h"

#define AML_TDM_PHYS_BASE 0xff642000

typedef struct {
    uint32_t ctl0;
    uint32_t ctl1;
} aml_tdm_sclk_ctl_t;

typedef enum {
    MCLK_A,
    MCLK_B,
    MCLK_C,
    MCLK_D,
    MCLK_E,
    MCLK_F
} aml_tdm_mclk_t;

typedef enum {
    TDM_OUT_A,
    TDM_OUT_B,
    TDM_OUT_C
} aml_tdm_out_t;

typedef enum {
    TDM_IN_A,
    TDM_IN_B,
    TDM_IN_C,
    TDM_IN_LB
} aml_tdm_in_t;

typedef struct {
    uint32_t    ctl0;
    uint32_t    ctl1;
    uint32_t    start_addr;
    uint32_t    finish_addr;
    uint32_t    int_addr;
    uint32_t    status1;
    uint32_t    status2;
    uint32_t    start_addr_b;
    uint32_t    finish_addr_b;
    uint32_t    reserved[7];
} aml_tdm_toddr_regs_t;

typedef struct {
    uint32_t    ctl0;
    uint32_t    ctl1;
    uint32_t    start_addr;
    uint32_t    finish_addr;
    uint32_t    int_addr;
    uint32_t    status1;
    uint32_t    status2;
    uint32_t    start_addr_b;
    uint32_t    finish_addr_b;
    uint32_t    reserved[7];
} aml_tdm_frddr_regs_t;

typedef struct {
    uint32_t     ctl;
    uint32_t     swap;
    uint32_t     mask[4];
    uint32_t     stat;
    uint32_t     mute_val;
    uint32_t     mute[4];
    uint32_t     reserved[4];
} aml_tdm_tdmin_regs_t;

typedef struct {
    uint32_t     ctl0;
    uint32_t     ctl1;
    uint32_t     swap;
    uint32_t     mask[4];
    uint32_t     stat;
    uint32_t     gain[2];
    uint32_t     mute_val;
    uint32_t     mute[4];
    uint32_t     mask_val;
} aml_tdm_tdmout_regs_t;

typedef volatile struct aml_tdm_regs {

    uint32_t            clk_gate_en;
    uint32_t            mclk_ctl[6];        //mclk control - a,b,c,d,e,f
    uint32_t            reserved0[9];

    aml_tdm_sclk_ctl_t  sclk_ctl[6];
    uint32_t            reserved1[4];

    uint32_t            clk_tdmin_ctl[4];   //tdm in control - a,b,c,lb
    uint32_t            clk_tdmout_ctl[3];  //tdm out control - a,b,c

    uint32_t            clk_spdifin_ctl;
    uint32_t            clk_spdifout_ctl;
    uint32_t            clk_resample_ctl;
    uint32_t            clk_locker_ctl;
    uint32_t            clk_pdmin_ctl0;
    uint32_t            clk_pdmin_ctl1;
    uint32_t            reserved2[19];

    aml_tdm_toddr_regs_t    toddr[3];
    aml_tdm_frddr_regs_t    frddr[3];

    uint32_t            arb_ctl;
    uint32_t            reserved3[15];

    uint32_t            lb_ctl0;
    uint32_t            lb_ctl1;
    uint32_t            reserved4[14];

    aml_tdm_tdmin_regs_t     tdmin[4];
    uint32_t                 reserved5[64];
    aml_tdm_tdmout_regs_t    tdmout[3];


//TODO - still more regs, will add as needed

} aml_tdm_regs_t;


/*
typedef struct aml_tdmout_dev aml_tdmout_dev_t;

struct aml_tdmout_dev {
    zx_handle_t    irq;
    io_buffer_t    regs_iobuff;
    io_buffer_t    ring_buff;
    aml_tdm_regs_t *regs;
    mtx_t          mutex;
};
*/


//zx_status_t aml_tdmout_init(aml_tdmout_dev_t *device, a113_bus_t *host_bus);

//Register offsets
#define AML_TDM_AUDIO_CLK_GATE_EN_REG    0x0000
#define AML_TDM_AUDIO_MCLK_A_CTL_REG     0x0001
#define AML_TDM_AUDIO_MCLK_B_CTL_REG     0x0002
#define AML_TDM_AUDIO_MCLK_C_CTL_REG     0x0003
#define AML_TDM_AUDIO_MCLK_D_CTL_REG     0x0004
#define AML_TDM_AUDIO_MCLK_E_CTL_REG     0x0005
#define AML_TDM_AUDIO_MCLK_F_CTL_REG     0x0006




#define AML_TDM_AUDIO_CLK_GATE_EN_REG    0x0000
#define AML_TDM_AUDIO_CLK_GATE_EN_REG    0x0000
#define AML_TDM_AUDIO_CLK_GATE_EN_REG    0x0000


