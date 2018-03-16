// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>

#include <blkctl/command.h>
#include <blkctl/blkctl.h>
#include <fbl/unique_ptr.h>
#include <unittest/unittest.h>
#include <zircon/types.h>

#include "utils.h"

namespace blkctl {
namespace testing {
namespace {

bool TestBadCommand(void) {
    BEGIN_TEST;

    // Missing command
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl ramdisk"));

    // Gibberish
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl ramdisk booplesnoot"));

    END_TEST;
}

bool TestInitDestroy(void) {
    BEGIN_TEST;

    // Missing block size and/or count
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl ramdisk init"));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl ramdisk init %zu", kBlockSize));

    // Bad block size
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl ramdisk init foo %zu", kBlockSize));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl ramdisk init -1 %zu", kBlockSize));

    // Bad block count
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl ramdisk init %zu foo", kBlockSize));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl ramdisk init %zu -1", kBlockSize));

    // Too many args
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl ramdisk init %zu %zu foo", kBlockSize,
                            kBlockCount));

    // Valid; do it "manually" to get ramdisk path
    fbl::Vector<char*> args;
    char buf[PATH_MAX];
    ASSERT_TRUE(SplitArgs(&args, buf, sizeof(buf), "blkctl ramdisk init %zu %zu",
                          kBlockSize, kBlockCount));
    BlkCtl cmdline;
    ASSERT_EQ(cmdline.Parse(static_cast<int>(args.size()), args.get()), ZX_OK);

    Command *cmd = cmdline.cmd();
    EXPECT_EQ(cmd->Run(), ZX_OK);
    const char *path = cmd->devname();

    // Missing/bad device
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl ramdisk destroy"));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl ramdisk destroy booplesnoot"));

    // Too many args
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl ramdisk destroy %s foo", path));

    // Valid
    EXPECT_TRUE(ParseAndRun(ZX_OK, "blkctl ramdisk destroy --force %s", path));

    END_TEST;
}

BEGIN_TEST_CASE(RamdiskCommandTest)
RUN_TEST(TestBadCommand)
RUN_TEST(TestInitDestroy)
END_TEST_CASE(RamdiskCommandTest)

} // namespace
} // namespace testing
} // namespace blkctl
