// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#pragma once

#include <fbl/canary.h>
#include <fbl/intrusive_double_list.h>
#include <fbl/intrusive_single_list.h>
#include <fbl/recycler.h>
#include <fbl/ref_counted.h>
#include <fbl/ref_ptr.h>
#include <kernel/event.h>
#include <zircon/listnode.h>

// delayed memory allocation
class PageAllocRequest final :
    public fbl::Canary<fbl::magic("PGAR")>,
    public fbl::RefCounted<PageAllocRequest>,
    public fbl::Recyclable<PageAllocRequest>,
    public fbl::DoublyLinkedListable<fbl::RefPtr<PageAllocRequest>> {
public:
    PageAllocRequest();
    ~PageAllocRequest();

    // retrieve a free test, should be in the FREE state
    static fbl::RefPtr<PageAllocRequest> GetRequest();

    // copy relevant data into the request, mark as QUEUED
    void SetQueued(size_t count, uint alloc_flags);

    // mark the request as complete
    void Complete(zx_status_t err, list_node* pages);

    zx_status_t Wait(zx_time_t deadline = ZX_TIME_INFINITE);

    // read the request state of the data
    zx_status_t error() const { return error_; }
    size_t count() const { return count_; }
    uint alloc_flags() const { return alloc_flags_; }

    void TakePageList(list_node *dest) { list_move(&page_list_, dest); }

private:
    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(PageAllocRequest);

    // when ref ptr goes to zero, call this
    friend class fbl::Recyclable<PageAllocRequest>;
    void fbl_recycle();

    // request state
    enum State {
        FREE,
        QUEUED,
        COMPLETED,
        ERROR
    } state_ = FREE;

    const char* StateToString(State s) {
        switch (s) {
            case FREE: return "FREE";
            case QUEUED: return "QUEUED";
            case COMPLETED: return "COMPLETED";
            case ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }

    Event event_;

    // allocation request data
    size_t count_;
    uint alloc_flags_;
    zx_status_t error_;
    list_node page_list_ = LIST_INITIAL_VALUE(page_list_);
};

void page_alloc_request_init();

