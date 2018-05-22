// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stdlib.h>
#include <string.h>
#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/platform-defs.h>

#define AML_TDM_METADATA (0x4d445464)


zx_status_t aml_tdm_bind(void* ctx, zx_device_t* parent) {
    zxlogf(INFO,"Made it to %s\n",__func__);

    size_t actual;
    uint8_t buffer[128];
    zx_status_t status = device_get_metadata(parent, AML_TDM_METADATA, buffer,
                                             sizeof(buffer), &actual);
    zxlogf(INFO,"amltdm got back status=%d  bytes=%lu\n",status,actual);
    if (status == ZX_OK) {
        zxlogf(INFO,"String = %s\n",buffer);
    }

    return ZX_OK;
}



//extern void gauss_tdm_release(void*);

static zx_driver_ops_t aml_tdm_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = aml_tdm_bind,
    //.release = gauss_tdm_release,
};

// clang-format off
ZIRCON_DRIVER_BEGIN(aml_tdm, aml_tdm_driver_ops, "aml-tdm", "0.1", 3)
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_AMLOGIC),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_PID, PDEV_PID_AMLOGIC_S905D2),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_DID, PDEV_DID_AMLOGIC_TDM),
ZIRCON_DRIVER_END(aml_tdm)
// clang-format on
