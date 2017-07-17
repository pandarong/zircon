// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zx/job.h>
#include <fbl/array.h>
#include <fbl/macros.h>
#include <fbl/unique_ptr.h>

// Holds onto job handles and keeps them alive.
// TODO: Consider making this object<T>-templated and move into zx.
class JobStack : private fbl::Array<zx::job> {
public:
    static JobStack *Create(size_t count);

    // Swaps the job onto the stack.
    void push(zx::job* job);

    // Returns the most-recently-pushed job.
    zx::job& top() const;

private:
    JobStack(zx::job* array, size_t count);
    DISALLOW_COPY_ASSIGN_AND_MOVE(JobStack);

    size_t next_;
};

// Creates a canned tree of jobs under the specified root job.
// Does not create any processes.
zx_status_t create_test_jobs_under(
    zx_handle_t root, fbl::unique_ptr<JobStack>* jobs);
