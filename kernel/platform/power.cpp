// Copyright 2016 The Fuchsia Authors
// Copyright (c) 2012 Google, Inc.
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <debug.h>
#include <err.h>
#include <kernel/thread.h>
#include <lib/console.h>
#include <platform.h>
#include <platform/debug.h>
#include <stdio.h>
#include <zircon/compiler.h>

#if WITH_LIB_CONSOLE

#include <lib/console.h>

static int cmd_reboot(int argc, const cmd_args* argv, uint32_t flags) {
    platform_halt(HALT_ACTION_REBOOT, HALT_REASON_SW_RESET);
    return 0;
}

static int cmd_poweroff(int argc, const cmd_args* argv, uint32_t flags) {
    platform_halt(HALT_ACTION_SHUTDOWN, HALT_REASON_SW_RESET);
    return 0;
}

STATIC_COMMAND_START
#if LK_DEBUGLEVEL > 1
STATIC_COMMAND_MASKED("reboot", "soft reset", &cmd_reboot, CMD_AVAIL_ALWAYS)
STATIC_COMMAND_MASKED("poweroff", "powerdown", &cmd_poweroff, CMD_AVAIL_ALWAYS)
#endif
STATIC_COMMAND_END(platform_power);

#endif
