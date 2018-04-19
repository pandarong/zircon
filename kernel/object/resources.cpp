// Copyright 2017 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <object/resources.h>

#include <fbl/ref_ptr.h>
#include <object/process_dispatcher.h>
#include <object/resource_dispatcher.h>
#include <zircon/syscalls/resource.h>

// Check if the resource referenced by |handle| is of kind |kind|, or ZX_RSRC_KIND_ROOT.
//
// Possible errors:
// ++ ZX_ERR_ACCESS_DENIED: |handle| is not the right |kind| of handle.
// ++ ZX_ERR_WRONG_TYPE: |handle| is not a valid Resource handle.
zx_status_t validate_resource(zx_handle_t handle, uint32_t kind) {
    auto up = ProcessDispatcher::GetCurrent();
    fbl::RefPtr<ResourceDispatcher> resource;
    auto status = up->GetDispatcher(handle, &resource);
    if (status != ZX_OK) {
        return status;
    }

    auto res_kind = resource->get_kind();
    if (res_kind == kind || res_kind == ZX_RSRC_KIND_ROOT) {
        return ZX_OK;
    }

    return ZX_ERR_WRONG_TYPE;
}

// Check if the resource referenced by |handle| is of kind |kind|, or ZX_RSRC_KIND_ROOT. If
// |kind| matches the resource's kind, then range validation between |base| and |len| will
// be made against the resource's backing address space allocation.
//
// Possible errors:
// ++ ZX_ERR_ACCESS_DENIED: |handle| is not the right |kind| of handle, or does not provide
// access to the specific range of |base|, |base|+|length|.
// ++ ZX_ERR_WRONG_TYPE: |handle| is not a valid Resource handle, or does not match |kind|.
// ++ ZX_ERR_OUT_OF_RANGE: The range specified by |base| and |Len| is not granted by this
// resource.
zx_status_t validate_ranged_resource(zx_handle_t handle,
                                     uint32_t kind,
                                     uint64_t base,
                                     uint64_t len) {
    auto up = ProcessDispatcher::GetCurrent();
    fbl::RefPtr<ResourceDispatcher> resource;
    auto status = up->GetDispatcher(handle, &resource);
    if (status != ZX_OK) {
        return status;
    }

    // Root gets access to everything and has no region to match against
    if (resource->get_kind() == ZX_RSRC_KIND_ROOT) {
        return ZX_OK;
    }

    if (resource->get_kind() != kind) {
        return ZX_ERR_WRONG_TYPE;
    }

    // Ensure the requested address range fits within the resource's allocated range
    uintptr_t rbase;
    size_t rlen;
    resource->get_range(&rbase, &rlen);
    if (base < rbase ||
        len > rlen ||
        base + len > rbase + rlen) {
        return ZX_ERR_OUT_OF_RANGE;
    }

    return ZX_OK;
}
