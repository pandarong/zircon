// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zircon/compiler.h>

__BEGIN_CDECLS

// The root resource
#define ZX_RSRC_KIND_ROOT         1u
#define ZX_RSRC_KIND_MMIO         2u
#define ZX_RSRC_KIND_IOPORT       3u
#define ZX_RSRC_KIND_IRQ          4u
#define ZX_RSRC_KIND_HYPERVISOR   5u
#define ZX_RSRC_KIND_COUNT        6u

__END_CDECLS
