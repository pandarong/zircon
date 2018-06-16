// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zircon/compiler.h>
#include <zircon/types.h>

__BEGIN_CDECLS;

typedef struct {
    zx_status_t (*do_usb_tuning)(void* ctx, bool host, bool set_default);
} astro_usb_protocol_ops_t;

typedef struct {
    astro_usb_protocol_ops_t* ops;
    void* ctx;
} astro_usb_protocol_t;

static inline zx_status_t astro_usb_do_usb_tuning(astro_usb_protocol_t* usb, bool host,
                                                  bool set_default) {
    return usb->ops->do_usb_tuning(usb->ctx, host, set_default);
}

__END_CDECLS;
