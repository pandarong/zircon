// Copyright 2016 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <object/resource_dispatcher.h>

#include <zircon/rights.h>
#include <fbl/alloc_checker.h>

#include <kernel/auto_lock.h>
#include <lib/pasm/pasm.h>
#include <string.h>

namespace {
static constexpr uint32_t resource_kind_to_allocator(uint64_t kind) {
    DEBUG_ASSERT(kind == ZX_RSRC_KIND_MMIO || kind == ZX_RSRC_KIND_IOPORT ||
                 kind == ZX_RSRC_KIND_IRQ);
    switch (kind) {
        case ZX_RSRC_KIND_MMIO:
            return PhysicalAspaceManager::kMmioAllocator;
        case ZX_RSRC_KIND_IOPORT:
            return PhysicalAspaceManager::kIoAllocator;
        case ZX_RSRC_KIND_IRQ:
            return PhysicalAspaceManager::kIrqAllocator;
    }

    // Return an invalid PhysicalAspaceManager region which will return an error in the Pasm calls.
    return UINT32_MAX;
}
} // namespace anon

zx_status_t ResourceDispatcher::Create(fbl::RefPtr<ResourceDispatcher>* dispatcher,
                                       zx_rights_t* rights, uint32_t kind,
                                       uint64_t base, uint64_t len) {
    if (kind >= ZX_RSRC_KIND_COUNT) {
        return ZX_ERR_INVALID_ARGS;
    }

    fbl::AllocChecker ac;
    ResourceDispatcher* disp = new (&ac) ResourceDispatcher(kind, base, len);
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    zx_status_t st = disp->Initialize();
    if (st != ZX_OK) {
        delete disp;
        return st;
    }

    *rights = ZX_DEFAULT_RESOURCE_RIGHTS;
    *dispatcher = fbl::AdoptRef<ResourceDispatcher>(disp);
    return ZX_OK;
}

zx_status_t ResourceDispatcher::Initialize() {
    zx_status_t st = ZX_OK;

    switch (kind_) {
    // The root resource has full access to everything in the kernel so there is
    // no need for it to make a PASM allocation.
    case ZX_RSRC_KIND_ROOT:
    // TODO(cja): The Hypervisor resource is a binary one, that may make more
    // sense as a capability in the future.
    case ZX_RSRC_KIND_HYPERVISOR:
        return ZX_OK;
    default:;
        auto& pasm = PhysicalAspaceManager::Get();
        DEBUG_ASSERT(pasm);

        st = pasm->ReserveAddressSpace(resource_kind_to_allocator(kind_), base_, len_, region_);
        if (st != ZX_OK) {
            return st;
        }
        printf("resource: got region %#" PRIxPTR " size %#lx\n", region_->base, region_->size);
    }

    return st;
}

ResourceDispatcher::ResourceDispatcher(uint32_t kind, uint64_t base, uint64_t len) :
    kind_(kind), base_(base), len_(len) {
}

zx_status_t ResourceDispatcher::CreateChildResource(fbl::RefPtr<ResourceDispatcher>* dispatcher, zx_rights_t* rights,
                                                    uint32_t kind, uint64_t base, uint64_t len) {
    // New resources may only be created if we are root and not trying
    // to create root.
    if (kind_ != ZX_RSRC_KIND_ROOT || kind == ZX_RSRC_KIND_ROOT) {
        return ZX_ERR_ACCESS_DENIED;
    }

    fbl::AllocChecker ac;
    ResourceDispatcher* disp = new (&ac) ResourceDispatcher(kind, base, len);
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    zx_status_t st = disp->Initialize();
    if (st != ZX_OK) {
        delete disp;
        return st;
    }

    if (rights) {
        *rights = ZX_DEFAULT_RESOURCE_RIGHTS;
    }
    *dispatcher = fbl::AdoptRef<ResourceDispatcher>(disp);
    return ZX_OK;
}

ResourceDispatcher::~ResourceDispatcher() {
}
