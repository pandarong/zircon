// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <blkctl/blkctl.h>
#include <blkctl/command.h>
#include <fbl/auto_call.h>
#include <fbl/unique_ptr.h>
#include <fbl/vector.h>
#include <fs-management/fvm.h>
#include <fs-management/ramdisk.h>
#include <fvm/fvm.h>
#include <lib/zx/time.h>
#include <unittest/unittest.h>
#include <zircon/assert.h>
#include <zircon/device/block.h>
#include <zircon/device/device.h>
#include <zircon/status.h>

#include "utils.h"

namespace blkctl {
namespace testing {
namespace {

const char* kBinName = "blkctl";

} // namespace

bool BlkCtlTest::SetCanned(const char* fmt, ...) {
    BEGIN_HELPER;

    va_list ap;
    va_start(ap, fmt);
    ssize_t len = vsnprintf(canned_, sizeof(canned_), fmt, ap);
    va_end(ap);

    ASSERT_GE(len, 0);
    size_t n = static_cast<size_t>(len);
    ASSERT_LT(n, sizeof(canned_));

    use_canned_ = true;

    END_HELPER;
}

bool BlkCtlTest::Run(zx_status_t expected, const char* fmt, ...) {
    BEGIN_HELPER;

    // Consume canned responses.
    const char* canned = use_canned_ ? canned_ : nullptr;
    use_canned_ = false;

    // Construct command line.
    char buf[PAGE_SIZE / 2];
    va_list ap;
    va_start(ap, fmt);
    ssize_t len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    // Make a copy for error reporting
    char cmd[PAGE_SIZE / 2];
    snprintf(cmd, sizeof(cmd), "When executing 'blkctl %s'", buf);

    ASSERT_GE(len, 0, cmd);
    size_t n = static_cast<size_t>(len);
    ASSERT_LT(n, sizeof(buf), cmd);
    fbl::Vector<char*> args;
    fbl::AllocChecker ac;

    // Push argv[0]
    args.push_back(const_cast<char*>(kBinName), &ac);
    ASSERT_TRUE(ac.check(), cmd);

    // Split remaining args
    bool token = true;
    for (size_t i = 0; i < n; ++i) {
        if (isspace(buf[i])) {
            buf[i] = '\0';
            token = true;
        } else if (token && isprint(buf[i])) {
            args.push_back(&buf[i], &ac);
            ASSERT_TRUE(ac.check(), cmd);
            token = false;
        }
    }

    // |expected| may match either parsing or execution
    zx_status_t rc;
    if ((rc = obj_.Parse(static_cast<int>(args.size()), args.get(), canned)) != ZX_OK) {
        EXPECT_EQ(rc, expected, cmd);
    } else {
        // Always skip confirmations
        obj_.set_force(true);
        EXPECT_EQ(obj_.cmd()->Run(), expected, cmd);
    }

    END_HELPER;
}

bool ScopedDevice::Set(int fd, fbl::unique_fd* out) {
    BEGIN_HELPER;
    fbl::unique_fd ufd(fd);
    ssize_t res = ioctl_device_get_topo_path(ufd.get(), path_, sizeof(path_));
    ASSERT_GE(res, 0, zx_status_get_string(static_cast<zx_status_t>(res)));
    if (out) {
        out->swap(ufd);
    }
    END_HELPER;
}

bool ScopedDevice::Open(const char* parent, const char* child, fbl::unique_fd* out) {
    BEGIN_HELPER;
    ssize_t n;
    if (child) {
        n = snprintf(path_, sizeof(path_), "%s/%s", parent, child);
    } else {
        n = snprintf(path_, sizeof(path_), "%s", parent);
    }
    ASSERT_GE(n, 0);
    ASSERT_LT(static_cast<size_t>(n), sizeof(path_));
    ASSERT_EQ(wait_for_device(path_, zx::sec(3).get()), 0);
    ASSERT_TRUE(Set(open(path_, O_RDWR), out));
    END_HELPER;
}

ScopedRamdisk::~ScopedRamdisk() {
    destroy_ramdisk(path());
}

bool ScopedRamdisk::Init(fbl::unique_fd* out) {
    BEGIN_HELPER;
    if (size_ != 0) {
        destroy_ramdisk(path());
    }
    char path[PATH_MAX];
    ASSERT_EQ(create_ramdisk(kBlockSize, kBlockCount, path), 0);
    ASSERT_TRUE(Open(path, nullptr, out));
    size_ = kBlockSize * kBlockCount;
    END_HELPER;
}

bool ScopedFvmVolume::Init(fbl::unique_fd* out) {
    BEGIN_HELPER;
    fbl::unique_fd fd;
    ASSERT_TRUE(ramdisk_.Init(&fd));
    ASSERT_TRUE(fd);
    ASSERT_EQ(fvm_init(fd.get(), kSliceSize), ZX_OK);
    const char* driver = "/boot/driver/fvm.so";
    ASSERT_GE(ioctl_device_bind(fd.get(), driver, strlen(driver)), 0);
    ASSERT_TRUE(Open(ramdisk_.path(), "fvm", out));
    slices_ = fvm::UsableSlicesCount(ramdisk_.size(), kSliceSize);
    END_HELPER;
}

bool ScopedFvmPartition::Init(fbl::unique_fd* out) {
    BEGIN_HELPER;
    fbl::unique_fd fd;
    ASSERT_TRUE(volume_.Init(&fd));
    ASSERT_TRUE(fd);
    slices_ = volume_.slices() / 2;
    alloc_req_t req;
    memset(&req, 0, sizeof(alloc_req_t));
    req.slice_count = slices_;
    ASSERT_TRUE(Set(fvm_allocate_partition(fd.get(), &req), out));
    END_HELPER;
}

} // namespace testing
} // namespace blkctl
