// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <audio-utils/audio-device-stream.h>
#include <audio-utils/audio-input.h>
#include <audio-utils/audio-output.h>
#include <fcntl.h>
#include <inttypes.h>
#include <zircon/assert.h>
#include <zircon/device/audio.h>
#include <zircon/process.h>
#include <zircon/syscalls.h>
#include <zx/channel.h>
#include <zx/handle.h>
#include <zx/vmar.h>
#include <zx/vmo.h>
#include <fbl/algorithm.h>
#include <fbl/auto_call.h>
#include <fbl/limits.h>
#include <fdio/io.h>
#include <stdio.h>
#include <string.h>

namespace audio {
namespace utils {

static constexpr zx_duration_t CALL_TIMEOUT = ZX_MSEC(500);
template <typename ReqType, typename RespType>
zx_status_t DoCallImpl(const zx::channel& channel,
                       const ReqType&     req,
                       RespType*          resp,
                       zx::handle*        resp_handle_out,
                       uint32_t*          resp_len_out = nullptr) {
    zx_channel_call_args_t args;

    ZX_DEBUG_ASSERT((resp_handle_out == nullptr) || !resp_handle_out->is_valid());

    args.wr_bytes       = const_cast<ReqType*>(&req);
    args.wr_num_bytes   = sizeof(ReqType);
    args.wr_handles     = nullptr;
    args.wr_num_handles = 0;
    args.rd_bytes       = resp;
    args.rd_num_bytes   = sizeof(RespType);
    args.rd_handles     = resp_handle_out ? resp_handle_out->reset_and_get_address() : nullptr;
    args.rd_num_handles = resp_handle_out ? 1 : 0;

    uint32_t bytes, handles;
    zx_status_t read_status, write_status;

    write_status = channel.call(0, zx_deadline_after(CALL_TIMEOUT), &args, &bytes, &handles,
                                &read_status);

    if (write_status != ZX_OK) {
        if (write_status == ZX_ERR_CALL_FAILED) {
            printf("Cmd read failure (cmd %04x, res %d)\n", req.hdr.cmd, read_status);
            return read_status;
        } else {
            printf("Cmd write failure (cmd %04x, res %d)\n", req.hdr.cmd, write_status);
            return write_status;
        }
    }

    // If the caller wants to know the size of the response length, let them
    // check to make sure it is consistent with what they expect.  Otherwise,
    // make sure that the number of bytes we got back matches the size of the
    // response structure.
    if (resp_len_out != nullptr) {
        *resp_len_out = bytes;
    } else
    if (bytes != sizeof(RespType)) {
        printf("Unexpected response size (got %u, expected %zu)\n", bytes, sizeof(RespType));
        return ZX_ERR_INTERNAL;
    }

    return ZX_OK;
}

template <typename ReqType, typename RespType>
zx_status_t DoCall(const zx::channel& channel,
                   const ReqType&     req,
                   RespType*          resp,
                   zx::handle*        resp_handle_out = nullptr) {
    zx_status_t res = DoCallImpl(channel, req, resp, resp_handle_out);
    return (res != ZX_OK) ? res : resp->result;
}

template <typename ReqType, typename RespType>
zx_status_t DoNoFailCall(const zx::channel& channel,
                         const ReqType&     req,
                         RespType*          resp,
                         zx::handle*        resp_handle_out = nullptr) {
    return DoCallImpl(channel, req, resp, resp_handle_out);
}

VideoDeviceStream::VideoDeviceStream(bool input, uint32_t dev_id)
    : input_(input) {
    snprintf(name_, sizeof(name_), "/dev/class/camera/%03u", dev_id);
}

VideoDeviceStream::VideoDeviceStream(bool input, const char* dev_path)
    : input_(input) {
    strncpy(name_, dev_path, sizeof(name_));
    name_[sizeof(name_) - 1] = 0;
}

VideoDeviceStream::~VideoDeviceStream() {
    Close();
}

zx_status_t VideoDeviceStream::Open() {
    if (stream_ch_ != ZX_HANDLE_INVALID)
        return ZX_ERR_BAD_STATE;

    int fd = ::open(name(), O_RDONLY);
    if (fd < 0) {
        printf("Failed to open \"%s\" (res %d)\n", name(), fd);
        return fd;
    }

    ssize_t res = ::fdio_ioctl(fd, AUDIO_IOCTL_GET_CHANNEL,
                               nullptr, 0,
                               &stream_ch_, sizeof(stream_ch_));
    ::close(fd);

    if (res != sizeof(stream_ch_)) {
        printf("Failed to obtain channel (res %zd)\n", res);
        return static_cast<zx_status_t>(res);
    }

    return ZX_OK;
}

zx_status_t VideoDeviceStream::GetSupportedFormats(
        fbl::Vector<camera_video_format_t>* out_formats) const {
    camera_stream_cmd_get_formats_req req;
    camera_stream_cmd_get_formats_resp_t resp;
    uint32_t rxed;
    zx_status_t res;

    if (out_formats == nullptr) {
        return ZX_ERR_INVALID_ARGS;
    }

    req.hdr.cmd = CAMERA_STREAM_CMD_GET_FORMATS;
    res = DoCallImpl(stream_ch_, req, &resp, nullptr, &rxed);
    if ((res != ZX_OK) || (rxed < MIN_RESP_SIZE)) {
        printf("Failed to fetch initial suppored format list chunk (res %d, rxed %u)\n",
                res, rxed);
        return res;
    }

    uint32_t expected_formats = resp.total_format_count;
    if (!expected_formats)
        return ZX_OK;
    // For now, just return the first set of formats:
    // expected_formats  = expected_formats > CAMERA_STREAM_CMD_GET_FORMATS_MAX_FORMATS_PER_RESPONSE ? CAMERA_STREAM_CMD_GET_FORMATS_MAX_FORMATS_PER_R : expected_formats;

    out_formats->reset();
    fbl::AllocChecker ac;
    out_formats->reserve(expected_formats, &ac);
    if (!ac.check()) {
        printf("Failed to allocated %u entries for format ranges\n", expected_formats);
        return ZX_ERR_NO_MEMORY;
    }

    for (uint16_t i = 0; i < expected_formats; ++i) {
        out_formats->push_back(resp.format_ranges[i]);
    }
    zx_txid_t txid = resp.hdr.transaction_id;
    uint32_t processed_formats = 0;
    

    while (true) {
        if (resp.hdr.cmd != CAMERA_STREAM_CMD_GET_FORMATS) {
            printf("Unexpected response command while fetching formats "
                   "(expected 0x%08x, got 0x%08x)\n",
                    CAMERA_STREAM_CMD_GET_FORMATS, resp.hdr.cmd);
            return ZX_ERR_INTERNAL;
        }

        if (resp.already_sent_count != processed_formats) {
            printf("Bad format index while fetching formats (expected %u, got %hu)\n",
                    processed_formats, resp.already_sent_count);
            return ZX_ERR_INTERNAL;
        }

        uint32_t todo = fbl::min(static_cast<uint32_t>(expected_formats - processed_formats),
                CAMERA_STREAM_CMD_GET_FORMATS_MAX_FORMATS_PER_RESPONSE);

        for (uint16_t i = 0; i < todo; ++i) {
            out_formats->push_back(resp.formats[i]);
        }

        processed_formats += todo;
        if (processed_formats == expected_formats)
            break;

        zx_signals_t pending_sig;
        res = stream_ch_.wait_one(ZX_CHANNEL_READABLE | ZX_CHANNEL_PEER_CLOSED,
                                  zx_deadline_after(CALL_TIMEOUT),
                                  &pending_sig);
        if (res != ZX_OK) {
            printf("Failed to wait for next response after processing %u/%u formats (res %d)\n",
                    processed_formats, expected_formats, res);
            return res;
        }

        res = stream_ch_.read(0u, &resp, sizeof(resp), &rxed, nullptr, 0, nullptr);
        if (res != ZX_OK) {
            printf("Failed to read next response after processing %u/%u formats (res %d)\n",
                    processed_formats, expected_formats, res);
            return res;
        }
    }

    return ZX_OK;
}


zx_status_t VideoDeviceStream::SetFormat(camera_video_format_t format, buffer_layout_t layout, 
                                         fbl::Vector<zx::event> &release_fences) {
    if ((stream_ch_ == ZX_HANDLE_INVALID) || (rb_ch_ != ZX_HANDLE_INVALID))
        return ZX_ERR_BAD_STATE;


    channel_cnt_   = channels;
    frame_sz_      = channels * sample_size_;
    frame_rate_    = frames_per_second;
    sample_format_ = sample_format;

    camera_stream_cmd_configure_stream_req_t req;
    camera_stream_cmd_configure_stream_resp_t resp;
    req.hdr.cmd                    = CAMERA_STREAM_CMD_CONFIGURE_STREAM;
    req.total_frame_entries_count  = release_fences.size();
    req.already_sent_count         = 0;
    req.video_format               = format;
    for (uint16_t i = 0; i < release_fences.size(); ++i) {
        req.frame_entries[i].offset = layout.buffer_offsets[i];
        req.frame_entries[i].size   = layout.buffer_sizes[i];
    }
    //Set the frame entries, up to CAMERA_STREAM_CMD_CONFIGURE_STREAM_MAX_FRAME_ENTRIES
    uint16_t handles_out = release_fences.size() + 2;

     zx_status_t DoCallImpl(const zx::channel& channel,
                       const ReqType&     req,
                       RespType*          resp,
                       zx::handle*        resp_handle_out,
                       uint32_t*          resp_len_out = nullptr) {
    zx_channel_call_args_t args;

    ZX_DEBUG_ASSERT((resp_handle_out == nullptr) || !resp_handle_out->is_valid());

    args.wr_bytes       = &req;
    args.wr_num_bytes   = sizeof(camera_stream_cmd_configure_stream_req_t);
    args.wr_handles     = nullptr; // TODO(garratt)
    args.wr_num_handles = handles_out;
    args.rd_bytes       = &resp;
    args.rd_num_bytes   = sizeof(camera_stream_cmd_configure_stream_resp_t);
    args.rd_handles     = nullptr;
    args.rd_num_handles = 0;

    uint32_t bytes, handles;
    zx_status_t read_status, write_status;

    write_status = stream_ch_.call(0, zx_deadline_after(CALL_TIMEOUT), &args, &bytes, &handles,
                                &read_status);

    if (write_status != ZX_OK) {
        if (write_status == ZX_ERR_CALL_FAILED) {
            printf("Cmd read failure (cmd %04x, res %d)\n", req.hdr.cmd, read_status);
            return read_status;
        } else {
            printf("Cmd write failure (cmd %04x, res %d)\n", req.hdr.cmd, write_status);
            return write_status;
        }
    }
    
    
    
    zx::handle tmp;
    zx_status_t res = DoCall(stream_ch_, req, &resp, &tmp);
    if (res != ZX_OK) {
        printf("Failed to set format %uHz %hu-Ch fmt 0x%x (res %d)\n",
                frames_per_second, channels, sample_format, res);
    }

    external_delay_nsec_ = resp.external_delay_nsec;

    // TODO(johngro) : Verify the type of this handle before transferring it to
    // our ring buffer channel handle.
    rb_ch_.reset(tmp.release());

    return res;
}


zx_status_t VideoDeviceStream::StartRingBuffer() {
    if (rb_ch_ == ZX_HANDLE_INVALID)
        return ZX_ERR_BAD_STATE;

    audio_rb_cmd_start_req_t  req;
    audio_rb_cmd_start_resp_t resp;

    req.hdr.cmd = AUDIO_RB_CMD_START;
    req.hdr.transaction_id = 1;

    zx_status_t res = DoCall(rb_ch_, req, &resp);

    if (res == ZX_OK) {
        start_time_ = resp.start_time;
    }

    return res;
}

zx_status_t VideoDeviceStream::StopRingBuffer() {
    if (rb_ch_ == ZX_HANDLE_INVALID)
        return ZX_ERR_BAD_STATE;

    start_time_ = 0;

    audio_rb_cmd_stop_req_t  req;
    audio_rb_cmd_stop_resp_t resp;

    req.hdr.cmd = AUDIO_RB_CMD_STOP;
    req.hdr.transaction_id = 1;

    return DoCall(rb_ch_, req, &resp);
}

void VideoDeviceStream::ResetRingBuffer() {
    if (rb_virt_ != nullptr) {
        ZX_DEBUG_ASSERT(rb_sz_ != 0);
        zx::vmar::root_self().unmap(reinterpret_cast<uintptr_t>(rb_virt_), rb_sz_);
    }
    rb_ch_.reset();
    rb_vmo_.reset();
    rb_sz_ = 0;
    rb_virt_ = nullptr;
}

void VideoDeviceStream::Close() {
    ResetRingBuffer();
    stream_ch_.reset();
}

bool VideoDeviceStream::IsChannelConnected(const zx::channel& ch) {
    if (!ch.is_valid())
        return false;

    zx_signals_t junk;
    return ch.wait_one(ZX_CHANNEL_PEER_CLOSED, 0u, &junk) != ZX_ERR_TIMED_OUT;
}


}  // namespace utils
}  // namespace audio
