// Copyright 2016 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT
#include "pmm_arena.h"

#include <err.h>
#include <inttypes.h>
#include <pretty/sizes.h>
#include <string.h>
#include <trace.h>

#include "pmm_node.h"
#include "vm_priv.h"

#define LOCAL_TRACE MAX(VM_GLOBAL_TRACE, 0)

PmmArena::PmmArena(const pmm_arena_info_t* info)
    : info_(*info) {}

PmmArena::~PmmArena() {}

void PmmArena::BootAllocArray(PmmNode *node) {
    /* allocate an array of pages to back this one */
    size_t page_count = size() / PAGE_SIZE;
    size_t size = page_count * VM_PAGE_STRUCT_SIZE;
    void* raw_page_array = boot_alloc_mem(size);

    LTRACEF("arena for base 0%#" PRIxPTR " size %#zx page array at %p size %zu\n", info_.base, info_.size,
            raw_page_array, size);

    memset(raw_page_array, 0, size);

    page_array_ = (vm_page*)raw_page_array;

    /* add them to the free list */
    list_node list;
    list_initialize(&list);
    for (size_t i = 0; i < page_count; i++) {
        auto& p = page_array_[i];
        p.paddr = base() + i * PAGE_SIZE;
        LTRACEF_LEVEL(2, "p %p, paddr 0x%lx\n", &p, p.paddr);

        list_add_tail(&list, &p.node);
    }

    node->AddFreePages(&list);
}

vm_page* PmmArena::FindSpecific(paddr_t pa) {
    if (!address_in_arena(pa))
        return nullptr;

    size_t index = (pa - base()) / PAGE_SIZE;

    DEBUG_ASSERT(index < size() / PAGE_SIZE);

    return get_page(index);
}

vm_page* PmmArena::FindFreeContiguous(size_t count, uint8_t alignment_log2) {
    /* walk the list starting at alignment boundaries.
     * calculate the starting offset into this arena, based on the
     * base address of the arena to handle the case where the arena
     * is not aligned on the same boundary requested.
     */
    paddr_t rounded_base = ROUNDUP(base(), 1UL << alignment_log2);
    if (rounded_base < base() || rounded_base > base() + size() - 1)
        return 0;

    paddr_t aligned_offset = (rounded_base - base()) / PAGE_SIZE;
    paddr_t start = aligned_offset;
    LTRACEF("starting search at aligned offset %#" PRIxPTR "\n", start);
    LTRACEF("arena base %#" PRIxPTR " size %zu\n", base(), size());

retry:
    /* search while we're still within the arena and have a chance of finding a slot
       (start + count < end of arena) */
    while ((start < size() / PAGE_SIZE) && ((start + count) <= size() / PAGE_SIZE)) {
        vm_page* p = &page_array_[start];
        for (uint i = 0; i < count; i++) {
            if (!page_is_free(p)) {
                /* this run is broken, break out of the inner loop.
                 * start over at the next alignment boundary
                 */
                start = ROUNDUP(start - aligned_offset + i + 1, 1UL << (alignment_log2 - PAGE_SIZE_SHIFT)) +
                        aligned_offset;
                goto retry;
            }
            p++;
        }

        /* we found a run */
        p = &page_array_[start];
        LTRACEF("found run from pa %#" PRIxPTR " to %#" PRIxPTR "\n", p->paddr, p->paddr + count * PAGE_SIZE);

        return p;
    }

    return nullptr;
}

void PmmArena::CountStates(size_t state_count[_VM_PAGE_STATE_COUNT]) const {
    for (size_t i = 0; i < size() / PAGE_SIZE; i++) {
        state_count[page_array_[i].state]++;
    }
}

void PmmArena::Dump(bool dump_pages, bool dump_free_ranges) const {
    char pbuf[16];
    printf("arena %p: name '%s' base %#" PRIxPTR " size %s (0x%zx) priority %u flags 0x%x\n", this, name(), base(),
           format_size(pbuf, sizeof(pbuf), size()), size(), priority(), flags());
    printf("\tpage_array %p\n", page_array_);

    /* dump all of the pages */
    if (dump_pages) {
        for (size_t i = 0; i < size() / PAGE_SIZE; i++) {
            dump_page(&page_array_[i]);
        }
    }

    /* count the number of pages in every state */
    size_t state_count[_VM_PAGE_STATE_COUNT] = {};
    CountStates(state_count);

    printf("\tpage states:\n");
    for (unsigned int i = 0; i < _VM_PAGE_STATE_COUNT; i++) {
        printf("\t\t%-12s %-16zu (%zu bytes)\n", page_state_to_string(i), state_count[i],
               state_count[i] * PAGE_SIZE);
    }

    /* dump the free pages */
    if (dump_free_ranges) {
        printf("\tfree ranges:\n");
        ssize_t last = -1;
        for (size_t i = 0; i < size() / PAGE_SIZE; i++) {
            if (page_is_free(&page_array_[i])) {
                if (last == -1) {
                    last = i;
                }
            } else {
                if (last != -1) {
                    printf("\t\t%#" PRIxPTR " - %#" PRIxPTR "\n", base() + last * PAGE_SIZE,
                           base() + i * PAGE_SIZE);
                }
                last = -1;
            }
        }

        if (last != -1) {
            printf("\t\t%#" PRIxPTR " - %#" PRIxPTR "\n", base() + last * PAGE_SIZE, base() + size());
        }
    }
}
