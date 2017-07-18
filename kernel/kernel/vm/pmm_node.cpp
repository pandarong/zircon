// Copyright 2017 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT
#include "pmm_node.h"

#include <kernel/mp.h>
#include <mxcpp/new.h>
#include <trace.h>

#include "vm_priv.h"

#define LOCAL_TRACE MAX(VM_GLOBAL_TRACE, 0)

PmmNode::PmmNode() {
}

PmmNode::~PmmNode() {
}

// We disable thread safety analysis here, since this function is only called
// during early boot before threading exists.
status_t PmmNode::AddArena(const pmm_arena_info_t* info) TA_NO_THREAD_SAFETY_ANALYSIS {
    LTRACEF("arena %p name '%s' base %#" PRIxPTR " size %#zx\n", info, info->name, info->base, info->size);

    // Make sure we're in early boot (ints disabled and no active CPUs according
    // to the scheduler).
    DEBUG_ASSERT(mp_get_active_mask() == 0);
    DEBUG_ASSERT(arch_ints_disabled());

    DEBUG_ASSERT(IS_PAGE_ALIGNED(info->base));
    DEBUG_ASSERT(IS_PAGE_ALIGNED(info->size));
    DEBUG_ASSERT(info->size > 0);

    // allocate a c++ arena object
    PmmArena* arena = new (boot_alloc_mem(sizeof(PmmArena))) PmmArena(info);

    // walk the arena list and add arena based on priority order
    for (auto& a : arena_list_) {
        if (a.priority() > arena->priority()) {
            arena_list_.insert(a, arena);
            goto done_add;
        }
    }

    // walked off the end, add it to the end of the list
    arena_list_.push_back(arena);

done_add:
    // tell the arena to allocate a page array
    arena->BootAllocArray();

    arena_cumulative_size_ += info->size;

    return MX_OK;
}

vm_page_t* PmmNode::AllocPage(uint alloc_flags, paddr_t* pa) {
    AutoLock al(&lock_);

    /* walk the arenas in order until we find one with a free page */
    for (auto& a : arena_list_) {
        /* skip the arena if it's not KMAP and the KMAP only allocation flag was passed */
        if (alloc_flags & PMM_ALLOC_FLAG_KMAP) {
            if ((a.flags() & PMM_ARENA_FLAG_KMAP) == 0)
                continue;
        }

        // try to allocate the page out of the arena
        vm_page_t* page = a.AllocPage(pa);
        if (page)
            return page;
    }

    LTRACEF("failed to allocate page\n");
    return nullptr;
}

size_t PmmNode::AllocPages(size_t count, uint alloc_flags, list_node* list) {
    LTRACEF("count %zu\n", count);

    /* list must be initialized prior to calling this */
    DEBUG_ASSERT(list);

    if (count == 0)
        return 0;

    AutoLock al(&lock_);

    /* walk the arenas in order, allocating as many pages as we can from each */
    size_t allocated = 0;
    for (auto& a : arena_list_) {
        DEBUG_ASSERT(count > allocated);

        /* skip the arena if it's not KMAP and the KMAP only allocation flag was passed */
        if (alloc_flags & PMM_ALLOC_FLAG_KMAP) {
            if ((a.flags() & PMM_ARENA_FLAG_KMAP) == 0)
                continue;
        }

        // ask the arena to allocate some pages
        allocated += a.AllocPages(count - allocated, list);
        DEBUG_ASSERT(allocated <= count);
        if (allocated == count)
            break;
    }

    return allocated;
}

size_t PmmNode::AllocRange(paddr_t address, size_t count, list_node* list) {
    LTRACEF("address %#" PRIxPTR ", count %zu\n", address, count);

    uint allocated = 0;
    if (count == 0)
        return 0;

    address = ROUNDDOWN(address, PAGE_SIZE);

    AutoLock al(&lock_);

    /* walk through the arenas, looking to see if the physical page belongs to it */
    for (auto& a : arena_list_) {
        while (allocated < count && a.address_in_arena(address)) {
            vm_page_t* page = a.AllocSpecific(address);
            if (!page)
                break;

            if (list)
                list_add_tail(list, &page->free.node);

            allocated++;
            address += PAGE_SIZE;
        }

        if (allocated == count)
            break;
    }

    return allocated;
}

size_t PmmNode::AllocContiguous(size_t count, uint alloc_flags, uint8_t alignment_log2, paddr_t* pa,
                                list_node* list) {
    LTRACEF("count %zu, align %u\n", count, alignment_log2);

    if (count == 0)
        return 0;
    if (alignment_log2 < PAGE_SIZE_SHIFT)
        alignment_log2 = PAGE_SIZE_SHIFT;

    AutoLock al(&lock_);

    for (auto& a : arena_list_) {
        /* skip the arena if it's not KMAP and the KMAP only allocation flag was passed */
        if (alloc_flags & PMM_ALLOC_FLAG_KMAP) {
            if ((a.flags() & PMM_ARENA_FLAG_KMAP) == 0)
                continue;
        }

        size_t allocated = a.AllocContiguous(count, alignment_log2, pa, list);
        if (allocated > 0) {
            DEBUG_ASSERT(allocated == count);
            return allocated;
        }
    }

    LTRACEF("couldn't find run\n");
    return 0;
}

size_t PmmNode::Free(list_node* list) {
    LTRACEF("list %p\n", list);

    DEBUG_ASSERT(list);

    AutoLock al(&lock_);

    uint count = 0;
    while (!list_is_empty(list)) {
        vm_page_t* page = list_remove_head_type(list, vm_page_t, free.node);

        DEBUG_ASSERT(!page_is_free(page));

        /* see which arena this page belongs to and add it */
        for (auto& a : arena_list_) {
            if (a.FreePage(page) >= 0) {
                count++;
                break;
            }
        }
    }

    LTRACEF("returning count %u\n", count);

    return count;
}

uint64_t PmmNode::CountFreePagesLocked() const TA_REQ(lock_) {
    uint64_t free = 0u;
    for (const auto& a : arena_list_) {
        free += a.free_count();
    }
    return free;
}

uint64_t PmmNode::CountFreePages() const {
    AutoLock al(&lock_);
    return CountFreePagesLocked();
}

uint64_t PmmNode::CountTotalBytes() const {
    return arena_cumulative_size_;
}

void PmmNode::CountTotalStates(size_t state_count[_VM_PAGE_STATE_COUNT]) const {
    // TODO(MG-833): This is extremely expensive, holding a global lock
    // and touching every page/arena. We should keep a running count instead.
    AutoLock al(&lock_);
    for (auto& a : arena_list_) {
        a.CountStates(state_count);
    }
}

void PmmNode::DumpFree() const TA_NO_THREAD_SAFETY_ANALYSIS {
    auto megabytes_free = CountFreePagesLocked() / 256u;
    printf(" %zu free MBs\n", megabytes_free);
}

// No lock analysis here, as we want to just go for it in the panic case without the lock.
void PmmNode::Dump(bool is_panic) const TA_NO_THREAD_SAFETY_ANALYSIS {
    if (!is_panic) {
        lock_.Acquire();
    }
    for (auto& a : arena_list_) {
        a.Dump(false, false);
    }
    if (!is_panic) {
        lock_.Release();
    }
}

#if PMM_ENABLE_FREE_FILL
void PmmNode::EnforceFill() {
    for (auto& a : arena_list_) {
        a.EnforceFill();
    }
}
#endif
