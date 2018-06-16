// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

// Standard Includes
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

// DDK includes
#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/io-buffer.h>
#include <ddk/protocol/astro-usb.h>
#include <ddk/protocol/platform-defs.h>
#include <ddk/protocol/platform-device.h>
#include <ddk/protocol/usb-bus.h>
#include <ddk/protocol/usb-dci.h>
#include <ddk/protocol/usb-hci.h>
#include <ddk/protocol/usb-mode-switch.h>
#include <ddk/protocol/usb.h>

// Zircon USB includes
#include <zircon/hw/usb-hub.h>
#include <zircon/hw/usb.h>
#include <sync/completion.h>

#include <zircon/listnode.h>
#include <zircon/process.h>

#include "usb_dwc_regs.h"

#define ENABLE_MPI 1

#define DWC_MAX_EPS    32

typedef enum dwc_ep0_state {
    EP0_STATE_DISCONNECTED,
    EP0_STATE_IDLE,
    EP0_STATE_DATA_OUT,
    EP0_STATE_DATA_IN,
    EP0_STATE_STATUS,
    EP0_STATE_STALL,
} dwc_ep0_state_t;

typedef struct {
    list_node_t queued_reqs;    // requests waiting to be processed
    usb_request_t* current_req; // request currently being processed
    uint32_t txn_offset;
    uint32_t txn_length;    

    // Used for synchronizing endpoint state
    // and ep specific hardware registers
    // This should be acquired before dwc3_t.lock
    // if acquiring both locks.
    mtx_t lock;

    uint16_t max_packet_size;
    uint8_t ep_num;
    bool enabled;
    uint8_t type;           // control, bulk, interrupt or isochronous
    uint8_t interval;

    bool got_not_ready;
    bool stalled;
} dwc_endpoint_t;

typedef struct {
    zx_device_t* zxdev;
    usb_bus_interface_t bus;
    zx_handle_t irq_handle;
    zx_handle_t bti_handle;
    thrd_t irq_thread;
    zx_device_t* parent;

    astro_usb_protocol_t astro_usb;

    dwc_regs_t* regs;

    // device stuff
    dwc_endpoint_t eps[DWC_MAX_EPS];

    usb_dci_interface_t dci_intf;
    bool configured;

    usb_setup_t cur_setup;    
    dwc_ep0_state_t ep0_state;
    io_buffer_t ep0_buffer;
    bool got_setup;
} dwc_usb_t;

// dwc2-device.c
extern usb_dci_protocol_ops_t dwc_dci_protocol;

void dwc_handle_reset_irq(dwc_usb_t* dwc);
void dwc_handle_enumdone_irq(dwc_usb_t* dwc);
void dwc_handle_rxstsqlvl_irq(dwc_usb_t* dwc);
void dwc_handle_inepintr_irq(dwc_usb_t* dwc);
void dwc_handle_outepintr_irq(dwc_usb_t* dwc);
void dwc_handle_nptxfempty_irq(dwc_usb_t* dwc);
void dwc_handle_usbsuspend_irq(dwc_usb_t* dwc);
void dwc_flush_fifo(dwc_usb_t* dwc, const int num);
