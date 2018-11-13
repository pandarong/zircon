// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <vm/page_alloc_request.h>

#include <fbl/alloc_checker.h>
#include <fbl/auto_lock.h>
#include <fbl/intrusive_single_list.h>
#include <fbl/ref_ptr.h>
#include <vm/pmm.h>
#include <zircon/listnode.h>
#include <zxcpp/new.h>
#include <trace.h>

#define LOCAL_TRACE 0

PageAllocRequest::PageAllocRequest() = default;

PageAllocRequest::~PageAllocRequest() {
    DEBUG_ASSERT(state_ == FREE || state_ == COMPLETED);
    DEBUG_ASSERT(list_is_empty(&page_list_));
}

void PageAllocRequest::Set(size_t count, uint alloc_flags) {
    DEBUG_ASSERT(state_ == FREE);
    DEBUG_ASSERT(list_is_empty(&page_list_));

    count_ = count;
    alloc_flags_ = alloc_flags;
    state_ = READY;
}

void PageAllocRequest::Queue() {
    DEBUG_ASSERT(state_ == READY);

    state_ = QUEUED;
}

void PageAllocRequest::Complete(zx_status_t err, bool signal) {
    DEBUG_ASSERT(state_ == READY || state_ == QUEUED);

    // set the status, copy the pages (if present), mark the request completed
    error_ = err;
    state_ = COMPLETED;
    if (signal) {
        event_.Signal();
    }
}

zx_status_t PageAllocRequest::Wait(zx_time_t deadline) {
    DEBUG_ASSERT(state_ != FREE);

    return event_.Wait(deadline);
}

void PageAllocRequest::Free() {
    DEBUG_ASSERT(state_ == READY || state_ == COMPLETED);

    if (!list_is_empty(&page_list_)) {
        pmm_free(&page_list_);
    }

    state_ = FREE;
}

const char* PageAllocRequest::StateToString(State s) {
    switch (s) {
        case FREE: return "FREE";
        case READY: return "READY";
        case QUEUED: return "QUEUED";
        case COMPLETED: return "COMPLETED";
        default: return "UNKNOWN";
    }
}


