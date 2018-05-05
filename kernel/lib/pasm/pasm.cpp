// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <err.h>
#include <fbl/mutex.h>
#include <inttypes.h>
#include <kernel/auto_lock.h>
#include <kernel/mutex.h>
#include <lib/console.h>
#include <lib/pasm/pasm.h>
#include <region-alloc/region-alloc.h>
#include <string.h>
#include <trace.h>

#define LOCAL_TRACE 0
static const char* kLogTag = "pasm: ";

namespace {
constexpr const char* region_id_to_str(int region_id) {
    switch (region_id) {
    case PhysicalAspaceManager::kMmioAllocator:
        return "mmio";
    case PhysicalAspaceManager::kIoAllocator:
        return "io";
    case PhysicalAspaceManager::kIrqAllocator:
        return "irq";
    default:
        return "{unknown}";
    }
}
}

// storage for class static members
fbl::unique_ptr<PhysicalAspaceManager> PhysicalAspaceManager::instance_;
// Backing memory for the kernel object allocated by PhysicalAspaceManager::Create()
alignas(PhysicalAspaceManager) static uint8_t instance_buf[sizeof(PhysicalAspaceManager)];

// The PhysicalAspaceManager class is designed to exist as a singleton for use by the kernel in
// tracking physical address space. For that reason the constructor is private
// and creation is done using this Create methods (except for tests, see
// PhysicalAspaceManagerTest). Since the Pasm needs to track reserved memory arenas before the
// pmm arenas exist, we cannot rely on asking the pmm for a backing page to use
// with the placement new nor can we use the heap because it isn't initialized
// until later in boot. For this reason the singleton is created using the
// backing BSS buffer allocated above.
zx_status_t PhysicalAspaceManager::Create() {
    if (instance_ != nullptr) {
        return ZX_ERR_ALREADY_EXISTS;
    }

    instance_.reset(new (instance_buf) PhysicalAspaceManager());
    LTRACEF("%s kernel singleton created.\n", kLogTag);
    return ZX_OK;
}

zx_status_t PhysicalAspaceManager::ReserveAddressSpaceEarly(uint32_t region_id, uintptr_t base, size_t size) {
    AutoSpinLock lock(&lock_);
    if (early_region_cnt_ == kEarlyRegionMaxCnt) {
        return ZX_ERR_NO_MEMORY;
    }

    auto& region = early_regions_[early_region_cnt_];
    region.id = region_id;
    region.base = base;
    region.size = size;
    early_region_cnt_++;

    return ZX_OK;
}

zx_status_t PhysicalAspaceManager::AddAddressSpace(uint32_t region_id, uintptr_t base, size_t size) {
    AutoSpinLock lock(&lock_);
    return AddAddressSpaceLocked(region_id, base, size);
}

zx_status_t PhysicalAspaceManager::AddAddressSpaceLocked(uint32_t region_id, uintptr_t base, size_t size) {
    if (region_id > PhysicalAspaceManager::kAllocatorCount) {
        return ZX_ERR_INVALID_ARGS;
    }

    // A size of zero is an invalid arg situation for AddRegion, but it's convenient for the
    // Initialize() to allow it here and return early.
    if (size == 0) {
        return ZX_OK;
    }

    zx_status_t st = regions_[region_id].AddRegion({.base = base, .size = size}, false);
    LTRACEF("%s reserving { base %#" PRIxPTR ", size %#lx }: %d\n", kLogTag, base, size, st);
    return st;
}

// Goes through early allocations
zx_status_t PhysicalAspaceManager::Initialize(uintptr_t mmio_base, size_t mmio_size,
                             uintptr_t io_base, size_t io_size,
                             uintptr_t irq_base, size_t irq_size) {
    AutoSpinLock lock(&lock_);
    zx_status_t st;

    // Create a backing pool and assign it to each of the allocators
    auto backing_pool = RegionAllocator::RegionPool::Create(PhysicalAspaceManager::kMaxRegionPoolSize);
    if (backing_pool == nullptr) {
        return ZX_ERR_NO_MEMORY;
    }

    for (int region_id = 0; region_id < PhysicalAspaceManager::kAllocatorCount; region_id++) {
        st = regions_[region_id].SetRegionPool(backing_pool);
        if (st != ZX_OK) {
            return st;
        }
    }

    // Add the initial address space regions to the allocators. The log messages of these
    // are output in all cases because they represent major system configuration errors.
    st = AddAddressSpaceLocked(PhysicalAspaceManager::kMmioAllocator, mmio_base, mmio_size);
    if (st != ZX_OK) {
        printf("pasm: couldn't add {%" PRIxPTR ", %#lx} to MMIO region: %d\n",
               mmio_base, mmio_size, st);
    }

    st = AddAddressSpaceLocked(PhysicalAspaceManager::kIoAllocator, io_base, io_size);
    if (st != ZX_OK) {
        printf("pasm: couldn't add {%" PRIxPTR ", %#lx} to IO region: %d\n", io_base, io_size, st);
    }

    st = AddAddressSpaceLocked(PhysicalAspaceManager::kIrqAllocator, irq_base, irq_size);
    if (st != ZX_OK) {
        printf("pasm: couldn't add {%" PRIxPTR ", %#lx} to IRQ region: %d\n", irq_base,
               irq_size, st);
    }

    // If early boot allocations were made then convert the entries and insert
    // them into the proper allocators.
    if (early_region_cnt_ > 0) {
        fbl::AllocChecker ac;

        // To handle the lifecycle of the overall PhysicalAspaceManager object we need to hold references
        // to the early regions we pull out so they can be cleaned up before the library's
        // bookkeeping RegionAllocator objects are torn down.
        early_region_uptrs_.reset(new (&ac) Allocation[early_region_cnt_](), early_region_cnt_);
        if (!ac.check()) {
            return ZX_ERR_NO_MEMORY;
        }

        // Walk all the early init regions that were reserved and subtract them
        // from their respective region allocators.
        for (uint i = 0; i < early_region_cnt_; i++) {
            auto& region = early_regions_[i];
            RegionAllocator::Region::UPtr uptr;
            // Being unable to add these regions is a boot configuration error
            // that likely should not break the system, but it should be
            // recorded for investigation.
            zx_status_t status = ReserveAddressSpaceLocked(region.id, region.base, region.size,
                                                           early_region_uptrs_[i]);
            if (status != ZX_OK) {
                printf("pasm: failed to add early region (id %u, base %" PRIxPTR
                       ", size %lu) to PASM: %d!\n",
                       region.id, region.base, region.size, status);
            }

            // We need to pull early regions out of the Region Allocator, but in
            // doing so handle lifecycle becomes important. For these early entries
            // we will release the handles so the regions stay alive.
        }
    }

    initialized_ = true;
    return ZX_OK;
}

zx_status_t PhysicalAspaceManager::ReserveAddressSpace(uint32_t region_id, uintptr_t base, size_t size,
                                      RegionUPtr& out_region) {
    AutoSpinLock lock(&lock_);
    return ReserveAddressSpaceLocked(region_id, base, size, out_region);
}

zx_status_t PhysicalAspaceManager::ReserveAddressSpaceLocked(uint32_t region_id, uintptr_t base, size_t size,
                                            RegionUPtr& out_region) {
    if (region_id >= PhysicalAspaceManager::kAllocatorCount) {
        return ZX_ERR_INVALID_ARGS;
    }

    zx_status_t st = regions_[region_id].GetRegion({.base = base, .size = size}, out_region);
    LTRACEF("%s reserving { base %#" PRIxPTR ", size %#lx }: %d\n", kLogTag, base, size, st);
    return st;
}

PhysicalAspaceManager::PhysicalAspaceManager() {
}

void PhysicalAspaceManager::Dump(uint32_t print_id) {
    AutoSpinLock lock(&lock_);
    for (size_t i = 0; i < early_region_cnt_; i++) {
        if (likely(print_id == PhysicalAspaceManager::kAllocatorCount) || early_regions_[i].id == print_id) {
            printf("%5s { .base = %#16" PRIxPTR ", .size = %#16lx }\n",
                   region_id_to_str(early_regions_[i].id), early_regions_[i].base,
                   early_regions_[i].size);
        }
    }
    printf("reservations:\n");
    uint32_t region_id;
    auto f = [](const ralloc_region_t* r) {
        printf("      { .base = %#16" PRIxPTR ", .size = %#16lx }\n", r->base, r->size);
        return true;
    };

    for (region_id = 0; region_id < PhysicalAspaceManager::kAllocatorCount; region_id++) {
        if (likely(print_id == PhysicalAspaceManager::kAllocatorCount) || region_id == print_id) {
            printf("%s: \n", region_id_to_str(region_id));
            regions_[region_id].WalkAllocatedRegions(f);
        }
    }
}

void PhysicalAspaceManager::Dump(const char* region_str) {
    if (!strncmp(region_str, "MMIO", 4)) {
        Dump(kMmioAllocator);
    } else if (!strncmp(region_str, "IO", 2)) {
        Dump(kIoAllocator);
    } else if (!strncmp(region_str, "IRQ", 3)) {
        Dump(kIrqAllocator);
    } else {
        printf("Unknown region '%s'...\n", region_str);
    }
}

void PhysicalAspaceManager::Dump() {
    Dump(PhysicalAspaceManager::kAllocatorCount);
}

fbl::RefPtr<PhysicalAspaceManager> PhysicalAspaceManagerTest::CreateTestInstance() {
    fbl::AllocChecker ac;
    auto rptr = fbl::AdoptRef(new (&ac) PhysicalAspaceManager());
    if (!ac.check()) {
        return nullptr;
    }

    return rptr;
}

#ifdef WITH_LIB_CONSOLE
static int print_help(const char* name) {
    printf("Print outstanding physical address space allocations.\n");
    printf("usage:\n");
    printf("\t%s MMIO\n", name);
    printf("\t%s IO\n", name);
    printf("\t%s IRQ\n", name);
    printf("tags can also be combined:\n");
    printf("\tex: %s MMIO IRQ\n", name);
    return 0;
}

static int cmd_pasm(int argc, const cmd_args* argv, uint32_t flags) {
    if (argc > 1 && (!strncmp(argv[1].str, "help", 4) || !strncmp(argv[1].str, "-h", 2))) {
        return print_help(argv[0].str);
    }

    if (!PhysicalAspaceManager::Get()) {
        printf("No PhysicalAspaceManager instance exists!\n");
        return -1;
    }

    // No args passed
    if (argc == 1) {
        PhysicalAspaceManager::Get()->Dump(PhysicalAspaceManager::kAllocatorCount);
    } else {
        // walk through all tags provided
        for (int i = 1; i < argc; i++) {
            PhysicalAspaceManager::Get()->Dump(argv[i].str);
        }
    }

    return true;
}

STATIC_COMMAND_START
STATIC_COMMAND("pasm", "Inspect address space allocations", &cmd_pasm)
STATIC_COMMAND_END(pasm);

#endif // WITH_LIB_CONSOLE
