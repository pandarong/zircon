// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddk/io-buffer.h>
#include <ddk/protocol/i2c.h>
#include <ddk/protocol/platform-device.h>
#include <ddktl/device.h>
#include <ddktl/device-internal.h>
#include <ddktl/gpio_pin.h>
#include <zircon/listnode.h>
#include <lib/zx/bti.h>
#include <lib/zx/vmo.h>
#include <fbl/mutex.h>
#include <fbl/vector.h>

#include <audio-proto/audio-proto.h>
#include <dispatcher-pool/dispatcher-channel.h>
#include <dispatcher-pool/dispatcher-execution-domain.h>
#include <dispatcher-pool/dispatcher-timer.h>

#include "aml-tdm.h"

namespace audio {
namespace astro {


class AmlAudioStream;
using AmlAudioStreamBase = ddk::Device<AmlAudioStream,
                                       ddk::Ioctlable,
                                       ddk::Unbindable>;

class AmlAudioStream : public AmlAudioStreamBase,
                       public fbl::RefCounted<AmlAudioStream> {
public:
    static zx_status_t Create(zx_device_t* parent);

    //void PrintDebugPrefix() const;

    // DDK device implementation
    void DdkUnbind();
    void DdkRelease();
    zx_status_t DdkIoctl(uint32_t op,
                         const void* in_buf, size_t in_len,
                         void* out_buf, size_t out_len, size_t* out_actual);

private:

    friend class fbl::RefPtr<AmlAudioStream>;

    // TODO(hollande) - the fifo bytes are adjustable on the audio fifos and should be scaled
    //                  with the desired sample rate.  Since this first pass has a fixed sample
    //                  sample rate we will set as constant for now.
    //                  We are using fifo C at this stage, which is max of 128 (64-bit wide)
    //                  Using 64 levels for now.
    static constexpr uint8_t kFifoDepth = 0x40;

    AmlAudioStream(zx_device_t* parent)
        : AmlAudioStreamBase(parent) { }

    platform_device_protocol_t pdev_;

    fbl::unique_ptr<AmlTdmDevice> tdm_;
    ddk::GpioPin audio_en_;
    ddk::GpioPin audio_fault_;

    virtual ~AmlAudioStream();

    //zx_status_t Bind(const char* devname);


};

}  // namespace usb
}  // namespace audio
