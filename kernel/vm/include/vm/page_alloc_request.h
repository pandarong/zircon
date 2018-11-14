// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#pragma once

#include <fbl/canary.h>
#include <fbl/intrusive_double_list.h>
#include <kernel/event.h>
#include <zircon/listnode.h>

// delayed memory allocation
class PageAllocRequest :
    public fbl::DoublyLinkedListable<PageAllocRequest*> {
public:
    PageAllocRequest();
    ~PageAllocRequest();

    // copy relevant data into the request, mark as READY
    void Set(size_t count, uint alloc_flags);

    // mark as queued
    void Queue();

    // mark the request as complete
    void Complete(zx_status_t err, bool signal);

    // wait for the status to change
    zx_status_t Wait(zx_time_t deadline = ZX_TIME_INFINITE);

    // reset the state back to FREE
    void Free();

    // read the request state of the data
    zx_status_t error() const { return error_; }
    size_t count() const { return count_; }
    uint alloc_flags() const { return alloc_flags_; }
    list_node* page_list() { return &page_list_; }

    bool IsComplete() const { return state_ == COMPLETED; }

    int state() const { return state_; }
    const char* stateString() const { return StateToString(state_); }

private:
    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(PageAllocRequest);

    // magic
    fbl::Canary<fbl::magic("PGAR")> canary_;

    // request state
    enum State {
        FREE,
        READY,
        QUEUED,
        COMPLETED
    } state_ = FREE;

    static const char* StateToString(State s);

    Event event_;

    // allocation request data
    size_t count_;
    uint alloc_flags_;
    zx_status_t error_;
    list_node page_list_ = LIST_INITIAL_VALUE(page_list_);
};

