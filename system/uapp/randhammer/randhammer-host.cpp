// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#define ZX_CPRNG_DRAW_MAX_LEN 256

const int kDefaultMaxWorkers = 64;
const size_t kTotalDrawLen = 1UL << 24;

typedef struct {
    std::mutex mtx;
    std::condition_variable cnd;
    bool waved;
} green_flag_t;

void print_usage(const char* argv0) {
    printf("usage: %s [-n <num_workers>]\n\n", argv0);
    printf("Spawn threads to zx_crpng_draw %zu bytes.\n\n", kTotalDrawLen);
    printf("-n <num_workers>   Specifies the max number of threads.\n");
    printf("                   Defaults to %d.\n", kDefaultMaxWorkers);
}

void worker(green_flag_t* green_flag) {
    ssize_t rc;

    uint8_t buf[ZX_CPRNG_DRAW_MAX_LEN];
    size_t iters = kTotalDrawLen / ZX_CPRNG_DRAW_MAX_LEN;
    size_t actual;

    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        perror("open('udev/random', O_RDONLY)");
        return;
    }

    {
        std::unique_lock<std::mutex> guard(green_flag->mtx);
        if (!green_flag->waved && green_flag->cnd.wait_for(guard, std::chrono::seconds(10)) == std::cv_status::timeout) {
            printf("timeout\n");
            return;
        }
    }

    for (size_t i = 0; i < iters; ++i) {
        if ((rc = read(fd, buf, sizeof(buf))) < 0) {
            perror("read");
            return;
        }
    }
}

int main(int argc, char** argv) {
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
        return EINVAL;
    }

    green_flag_t green_flag;
    std::vector<std::thread> threads;
    struct timespec tp;
    uint64_t start, finish, per_call;
    for (int num_workers = 1; num_workers <= max_workers; ++num_workers) {
        {
            std::unique_lock<std::mutex> guard(green_flag.mtx);
            threads.clear();
            green_flag.waved = false;

            for (int i = 0; i < num_workers; ++i) {
                threads.push_back(std::thread(worker, &green_flag));
            }

            if (clock_gettime(CLOCK_MONOTONIC, &tp) < 0) {
                perror("clock_gettime");
                return errno;
            }
            start = (tp.tv_sec * 1000000000) + tp.tv_nsec;
            green_flag.waved = true;
            green_flag.cnd.notify_all();
        }

        for (auto& thrd : threads) {
            thrd.join();
        }

        if (clock_gettime(CLOCK_MONOTONIC, &tp) < 0) {
            perror("clock_gettime");
            return errno;
        }
        finish = (tp.tv_sec * 1000000000) + tp.tv_nsec;
        per_call = (finish - start) /  ((kTotalDrawLen / 1024) * num_workers);
        printf("%d workers, %zu bytes, %d bytes/call => %" PRIu64 " ns/kb.\n",
               num_workers, kTotalDrawLen, ZX_CPRNG_DRAW_MAX_LEN, per_call);
    }

    return 0;
}
