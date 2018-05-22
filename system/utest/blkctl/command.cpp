// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>

#include <fbl/auto_call.h>
#include <fbl/unique_ptr.h>
#include <unittest/unittest.h>
#include <zircon/types.h>

#include "utils.h"

namespace blkctl {
namespace testing {
namespace {

bool TestBadCommand(void) {
    BEGIN_TEST;
    BlkCtlTest blkctl;

    // Missing everything!
    EXPECT_EQ(ZX_ERR_INVALID_ARGS, BlkCtl::Execute(0, nullptr));

    // Missing command
    char* argv0 = strdup("blkctl");
    auto cleanup = fbl::MakeAutoCall([argv0]() { free(argv0); });
    EXPECT_EQ(ZX_ERR_INVALID_ARGS, BlkCtl::Execute(1, &argv0));

    // Gibberish
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "booplesnoot"));

    END_TEST;
}

// blkctl ls
bool TestList(void) {
    BEGIN_TEST;
    BlkCtlTest blkctl;
    ScopedRamdisk ramdisk;
    ASSERT_TRUE(ramdisk.Init());

    // Too many args
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "ls foo"));

    // Valid
    EXPECT_TRUE(blkctl.Run(ZX_OK, "ls"));

    END_TEST;
}

// blkctl -d <dev> dump
bool TestDump(void) {
    BEGIN_TEST;
    BlkCtlTest blkctl;
    ScopedRamdisk ramdisk;
    ASSERT_TRUE(ramdisk.Init());

    // Missing device
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "dump"));

    // Too many args
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "dump %s foo", ramdisk.path()));

    // Valid
    EXPECT_TRUE(blkctl.Run(ZX_OK, "dump %s", ramdisk.path()));

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
