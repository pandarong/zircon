// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <blkctl/command.h>
#include <crypto/bytes.h>
#include <zxcrypt/volume.h>

namespace blkctl {
namespace zxcrypt {

using ::zxcrypt::key_slot_t;

// |ZxcryptCommand| extends the base |Command| to be able to prompt for a key from the user.
// TODO(security): ZX-1130.  This eventually should hook into the same authentication flow used to
// get keys
class ZxcryptCommand : public Command {
public:
    ZxcryptCommand(BlkCtl* cmdline) : Command(cmdline) {}

protected:
    // Prompts the user for the key for the given |slot|.
    zx_status_t ReadKey(key_slot_t slot, crypto::Secret* out);
};

DEFINE_DERIVED_COMMAND(ZxcryptCommand, Create);
DEFINE_DERIVED_COMMAND(ZxcryptCommand, Open);
DEFINE_DERIVED_COMMAND(ZxcryptCommand, Enroll);
DEFINE_DERIVED_COMMAND(ZxcryptCommand, Revoke);
DEFINE_DERIVED_COMMAND(ZxcryptCommand, Shred);

constexpr const char* kType = "zxcrypt";

constexpr Cmd kCommands[] = {
    {"create", "<device>", "Creates a new zxcrypt volume with given key in slot 0",
     Instantiate<zxcrypt::Create>},
    {"open", "<device> <slot>", "Unlocks the zxcrypt volume", Instantiate<zxcrypt::Open>},
    {"enroll", "<device> <slot> <new_slot>", "Unlocks and then enrolls a new key slot",
     Instantiate<zxcrypt::Enroll>},
    {"revoke", "<device> <slot> <old_slot>", "Unlocks and then revokes a given key slot",
     Instantiate<zxcrypt::Revoke>},
    {"shred", "<device> <slot>", "Unlocks and then destroys a zxcrypt volume",
     Instantiate<zxcrypt::Shred>},
};

constexpr size_t kNumCommands = sizeof(kCommands) / sizeof(kCommands[0]);

} // namespace zxcrypt
} // namespace blkctl
