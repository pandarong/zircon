// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zircon/types.h>
#include <zircon/syscalls/pci.h>
#include <ddk/protocol/pciroot.h>

#include "acpi-private.h"

static zx_status_t pciroot_op_get_auxdata(void* context, const char* args,
                                          void* data, uint32_t bytes,
                                          uint32_t* actual);
static zx_status_t pciroot_op_get_bti(void* context, uint32_t bdf, uint32_t index,
                                      zx_handle_t* bti);
static zx_status_t pciroot_op_get_pci_mcfgs(void* context, zx_pci_init_arg_t** arg, size_t* size);

pciroot_protocol_ops_t* get_pciroot_proto(void);
