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

// TODO(security): ZX-1130
// const char* kKey0 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
// const char* kKey1 = "0000111122223333444455556666777788889999aaaabbbbccccddddeeeeffff";
const char* kKey0 = "0000000000000000000000000000000000000000000000000000000000000000";
const char* kKey1 = "0000000000000000000000000000000000000000000000000000000000000000";

bool TestBadCommand(void) {
    BEGIN_TEST;
    BlkCtlTest blkctl;

    // Missing command
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "zxcrypt"));

    // Gibberish
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "zxcrypt foo"));

    END_TEST;
}

bool TestCreate(void) {
    BEGIN_TEST;
    BlkCtlTest blkctl;

    fbl::unique_fd fd;
    ScopedRamdisk ramdisk;
    ASSERT_TRUE(ramdisk.Init(&fd));

    // Missing/extra arguments
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "zxcrypt create"));
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "zxcrypt create %s foo", ramdisk.path()));

    // Bad device
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "zxcrypt create foo"));

    // Empty key
    blkctl.SetCanned("");
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "zxcrypt create %s", ramdisk.path()));

    // Non-hex key
    blkctl.SetCanned("This is not hex! It is not even close. Just what did you expect?");
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "zxcrypt create %s", ramdisk.path()));

    // Short key
    char fmt[8];
    snprintf(fmt, sizeof(fmt), "%%.%zus", strlen(kKey0) - 1);
    blkctl.SetCanned(fmt, kKey0);
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "zxcrypt create %s", ramdisk.path()));

    // Long key
    blkctl.SetCanned("%s0", kKey0);
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "zxcrypt create %s", ramdisk.path()));

    // Valid
    blkctl.SetCanned(kKey0);
    ASSERT_TRUE(ramdisk.Init(&fd));
    EXPECT_TRUE(blkctl.Run(ZX_OK, "zxcrypt create %s", ramdisk.path()));

    END_TEST;
}

bool TestOpen(void) {
    BEGIN_TEST;
    BlkCtlTest blkctl;

    fbl::unique_fd fd;
    ScopedRamdisk ramdisk;
    ASSERT_TRUE(ramdisk.Init(&fd));

    // Missing/extra arguments
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "zxcrypt open %s", ramdisk.path()));
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "zxcrypt open %s 0 foo", ramdisk.path()));

    // Not a zxcrypt volume
    blkctl.SetCanned(kKey0);
    EXPECT_TRUE(blkctl.Run(ZX_ERR_ACCESS_DENIED, "zxcrypt open %s 0", ramdisk.path()));

    // Bad key
    blkctl.SetCanned(kKey0);
    EXPECT_TRUE(blkctl.Run(ZX_OK, "zxcrypt create %s", ramdisk.path()));

    // TODO(security): ZX-1130
    // blkctl.SetCanned(kKey1);
    // EXPECT_TRUE(blkctl.Run(ZX_ERR_ACCESS_DENIED, "zxcrypt open %s 0", ramdisk.path()));
    // ASSERT_TRUE(ramdisk.Init(&fd));

    // Valid
    blkctl.SetCanned(kKey0);
    EXPECT_TRUE(blkctl.Run(ZX_OK, "zxcrypt create %s", ramdisk.path()));

    blkctl.SetCanned(kKey0);
    EXPECT_TRUE(blkctl.Run(ZX_OK, "zxcrypt open %s 0", ramdisk.path()));

    END_TEST;
}

bool TestEnroll(void) {
    BEGIN_TEST;
    BlkCtlTest blkctl;

    fbl::unique_fd fd;
    ScopedRamdisk ramdisk;
    ASSERT_TRUE(ramdisk.Init(&fd));

    // Missing/extra arguments
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "zxcrypt enroll %s 0", ramdisk.path()));
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "zxcrypt enroll %s 0 1 foo", ramdisk.path()));

    // Not a zxcrypt volume
    blkctl.SetCanned("%s\n%s", kKey1, kKey1);
    EXPECT_TRUE(blkctl.Run(ZX_ERR_ACCESS_DENIED, "zxcrypt enroll %s 0 1", ramdisk.path()));

    // Bad key
    blkctl.SetCanned(kKey0);
    EXPECT_TRUE(blkctl.Run(ZX_OK, "zxcrypt create %s", ramdisk.path()));

    blkctl.SetCanned("%s\n%s", kKey1, kKey1);
    EXPECT_TRUE(blkctl.Run(ZX_ERR_ACCESS_DENIED, "zxcrypt enroll %s 0 1", ramdisk.path()));
    ASSERT_TRUE(ramdisk.Init(&fd));

    // Valid
    blkctl.SetCanned(kKey0);
    EXPECT_TRUE(blkctl.Run(ZX_OK, "zxcrypt create %s", ramdisk.path()));

    blkctl.SetCanned("%s\n%s", kKey0, kKey1);
    EXPECT_TRUE(blkctl.Run(ZX_OK, "zxcrypt enroll %s 0 1", ramdisk.path()));

    blkctl.SetCanned(kKey1);
    EXPECT_TRUE(blkctl.Run(ZX_OK, "zxcrypt open %s 1", ramdisk.path()));

    END_TEST;
}

bool TestRevoke(void) {
    BEGIN_TEST;
    BlkCtlTest blkctl;

    fbl::unique_fd fd;
    ScopedRamdisk ramdisk;
    ASSERT_TRUE(ramdisk.Init(&fd));

    // Missing/extra arguments
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "zxcrypt revoke %s 0", ramdisk.path()));
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "zxcrypt revoke %s 0 1 foo", ramdisk.path()));

    // Not a zxcrypt volume
    blkctl.SetCanned(kKey0);
    EXPECT_TRUE(blkctl.Run(ZX_ERR_ACCESS_DENIED, "zxcrypt revoke %s 0 1", ramdisk.path()));

    // Bad key
    blkctl.SetCanned(kKey0);
    EXPECT_TRUE(blkctl.Run(ZX_OK, "zxcrypt create %s", ramdisk.path()));

    blkctl.SetCanned(kKey1);
    EXPECT_TRUE(blkctl.Run(ZX_ERR_ACCESS_DENIED, "zxcrypt revoke %s 0 1", ramdisk.path()));
    ASSERT_TRUE(ramdisk.Init(&fd));

    // Valid (even without enroll)
    blkctl.SetCanned(kKey0);
    EXPECT_TRUE(blkctl.Run(ZX_OK, "zxcrypt create %s", ramdisk.path()));

    blkctl.SetCanned(kKey0);
    EXPECT_TRUE(blkctl.Run(ZX_OK, "zxcrypt revoke %s 0 1", ramdisk.path()));
    ASSERT_TRUE(ramdisk.Init(&fd));

    // Valid, and check that the key isn't usable
    blkctl.SetCanned(kKey0);
    EXPECT_TRUE(blkctl.Run(ZX_OK, "zxcrypt create %s", ramdisk.path()));

    blkctl.SetCanned("%s\n%s", kKey0, kKey1);
    EXPECT_TRUE(blkctl.Run(ZX_OK, "zxcrypt enroll %s 0 1", ramdisk.path()));

    blkctl.SetCanned(kKey1);
    EXPECT_TRUE(blkctl.Run(ZX_OK, "zxcrypt revoke %s 1 0", ramdisk.path()));

    blkctl.SetCanned(kKey0);
    EXPECT_TRUE(blkctl.Run(ZX_ERR_ACCESS_DENIED, "zxcrypt open %s 0", ramdisk.path()));

    END_TEST;
}

bool TestShred(void) {
    BEGIN_TEST;
    BlkCtlTest blkctl;

    fbl::unique_fd fd;
    ScopedRamdisk ramdisk;
    ASSERT_TRUE(ramdisk.Init(&fd));

    // Missing/extra arguments
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "zxcrypt shred %s", ramdisk.path()));
    EXPECT_TRUE(blkctl.Run(ZX_ERR_INVALID_ARGS, "zxcrypt shred %s 0 foo", ramdisk.path()));

    // Not a zxcrypt volume
    blkctl.SetCanned(kKey1);
    EXPECT_TRUE(blkctl.Run(ZX_ERR_ACCESS_DENIED, "zxcrypt shred %s 0", ramdisk.path()));

    // Bad key
    blkctl.SetCanned(kKey0);
    EXPECT_TRUE(blkctl.Run(ZX_OK, "zxcrypt create %s", ramdisk.path()));

    blkctl.SetCanned(kKey1);
    EXPECT_TRUE(blkctl.Run(ZX_ERR_ACCESS_DENIED, "zxcrypt shred %s 0", ramdisk.path()));
    ASSERT_TRUE(ramdisk.Init(&fd));

    // Valid
    blkctl.SetCanned(kKey0);
    EXPECT_TRUE(blkctl.Run(ZX_OK, "zxcrypt create %s", ramdisk.path()));

    blkctl.SetCanned("%s\n%s", kKey0, kKey1);
    EXPECT_TRUE(blkctl.Run(ZX_OK, "zxcrypt enroll %s 0 1", ramdisk.path()));

    blkctl.SetCanned(kKey1);
    EXPECT_TRUE(blkctl.Run(ZX_OK, "zxcrypt shred %s 1", ramdisk.path()));

    blkctl.SetCanned(kKey0);
    EXPECT_TRUE(blkctl.Run(ZX_ERR_ACCESS_DENIED, "zxcrypt open %s 0", ramdisk.path()));

    blkctl.SetCanned(kKey1);
    EXPECT_TRUE(blkctl.Run(ZX_ERR_ACCESS_DENIED, "zxcrypt open %s 1", ramdisk.path()));

    END_TEST;
}

BEGIN_TEST_CASE(ZxcryptCommandTest)
RUN_TEST(TestCreate)
RUN_TEST(TestOpen)
RUN_TEST(TestEnroll)
RUN_TEST(TestRevoke)
RUN_TEST(TestShred)
END_TEST_CASE(ZxcryptCommandTest)

} // namespace
} // namespace testing
} // namespace blkctl
