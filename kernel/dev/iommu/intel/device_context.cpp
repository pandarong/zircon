// Copyright 2017 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include "device_context.h"

#include <fbl/unique_ptr.h>
#include <trace.h>
#include <vm/vm.h>
#include <zxcpp/new.h>

#include "hw.h"
#include "iommu_impl.h"

#define LOCAL_TRACE 0

namespace intel_iommu {

DeviceContext::DeviceContext(uint8_t bus, uint8_t dev_func, uint32_t domain_id, IommuImpl* parent,
                             volatile ds::ExtendedContextEntry* context_entry)
        : parent_(parent), extended_context_entry_(context_entry), second_level_pt_(parent, this),
          bus_(bus), dev_func_(dev_func), extended_(true),
          domain_id_(domain_id) {
}

DeviceContext::DeviceContext(uint8_t bus, uint8_t dev_func, uint32_t domain_id, IommuImpl* parent,
                             volatile ds::ContextEntry* context_entry)
        : parent_(parent), context_entry_(context_entry), second_level_pt_(parent, this),
          bus_(bus), dev_func_(dev_func), extended_(false),
          domain_id_(domain_id) {
}

DeviceContext::~DeviceContext() {
    bool was_present;
    if (extended_) {
        ds::ExtendedContextEntry entry;
        entry.ReadFrom(extended_context_entry_);
        was_present = entry.present();
        entry.set_present(0);
        entry.WriteTo(extended_context_entry_);
    } else {
        ds::ContextEntry entry;
        entry.ReadFrom(context_entry_);
        was_present = entry.present();
        entry.set_present(0);
        entry.WriteTo(context_entry_);
    }

    if (was_present) {
        // When modifying a present (extended) context entry, we must serially
        // invalidate the context-cache, the PASID-cache, then the IOTLB (see
        // 6.2.2.1 "Context-Entry Programming Considerations" in the VT-d spec,
        // Oct 2014 rev).
        parent_->InvalidateContextCacheDomain(domain_id_);
        // TODO(teisenbe): Invalidate the PASID cache once we support those
        parent_->InvalidateIotlbDomainAll(domain_id_);
    }

    second_level_pt_.Destroy();
}

zx_status_t DeviceContext::Create(uint8_t bus, uint8_t dev_func, uint32_t domain_id, IommuImpl* parent,
                                  volatile ds::ContextEntry* context_entry,
                                  fbl::unique_ptr<DeviceContext>* device) {
    ds::ContextEntry entry;
    entry.ReadFrom(context_entry);

    // It's a bug if we're trying to re-initialize an existing entry
    ASSERT(!entry.present());

    fbl::AllocChecker ac;
    fbl::unique_ptr<DeviceContext> dev(new (&ac) DeviceContext(bus, dev_func, domain_id,
                                                               parent, context_entry));
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    // TODO(teisenbe): don't hardcode PML4_L
    DEBUG_ASSERT(parent->caps()->supports_48_bit_agaw());
    zx_status_t status = dev->second_level_pt_.Init(PML4_L);
    if (status != ZX_OK) {
        return status;
    }

    entry.set_present(1);
    entry.set_fault_processing_disable(0);
    entry.set_translation_type(ds::ContextEntry::kDeviceTlbDisabled);
    // TODO(teisenbe): don't hardcode this
    entry.set_address_width(ds::ContextEntry::k48Bit);
    entry.set_domain_id(domain_id);
    entry.set_second_level_pt_ptr(dev->second_level_pt_.phys() >> 12);

    entry.WriteTo(context_entry);

    *device = fbl::move(dev);
    return ZX_OK;
}

zx_status_t DeviceContext::Create(uint8_t bus, uint8_t dev_func, uint32_t domain_id, IommuImpl* parent,
                                  volatile ds::ExtendedContextEntry* context_entry,
                                  fbl::unique_ptr<DeviceContext>* device) {

    ds::ExtendedContextEntry entry;
    entry.ReadFrom(context_entry);

    // It's a bug if we're trying to re-initialize an existing entry
    ASSERT(!entry.present());

    fbl::AllocChecker ac;
    fbl::unique_ptr<DeviceContext> dev(new (&ac) DeviceContext(bus, dev_func, domain_id,
                                                               parent, context_entry));
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    DEBUG_ASSERT(parent->caps()->supports_48_bit_agaw());
    zx_status_t status = dev->second_level_pt_.Init(PML4_L);
    if (status != ZX_OK) {
        return status;
    }

    entry.set_present(1);
    entry.set_fault_processing_disable(0);
    entry.set_translation_type(ds::ExtendedContextEntry::kHostModeWithDeviceTlbDisabled);
    entry.set_deferred_invld_enable(0);
    entry.set_page_request_enable(0);
    entry.set_nested_translation_enable(0);
    entry.set_pasid_enable(0);
    entry.set_global_page_enable(0);
    // TODO(teisenbe): don't hardcode this
    entry.set_address_width(ds::ExtendedContextEntry::k48Bit);
    entry.set_no_exec_enable(1);
    entry.set_write_protect_enable(1);
    entry.set_cache_disable(0);
    entry.set_extended_mem_type_enable(0);
    entry.set_domain_id(domain_id);
    entry.set_smep_enable(1);
    entry.set_extended_accessed_flag_enable(0);
    entry.set_execute_requests_enable(0);
    entry.set_second_level_execute_bit_enable(0);
    entry.set_second_level_pt_ptr(dev->second_level_pt_.phys() >> 12);

    entry.WriteTo(context_entry);

    *device = fbl::move(dev);
    return ZX_OK;
}

zx_status_t DeviceContext::SecondLevelMap(const fbl::RefPtr<VmObject>& vmo,
                                          uint64_t offset, size_t size, uint32_t perms,
                                          paddr_t* virt_paddr, size_t* mapped_len) {
    DEBUG_ASSERT(IS_PAGE_ALIGNED(offset));
    size_t mapped;
    uint flags = 0;
    if (perms & IOMMU_FLAG_PERM_READ) {
        flags |= ARCH_MMU_FLAG_PERM_READ;
    }
    if (perms & IOMMU_FLAG_PERM_WRITE) {
        flags |= ARCH_MMU_FLAG_PERM_WRITE;
    }
    if (perms & IOMMU_FLAG_PERM_EXECUTE) {
        flags |= ARCH_MMU_FLAG_PERM_EXECUTE;
    }

    auto lookup_fn = [](void* ctx, size_t offset, size_t index, paddr_t pa) {
        paddr_t* paddr = static_cast<paddr_t*>(ctx);
        *paddr = pa;
        return ZX_OK;
    };

    paddr_t paddr = UINT64_MAX;
    zx_status_t status = vmo->Lookup(offset, fbl::min<size_t>(PAGE_SIZE, size), 0, lookup_fn,
                                     &paddr);
    if (status != ZX_OK) {
        return status;
    }
    if (paddr == UINT64_MAX) {
        return ZX_ERR_BAD_STATE;
    }

    size_t map_len;
    if (vmo->is_paged()) {
        map_len = 1;
    } else {
        map_len = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
    }

    // TODO(teisenbe): Instead of doing direct mapping, remap to form contiguous
    // ranges, and handle more than one page at a time in here.
    status = second_level_pt_.MapPagesContiguous(paddr, paddr, map_len, flags,
                                                 &mapped);
    if (status != ZX_OK) {
        return status;
    }
    ASSERT(mapped == map_len);

    *virt_paddr = paddr;
    *mapped_len = map_len * PAGE_SIZE;

    LTRACEF("Map(%02x:%02x.%1x): [%p, %p) -> %p %#x\n", bus_, (unsigned int)dev_func_ >> 3,
            (unsigned int)dev_func_ & 0x7, (void*)paddr, (void*)(paddr + PAGE_SIZE), (void*)paddr,
            flags);
    return ZX_OK;
}

zx_status_t DeviceContext::SecondLevelUnmap(paddr_t virt_paddr, size_t size) {
    DEBUG_ASSERT(IS_PAGE_ALIGNED(virt_paddr));
    DEBUG_ASSERT(IS_PAGE_ALIGNED(size));
    size_t unmapped;
    LTRACEF("Unmap(%02x:%02x.%1x): [%p, %p)\n", bus_, (unsigned int)dev_func_ >> 3,
            (unsigned int)dev_func_ & 0x7, (void*)virt_paddr, (void*)(virt_paddr + size));
    return second_level_pt_.UnmapPages(virt_paddr, size / PAGE_SIZE, &unmapped);
}

} // namespace intel_iommu
