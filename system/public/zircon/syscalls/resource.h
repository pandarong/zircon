// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zircon/compiler.h>

// Resources that require a region allocator to handle exclusive reservations
// are defined in a contiguous block starting at 0 up to ZX_RSRC_STATIC_COUNT-1.
// After that point, all resource 'kinds' are abstract and need no underlying
// bookkeeping. It's important that ZX_RSRC_STATIC_COUNT is defined for each
// architecture to properly allocate only the bookkeeping necessary.

#define ZX_RSRC_KIND_MMIO           0u
#define ZX_RSRC_KIND_IRQ            1u
#if defined(__x86_64__)
    #define ZX_RSRC_KIND_IOPORT     2u
    #define ZX_RSRC_STATIC_COUNT    3u
#endif
#if defined (__aarch64__)
    #define ZX_RSRC_STATIC_COUNT    2u
#endif
#define ZX_RSRC_KIND_HYPERVISOR     ZX_RSRC_STATIC_COUNT
#define ZX_RSRC_KIND_ROOT           ZX_RSRC_KIND_HYPERVISOR + 1u
#define ZX_RSRC_KIND_COUNT          ZX_RSRC_KIND_ROOT + 1u

#define ZX_RSRC_FLAG_EXCLUSIVE      1u
#define ZX_RSRC_FLAGS_MASK          (ZX_RSRC_FLAG_EXCLUSIVE)
