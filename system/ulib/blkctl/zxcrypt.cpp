// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <blkctl/blkctl.h>
#include <blkctl/command.h>
#include <crypto/secret.h>
#include <zircon/types.h>

#include "zxcrypt.h"

namespace blkctl {
namespace zxcrypt {

using ::zxcrypt::Volume;

constexpr zx::duration kTimeout = zx::sec(3);

zx_status_t ZxcryptCommand::ReadKey(key_slot_t slot, crypto::Secret* out) {
    zx_status_t rc;
    BlkCtl* cmdline = this->cmdline();

    // TODO(security): Add a 'max key len' to Volume when ZX-1130 is resolved.
    size_t key_len = ::zxcrypt::kZx1130KeyLen;
    char prompt[32];
    snprintf(prompt, sizeof(prompt), "key for slot %" PRIu64, slot);
    size_t hex_len = (key_len * 2) + 1;
    char hex[hex_len];
    uint8_t *buf;
    if ((rc = out->Allocate(key_len, &buf)) != ZX_OK ||
        (rc = cmdline->Prompt(prompt, hex, hex_len)) != ZX_OK ||
        (rc = ParseHex(hex, buf, key_len)) != ZX_OK) {
        return rc;
    }

    // TODO(security): ZX-1130
    uint8_t zx1130[::zxcrypt::kZx1130KeyLen] = {0};
    if (out->len() != sizeof(zx1130) || memcmp(out->get(), zx1130, out->len()) != 0) {
        return ZX_ERR_INVALID_ARGS;
    }

    return ZX_OK;
}

zx_status_t Create::Run() {
    zx_status_t rc;
    BlkCtl* cmdline = this->cmdline();

    const char* dev;
    int fd;
    crypto::Secret key;

    if ((rc = cmdline->GetStrArg("device", &dev)) != ZX_OK || (rc = cmdline->ArgsDone()) != ZX_OK ||
        (rc = OpenReadable(dev, &fd)) != ZX_OK || (rc = ReadKey(0, &key)) != ZX_OK ||
        (rc = cmdline->Confirm()) != ZX_OK || (rc = ReopenWritable(&fd)) != ZX_OK) {
        return rc;
    }
    fbl::unique_fd ufd(fd);
    if ((rc = Volume::Create(fbl::move(ufd), key)) != ZX_OK) {
        return rc;
    }

    return ZX_OK;
}

zx_status_t Open::Run() {
    zx_status_t rc;
    BlkCtl* cmdline = this->cmdline();

    const char* dev;
    key_slot_t slot;
    crypto::Secret key;
    int fd;
    fbl::unique_ptr<Volume> volume;
    fbl::unique_fd opened;

    // Unlock the volume before opening it to verify the key is correct.
    if ((rc = cmdline->GetStrArg("device", &dev)) != ZX_OK ||
        (rc = cmdline->GetNumArg("slot", &slot)) != ZX_OK || (rc = cmdline->ArgsDone()) != ZX_OK ||
        (rc = OpenReadable(dev, &fd)) != ZX_OK || (rc = ReadKey(slot, &key)) != ZX_OK) {
        return rc;
    }
    fbl::unique_fd ufd(fd);
    if ((rc = Volume::Unlock(fbl::move(ufd), key, slot, &volume)) != ZX_OK ||
        (rc = volume->Open(kTimeout, &opened)) != ZX_OK) {
        return rc;
    }

    return ZX_OK;
}

zx_status_t Enroll::Run() {
    zx_status_t rc;
    BlkCtl* cmdline = this->cmdline();

    const char* dev;
    key_slot_t slot, new_slot;
    int fd;
    crypto::Secret key, new_key;
    fbl::unique_ptr<Volume> volume;

    if ((rc = cmdline->GetStrArg("device", &dev)) != ZX_OK ||
        (rc = cmdline->GetNumArg("slot", &slot)) != ZX_OK ||
        (rc = cmdline->GetNumArg("new_slot", &new_slot)) != ZX_OK ||
        (rc = cmdline->ArgsDone()) != ZX_OK || (rc = OpenReadable(dev, &fd)) != ZX_OK ||
        (rc = ReadKey(slot, &key)) != ZX_OK || (rc = ReadKey(new_slot, &new_key)) != ZX_OK ||
        (rc = cmdline->Confirm()) != ZX_OK || (rc = ReopenWritable(&fd)) != ZX_OK) {
        return rc;
    }
    fbl::unique_fd ufd(fd);
    if ((rc = Volume::Unlock(fbl::move(ufd), key, slot, &volume)) != ZX_OK ||
        (rc = volume->Enroll(new_key, new_slot)) != ZX_OK) {
        return rc;
    }

    return ZX_OK;
}

zx_status_t Revoke::Run() {
    zx_status_t rc;
    BlkCtl* cmdline = this->cmdline();

    const char* dev;
    key_slot_t slot, old_slot;
    int fd;
    crypto::Secret key;
    fbl::unique_ptr<Volume> volume;

    if ((rc = cmdline->GetStrArg("device", &dev)) != ZX_OK ||
        (rc = cmdline->GetNumArg("slot", &slot)) != ZX_OK ||
        (rc = cmdline->GetNumArg("old_slot", &old_slot)) != ZX_OK ||
        (rc = cmdline->ArgsDone()) != ZX_OK || (rc = OpenReadable(dev, &fd)) != ZX_OK ||
        (rc = ReadKey(slot, &key)) != ZX_OK || (rc = cmdline->Confirm()) != ZX_OK ||
        (rc = ReopenWritable(&fd)) != ZX_OK) {
        return rc;
    }
    fbl::unique_fd ufd(fd);
    if ((rc = Volume::Unlock(fbl::move(ufd), key, slot, &volume)) != ZX_OK ||
        (rc = volume->Revoke(old_slot)) != ZX_OK) {
        return rc;
    }

    return ZX_OK;
}

zx_status_t Shred::Run() {
    zx_status_t rc;
    BlkCtl* cmdline = this->cmdline();

    const char* dev;
    key_slot_t slot;
    int fd;
    crypto::Secret key;
    fbl::unique_ptr<Volume> volume;

    // TODO(security); ZX-1138.  This should also unbind the device.
    if ((rc = cmdline->GetStrArg("device", &dev)) != ZX_OK ||
        (rc = cmdline->GetNumArg("slot", &slot)) != ZX_OK || (rc = cmdline->ArgsDone()) != ZX_OK ||
        (rc = OpenReadable(dev, &fd)) != ZX_OK || (rc = ReadKey(slot, &key)) != ZX_OK ||
        (rc = cmdline->Confirm()) != ZX_OK || (rc = ReopenWritable(&fd)) != ZX_OK) {
        return rc;
    }
    fbl::unique_fd ufd(fd);
    if ((rc = Volume::Unlock(fbl::move(ufd), key, slot, &volume)) != ZX_OK ||
        (rc = volume->Shred()) != ZX_OK) {
        return rc;
    }

    return ZX_OK;
}

} // namespace zxcrypt
} // namespace blkctl
