// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#pragma once

#include <stddef.h>
#include <zircon/compiler.h>

__BEGIN_CDECLS

// Convenience routines to allocate page aligned chunks of kernel space.
// Each of the allocations are contained within a unique VMO + VM mapping.
void* vmalloc(size_t len, const char* name);
void vmfree(void* ptr);

__END_CDECLS
