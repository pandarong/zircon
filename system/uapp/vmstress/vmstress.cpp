// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <zircon/device/sysinfo.h>
#include <zircon/status.h>
#include <zircon/syscalls.h>
#include <zircon/syscalls/exception.h>
#include <zircon/syscalls/object.h>
#include <pretty/sizes.h>

#include <fbl/auto_call.h>
#include <fbl/ref_ptr.h>
#include <zx/vmar.h>
#include <zx/vmo.h>

#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

static volatile bool shutdown = false;

struct stress_thread_args {
    zx::vmo vmo;

};

static int stress_thread_entry(void *arg_ptr) {
    stress_thread_args *args = (stress_thread_args *)arg_ptr;
    zx_status_t status;

    //printf("top of thread %lu\n", thrd_current());

    uintptr_t ptr = 0;
    uint64_t size = 0;
    status = args->vmo.get_size(&size);
    assert(size > 0);

    while (!shutdown) {
        switch (rand() % 3) {
            case 0:
                printf("c");
                status = args->vmo.op_range(ZX_VMO_OP_COMMIT, rand() % size, size, nullptr, 0);
                if (status != ZX_OK) {
                    fprintf(stderr, "failed to commit range, error %d (%s)\n", status, zx_status_get_string(status));
                }
                break;
            case 1:
                printf("d");
                status = args->vmo.op_range(ZX_VMO_OP_DECOMMIT, rand() % size, size, nullptr, 0);
                if (status != ZX_OK) {
                    fprintf(stderr, "failed to decommit range, error %d (%s)\n", status, zx_status_get_string(status));
                }
                break;
            case 2:
                if (ptr) {
                    printf("u");
                    status = zx::vmar::root_self().unmap(ptr, size);
                    if (status != ZX_OK) {
                        fprintf(stderr, "failed to unmap range, error %d (%s)\n", status, zx_status_get_string(status));
                    }
                }
                printf("m");
                status = zx::vmar::root_self().map(0, args->vmo, 0, size,
                        ZX_VM_FLAG_PERM_READ | ZX_VM_FLAG_PERM_WRITE | ZX_VM_FLAG_MAP_RANGE, &ptr);
                if (status != ZX_OK) {
                    fprintf(stderr, "failed to map range, error %d (%s)\n", status, zx_status_get_string(status));
                }
                break;
        }
        fflush(stdout);
    }

    //printf("bottom of thread %lu\n", thrd_current());

    if (ptr) {
        status = zx::vmar::root_self().unmap(ptr, size);
    }
    delete args;

    return 0;
}

static int vmstress(zx_handle_t root_resource) {
    zx_info_kmem_stats_t stats;
    zx_status_t err = zx_object_get_info(
        root_resource, ZX_INFO_KMEM_STATS, &stats, sizeof(stats), NULL, NULL);
    if (err != ZX_OK) {
        fprintf(stderr, "ZX_INFO_KMEM_STATS returns %d (%s)\n",
                err, zx_status_get_string(err));
        return err;
    }

    uint64_t free_bytes = stats.free_bytes;
    uint64_t vmo_test_size = stats.free_bytes / 64;

    printf("starting stress test: free bytes %" PRIu64 "\n", free_bytes);

    printf("creating test vmo of size %" PRIu64 "\n", vmo_test_size);

    // create a test vmo
    zx::vmo vmo;
    auto status = zx::vmo::create(vmo_test_size, 0, &vmo);
    if (status != ZX_OK)
        return status;

    // map it
    uintptr_t ptr[16];
    for (auto& p: ptr) {
        status = zx::vmar::root_self().map(0, vmo, 0, vmo_test_size,
                ZX_VM_FLAG_PERM_READ | ZX_VM_FLAG_PERM_WRITE | ZX_VM_FLAG_MAP_RANGE, &p);
        if (status != ZX_OK) {
            fprintf(stderr, "mx_vmar_map returns %d (%s)\n", status, zx_status_get_string(status));
            return status;
        }

        memset((void *)p, 0, vmo_test_size);
    }

    // clean up all the mappings on the way out
    auto cleanup = fbl::MakeAutoCall([&] {
            for (auto& p: ptr) {
                zx::vmar::root_self().unmap(p, vmo_test_size);
            }
        });

    // possible algorithm:
    // create one or more vmos: fill with random value that can be computed at any offset
    // spawn a bunch of threads
    // have each thread map/unmap a pile of mappings, read from random spots, validate the vmo has good data in it
    // randomly decommit ranges of the vmos, validate that decommitted ranges are always full of zeros
    // figure out how to handle this race condition

    thrd_t thread[16];
    for (auto& t: thread) {
        stress_thread_args *args = new stress_thread_args;

        vmo.duplicate(ZX_RIGHT_SAME_RIGHTS, &args->vmo);

        thrd_create_with_name(&t, &stress_thread_entry, args, "crashy");
    }

    for (;;)
        zx_nanosleep(zx_deadline_after(ZX_SEC(10)));
    shutdown = true;

    auto cleanup2 = fbl::MakeAutoCall([&] {
            for (auto& t: thread) {
                thrd_join(t, nullptr);
            }
        });

    return ZX_OK;
}


static zx_status_t get_root_resource(zx_handle_t* root_resource) {
    int fd = open("/dev/misc/sysinfo", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "ERROR: Cannot open sysinfo: %s (%d)\n",
                strerror(errno), errno);
        return ZX_ERR_NOT_FOUND;
    }

    ssize_t n = ioctl_sysinfo_get_root_resource(fd, root_resource);
    close(fd);
    if (n != sizeof(*root_resource)) {
        if (n < 0) {
            fprintf(stderr, "ERROR: Cannot obtain root resource: %s (%zd)\n",
                    zx_status_get_string((zx_status_t)n), n);
            return (zx_status_t)n;
        } else {
            fprintf(stderr, "ERROR: Cannot obtain root resource (%zd != %zd)\n",
                    n, sizeof(root_resource));
            return ZX_ERR_NOT_FOUND;
        }
    }
    return ZX_OK;
}

static void print_help(char **argv, FILE* f) {
    fprintf(f, "Usage: %s [options]\n", argv[0]);
#if 0
    fprintf(f, "Options:\n");
    fprintf(f, " -c              Print system CPU stats\n");
    fprintf(f, " -m              Print system memory stats\n");
    fprintf(f, " -d <delay>      Delay in seconds (default 1 second)\n");
    fprintf(f, " -n <times>      Run this many times and then exit\n");
    fprintf(f, " -t              Print timestamp for each report\n");
    fprintf(f, "\nCPU stats columns:\n");
    fprintf(f, "\tcpu:  cpu #\n");
    fprintf(f, "\tload: percentage load\n");
    fprintf(f, "\tsched (cs ylds pmpts irq_pmpts): scheduler statistics\n");
    fprintf(f, "\t\tcs:        context switches\n");
    fprintf(f, "\t\tylds:      explicit thread yields\n");
    fprintf(f, "\t\tpmpts:     thread preemption events\n");
    fprintf(f, "\t\tirq_pmpts: thread preemption events from interrupt\n");

    fprintf(f, "\texcep: exceptions (undefined instruction, bad memory access, etc)\n");
    fprintf(f, "\tpagef: page faults\n");
    fprintf(f, "\tsysc:  syscalls\n");
    fprintf(f, "\tints (hw  tmr tmr_cb): interrupt statistics\n");
    fprintf(f, "\t\thw:     hardware interrupts\n");
    fprintf(f, "\t\ttmr:    timer interrupts\n");
    fprintf(f, "\t\ttmr_cb: kernel timer events\n");
    fprintf(f, "\tipi (rs  gen): inter-processor-interrupts\n");
    fprintf(f, "\t\trs:     reschedule events\n");
    fprintf(f, "\t\tgen:    generic interprocessor interrupts\n");
#endif
}

int main(int argc, char** argv) {
    int c;
    while ((c = getopt(argc, argv, "h")) > 0) {
        switch (c) {
            case 'h':
                print_help(argv, stdout);
                return 0;
            default:
                fprintf(stderr, "Unknown option\n");
                print_help(argv, stderr);
                return 1;
        }
    }

    zx_handle_t root_resource;
    zx_status_t ret = get_root_resource(&root_resource);
    if (ret != ZX_OK) {
        return ret;
    }

    ret = vmstress(root_resource);

    zx_handle_close(root_resource);

    return ret;
}
