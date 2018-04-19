// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/zx/resource.h>

#include <zircon/syscalls.h>

namespace zx {

zx_status_t resource::create(const resource& parent,
                             uint32_t kind,
                             uint64_t base,
                             size_t len,
                             uint32_t flags,
                             const char* name,
                             size_t namelen,
                             resource* result) {
    resource h;
    zx_status_t status = zx_resource_create(parent.get(), kind, base, len, flags, name, namelen,
                                            h.reset_and_get_address());
    result->reset(h.release());
    return status;
}

} // namespace zx
