// Copyright 2016 The Fuchsia Authors
// Copyright (c) 2014 Travis Geiselbrecht
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <kernel/vm/pmm.h>

#include <assert.h>
#include <err.h>
#include <inttypes.h>
#include <kernel/auto_lock.h>
#include <kernel/mp.h>
#include <kernel/mutex.h>
#include <kernel/timer.h>
#include <kernel/vm.h>
#include <lib/console.h>
#include <list.h>
#include <lk/init.h>
#include <platform.h>
#include <pow2.h>
#include <stdlib.h>
#include <string.h>
#include <trace.h>

#include "pmm_arena.h"
#include "pmm_node.h"
#include "vm_priv.h"

#include <magenta/thread_annotations.h>
#include <mxcpp/new.h>
#include <mxtl/intrusive_double_list.h>

#define LOCAL_TRACE MAX(VM_GLOBAL_TRACE, 0)

// The (currently) one and only pmm node
static PmmNode pmm_node;

#if PMM_ENABLE_FREE_FILL
static void pmm_enforce_fill(uint level) {
    pmm_node.EnforceFill();
}
LK_INIT_HOOK(pmm_fill, &pmm_enforce_fill, LK_INIT_LEVEL_VM);
#endif

paddr_t vm_page_to_paddr(const vm_page* page) {
    return page->paddr;
}

vm_page* paddr_to_vm_page(paddr_t addr) {
    return pmm_node.PaddrToPage(addr);
}

status_t pmm_add_arena(const pmm_arena_info_t* info) {
    return pmm_node.AddArena(info);
}

vm_page* pmm_alloc_page(uint alloc_flags, paddr_t* pa) {
    return pmm_node.AllocPage(alloc_flags, pa);
}

size_t pmm_alloc_pages(size_t count, uint alloc_flags, list_node* list) {
    return pmm_node.AllocPages(count, alloc_flags, list);
}

size_t pmm_alloc_range(paddr_t address, size_t count, list_node* list) {
    return pmm_node.AllocRange(address, count, list);
}

size_t pmm_alloc_contiguous(size_t count, uint alloc_flags, uint8_t alignment_log2, paddr_t* pa,
                            list_node* list) {
    return pmm_node.AllocContiguous(count, alloc_flags, alignment_log2, pa, list);
}

/* physically allocate a run from arenas marked as KMAP */
void* pmm_alloc_kpages(size_t count, list_node* list, paddr_t* _pa) {
    LTRACEF("count %zu\n", count);

    paddr_t pa;
    /* fast path for single count allocations */
    if (count == 1) {
        vm_page* p = pmm_node.AllocPage(PMM_ALLOC_FLAG_KMAP, &pa);
        if (!p)
            return nullptr;

        if (list) {
            list_add_tail(list, &p->queue_node);
        }
    } else {
        size_t alloc_count = pmm_node.AllocContiguous(count, PMM_ALLOC_FLAG_KMAP, PAGE_SIZE_SHIFT, &pa, list);
        if (alloc_count == 0)
            return nullptr;
    }

    LTRACEF("pa %#" PRIxPTR "\n", pa);
    void* ptr = paddr_to_kvaddr(pa);
    DEBUG_ASSERT(ptr);

    if (_pa)
        *_pa = pa;
    return ptr;
}

/* allocate a single page from a KMAP arena and return its virtual address */
void* pmm_alloc_kpage(paddr_t* _pa, vm_page** _p) {
    LTRACE_ENTRY;

    paddr_t pa;
    vm_page* p = pmm_node.AllocPage(PMM_ALLOC_FLAG_KMAP, &pa);
    if (!p)
        return nullptr;

    void* ptr = paddr_to_kvaddr(pa);
    DEBUG_ASSERT(ptr);

    if (_pa)
        *_pa = pa;
    if (_p)
        *_p = p;
    return ptr;
}

size_t pmm_free_kpages(void* _ptr, size_t count) {
    LTRACEF("ptr %p, count %zu\n", _ptr, count);

    uint8_t* ptr = (uint8_t*)_ptr;

    list_node list;
    list_initialize(&list);

    while (count > 0) {
        vm_page* p = paddr_to_vm_page(vaddr_to_paddr(ptr));
        if (p) {
            DEBUG_ASSERT(!p->is_free());

            if (list_in_list(&p->queue_node))
                list_delete(&p->queue_node);

            list_add_tail(&list, &p->queue_node);
        }

        ptr += PAGE_SIZE;
        count--;
    }

    return pmm_node.Free(&list);
}

size_t pmm_free(list_node* list) {
    return pmm_node.Free(list);
}

size_t pmm_free_page(vm_page* page) {
    pmm_node.Free(page);
    return 1;
}

uint64_t pmm_count_free_pages() {
    return pmm_node.CountFreePages();
}

uint64_t pmm_count_total_bytes() {
    return pmm_node.CountTotalBytes();
}

void pmm_count_total_states(size_t state_count[_VM_PAGE_STATE_COUNT]) {
    pmm_node.CountTotalStates(state_count);
}

extern "C" enum handler_return pmm_dump_timer(struct timer* t, lk_time_t now, void*) {
    timer_set_oneshot(t, now + LK_SEC(1), &pmm_dump_timer, nullptr);
    pmm_node.DumpFree();
    return INT_NO_RESCHEDULE;
}

static int cmd_pmm(int argc, const cmd_args* argv, uint32_t flags) {
    bool is_panic = flags & CMD_FLAG_PANIC;

    if (argc < 2) {
    notenoughargs:
        printf("not enough arguments\n");
    usage:
        printf("usage:\n");
        printf("%s dump\n", argv[0].str);
        if (!is_panic) {
            printf("%s free\n", argv[0].str);
        }
        return MX_ERR_INTERNAL;
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
            timer_set_oneshot(&timer, current_time() + LK_SEC(1), &pmm_dump_timer, nullptr);
            show_mem = true;
        } else {
            timer_cancel(&timer);
            show_mem = false;
        }
    } else {
        printf("unknown command\n");
        goto usage;
    }

    return MX_OK;
}

STATIC_COMMAND_START
#if LK_DEBUGLEVEL > 0
STATIC_COMMAND_MASKED("pmm", "physical memory manager", &cmd_pmm, CMD_AVAIL_ALWAYS)
#endif
STATIC_COMMAND_END(pmm);
