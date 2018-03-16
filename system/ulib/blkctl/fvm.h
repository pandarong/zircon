// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <blkctl/command.h>

namespace blkctl {
namespace fvm {

DEFINE_COMMAND(Init);
DEFINE_COMMAND(Dump);
DEFINE_COMMAND(Add);
DEFINE_COMMAND(Query);
DEFINE_COMMAND(Extend);
DEFINE_COMMAND(Shrink);
DEFINE_COMMAND(Remove);
DEFINE_COMMAND(Destroy);

constexpr const char* kType = "fvm";

constexpr Cmd kCommands[] = {
    {"init", "<device> <slice_size>", "Format a block device to be an empty FVM volume.", Instantiate<Init>},
    {"dump", "<device>", "Dump block device and FVM volume information.", Instantiate<Dump>},
    {"add", "<device> <name> <slices> [type-guid]", "Allocates a new partition in the FVM volume.", Instantiate<Add>},
    {"query", "<device>", "List ranges of allocated slices.", Instantiate<Query>},
    {"extend", "<device> <start> <length>", "Allocates slices for the partition.", Instantiate<Extend>},
    {"shrink", "<device> <start> <length>", "Free slices from the partition.", Instantiate<Shrink>},
    {"remove", "<device>", "Removes partition from the FVM volume.", Instantiate<Remove>},
    {"destroy", "<device>", "Overwrites and unbinds an FVM volume.", Instantiate<Destroy>},
};
constexpr size_t kNumCommands = sizeof(kCommands) / sizeof(kCommands[0]);

constexpr const char *kDriver = "/boot/driver/fvm.so";

} // namespace fvm
} // namespace blkctl
