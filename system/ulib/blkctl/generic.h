// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <blkctl/command.h>

namespace blkctl {
namespace generic {

DEFINE_COMMAND(Help);
DEFINE_COMMAND(List);
DEFINE_COMMAND(Dump);

constexpr const char *kType = nullptr;

constexpr Cmd kCommands[] = {
    {"help", "", "Print this message and exit.", Instantiate<generic::Help>},
    {"ls", "", "List available block devices.", Instantiate<generic::List>},
    {"dump", "<device>", "Dump block device information.", Instantiate<generic::Dump>}};

constexpr size_t kNumCommands = sizeof(kCommands) / sizeof(kCommands[0]);

} // namespace generic
} // namespace blkctl
