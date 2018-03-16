// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <fbl/macros.h>
#include <fbl/string.h>
#include <fbl/unique_fd.h>
#include <fbl/unique_ptr.h>
#include <fbl/alloc_checker.h>
#include <zircon/types.h>

namespace blkctl {

class BlkCtl;

// |blkctl::Command| is the base class for device-specific commands.  It handles common tasks like
// argument parsing and help and error message printing, allowing derived classes to focus on
// implementing device-specific interfaces, via implementations of |PrintCommand|, |Run|, and
class Command {
public:
    // Location of block device aliases
    static constexpr const char* kDevClassBlock = "/dev/class/block";

    virtual ~Command();

    const char * devname() const { return devname_.c_str(); }

    // Execute the command.  See subclasses for specific behavior.
    virtual zx_status_t Run() { return ZX_ERR_NOT_SUPPORTED; }

protected:
    explicit Command(BlkCtl* cmdline) : cmdline_(cmdline) {}

    BlkCtl* cmdline() { return cmdline_; }

    // Convenience functions to create a random GUIDs, parse GUIDs from strings, and print GUIDs.
    static void GenerateGuid(uint8_t* out, size_t out_len);
    static zx_status_t ParseGuid(const char* in, uint8_t* out, size_t out_len);
    static void PrintGuid(const uint8_t* guid, size_t guid_len);

    // Convenience functions to get the successive arguments.  If the next argument is of the wrong
    // type, or is missing and the optional flag is not set, it will return |ZX_ERR_INVALID_ARGS|.
    // If the argument is missing, but |optional| is true, it will return |ZX_ERR_NOT_FOUND|.
    zx_status_t GetFdArg(const char* argname, int* out_fd);
    zx_status_t GetNumArg(const char* argname, uint64_t* out, bool optional = false);
    zx_status_t GetStrArg(const char* argname, const char** out, bool optional = false);

    // Open the device as read-only and return a file descriptor to it via |out|.
    zx_status_t OpenReadable(const char* dev, int* out);

    // Reopen the file descriptor to the device as read/write and returns it via |out|.
    zx_status_t ReopenWritable(int* out);

private:
    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Command);

    BlkCtl* cmdline_;
    // A file descriptor to the open device.
    fbl::unique_fd devfd_;
    // The name of the device being controlled.
    fbl::String devname_;
};

// The remainder of this file is machinery to make it easy to add new commands.  To add new
// commands, simply create a namespace representing the type of commands and use the DEFINE_COMMAND
// macro, followed by an array of Cmd elements, e.g.
//
//   namespace blkctl {
//   namespace example {
//
//   DEFINE_COMMAND(Foo);
//   DEFINE_COMMAND(Bar);
//
//   constexpr const char *kType = "example";
//   constexpr const Cmd kCommands[] = {
//       {"foo", "<device>", "Does the foo-y thing", Instantiate<Foo>},
//       {"bar", "", "Does an extra bar-ish thing", Instantiate<Bar>},
//   };
//   constexpr size_t kNumCommands = sizeof(kCommands)/sizeof(kCommands[0]);
//
//   }
//   }
//
// And then implement Foo::Run() and Bar::Run(). This will make BlkCtl::Parse recognize the
// following commands:
//   > blkctl example foo <device>
//   > blkctl example bar

// This macro can be used to define new command functors with the given |Name|.
#define DEFINE_COMMAND(Name)                                                                       \
    class Name : public Command {                                                                  \
    public:                                                                                        \
        Name(BlkCtl* cmdline) : Command(cmdline) {}                                               \
        zx_status_t Run() override;                                                                \
    }

// |Cmd| describes a command and provides a way to build the command functor.  Specific command
// types must provide an array of these named |kCommands| with length |kNumCommands|.
struct Cmd {
    // The name of the command, e.g. "ls".
    const char* name;
    // The names of the arguments, e.g. "<device> <name>".  May be null or the empty string.
    const char* args;
    // A descriptive message used when printing usage.
    const char* help;
    // A pointer-to-member-function that performs the command.
    zx_status_t (*Instantiate)(BlkCtl* cmdline, fbl::unique_ptr<Command>* out);
};

// Create a functor of the given type.  Use as |Instantiate<Name>| for the last field of |Cmd|.
template <typename C>
static zx_status_t Instantiate(BlkCtl* cmdline, fbl::unique_ptr<Command>* out) {
    static_assert(fbl::is_base_of<Command, C>::value, "must derive from Command");
    ZX_DEBUG_ASSERT(out);
    fbl::AllocChecker ac;
    fbl::unique_ptr<Command> cmd(new (&ac) C(cmdline));
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }
    *out = fbl::move(cmd);

    return ZX_OK;
}

} // namespace blkctl
