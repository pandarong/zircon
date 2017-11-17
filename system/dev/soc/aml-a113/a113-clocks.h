// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddk/io-buffer.h>
#include <ddk/protocol/platform-bus.h>

#include <zircon/listnode.h>

#include "a113-bus.h"

#define A113_CLOCKS_BASE_PHYS 0xff63c000

typedef struct {
    a113_bus_t      *host_bus;
    io_buffer_t     regs_iobuff;
    zx_vaddr_t      virt_regs;
} a113_clk_dev_t;


static inline uint32_t a113_clk_get_reg(a113_clk_dev_t *dev, uint32_t offset) {
    return   ((uint32_t *)dev->virt_regs)[offset];
}

static inline uint32_t a113_clk_set_reg(a113_clk_dev_t *dev, uint32_t offset, uint32_t value) {
    ((uint32_t *)dev->virt_regs)[offset] = value;
    return   ((uint32_t *)dev->virt_regs)[offset];
}

zx_status_t a113_clk_init(a113_clk_dev_t **device, a113_bus_t *host_bus);