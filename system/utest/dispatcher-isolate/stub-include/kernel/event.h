// Copyright 2017 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#pragma once

#include <zircon/types.h>
typedef uint64_t lk_time_t;

class Event {
public:
    Event(uint32_t opts = 0) {
    }
    ~Event() {
    }

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    // Returns:
    // ZX_OK - signaled
    // ZX_ERR_TIMED_OUT - time out expired
    // ZX_ERR_INTERNAL_INTR_KILLED - thread killed
    // Or the |status| which the caller specified in Event::Signal(status)
    zx_status_t Wait(lk_time_t deadline) {
        return ZX_OK;
    }

    // returns number of ready threads
    int Signal(zx_status_t status = ZX_OK) {
        return ZX_OK;
    }

    zx_status_t Unsignal() {
        return ZX_OK;
    }
};
