// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/binding.h>
#include <ddk/device.h>
#include <ddk/driver.h>

#include <zircon/syscalls.h>
#include <zircon/threads.h>
#include <zircon/types.h>

// ioctls: #include <zircon/device/oom.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

static zx_status_t oom_ioctl(void* ctx, uint32_t op,
                             const void* cmd, size_t cmdlen,
                             void* reply, size_t max, size_t* out_actual) {
    switch (op) {
    // case IOCTL_OOM_START: {
    //     if (cmdlen != sizeof(uint32_t)) {
    //         return ZX_ERR_INVALID_ARGS;
    //     }
    //     uint32_t group_mask = *(uint32_t *)cmd;
    //     return zx_oom_control(get_root_resource(), OOM_ACTION_START, group_mask, NULL);
    // }
    // case IOCTL_OOM_STOP: {
    //     zx_oom_control(get_root_resource(), OOM_ACTION_STOP, 0, NULL);
    //     zx_oom_control(get_root_resource(), OOM_ACTION_REWIND, 0, NULL);
    //     return ZX_OK;
    // }
    // TODO: get a port that's notified on lowmem events, at certain thresholds,
    // requests for cache clearing, ...
    default:
        return ZX_ERR_INVALID_ARGS;
    }
}

static /*const*/ zx_protocol_device_t oom_device_proto = {
    .version = DEVICE_OPS_VERSION,
    .ioctl = oom_ioctl,
};

static int ranker_thread(void *arg) {
    while (true) {
        zx_nanosleep(zx_deadline_after(ZX_SEC(1)));
    }
    return 0;
}

// Type of the |cookie| args to zx_driver_ops_t functions.
typedef struct {
    zx_device_t* dev;
    thrd_t ranker_thread;
} oom_cookie_t;

static zx_status_t oom_bind(void* unused_ctx, zx_device_t* parent,
                            void** out_cookie) {
    // Add the device.
    /*const*/ device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "oom",
        // TODO(dbort): Use .ctx to hold a channel to the ranker thread, and
        // maybe dispatchers that will notify observers.
        .ops = &oom_device_proto,
    };
    zx_device_t* dev;
    zx_status_t s = device_add(parent, &args, &dev);
    if (s != ZX_OK) {
        return s;
    }

    // Allocate the cookie.
    oom_cookie_t *cookie = (oom_cookie_t*)calloc(1, sizeof(oom_cookie_t));
    if (cookie == NULL) {
        device_remove(dev);
        return ZX_ERR_NO_MEMORY;
    }
    cookie->dev = dev;

    // Start the thread.
    int ts = thrd_create_with_name(
        &cookie->ranker_thread, ranker_thread, NULL, "ranker");
    if (ts != thrd_success) {
        device_remove(dev);
        free(cookie);
        return thrd_status_to_zx_status(ts);
    }

    *out_cookie = cookie;
    return ZX_OK;
}

void oom_unbind(void* unused_ctx, zx_device_t* parent, void* cookie) {
    //xxx kill, join the thread. Ask it to die nicely
    // thrd_join(cookie->ranker_thread, NULL);
    memset(cookie, 0, sizeof(oom_cookie_t));
    free(cookie);
}

static /*const*/ zx_driver_ops_t oom_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = oom_bind,
    .unbind = oom_unbind,
};

ZIRCON_DRIVER_BEGIN(oom, oom_driver_ops, "zircon", "0.1", 1)
BI_MATCH_IF(EQ, BIND_PROTOCOL, ZX_PROTOCOL_MISC_PARENT)
,
    ZIRCON_DRIVER_END(oom)
