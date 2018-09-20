// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/sysmem/sysmem.h>

#include <fbl/algorithm.h>
#include <fuchsia/sysmem/c/fidl.h>
#include <lib/fidl/bind.h>
#include <ddk/debug.h>
#include <lib/zx/vmo.h>
#include <string.h>
#include <zircon/assert.h>
#include <zircon/syscalls.h>


zx_status_t PickImageFormat(const fuchsia_sysmem_BufferSpec &spec,
                                           fuchsia_sysmem_ImageFormat *format,
                                           size_t *buffer_size) {

    // For the simple case, just use whatever format was given.
    *format_out = {
        .width = spec.image.min_width;
        .height = spec.image.min_height;
        .layers = spec.image.layers;
        .pixel_format = spec.image.pixel_format;
        .color_space = spec.image.color_space;
    }
    // Need to choose bytes_per_row, which depends on pixel_format:
    // (More generally, it also depends on color space and BufferUsage,
    // but this is a simplified version.)
    switch (spec.image.pixel_format) {
        case fuchsia_sysmem_PixelFormatType_R8G8B8A8:
        case fuchsia_sysmem_PixelFormatType_BGRA32:
            format->layers[0].bytes_per_row = 4 * format->width;
            *buffer_size = fbl::round_up(format->image.height * format->image.planes[0].bytes_per_row, ZX_PAGE_SIZE);
            break;
        case fuchsia_sysmem_PixelFormatType_I420: // An NxN Y plane and (N/2)x(N/2) U and V planes.
            format->layers[0].bytes_per_row = format->width;
            format->layers[1].bytes_per_row = format->width / 2;
            format->layers[1].byte_offset = format->layers[0].bytes_per_row * format->height;
            format->layers[2].bytes_per_row = format->width / 2;
            format->layers[2].byte_offset = format->layers[1].byte_offset + format->layers[1].bytes_per_row * format->height / 2;
            *buffer_size = fbl::round_up(ormat->layers[2].byte_offset + format->layers[2].bytes_per_row * format->height / 2, ZX_PAGE_SIZE);
            break;

}



// This should be called with a fully specified BufferFormat, after the allocator has
// decided what format to use:
size_t GetBufferSize(const fuchsia_sysmem_BufferFormat& format) {
    // Simple GetBufferSize.  Only valid for simple formats:
    return fbl::round_up(format.image.height * format.image.planes[0].bytes_per_row, ZX_PAGE_SIZE);
}

static zx_status_t Allocator_AllocateCollection(void* ctx,
                                                uint32_t buffer_count,
                                                const fuchsia_sysmem_BufferSpec* spec,
                                                const fuchsia_sysmem_BufferUsage* usage,
                                                fidl_txn_t* txn) {
    fuchsia_sysmem_BufferCollectionInfo info;
    memset(&info, 0, sizeof(info));
    // Most basic usage of the allocator: create vmos with no special vendor format:
    // 1) Pick which format gets used.  For the simple case, just use whatever format was given.
    //    We also assume here that the format is an ImageFormat
    ZX_ASSERT(info.format.tag == fuchsia_sysmem_BufferFormatTagimage);
    info.format = *format;

    // 2) Determine the size of the buffer from the format.
    info.vmo_size = GetBufferSize(info.format);

    // 3) Allocate the buffers.  This will be specialized for different formats.
    info.buffer_count = buffer_count;
    zx_status_t status;
    for (uint32_t i = 0; i < buffer_count; ++i) {
        status = zx_vmo_create(info.vmo_size, 0, &info.vmos[i]);
        if (status != ZX_OK) {
            // Close the handles we created already.  We do not support partial allocations.
            for (uint32_t j = 0; j < i; ++j) {
                zx_handle_close(info.vmos[j]);
            }
            info.buffer_count = 0;
            zxlogf(ERROR, "Failed to allocate Buffer Collection\n");
            return fuchsia_sysmem_AllocatorAllocateCollection_reply(txn, ZX_ERR_NO_MEMORY, &info);
        }
    }
    // If everything is happy and allocated, can give ZX_OK:
    return fuchsia_sysmem_AllocatorAllocateCollection_reply(txn, ZX_OK, &info);
}

static zx_status_t Allocator_AllocateSharedCollection(void* ctx,
                                                      uint32_t buffer_count,
                                                      const fuchsia_sysmem_BufferSpec* spec,
                                                      zx_handle_t token_peer,
                                                      fidl_txn_t* txn) {
    return fuchsia_sysmem_AllocatorAllocateSharedCollection_reply(txn, ZX_ERR_NOT_SUPPORTED);
}

static zx_status_t Allocator_BindSharedCollection(void* ctx,
                                                  const fuchsia_sysmem_BufferUsage* usage,
                                                  zx_handle_t token,
                                                  fidl_txn_t* txn) {
    fuchsia_sysmem_BufferCollectionInfo info;
    memset(&info, 0, sizeof(info));
    return fuchsia_sysmem_AllocatorBindSharedCollection_reply(txn, ZX_ERR_NOT_SUPPORTED, &info);
}

static constexpr const fuchsia_sysmem_Allocator_ops_t allocator_ops = {
    .AllocateCollection = Allocator_AllocateCollection,
    .AllocateSharedCollection = Allocator_AllocateSharedCollection,
    .BindSharedCollection = Allocator_BindSharedCollection,
};

static zx_status_t connect(void* ctx, async_dispatcher_t* dispatcher,
                           const char* service_name, zx_handle_t request) {
    if (!strcmp(service_name, fuchsia_sysmem_Allocator_Name)) {
        return fidl_bind(dispatcher, request,
                         (fidl_dispatch_t*)fuchsia_sysmem_Allocator_dispatch,
                         ctx, &allocator_ops);
    }

    zx_handle_close(request);
    return ZX_ERR_NOT_SUPPORTED;
}

static constexpr const char* sysmem_services[] = {
    fuchsia_sysmem_Allocator_Name,
    nullptr,
};

static constexpr zx_service_ops_t sysmem_ops = {
    .init = nullptr,
    .connect = connect,
    .release = nullptr,
};

static constexpr zx_service_provider_t sysmem_service_provider = {
    .version = SERVICE_PROVIDER_VERSION,
    .services = sysmem_services,
    .ops = &sysmem_ops,
};

const zx_service_provider_t* sysmem_get_service_provider() {
    return &sysmem_service_provider;
}
