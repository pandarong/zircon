// Copyright 2017 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT
#pragma once

#include <mxtl/canary.h>
#include <mxtl/intrusive_double_list.h>

#include <kernel/mutex.h>
#include <kernel/vm/pmm.h>

#include "pmm_arena.h"

// per numa node collection of pmm arenas and worker threads
class PmmNode {
public:
    PmmNode();
    ~PmmNode();

    DISALLOW_COPY_ASSIGN_AND_MOVE(PmmNode);

    paddr_t PageToPaddr(const vm_page_t* page) TA_NO_THREAD_SAFETY_ANALYSIS;
    vm_page_t* PaddrToPage(paddr_t addr) TA_NO_THREAD_SAFETY_ANALYSIS;

    status_t AddArena(const pmm_arena_info_t* info);
    vm_page_t* AllocPage(uint alloc_flags, paddr_t* pa);
    size_t AllocPages(size_t count, uint alloc_flags, list_node* list);
    size_t AllocRange(paddr_t address, size_t count, list_node* list);
    size_t AllocContiguous(size_t count, uint alloc_flags, uint8_t alignment_log2, paddr_t* pa, list_node* list);
    size_t Free(list_node* list);

    uint64_t CountFreePages() const;
    uint64_t CountTotalBytes() const;
    void CountTotalStates(size_t state_count[_VM_PAGE_STATE_COUNT]) const;

    // printf free and overall state of the internal arenas
    // NOTE: both functions skip mutexes and can be called inside timer or crash context
    // though the data they return may be questionable
    void DumpFree() const TA_NO_THREAD_SAFETY_ANALYSIS;
    void Dump(bool is_panic) const TA_NO_THREAD_SAFETY_ANALYSIS;

#if PMM_ENABLE_FREE_FILL
    void EnforceFill() TA_NO_THREAD_SAFETY_ANALYSIS;
#endif

private:
    uint64_t CountFreePagesLocked() const TA_REQ(lock_);

    mxtl::Canary<mxtl::magic("PNOD")> canary_;

    mutable Mutex lock_;

    uint64_t arena_cumulative_size_ = 0;

    mxtl::DoublyLinkedList<PmmArena*> arena_list_ TA_GUARDED(lock_);
};

// We don't need to hold the arena lock while executing this, since it is
// only accesses values that are set once during system initialization.
inline paddr_t PmmNode::PageToPaddr(const vm_page_t* page) TA_NO_THREAD_SAFETY_ANALYSIS {
    for (const auto& a : arena_list_) {
        // LTRACEF("testing page %p against arena %p\n", page, &a);
        if (a.page_belongs_to_arena(page)) {
            return a.page_address_from_arena(page);
        }
    }
    return -1;
}

// We don't need to hold the arena lock while executing this, since it is
// only accesses values that are set once during system initialization.
inline vm_page_t* PmmNode::PaddrToPage(paddr_t addr) TA_NO_THREAD_SAFETY_ANALYSIS {
    for (auto& a : arena_list_) {
        if (a.address_in_arena(addr)) {
            size_t index = (addr - a.base()) / PAGE_SIZE;
            return a.get_page(index);
        }
    }
    return nullptr;
}
