// Copyright 2017 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include "second_level_pt.h"

#include <arch/x86/mmu.h>

#include "device_context.h"
#include "iommu_impl.h"

#define LOCAL_TRACE 0

#define SLPT_R               (1u << 0)       /* R    Read        */
#define SLPT_W               (1u << 1)       /* W    Write       */
#define SLPT_X               (1u << 2)       /* X    Execute     */

namespace {

vaddr_t compute_vaddr_mask(PageTableLevel top_level) {
    uint width;
    switch (top_level) {
        case PD_L: width = 30; break;
        case PDP_L: width = 39; break;
        case PML4_L: width = 48; break;
        default: panic("Unsupported iommu width\n");
    }

    // Valid vaddrs for mapping should be page-aligned and not larger than the
    // width of the top level.
    return ((1ull << width) - 1) & ~(PAGE_SIZE - 1);
}

} // namespace

namespace intel_iommu {

SecondLevelPageTable::SecondLevelPageTable(IommuImpl* iommu, DeviceContext* parent)
    : iommu_(iommu),
      parent_(parent),
      needs_flushes_(!iommu->extended_caps()->page_walk_coherency()),
      supports_2mb_(iommu->caps()->supports_second_level_2mb_page()),
      supports_1gb_(iommu->caps()->supports_second_level_1gb_page()),
      initialized_(false) {
}

SecondLevelPageTable::~SecondLevelPageTable() {
    DEBUG_ASSERT(!initialized_);
}

zx_status_t SecondLevelPageTable::Init(PageTableLevel top_level) {
    DEBUG_ASSERT(!initialized_);

    top_level_ = top_level;
    valid_vaddr_mask_ = compute_vaddr_mask(top_level);
    zx_status_t status = X86PageTableBase::Init(nullptr);
    if (status != ZX_OK) {
        return status;
    }
    initialized_ = true;
    return ZX_OK;
}

void SecondLevelPageTable::Destroy() {
    if (!initialized_) {
        return;
    }

    size_t size = valid_vaddr_mask_ + PAGE_SIZE;
    initialized_ = false;
    X86PageTableBase::Destroy(0, size);
}

bool SecondLevelPageTable::allowed_flags(uint flags) {
    constexpr uint kSupportedFlags =
            ARCH_MMU_FLAG_PERM_READ | ARCH_MMU_FLAG_PERM_WRITE | ARCH_MMU_FLAG_PERM_EXECUTE;
    if (flags & ~kSupportedFlags)
        return false;
    return true;
}

// Validation for host physical addresses
bool SecondLevelPageTable::check_paddr(paddr_t paddr) {
    return x86_mmu_check_paddr(paddr);
}

// Validation for virtual physical addresses
bool SecondLevelPageTable::check_vaddr(vaddr_t vaddr) {
    return !(vaddr & ~valid_vaddr_mask_);
}

bool SecondLevelPageTable::supports_page_size(PageTableLevel level) {
    switch (level) {
        case PT_L: return true;
        case PD_L: return supports_2mb_;
        case PDP_L: return supports_1gb_;
        default: return false;
    }
}

X86PageTableBase::IntermediatePtFlags SecondLevelPageTable::intermediate_flags() {
    return SLPT_R | SLPT_W | SLPT_X;
}

X86PageTableBase::PtFlags SecondLevelPageTable::terminal_flags(PageTableLevel level,
                                                               uint flags) {
    X86PageTableBase::PtFlags terminal_flags = 0;

    if (flags & ARCH_MMU_FLAG_PERM_READ)
        terminal_flags |= SLPT_R;

    if (flags & ARCH_MMU_FLAG_PERM_WRITE)
        terminal_flags |= SLPT_W;

    if (flags & ARCH_MMU_FLAG_PERM_EXECUTE)
        terminal_flags |= SLPT_X;

    return terminal_flags;
}

X86PageTableBase::PtFlags SecondLevelPageTable::split_flags(PageTableLevel level,
                                                            PtFlags flags) {
    // We don't need to relocate any flags on split
    return flags;
}

// We disable thread safety analysis here, since the lock being held is being
// held across the MMU operations, but goes through code that is not aware of
// the lock.
void SecondLevelPageTable::TlbInvalidatePage(PageTableLevel level, vaddr_t vaddr, bool global_page,
                                             bool was_terminal) TA_NO_THREAD_SAFETY_ANALYSIS {
    DEBUG_ASSERT(!global_page);
    constexpr uint kBitsPerLevel = 9;
    uint address_mask = kBitsPerLevel * (uint)level;
    if (!was_terminal) {
        // If this is non-terminal, force the paging-structure cache to be
        // cleared for this address still, even though a terminal mapping hasn't
        // been changed.
        // TODO(teisenbe): Not completely sure this is necessary.  Including for
        // now out of caution.
        address_mask = 0;
    }
    iommu_->InvalidateIotlbPageLocked(parent_->domain_id(), vaddr, address_mask);
}

uint SecondLevelPageTable::pt_flags_to_mmu_flags(PtFlags flags, PageTableLevel level) {
    uint mmu_flags = 0;

    if (flags & SLPT_R)
        mmu_flags |= ARCH_MMU_FLAG_PERM_READ;

    if (flags & SLPT_W)
        mmu_flags |= ARCH_MMU_FLAG_PERM_WRITE;

    if (flags & SLPT_X)
        mmu_flags |= ARCH_MMU_FLAG_PERM_EXECUTE;

    return mmu_flags;
}

} // namespace intel_iommu
