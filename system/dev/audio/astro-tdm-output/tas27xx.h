// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddk/debug.h>
#include <ddktl/pdev.h>
#include <fbl/unique_ptr.h>

#define SW_RESET    (0x01)      //sw reset
#define PWR_CTL     (0x02)      //power control
#define PB_CFG2     (0x05)      //pcm gain register

#define TDM_CFG0    (0x0a)
#define TDM_CFG1    (0x0b)
#define TDM_CFG2    (0x0c)
#define TDM_CFG3    (0x0d)
#define TDM_CFG4    (0x0e)
#define TDM_CFG5    (0x0f)
#define TDM_CFG6    (0x10)
#define TDM_CFG7    (0x11)
#define TDM_CFG8    (0x12)
#define TDM_CFG9    (0x13)
#define TDM_CFG10   (0x14)

#define CLOCK_CFG   (0x3c)      //Clock Config


namespace audio {
namespace astro {

class Tas27xx : public fbl::unique_ptr<Tas27xx>{
public:
    static fbl::unique_ptr<Tas27xx> Create(ddk::I2cChannel&& i2c);
    bool ValidGain(float gain);
    zx_status_t SetGain(float gain);
    zx_status_t GetGain(float *gain);
    zx_status_t Init();
    zx_status_t Reset();
    zx_status_t Standby();
    zx_status_t ExitStandby();
    uint8_t ReadReg(uint8_t reg);

private:
    friend class fbl::unique_ptr<Tas27xx>;
    static constexpr float kMaxGain = 0;
    static constexpr float kMinGain = -100.0;
    Tas27xx();
    ~Tas27xx();


    zx_status_t WriteReg(uint8_t reg, uint8_t value);

    zx_status_t SetStandby(bool stdby);

    ddk::I2cChannel i2c_;

    float current_gain_ = 0;
};
} // namespace astro
} // namespace audio