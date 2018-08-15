// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform-proxy-host.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <fbl/unique_ptr.h>

#include "platform-proxy.h"

namespace platform_bus {

zx_status_t ProxyHost::Create(uint32_t proto_id, zx_device_t* parent,
                              fbl::RefPtr<PlatformProxy> proxy) {
    fbl::AllocChecker ac;
    fbl::unique_ptr<platform_bus::ProxyHost> host(new (&ac)
                                            platform_bus::ProxyHost(proto_id, parent, proxy));
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    char name[ZX_DEVICE_NAME_MAX];
    snprintf(name, sizeof(name), "ProxyHost[%08x]", proto_id);

    zx_device_prop_t props[] = {
        {BIND_PLATFORM_PROTO, 0, proto_id},
    };

    auto status = host->DdkAdd(name, 0, props, countof(props));
    if (status != ZX_OK) {
        return status;
    }

    // devmgr is now in charge of the device.
    __UNUSED auto* dummy = host.release();
    return ZX_OK;
}

void ProxyHost::DdkRelease() {
    delete this;
}

zx_status_t ProxyHost::SetProtocol(uint32_t proto_id, void* protocol) {
return -1;

}

zx_status_t ProxyHost::Proxy(uint32_t proto_id, const void* req_buf, uint32_t req_size,
                             void* rsp_buf, uint32_t rsp_buf_size, uint32_t* out_rsp_actual) {
return -1;
}

} // namespace platform_bus
