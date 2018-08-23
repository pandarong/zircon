
#include <ddk/debug.h>
#include <ddktl/pdev.h>

#include "audio.h"
#include "aml-audio.h"

namespace audio {
namespace astro {
/*
    Assumes the GPIO for all resources are configured in the board file
*/
zx_status_t AmlAudioStream::Create(zx_device_t* parent) {

    fbl::AllocChecker ac;

    __UNUSED auto stream = fbl::AdoptRef(new (&ac) AmlAudioStream(parent));
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    fbl::RefPtr<ddk::Pdev> pdev = ddk::Pdev::Create(parent);

    ddk::MmioBlock mmio;
    mmio = pdev->GetMmio(0);

    if (!mmio.isMapped()) {
        zxlogf(ERROR,"AmlAudio: Failed to allocate mmio\n");
        return ZX_ERR_NO_RESOURCES;
    }

    stream->audio_fault_ = pdev->GetGpio(0);
    stream->audio_en_ = pdev->GetGpio(1);
    if (!(stream->audio_fault_.is_valid() && stream->audio_en_.is_valid())) {
        zxlogf(ERROR,"%s failed to allocate gpio\n", __func__);
        return ZX_ERR_NO_RESOURCES;
    }

    stream->codec_ = Tas27xx::Create(pdev->GetI2cChan(0).release());
    if (!stream->codec_) {
        zxlogf(ERROR,"%s could not get tas27xx\n", __func__);
        return ZX_ERR_NO_RESOURCES;
    }

    for (uint8_t i = 0; i<20; i++) {
        zxlogf(INFO,"TAS reg%02x = %02x\n", i, stream->codec_->ReadReg(i));
    }

    stream->tdm_ = AmlTdmDevice::Create(mmio.release());
    if (stream->tdm_ == nullptr) {
        zxlogf(ERROR,"%s failed to create tdm device\n", __func__);
        return ZX_ERR_NO_MEMORY;
    }

    stream->tdm_->SetMclk(EE_AUDIO_MCLK_A, HIFI_PLL, 124);
    stream->tdm_->SetSclk(EE_AUDIO_MCLK_A, 1, 0, 127);
    stream->tdm_->SetTdmOutClk(EE_AUDIO_TDMOUTB, EE_AUDIO_MCLK_A,
                               EE_AUDIO_MCLK_A, false);
    stream->tdm_->AudioClkEna(EE_AUDIO_CLK_GATE_TDMOUTB);

    zxlogf(INFO,"%s created successfully\n",__func__);
    __UNUSED auto dummy = stream.leak_ref();
    return ZX_OK;
}

    // DDK device implementation
void AmlAudioStream::DdkUnbind() {}
void AmlAudioStream::DdkRelease() {}

zx_status_t AmlAudioStream::DdkIoctl(uint32_t op,
                         const void* in_buf, size_t in_len,
                         void* out_buf, size_t out_len, size_t* out_actual)
{
    return ZX_OK;
}

AmlAudioStream::~AmlAudioStream(void) {}

} //astro
} //audio

extern "C" zx_status_t audio_bind(void* ctx, zx_device_t* device, void** cookie) {
    audio::astro::AmlAudioStream::Create(device);
    return ZX_OK;
}

