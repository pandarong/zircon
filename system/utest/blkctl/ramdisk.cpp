// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>

#include <blkctl/blkctl.h>
#include <blkctl/command.h>
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

    // Missing command
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "ramdisk"));

    // Gibberish
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "ramdisk booplesnoot"));

    END_TEST;
}

bool TestInitDestroy(void) {
    BEGIN_TEST;
    BlkCtlTest blkctl;

    // Missing block size and/or count
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "blkctl ramdisk init"));
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "ramdisk init %zu", kBlockSize));

    // Bad block size
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "ramdisk init foo %zu", kBlockSize));
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "ramdisk init -1 %zu", kBlockSize));

    // Bad block count
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "ramdisk init %zu foo", kBlockSize));
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "ramdisk init %zu -1", kBlockSize));

    // Too many args
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "ramdisk init %zu %zu f", kBlockSize, kBlockCount));

    // Valid
    EXPECT_TRUE(blkctl.Run(ZX_OK, "ramdisk init %zu %zu", kBlockSize, kBlockCount));
    const char* path = blkctl.devname();

    // Missing/bad device
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "ramdisk destroy"));
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "ramdisk destroy booplesnoot"));

    // Too many args
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "ramdisk destroy %s foo", path));

    // Valid
    EXPECT_TRUE(blkctl.Run(ZX_OK, "ramdisk destroy %s", path));

    END_TEST;
}

BEGIN_TEST_CASE(RamdiskCommandTest)
RUN_TEST(TestBadCommand)
RUN_TEST(TestInitDestroy)
END_TEST_CASE(RamdiskCommandTest)

} // namespace
} // namespace testing
} // namespace blkctl
