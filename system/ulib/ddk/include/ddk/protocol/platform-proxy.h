// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zircon/compiler.h>
#include <zircon/types.h>

__BEGIN_CDECLS;

typedef zx_status_t (*platform_proxy_cb)(const void* req_buf, uint32_t req_size, void* rsp_buf,
                                             uint32_t rsp_buf_size, uint32_t* out_rsp_actual);

typedef struct {
    zx_status_t (*set_protocol)(void* ctx, uint32_t proto_id, void* protocol);
    zx_status_t (*proxy)(void* ctx, uint32_t proto_id, const void* req_buf, uint32_t req_size,
                         void* rsp_buf, uint32_t rsp_buf_size, uint32_t* out_rsp_actual);
} platform_proxy_protocol_ops_t;

typedef struct {
    platform_proxy_protocol_ops_t* ops;
    void* ctx;
} platform_proxy_protocol_t;

__END_CDECLS;
