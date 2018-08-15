// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddktl/device.h>
#include <ddktl/protocol/platform-proxy.h>
#include <fbl/ref_ptr.h>
#include <fbl/vector.h>
#include <lib/zx/channel.h>
#include <lib/zx/handle.h>

#include "platform-proxy.h"

namespace platform_bus {

class ProxyHost;
using ProxyHostType = ddk::Device<ProxyHost>;

class ProxyHost : public ProxyHostType,
                  public ddk::PlatformProxyProtocol<ProxyHost> {
public:
    static zx_status_t Create(uint32_t proto_id, zx_device_t* parent,
                              fbl::RefPtr<PlatformProxy> proxy);

    // Device protocol implementation.
    void DdkRelease();

    // Platform proxy protocol implementation.
     zx_status_t SetProtocol(uint32_t proto_id, void* protocol);
     zx_status_t Proxy(uint32_t proto_id, const void* req_buf, uint32_t req_size, void* rsp_buf,
                       uint32_t rsp_buf_size, uint32_t* out_rsp_actual);

private:
    explicit ProxyHost(uint32_t proto_id, zx_device_t* parent, fbl::RefPtr<PlatformProxy> proxy)
        :  ProxyHostType(parent), proto_id_(proto_id), proxy_(proxy) {}

    DISALLOW_COPY_ASSIGN_AND_MOVE(ProxyHost);

    uint32_t proto_id_;
    fbl::RefPtr<PlatformProxy> proxy_;
};

} // namespace platform_bus
