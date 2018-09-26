// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#pragma once

#include <kernel/event.h>
#include <kernel/thread.h>
#include <fbl/macros.h>
#include <zircon/types.h>

class VmWorker {
public:
    using VmWorkerRoutine = zx_time_t (*)(void* arg);

    VmWorker() = default;

    void Run(const char* name, VmWorkerRoutine routine, void* arg);
    void Signal();

private:
    DISALLOW_COPY_ASSIGN_AND_MOVE(VmWorker);

    void Loop();

    VmWorkerRoutine routine_ = nullptr;
    void* arg_ = nullptr;

    thread_t* thread_ = nullptr;
    Event event_ { EVENT_FLAG_AUTOUNSIGNAL };
};
