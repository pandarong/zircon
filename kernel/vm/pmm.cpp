// Copyright 2016 The Fuchsia Authors
// Copyright (c) 2014 Travis Geiselbrecht
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <vm/pmm.h>

#include <assert.h>
#include <err.h>
#include <fbl/auto_lock.h>
#include <fbl/intrusive_double_list.h>
#include <fbl/mutex.h>
#include <inttypes.h>
#include <kernel/mp.h>
#include <kernel/timer.h>
#include <lib/console.h>
#include <lk/init.h>
#include <new>
#include <platform.h>
#include <pow2.h>
#include <stdlib.h>
#include <string.h>
#include <trace.h>
#include <vm/bootalloc.h>
#include <vm/page_alloc_request.h>
#include <vm/physmap.h>
#include <vm/vm.h>
#include <zircon/thread_annotations.h>
#include <zircon/time.h>
#include <zircon/types.h>

#include "pmm_arena.h"
#include "pmm_node.h"
#include "vm_priv.h"
#include "vm_worker.h"

#define LOCAL_TRACE MAX(VM_GLOBAL_TRACE, 0)

namespace {

// The (currently) one and only pmm node
PmmNode pmm_node;

// allocation queue
fbl::Mutex alloc_queue_lock;
fbl::DoublyLinkedList<PageAllocRequest*> alloc_queue TA_GUARDED(alloc_queue_lock);

// worker to consume the queue
VmWorker alloc_queue_worker;
zx_time_t alloc_queue_worker_routine(void *arg);

} // namespace

#if PMM_ENABLE_FREE_FILL
static void pmm_enforce_fill(uint level) {
    pmm_node.EnforceFill();
}
LK_INIT_HOOK(pmm_fill, &pmm_enforce_fill, LK_INIT_LEVEL_VM);
#endif

vm_page_t* paddr_to_vm_page(paddr_t addr) {
    return pmm_node.PaddrToPage(addr);
}

zx_status_t pmm_add_arena(const pmm_arena_info_t* info) {
    return pmm_node.AddArena(info);
}

zx_status_t pmm_alloc_page(uint alloc_flags, paddr_t* pa) {
    return pmm_node.AllocPage(alloc_flags, nullptr, pa);
}

zx_status_t pmm_alloc_page(uint alloc_flags, vm_page_t** page) {
    return pmm_node.AllocPage(alloc_flags, page, nullptr);
}

zx_status_t pmm_alloc_page(uint alloc_flags, vm_page_t** page, paddr_t* pa) {
    return pmm_node.AllocPage(alloc_flags, page, pa);
}

zx_status_t pmm_alloc_pages(size_t count, uint alloc_flags, list_node* list) {
    return pmm_node.AllocPages(count, alloc_flags, list);
}

zx_status_t pmm_alloc_pages_delayed(size_t count, uint alloc_flags,
                                    PageAllocRequest* request) {
    DEBUG_ASSERT(request);

    request->Set(count, alloc_flags);

    if (alloc_flags & PMM_ALLOC_FLAG_FORCE_IMMED_TEST) {
        // fallthrough for now
    } else if (alloc_flags & PMM_ALLOC_FLAG_FORCE_DELAYED_TEST) {
        LTRACEF("delayed count %zu flags %#x\n", count, alloc_flags);

        // set up and queue the request
        request->Queue();

        {
            fbl::AutoLock guard(&alloc_queue_lock);
            alloc_queue.push_back(request);
        }

        alloc_queue_worker.Signal();

        return ZX_ERR_SHOULD_WAIT;
    }

    LTRACEF("immed count %zu flags %#x\n", count, alloc_flags);
    zx_status_t err = pmm_alloc_pages(request->count(), request->alloc_flags(), request->page_list());

    request->Complete(err, false);

    return err;
}

namespace {

// called once per Signal() on alloc_queue_worker
zx_time_t alloc_queue_worker_routine(void *arg) {
    // pop off an allocation request(s) until the queue is empty
    for (;;) {
        PageAllocRequest* request;
        {
            fbl::AutoLock guard(&alloc_queue_lock);

            request = alloc_queue.pop_front();
            if (unlikely(!request))
                return ZX_TIME_INFINITE;
        }

        LTRACEF("handling request %p\n", request);

        // do the request
        size_t count = request->count();
        uint alloc_flags = request->alloc_flags();
        list_node list = LIST_INITIAL_VALUE(list);

        zx_status_t status = pmm_alloc_pages(count, alloc_flags, request->page_list());

        // complete the transfer
        request->Complete(status, true);
    }

    return ZX_TIME_INFINITE;
}

void pmm_start_workers(uint level) {
    alloc_queue_worker.Run("PMM allocation queue", &alloc_queue_worker_routine, nullptr);
}

LK_INIT_HOOK(pmm_vm_workers, &pmm_start_workers, LK_INIT_LEVEL_THREADING);
} // namespace

zx_status_t pmm_alloc_range(paddr_t address, size_t count, list_node* list) {
    return pmm_node.AllocRange(address, count, list);
}

zx_status_t pmm_alloc_contiguous(size_t count, uint alloc_flags, uint8_t alignment_log2, paddr_t* pa,
                                 list_node* list) {
    // if we're called with a single page, just fall through to the regular allocation routine
    if (unlikely(count == 1 && alignment_log2 <= PAGE_SIZE_SHIFT)) {
        vm_page_t* page;
        zx_status_t status = pmm_node.AllocPage(alloc_flags, &page, pa);
        if (status != ZX_OK) {
            return status;
        }
        list_add_tail(list, &page->queue_node);
        return ZX_OK;
    }

    return pmm_node.AllocContiguous(count, alloc_flags, alignment_log2, pa, list);
}

void pmm_free(list_node* list) {
    pmm_node.FreeList(list);
}

void pmm_free_page(vm_page* page) {
    pmm_node.FreePage(page);
}

uint64_t pmm_count_free_pages() {
    return pmm_node.CountFreePages();
}

uint64_t pmm_count_total_bytes() {
    return pmm_node.CountTotalBytes();
}

void pmm_count_total_states(size_t state_count[VM_PAGE_STATE_COUNT_]) {
    pmm_node.CountTotalStates(state_count);
}

namespace {

void pmm_dump_timer(struct timer* t, zx_time_t now, void*) {
    zx_time_t deadline = zx_time_add_duration(now, ZX_SEC(1));
    timer_set_oneshot(t, deadline, &pmm_dump_timer, nullptr);
    pmm_node.DumpFree();
}

int cmd_pmm(int argc, const cmd_args* argv, uint32_t flags) {
    bool is_panic = flags & CMD_FLAG_PANIC;

    if (argc < 2) {
        printf("not enough arguments\n");
    usage:
        printf("usage:\n");
        printf("%s dump\n", argv[0].str);
        if (!is_panic) {
            printf("%s free\n", argv[0].str);
        }
        return ZX_ERR_INTERNAL;
    }

    if (!strcmp(argv[1].str, "dump")) {
        pmm_node.Dump(is_panic);
    } else if (is_panic) {
        // No other operations will work during a panic.
        printf("Only the \"arenas\" command is available during a panic.\n");
        goto usage;
    } else if (!strcmp(argv[1].str, "free")) {
        static bool show_mem = false;
        static timer_t timer;

        if (!show_mem) {
            printf("pmm free: issue the same command to stop.\n");
            timer_init(&timer);
            zx_time_t deadline = zx_time_add_duration(current_time(), ZX_SEC(1));
            timer_set(&timer, deadline, TIMER_SLACK_CENTER, ZX_MSEC(20), &pmm_dump_timer, nullptr);
            show_mem = true;
        } else {
            timer_cancel(&timer);
            show_mem = false;
        }
    } else {
        printf("unknown command\n");
        goto usage;
    }

    return ZX_OK;
}

STATIC_COMMAND_START
#if LK_DEBUGLEVEL > 0
STATIC_COMMAND_MASKED("pmm", "physical memory manager", &cmd_pmm, CMD_AVAIL_ALWAYS)
#endif
STATIC_COMMAND_END(pmm);

} // namespace


