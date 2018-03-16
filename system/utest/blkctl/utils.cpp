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

bool VSplitArgs(fbl::Vector<char*>* out, char* buf, size_t buf_len, const char* fmt, va_list ap) {
    BEGIN_HELPER;
    ASSERT_NONNULL(out);
    ASSERT_NONNULL(buf);

    ssize_t len = vsnprintf(buf, buf_len, fmt, ap);
    ASSERT_GE(len, 0);
    size_t n = static_cast<size_t>(len);
    ASSERT_LT(n, buf_len);

    fbl::AllocChecker ac;
    bool token = true;
    for (size_t i = 0; i < n; ++i) {
        if (isspace(buf[i])) {
            buf[i] = '\0';
            token = true;
        } else if (token && isprint(buf[i])) {
            out->push_back(&buf[i], &ac);
            ASSERT_TRUE(ac.check());
            token = false;
        }
    }
    END_HELPER;
}

} // namespace

bool SplitArgs(fbl::Vector<char*>* out, char* buf, size_t buf_len, const char* fmt, ...) {
    BEGIN_HELPER;

    va_list ap;
    va_start(ap, fmt);
    ASSERT_TRUE(VSplitArgs(out, buf, buf_len, fmt, ap));
    va_end(ap);

    END_HELPER;
}

bool ParseAndRun(zx_status_t expected, const char* fmt, ...) {
    BEGIN_HELPER;

    char buf[PATH_MAX];
    va_list ap;
    va_start(ap, fmt);
    fbl::Vector<char*> args;
    ASSERT_TRUE(VSplitArgs(&args, buf, sizeof(buf), fmt, ap));
    va_end(ap);

    EXPECT_EQ(BlkCtl::Execute(static_cast<int>(args.size()), args.get()), expected);

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
