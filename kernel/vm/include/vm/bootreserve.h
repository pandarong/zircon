// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#pragma once

#include <zircon/compiler.h>
#include <sys/types.h>

void boot_reserve_add_range(paddr_t pa, size_t len);

void boot_reserve_wire();

// given a range, find a non intersection region with any reserved area
// returns upper aligned
struct reserve_range {
    paddr_t pa;
    size_t len;
};

reserve_range boot_reserve_range_search(paddr_t range_pa, size_t range_len, size_t alloc_len);

