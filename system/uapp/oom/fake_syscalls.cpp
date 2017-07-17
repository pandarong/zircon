// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Fake syscalls for testing.

#include "fake_syscalls.h"

#include <string.h>
#include <inttypes.h>

#include <fbl/intrusive_double_list.h>
#include <fbl/unique_ptr.h>
#include <zircon/status.h>
#include <zircon/syscalls.h>
#include <zircon/syscalls/object.h>
#include <zxcpp/new.h>

#include "macros.h"

using fbl::unique_ptr;

namespace {

zx_status_t get_handle_koid(zx_handle_t handle, zx_koid_t* koid) {
    zx_info_handle_basic_t info;
    RETURN_IF_ERROR(zx_object_get_info(
        handle, ZX_INFO_HANDLE_BASIC, &info, sizeof(info), nullptr, nullptr));
    *koid = info.koid;
    return ZX_OK;
}

class RankedJob
    : public fbl::DoublyLinkedListable<fbl::unique_ptr<RankedJob>> {
public:
    static zx_status_t Create(zx_handle_t handle, unique_ptr<RankedJob>* out) {
        zx_koid_t koid;
        RETURN_IF_ERROR(get_handle_koid(handle, &koid));

        char name[ZX_MAX_NAME_LEN];
        RETURN_IF_ERROR(
            zx_object_get_property(handle, ZX_PROP_NAME, name, sizeof(name)));

        fbl::AllocChecker ac;
        out->reset(new (&ac) RankedJob(koid, name));
        if (!ac.check()) {
            return ZX_ERR_NO_MEMORY;
        }
        return ZX_OK;
    }

    zx_koid_t koid() const { return koid_; }
    const char* name() const { return name_; }

    // Type of linked lists of this class.
    using List = fbl::DoublyLinkedList<fbl::unique_ptr<RankedJob>>;

private:
    RankedJob(zx_koid_t koid, const char* name)
        : koid_(koid) {
        strlcpy(const_cast<char*>(name_), name, sizeof(name_));
    }

    const char name_[ZX_MAX_NAME_LEN] = {};
    const zx_koid_t koid_;
};

RankedJob::List ranked_jobs;

RankedJob* get_ranked_job_by_koid(zx_koid_t koid) {
    for (auto& job : ranked_jobs) {
        if (job.koid() == koid) {
            return &job;
        }
    }
    return nullptr;
}

} // namespace

zx_status_t _fake_job_set_relative_importance(
    zx_handle_t root_resource,
    zx_handle_t job, zx_handle_t less_important_job) {

    // Make sure the root resource looks legit.
    zx_info_handle_basic_t info;
    RETURN_IF_ERROR(zx_object_get_info(root_resource, ZX_INFO_HANDLE_BASIC,
                                       &info, sizeof(info), nullptr, nullptr));
    if (info.type != ZX_OBJ_TYPE_RESOURCE) {
        return ZX_ERR_WRONG_TYPE;
    }

    unique_ptr<RankedJob> rjob;
    {
        zx_koid_t koid;
        RETURN_IF_ERROR(get_handle_koid(job, &koid));
        RankedJob* j = get_ranked_job_by_koid(koid);
        if (j == nullptr) {
            RETURN_IF_ERROR(RankedJob::Create(job, &rjob));
        } else {
            rjob.reset(ranked_jobs.erase(*j).release());
        }
    }
    fflush(stdout);
    ZX_ASSERT(!rjob->InContainer());

    if (less_important_job == ZX_HANDLE_INVALID) {
        // Make this the least important.
        ranked_jobs.push_front(fbl::move(rjob));
    } else {
        // Insert rjob just after less_important_job.
        zx_koid_t koid;
        RETURN_IF_ERROR(get_handle_koid(less_important_job, &koid));
        RankedJob* li_job = get_ranked_job_by_koid(koid);
        // Simplification: less_important_job must exist already. The real
        // syscall wouldn't have this restriction.
        ZX_DEBUG_ASSERT(li_job != nullptr);
        ranked_jobs.insert_after(
            ranked_jobs.make_iterator(*li_job), fbl::move(rjob));
    }
    return ZX_OK;
}

void dump_importance_list() {
    printf("Least important:\n");
    for (const auto& rj : ranked_jobs) {
        printf("- k:%" PRIu64 " [%-*s]\n",
               rj.koid(), ZX_MAX_NAME_LEN, rj.name());
    }
}
