// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <vm/vmalloc.h>

#include <trace.h>
#include <vm/vm_aspace.h>

static const uint kArchRwFlags = ARCH_MMU_FLAG_PERM_READ | ARCH_MMU_FLAG_PERM_WRITE;

void* vmalloc(size_t len, const char* name) {
    void* ptr;
    zx_status_t status = VmAspace::kernel_aspace()->Alloc(name ? name : "vmalloc",
            len, &ptr, 0, VmAspace::VMM_FLAG_COMMIT, kArchRwFlags);
    if (status != ZX_OK) {
        return nullptr;
    }

    return ptr;
}

void vmfree(void* ptr) {
    vaddr_t va = reinterpret_cast<vaddr_t>(ptr);

    DEBUG_ASSERT(is_kernel_address(va));

    zx_status_t status = VmAspace::kernel_aspace()->FreeRegion(va);
    if (status != ZX_OK) {
        TRACEF("warning: vmfree at %p failed\n", ptr);
    }
}

