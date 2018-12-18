// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fbl/algorithm.h>
#include <fbl/array.h>
#include <fbl/auto_call.h>
#include <fbl/auto_lock.h>
#include <fbl/mutex.h>
#include <fbl/ref_ptr.h>
#include <fbl/unique_ptr.h>
#include <lib/zx/port.h>
#include <lib/zx/thread.h>
#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>
#include <zircon/status.h>
#include <zircon/syscalls.h>
#include <zircon/syscalls/debug.h>
#include <zircon/syscalls/exception.h>
#include <zircon/threads.h>

#include <assert.h>
#include <atomic>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#include "stress_test.h"

class VmStressTest;

// VM Stresser
//
// The current stress test runs multiple independent test instance which get randomly
// initialized and torn down over time. Each creates a single pager vmo and hands it to a
// pool of worker threads. Some of the worker threads randomly commit/decommit/read/write/map/unmap
// the vmo. The rest of the worker threads randomly service pager requests or randomly supply
// their own 'prefetch' pages. This is intended to pick out any internal races with the
// VMO/VMAR/Pager system.
//
// Currently does not validate that any given operation was successfully performed, only
// that the apis do not return an error (or crash).
//
// Will evolve over time to use cloned vmos.
//
// TODO: add plain vmos back in once the scheduler stops starving threads on overloaded systems.

class VmStressTest : public StressTest {
public:
    VmStressTest() = default;
    virtual ~VmStressTest() = default;

    virtual zx_status_t Start();
    virtual zx_status_t Stop();

    virtual const char* name() const { return "VM Stress"; }

private:
    int test_thread();

    std::atomic<bool> shutdown_{false};

    thrd_t test_thread_;
} vmstress;

class TestInstance {
public:
    TestInstance(VmStressTest* test, uint64_t vmo_size) : test_(test), vmo_size_(vmo_size) {}

    zx_status_t Start();
    zx_status_t Stop();

private:
    int vmo_thread();
    int pager_thread();

    template<typename... Args> void Printf(const char *str, Args... args) const {
        test_->Printf(str, args...);
    }
    template<typename... Args> void PrintfAlways(const char *str, Args... args) const {
        test_->PrintfAlways(str, args...);
    }

    void CheckVmoThreadError(zx_status_t status, const char* error);

    VmStressTest* const test_;
    const uint64_t vmo_size_;

    static constexpr uint64_t kNumThreads = 6;
    static constexpr uint64_t kNumPagerThreads = 3;
    thrd_t threads_[kNumThreads];

    // Array used for storing vmo mappings. All mappings are cleaned up by the test thread,
    // as vmo threads will sometimes crash if the instance is torn down during a page fault.
    std::atomic<uint32_t> vmo_thread_idx_{0};
    uintptr_t ptrs_[kNumThreads - kNumPagerThreads] = {};
    fbl::Array<uint8_t> bufs_[kNumThreads - kNumPagerThreads] = {};

    // Vector of page requests shared by all pager threads of the instance, to allow
    // requests to be serviced out-of-order.
    fbl::Mutex mtx_;
    fbl::Vector<zx_packet_page_request_t> requests_;

    // Flag used to signal shutdown to worker threads.
    std::atomic<bool> shutdown_{false};

    // Counter that allows the last pager thread to clean up the pager itself.
    std::atomic<uint32_t> pager_thread_count_{kNumPagerThreads};

    zx::vmo vmo_{};
    zx_handle_t pager_;
    zx::port port_{};
};

int TestInstance::vmo_thread() {
    zx_status_t status;

    uint64_t idx = vmo_thread_idx_++;

    // allocate a local buffer
    const size_t buf_size = PAGE_SIZE * 16;
    bufs_[idx] = fbl::Array<uint8_t>(new uint8_t[buf_size], buf_size);
    const fbl::Array<uint8_t>& buf = bufs_[idx];

    // local helper routines to calculate a random range within a vmo and
    // a range appropriate to read into the local buffer above
    auto rand_vmo_range = [this](uint64_t *out_offset, uint64_t *out_size) {
        *out_offset = rand() % vmo_size_;
        *out_size = fbl::min(rand() % vmo_size_, vmo_size_ - *out_offset);
    };
    auto rand_buffer_range = [this](uint64_t *out_offset, uint64_t *out_size) {
        *out_size = rand() % buf_size;
        *out_offset = rand() % (vmo_size_ - *out_size);
    };

    ZX_ASSERT(buf_size < vmo_size_);

    while (!shutdown_.load()) {
        uint64_t off, len;

        int r = rand() % 100;
        switch (r) {
        case 0 ... 9: // commit a range of the vmo
            Printf("c");
            rand_vmo_range(&off, &len);
            status = vmo_.op_range(ZX_VMO_OP_COMMIT, off, len, nullptr, 0);
            CheckVmoThreadError(status, "Failed to commit range");
            break;
        case 10 ... 19: // decommit a range of the vmo
            Printf("d");
            rand_vmo_range(&off, &len);
            status = vmo_.op_range(ZX_VMO_OP_DECOMMIT, off, len, nullptr, 0);
            CheckVmoThreadError(status, "failed to decommit range");
            break;
        case 20 ... 29:
            if (ptrs_[idx]) {
                // unmap the vmo if it already was
                Printf("u");
                status = zx::vmar::root_self()->unmap(ptrs_[idx], vmo_size_);
                CheckVmoThreadError(status, "failed to unmap range");
                ptrs_[idx]= 0;
            }
            // map it somewhere
            Printf("m");
            status = zx::vmar::root_self()->map(0, vmo_, 0, vmo_size_,
                                                ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, ptrs_ + idx);
            CheckVmoThreadError(status, "failed to map range");
            break;
        case 30 ... 39:
            // read from a random range of the vmo
            Printf("r");
            rand_buffer_range(&off, &len);
            status = vmo_.read(buf.get(), off, len);
            CheckVmoThreadError(status, "error reading from vmo");
            break;
        case 40 ... 49:
            // write to a random range of the vmo
            Printf("w");
            rand_buffer_range(&off, &len);
            status = vmo_.write(buf.get(), off, len);
            CheckVmoThreadError(status, "error writing to vmo");
            break;
        case 50 ... 74:
            // read from a random range of the vmo via a direct memory reference
            if (ptrs_[idx]) {
                Printf("R");
                rand_buffer_range(&off, &len);
                memcpy(buf.get(), reinterpret_cast<const void *>(ptrs_[idx] + off), len);
            }
            break;
        case 75 ... 99:
            // write to a random range of the vmo via a direct memory reference
            if (ptrs_[idx]) {
                Printf("W");
                rand_buffer_range(&off, &len);
                memcpy(reinterpret_cast<void *>(ptrs_[idx] + off), buf.get(), len);
            }
            break;
        }

        fflush(stdout);
    }

    return 0;
}

void TestInstance::CheckVmoThreadError(zx_status_t status, const char* error) {
    // Ignore errors while shutting down, since they're almost certainly due to the
    // pager disappearing.
    if (!shutdown_ && status != ZX_OK) {
        fprintf(stderr, "failed to commit range, error %d (%s)\n",
                status, zx_status_get_string(status));
    }
}

static bool is_thread_blocked(zx_handle_t handle) {
    zx_info_thread_t info;
    uint64_t actual, actual_count;
    ZX_ASSERT(zx_object_get_info(handle, ZX_INFO_THREAD, &info,
                                 sizeof(info), &actual, &actual_count) != ZX_OK);
    return info.state == ZX_THREAD_STATE_BLOCKED_PAGER;
}

int TestInstance::pager_thread() {
    zx_status_t status;

    uint64_t vmo_page_count = vmo_size_ / ZX_PAGE_SIZE;
    ZX_ASSERT(vmo_page_count > 0);

    auto supply_pages = [this](uint64_t off, uint64_t len) {
        zx::vmo tmp_vmo;
        zx_status_t status = zx::vmo::create(len, 0, &tmp_vmo);
        if (status != ZX_OK) {
            fprintf(stderr, "failed to create tmp vmo, error %d (%s)\n",
                    status, zx_status_get_string(status));
            return;
        }
        status = tmp_vmo.op_range(ZX_VMO_OP_COMMIT, 0, len, nullptr, 0);
        if (status != ZX_OK) {
            fprintf(stderr, "failed to commit tmp vmo, error %d (%s)\n",
                    status, zx_status_get_string(status));
            return;
        }
        status = zx_pager_vmo_op(pager_, vmo_.get(), ZX_PAGER_OP_SUPPLY_PAGES, off,
                                 len, tmp_vmo.get(), 0);
        if (status != ZX_OK) {
            fprintf(stderr, "failed to supply pages %d, error %d (%s)\n",
                    pager_, status, zx_status_get_string(status));
            return;
        }
    };

    while (!shutdown_.load()) {
        zx::vmo tmp_vmo;
        uint64_t off, size;
        zx::time deadline;

        int r = rand() % 100;
        switch (r) {
        case 0 ... 4: // supply a random range of pages
            off = rand() % vmo_page_count;
            size = fbl::min(rand() % vmo_page_count, vmo_page_count - off); 
            supply_pages(off * PAGE_SIZE, size * PAGE_SIZE);
            break;
        case 5 ... 54: // read from the port
            {
                fbl::AutoLock lock(&mtx_);
                if (requests_.size() == kNumThreads - kNumPagerThreads) {
                    break;
                } else {
                    // We still need to at least query the port if all threads are
                    // blocked, in case we need to read the last thread's packet.
                    deadline = zx::time::infinite_past();
                    for (unsigned i = kNumThreads; i < kNumPagerThreads; i++) {
                        if (!is_thread_blocked(thrd_get_zx_handle(threads_[i]))) {
                            deadline = zx::clock::get_monotonic() + zx::msec(10);
                            break;
                        }
                    }
                }
            }

            zx_port_packet_t packet;
            status = port_.wait(deadline, &packet);
            if (status != ZX_OK) {
                if (status != ZX_ERR_TIMED_OUT) {
                    fprintf(stderr, "failed to read port, error %d (%s)\n",
                            status, zx_status_get_string(status));
                }
            } else if (packet.type != ZX_PKT_TYPE_PAGE_REQUEST
                    || packet.page_request.command != ZX_PAGER_VMO_READ) {
                fprintf(stderr, "unexpected packet, error %d %d\n",
                        packet.type, packet.page_request.command);
            } else {
                fbl::AutoLock lock(&mtx_);
                requests_.push_back(packet.page_request);
            }
            break;
        case 55 ... 99: // fullfil a random request
            fbl::AutoLock lock(&mtx_);
            if (requests_.is_empty()) {
                break;
            }
            off = rand() % requests_.size();
            zx_packet_page_request_t req = requests_.erase(off);
            lock.release();

            supply_pages(req.offset, req.length);
            break;
        }

        fflush(stdout);
    }

    // Have the last pager thread tear down the pager. Randomly either detach the vmo (and
    // close the pager after all test threads are done) or immediately close the pager handle.
    if (--pager_thread_count_ == 0) {
        if (rand() % 2) {
            zx_pager_detach_vmo(pager_, vmo_.get());
        } else {
            zx_handle_close(pager_);
            pager_ = ZX_HANDLE_INVALID;
        }
    }

    return 0;
}

zx_status_t TestInstance::Start() {
    auto status = zx::port::create(0, &port_);
    if (status != ZX_OK) {
        return status;
    }

    status = zx_pager_create(0, &pager_);
    if (status != ZX_OK) {
        return status;
    }

    // create a test vmo
    status = zx_pager_create_vmo(pager_, port_.get(), 0, vmo_size_, 0,
                                 vmo_.reset_and_get_address());
    if (status != ZX_OK)
        return status;

    // create a pile of threads
    // TODO: scale based on the number of cores in the system and/or command line arg
    auto worker = [](void* arg) -> int {
        return static_cast<TestInstance*>(arg)->vmo_thread();
    };
    auto pager_worker = [](void * arg) -> int {
        return static_cast<TestInstance*>(arg)->pager_thread();
    };

    for (uint32_t i = 0; i < fbl::count_of(threads_); i++) {
        bool is_pager_worker = i < kNumPagerThreads;
        thrd_create_with_name(threads_ + i, is_pager_worker ? pager_worker : worker,
                              this, is_pager_worker ? "pager_worker" : "vmstress_worker");
    }
    return ZX_OK;
}

zx_status_t TestInstance::Stop() {
    zx::port port;
    zx::port::create(0, &port);

    // We need to handle potential crashes in the vmo threads when the pager is torn down. Since
    // not all threads will actually crash, we can't stop handling crashes until all threads have
    // terminated. Since the runtime closes a thrd's handle if the thread exists cleanly, we need
    // to wait on a duplicate to properly recieve the termination signal.
    fbl::Vector<zx::thread> thrds;
    for (unsigned i = kNumPagerThreads; i < fbl::count_of(threads_); i++) {
        zx::thread thrd;
        zx::unowned_thread unowned(thrd_get_zx_handle(threads_[i]));
        ZX_ASSERT(unowned->duplicate(ZX_RIGHT_SAME_RIGHTS, &thrd) == ZX_OK);

        zx_status_t status = thrd.bind_exception_port(port, i - kNumPagerThreads, 0);
        ZX_ASSERT(status == ZX_OK);
        status = thrd.wait_async(port, 0, ZX_THREAD_TERMINATED, ZX_WAIT_ASYNC_ONCE);
        ZX_ASSERT(status == ZX_OK);

        thrds.push_back(std::move(thrd));
    }

    shutdown_.store(true);

    uint64_t running_count = fbl::count_of(threads_) - kNumPagerThreads;
    while (running_count) {
        zx_port_packet_t packet;
        ZX_ASSERT(port.wait(zx::time::infinite(), &packet) == ZX_OK);

        if (ZX_PKT_IS_EXCEPTION(packet.type)) {
            zx::thread& thrd = thrds[packet.key];

            zx_exception_report_t report;
            ZX_ASSERT(thrd.get_info(ZX_INFO_THREAD_EXCEPTION_REPORT,
                                    &report, sizeof(report), NULL, NULL) == ZX_OK);
            ZX_ASSERT(report.header.type == ZX_EXCP_FATAL_PAGE_FAULT);

            // thrd_exit takes a parameter, but we don't actually read it when we join
            zx_thread_state_general_regs_t regs;
            ZX_ASSERT(thrd.read_state(ZX_THREAD_STATE_GENERAL_REGS,
                                      &regs, sizeof(regs)) == ZX_OK);
#if defined(__x86_64__)
            regs.rip = reinterpret_cast<uintptr_t>(thrd_exit);
#else
            regs.pc = reinterpret_cast<uintptr_t>(thrd_exit);
#endif
            ZX_ASSERT(thrd.write_state(ZX_THREAD_STATE_GENERAL_REGS,
                                       &regs, sizeof(regs)) == ZX_OK);

            ZX_ASSERT(thrd.resume_from_exception(port, 0) == ZX_OK);
        } else {
            running_count--;
        }
    }

    for (unsigned i = 0; i < fbl::count_of(threads_); i++) {
        thrd_join(threads_[i], nullptr);
    }

    for (unsigned i = 0; i < kNumThreads - kNumPagerThreads; i++) {
        if (ptrs_[i]) {
            zx::vmar::root_self()->unmap(ptrs_[i], vmo_size_);
        }
    }

    if (pager_ != ZX_HANDLE_INVALID) {
        zx_handle_close(pager_);
    }

    return ZX_OK;
}

// Test thread which initializes/tears down TestInstances
int VmStressTest::test_thread() {
    constexpr uint64_t kMaxInstances = 8;
    fbl::unique_ptr<TestInstance> test_instances[kMaxInstances] = {};

    const uint64_t free_bytes = kmem_stats_.free_bytes;
    // scale the size of the VMO we create based on the size of memory in the system.
    // 1/64th the size of total memory generates a fairly sizeable vmo (16MB per 1GB)
    const uint64_t vmo_test_size = free_bytes / 64 / kMaxInstances;

    PrintfAlways("VM stress test: using vmo of size %" PRIu64 "\n", vmo_test_size);

    zx::time deadline = zx::clock::get_monotonic();
    while (!shutdown_.load()) {
        uint64_t r = rand() % kMaxInstances;
        if (test_instances[r]) {
            test_instances[r]->Stop();
            test_instances[r].reset();
        } else {
            test_instances[r] = fbl::make_unique<TestInstance>(this, vmo_test_size);
            test_instances[r]->Start();
        }

        constexpr uint64_t kOpsPerSec = 50;
        deadline += zx::duration(ZX_SEC(1) / kOpsPerSec);
        zx::nanosleep(deadline);
    }

    for (uint64_t i = 0; i < kMaxInstances; i++) {
        if (test_instances[i]) {
            test_instances[i]->Stop();
        }
    }
    return 0;
}

zx_status_t VmStressTest::Start() {
    auto test_worker = [](void* arg) -> int {
        return static_cast<VmStressTest*>(arg)->test_thread();
    };
    thrd_create_with_name(&test_thread_, test_worker, this, "test_worker");

    return ZX_OK;
}

zx_status_t VmStressTest::Stop() {
    shutdown_.store(true);
    thrd_join(test_thread_, nullptr);
    return ZX_OK;
}
