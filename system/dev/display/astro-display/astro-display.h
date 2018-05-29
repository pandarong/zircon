// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <threads.h>
#include <hw/reg.h>
#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/io-buffer.h>
#include <ddk/protocol/display.h>
#include <ddk/protocol/gpio.h>
#include <ddk/protocol/i2c.h>
#include <ddk/protocol/platform-device.h>
#include <ddk/protocol/platform-defs.h>
#include <zircon/device/display.h>
#include <zircon/listnode.h>
#include <zircon/types.h>
#include <zircon/assert.h>
#include <zircon/device/display.h>
#include <zircon/syscalls.h>

#include "hhi.h"
#include "dw_dsi.h"
#include "aml_dsi.h"

#define DISP_ERROR(fmt, ...)    zxlogf(ERROR, "[%s %d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define DISP_INFO(fmt, ...)     zxlogf(INFO, "[%s %d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define DISP_TRACE              zxlogf(INFO, "[%s %d]\n", __func__, __LINE__)

#define DISPLAY_MASK(start, count) (((1 << (count)) - 1) << (start))
#define DISPLAY_SET_MASK(mask, start, count, value) \
                        ((mask & ~DISPLAY_MASK(start, count)) | \
                                (((value) << (start)) & DISPLAY_MASK(start, count)))

#define READ32_DMC_REG(a)                   readl(io_buffer_virt(&display->mmio_dmc) + a)
#define WRITE32_DMC_REG(a, v)               writel(v, io_buffer_virt(&display->mmio_dmc) + a)

#define READ32_MIPI_DSI_REG(a)              readl(io_buffer_virt(&display->mmio_mipi_dsi) + a)
#define WRITE32_MIPI_DSI_REG(a, v)          writel(v, io_buffer_virt(&display->mmio_mipi_dsi) + a)

#define READ32_DSI_PHY_REG(a)               readl(io_buffer_virt(&display->mmio_dsi_phy) + a)
#define WRITE32_DSI_PHY_REG(a, v)           writel(v, io_buffer_virt(&display->mmio_dsi_phy) + a)

#define READ32_HHI_REG(a)                   readl(io_buffer_virt(&display->mmio_hhi) + a)
#define WRITE32_HHI_REG(a, v)               writel(v, io_buffer_virt(&display->mmio_hhi) + a)

#define READ32_VPU_REG(a)                   readl(io_buffer_virt(&display->mmio_vpu) + a)
#define WRITE32_VPU_REG(a, v)               writel(v, io_buffer_virt(&display->mmio_vpu) + a)

#define SET_BIT32(x, dest, value, start, count) \
            WRITE32_##x##_REG(dest, (READ32_##x##_REG(dest) & ~DISPLAY_MASK(start, count)) | \
                                (((value) << (start)) & DISPLAY_MASK(start, count)))

#define GET_BIT32(x, dest, start, count) \
            ((READ32_##x##_REG(dest) >> (start)) & ((1 << (count)) - 1))

#define WRITE32_REG(x, a, v)    WRITE32_##x##_REG(a, v)
#define READ32_REG(x, a)        READ32_##x##_REG(a)


#define DMC_CAV_LUT_DATAL               (0x12 << 2)
#define DMC_CAV_LUT_DATAH               (0x13 << 2)
#define DMC_CAV_LUT_ADDR                (0x14 << 2)

#define DMC_CAV_ADDR_LMASK              (0x1fffffff)
#define DMC_CAV_WIDTH_LMASK             (0x7)
#define DMC_CAV_WIDTH_LWID              (3)
#define DMC_CAV_WIDTH_LBIT              (29)

#define DMC_CAV_WIDTH_HMASK             (0x1ff)
#define DMC_CAV_WIDTH_HBIT              (0)
#define DMC_CAV_HEIGHT_MASK             (0x1fff)
#define DMC_CAV_HEIGHT_BIT              (9)

#define DMC_CAV_LUT_ADDR_INDEX_MASK     (0x7)
#define DMC_CAV_LUT_ADDR_RD_EN          (1 << 8)
#define DMC_CAV_LUT_ADDR_WR_EN          (2 << 8)

#define OSD2_DMC_CAV_INDEX 0x40

// Should match display_mmios table in board driver
enum {
    MMIO_CANVAS,
    MMIO_MPI_DSI,
    MMIO_DSI_PHY,
    MMIO_HHI,
    MMIO_VPU,
};

struct lcd_clk_config { /* unit: kHz */
    /* IN-OUT parameters */
    uint32_t fin;
    uint32_t fout;

    /* pll parameters */
    uint32_t pll_mode; /* txl */
    uint32_t od_fb;
    uint32_t pll_m;
    uint32_t pll_n;
    uint32_t pll_fvco;
    uint32_t pll_od1_sel;
    uint32_t pll_od2_sel;
    uint32_t pll_od3_sel;
    uint32_t pll_pi_div_sel; /* txhd */
    uint32_t pll_level;
    uint32_t pll_frac;
    uint32_t pll_fout;
    uint32_t ss_level;
    uint32_t div_sel;
    uint32_t xd;

    /* clk path node parameters */
    uint32_t ss_level_max;
    uint32_t pll_m_max;
    uint32_t pll_m_min;
    uint32_t pll_n_max;
    uint32_t pll_n_min;
    uint32_t pll_frac_range;
    uint32_t pll_od_sel_max;
    uint32_t div_sel_max;
    uint32_t xd_max;
    uint32_t pll_ref_fmax;
    uint32_t pll_ref_fmin;
    uint32_t pll_vco_fmax;
    uint32_t pll_vco_fmin;
    uint32_t pll_out_fmax;
    uint32_t pll_out_fmin;
    uint32_t div_in_fmax;
    uint32_t div_out_fmax;
    uint32_t xd_out_fmax;
    uint32_t err_fmin;
};

/* This structure is populated based on hardware/lcd type. Its values come from vendor.
 * This table is the top level structure used to populated all Clocks/LCD/DSI/BackLight/etc
 * values
 */
struct display_setting {
    // DSI/LCD Timings
    uint32_t lcd_bits;
    uint32_t hActive;
    uint32_t vActive;
    uint32_t hPeriod;
    uint32_t vPeriod;
    uint32_t hSync_width;
    uint32_t hSync_backPorch;
    uint32_t hSync_pol;
    uint32_t vSync_width;
    uint32_t vSync_backPorch;
    uint32_t vSync_pol;

    // LCD configs
    uint32_t fr_adj_type;
    uint32_t ss_level;
    uint32_t clk_auto_gen;
    uint32_t lcd_clock;

    // MIPI/DSI configs
    uint32_t lane_num;
    uint32_t bit_rate_max;
    uint32_t factor_numerator;
    uint32_t opp_mode_init;
    uint32_t opp_mode_display;
    uint32_t video_mode_type;
    uint32_t clk_always_hs;
    uint32_t phy_switch;
};

typedef struct {
    zx_device_t*                        zxdev;
    platform_device_protocol_t          pdev;
    zx_device_t*                        parent;
    zx_device_t*                        mydevice;
    zx_device_t*                        fbdevice;
    zx_handle_t                         bti;
    zx_handle_t                         inth;

    gpio_protocol_t                     gpio;
    i2c_protocol_t                      i2c;
    thrd_t                              main_thread;

    io_buffer_t                         mmio_dmc;
    io_buffer_t                         mmio_mipi_dsi;
    io_buffer_t                         mmio_dsi_phy;
    io_buffer_t                         mmio_hhi;
    io_buffer_t                         mmio_vpu;
    io_buffer_t                         fbuffer;
    zx_display_info_t                   disp_info;

    lcd_timing_t*                       lcd_timing;
    dsi_config_t*                       dsi_cfg;
    dsi_phy_config_t*                   dsi_phy_cfg;
    dsi_video_t*                        dsi_vid;
    struct display_setting*             disp_setting;
    struct lcd_clk_config*              lcd_clk_cfg;

    uint8_t                             input_color_format;
    uint8_t                             output_color_format;
    uint8_t                             color_depth;

    bool                                console_visible;
    zx_display_cb_t                     ownership_change_callback;
    void*                               ownership_change_cookie;
} astro_display_t;


void init_backlight(astro_display_t* display);
void config_canvas(astro_display_t* display);
zx_status_t astro_lcd_timing(astro_display_t* display);
zx_status_t astro_dsi_generate_hpll(astro_display_t* display);
zx_status_t astro_dsi_load_config(astro_display_t* display);
void dump_display_info(astro_display_t* display);

zx_status_t display_clock_init(astro_display_t* display);
void lcd_mipi_phy_set(astro_display_t* display, bool enable);
zx_status_t aml_dsi_host_on(astro_display_t* display);