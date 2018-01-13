// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#pragma once

#include <sys/types.h>
#include <zircon/types.h>

// The boot memory reservation system is a one-use early boot mechanism for
// a platform to mark certain ranges of physical space as occupied by something
// prior to adding arenas to the PMM.
//
// boot_reserve_init() must be called before adding the first pmm arena and
// boot_reserve_wire() should be called after the last arena is added to mark
// pages the reserved ranges intersect as WIRED.
//
// As the PMM arenas are added, the boot reserved ranges are consulted to make
// sure the pmm data structures do not overlap with any reserved ranges.

void boot_reserve_init();
void boot_reserve_wire();

zx_status_t boot_reserve_add_range(paddr_t pa, size_t len);

struct reserve_range {
    paddr_t pa;
    size_t len;
};

// Given a range, find a non intersection region with any reserved area
// returns upper aligned.
// On failure, return range.len == 0.
// Used by the PMM arena initialization code to allocate memory for itself.
reserve_range boot_reserve_range_search(paddr_t range_pa, size_t range_len, size_t alloc_len);
