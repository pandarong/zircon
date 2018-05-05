// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#pragma once

#include <fbl/array.h>
#include <inttypes.h>
#include <kernel/auto_lock.h>
#include <kernel/spinlock.h>
#include <region-alloc/region-alloc.h>

using fbl::AutoLock;
using RegionUPtr = RegionAllocator::Region::UPtr;

class PhysicalAspaceManager : public fbl::RefCounted<PhysicalAspaceManager> {
public:
    using Allocation = RegionAllocator::Region::UPtr;
    // These values map to allocator ids.
    enum {
        kMmioAllocator = 0u,
        kIoAllocator,
        kIrqAllocator,
        kAllocatorCount,
    };

    struct EarlyRegion {
        uint32_t id;
        uintptr_t base;
        size_t size;
    };

    // The max early region entries we allow to be allocated before Initialize()
    // is called and they are moved to RegionAllocators.
    static constexpr size_t kEarlyRegionMaxCnt = 64;
    // The maximum number of bytes the backing region pool for the allocators can
    // expand to. In general use we should not approach this.
    static constexpr size_t kMaxRegionPoolSize = 64 << 10;

    // Allocates the singleton class instance used for the kernel bookkeeping. Tests
    // will have access to the standard constructor via a test friend class.
    //
    // Possible error values:
    // ++ ZX_ERR_ALREADY_EXISTS : The object has already been created (Create() was already called).
    // ++ ZX_ERR_NO_MEMORY      : The class allocation failed.
    static zx_status_t Create();

    // Returns a reference to the global kernel bookkeeping structure for PASM.
    static const fbl::unique_ptr<PhysicalAspaceManager>& Get() {
        return instance_;
    }

    bool is_initialized() {
        AutoSpinLock lock(&lock_);
        return initialized_;
    }
    // Request a region of address space in the  |region_id| allocator starting
    // at |base| of length |size| bytes.  This allocates
    //
    // ** No checking of overlap or exclusive allocation is done at this stage.
    //
    // Possible error values:
    // ++ ZX_ERR_NO_MEMORY : There is no space left for entries in the early page.
    zx_status_t ReserveAddressSpaceEarly(uint32_t region_id, uintptr_t base, size_t size);

    // Sets up allocators for MMIO, IO, and IRQ regions and adds initial memory
    // space allocations starting at |*_base| of |*_size| bytes for each. Regions
    // with a size of zero are ignored. This must be called before any usage of
    // AddAddressSpace() and ReserverAddressSpace().
    //
    // Possible error values:
    // ++ ZX_ERR_NO_MEMORY : Region pool allocations failed.
    // ++ ZX_ERR_BAD_STATE : Initialize() has already been called.
    zx_status_t Initialize(uintptr_t mmio_base, size_t mmio_size, uintptr_t io_base, size_t io_size,
                           uintptr_t irq_base, size_t irq_size);

    // Provides a way to add address space to an allocator specified by |region_id|. A range from
    // address |base| up to |base| + |size| bytes is added to the allocator. Library users are not
    // expected to call these directly, they are typically going to be used by Create() and tests.
    //
    // Possible error values:
    // ++ ZX_ERR_NO_MEMORY : Region pool allocations failed.
    // ++ ZX_ERR_BAD_STATE : Initialize() not been called.
    // ++ ZX_ERR_INVALID_ARGS : |region_id|, |base|, |size|, or |base| + |size|, are invalid,
    // or the specified range of address space intersects with space already in the allocator
    // specified by |region_id|.
    zx_status_t AddAddressSpace(uint32_t region_id, uintptr_t base, size_t size) TA_EXCL(lock_);
    zx_status_t AddAddressSpaceLocked(uint32_t region_id, uintptr_t base, size_t size)
        TA_REQ(lock_);

    // Exclusively pulls a region starting at |base| of size |size| bytes from the allocator for
    // region |region_id|. A unique pointer for the region is returned in out_region.
    //
    // Possible error values:
    // ++ ZX_ERR_INVALID_ARGS : The region_id is invalid.
    // ++ ZX_ERR_NO_MEMORY : The region pool's backing struct cannot be allocated.
    // ++ ZX_ERR_BAD_STATE : Initialize() has not been called.
    zx_status_t ReserveAddressSpace(uint32_t region_id, uintptr_t base, size_t size,
                                    RegionUPtr& out_region) TA_EXCL(lock_);
    zx_status_t ReserveAddressSpaceLocked(uint32_t region_id, uintptr_t base,
                                          size_t size, RegionUPtr& out_region) TA_REQ(lock_);

    // Prints the allocated regions to the kernel log.
    // This can be called after InitializeEarly() and Initialize() and  will
    // work in both situations.  The variants take a region id or a string
    // corresponding to "MMIO", "IO", or "IRQ" as filters.
    void Dump(uint32_t region_id);
    void Dump(const char* str);
    void Dump();

    DISALLOW_COPY_ASSIGN_AND_MOVE(PhysicalAspaceManager);

private:
    PhysicalAspaceManager();

    // The order here is important. dtors will be handled in reverse order to their
    // definitions here, which is important because the early_region_uptrs_ array
    // can hold references to regions from the regions_ allocators. If those are
    // still held when the RegionAllocator dtors are called we will rightfully assert
    // because regions are outstanding. To prevent this, early_region_uptrs_ comes
    // second so it is cleaned up before regions_
    SpinLock lock_;
    RegionAllocator regions_[PhysicalAspaceManager::kAllocatorCount] TA_GUARDED(lock_);
    fbl::Array<PhysicalAspaceManager::Allocation> early_region_uptrs_ TA_GUARDED(lock_);
    size_t early_region_cnt_ TA_GUARDED(lock_) = 0;
    EarlyRegion early_regions_[kEarlyRegionMaxCnt] TA_GUARDED(lock_);
    bool initialized_ TA_GUARDED(lock_) = false;

    // kernel singleton instance
    static fbl::unique_ptr<PhysicalAspaceManager> instance_;
    friend class PhysicalAspaceManagerTest;
};

// Class to create PhysicalAspaceManager objects for testing purposes
class PhysicalAspaceManagerTest {
public:
    static fbl::RefPtr<PhysicalAspaceManager> CreateTestInstance();
};

static_assert((sizeof(PhysicalAspaceManager) < PAGE_SIZE), "PASM has grown beyond a single page!");
