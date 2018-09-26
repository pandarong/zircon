// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include "vm_worker.h"

#include <assert.h>
#include <kernel/event.h>

void VmWorker::Run(const char* name, VmWorkerRoutine routine, void* arg) {
    DEBUG_ASSERT(routine_ == nullptr && arg_ == nullptr);

    routine_ = routine;
    arg_ = arg;

    // wrapper routine
    auto wrapper = [](void* arg) -> int {
        VmWorker* worker = static_cast<VmWorker*>(arg);
        worker->Loop();
        return ZX_OK;
    };

    // initialize the thread
    dprintf(INFO, "VM Worker: starting '%s' worker thread\n", name);
    thread_ = thread_create(name, wrapper, this, HIGH_PRIORITY);
    DEBUG_ASSERT(thread_);
    thread_resume(thread_);
}

void VmWorker::Signal() {
    event_.Signal(ZX_OK);
}

void VmWorker::Loop() {
    for (;;) {
        zx_time_t wait_time = routine_(arg_);

        event_.Wait(wait_time);
    }
}
