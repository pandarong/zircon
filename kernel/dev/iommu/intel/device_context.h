// Copyright 2017 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#pragma once

#include <fbl/intrusive_double_list.h>
#include <fbl/macros.h>
#include <fbl/unique_ptr.h>
#include <vm/vm_object.h>

#include "hw.h"
#include "second_level_pt.h"

namespace intel_iommu {

class IommuImpl;

class DeviceContext : public fbl::DoublyLinkedListable<fbl::unique_ptr<DeviceContext>> {
public:
    ~DeviceContext();

    // Create a new DeviceContext representing the given BDF.  It is a fatal error
    // to try to create a context for a BDF that already has one.
    static zx_status_t Create(uint8_t bus, uint8_t dev_func, uint32_t domain_id, IommuImpl* parent,
                              volatile ds::ExtendedContextEntry* context_entry,
                              fbl::unique_ptr<DeviceContext>* device);
    static zx_status_t Create(uint8_t bus, uint8_t dev_func, uint32_t domain_id, IommuImpl* parent,
                              volatile ds::ContextEntry* context_entry,
                              fbl::unique_ptr<DeviceContext>* device);

    // Check if this DeviceContext is for the given BDF
    bool is_bdf(uint8_t bus, uint8_t dev_func) const {
        return bus == bus_ && dev_func == dev_func_;
    }

    uint32_t domain_id() const { return domain_id_; }

    // Use the second-level translation table to map the host pages in the given
    // range on |vmo| to the guest's address |*virt_paddr|.  |size| is in bytes.
    // |mapped_len| may be larger than |size|, if |size| was not page-aligned.
    zx_status_t SecondLevelMap(const fbl::RefPtr<VmObject>& vmo,
                               uint64_t offset, size_t size, uint32_t perms,
                               paddr_t* virt_paddr, size_t* mapped_len);
    zx_status_t SecondLevelUnmap(paddr_t virt_paddr, size_t size);

private:
    DeviceContext(uint8_t bus, uint8_t dev_func, uint32_t domain_id, IommuImpl* parent,
                  volatile ds::ExtendedContextEntry* context_entry);
    DeviceContext(uint8_t bus, uint8_t dev_func, uint32_t domain_id, IommuImpl* parent,
                  volatile ds::ContextEntry* context_entry);

    DISALLOW_COPY_ASSIGN_AND_MOVE(DeviceContext);

    IommuImpl* const parent_;
    union {
        volatile ds::ExtendedContextEntry* const extended_context_entry_;
        volatile ds::ContextEntry* const context_entry_;
    };

    // Page tables used for translating requests-without-PASID and for nested
    // translation of requests-with-PASID.
    SecondLevelPageTable second_level_pt_;

    const uint8_t bus_;
    const uint8_t dev_func_;
    const bool extended_;
    const uint32_t domain_id_;
};

} // namespace intel_iommu
