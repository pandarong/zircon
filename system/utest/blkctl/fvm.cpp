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

    // Missing command
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm"));

    // Gibberish
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm booplesnoot"));

    END_TEST;
}

bool TestInitDestroy(void) {
    BEGIN_TEST;
    ScopedRamdisk ramdisk;
    ScopedRamdisk nonfvm;
    ASSERT_TRUE(ramdisk.Init());
    ASSERT_TRUE(nonfvm.Init());

    // Missing/bad device
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm init"));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm init booplesnoot"));

    // Missing/bad slice size
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm init %s", ramdisk.path()));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm init %s foo", ramdisk.path()));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm init %s -1", ramdisk.path()));

    // Too many args
    EXPECT_TRUE(
        ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm init %s %zu foo", ramdisk.path(), kSliceSize));

    // Valid
    EXPECT_TRUE(ParseAndRun(ZX_OK, "blkctl fvm init --force %s %zu", ramdisk.path(), kSliceSize));

    // Missing/bad device
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm destroy"));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm destroy booplesnoot"));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm destroy %s", nonfvm.path()));

    // Too many args
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm destroy %s foo", ramdisk.path()));

    // Valid
    EXPECT_TRUE(ParseAndRun(ZX_OK, "blkctl fvm destroy --force %s", ramdisk.path()));

    END_TEST;
}

bool TestDump(void) {
    BEGIN_TEST;
    ScopedFvmPartition partition;
    ASSERT_TRUE(partition.Init());
    const ScopedFvmVolume& volume = partition.volume();
    const ScopedRamdisk& ramdisk = volume.ramdisk();

    // Missing/bad device
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm dump"));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm dump booplesnoot"));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm add %s", ramdisk.path()));

    // Too many args
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm dump %s foo", volume.path()));

    // Valid for volume
    EXPECT_TRUE(ParseAndRun(ZX_OK, "blkctl fvm dump %s", volume.path()));

    // Valid for partition
    EXPECT_TRUE(ParseAndRun(ZX_OK, "blkctl fvm dump %s", partition.path()));

    END_TEST;
}

bool TestAdd(void) {
    BEGIN_TEST;
    ScopedFvmVolume volume;
    ASSERT_TRUE(volume.Init());
    const ScopedRamdisk& ramdisk = volume.ramdisk();

    // Missing device
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm add"));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm add booplesnoot"));

    // Missing/bad name and/or slices
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm add %s", volume.path()));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm add %s foo", volume.path()));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm add %s foo bar", volume.path()));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm add %s foo -1", volume.path()));

    // GUID is optional
    EXPECT_TRUE(ParseAndRun(ZX_OK, "blkctl fvm add --force %s foo %zu", volume.path(),
                            volume.slices() / 2));

    // Bad GUID
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS,
                            "blkctl fvm add %s bar %zu 00000000-0000-0000-0000000000000000",
                            volume.path(), volume.slices()));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS,
                            "blkctl fvm add %s bar %zu thisisno-thex-adec-imal-anditmustbe!",
                            volume.path(), volume.slices()));

    // Too many args
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS,
                            "blkctl fvm add %s bar %zu deadbeef-dead-beef-dead-beefdeadbeef bar",
                            volume.path(), volume.slices()));

    // Bad device
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm add %s bar %zu", ramdisk.path(),
                            volume.slices() / 2));

    // Valid
    EXPECT_TRUE(
        ParseAndRun(ZX_OK, "blkctl fvm add --force %s bar %zu deadbeef-dead-beef-dead-beefdeadbeef",
                    volume.path(), volume.slices() / 2));

    END_TEST;
}

bool TestQuery(void) {
    BEGIN_TEST;
    ScopedFvmPartition partition;
    ASSERT_TRUE(partition.Init());
    const ScopedFvmVolume& volume = partition.volume();

    // Missing/bad device
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm query"));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm query booplesnoot"));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm query %s", volume.path()));

    // Too many args
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm query %s foo", partition.path()));

    // Valid
    EXPECT_TRUE(ParseAndRun(ZX_OK, "blkctl fvm query %s", partition.path()));

    END_TEST;
}

bool TestExtend(void) {
    BEGIN_TEST;
    ScopedFvmPartition partition;
    ASSERT_TRUE(partition.Init());
    const ScopedFvmVolume& volume = partition.volume();

    // Missing device
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm extend"));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm extend booplesnoot"));

    // Missing/bad start and/or length
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm extend %s", partition.path()));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm extend %s %zu", partition.path(),
                            partition.slices()));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm extend %s foo 1", partition.path()));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm extend %s -1 1", partition.path()));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm extend %s %zu foo", partition.path(),
                            partition.slices()));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm extend %s %zu -1", partition.path(),
                            partition.slices()));

    // Too many args
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm extend %s %zu 1 foo", partition.path(),
                            partition.slices()));

    // Bad device
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm extend %s %zu 1", volume.path(),
                            partition.slices()));

    // Valid
    EXPECT_TRUE(ParseAndRun(ZX_OK, "blkctl fvm extend --force %s %zu 1", partition.path(),
                            partition.slices()));

    END_TEST;
}

bool TestShrink(void) {
    BEGIN_TEST;
    ScopedFvmPartition partition;
    ASSERT_TRUE(partition.Init());
    const ScopedFvmVolume& volume = partition.volume();

    // Missing device
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm shrink"));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm shrink booplesnoot"));

    // Missing/bad start and/or length
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm shrink %s", partition.path()));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm shrink %s %zu", partition.path(),
                            partition.slices() - 1));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm shrink %s foo 1", partition.path()));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm shrink %s -1 1", partition.path()));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm shrink %s %zu foo", partition.path(),
                            partition.slices() - 1));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm shrink %s %zu -1", partition.path(),
                            partition.slices() - 1));

    // Too many args
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm shrink %s %zu 1 foo", partition.path(),
                            partition.slices() - 1));

    // Bad device
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm shrink %s %zu 1", volume.path(),
                            partition.slices() - 1));

    // Valid
    EXPECT_TRUE(ParseAndRun(ZX_OK, "blkctl fvm shrink --force %s %zu 1", partition.path(),
                            partition.slices() - 1));

    END_TEST;
}

bool TestRemove(void) {
    BEGIN_TEST;
    ScopedFvmPartition partition;
    ASSERT_TRUE(partition.Init());
    const ScopedFvmVolume& volume = partition.volume();

    // Missing device
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm remove"));
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm remove booplesnoot"));

    // Too many args
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm remove %s foo", partition.path()));

    // Bad device
    EXPECT_TRUE(ParseAndRun(ZX_ERR_INVALID_ARGS, "blkctl fvm remove %s", volume.path()));

    // Valid
    EXPECT_TRUE(ParseAndRun(ZX_OK, "blkctl fvm remove --force %s", partition.path()));

    END_TEST;
}

BEGIN_TEST_CASE(FvmCommandTest)
RUN_TEST(TestInitDestroy)
RUN_TEST(TestDump)
RUN_TEST(TestAdd)
RUN_TEST(TestQuery)
RUN_TEST(TestExtend)
RUN_TEST(TestShrink)
RUN_TEST(TestRemove)
END_TEST_CASE(FvmCommandTest)

} // namespace
} // namespace testing
} // namespace blkctl
