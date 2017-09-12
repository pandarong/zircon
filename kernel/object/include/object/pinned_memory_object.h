// Copyright 2017 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#pragma once

#include <dev/iommu.h>
#include <err.h>
#include <fbl/canary.h>
#include <fbl/intrusive_double_list.h>
#include <fbl/unique_ptr.h>

#include <sys/types.h>

class BusTransactionInitiatorDispatcher;
class VmObject;

class PinnedMemoryObject final : public fbl::DoublyLinkedListable<fbl::unique_ptr<PinnedMemoryObject>> {
public:
    // Pin memory in |vmo|'s range [offset, offset+size) on behalf of |bti|,
    // with permissions specified by |perms|.  |perms| should be flags suitable
    // for the Iommu::Map() interface.
    static zx_status_t Create(const BusTransactionInitiatorDispatcher& bti,
                              fbl::RefPtr<VmObject> vmo, size_t offset,
                              size_t size, uint32_t perms,
                              fbl::unique_ptr<PinnedMemoryObject>* out);
    ~PinnedMemoryObject();

    class Extent {
     public:
        // Implicit conversion to uint64_t
        operator uint64_t() const { return val_; }

        Extent() : val_(0) { }
        Extent(uint64_t base, size_t pages) : val_(base | (pages - 1)) {
            ASSERT(pages > 0 && pages <= PAGE_SIZE);
        }
        uint64_t base() const { return val_ & ~(PAGE_SIZE - 1); }
        size_t pages() const { return (val_ & (PAGE_SIZE - 1)) + 1; }
        zx_status_t extend(size_t num_pages) {
            if (pages() + num_pages > PAGE_SIZE) return ZX_ERR_OUT_OF_RANGE;
            val_ += num_pages;
            return ZX_OK;
        }
     private:
        uint64_t val_;
    };

    // Returns an array of the extents usable by the given device
    const fbl::unique_ptr<Extent[]>& mapped_extents() const { return mapped_extents_; }
    size_t mapped_extents_len() const { return mapped_extents_len_; }
private:
    PinnedMemoryObject(const BusTransactionInitiatorDispatcher& bti,
                       fbl::RefPtr<VmObject> vmo, size_t offset, size_t size,
                       bool is_contiguous,
                       fbl::unique_ptr<Extent[]> mapped_extents);
    DISALLOW_COPY_ASSIGN_AND_MOVE(PinnedMemoryObject);

    zx_status_t MapIntoIommu(uint32_t perms);
    zx_status_t UnmapFromIommu();

    fbl::Canary<fbl::magic("PMO_")> canary_;

    const fbl::RefPtr<VmObject> vmo_;
    const uint64_t offset_;
    const uint64_t size_;
    const bool is_contiguous_;

    const BusTransactionInitiatorDispatcher& bti_;
    const fbl::unique_ptr<Extent[]> mapped_extents_;
    size_t mapped_extents_len_;
};
