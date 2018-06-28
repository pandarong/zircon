// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddk/protocol/bad-block.h>
#include <ddktl/device-internal.h>

#include "bad-block-internal.h"

// DDK nand protocol support.
//
// :: Proxies ::
//
// ddk::BadBlockProtocolProxy is a simple wrappers around bad_block_protocol_t. It does
// not own the pointers passed to it.
//
// :: Examples ::
//
// // A driver that implements a ZX_PROTOCOL_BAD_BLOCK device.
// class BadBlockDevice;
// using BadBlockDeviceType = ddk::Device<NandDevice, /* ddk mixins */>;
//
// class NandDevice : public NandDeviceType,
//                    public ddk::NandProtocol<NandDevice> {
//   public:
//     NandDevice(zx_device_t* parent)
//       : NandDeviceType("my-nand-device", parent) {}
//
//     void Query(nand_info_t* info_out, size_t* nand_op_size_out);
//     void Queue(nand_op_t* operation);
//     void GetBadBlockList(uint32_t* bad_blocks, uint32_t bad_block_len,
//                          uint32_t* num_bad_blocks);
//     ...
// };

namespace ddk {

template <typename D>
class BadBlockable : public internal::base_mixin {
public:
    BadBlockable() {
        internal::CheckBadBlockable<D>();
        bad_block_proto_ops_.get_bad_block_list = GetBadBlockList;
        bad_block_proto_ops_.is_block_bad = IsBlockBad;
        bad_block_proto_ops_.mark_block_bad = MakeBlockBad;
    }

protected:
    bad_block_protocol_ops_t bad_block_proto_ops_ = {};

private:
    static zx_status_t GetBadBlockList(void* ctx, uint32_t* bad_block_list,
                                       uint32_t bad_block_list_len, uint32_t* bad_block_count) {
        return static_cast<D*>(ctx)->GetBadBlockList2(bad_block_list, bad_block_list_len,
                                                     bad_block_count);
    }

    static zx_status_t IsBlockBad(void* ctx, uint32_t block, bool* is_bad) {
        return static_cast<D*>(ctx)->IsBlockBad(block, is_bad);
    }

    static zx_status_t MakeBlockBad(void* ctx, uint32_t block) {
        return static_cast<D*>(ctx)->MarkBlockBad(block);
    }
};

class BadBlockProtocolProxy {
public:
    BadBlockProtocolProxy(bad_block_protocol_t* proto)
        : ops_(proto->ops), ctx_(proto->ctx) {}

    zx_status_t GetBadBlockList(uint32_t* bad_block_list, uint32_t bad_block_list_len,
                                uint32_t* bad_block_count) {
        return ops_->get_bad_block_list(ctx_, bad_block_list, bad_block_list_len, bad_block_count);
    }

    zx_status_t IsBadBlock(uint32_t block, bool* is_bad) {
        return ops_->is_block_bad(ctx_, block, is_bad);
    }

    zx_status_t MarkBlockBad(uint32_t block) {
        return ops_->mark_block_bad(ctx_, block);
    }

private:
    bad_block_protocol_ops_t* ops_;
    void* ctx_;
};

} // namespace ddk
