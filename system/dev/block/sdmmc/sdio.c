// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ddk/device.h>
#include <ddk/debug.h>
#include <ddk/protocol/sdmmc.h>

#include <pretty/hexdump.h>

#include "sdmmc.h"

#define FREQ_200MHZ 200000000
#define FREQ_52MHZ 52000000
#define FREQ_25MHZ 25000000

zx_status_t sdmmc_probe_sdio(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = ZX_ERR_NOT_SUPPORTED;
    return st;
}
