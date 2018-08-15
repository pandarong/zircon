// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddk/protocol/platform-proxy.h>
#include <ddktl/device-internal.h>
#include <zircon/assert.h>

#include "platform-proxy-internal.h"

// DDK platform proxy protocol support.
//
// :: Proxies ::
//
// ddk::PlatformProxyProtocolProxy is a simple wrappers around platform_proxy_protocol_t. It does
// not own the pointers passed to it.
//
// :: Mixins ::
//
// ddk::PlatformProxyProtocol is a mixin class that simplifies writing DDK drivers that
// implement the platform proxy protocol.
//
// :: Examples ::
//
// // A driver that implements a ZX_PROTOCOL_PLATFORM_PROXY device.
// class PlatformProxyDevice;
// using PlatformProxyDeviceType = ddk::Device<PlatformProxyDevice, /* ddk mixins */>;
//
// class PlatformProxyDevice : public PlatformProxyDeviceType,
//                             public ddk::PlatformProxyProtocol<PlatformProxyDevice> {
//   public:
//     PlatformProxyDevice(zx_device_t* parent)
//       : PlatformProxyDeviceType("my-platform-proxy", parent) {}
//
//     zx_status_t SetProtocol(uint32_t proto_id, void* protocol);
//     zx_status_t Proxy(uint32_t proto_id, const void* req_buf, uint32_t req_size, void* rsp_buf,
//                       uint32_t rsp_buf_size, uint32_t* out_rsp_actual);
//     ...
// };

namespace ddk {

template <typename D>
class PlatformProxyProtocol : public internal::base_protocol {
public:
    PlatformProxyProtocol() {
        internal::CheckPlatformProxyProtocolSubclass<D>();
        platform_proxy_proto_ops_.set_protocol = SetProtocol;
        platform_proxy_proto_ops_.proxy = Proxy;

       // Can only inherit from one base_protocol implementation.
        ZX_ASSERT(ddk_proto_id_ == 0);
        ddk_proto_id_ = ZX_PROTOCOL_PLATFORM_PROXY;
        ddk_proto_ops_ = &platform_proxy_proto_ops_;
    }

protected:
    platform_proxy_protocol_ops_t platform_proxy_proto_ops_ = {};

private:
    static zx_status_t SetProtocol(void* ctx, uint32_t proto_id, void* protocol) {
        return static_cast<D*>(ctx)->SetProtocol(proto_id, protocol);
    }

    static zx_status_t Proxy(void* ctx, uint32_t proto_id, const void* req_buf, uint32_t req_size,
                         void* rsp_buf, uint32_t rsp_buf_size, uint32_t* out_rsp_actual) {
        return static_cast<D*>(ctx)->Proxy(proto_id, req_buf, req_size, rsp_buf, rsp_buf_size,
                                           out_rsp_actual);
    }
};

class PlatformProxyProtocolProxy {
public:
    PlatformProxyProtocolProxy(platform_proxy_protocol_t* proto)
        : ops_(proto->ops), ctx_(proto->ctx) {}

    zx_status_t SetProtocol(uint32_t proto_id, void* protocol) {
        return ops_->set_protocol(ctx_, proto_id, protocol);
    }

    zx_status_t Proxy(uint32_t proto_id, const void* req_buf, uint32_t req_size, void* rsp_buf,
                      uint32_t rsp_buf_size, uint32_t* out_rsp_actual) {
        return ops_->proxy(ctx_, proto_id, req_buf, req_size, rsp_buf, rsp_buf_size,
                           out_rsp_actual);
    }

private:
    platform_proxy_protocol_ops_t* ops_;
    void* ctx_;
};

} // namespace ddk
