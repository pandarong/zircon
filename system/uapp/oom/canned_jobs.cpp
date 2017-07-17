// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "canned_jobs.h"

#include <string.h>

#include <fbl/array.h>
#include <fbl/unique_ptr.h>
#include <zircon/assert.h>
#include <zircon/status.h>
#include <zircon/types.h>
#include <zx/handle.h>
#include <zx/job.h>
#include <zx/task.h>
#include <zxcpp/new.h>

#include "macros.h"

using fbl::unique_ptr;

JobStack::JobStack(zx::job* array, size_t count)
    : Array(array, count), next_(0) {}

JobStack* JobStack::Create(size_t count) {
    fbl::AllocChecker ac;
    fbl::AllocChecker ac2;
    JobStack* jobs = new (&ac) JobStack(new (&ac2) zx::job[count], count);
    ZX_ASSERT(ac.check());
    ZX_ASSERT(ac2.check());
    return jobs;
}

// Swaps the job onto the stack.
void JobStack::push(zx::job* job) {
    // Will assert on overrun.
    (*this)[next_].swap(*job);
    next_++;
}

// Returns the most-recently-pushed job.
zx::job& JobStack::top() const {
    ZX_ASSERT(next_ > 0);
    return (*this)[next_ - 1];
}

namespace {
zx_status_t set_job_name(const zx::job& job, const char* name) {
    char buf[ZX_MAX_NAME_LEN];
    strncpy(buf, name, sizeof(buf));
    return job.set_property(ZX_PROP_NAME, buf, sizeof(buf));
}

zx_status_t set_job_importance(const zx::job& job, zx_job_importance_t importance) {
    uint32_t value = static_cast<uint32_t>(importance);
    return job.set_property(ZX_PROP_JOB_IMPORTANCE, &value, sizeof(value));
}

zx_status_t create_job(const zx::job& parent,
                       const char* name, zx_job_importance_t importance,
                       JobStack* jobs) {
    zx::job job;
    RETURN_IF_ERROR(zx::job::create(parent.get(), 0, &job));
    RETURN_IF_ERROR(set_job_name(job, name));
    RETURN_IF_ERROR(set_job_importance(job, importance));
    jobs->push(&job);
    return ZX_OK;
}

constexpr zx_job_importance_t IMPORTANCE_CRITICAL = 0xc8;
constexpr zx_job_importance_t IMPORTANCE_FOREGROUND = 0x96;
constexpr zx_job_importance_t IMPORTANCE_BACKGROUND = 0x64;
constexpr zx_job_importance_t IMPORTANCE_DISPOSABLE = 0x32;

// Builds a tree of jobs so we have something interesting to look at.
zx_status_t create_jobs_under(const zx::job& superroot,
                              unique_ptr<JobStack>* jobs_out) {
    JobStack* jobs = JobStack::Create(32);

    RETURN_IF_ERROR(
        create_job(superroot, "test:root", IMPORTANCE_CRITICAL, jobs));
    auto& root = jobs->top();

    RETURN_IF_ERROR(
        create_job(root, "test:drivers", IMPORTANCE_CRITICAL, jobs));
    auto& drivers = jobs->top();
    RETURN_IF_ERROR(create_job(
        drivers, "test:driver-fg", IMPORTANCE_FOREGROUND, jobs));
    RETURN_IF_ERROR(create_job(
        drivers, "test:driver-bg", IMPORTANCE_BACKGROUND, jobs));
    RETURN_IF_ERROR(create_job(
        drivers, "test:driver-disp", IMPORTANCE_DISPOSABLE, jobs));

    RETURN_IF_ERROR(create_job(
        root, "test:framework", IMPORTANCE_FOREGROUND, jobs));
    auto& fw = jobs->top();

    RETURN_IF_ERROR(
        create_job(fw, "test:app-root", IMPORTANCE_CRITICAL, jobs));
    auto& app_root = jobs->top();

    // Creates an app job tree with a settable top-level importance
    // and a range of internal importances.
    auto add_app = [&](const char* name, zx_job_importance_t imp) {
        RETURN_IF_ERROR(create_job(app_root, name, imp, jobs));
        auto& app = jobs->top();
        char subname[ZX_MAX_NAME_LEN];
        snprintf(subname, sizeof(subname), "%s:UI", name);
        RETURN_IF_ERROR(
            create_job(app, subname, IMPORTANCE_CRITICAL, jobs));
        snprintf(subname, sizeof(subname), "%s:service", name);
        RETURN_IF_ERROR(
            create_job(app, subname, IMPORTANCE_BACKGROUND, jobs));
        snprintf(subname, sizeof(subname), "%s:cache", name);
        RETURN_IF_ERROR(
            create_job(app, subname, IMPORTANCE_DISPOSABLE, jobs));
        return ZX_OK;
    };
    RETURN_IF_ERROR(add_app("test:fg-app", IMPORTANCE_FOREGROUND));
    RETURN_IF_ERROR(add_app("test:bg-app", IMPORTANCE_BACKGROUND));
    RETURN_IF_ERROR(add_app("test:disp-app", IMPORTANCE_DISPOSABLE));

    jobs_out->reset(jobs);
    return ZX_OK;
}
} // namespace

zx_status_t create_test_jobs_under(
    zx_handle_t root_handle, unique_ptr<JobStack>* jobs) {
    zx::job root{zx::handle(root_handle)};
    zx_status_t s = create_jobs_under(root, jobs);
    root_handle = root.release(); // Don't close our parent job handle.
    return s;
}
