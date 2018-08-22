// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddk/io-buffer.h>
#include <ddk/protocol/i2c.h>
#include <ddk/protocol/platform-device.h>
#include <ddktl/device.h>
#include <ddktl/device-internal.h>
#include <ddktl/mmio.h>
#include <zircon/listnode.h>
#include <lib/zx/bti.h>
#include <lib/zx/vmo.h>
#include <fbl/mutex.h>
#include <fbl/ref_ptr.h>
#include <fbl/unique_ptr.h>
#include <fbl/vector.h>

#include <soc/aml-common/aml-tdm.h>
#include "aml-audio.h"


class AmlTdmDevice : public fbl::unique_ptr<AmlTdmDevice> {

public:
    static fbl::unique_ptr<AmlTdmDevice> Create(ddk::MmioBlock&& mmio);

    //Configure an mclk channel (a..f) with source and divider
    zx_status_t SetMclk(uint32_t ch, ee_audio_mclk_src_t src, uint32_t div);
    //Configure an sclk/lclk generator block
    zx_status_t SetSclk(uint32_t ch, uint32_t sdiv,
                        uint32_t lrduty, uint32_t lrdiv);
    //Configure signals driving the output block (sclk, lrclk)
    zx_status_t SetTdmOutClk(uint32_t tdm_blk, uint32_t sclk_src,
                                       uint32_t lrclk_src, bool inv);

    void AudioClkEna(uint32_t audio_blk_mask);
private:
    //static int IrqThread(void* arg);

    friend class fbl::unique_ptr<AmlTdmDevice>;

    AmlTdmDevice() { };

    void InitRegs();

    uint32_t tdm_out_ch_;    //Which tdm output block this instance uses
    uint32_t frddr_ch_;     //which fromddr channel is used by this instance

    ddk::MmioBlock mmio_;

    virtual ~AmlTdmDevice();


#if 0
    fbl::Mutex lock_;
    fbl::Mutex req_lock_ __TA_ACQUIRED_AFTER(lock_);

    // Dispatcher framework state
    fbl::RefPtr<dispatcher::Channel> stream_channel_ __TA_GUARDED(lock_);
    fbl::RefPtr<dispatcher::Channel> rb_channel_     __TA_GUARDED(lock_);
    fbl::RefPtr<dispatcher::ExecutionDomain> default_domain_;

    uint32_t ring_buffer_phys_  = 0;
    uint32_t ring_buffer_size_  = 0;
#endif
};
