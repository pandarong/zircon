// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/protocol/i2c.h>
#include <fbl/algorithm.h>
#include <fbl/alloc_checker.h>

#include "tas27xx.h"

namespace audio {
namespace astro {

constexpr float Tas27xx::kMaxGain;
constexpr float Tas27xx::kMinGain;

// static
fbl::unique_ptr<Tas27xx> Tas27xx::Create(ddk::I2cChannel&& i2c) {
    if (!i2c.is_valid()) {
        return nullptr;
    }

    fbl::AllocChecker ac;

    auto ptr = fbl::unique_ptr<Tas27xx>(new (&ac) Tas27xx());
    if (!ac.check()) {
        return nullptr;
    }
    ptr->i2c_ = i2c.release();

    return ptr;
}
Tas27xx::~Tas27xx() {}

Tas27xx::Tas27xx() {}

zx_status_t Tas27xx::Reset(){
    return WriteReg(SW_RESET, 0x01);
}

zx_status_t Tas27xx::SetGain(float gain) {
    gain = fbl::clamp(gain, kMinGain, kMaxGain);
    uint8_t gain_reg = static_cast<uint8_t>(-2*gain);

    zx_status_t status;
    status = WriteReg(PB_CFG2, gain_reg);
    if (status == ZX_OK) {
        current_gain_ = gain;
    }
    return status;
}

zx_status_t Tas27xx::GetGain(float *gain) {
    *gain = current_gain_;
    return ZX_OK;
}

bool Tas27xx::ValidGain(float gain) {
    return (gain <= kMaxGain) && (gain >= kMinGain);
}

zx_status_t Tas27xx::Init() {
    zx_status_t status;

    //Put part in active, but muted state
    Standby();

    // 128 clocks per frame, manually configure dividers
    status = WriteReg(CLOCK_CFG, (0x06 << 2) | 1);
    if (status != ZX_OK) return status;

    // 48kHz, FSYNC on low to high transition
    status = WriteReg(TDM_CFG0, (1 << 4) | (0x03 << 1) | 1);
    if (status != ZX_OK) return status;

    // Left justified, offset 1 bclk, clock on falling edge of sclk
    status = WriteReg(TDM_CFG1, (1 << 1) |  1);
    if (status != ZX_OK) return status;

    // Mono (L+R)/2, 32bit sample, 32bit slot
    status = WriteReg(TDM_CFG2, (0x03 << 4) | (0x03 << 2) | 0x03);
    if (status != ZX_OK) return status;

    // Left channel slot 0, Right channel slot 1
    status = WriteReg(TDM_CFG3, (1 << 4) | 0);
    if (status != ZX_OK) return status;

    SetGain(-20);
    // Disable v and i sense, enter active mode
    status = WriteReg(PWR_CTL, (0x03 << 2));
    if (status != ZX_OK) return status;

    return ZX_OK;
}

//Standby puts the part in active, but muted state
zx_status_t Tas27xx::Standby() {
    return WriteReg(PWR_CTL, (0x03 << 2) | 0x01);
}

zx_status_t Tas27xx::ExitStandby() {
    return WriteReg(PWR_CTL, (0x03 << 2) );
}

uint8_t Tas27xx::ReadReg(uint8_t reg) {
    uint8_t val;
    i2c_.Read(reg, &val, 1);
    return val;
}

zx_status_t Tas27xx::WriteReg(uint8_t reg, uint8_t value) {
    uint8_t write_buf[2];
    write_buf[0] = reg;
    write_buf[1] = value;
    return i2c_.Write(write_buf,2);
}
} //namespace astro
} //namespace audio