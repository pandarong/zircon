// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <blkctl/blkctl.h>
#include <blkctl/command.h>
#include <fbl/string.h>
#include <fbl/unique_ptr.h>
#include <fbl/vector.h>
#include <zircon/assert.h>
#include <zircon/errors.h>
#include <zircon/types.h>

#include "fvm.h"
#include "generic.h"
#include "ramdisk.h"
#include "zxcrypt.h"

namespace blkctl {
namespace {

#define ADD_CMD_TYPE(T)                                                                            \
    { T::kType, T::kCommands, T::kNumCommands }
struct CmdType {
    const char* type;
    const Cmd* cmds;
    size_t num;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// *** LOOK HERE FIRST! ***
//
// This associates types with subclasses.  To add new block device types, implement a subclass of
// Command that includes:
//   - A constructor that takes an r-value reference to a Command
//   - An array (and length) of CommandImpls that describe individual commands and their arguments.
// Then simply #include the appropriate header and add a DEVICE_TYPE to the list below.
constexpr CmdType kTypes[] = {
    ADD_CMD_TYPE(ramdisk),
    ADD_CMD_TYPE(fvm),
    ADD_CMD_TYPE(zxcrypt),
    // The generic commands should be last, so that various routines use them if no type matches
    ADD_CMD_TYPE(generic),
};
////////////////////////////////////////////////////////////////////////////////////////////////////
constexpr size_t kNumTypes = (sizeof(kTypes) / sizeof(kTypes[0]));

// This option skips confirmation
const char* kForce = "--force";

// The column widths when print usage and/or help messages.
constexpr size_t kLineLen = 80;

} // namespace

// Public methods

void BlkCtl::Usage() const {
    printf("Usage: %s [%s] [<type>] <command> [args...]\n", binname_.c_str(), kForce);
    printf("\nOptions:\n");
    printf("  %8s  %64s\n", kForce, "Skips confirmation prompts; be careful!");
    printf("\nTypes and Commands:\n");
    printf("  In the commands below, <device> may refer to a path in the device tree, or an\n");
    printf("  ID under %s.\n\n", Command::kDevClassBlock);
    for (size_t i = 0; i < kNumTypes; ++i) {
        for (size_t j = 0; j < kTypes[i].num; ++j) {
            const Cmd* cmd = &kTypes[i].cmds[j];
            printf("> %s ", binname_.c_str());
            if (kTypes[i].type) {
                printf("%s ", kTypes[i].type);
            }
            printf("%s %s\n", cmd->name ? cmd->name : "", cmd->args ? cmd->args : "");
            char buf[kLineLen - 4];
            const char* p = cmd->help;
            char* q;
            while (true) {
                ssize_t n = snprintf(buf, sizeof(buf), "%s", p);
                ZX_DEBUG_ASSERT(n >= 0);
                if (static_cast<size_t>(n) < sizeof(buf)) {
                    break;
                }
                if (!(q = strrchr(buf, ' '))) {
                    p += sizeof(buf);
                } else {
                    *q = '\0';
                    p += q - buf + 1;
                }
                printf("    %s\n", buf);
            }
            printf("    %s\n\n", buf);
        }
    }
}

zx_status_t BlkCtl::Execute(int argc, char** argv) {
    zx_status_t rc;

    BlkCtl tmp;
    if ((rc = tmp.Parse(argc, argv)) != ZX_OK) {
        return rc;
    }

    Command* cmd = tmp.cmd();
    return cmd->Run();
}

zx_status_t BlkCtl::Parse(int argc, char** argv, const char* canned) {
    zx_status_t rc;

    if (argc == 0 || !argv) {
        fprintf(stderr, "bad arguments: argc=%d, argv=%p\n", argc, argv);
        return ZX_ERR_INVALID_ARGS;
    }

    // Reset the internal state of the command line
    force_ = false;
    argn_ = 0;
    canned_ = canned;

    // Consume binname
    fbl::AllocChecker ac;
    binname_.Set(argv[0], &ac);
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }
    argv = &argv[1];
    --argc;

    // Consume force_ option, if present
    for (int i = 0; i < argc; ++i) {
        if (force_) {
            argv[i - 1] = argv[i];
        } else if (strcmp(argv[i], kForce) == 0) {
            force_ = true;
        }
    }
    if (force_) {
        --argc;
    }
    args_.reset();

    // Consume type, if present. Ignore the "generic" type.
    size_t type;
    for (type = 0; argc != 0 && type < kNumTypes - 1; ++type) {
        if (strcmp(argv[0], kTypes[type].type) == 0) {
            argv = &argv[1];
            --argc;
            break;
        }
    }

    // Consume command
    if (argc == 0) {
        fprintf(stderr, "missing command\n");
        return ZX_ERR_INVALID_ARGS;
    }
    const char* name = argv[0];
    argv = &argv[1];
    --argc;

    // Consume arguments
    for (int i = 0; i < argc; ++i) {
        fbl::String arg(argv[i], &ac);
        if (!ac.check()) {
            return ZX_ERR_NO_MEMORY;
        }
        args_.push_back(arg, &ac);
        if (!ac.check()) {
            return ZX_ERR_NO_MEMORY;
        }
    }

    // Build a specific command
    rc = ZX_ERR_INVALID_ARGS;
    for (size_t i = 0; i < kTypes[type].num; ++i) {
        const Cmd* cmd = &kTypes[type].cmds[i];
        if (strcmp(name, cmd->name) == 0) {
            rc = cmd->Instantiate(this, &cmd_);
            break;
        }
    }
    if (rc != ZX_OK) {
        fprintf(stderr, "unrecognized command: %s\n", name);
        return rc;
    }

    return ZX_OK;
}

zx_status_t BlkCtl::GetNumArg(const char* argname, uint64_t* out, bool optional) {
    zx_status_t rc;

    const char* arg;
    if ((rc = GetStrArg(argname, &arg, optional)) != ZX_OK) {
        return rc;
    }
    char* endptr = nullptr;
    int64_t tmp = strtoll(arg, &endptr, 0);
    if (*endptr != '\0') {
        fprintf(stderr, "non-numeric value for '%s': %s\n", argname, arg);
        return ZX_ERR_INVALID_ARGS;
    }
    if (tmp < 0) {
        fprintf(stderr, "negative value for '%s': %" PRId64 "\n", argname, tmp);
        return ZX_ERR_INVALID_ARGS;
    }

    *out = static_cast<uint64_t>(tmp);
    return ZX_OK;
}

zx_status_t BlkCtl::GetStrArg(const char* argname, const char** out, bool optional) {
    if (argn_ < args_.size()) {
        *out = args_[argn_].c_str();
        ++argn_;
        return ZX_OK;
    }
    if (optional) {
        return ZX_ERR_NOT_FOUND;
    }
    fprintf(stderr, "missing required argument: %s\n", argname);
    return ZX_ERR_INVALID_ARGS;
}

zx_status_t BlkCtl::UngetArgs(size_t n) {
    argn_ = argn_ < n ? 0 : argn_ - n;
    return ZX_OK;
}

zx_status_t BlkCtl::ArgsDone() const {
    if (argn_ < args_.size()) {
        fprintf(stderr, "too many arguments\n");
        return ZX_ERR_INVALID_ARGS;
    }
    return ZX_OK;
}

zx_status_t BlkCtl::Confirm() const {
    if (force_) {
        return ZX_OK;
    }
    printf("About to commit changes to disk.  Are you sure? [y/N] ");
    fflush(stdout);
    int c = fgetc(stdin);
    printf("\n");
    switch (c) {
    case 'y':
    case 'Y':
        return ZX_OK;
    default:
        return ZX_ERR_CANCELED;
    }
}

zx_status_t BlkCtl::Prompt(const char* prompt, char* s, size_t n) {
    if (!canned_) {
        printf("Enter %s: ", prompt);
        fflush(stdout);
    }
    for (size_t i = 0; i < n;) {
        int c = canned_ ? *canned_++ : fgetc(stdin);
        if (c == 0x7f && i != 0) {
            printf("\x1b[D\x1b[K");
            fflush(stdout);
            --i;
        } else if (c == '\0' || c == '\n' || c == '\r') {
            s[i] = '\0';
            printf("\n");
            break;
        } else if (!iscntrl(c)) {
            printf("%c", c);
            fflush(stdout);
            s[i] = static_cast<char>(c);
            ++i;
        }
    }

    return ZX_OK;
}

} // namespace blkctl
