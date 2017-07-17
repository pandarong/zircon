// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//xxx this should be some kind of service, maybe a device
/*xxx
Wish list:
- Notification when job importances change (permissions can be tough)
- Notification on job creation/death
  - Process creation/death would be nice too
  - Could be a job-level channel that watches for immediate children
    or all descendants. Kinda looks like inotify, if there's a namespace
    for jobs

A bunch of this can happen down in the kernel if this userspace stuff goes away
*/

#include <inttypes.h>
#include <string.h>

#include <task-utils/walker.h>
#include <zircon/status.h>
#include <zircon/syscalls.h>
#include <zircon/syscalls/object.h>
#include <zxcpp/new.h>

#include "canned_jobs.h"
#include "job.h"
#include "macros.h"
#include "resources.h"

using fbl::unique_ptr;

// Builds a list of all jobs in the system.
class JobWalker final : private TaskEnumerator {
public:
    static zx_status_t BuildList(Job::List* jobs) {
        fbl::AllocChecker ac;
        unique_ptr<const Job* []> stack(new (&ac) const Job*[kMaxDepth]);
        if (!ac.check()) {
            return ZX_ERR_NO_MEMORY;
        }
        zx_status_t s =
            JobWalker(jobs, fbl::move(stack)).WalkRootJobTree();
        if (s != ZX_OK) {
            return s;
        }
        return ZX_OK;
    }

private:
    JobWalker(Job::List* jobs, unique_ptr<const Job* []> stack)
        : jobs_(jobs), stack_(fbl::move(stack)) {
        memset(stack_.get(), 0, kMaxDepth);
    }

    zx_status_t OnJob(int depth, zx_handle_t handle,
                      zx_koid_t koid, zx_koid_t parent_koid) override {
        ZX_ASSERT(depth >= 0);
        ZX_ASSERT(depth < kMaxDepth);
        // Make sure our entry on the stack won't point to a stale entry
        // if we fail before inserting ourselves.
        stack_[depth] = nullptr;
        // Clear a few more entries to highlight any bugs in this code.
        stack_[depth + 1] = nullptr;
        stack_[depth + 2] = nullptr;
        stack_[depth + 3] = nullptr;

        const Job* parent;
        if (depth == 0) {
            // Root job.
            parent = nullptr;
        } else {
            parent = stack_[depth - 1];
            ZX_ASSERT(parent != nullptr);
        }

        zx_handle_t dup;
        zx_status_t s = zx_handle_duplicate(handle, ZX_RIGHT_SAME_RIGHTS, &dup);
        if (s != ZX_OK) {
            fprintf(stderr,
                    "ERROR: duplicating handle for job %" PRIu64 ": %s (%d)\n",
                    koid, zx_status_get_string(s), s);
            dup = ZX_HANDLE_INVALID;
        }

        // Read some object properties.
        // TODO: Don't stop walking the tree if one job is bad; it might have
        // just died. Watch out for the walker visiting its children, though;
        // maybe put a tombstone in the stack.
        ZX_DEBUG_ASSERT(handle != ZX_HANDLE_INVALID);
        char name[ZX_MAX_NAME_LEN];
        RETURN_IF_ERROR(zx_object_get_property(dup, ZX_PROP_NAME,
                                               name, sizeof(name)));
        uint32_t importance;
        RETURN_IF_ERROR(zx_object_get_property(dup, ZX_PROP_JOB_IMPORTANCE,
                                               &importance, sizeof(uint32_t)));

        unique_ptr<Job> job;
        RETURN_IF_ERROR(Job::Create(koid, dup, name, importance, parent, &job));

        // Push ourselves on the stack so our children can find us.
        stack_[depth] = job.get();

        jobs_->push_back(fbl::move(job));
        return ZX_OK;
    }

    bool has_on_job() const override { return true; }

    Job::List* const jobs_;

    static constexpr int kMaxDepth = 128;
    unique_ptr<const Job* []> stack_; // kMaxDepth entries
};

#define FAKE_RANKING 0
#if FAKE_RANKING
#include "fake_syscalls.h"
#undef zx_job_set_relative_importance
#define zx_job_set_relative_importance _fake_job_set_relative_importance
#else // !FAKE_RANKING
#define dump_importance_list(...) ((void)0)
#endif // !FAKE_RANKING

static zx_status_t do_job_stuff() {
    Job::List jobs;
    RETURN_IF_ERROR(JobWalker::BuildList(&jobs));
    sort_jobs_by_importance_key(&jobs);

    zx_handle_t root_resource;
    RETURN_IF_ERROR(get_root_resource(&root_resource));

    zx_handle_t less_important_job = ZX_HANDLE_INVALID;
    for (const auto& job : jobs) {
        printf("+ k:%" PRIu64 " [%-*s] |i=%02x, c=%02x| %s\n",
               job.koid(), ZX_MAX_NAME_LEN, job.name(),
               job.importance(),
               job.capped_importance(),
               job.importance_key());
        RETURN_IF_ERROR(zx_job_set_relative_importance(
            root_resource, job.handle(), less_important_job));
        less_important_job = job.handle();
    }

    dump_importance_list();
    return ZX_OK;
}

int main(int argc, char** argv) {
    //xxx add an arg to create these jobs, and to dump the list.
    //xxx though in the long run this will be a service/device.
    unique_ptr<JobStack> jobs; // Keeps job handles alive.
    create_test_jobs_under(zx_job_default(), &jobs);
    zx_status_t s = do_job_stuff();
    if (s != ZX_OK) {
        fprintf(stderr, "Ranking failed: %s (%d)\n",
                zx_status_get_string(s), s);
        return 1;
    }
    return 0;
}
