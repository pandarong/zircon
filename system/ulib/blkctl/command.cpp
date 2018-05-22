// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <blkctl/blkctl.h>
#include <blkctl/command.h>
#include <fbl/auto_call.h>
#include <fbl/string.h>
#include <fbl/unique_fd.h>
#include <zircon/assert.h>
#include <zircon/device/block.h>
#include <zircon/errors.h>
#include <zircon/syscalls.h>
#include <zircon/types.h>

namespace blkctl {
namespace {

// Byte offsets where dashes are placed in a printed GUID
constexpr size_t kGuidSegmentLens[] = {4, 2, 2, 2, 6};

} // namespace

// Public methods

Command::~Command() {}

// Protected methods

void Command::GenerateGuid(uint8_t* out, size_t out_len) {
    ZX_DEBUG_ASSERT(out);
    ZX_DEBUG_ASSERT(out_len == GUID_LEN);
    zx_cprng_draw(out, out_len);
    out[6] = (out[6] & 0x0F) | 0x40;
    out[8] = (out[8] & 0x3F) | 0x80;
}

zx_status_t Command::ParseHex(const char* str, uint8_t* out, size_t out_len, char delim) {
    ZX_DEBUG_ASSERT(out || out_len == 0);
    auto printError =
        fbl::MakeAutoCall([str] { fprintf(stderr, "failed to parse hex: '%s'\n", str); });
    if (strlen(str) < out_len * 2) {
        return ZX_ERR_INVALID_ARGS;
    }
    const char* p = str;
    for (size_t i = 0; i < out_len; ++i) {
        if (sscanf(p, "%02" SCNx8, &out[i]) != 1) {
            return ZX_ERR_INVALID_ARGS;
        }
        p += 2;
    }
    if (*p != delim) {
        return ZX_ERR_INVALID_ARGS;
    }
    printError.cancel();

    return ZX_OK;
}

zx_status_t Command::ParseGuid(const char* guid, uint8_t* out, size_t out_len) {
    ZX_DEBUG_ASSERT(out);
    ZX_DEBUG_ASSERT(out_len == GUID_LEN);
    zx_status_t rc;

    for (size_t i = 0; i < sizeof(kGuidSegmentLens) / sizeof(kGuidSegmentLens[0]); ++i) {
        size_t segment_len = kGuidSegmentLens[i];
        if (out_len < segment_len) {
            return ZX_ERR_INVALID_ARGS;
        }
        if (out_len > segment_len) {
            rc = ParseHex(guid, out, segment_len, '-');
        } else {
            rc = ParseHex(guid, out, out_len);
        }
        if (rc != ZX_OK) {
            return rc;
        }
        guid += (2 * segment_len) + 1;
        out += segment_len;
        out_len -= segment_len;
    }

    return ZX_OK;
}

void Command::PrintGuid(const uint8_t* guid, size_t guid_len) {
    for (size_t i = 0; i < sizeof(kGuidSegmentLens) / sizeof(kGuidSegmentLens[0]); ++i) {
        size_t segment_len = kGuidSegmentLens[i];
        ZX_DEBUG_ASSERT(guid_len >= segment_len);
        for (size_t j = 0; j < segment_len; ++j) {
            printf("%02" PRIx8, *guid++);
        }
        guid_len -= segment_len;
        if (guid_len != 0) {
            printf("-");
        }
    }
}

zx_status_t Command::OpenReadable(const char* dev, int* out_fd) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s", dev);
    devfd_.reset(open(dev, O_RDONLY));
    if (!devfd_) {
        snprintf(path, sizeof(path), "%s/%s", kDevClassBlock, dev);
        devfd_.reset(open(path, O_RDONLY));
    }
    if (!devfd_) {
        fprintf(stderr, "unable to open: %s\n", path);
        return ZX_ERR_INVALID_ARGS;
    }
    fbl::AllocChecker ac;
    devname_.Set(path, &ac);
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    *out_fd = devfd_.get();
    return ZX_OK;
}

zx_status_t Command::ReopenWritable(int* out) {
    devfd_.reset(open(devname_.c_str(), O_RDWR));
    if (!devfd_) {
        fprintf(stderr, "failed to reopen %s as read-write\n", devname_.c_str());
        return ZX_ERR_IO;
    }

    *out = devfd_.get();
    return ZX_OK;
}

} // namespace blkctl
