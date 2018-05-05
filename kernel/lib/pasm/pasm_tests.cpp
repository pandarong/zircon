// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <err.h>
#include <fbl/algorithm.h>
#include <lib/pasm/pasm.h>
#include <lib/unittest/unittest.h>
#include <sys/types.h>

// Many of this library's tests are covered via RegionAllocator's testing
// of the regions. Most of the tests here are aiming to test the integration
// of the library api and the bookkeeping structures provided by ralloc.
// Test that the error conditions for out-of-order calls all behave propelry
static bool pasm_error_test() {
    BEGIN_TEST;
    auto pasm = PhysicalAspaceManagerTest::CreateTestInstance();
    ASSERT_NONNULL(pasm, "");

    // Try to reserve more than the max number of entries that fit in a page
    for (size_t i = 0; i < PhysicalAspaceManager::kEarlyRegionMaxCnt + 2; i++) {
        zx_status_t st = pasm->ReserveAddressSpaceEarly(PhysicalAspaceManager::kIoAllocator, i, 1);
        if (i < PhysicalAspaceManager::kEarlyRegionMaxCnt) {
            EXPECT_EQ(ZX_OK, st, "");
        } else {
            EXPECT_EQ(ZX_ERR_NO_MEMORY, st, "");
        }
    }

    RegionAllocator::Region::UPtr uptr1, uptr2;
    EXPECT_EQ(ZX_ERR_BAD_STATE,
              pasm->ReserveAddressSpace(PhysicalAspaceManager::kMmioAllocator, 0, 1024u, uptr1), "");
    EXPECT_EQ(ZX_OK, pasm->Initialize(0, PAGE_SIZE, 0, UINT64_MAX, 0, UINT64_MAX), "");

    uintptr_t base = PAGE_SIZE;
    size_t size = PAGE_SIZE;
    EXPECT_EQ(ZX_ERR_NOT_FOUND, pasm->ReserveAddressSpace(PhysicalAspaceManager::kMmioAllocator,
                                                          base, size, uptr2),
              "");
    END_TEST;
}

static bool pasm_early_alloc_test() {
    BEGIN_TEST;
    auto pasm = PhysicalAspaceManagerTest::CreateTestInstance();
    const uintptr_t test_base = 1 << 30;
    const size_t test_size = PAGE_SIZE;
    RegionAllocator::Region::UPtr uptr1, uptr2, uptr3;

    ASSERT_NONNULL(pasm, "");
    ASSERT_EQ(ZX_OK, pasm->ReserveAddressSpaceEarly(
                         PhysicalAspaceManager::kMmioAllocator, test_base, test_size),
              "");
    ASSERT_EQ(ZX_OK, pasm->Initialize(0, UINT64_MAX, 0, 0, 0, 0), "");
    // Check regions that overlap partially with valid space
    EXPECT_EQ(ZX_ERR_NOT_FOUND, pasm->ReserveAddressSpace(PhysicalAspaceManager::kMmioAllocator,
                                                          test_base - 512, test_size, uptr1),
              "");
    EXPECT_EQ(ZX_ERR_NOT_FOUND, pasm->ReserveAddressSpace(PhysicalAspaceManager::kMmioAllocator,
                                                          test_base + 512, test_size, uptr2),
              "");
    // Try to pull out the previously early allocated region
    EXPECT_EQ(ZX_ERR_NOT_FOUND, pasm->ReserveAddressSpace(PhysicalAspaceManager::kMmioAllocator,
                                                          test_base, test_size, uptr3),
              "");

    END_TEST;
}

static bool pasm_valid_alloc_test() {
    BEGIN_TEST;

    auto pasm = PhysicalAspaceManagerTest::CreateTestInstance();
    const uintptr_t base = 1 << 20;
    const size_t size = PAGE_SIZE;
    RegionAllocator::Region::UPtr uptr1, uptr2, uptr3, uptr4, uptr5;
    ASSERT_NONNULL(pasm, "");
    ASSERT_EQ(ZX_OK, pasm->Initialize(0, UINT64_MAX, 0, UINT64_MAX, 0, UINT64_MAX), "");
    EXPECT_EQ(ZX_OK, pasm->ReserveAddressSpace(PhysicalAspaceManager::kMmioAllocator,
                                               base, size, uptr1),
              "");
    EXPECT_EQ(uptr1->base, base, "");
    EXPECT_EQ(uptr1->size, size, "");
    EXPECT_EQ(ZX_ERR_NOT_FOUND, pasm->ReserveAddressSpace(PhysicalAspaceManager::kMmioAllocator,
                                                          base, size, uptr2),
              "");
    EXPECT_EQ(ZX_OK, pasm->ReserveAddressSpace(PhysicalAspaceManager::kIoAllocator,
                                               base, size, uptr3),
              "");
    EXPECT_EQ(ZX_ERR_NOT_FOUND, pasm->ReserveAddressSpace(PhysicalAspaceManager::kIoAllocator,
                                                          base, size, uptr4),
              "");
    EXPECT_EQ(ZX_OK, pasm->ReserveAddressSpace(PhysicalAspaceManager::kIrqAllocator,
                                               base, size, uptr5),
              "");
    END_TEST;
}

static bool pasm_cleanup_test() {
    BEGIN_TEST;

    const uintptr_t base = 1 << 20;
    const size_t size = PAGE_SIZE << 10;
    auto pasm = PhysicalAspaceManagerTest::CreateTestInstance();
    ASSERT_NONNULL(pasm, "");
    ASSERT_EQ(ZX_OK, pasm->Initialize(0, UINT64_MAX, 0, UINT64_MAX, 0, UINT64_MAX), "");

    // Reserve some space, confirm that it was removed
    {
        RegionAllocator::Region::UPtr uptr1, uptr2;
        EXPECT_EQ(ZX_OK, pasm->ReserveAddressSpace(
                             PhysicalAspaceManager::kIrqAllocator, base, size, uptr1),
                  "");
        EXPECT_EQ(ZX_ERR_NOT_FOUND, pasm->ReserveAddressSpace(
                                        PhysicalAspaceManager::kIrqAllocator, base, size, uptr2),
                  "");
    }

    // Try to reserve the same space to ensure it was released back to the pool
    RegionAllocator::Region::UPtr uptr1, uptr2;
    EXPECT_EQ(ZX_OK, pasm->ReserveAddressSpace(
                         PhysicalAspaceManager::kIrqAllocator, base, size, uptr1),
              "");
    EXPECT_EQ(ZX_ERR_NOT_FOUND, pasm->ReserveAddressSpace(
                                    PhysicalAspaceManager::kIrqAllocator, base, size, uptr2),
              "");

    END_TEST;
}

UNITTEST_START_TESTCASE(pasm_tests)
UNITTEST("pasm_error_test", pasm_error_test)
UNITTEST("pasm_early_alloc_test", pasm_early_alloc_test)
UNITTEST("pasm_valid_alloc_test", pasm_valid_alloc_test)
UNITTEST("pasm_cleanup_test", pasm_cleanup_test)
UNITTEST_END_TESTCASE(pasm_tests, "pasm_tests", "Physical Address Space Manager tests");
