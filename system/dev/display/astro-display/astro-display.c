// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "astro-display.h"


const struct display_setting g_disp_setting = {
    .lcd_bits                   = 8,
    .hActive                    = 600,
    .vActive                    = 1024,
    .hPeriod                    = 680,
    .vPeriod                    = 1194,
    .hSync_width                = 24,
    .hSync_backPorch            = 36,
    .hSync_pol                  = 0,
    .vSync_width                = 10,
    .vSync_backPorch            = 80,
    .vSync_pol                  = 0,

    // Clock configs
    .fr_adj_type                = 0,
    .ss_level                   = 0,
    .clk_auto_gen               = 1,
    .lcd_clock                  = 48715200,

    //mipi vals
    .lane_num                   = 4,
    .bit_rate_max               = 400,
    .factor_numerator           = 0,
    .opp_mode_init              = 1,
    .opp_mode_display           = 0,
    .video_mode_type            = 2,
    .clk_always_hs              = 0,
    .phy_switch                 = 0,
};

struct lcd_clk_config g_lcd_clk_cfg = {
    .od_fb                  = PLL_FRAC_OD_FB_HPLL_G12A,
    .ss_level_max           = SS_LEVEL_MAX_HPLL_G12A,
    .pll_frac_range         = PLL_FRAC_RANGE_HPLL_G12A,
    .pll_od_sel_max         = PLL_OD_SEL_MAX_HPLL_G12A,
    .pll_vco_fmax           = PLL_VCO_MAX_HPLL_G12A,
    .pll_vco_fmin           = PLL_VCO_MIN_HPLL_G12A,
    .pll_m_max              = PLL_M_MAX_G12A,
    .pll_m_min              = PLL_M_MIN_G12A,
    .pll_n_max              = PLL_N_MAX_G12A,
    .pll_n_min              = PLL_N_MIN_G12A,
    .pll_ref_fmax           = PLL_FREF_MAX_G12A,
    .pll_ref_fmin           = PLL_FREF_MIN_G12A,
    .pll_out_fmax           = CRT_VID_CLK_IN_MAX_G12A,
    .pll_out_fmin           = PLL_VCO_MIN_HPLL_G12A / 16,
    .div_out_fmax           = CRT_VID_CLK_IN_MAX_G12A,
    .xd_out_fmax            = ENCL_CLK_IN_MAX_G12A,
};

static zx_status_t vc_set_mode(void* ctx, zx_display_info_t* info) {
    return ZX_OK;
}

static zx_status_t vc_get_mode(void* ctx, zx_display_info_t* info) {
    astro_display_t* display = ctx;
    memcpy(info, &display->disp_info, sizeof(zx_display_info_t));
    return ZX_OK;
}

static zx_status_t vc_get_framebuffer(void* ctx, void** framebuffer) {
    if (!framebuffer) return ZX_ERR_INVALID_ARGS;
    astro_display_t* display = ctx;
    *framebuffer = io_buffer_virt(&display->fbuffer);
    return ZX_OK;
}

static void flush_framebuffer(astro_display_t* display) {
    io_buffer_cache_flush(&display->fbuffer, 0,
        (display->disp_info.stride * display->disp_info.height *
            ZX_PIXEL_FORMAT_BYTES(display->disp_info.format)));
}

static void vc_flush_framebuffer(void* ctx) {
    flush_framebuffer(ctx);
}

static void vc_display_set_ownership_change_callback(void* ctx, zx_display_cb_t callback,
                                                     void* cookie) {
    astro_display_t* display = ctx;
    display->ownership_change_callback = callback;
    display->ownership_change_cookie = cookie;
}

static void vc_display_acquire_or_release_display(void* ctx, bool acquire) {
    astro_display_t* display = ctx;

    if (acquire) {
        display->console_visible = true;
        if (display->ownership_change_callback)
            display->ownership_change_callback(true, display->ownership_change_cookie);
    } else if (!acquire) {
        display->console_visible = false;
        if (display->ownership_change_callback)
            display->ownership_change_callback(false, display->ownership_change_cookie);
    }
}

static display_protocol_ops_t vc_display_proto = {
    .set_mode = vc_set_mode,
    .get_mode = vc_get_mode,
    .get_framebuffer = vc_get_framebuffer,
    .flush = vc_flush_framebuffer,
    .set_ownership_change_callback = vc_display_set_ownership_change_callback,
    .acquire_or_release_display = vc_display_acquire_or_release_display,
};

static void display_release(void* ctx) {
    astro_display_t* display = ctx;

    if (display) {
        io_buffer_release(&display->fbuffer);
        zx_handle_close(display->bti);
    }
    free(display);
}

static zx_protocol_device_t main_device_proto = {
    .version = DEVICE_OPS_VERSION,
    .release =  display_release,
};

struct display_client_device {
    astro_display_t* display;
    zx_device_t* device;
};

static zx_status_t display_client_ioctl(void* ctx, uint32_t op, const void* in_buf, size_t in_len,
                                        void* out_buf, size_t out_len, size_t* out_actual) {
    struct display_client_device* client_struct = ctx;
    astro_display_t* display = client_struct->display;
    switch (op) {
    case IOCTL_DISPLAY_GET_FB: {
        if (out_len < sizeof(ioctl_display_get_fb_t))
            return ZX_ERR_INVALID_ARGS;
        ioctl_display_get_fb_t* description = (ioctl_display_get_fb_t*)(out_buf);
        zx_status_t status = zx_handle_duplicate(display->fbuffer.vmo_handle, ZX_RIGHT_SAME_RIGHTS,
                                                    &description->vmo);
        if (status != ZX_OK)
            return ZX_ERR_NO_RESOURCES;
        description->info = display->disp_info;
        *out_actual = sizeof(ioctl_display_get_fb_t);
        if (display->ownership_change_callback)
            display->ownership_change_callback(false, display->ownership_change_cookie);
        return ZX_OK;
    }
    case IOCTL_DISPLAY_FLUSH_FB:
    case IOCTL_DISPLAY_FLUSH_FB_REGION:
        flush_framebuffer(display);
        return ZX_OK;
    default:
        DISP_ERROR("Invalid ioctl %d\n", op);
        return ZX_ERR_INVALID_ARGS;
    }
}

static zx_status_t display_client_close(void* ctx, uint32_t flags) {
    struct display_client_device* client_struct = ctx;
    astro_display_t* display = client_struct->display;
    if (display->ownership_change_callback)
        display->ownership_change_callback(true, display->ownership_change_cookie);
    free(ctx);
    return ZX_OK;
}

static zx_protocol_device_t client_device_proto = {
    .version = DEVICE_OPS_VERSION,
    .ioctl = display_client_ioctl,
    .close = display_client_close,
};

static zx_status_t vc_open(void* ctx, zx_device_t** dev_out, uint32_t flags) {
    struct display_client_device* s = calloc(1, sizeof(struct display_client_device));

    s->display = ctx;

    device_add_args_t vc_fbuff_args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "astro-display",
        .ctx = s,
        .ops = &client_device_proto,
        .flags = DEVICE_ADD_INSTANCE,
    };
    zx_status_t status = device_add(s->display->fbdevice, &vc_fbuff_args, &s->device);
    if (status != ZX_OK) {
        free(s);
        return status;
    }
    *dev_out = s->device;
    return ZX_OK;
}

static zx_protocol_device_t display_device_proto = {
    .version = DEVICE_OPS_VERSION,
    .open = vc_open,
};

static zx_status_t populate_init_config(astro_display_t* display) {
    display->disp_setting = calloc(1, sizeof(struct display_setting));
    if (!display->disp_setting) {
        DISP_ERROR("Could not allocate disp_setting structure.\n");
        return ZX_ERR_NO_MEMORY;
    }

    display->lcd_clk_cfg = calloc(1, sizeof(struct lcd_clk_config));
    if (!display->lcd_clk_cfg) {
        DISP_ERROR("Could not allocate lcd_clk_config structure\n");
        return ZX_ERR_NO_MEMORY;
    }

    memcpy(display->disp_setting, &g_disp_setting, sizeof(struct display_setting));
    memcpy(display->lcd_clk_cfg, &g_lcd_clk_cfg, sizeof(struct lcd_clk_config));

    return ZX_OK;
}

static zx_status_t setup_display_if(astro_display_t* display) {
    zx_status_t status;

    // allocate frame buffer
    display->disp_info.format = ZX_PIXEL_FORMAT_RGB_565;
    display->disp_info.width  = 608;
    display->disp_info.height = 1024;
    display->disp_info.pixelsize = ZX_PIXEL_FORMAT_BYTES(display->disp_info.format);
    // The astro display controller needs buffers with a stride that is an even
    // multiple of 32.
    display->disp_info.stride = ROUNDUP(display->disp_info.width,
                                        32 / display->disp_info.pixelsize);

    status = io_buffer_init(&display->fbuffer, display->bti,
                            (display->disp_info.stride * display->disp_info.height *
                             ZX_PIXEL_FORMAT_BYTES(display->disp_info.format)),
                            IO_BUFFER_RW | IO_BUFFER_CONTIG);
    if (status != ZX_OK) {
        return status;
    }

    // Populated internal structures based on predefined tables
    if ((status = populate_init_config(display)) != ZX_OK) {
        DISP_ERROR("populate_init_config failed!\n");
        return status;
    }

    // Populate internal LCD timing structure based on predefined tables
    if ((status = astro_lcd_timing(display)) != ZX_OK) {
        DISP_ERROR("astro_lcd_timing failed!\n");
        return status;
    }

    // Populate internal DSI Config structure based on predefined tables
    if ((status = astro_dsi_load_config(display)) != ZX_OK) {
        DISP_ERROR("astro_dsi_load_config failed!\n");
        return status;
    }

    // Populate dsi clock related values
    if ((status = astro_dsi_generate_hpll(display)) != ZX_OK) {
        DISP_ERROR("astro_dsi_generate_hpll failed!\n");
        return status;
    }

    display_clock_init(display);
    lcd_mipi_phy_set(display, true); // enable mipi-phy
    aml_dsi_host_on(display);

    // dump_display_info(display);

    config_canvas(display);
    init_backlight(display);

    zx_set_framebuffer(get_root_resource(), io_buffer_virt(&display->fbuffer),
                       display->fbuffer.size, display->disp_info.format,
                       display->disp_info.width, display->disp_info.height,
                       display->disp_info.stride);

    device_add_args_t vc_fbuff_args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "astro-display",
        .ctx = display,
        .ops = &display_device_proto,
        .proto_id = ZX_PROTOCOL_DISPLAY,
        .proto_ops = &vc_display_proto,
    };

    status = device_add(display->mydevice, &vc_fbuff_args, &display->fbdevice);
    if (status != ZX_OK) {
        free(display);
        return status;
    }
    return ZX_OK;
}

static int main_astro_display_thread(void *arg) {
    astro_display_t* display = arg;
    setup_display_if(display);
    return ZX_OK;
}

zx_status_t astro_display_bind(void* ctx, zx_device_t* parent) {
    astro_display_t* display = calloc(1, sizeof(astro_display_t));
    if (!display) {
        DISP_ERROR("Could not allocated display structure\n");
        return ZX_ERR_NO_MEMORY;
    }

    display->parent = parent;
    display->console_visible = true;

    zx_status_t status = device_get_protocol(parent, ZX_PROTOCOL_PLATFORM_DEV, &display->pdev);
    if (status !=  ZX_OK) {
        DISP_ERROR("Could not get parent protocol\n");
        goto fail;
    }

    // Obtain I2C Protocol
    status = device_get_protocol(parent, ZX_PROTOCOL_I2C, &display->i2c);
    if (status != ZX_OK) {
        DISP_ERROR("Could not obtain I2C protocol\n");
        goto fail;
    }

    // Obtain GPIO Protocol
    status = device_get_protocol(parent, ZX_PROTOCOL_GPIO, &display->gpio);
    if (status != ZX_OK) {
        DISP_ERROR("Could not obtain GPIO protocol\n");
        goto fail;
    }

    status = pdev_get_bti(&display->pdev, 0, &display->bti);
    if (status != ZX_OK) {
        DISP_ERROR("Could not get BTI handle\n");
        goto fail;
    }

    // Map all the various MMIOs
    status = pdev_map_mmio_buffer(&display->pdev, MMIO_CANVAS, ZX_CACHE_POLICY_UNCACHED_DEVICE,
        &display->mmio_dmc);
    if (status != ZX_OK) {
        DISP_ERROR("Could not map display MMIO DMC\n");
        goto fail;
    }

    status = pdev_map_mmio_buffer(&display->pdev, MMIO_MPI_DSI, ZX_CACHE_POLICY_UNCACHED_DEVICE,
        &display->mmio_mipi_dsi);
    if (status != ZX_OK) {
        DISP_ERROR("Could not map display MMIO MIPI_DSI\n");
        goto fail;
    }

    status = pdev_map_mmio_buffer(&display->pdev, MMIO_DSI_PHY, ZX_CACHE_POLICY_UNCACHED_DEVICE,
        &display->mmio_dsi_phy);
    if (status != ZX_OK) {
        DISP_ERROR("Could not map display MMIO DSI PHY\n");
        goto fail;
    }

    status = pdev_map_mmio_buffer(&display->pdev, MMIO_HHI, ZX_CACHE_POLICY_UNCACHED_DEVICE,
        &display->mmio_hhi);
    if (status != ZX_OK) {
        DISP_ERROR("Could not map display MMIO HHI\n");
        goto fail;
    }

    status = pdev_map_mmio_buffer(&display->pdev, MMIO_VPU, ZX_CACHE_POLICY_UNCACHED_DEVICE,
        &display->mmio_vpu);
    if (status != ZX_OK) {
        DISP_ERROR("Could not map display MMIO VPU\n");
        goto fail;
    }

    device_add_args_t vc_fbuff_args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "astro-display",
        .ctx = display,
        .ops = &main_device_proto,
        .flags = (DEVICE_ADD_NON_BINDABLE | DEVICE_ADD_INVISIBLE),
    };

    status = device_add(display->parent, &vc_fbuff_args, &display->mydevice);

    thrd_create_with_name(&display->main_thread, main_astro_display_thread, display,
                                                    "main_astro_display_thread");
    return ZX_OK;

fail:
    DISP_ERROR("bind failed! %d\n", status);
    display_release(display);
    return status;

}

static zx_driver_ops_t astro_display_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = astro_display_bind,
};

ZIRCON_DRIVER_BEGIN(astro_display, astro_display_driver_ops, "zircon", "0.1", 4)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_PLATFORM_DEV),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_AMLOGIC),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_PID, PDEV_PID_AMLOGIC_S905D2),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_DID, PDEV_DID_AMLOGIC_DISPLAY),
ZIRCON_DRIVER_END(astro_display)
