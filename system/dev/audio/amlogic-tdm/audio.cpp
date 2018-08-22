
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

    ddk::I2cChannel i2c;
    i2c = pdev->GetI2cChan(0);
    if (!i2c.is_valid()) {
        zxlogf(ERROR,"%s failed to init i2c\n", __func__);
        return ZX_ERR_NO_RESOURCES;
    } else {
        zxlogf(INFO,"i2c created successfully\n");
    }

    stream->audio_fault_ = pdev->GetGpio(0);
    stream->audio_en_ = pdev->GetGpio(1);
    if (!(stream->audio_fault_.is_valid() && stream->audio_en_.is_valid())) {
        zxlogf(ERROR,"%s failed to allocate gpio\n", __func__);
        return ZX_ERR_NO_RESOURCES;
    }


    stream->tdm_ = AmlTdmDevice::Create(mmio.release());
    if (stream->tdm_ == nullptr) {
        zxlogf(ERROR,"%s failed to create tdm device\n",__func__);
        return ZX_ERR_NO_MEMORY;
    }
    zxlogf(INFO, "Setting mclk\n");
    stream->tdm_->SetMclk(EE_AUDIO_MCLK_A, HIFI_PLL, 124);

    zxlogf(INFO, "Setting sclk\n");
    stream->tdm_->SetSclk(EE_AUDIO_MCLK_A, 1, 0, 127);

    zxlogf(INFO, "Setting tdmoutclk\n");
    stream->tdm_->SetTdmOutClk(EE_AUDIO_TDMOUTB, EE_AUDIO_MCLK_A,
                               EE_AUDIO_MCLK_A, false);
    zxlogf(INFO, "Enabling clk\n");
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

