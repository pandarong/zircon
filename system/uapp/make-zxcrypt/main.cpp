// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>

#include <fbl/unique_fd.h>
#include <fs-management/fvm.h>
#include <zircon/device/block.h>
#include <zircon/status.h>
#include <zircon/types.h>
#include <zxcrypt/volume.h>

constexpr uint8_t kDeadbeefGUID[GUID_LEN] = {
    0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef};

// Quick and dirty hack to add a zxcrypt volume to debug devccord hang.
int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "error: wrong number of arguments\n");
        fprintf(stderr, "usage: %s /path/to/some/fvm\n", argv[0]);
        return -1;
    }

    fbl::unique_fd fd(open(argv[1], O_RDONLY));
    if (!fd) {
        fprintf(stderr, "error: failed to open '%s'\n", argv[1]);
        return -1;
    }

    // Returns an open fd to the new partition on success, -1 on error.
    alloc_req_t request;
    request.slice_count = 1;
    memcpy(request.type, kDeadbeefGUID, sizeof(request.type));
    memcpy(request.guid, kDeadbeefGUID, sizeof(request.type));
    snprintf(request.name, NAME_LEN, "deadbeef");
    request.flags = 0;
    if (fvm_allocate_partition(fd.get(), &request) < 0) {
        fprintf(stderr, "warning: failed to allocate FVM partition\n");
    }

    char path[PATH_MAX];
    fd.reset(open_partition(kDeadbeefGUID, kDeadbeefGUID, ZX_SEC(3), path));
    if (!fd) {
        fprintf(stderr, "error: failed to FVM partition\n");
        return -1;
    }

    zx_status_t rc;
    crypto::Secret key;
    uint8_t* buf;
    if ((rc = key.Allocate(zxcrypt::kZx1130KeyLen, &buf)) != ZX_OK) {
        fprintf(stderr, "error: failed to allocate key: %s\n", zx_status_get_string(rc));
        return -1;
    }
    memset(buf, 0, zxcrypt::kZx1130KeyLen);

    if ((rc = zxcrypt::Volume::Create(fbl::move(fd), key)) != ZX_OK) {
        fprintf(stderr, "error: failed to create zxcrypt volume: %s\n", zx_status_get_string(rc));
        return -1;
    }

    return 0;
}
