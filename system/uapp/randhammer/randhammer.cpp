// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include <zircon/status.h>
#include <zircon/syscalls.h>
#include <zircon/types.h>
#include <zx/event.h>
#include <zx/time.h>

const int kDefaultMaxWorkers = 64;
const size_t kTotalDrawLen = 1UL << 24;

void print_usage(const char* argv0) {
    printf("usage: %s [-n <num_workers>]\n\n", argv0);
    printf("Spawn threads to zx_crpng_draw %zu bytes.\n\n", kTotalDrawLen);
    printf("-n <num_workers>   Specifies the max number of threads.\n");
    printf("                   Defaults to %d.\n", kDefaultMaxWorkers);
}

int worker(void* arg) {
    zx_status_t rc;

    uint8_t buf[ZX_CPRNG_DRAW_MAX_LEN];
    size_t iters = kTotalDrawLen / ZX_CPRNG_DRAW_MAX_LEN;
    size_t actual;

    zx::event* green_flag = static_cast<zx::event*>(arg);
    zx_signals_t observed;
    if ((rc = green_flag->wait_one(ZX_USER_SIGNAL_0,
                                   zx::deadline_after(zx::sec(10)),
                                   &observed)) != ZX_OK) {
        printf("event::wait_one failed: %s\n", zx_status_get_string(rc));
        return rc;
    }

    for (size_t i = 0; i < iters; ++i) {
        if ((rc = zx_cprng_draw(buf, sizeof(buf), &actual)) != ZX_OK) {
            return rc;
        }
    }

    return ZX_OK;
}

int main(int argc, char** argv) {
    zx_status_t rc;

    int max_workers;
    switch (argc) {
    case 1:
        max_workers = kDefaultMaxWorkers;
        break;
    case 3:
        max_workers = atoi(argv[2]);
        if (strcmp(argv[1], "-n") == 0 && max_workers > 0) {
            break;
        }
    // fall through
    default:
        print_usage(argv[0]);
        return ZX_ERR_INVALID_ARGS;
    }

    zx::event green_flag;
    if ((rc = zx::event::create(0, &green_flag)) != ZX_OK) {
        printf("zx::event::create failed: %s\n", zx_status_get_string(rc));
        return rc;
    }

    for (int num_workers = 1; num_workers <= max_workers; ++num_workers) {
        if ((rc = green_flag.signal(ZX_USER_SIGNAL_0, 0)) != ZX_OK) {
            printf("zx::event::signal failed: %s\n", zx_status_get_string(rc));
            return rc;
        }

        thrd_t tids[num_workers];
        zx_status_t rcs[num_workers];
        for (int i = 0; i < num_workers; ++i) {
            if (thrd_create(&tids[i], worker, &green_flag) != thrd_success) {
                printf("failed to start worker %d\n", i);
                return ZX_ERR_NO_RESOURCES;
            }
        }

        zx::time start = zx::clock::get(ZX_CLOCK_MONOTONIC);
        if ((rc = green_flag.signal(0, ZX_USER_SIGNAL_0)) != ZX_OK) {
            printf("zx::event::signal failed: %s\n", zx_status_get_string(rc));
            return rc;
        }
        for (int i = 0; i < num_workers; ++i) {
            thrd_join(tids[i], &rcs[i]);
        }
        zx::time finish = zx::clock::get(ZX_CLOCK_MONOTONIC);

        for (int i = 0; i < num_workers; ++i) {
            if (rcs[i] != ZX_OK) {
                printf("worker %d returned %s\n", i, zx_status_get_string(rcs[i]));
            }
        }

        zx::duration per_call = (finish - start) / ((kTotalDrawLen / 1024) * num_workers);
        printf("%d workers, %zu bytes, %d bytes/call => %" PRIu64 " ns/kb.\n",
            num_workers, kTotalDrawLen, ZX_CPRNG_DRAW_MAX_LEN, per_call.get());
    }

    return 0;
}
