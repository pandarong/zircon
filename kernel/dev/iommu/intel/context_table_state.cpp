// Copyright 2017 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include "context_table_state.h"

#include <zxcpp/new.h>
#include <fbl/unique_ptr.h>

#include "device_context.h"
#include "hw.h"
#include "iommu_impl.h"

namespace intel_iommu {

ContextTableState::ContextTableState(uint8_t bus, bool extended, bool upper,
                                     IommuImpl* parent, volatile ds::RootEntrySubentry* root_entry,
                                     IommuPage page)
        : parent_(parent), root_entry_(root_entry), page_(fbl::move(page)),
          bus_(bus), extended_(extended), upper_(upper) {
}

ContextTableState::~ContextTableState() {
    ds::RootEntrySubentry entry;
    entry.ReadFrom(root_entry_);
    entry.set_present(0);
    entry.WriteTo(root_entry_);

    // When modifying a present (extended) root entry, we must serially
    // invalidate the context-cache, the PASID-cache, then the IOTLB (see
    // 6.2.2.1 "Context-Entry Programming Considerations" in the VT-d spec,
    // Oct 2014 rev).
    parent_->InvalidateContextCacheGlobal();
    // TODO(teisenbe): Invalidate the PASID cache once we support those
    parent_->InvalidateIotlbGlobal();
}

zx_status_t ContextTableState::Create(uint8_t bus, bool extended, bool upper,
                                      IommuImpl* parent, volatile ds::RootEntrySubentry* root_entry,
                                      fbl::unique_ptr<ContextTableState>* table) {
    ds::RootEntrySubentry entry;
    entry.ReadFrom(root_entry);
    DEBUG_ASSERT(!entry.present());

    IommuPage page;
    zx_status_t status = IommuPage::AllocatePage(&page);
    if (status != ZX_OK) {
        return status;
    }

    fbl::AllocChecker ac;
    fbl::unique_ptr<ContextTableState> tbl(new (&ac) ContextTableState(bus, extended, upper,
                                                                       parent, root_entry,
                                                                       fbl::move(page)));
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    entry.set_present(1);
    entry.set_context_table(tbl->page_.paddr() >> 12);
    entry.WriteTo(root_entry);

    *table = fbl::move(tbl);
    return ZX_OK;
}

zx_status_t ContextTableState::CreateDeviceContext(uint8_t bus, uint8_t dev_func, uint32_t domain_id,
                                                   DeviceContext** context) {
    fbl::unique_ptr<DeviceContext> dev;
    zx_status_t status;
    if (extended_) {
        volatile ds::ExtendedContextTable* tbl = extended_table();
        volatile ds::ExtendedContextEntry* entry = &tbl->entry[dev_func & 0x7f];
        status = DeviceContext::Create(bus, dev_func, domain_id, parent_, entry, &dev);
    } else {
        volatile ds::ContextTable* tbl = table();
        volatile ds::ContextEntry* entry = &tbl->entry[dev_func];
        status = DeviceContext::Create(bus, dev_func, domain_id, parent_, entry, &dev);
    }
    if (status != ZX_OK) {
        return status;
    }

    *context = dev.get();
    devices_.push_back(fbl::move(dev));
    return ZX_OK;
}

zx_status_t ContextTableState::GetDeviceContext(uint8_t bus, uint8_t dev_func,
                                                DeviceContext** context) {
    for (auto& dev : devices_) {
        if (dev.is_bdf(bus, dev_func)) {
            *context = &dev;
            return ZX_OK;
        }
    }
    return ZX_ERR_NOT_FOUND;
}

} // namespace intel_iommu
