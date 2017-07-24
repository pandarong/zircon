// Copyright 2016 The Fuchsia Authors
// Copyright (c) 2014 Travis Geiselbrecht
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#pragma once

#include <list.h>
#include <magenta/compiler.h>
#include <stdint.h>
#include <sys/types.h>

// forward declare
class VmObject;

#define VM_PAGE_OBJECT_PIN_COUNT_BITS 5
#define VM_PAGE_OBJECT_MAX_PIN_COUNT ((1ul << VM_PAGE_OBJECT_PIN_COUNT_BITS) - 1)

// core per page structure allocated at pmm arena creation time
struct vm_page {
    struct list_node queue_node;
    paddr_t paddr;
    // offset 0x18

    struct {
        uint32_t flags : 8;
        uint32_t state : 3;
    };
    uint32_t map_count;
    // offset: 0x20

    union {
        struct {
            // in allocated/just freed state, use a linked list to hold the page in a queue
            //struct list_node node;
            // offset: 0x30
        } free;
        struct {
            // attached to a vm object
            uint64_t offset; // unused currently
            // offset: 0x28
            VmObject* obj; // unused currently

            // offset: 0x30
            uint8_t pin_count : VM_PAGE_OBJECT_PIN_COUNT_BITS;
            // If true, one pin slot is used by the VmObject to keep a run
            // contiguous.
            bool contiguous_pin : 1;
        } object;

        uint8_t pad[0x38 - 0x20]; // pad out to 0x38 bytes
    };

    // helper routines
    bool is_free();

    // state manipulation routines
    void set_state_alloc();
};

// assert that the page structure isn't growing uncontrollably
static_assert(sizeof(vm_page) == 0x38, "");

enum vm_page_state {
    VM_PAGE_STATE_FREE,
    VM_PAGE_STATE_ALLOC,
    VM_PAGE_STATE_OBJECT,
    VM_PAGE_STATE_WIRED,
    VM_PAGE_STATE_HEAP,
    VM_PAGE_STATE_MMU, /* allocated to serve arch-specific mmu purposes */

    _VM_PAGE_STATE_COUNT
};

// helpers
inline bool vm_page::is_free() {
    return state == VM_PAGE_STATE_FREE;
}

const char* page_state_to_string(unsigned int state);
void dump_page(const vm_page* page);

// state transition routines
void pmm_page_set_state_alloc(vm_page *page);
void pmm_page_set_state_wired(vm_page *page);

