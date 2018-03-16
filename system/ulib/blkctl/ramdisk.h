// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <blkctl/command.h>

namespace blkctl {
namespace ramdisk {

DEFINE_COMMAND(Init);
DEFINE_COMMAND(Destroy);

constexpr const char* kType = "ramdisk";

constexpr Cmd kCommands[] = {
    {"init", "<block_size> <block_count>", "Creates a new ramdisk", Instantiate<ramdisk::Init>},
    {"destroy", "<device>", "Destroys the ramdisk", Instantiate<ramdisk::Destroy>},
};

constexpr size_t kNumCommands = sizeof(kCommands) / sizeof(kCommands[0]);

} // namespace ramdisk
} // namespace blkctl
