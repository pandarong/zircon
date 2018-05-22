// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <limits.h>
#include <stddef.h>

#include <blkctl/blkctl.h>
#include <blkctl/command.h>
#include <fbl/macros.h>
#include <fbl/unique_fd.h>
#include <fbl/vector.h>
#include <zircon/types.h>

namespace blkctl {
namespace testing {

constexpr size_t kBlockCount = 512;
constexpr size_t kBlockSize = 512;
constexpr size_t kSliceSize = 8192;
constexpr size_t kSliceCount = (kBlockCount * kBlockSize) / kSliceSize;

// |BlkCtlTest| wraps accesses to a |BlkCtl| object with enough state to pass canned responses in
// and get device names back out during testing.
class BlkCtlTest {
public:
    BlkCtlTest() : use_canned_(false) {}
    ~BlkCtlTest() {}

    const char* devname() const { return obj_.cmd()->devname(); }

    // Sets the the canned response to prompts, using a printf-style |fmt| string and arguments.
    bool SetCanned(const char* fmt, ...);

    // Produces a formatted command line from |fmt| and the trailing arguments, then parses and runs
    // it. If parsing returns an error, it it checked against |expected|, otherwise the result of
    // running it is checked.
    bool Run(zx_status_t expected, const char* fmt, ...);

private:
    DISALLOW_COPY_ASSIGN_AND_MOVE(BlkCtlTest);

    // The wrapped object
    BlkCtl obj_;

    // Canned responses to prompts
    char canned_[PATH_MAX];
    bool use_canned_;
};

// |ScopedDevice| is the base class for creating block devices during testing that will
// automatically clean up on test completion.
class ScopedDevice {
public:
    ScopedDevice() : path_{0} {}
    virtual ~ScopedDevice() {}

    const char* path() const { return path_; }

    // Sets up the device.
    virtual bool Init(fbl::unique_fd* out = nullptr) = 0;

protected:
    // Saves the topological path of the device referred to by |fd| and wraps that argument in a
    // unique_fd.
    bool Set(int fd, fbl::unique_fd* out);

    // Waits for a |child| driver to bind to the |parent| device, then opens it and returns the
    // unique_fd via |out|.
    bool Open(const char* parent, const char* child, fbl::unique_fd* out);

private:
    DISALLOW_COPY_ASSIGN_AND_MOVE(ScopedDevice);

    // Topological path of this device
    char path_[PATH_MAX];
};

// |ScopedRamdisk| creates a ramdisk that is destroyed when the object goes out of scope.
class ScopedRamdisk final : public ScopedDevice {
public:
    ScopedRamdisk() : size_(0) {}
    ~ScopedRamdisk() override;

    size_t size() const { return size_; }

    // Creates the ramdisk
    bool Init(fbl::unique_fd* out = nullptr) override;

private:
    // Size of the ramdisk, in bytes
    size_t size_;
};

// |ScopedRamdisk| creates an FVM volume backed by a ramdisk that is destroyed when the object goes
// out of scope.
class ScopedFvmVolume final : public ScopedDevice {
public:
    ScopedFvmVolume() : slices_(0) {}

    const ScopedRamdisk& ramdisk() const { return ramdisk_; }
    size_t slices() const { return slices_; }

    // Creates the ramdisk and FVM volume
    bool Init(fbl::unique_fd* out = nullptr) override;

private:
    // Ramdisk backing this FVM volume
    ScopedRamdisk ramdisk_;
    // Number of allocatable slices in the volume
    size_t slices_;
};

// |ScopedRamdisk| creates an FVM partition that has half the slices of a created FVM volume backed
// by a ramdisk that is destroyed when the object goes out of scope.
class ScopedFvmPartition : public ScopedDevice {
public:
    ScopedFvmPartition() {}

    const ScopedFvmVolume& volume() const { return volume_; }
    size_t slices() const { return slices_; }

    // Creates the ramdisk, FVM volume, and partition
    bool Init(fbl::unique_fd* out = nullptr) override;

private:
    // FVM volume containing this partition
    ScopedFvmVolume volume_;
    // Number of slices initially allocated to this partition
    size_t slices_;
};

} // namespace testing
} // namespace blkctl
