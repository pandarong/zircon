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
#include <zircon/listnode.h>
#include <zxcpp/new.h>
#include <trace.h>

#define LOCAL_TRACE 1

namespace {

// page alloctor request queue state
fbl::Mutex free_request_lock;
fbl::DoublyLinkedList<fbl::RefPtr<PageAllocRequest>> free_requests;
size_t free_request_count;
const size_t initial_free_request_pool = 1024;

} // namespace

void page_alloc_request_init() {
    LTRACE_ENTRY;

    // TODO: move this into an arena or other more efficient allocator
    for (size_t i = 0; i < initial_free_request_pool; i++) {
        fbl::AllocChecker ac;
        PageAllocRequest *par = new (&ac) PageAllocRequest();
        if (!ac.check()) {
            panic("error allocating page allocator pool\n");
        }

        free_requests.push_back(fbl::AdoptRef(par));
        free_request_count++;
    }

    LTRACE_EXIT;
}

PageAllocRequest::PageAllocRequest() = default;
PageAllocRequest::~PageAllocRequest() = default;

fbl::RefPtr<PageAllocRequest> PageAllocRequest::GetRequest() {
    fbl::AutoLock guard(&free_request_lock);

    if (unlikely(free_request_count == 0)) {
        return nullptr;
    }

    auto request = free_requests.pop_front();
    free_request_count--;

    LTRACEF("returning %p count %zu\n", request.get(), free_request_count);

    return fbl::move(request);
}

void PageAllocRequest::fbl_recycle() {
    // put the request back in the free state and add to the free queue
    DEBUG_ASSERT_MSG(state_ == COMPLETED || state_ == FREE,
                     "state is %s", StateToString(state_));
    DEBUG_ASSERT(list_is_empty(&page_list_));

    state_ = FREE;
    count_ = 0;
    event_.Unsignal();

    fbl::AutoLock guard(&free_request_lock);

    LTRACEF("this %p: count %zu\n", this, free_request_count);

    // Tricky: manually deconstruct and reconstruct here so we can be added back
    // to the free list
    this->~PageAllocRequest();
    new (reinterpret_cast<void*>(this)) PageAllocRequest;

    free_requests.push_back(fbl::AdoptRef(this));
    free_request_count++;
}

void PageAllocRequest::SetQueued(size_t count, uint alloc_flags) {
    DEBUG_ASSERT(state_ == FREE);
    DEBUG_ASSERT(list_is_empty(&page_list_));

    count_ = count;
    alloc_flags_ = alloc_flags;
    state_ = QUEUED;
}

void PageAllocRequest::Complete(zx_status_t err, list_node* pages) {
    DEBUG_ASSERT(state_ == QUEUED);
    DEBUG_ASSERT(list_is_empty(&page_list_));

    // set the status, copy the pages (if present), mark the request completed
    error_ = err;
    if (pages) {
        list_move(pages, &page_list_);
    }
    state_ = COMPLETED;
    event_.Signal();
}

zx_status_t PageAllocRequest::Wait(zx_time_t deadline) {
    DEBUG_ASSERT(state_ != FREE);

    return event_.Wait(deadline);
}

