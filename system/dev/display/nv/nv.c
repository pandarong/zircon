// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/binding.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/display.h>
#include <ddk/protocol/pci.h>
#include <hw/pci.h>

#include <assert.h>
#include <inttypes.h>
#include <zircon/syscalls.h>
#include <zircon/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// simple framebuffer device to match against an Nvidia display controler already
// initialized from EFI
#define NV_GFX_VID (0x10de)

typedef struct nv_disp_device {
    void* regs;
    uint64_t regs_size;
    zx_handle_t regs_handle;

    void* framebuffer;
    uint64_t framebuffer_size;
    zx_handle_t framebuffer_handle;

    zx_display_info_t info;
} nv_disp_device_t;

// implement display protocol
static zx_status_t nv_disp_set_mode(void* ctx, zx_display_info_t* info) {
    return ZX_ERR_NOT_SUPPORTED;
}

static zx_status_t nv_disp_get_mode(void* ctx, zx_display_info_t* info) {
    assert(info);
    nv_disp_device_t* device = ctx;

    memcpy(info, &device->info, sizeof(zx_display_info_t));
    return ZX_OK;
}

static zx_status_t nv_disp_get_framebuffer(void* ctx, void** framebuffer) {
    assert(framebuffer);
    nv_disp_device_t* device = ctx;

    (*framebuffer) = device->framebuffer;
    return ZX_OK;
}

static display_protocol_ops_t nv_disp_display_proto = {
    .set_mode = nv_disp_set_mode,
    .get_mode = nv_disp_get_mode,
    .get_framebuffer = nv_disp_get_framebuffer,
};

// implement device protocol

static void nv_disp_release(void* ctx) {
    nv_disp_device_t* device = ctx;

    if (device->regs) {
        zx_handle_close(device->regs_handle);
        device->regs_handle = -1;
    }

    if (device->framebuffer) {
        zx_handle_close(device->framebuffer_handle);
        device->framebuffer_handle = -1;
    }

    free(device);
}

static zx_protocol_device_t nv_disp_device_proto = {
    .version = DEVICE_OPS_VERSION,
    .release = nv_disp_release,
};

// implement driver object:

static zx_status_t nv_disp_bind(void* ctx, zx_device_t* dev) {
    pci_protocol_t pci;
    zx_status_t status;

    if (device_get_protocol(dev, ZX_PROTOCOL_PCI, &pci))
        return ZX_ERR_NOT_SUPPORTED;

    // map resources and initialize the device
    nv_disp_device_t* device = calloc(1, sizeof(nv_disp_device_t));
    if (!device)
        return ZX_ERR_NO_MEMORY;

    // map register window
    // seems to be bar 0
    status = pci_map_bar(&pci, 0u, ZX_CACHE_POLICY_UNCACHED_DEVICE,
                              &device->regs, &device->regs_size, &device->regs_handle);
    if (status != ZX_OK) {
        printf("nv: failed to map pci bar 5: %d\n", status);
        goto fail;
    }

    // map framebuffer window
    // seems to be bar 1
    status = pci_map_bar(&pci, 1u, ZX_CACHE_POLICY_WRITE_COMBINING,
                              &device->framebuffer,
                              &device->framebuffer_size,
                              &device->framebuffer_handle);
    if (status != ZX_OK) {
        printf("nv-disp: failed to map pci bar 0: %d\n", status);
        goto fail;
    }

    zx_display_info_t* di = &device->info;
    uint32_t format, width, height, stride;
    status = zx_bootloader_fb_get_info(&format, &width, &height, &stride);
    if (status == ZX_OK) {
        di->format = format;
        di->width = width;
        di->height = height;
        di->stride = stride;
    } else {
        status = ZX_ERR_NOT_SUPPORTED;
        goto fail;
    }
    di->flags = ZX_DISPLAY_FLAG_HW_FRAMEBUFFER;

    zx_set_framebuffer(get_root_resource(), device->framebuffer, device->framebuffer_size,
                       format, width, height, stride);


    // create and add the display (char) device
   device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "nv_disp",
        .ctx = device,
        .ops = &nv_disp_device_proto,
        .proto_id = ZX_PROTOCOL_DISPLAY,
        .proto_ops = &nv_disp_display_proto,
    };

    status = device_add(dev, &args, NULL);
    if (status != ZX_OK) {
        goto fail;
    }

    printf("initialized nv display driver, reg=%p regsize=0x%" PRIx64 " fb=%p fbsize=0x%" PRIx64 "\n",
           device->regs, device->regs_size, device->framebuffer, device->framebuffer_size);
    printf("\twidth %u height %u stride %u format %u\n",
           device->info.width, device->info.height, device->info.stride, device->info.format);

    return ZX_OK;

fail:
    free(device);
    return status;
}

static zx_driver_ops_t nv_disp_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = nv_disp_bind,
};

// clang-format off
ZIRCON_DRIVER_BEGIN(nv_disp, nv_disp_driver_ops, "zircon", "0.1", 3)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_PCI),
    BI_ABORT_IF(NE, BIND_PCI_VID, NV_GFX_VID),
    BI_MATCH_IF(EQ, BIND_PCI_CLASS, 0x3), // display controller
ZIRCON_DRIVER_END(nv_disp)
