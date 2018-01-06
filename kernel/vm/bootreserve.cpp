// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <vm/bootreserve.h>

#include "vm_priv.h"

#include <sys/types.h>
#include <vm/pmm.h>
#include <trace.h>

#define LOCAL_TRACE MAX(VM_GLOBAL_TRACE, 1)

static const size_t NUM_RESERVES = 16;
static reserve_range res[NUM_RESERVES];
static size_t res_ptr;
static list_node reserved_page_list = LIST_INITIAL_VALUE(reserved_page_list);

void boot_reserve_add_range(paddr_t pa, size_t len) {
    dprintf(INFO, "PMM: boot reserve add [%#" PRIxPTR ", %#" PRIxPTR "]\n", pa, pa + len - 1);

    if (res_ptr == NUM_RESERVES) {
        panic("too many boot reservations\n");
    }

    res[res_ptr].pa = pa;
    res[res_ptr].len = len;
    res_ptr++;
}

void boot_reserve_wire() {
    for (size_t i = 0; i < res_ptr; i++) {
        size_t pages = ROUNDUP_PAGE_SIZE(res[i].len) / PAGE_SIZE;
        size_t actual = pmm_alloc_range(res[i].pa, pages, &reserved_page_list);
        if (actual != pages) {
            panic("unable to reserve reserved range\n");
        }

        dprintf(INFO, "PMM: boot reserve reserving [%#" PRIxPTR ", %#" PRIxPTR "]\n",
                res[i].pa, res[i].pa + res[i].len - 1);
    }

    // mark all of the pages we allocated as WIRED
    vm_page_t* p;
    list_for_every_entry (&reserved_page_list, p, vm_page_t, free.node) {
        p->state = VM_PAGE_STATE_WIRED;
    }
}

static paddr_t upper_align(paddr_t range_pa, size_t range_len, size_t len) {
    return (range_pa + range_len - len);
}

reserve_range boot_reserve_range_search(paddr_t range_pa, size_t range_len, size_t alloc_len) {
    LTRACEF("range pa %#" PRIxPTR " len %#zx alloc_len %#zx\n", range_pa, range_len, alloc_len);

    paddr_t alloc_pa = upper_align(range_pa, range_len, alloc_len);

    for (;;) {
        // see if it intersects any reserved range
        bool intersects = false;
        for (size_t i = 0; i < res_ptr; i++) {
            if (Intersects(res[i].pa, res[i].len, alloc_pa, alloc_len)) {
                intersects = true;
            }
        }
        if (!intersects) {
            LTRACEF("does not intersect, returning [%#" PRIxPTR ", %#" PRIxPTR "]\n",
                    alloc_pa, alloc_pa + alloc_len - 1);
            return { alloc_pa, alloc_len };
        }

        // XXX FAIL
        break;
    }

    LTRACEF("failed to allocate\n");
    return { 0, 0 };
}
