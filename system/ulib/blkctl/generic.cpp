// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <blkctl/blkctl.h>
#include <blkctl/command.h>
#include <fbl/auto_call.h>
#include <zircon/assert.h>
#include <zircon/device/block.h>
#include <zircon/device/device.h>
#include <zircon/errors.h>
#include <zircon/status.h>
#include <zircon/types.h>

#include "generic.h"

namespace blkctl {
namespace generic {

zx_status_t Help::Run() {
    BlkCtl* cmdline = this->cmdline();
    cmdline->Usage();
    return ZX_OK;
}

zx_status_t List::Run() {
    zx_status_t rc;
    BlkCtl* cmdline = this->cmdline();

    if ((rc = cmdline->ArgsDone()) != ZX_OK) {
        return rc;
    }

    DIR* dptr = opendir(kDevClassBlock);
    ZX_DEBUG_ASSERT(dptr);
    auto cleanup = fbl::MakeAutoCall([&] { closedir(dptr); });

    struct dirent* dent;
    printf("%8s    %s\n", "ID", "Topological path");
    while ((dent = readdir(dptr))) {
        char* id = dent->d_name;
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", kDevClassBlock, id);
        fbl::unique_fd fd(open(path, O_RDONLY));
        if (!fd || ioctl_device_get_topo_path(fd.get(), path, sizeof(path)) < 0) {
            continue;
        }
        printf("%8s -> %s\n", id, path);
    }

    return ZX_OK;
}

zx_status_t Dump::Run() {
    zx_status_t rc;
    BlkCtl* cmdline = this->cmdline();

    char buf[PATH_MAX];
    ssize_t res;

    const char* dev;
    int fd;
    if ((rc = cmdline->GetStrArg("device", &dev)) != ZX_OK || (rc = cmdline->ArgsDone()) != ZX_OK ||
        (rc = OpenReadable(dev, &fd)) != ZX_OK) {
        return rc;
    }
    // Topological path
    printf("%16s: ", "Topological path");
    res = ioctl_device_get_topo_path(fd, buf, sizeof(buf));
    if (res >= 0) {
        printf("%s", buf);
    } else if (res != ZX_ERR_NOT_SUPPORTED) {
        rc = static_cast<zx_status_t>(res);
        fprintf(stderr, "ioctl_device_get_topo_path failed: %s\n", zx_status_get_string(rc));
        return rc;
    }
    printf("\n");

    // Block name
    printf("%16s: ", "Name");
    res = ioctl_block_get_name(fd, buf, sizeof(buf));
    if (res >= 0) {
        printf("%s", buf);
    } else if (res != ZX_ERR_NOT_SUPPORTED) {
        rc = static_cast<zx_status_t>(res);
        fprintf(stderr, "ioctl_block_get_name failed: %s\n", zx_status_get_string(rc));
        return rc;
    }
    printf("\n");

    // Block info
    block_info_t info;
    res = ioctl_block_get_info(fd, &info);
    if (res >= 0) {
    } else if (res != ZX_ERR_NOT_SUPPORTED) {
        rc = static_cast<zx_status_t>(res);
        fprintf(stderr, "ioctl_block_get_info failed: %s\n", zx_status_get_string(rc));
        return rc;
    }
    printf("%16s: %" PRIu64 "\n", "Block count", info.block_count);
    printf("%16s: %" PRIu32 "\n", "Block size", info.block_size);
    printf("%16s: %" PRIu32 "\n", "Max transfer", info.max_transfer_size);
    printf("%16s: %08" PRIx32 "\n", "Flags", info.flags);

    // Optional type guid
    printf("%16s: ", "Type GUID");
    uint8_t guid[GUID_LEN];
    res = ioctl_block_get_type_guid(fd, guid, sizeof(guid));
    if (res >= 0) {
        PrintGuid(guid, static_cast<size_t>(res));
    } else if (res != ZX_ERR_NOT_SUPPORTED) {
        rc = static_cast<zx_status_t>(res);
        fprintf(stderr, "ioctl_block_get_type_guid failed: %s\n", zx_status_get_string(rc));
        return rc;
    }
    printf("\n");

    // Optional instance guid
    res = ioctl_block_get_partition_guid(fd, guid, sizeof(guid));
    if (res >= 0) {
        printf("%16s: ", "Instance GUID");
        PrintGuid(guid, static_cast<size_t>(res));
        printf("\n");
    } else if (res != ZX_ERR_NOT_SUPPORTED) {
        rc = static_cast<zx_status_t>(res);
        fprintf(stderr, "ioctl_block_get_partition_guid failed: %s\n", zx_status_get_string(rc));
        return rc;
    }

    return ZX_OK;
}

} // namespace generic
} // namespace blkctl
