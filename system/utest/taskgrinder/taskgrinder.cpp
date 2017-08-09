// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>

#include <zircon/compiler.h>
#include <zircon/status.h>
#include <zircon/syscalls.h>
#include <zircon/syscalls/object.h>
#include <zircon/types.h>
#include <zx/handle.h>
#include <zx/job.h>
#include <fbl/auto_lock.h>
#include <fbl/mutex.h>

#include "vector.h"

/*
threads creating and removing children (by closing final handles)
thread walking children, getting handles, calling INFO on them, closing them

handle to deep leaf job, none in between, let the whole thing collapse

hard part is seeing if we actually hit any corner cases
*/

#define RETURN_IF_ERROR(x)                                             \
    do {                                                               \
        zx_status_t TRY_status__ = (x);                                \
        if (TRY_status__ != ZX_OK) {                                   \
            fprintf(stderr, "%s:%d: %s failed: %s (%d)\n",             \
                    __func__, __LINE__, #x,                            \
                    zx_status_get_string(TRY_status__), TRY_status__); \
            return TRY_status__;                                       \
        }                                                              \
    } while (0)

typedef Vector<zx::handle> HandleVector;

bool is_good_handle(zx_handle_t h) {
    zx_info_handle_basic_t info;
    zx_status_t s = zx_object_get_info(h, ZX_INFO_HANDLE_BASIC,
                                       &info, sizeof(info), nullptr, nullptr);
    return s == ZX_OK;
}

void tvtest() {
    static constexpr size_t kNumHandles = 16;
    zx_handle_t raw_handles[kNumHandles];
    {
        Vector<zx::handle> handles;
        for (size_t i = 0; i < kNumHandles; i++) {
            zx::handle h;
            zx_status_t s = zx_event_create(0u, h.reset_and_get_address());
            if (s != ZX_OK) {
                fprintf(stderr, "Can't create event %zu: %d\n", i, s);
                return;
            }
            raw_handles[i] = h.get();
            handles.push_back(fbl::move(h));
            ZX_DEBUG_ASSERT(!h.is_valid());
            ZX_DEBUG_ASSERT(handles[handles.size() - 1].get() == raw_handles[i]);
        }

        for (const auto& h : handles) {
            ZX_DEBUG_ASSERT(is_good_handle(h.get()));
            printf("Good: %" PRIu32 "\n", h.get());
        }
    }

    for (size_t i = 0; i < kNumHandles; i++) {
        ZX_DEBUG_ASSERT(!is_good_handle(raw_handles[i]));
        printf("Bad: %" PRIu32 "\n", raw_handles[i]);
    }
    printf("*** ok ***\n");
}

// Creates child jobs until it hits the bottom, closing intermediate handles
// along the way.
zx_status_t create_max_height_job(zx_handle_t parent_job,
                                  zx::handle* leaf_job) {
    bool first = true;
    zx_handle_t prev_job = parent_job;
    while (true) {
        zx_handle_t child_job;
        zx_status_t s = zx_job_create(prev_job, 0u, &child_job);
        if (s == ZX_ERR_OUT_OF_RANGE) {
            // Hit the max job height.
            leaf_job->reset(prev_job);
            return ZX_OK;
        }

        if (!first) {
            zx_handle_close(prev_job);
        } else {
            first = false;
        }

        if (s != ZX_OK) {
            leaf_job->reset();
            return s;
        }
        //xxx give it a unique name; supply prefix
        zx_object_set_property(child_job, ZX_PROP_NAME, "tg-job", 7);
        prev_job = child_job;
    }
}

// Creates some number of jobs under a parent job, pushing their handles
// onto the output vector.
zx_status_t create_child_jobs(zx_handle_t parent_job, size_t n,
                              HandleVector* out_handles) {
    for (size_t i = 0; i < n; i++) {
        zx::handle child;
        RETURN_IF_ERROR(zx_job_create(parent_job, 0u,
                                      child.reset_and_get_address()));
        //xxx give it a unique name; supply prefix
        child.set_property(ZX_PROP_NAME, "tg-job", 7);
        out_handles->push_back(fbl::move(child));
    }
    return ZX_OK;
}

// Creates some number of processes under a parent job, pushing their handles
// onto the output vector.
zx_status_t create_child_processes(zx_handle_t parent_job, size_t n,
                                   HandleVector* out_handles) {
    for (size_t i = 0; i < n; i++) {
        zx::handle child;
        zx::handle vmar;
        //xxx give it a unique name; supply prefix
        RETURN_IF_ERROR(zx_process_create(parent_job, "tg-proc", 8, 0u,
                                          child.reset_and_get_address(),
                                          vmar.reset_and_get_address()));
        out_handles->push_back(fbl::move(child));
        // Let the VMAR handle close.
    }
    return ZX_OK;
}

// Creates some number of threads under a parent process, pushing their handles
// onto the output vector.
zx_status_t create_child_threads(zx_handle_t parent_process, size_t n,
                                 HandleVector* out_handles) {
    for (size_t i = 0; i < n; i++) {
        zx::handle child;
        //xxx give it a unique name; supply prefix
        RETURN_IF_ERROR(zx_thread_create(parent_process, "tg-thread", 10,
                                         0u, child.reset_and_get_address()));
        out_handles->push_back(fbl::move(child));
    }
    return ZX_OK;
}

//xxx something that keeps creating children, writing handles to a pool?
//xxx another thing that reads handles out of the pool and closes them?
//xxx watch out for synchronization on that pool serializing things
//xxx   could have a thread grab a bunch of handles and then operate on them
//xxx   on its own

//xxx child-walker function: take this process or job, walk its children;
//  maybe recurse

class HandleRegistry {
public:
    void AddJobs(HandleVector* jobs) {
        if (!jobs->empty()) {
            fbl::AutoLock al(&jobs_lock_);
            Merge(jobs, &jobs_, &num_jobs_);
        }
    }

    void AddProcesses(HandleVector* processes) {
        if (!processes->empty()) {
            fbl::AutoLock al(&processes_lock_);
            Merge(processes, &processes_, &num_processes_);
        }
    }

    void AddThreads(HandleVector* threads) {
        if (!threads->empty()) {
            fbl::AutoLock al(&threads_lock_);
            Merge(threads, &threads_, &num_threads_);
        }
    }

    __attribute__((warn_unused_result)) zx_handle_t ReleaseRandomJob() {
        fbl::AutoLock al(&jobs_lock_);
        return ReleaseRandomHandle(&jobs_, &num_jobs_);
    }

    __attribute__((warn_unused_result)) zx_handle_t ReleaseRandomProcess() {
        fbl::AutoLock al(&processes_lock_);
        return ReleaseRandomHandle(&processes_, &num_processes_);
    }

    __attribute__((warn_unused_result)) zx_handle_t ReleaseRandomThread() {
        fbl::AutoLock al(&threads_lock_);
        return ReleaseRandomHandle(&threads_, &num_threads_);
    }

    __attribute__((warn_unused_result)) zx_handle_t ReleaseRandomTask() {
        size_t total = num_jobs_ + num_processes_ + num_threads_;
        const size_t r = rand() % total;
        if (r < num_jobs_) {
            return ReleaseRandomJob();
        } else if (r < num_jobs_ + num_processes_) {
            return ReleaseRandomProcess();
        } else {
            return ReleaseRandomThread();
        }
        //xxx try another if we picked a list with no handles
    }

    //xxx use atomics
    size_t num_jobs() const { return num_jobs_; }
    size_t num_processes() const { return num_processes_; }
    size_t num_threads() const { return num_threads_; }
    size_t num_tasks() const {
        return num_jobs_ + num_processes_ + num_threads_;
    }

private:
    static void Merge(HandleVector* src, HandleVector* dst, size_t* count) {
        const size_t dst_size = dst->size(); // No holes after this index.
        //xxx use an iterator
        size_t di = 0; // Destination index
        for (auto& sit : *src) {
            if (!sit.is_valid()) {
                continue;
            }
            // Look for a hole in the destination.
            while (di < dst_size && (*dst)[di].is_valid()) {
                di++;
            }
            if (di < dst_size) {
                (*dst)[di] = fbl::move(sit);
            } else {
                dst->push_back(fbl::move(sit));
            }
        }
        *count += src->size();
    }

    static __attribute__((warn_unused_result))
    zx_handle_t
    ReleaseRandomHandle(HandleVector* hv, size_t* count) {
        const size_t size = hv->size();
        if (size == 0) {
            return ZX_HANDLE_INVALID;
        }
        const size_t start = rand() % size;
        //xxx use an iterator
        for (size_t i = start; i < size; i++) {
            if ((*hv)[i].is_valid()) {
                (*count)--;
                return (*hv)[i].release();
            }
        }
        for (size_t i = 0; i < start; i++) {
            if ((*hv)[i].is_valid()) {
                (*count)--;
                return (*hv)[i].release();
            }
        }
        return ZX_HANDLE_INVALID;
    }

    mutable fbl::Mutex jobs_lock_;
    HandleVector jobs_; // TA_GUARDED(jobs_lock_);
    size_t num_jobs_; // TA_GUARDED(jobs_lock_);

    mutable fbl::Mutex processes_lock_;
    HandleVector processes_; // TA_GUARDED(processes_lock_);
    size_t num_processes_; // TA_GUARDED(processes_lock_);

    mutable fbl::Mutex threads_lock_;
    HandleVector threads_; // TA_GUARDED(threads_lock_);
    size_t num_threads_; // TA_GUARDED(threads_lock_);
};

//xxx Pass in as a param
static constexpr size_t kMaxTasks = 1000;

#define MTRACE(args...) printf(args)

zx_status_t mutate(HandleRegistry* registry) {
    size_t total = registry->num_tasks();

    enum {
        OP_ADD,
        OP_DELETE,
    } op_class;

    // Randomly pick between add, mutate, and delete.
    if (total < kMaxTasks / 10) {
        op_class = OP_ADD;
    } else if (total > (9 * kMaxTasks) / 10) {
        op_class = OP_DELETE;
    } else {
        op_class = (rand() % 32) < 16 ? OP_ADD : OP_DELETE;
    }

    enum {
        TARGET_JOB,
        TARGET_PROCESS,
        TARGET_THREAD,
    } op_target;
    const size_t r = rand() % 48;
    if (r < 16) {
        op_target = TARGET_JOB;
    } else if (r < 32) {
        op_target = TARGET_PROCESS;
    } else {
        op_target = TARGET_THREAD;
    }

    // Handles that should go into the registry before we return.
    HandleVector jobs;
    HandleVector processes;
    HandleVector threads;

    switch (op_class) {
    case OP_ADD: {
        const size_t num_children = rand() % 5 + 1;
        switch (op_target) {
        case TARGET_JOB: {
            zx_handle_t parent = registry->ReleaseRandomJob();
            if (parent != ZX_HANDLE_INVALID) {
                MTRACE("Create %zu jobs\n", num_children);
                jobs.push_back(zx::handle(parent));
                create_child_jobs(parent, num_children, &jobs);
                //xxx if creation fails with BAD_STATE, the parent
                //xxx is probably dead; don't put it back in the list
            }
            //xxx chance to create super-deep job
        } break;
        case TARGET_PROCESS: {
            zx_handle_t parent = registry->ReleaseRandomJob();
            if (parent != ZX_HANDLE_INVALID) {
                MTRACE("Create %zu processes\n", num_children);
                jobs.push_back(zx::handle(parent));
                create_child_processes(parent, num_children, &processes);
            }
        } break;
        case TARGET_THREAD: {
            zx_handle_t parent = registry->ReleaseRandomProcess();
            if (parent != ZX_HANDLE_INVALID) {
                MTRACE("Create %zu threads\n", num_children);
                processes.push_back(zx::handle(parent));
                create_child_threads(parent, num_children, &threads);
            }
        } break;
        }
    } break;
    case OP_DELETE: {
        const bool kill = rand() % 32 < 16;
        const bool close = rand() % 32 < 16;
        if (kill || close) {
            zx_handle_t task = registry->ReleaseRandomTask();
            if (task != ZX_HANDLE_INVALID) {
                if (kill) {
                    MTRACE("Kill one\n");
                    zx_task_kill(task);
                }
                if (close) {
                    MTRACE("Close one\n");
                    zx_handle_close(task);
                } else {
                    MTRACE("(Close one)\n");
                    //xxx need to figure out the type so we can put it back.
                    zx_handle_close(task);
                }
            }
        }
    } break;
    }

    registry->AddJobs(&jobs);
    registry->AddProcesses(&processes);
    registry->AddThreads(&threads);

    return ZX_OK;
}

zx_status_t buildup(const zx::handle& root_job) {
    HandleRegistry registry;
    {
        HandleVector jobs;
        jobs.push_back(zx::handle(root_job.get())); //xxx can't let them delete this
        registry.AddJobs(&jobs);
    }
    for (int i = 0; i < 1000; i++) {
        mutate(&registry);
        if (i > 0 && i % 100 == 0) {
            printf("%d mutations. Press a key:\n", i);
            fgetc(stdin);
        }
    }
    printf("Mutations done. Press a key:\n");
    fgetc(stdin);

    printf("Done.\n");
    return ZX_OK;
}

int main(int argc, char** argv) {
    //tvtest();
    zx::handle test_root_job;
    zx_status_t s = zx_job_create(zx_job_default(), 0u,
                                  test_root_job.reset_and_get_address());
    if (s != ZX_OK) {
        return s;
    }
    test_root_job.set_property(ZX_PROP_NAME, "tg-root", 8);
    return buildup(test_root_job);
}
