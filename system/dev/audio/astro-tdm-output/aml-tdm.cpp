// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <audio-proto-utils/format-utils.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <fbl/limits.h>
#include <string.h>
#include <zircon/device/audio.h>
#include <lib/zx/vmar.h>
#include <ddk/protocol/platform-device.h>

#include "aml-tdm.h"
#include "aml-audio.h"


fbl::unique_ptr<AmlTdmDevice> AmlTdmDevice::Create(ddk::MmioBlock&& mmio) {

    auto tdm_dev = fbl::unique_ptr<AmlTdmDevice>(new AmlTdmDevice());
    zxlogf(INFO,"before\n");
    mmio.Info();
    tdm_dev->mmio_ = mmio.release();
    zxlogf(INFO,"after\n");
    mmio.Info();
    tdm_dev->mmio_.Info();

    tdm_dev->InitRegs();

    return tdm_dev;
}


/* Notes
    -div is desired divider minus 1. (want /100? write 99)
*/
zx_status_t AmlTdmDevice::SetMclk(uint32_t ch, ee_audio_mclk_src_t src, uint32_t div) {
    zx_off_t ptr = EE_AUDIO_MCLK_A_CTRL + (ch * sizeof(uint32_t));
    mmio_.Write(EE_AUDIO_MCLK_ENA | (src << 24) | (div & 0xffff), ptr);
    return ZX_OK;
}

/* Notes:
    -sdiv is desired divider -1 (Want a divider of 10? write a value of 9)
    -sclk needs to be at least 2x mclk.  writing a value of 0 (/1) to sdiv
        will result in no sclk being generated on the sclk pin.  However, it
        appears that it is running properly as a lrclk is still generated at
        an expected rate (lrclk is derived from sclk)
*/
zx_status_t AmlTdmDevice::SetSclk(uint32_t ch, uint32_t sdiv,
                                  uint32_t lrduty, uint32_t lrdiv) {
    zx_off_t ptr = EE_AUDIO_MST_A_SCLK_CTRL0 + 2 * ch * sizeof(uint32_t);
    mmio_.Write(    (0x3 << 30) |      //Enable the channel
                    (sdiv << 20) |     // sclk divider sclk=mclk/sdiv
                    (lrduty << 10) |   // lrclk duty cycle in sclk cycles
                    (lrdiv << 0),      // lrclk = sclk/lrdiv
                    ptr);
    mmio_.Write(0, ptr + sizeof(uint32_t));           //Clear delay lines for phases
    return ZX_OK;
}

zx_status_t AmlTdmDevice::SetTdmOutClk(uint32_t tdm_blk, uint32_t sclk_src,
                                       uint32_t lrclk_src, bool inv) {
    zx_off_t ptr = EE_AUDIO_CLK_TDMOUT_A_CTL + tdm_blk * sizeof(uint32_t);
    mmio_.Write( (0x3 << 30) | //Enable the clock
                 (inv ? (1 << 29) : 0) | //invert sclk
                 (sclk_src << 24) |
                 (lrclk_src << 20), ptr);
    return ZX_OK;
}

void AmlTdmDevice::AudioClkEna(uint32_t audio_blk_mask) {
    mmio_.SetBits( audio_blk_mask, EE_AUDIO_CLK_GATE_EN);
}

void AmlTdmDevice::InitRegs() {

    //uregs_->SetBits(0x00002000, AML_TDM_CLK_GATE_EN);


}

AmlTdmDevice::~AmlTdmDevice() {

}