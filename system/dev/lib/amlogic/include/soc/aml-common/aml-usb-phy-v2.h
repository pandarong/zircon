// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddk/io-buffer.h>
#include <zircon/types.h>
#include <threads.h>

typedef struct {
    io_buffer_t usbctrl_buf;
    io_buffer_t reset_buf;
    io_buffer_t phy20_buf;
    io_buffer_t phy21_buf;
    volatile void* usbctrl_regs;
    zx_handle_t iddig_irq_handle;
    thrd_t iddig_irq_thread;
} aml_usb_phy_v2_t;

zx_status_t aml_usb_phy_v2_init(aml_usb_phy_v2_t* phy, zx_handle_t bti, bool host);
