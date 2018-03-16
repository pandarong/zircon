// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

#include <blkctl/blkctl.h>
#include <blkctl/command.h>
#include <fs-management/fvm.h>
#include <fs-management/ramdisk.h>
#include <fvm/fvm.h>
#include <lib/zx/time.h>
#include <zircon/device/block.h>
#include <zircon/device/device.h>
#include <zircon/errors.h>
#include <zircon/status.h>
#include <zircon/types.h>

#include "fvm.h"
#include "generic.h"

namespace blkctl {
namespace fvm {
namespace {

bool SupportsFvmQuery(int fd) {
    if (fd < 0) {
        return false;
    }
    fvm_info_t info;
    memset(&info, 0, sizeof(info));
    return ioctl_block_fvm_query(fd, &info) != ZX_ERR_NOT_SUPPORTED;
}

bool SupportsFvmVSliceQuery(int fd) {
    if (fd < 0) {
        return false;
    }
    query_request_t request;
    query_response_t response;
    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    return ioctl_block_fvm_vslice_query(fd, &request, &response) != ZX_ERR_NOT_SUPPORTED;
}

zx_status_t CheckFvm(int fd) {
    if (!SupportsFvmQuery(fd) && !SupportsFvmVSliceQuery(fd)) {
        fprintf(stderr, "device does not appear to be an FVM volume or partition\n");
        return ZX_ERR_INVALID_ARGS;
    }
    return ZX_OK;
}

zx_status_t CheckFvmVolume(int fd) {
    if (!SupportsFvmQuery(fd)) {
        fprintf(stderr, "device does not appear to be an FVM volume\n");
        return ZX_ERR_INVALID_ARGS;
    }
    return ZX_OK;
}

zx_status_t CheckFvmPartition(int fd) {
    if (!SupportsFvmVSliceQuery(fd)) {
        fprintf(stderr, "device does not appear to be an FVM partition\n");
        return ZX_ERR_INVALID_ARGS;
    }
    return ZX_OK;
}

} // namespace

zx_status_t Init::Run() {
    zx_status_t rc;
    ssize_t res;
    BlkCtl* cmdline = this->cmdline();

    const char* dev;
    int fd;
    size_t slice_size;
    if ((rc = cmdline->GetStrArg("device", &dev)) != ZX_OK ||
        (rc = cmdline->GetNumArg("slice_size", &slice_size)) != ZX_OK ||
        (rc = cmdline->ArgsDone()) != ZX_OK || (rc = OpenReadable(dev, &fd)) != ZX_OK) {
        return rc;
    }
    char path[PATH_MAX];
    if ((res = ioctl_device_get_topo_path(fd, path, sizeof(path))) < 0) {
        rc = static_cast<zx_status_t>(res);
        fprintf(stderr, "failed to get topological path: %s\n", zx_status_get_string(rc));
        return rc;
    }
    if ((rc = cmdline->Confirm()) != ZX_OK || (rc = ReopenWritable(&fd)) != ZX_OK) {
        return rc;
    }
    if ((rc = fvm_init(fd, slice_size)) != ZX_OK) {
        fprintf(stderr, "fvm_init failed: %s\n", zx_status_get_string(rc));
        return rc;
    }
    if ((res = ioctl_device_bind(fd, kDriver, strlen(kDriver))) < 0) {
        rc = static_cast<zx_status_t>(res);
        fprintf(stderr, "could not bind fvm driver: %s\n", zx_status_get_string(rc));
        return rc;
    }
    char name[PATH_MAX];
    snprintf(name, sizeof(name), "%s/fvm", path);
    if (wait_for_device(name, zx::sec(3).get()) != 0) {
        fprintf(stderr, "timed out waiting for fvm driver to bind\n");
        return ZX_ERR_TIMED_OUT;
    }
    printf("%s created\n", name);
    return ZX_OK;
}

zx_status_t Dump::Run() {
    zx_status_t rc;
    ssize_t res;
    BlkCtl* cmdline = this->cmdline();

    const char* dev;
    int fd;
    fvm_info_t info;
    if ((rc = cmdline->GetStrArg("device", &dev)) != ZX_OK || (rc = cmdline->ArgsDone()) != ZX_OK ||
        (rc = OpenReadable(dev, &fd)) != ZX_OK || (rc = CheckFvm(fd)) != ZX_OK) {
        return rc;
    }
    if ((res = ioctl_block_fvm_query(fd, &info)) < 0) {
        rc = static_cast<zx_status_t>(res);
        fprintf(stderr, "ioctl_block_fvm_query failed: %s\n", zx_status_get_string(rc));
        return rc;
    }
    generic::Dump baseCmd(cmdline);
    if ((rc = cmdline->UngetArgs(1)) != ZX_OK || (rc = baseCmd.Run()) != ZX_OK) {
        return rc;
    }
    printf("%16s: %zu\n", "FVM slice size", info.slice_size);
    printf("%16s: %zu\n", "FVM slice count", info.vslice_count);
    return ZX_OK;
}

zx_status_t Add::Run() {
    zx_status_t rc;
    ssize_t res;
    BlkCtl* cmdline = this->cmdline();

    const char* dev;
    int fd;
    alloc_req_t request;
    memset(&request, 0, sizeof(request));
    const char* name;
    if ((rc = cmdline->GetStrArg("device", &dev)) != ZX_OK ||
        (rc = cmdline->GetStrArg("name", &name)) != ZX_OK ||
        (rc = cmdline->GetNumArg("slices", &request.slice_count)) != ZX_OK) {
        return rc;
    }
    const char* guid;

    rc = cmdline->GetStrArg("guid", &guid, true /* optional */);

    if (rc == ZX_ERR_NOT_FOUND) {
        GenerateGuid(request.type, sizeof(request.type));
    } else if (rc != ZX_OK || (rc = ParseGuid(guid, request.type, sizeof(request.type))) != ZX_OK) {
        return rc;
    }
    if ((rc = cmdline->ArgsDone()) != ZX_OK ||
        (rc = OpenReadable(dev, &fd)) != ZX_OK || (rc = CheckFvmVolume(fd)) != ZX_OK ||
        (rc = cmdline->Confirm()) != ZX_OK || (rc = ReopenWritable(&fd)) != ZX_OK) {
        return rc;
    }

    GenerateGuid(request.guid, sizeof(request.guid));
    if ((res = ioctl_block_fvm_alloc_partition(fd, &request)) < 0) {
        rc = static_cast<zx_status_t>(res);
        fprintf(stderr, "ioctl_block_fvm_alloc_partition failed: %s\n", zx_status_get_string(rc));
        return rc;
    }
    printf("added partition '%s' as ", name);
    PrintGuid(request.guid, sizeof(request.guid));
    printf("\n");
    return ZX_OK;
}

zx_status_t Query::Run() {
    zx_status_t rc;
    ssize_t res;
    BlkCtl* cmdline = this->cmdline();

    const char* dev;
    int fd;
    if ((rc = cmdline->GetStrArg("device", &dev)) != ZX_OK || (rc = cmdline->ArgsDone()) != ZX_OK ||
        (rc = OpenReadable(dev, &fd)) != ZX_OK || (rc = CheckFvmPartition(fd)) != ZX_OK) {
        return rc;
    }
    fvm_info_t info;
    if ((res = ioctl_block_fvm_query(fd, &info)) < 0) {
        rc = static_cast<zx_status_t>(res);
        fprintf(stderr, "ioctl_block_fvm_query failed: %s\n", zx_status_get_string(rc));
        return rc;
    }
    query_request_t request;
    query_response_t response;
    request.count = 1;
    printf("Allocated ranges:\n");
    size_t end = 0;
    for (size_t off = 0; off < info.vslice_count; off = end) {
        request.vslice_start[0] = off;
        if ((res = ioctl_block_fvm_vslice_query(fd, &request, &response)) < 0) {
            rc = static_cast<zx_status_t>(res);
            fprintf(stderr, "ioctl_block_fvm_vslice_query failed: %s\n", zx_status_get_string(rc));
            return rc;
        }
        ZX_DEBUG_ASSERT(response.count == 1);
        ZX_DEBUG_ASSERT(response.vslice_range[0].count != 0);
        end = off + response.vslice_range[0].count;
        if (response.vslice_range[0].allocated) {
            printf("  <0x%016" PRIx64 ", 0x%016" PRIx64 ">:   slices %zu through %zu\n",
                   static_cast<uint64_t>(off * info.slice_size),
                   static_cast<uint64_t>((end * info.slice_size) - 1), off, end);
        }
    }
    return ZX_OK;
}

zx_status_t Extend::Run() {
    zx_status_t rc;
    ssize_t res;
    BlkCtl* cmdline = this->cmdline();

    const char* dev;
    int fd;
    extend_request_t request;
    if ((rc = cmdline->GetStrArg("device", &dev)) != ZX_OK ||
        (rc = cmdline->GetNumArg("start", &request.offset)) != ZX_OK ||
        (rc = cmdline->GetNumArg("length", &request.length)) != ZX_OK ||
        (rc = cmdline->ArgsDone()) != ZX_OK || (rc = OpenReadable(dev, &fd)) != ZX_OK ||
        (rc = CheckFvmPartition(fd)) != ZX_OK) {
        return rc;
    }
    if ((rc = cmdline->Confirm()) != ZX_OK || (rc = ReopenWritable(&fd)) != ZX_OK) {
        return rc;
    }
    if ((res = ioctl_block_fvm_extend(fd, &request)) < 0) {
        fprintf(stderr, "ioctl_block_fvm_extend failed: %s\n", zx_status_get_string(rc));
        rc = static_cast<zx_status_t>(res);
        return rc;
    }
    printf("partition extended\n");
    return ZX_OK;
}

zx_status_t Shrink::Run() {
    zx_status_t rc;
    ssize_t res;
    BlkCtl* cmdline = this->cmdline();

    const char* dev;
    int fd;
    extend_request_t request;
    if ((rc = cmdline->GetStrArg("device", &dev)) != ZX_OK ||
        (rc = cmdline->GetNumArg("start", &request.offset)) != ZX_OK ||
        (rc = cmdline->GetNumArg("length", &request.length)) != ZX_OK ||
        (rc = cmdline->ArgsDone()) != ZX_OK || (rc = OpenReadable(dev, &fd)) != ZX_OK ||
        (rc = CheckFvmPartition(fd)) != ZX_OK) {
        return rc;
    }
    if ((rc = cmdline->Confirm()) != ZX_OK || (rc = ReopenWritable(&fd)) != ZX_OK) {
        return rc;
    }
    if ((res = ioctl_block_fvm_shrink(fd, &request)) < 0) {
        rc = static_cast<zx_status_t>(res);
        fprintf(stderr, "ioctl_block_fvm_shrink failed: %s\n", zx_status_get_string(rc));
        return rc;
    }
    printf("partition shrunk\n");
    return ZX_OK;
}

zx_status_t Remove::Run() {
    zx_status_t rc;
    ssize_t res;
    BlkCtl* cmdline = this->cmdline();

    const char* dev;
    int fd;
    if ((rc = cmdline->GetStrArg("device", &dev)) != ZX_OK || (rc = cmdline->ArgsDone()) != ZX_OK ||
        (rc = OpenReadable(dev, &fd)) != ZX_OK || (rc = CheckFvmPartition(fd)) != ZX_OK) {
        return rc;
    }
    if ((rc = cmdline->Confirm()) != ZX_OK || (rc = ReopenWritable(&fd)) != ZX_OK) {
        return rc;
    }
    if ((res = ioctl_block_fvm_destroy_partition(fd)) < 0) {
        rc = static_cast<zx_status_t>(res);
        fprintf(stderr, "ioctl_block_fvm_destroy_partition failed: %s\n", zx_status_get_string(rc));
        return rc;
    }
    printf("partition removed\n");
    return ZX_OK;
}

zx_status_t Destroy::Run() {
    zx_status_t rc;
    ssize_t res;
    BlkCtl* cmdline = this->cmdline();

    const char* dev;
    int fd;
    if ((rc = cmdline->GetStrArg("device", &dev)) != ZX_OK || (rc = cmdline->ArgsDone()) != ZX_OK ||
        (rc = OpenReadable(dev, &fd)) != ZX_OK) {
        return rc;
    }
    char path[PATH_MAX];
    constexpr const char* suffix = "/fvm";
    ZX_DEBUG_ASSERT(strlen(suffix) < sizeof(path));
    size_t path_max = sizeof(path) - (strlen(suffix) + 1);
    if ((res = ioctl_device_get_topo_path(fd, path, path_max)) < 0) {
        rc = static_cast<zx_status_t>(res);
        fprintf(stderr, "failed to get topological path: %s\n", zx_status_get_string(rc));
        return rc;
    }
    strcat(path, suffix); // Safe to due to size limit above
    fbl::unique_fd fvm_fd(open(path, O_RDONLY));
    if ((rc = CheckFvmVolume(fvm_fd.get())) != ZX_OK) {
        return rc;
    }
    fvm_fd.reset();
    if ((rc = cmdline->Confirm()) != ZX_OK || (rc = ReopenWritable(&fd)) != ZX_OK) {
        return rc;
    }
    if ((rc = fvm_destroy(devname())) != ZX_OK) {
        fprintf(stderr, "fvm_destroy failed\n");
        return rc;
    }
    if ((res = ioctl_block_rr_part(fd)) < 0) {
        rc = static_cast<zx_status_t>(res);
        fprintf(stderr, "failed to unbind FVM driver: %s\n", zx_status_get_string(rc));
        return rc;
    }
    printf("FVM volume metadata destroyed\n");
    return ZX_OK;
}

} // namespace fvm
} // namespace blkctl
