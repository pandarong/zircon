// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>

#include <fbl/unique_ptr.h>
#include <unittest/unittest.h>
#include <zircon/types.h>

#include "utils.h"

namespace blkctl {
namespace testing {
namespace {

bool TestBadCommand(void) {
    BEGIN_TEST;

    // Missing everything!
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, ""));

    // Missing command
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl"));

    // Gibberish
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl booplesnoot"));

    END_TEST;
}

// blkctl ls
bool TestList(void) {
    BEGIN_TEST;
    ScopedRamdisk ramdisk;
    ASSERT_TRUE(ramdisk.Init());

    // Too many args
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl ls foo"));

    // Valid
    EXPECT_TRUE(ParseAndRun(ZX_OK, "blkctl ls"));

    END_TEST;
}

// blkctl -d <dev> dump
bool TestDump(void) {
    BEGIN_TEST;
    ScopedRamdisk ramdisk;
    ASSERT_TRUE(ramdisk.Init());

    // Missing device
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl dump"));

    // Too many args
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl dump %s foo", ramdisk.path()));

    // Valid
    EXPECT_TRUE(ParseAndRun(ZX_OK, "blkctl dump %s", ramdisk.path()));

    END_TEST;
}

BEGIN_TEST_CASE(CommandTest)
RUN_TEST(TestBadCommand)
RUN_TEST(TestList)
RUN_TEST(TestDump)
END_TEST_CASE(CommandTest)

} // namespace
} // namespace testing
} // namespace blkctl
