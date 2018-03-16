// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdio.h>
#include <limits.h>

#include <blkctl/blkctl.h>
#include <blkctl/command.h>
#include <fs-management/ramdisk.h>
#include <zircon/types.h>

#include "ramdisk.h"

namespace blkctl {
namespace ramdisk {

zx_status_t Init::Run() {
    zx_status_t rc;
    BlkCtl* cmdline = this->cmdline();

    uint64_t blk_size, blk_count;
    if ((rc = cmdline->GetNumArg("blk_size", &blk_size)) != ZX_OK ||
        (rc = cmdline->GetNumArg("blk_count", &blk_count)) != ZX_OK ||
        (rc = cmdline->ArgsDone()) != ZX_OK) {
        return rc;
    }
    char path[PATH_MAX];
    if (create_ramdisk(blk_size, blk_count, path) != 0) {
        return ZX_ERR_INTERNAL;
    }
    // Try to open; sets devname as a useful side-effect.
    int fd;
    if ((rc = OpenReadable(path, &fd)) != ZX_OK) {
        fprintf(stderr, "unable to open %s\n", path);
        return rc;
    }
    printf("created %s\n", path);
    return ZX_OK;
}

zx_status_t Destroy::Run() {
    zx_status_t rc;
    BlkCtl* cmdline = this->cmdline();

    const char* dev;
    int fd;
    if ((rc = cmdline->GetStrArg("device", &dev)) != ZX_OK || (rc = cmdline->ArgsDone()) != ZX_OK ||
        (rc = OpenReadable(dev, &fd)) != ZX_OK || (rc = cmdline->Confirm()) != ZX_OK ||
        (rc = ReopenWritable(&fd)) != ZX_OK) {
        return rc;
    }
    if (destroy_ramdisk(devname()) != 0) {
        return ZX_ERR_INTERNAL;
    }
    printf("destroyed %s\n", devname());

    return ZX_OK;
}

} // namespace ramdisk
} // namespace blkctl
