// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddk/protocol/platform-bus.h>
#include <ddk/io-buffer.h>

#include <sync/completion.h>

#include "a113-bus.h"

#define AML_TDM_PHYS_BASE 0xff642000

typedef volatile struct {
    uint32_t    clk_gate_en;
    uint32_t    mclk_a_ctl;
    uint32_t    mclk_b_ctl;
    uint32_t    mclk_c_ctl;
    uint32_t    mclk_d_ctl;
    uint32_t    mclk_e_ctl;
    uint32_t    mclk_f_ctl;
    uint32_t    reserved0[9];
    uint32_t    mst_a_sclk_ctl0;
    uint32_t    mst_a_sclk_ctl1;
    uint32_t    mst_b_sclk_ctl0;
    uint32_t    mst_b_sclk_ctl1;
    uint32_t    mst_c_sclk_ctl0;
    uint32_t    mst_c_sclk_ctl1;
    uint32_t    mst_d_sclk_ctl0;
    uint32_t    mst_d_sclk_ctl1;
    uint32_t    mst_e_sclk_ctl0;
    uint32_t    mst_e_sclk_ctl1;
    uint32_t    mst_f_sclk_ctl0;
    uint32_t    mst_f_sclk_ctl1;
    uint32_t    reserved1[4];
    uint32_t    clk_tdmin_a_ctl;
    uint32_t    clk_tdmin_b_ctl;
    uint32_t    clk_tdmin_c_ctl;
    uint32_t    clk_tdmin_lb_ctl;
    uint32_t    clk_tdmout_a_ctl;
    uint32_t    clk_tdmout_b_ctl;
    uint32_t    clk_tdmout_c_ctl;

    uint32_t    clk_spdifin_ctl;
    uint32_t    clk_spdifout_ctl;
    uint32_t    clk_resample_ctl;
    uint32_t    clk_locker_ctl;
    uint32_t    clk_pdmin_ctl0;
    uint32_t    clk_pdmin_ctl1;

    uint32_t    reserved2[19];

    uint32_t    toddr_a_ctl0;
    uint32_t    toddr_a_ctl1;
    uint32_t    toddr_a_start_addr_a;
    uint32_t    toddr_a_finish_addr_a;
    uint32_t    toddr_a_int_addr;
    uint32_t    toddr_a_status1;
    uint32_t    toddr_a_status2;
    uint32_t    toddr_a_start_addr_b;
    uint32_t    toddr_a_finish_addr_b;
    uint32_t    reserved3[7];

    uint32_t    toddr_b_ctl0;
    uint32_t    toddr_b_ctl1;
    uint32_t    toddr_b_start_addr_a;
    uint32_t    toddr_b_finish_addr_a;
    uint32_t    toddr_b_int_addr;
    uint32_t    toddr_b_status1;
    uint32_t    toddr_b_status2;
    uint32_t    toddr_b_start_addr_b;
    uint32_t    toddr_b_finish_addr_b;
    uint32_t    reserved4[7];

    uint32_t    toddr_c_ctl0;
    uint32_t    toddr_c_ctl1;
    uint32_t    toddr_c_start_addr_a;
    uint32_t    toddr_c_finish_addr_a;
    uint32_t    toddr_c_int_addr;
    uint32_t    toddr_c_status1;
    uint32_t    toddr_c_status2;
    uint32_t    toddr_c_start_addr_b;
    uint32_t    toddr_c_finish_addr_b;
    uint32_t    reserved5[7];

    uint32_t    frddr_a_ctl0;
    uint32_t    frddr_a_ctl1;
    uint32_t    frddr_a_start_addr;
    uint32_t    frddr_a_finish_addr;
    uint32_t    frddr_a_int_addr;
    uint32_t    frddr_a_status1;
    uint32_t    frddr_a_status2;
    uint32_t    frddr_a_start_addr_b;
    uint32_t    frddr_a_finish_addr_b;
    uint32_t    reserved6[7];

    uint32_t    frddr_b_ctl0;
    uint32_t    frddr_b_ctl1;
    uint32_t    frddr_b_start_addr;
    uint32_t    frddr_b_finish_addr;
    uint32_t    frddr_b_int_addr;
    uint32_t    frddr_b_status1;
    uint32_t    frddr_b_status2;
    uint32_t    frddr_b_start_addr_b;
    uint32_t    frddr_b_finish_addr_b;
    uint32_t    reserved7[7];

    uint32_t    frddr_c_ctl0;
    uint32_t    frddr_c_ctl1;
    uint32_t    frddr_c_start_addr;
    uint32_t    frddr_c_finish_addr;
    uint32_t    frddr_c_int_addr;
    uint32_t    frddr_c_status1;
    uint32_t    frddr_c_status2;
    uint32_t    frddr_c_start_addr_b;
    uint32_t    frddr_c_finish_addr_b;
    uint32_t    reserved8[7];


//TODO - still more regs, will add as needed

} aml_tdm_regs_t;

typedef struct aml_tdm_dev aml_tdm_dev_t;

struct aml_tdm_dev {
    zx_handle_t    irq;
    a113_bus_t     *host_bus;
    io_buffer_t    regs_iobuff;
    aml_tdm_regs_t *virt_regs;
    mtx_t          mutex;
};



zx_status_t aml_tdm_init(aml_tdm_dev_t *device, a113_bus_t *host_bus);

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


