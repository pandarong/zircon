// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zircon/device/camera.h>
#include <zircon/types.h>
#include <zx/channel.h>
#include <zx/vmo.h>
#include <fbl/unique_ptr.h>
#include <fbl/vector.h>

namespace video {
namespace utils {

class VideoDeviceStream {
public:
    zx_status_t Open();
    zx_status_t GetSupportedFormats(fbl::Vector<camera_video_format_t>* out_formats) const;
    zx_status_t SetFormat(camera_video_format_t format, buffer_layout_t layout, fbl::Vector<zx::event> &release_fences);
    zx_status_t StartRingBuffer();
    zx_status_t StopRingBuffer();
    void        ResetRingBuffer();
    void        Close();

    bool IsStreamBufChannelConnected() const { return IsChannelConnected(stream_ch_); }
    bool IsRingBufChannelConnected() const { return IsChannelConnected(rb_ch_); }

    const char* name()                 const { return name_; }
    bool        input()                const { return input_; }
    uint32_t    frame_rate()           const { return frame_rate_; }
    uint32_t    sample_size()          const { return sample_size_; }
    uint32_t    channel_cnt()          const { return channel_cnt_; }
    uint32_t    frame_sz()             const { return frame_sz_; }
    uint64_t    fifo_depth()           const { return fifo_depth_; }
    uint32_t    ring_buffer_bytes()    const { return rb_sz_; }
    void*       ring_buffer()          const { return rb_virt_; }
    uint64_t    start_time()           const { return start_time_; }
    uint64_t    external_delay_nsec()  const { return external_delay_nsec_; }

protected:
    friend class fbl::unique_ptr<AudioDeviceStream>;

    static bool IsChannelConnected(const zx::channel& ch);

    AudioDeviceStream(bool input, uint32_t dev_id);
    AudioDeviceStream(bool input, const char* dev_path);
    virtual ~AudioDeviceStream();

    zx::channel stream_ch_;
    zx::channel rb_ch_;
    zx::vmo     rb_vmo_;

    const bool  input_;
    char        name_[64] = { 0 };

    audio_sample_format_t sample_format_;
    uint64_t start_time_           = 0;
    uint64_t external_delay_nsec_  = 0;
    uint32_t frame_rate_           = 0;
    uint32_t sample_size_          = 0;
    uint32_t channel_cnt_          = 0;
    uint32_t frame_sz_             = 0;
    uint32_t fifo_depth_           = 0;
    uint32_t rb_sz_                = 0;
    void*    rb_virt_              = nullptr;
};

}  // namespace utils
}  // namespace video
