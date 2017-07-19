// Copyright 2016 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT
#pragma once

#include <mxtl/canary.h>
#include <mxtl/intrusive_double_list.h>
#include <mxtl/macros.h>

#include <kernel/vm/pmm.h>
#include <trace.h>

class PmmNode;

class PmmArena : public mxtl::DoublyLinkedListable<PmmArena*> {
public:
    PmmArena(const pmm_arena_info_t* info);
    ~PmmArena();

    DISALLOW_COPY_ASSIGN_AND_MOVE(PmmArena);

    // set up the per page structures, allocated out of the boot time allocator
    // link them to the free list in the PmmNode
    void BootAllocArray(PmmNode *);

    // accessors
    const pmm_arena_info_t& info() const { return info_; }
    const char* name() const { return info_.name; }
    paddr_t base() const { return info_.base; }
    size_t size() const { return info_.size; }
    unsigned int flags() const { return info_.flags; }
    unsigned int priority() const { return info_.priority; }

    // Counts the number of pages in every state. For each page in the arena,
    // increments the corresponding VM_PAGE_STATE_*-indexed entry of
    // |state_count|. Does not zero out the entries first.
    void CountStates(size_t state_count[_VM_PAGE_STATE_COUNT]) const;

    vm_page* get_page(size_t index) { return &page_array_[index]; }

    // find a free run of contiguous pages
    vm_page* FindFreeContiguous(size_t count, uint8_t alignment_log2);

    // return a pointer to a specific page
    vm_page* FindSpecific(paddr_t pa);

    // helpers
    bool page_belongs_to_arena(const vm_page* page) const {
        return (page->paddr >= base() && page->paddr < (base() + size()));
    }

    bool address_in_arena(paddr_t address) const {
        return (address >= info_.base && address <= info_.base + info_.size - 1);
    }

    void Dump(bool dump_pages, bool dump_free_ranges) const;

private:
    mxtl::Canary<mxtl::magic("PARN")> canary_;

    const pmm_arena_info_t info_;
    vm_page* page_array_ = nullptr;
};
