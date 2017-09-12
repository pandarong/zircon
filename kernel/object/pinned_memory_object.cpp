// Copyright 2017 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT


#include <object/pinned_memory_object.h>

#include <assert.h>
#include <err.h>
#include <vm/vm.h>
#include <vm/vm_object.h>
#include <zxcpp/new.h>
#include <fbl/algorithm.h>
#include <fbl/auto_call.h>
#include <fbl/auto_lock.h>
#include <object/bus_transaction_initiator_dispatcher.h>
#include <trace.h>

#define LOCAL_TRACE 0

zx_status_t PinnedMemoryObject::Create(const BusTransactionInitiatorDispatcher& bti,
                                       fbl::RefPtr<VmObject> vmo, size_t offset,
                                       size_t size, uint32_t perms,
                                       fbl::unique_ptr<PinnedMemoryObject>* out) {
    LTRACE_ENTRY;
    DEBUG_ASSERT(IS_PAGE_ALIGNED(offset));

    // Pin the memory to make sure it doesn't change from underneath us for the
    // lifetime of the created PMO.
    zx_status_t status = vmo->Pin(offset, size);
    if (status != ZX_OK) {
        LTRACEF("vmo->Pin failed: %d\n", status);
        return status;
    }

    uint64_t expected_addr = 0;
    auto check_contiguous = [](void* context, size_t offset, size_t index, paddr_t pa) {
        auto expected_addr = static_cast<uint64_t*>(context);
        if (index != 0 && pa != *expected_addr) {
            return ZX_ERR_NOT_FOUND;
        }
        *expected_addr = pa + PAGE_SIZE;
        return ZX_OK;
    };
    status = vmo->Lookup(offset, size, 0, check_contiguous, &expected_addr);
    bool is_contiguous = (status == ZX_OK);

    // Set up a cleanup function to undo the pin if we need to fail this
    // operation.
    auto unpin_vmo = fbl::MakeAutoCall([vmo, offset, size]() {
        vmo->Unpin(offset, size);
    });

    // TODO(teisenbe): Be more intelligent about allocating this, since if this
    // is backed by a real IOMMU, we will likely compress the page array greatly
    // using extents.
    fbl::AllocChecker ac;
    const size_t num_pages = is_contiguous ? 1 : ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
    fbl::unique_ptr<Extent[]> page_array(new (&ac) Extent[num_pages]);
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    fbl::unique_ptr<PinnedMemoryObject> pmo(
            new (&ac) PinnedMemoryObject(bti, fbl::move(vmo), offset, size, is_contiguous,
                                         fbl::move(page_array)));
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    // Now that the pmo object has been created, it is responsible for
    // unpinning.
    unpin_vmo.cancel();

    status = pmo->MapIntoIommu(perms);
    if (status != ZX_OK) {
        LTRACEF("MapIntoIommu failed: %d\n", status);
        return status;
    }

    *out = fbl::move(pmo);
    return ZX_OK;
}

// Used during initialization to set up the IOMMU state for this PMO.
zx_status_t PinnedMemoryObject::MapIntoIommu(uint32_t perms) {
    if (is_contiguous_) {
        dev_vaddr_t vaddr_base = 1;
        // Usermode drivers assume that if they requested a contiguous buffer in
        // memory, then they will only get a single address back.  Return an
        // error if we can't acutally map the address contiguously.
        size_t remaining = size_;
        size_t curr_offset = offset_;
        while (true) {
            dev_vaddr_t vaddr;
            size_t mapped_len;
            zx_status_t status = bti_.iommu()->Map(bti_.bti_id(), vmo_, curr_offset, remaining, perms,
                                                   &vaddr, &mapped_len);
            if (status != ZX_OK) {
                if (vaddr_base != 1) {
                    bti_.iommu()->Unmap(bti_.bti_id(), vaddr_base, curr_offset - offset_);
                }
                return status;
            }
            if (vaddr_base == 1) {
                vaddr_base = vaddr;
            } else if (vaddr != vaddr_base + curr_offset) {
                bti_.iommu()->Unmap(bti_.bti_id(), vaddr_base, curr_offset - offset_);
                bti_.iommu()->Unmap(bti_.bti_id(), vaddr, mapped_len);
                return ZX_ERR_INTERNAL;
            }

            // Note that |mapped_len| could actually be larger than |remaining|
            // due to VMOs not requiring page-aligned sizes.
            if (remaining <= mapped_len) {
                break;
            }
            curr_offset += mapped_len;
            remaining -= mapped_len;
        }

        mapped_extents_[0] = PinnedMemoryObject::Extent(vaddr_base, 1);
        mapped_extents_len_ = 1;
        return ZX_OK;
    }

    size_t remaining = size_;
    uint64_t curr_offset = offset_;
    while (true) {
        dev_vaddr_t vaddr;
        size_t mapped_len;
        zx_status_t status = bti_.iommu()->Map(bti_.bti_id(), vmo_, curr_offset, remaining, perms,
                                               &vaddr, &mapped_len);
        if (status != ZX_OK) {
            zx_status_t err = UnmapFromIommu();
            ASSERT(err == ZX_OK);
            return status;
        }

        // Break up extents if necessary (since extents pack size in the
        // low bits of the base address, we can only support up to PAGE_SIZE
        // pages per extent).
        size_t mapped_pages = mapped_len / PAGE_SIZE;
        while (mapped_pages > 0) {
            size_t extent_pages = fbl::min<size_t>(mapped_pages, PAGE_SIZE);
            mapped_extents_[mapped_extents_len_] = Extent(vaddr, extent_pages);
            mapped_extents_len_++;
            mapped_pages -= extent_pages;
        }

        if (remaining <= mapped_len) {
            break;
        }
        curr_offset += mapped_len;
        remaining -= mapped_len;
    }

    return ZX_OK;
}

zx_status_t PinnedMemoryObject::UnmapFromIommu() {
    auto iommu = bti_.iommu();
    const uint64_t bus_txn_id = bti_.bti_id();

    if (unlikely(mapped_extents_len_ == 0)) {
        return ZX_OK;
    }

    zx_status_t status = ZX_OK;
    if (is_contiguous_) {
        status = iommu->Unmap(bus_txn_id, mapped_extents_[0].base(), ROUNDUP(size_, PAGE_SIZE));
    } else {
        for (size_t i = 0; i < mapped_extents_len_; ++i) {
            // Try to unmap all pages even if we get an error, and return the
            // first error encountered.
            zx_status_t err = iommu->Unmap(bus_txn_id, mapped_extents_[i].base(),
                                        mapped_extents_[i].pages() * PAGE_SIZE);
            DEBUG_ASSERT(err == ZX_OK);
            if (err != ZX_OK && status == ZX_OK) {
                status = err;
            }
        }
    }

    // Clear this so we won't try again if this gets called again in the
    // destructor.
    mapped_extents_len_ = 0;
    return status;
}

PinnedMemoryObject::~PinnedMemoryObject() {
    zx_status_t status = UnmapFromIommu();
    ASSERT(status == ZX_OK);
    vmo_->Unpin(offset_, size_);
}

PinnedMemoryObject::PinnedMemoryObject(const BusTransactionInitiatorDispatcher& bti,
                                       fbl::RefPtr<VmObject> vmo, size_t offset, size_t size,
                                       bool is_contiguous,
                                       fbl::unique_ptr<Extent[]> mapped_extents)
    : vmo_(fbl::move(vmo)), offset_(offset), size_(size), is_contiguous_(is_contiguous), bti_(bti),
      mapped_extents_(fbl::move(mapped_extents)), mapped_extents_len_(0) {
}
