// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zircon/device/camera.h>
// #include <camera-driver-proto/camera-proto.h>
#include <zircon/types.h>
#include <zx/channel.h>
#include <zx/vmo.h>
#include <fbl/unique_ptr.h>
#include <fbl/vector.h>
#include <async/auto_wait.h>

namespace video {
namespace utils {

class VideoDeviceStream {
public:
    zx_status_t Open();
    zx_status_t GetSupportedFormats(fbl::Vector<camera_video_format_t>* out_formats) const;
    zx_status_t SetFormat(camera_video_format_t format);
    zx_status_t SetBuffer(const zx::vmo &vmo);
    zx_status_t StartRingBuffer();
    zx_status_t StopRingBuffer();
    void OnNewFrame(camera_vb_frame_notify_t frame_info);
    void ReleaseFrame(uint64_t data_offset);
    void        ResetRingBuffer();
    void        Close();

protected:
    // friend class fbl::unique_ptr<AudioDeviceStream>;
    async_wait_result_t OnNewMessageSignalled(async_t* async, zx_status_t status,
                            const zx_packet_signal* signal); 

    // static bool IsChannelConnected(const zx::channel& ch);

    // The maximum size a frame will occupy in the video stream.
    // A value of zero means that the video buffer channel is uninitialized.
    uint32_t max_frame_size_ = 0;

    VideoDeviceStream(async_t *async, bool input, uint32_t dev_id);
    VideoDeviceStream(async_t *async, bool input, const char* dev_path);
    virtual ~VideoDeviceStream();

    zx::channel stream_ch_;
    zx::channel vb_ch_;

    async::AutoWait new_frame_waiter_;
    const bool  input_;
    char        name_[64] = { 0 };

};

}  // namespace utils
}  // namespace video
