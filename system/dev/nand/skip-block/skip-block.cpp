// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skip-block.h"

#include <string.h>

#include <ddk/debug.h>
#include <ddk/protocol/bad-block.h>
#include <ddk/protocol/nand.h>

#include <fbl/alloc_checker.h>
#include <fbl/array.h>
#include <fbl/auto_lock.h>
#include <fbl/unique_ptr.h>
#include <sync/completion.h>
#include <zircon/boot/image.h>

namespace nand {

namespace {

#define OLD_NAND_PROTO 1

struct BlockOperationContext {
    const skip_block_rw_operation_t& op;
    nand_info_t* nand_info;
    ddk::NandProtocolProxy* nand;
#if OLD_NAND_PROTO
    uint32_t current_page;
#endif
    completion_t* completion_event;
    zx_status_t status;
};

void ReadCompletionCallback(nand_op_t* op, zx_status_t status) {
    auto* ctx = static_cast<BlockOperationContext*>(op->cookie);

#if OLD_NAND_PROTO
    if (status != ZX_OK || ctx->current_page + 1 == ctx->nand_info->pages_per_block) {
        ctx->status = status;
        completion_signal(ctx->completion_event);
        return;
    }
    op->rw_data_oob.page_num += 1;
    op->rw_data_oob.data.offset_vmo += ctx->nand_info->page_size;
    ctx->nand->Queue(op);
#else
    ctx->status = status;
    completion_signal(ctx->completion_event);
#endif
    return;
}

void WriteCompletionCallback(nand_op_t* op, zx_status_t status) {
    auto* ctx = static_cast<BlockOperationContext*>(op->cookie);

#if OLD_NAND_PROTO
    if (status != ZX_OK || ctx->current_page + 1 == ctx->nand_info->pages_per_block) {
        ctx->status = status;
        completion_signal(ctx->completion_event);
        return;
    }
    op->rw_data_oob.command = NAND_OP_WRITE_PAGE_DATA_OOB;
    op->rw_data_oob.page_num += 1;
    op->rw_data_oob.data.offset_vmo += ctx->nand_info->page_size;
    ctx->nand->Queue(op);
#else
    ctx->status = status;
    completion_signal(ctx->completion_event);
#endif
    return;
}

void EraseCompletionCallback(nand_op_t* op, zx_status_t status) {
    auto* ctx = static_cast<BlockOperationContext*>(op->cookie);

    if (status != ZX_OK) {
        ctx->status = status;
        completion_signal(ctx->completion_event);
        return;
    }

    const uint32_t physical_block = op->erase.first_block;
#if OLD_NAND_PROTO
    ctx->current_page = 0;
    op->rw_data_oob.command = NAND_OP_WRITE_PAGE_DATA_OOB;
    op->rw_data_oob.page_num = physical_block * ctx->nand_info->pages_per_block;
    op->rw_data_oob.data.vmo = ctx->op.vmo;
    op->rw_data_oob.data.length = 1;
    op->rw_data_oob.data.offset_vmo = ctx->op.vmo_offset;
    op->rw_data_oob.oob.length = 0;
#else
    op->rw.command = NAND_OP_WRITE_PAGE_DATA;
    op->rw.data_vmo = ctx->op.vmo;
    op->rw.oob_vmo = ZX_HANDLE_INVALID;
    op->rw.length = nand_info_.pages_per_block;
    op->rw.offset_nand = physical_block * ctx->nand_info->pages_per_block;
    op->rw.offset_data_vmo = ctx->op.vmo;
#endif
    op->completion_cb = WriteCompletionCallback;
    ctx->nand->Queue(op);
    return;
}

} // namespace

zx_status_t SkipBlockDevice::Create(zx_device_t* parent) {
    // Get NAND protocol.
    nand_protocol_t nand_proto;
    if (device_get_protocol(parent, ZX_PROTOCOL_NAND, &nand_proto) != ZX_OK) {
        zxlogf(ERROR, "skip-block: parent device '%s': does not support nand protocol\n",
               device_get_name(parent));
        return ZX_ERR_NOT_SUPPORTED;
    }

    // Get bad block protocol.
    bad_block_protocol_t bad_block_proto;
    if (device_get_protocol(parent, ZX_PROTOCOL_BAD_BLOCK, &bad_block_proto) != ZX_OK) {
        zxlogf(ERROR, "skip-block: parent device '%s': does not support bad_block protocol\n",
               device_get_name(parent));
        return ZX_ERR_NOT_SUPPORTED;
    }

    fbl::AllocChecker ac;
    fbl::unique_ptr<SkipBlockDevice> device(new (&ac) SkipBlockDevice(parent, nand_proto,
                                                                           bad_block_proto));
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    zx_status_t status = device->Bind();
    if (status != ZX_OK) {
        return status;
    }

    // devmgr is now in charge of the device.
    __UNUSED auto* dummy = device.release();
    return ZX_OK;
}

zx_status_t SkipBlockDevice::GetBadBlockList(fbl::Array<uint32_t>* bad_blocks) {
    uint32_t bad_block_count;
    auto status = bad_block_.GetBadBlockList(nullptr, 0, &bad_block_count);
    if (status) {
      return status;
    }
    if (bad_block_count == 0) {
        // Early return.
        bad_blocks->reset();
        return ZX_OK;
    }
    const uint32_t bad_block_list_len = bad_block_count;
    fbl::unique_ptr<uint32_t[]> bad_block_list(new uint32_t[bad_block_count]);
    status = bad_block_.GetBadBlockList(bad_block_list.get(), bad_block_list_len, &bad_block_count);
    if (status) {
      return status;
    }
    if (bad_block_list_len != bad_block_count) {
        return ZX_ERR_INTERNAL;
    }
    *bad_blocks = fbl::move(fbl::Array<uint32_t>(bad_block_list.release(), bad_block_count));
    return ZX_OK;
}

zx_status_t SkipBlockDevice::Bind() {
    zxlogf(INFO, "skip-block: Binding to %s\n", device_get_name(parent()));

    if (sizeof(nand_op_t) > parent_op_size_) {
        zxlogf(ERROR, "skip-block: parent op size, %zu, is smaller than minimum op size: %zu\n",
               sizeof(nand_op_t), parent_op_size_);
        return ZX_ERR_INTERNAL;
    }

    fbl::AllocChecker ac;
    fbl::Array<uint8_t> nand_op(new (&ac) uint8_t[parent_op_size_], parent_op_size_);
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }
    nand_op_ = fbl::move(nand_op);

    // TODO(surajmalhotra): Potentially make this lazy instead of in the bind.
    fbl::Array<uint32_t> bad_blocks;
    const auto status = GetBadBlockList(&bad_blocks);
    if (status) {
        zxlogf(ERROR, "skip-block: Failed to get bad block list\n");
        return status;
    }

    return DdkAdd("skip-block");
}

zx_status_t SkipBlockDevice::GetPartitionInfo(skip_block_partition_info_t* info) const {
    info->block_size_bytes = GetBlockSize();
    info->partition_block_count = ltop_.LogicalBlockCount();
    memcpy(info->partition_guid, nand_info_.partition_guid, ZBI_PARTITION_GUID_LEN);

    return ZX_OK;
}

zx_status_t SkipBlockDevice::ValidateVmo(const skip_block_rw_operation_t& op) const {
    uint64_t vmo_size;
    auto status = zx_vmo_get_size(op.vmo, &vmo_size);
    if (status != ZX_OK) {
        return ZX_ERR_INVALID_ARGS;
    }
    if (vmo_size < op.vmo_offset + op.block_count * GetBlockSize()) {
        return ZX_ERR_OUT_OF_RANGE;
    }
    return ZX_OK;
}

zx_status_t SkipBlockDevice::Read(skip_block_rw_operation_t* op) {
    auto status = ValidateVmo(*op);
    if (status != ZX_OK) {
        return status;
    }

    completion_t completion;
    BlockOperationContext op_context = {
        .op = *op,
        .nand_info = &nand_info_,
        .nand = &nand_,
#if OLD_NAND_PROTO
        .current_page = 0,
#endif
        .completion_event = &completion,
        .status = ZX_OK,
    };
    for (uint32_t logical_block = op->block; logical_block < op->block_count; logical_block++) {
        uint32_t physical_block;
        status = ltop_.GetPhysical(logical_block, &physical_block);
        if (status != ZX_OK) {
            return status;
        }

        auto* nand_op = reinterpret_cast<nand_op_t*>(nand_op_.get());
#if OLD_NAND_PROTO
        const uint32_t page_num = physical_block * nand_info_.pages_per_block;
        nand_op->rw_data_oob.command = NAND_OP_READ_PAGE_DATA_OOB;
        nand_op->rw_data_oob.page_num = page_num;
        nand_op->rw_data_oob.data.vmo = op->vmo;
        nand_op->rw_data_oob.data.length = 1;
        nand_op->rw_data_oob.data.offset_vmo = op->vmo_offset;
        nand_op->rw_data_oob.oob.length = 0;
#else
        nand_op->rw.command = NAND_OP_READ_PAGE_DATA;
        nand_op->rw.data_vmo = op->vmo;
        nand_op->rw.oob_vmo = ZX_HANDLE_INVALID;
        nand_op->rw.length = nand_info_.pages_per_block;
        nand_op->rw.offset_nand = physical_block * nand_info_.pages_per_block;
        nand_op->rw.offset_data_vmo = op->vmo_offset;
#endif
        nand_op->completion_cb = ReadCompletionCallback;
        nand_op->cookie = &op_context;
        nand_.Queue(nand_op);

        // Wait on completion.
        completion_wait(&completion, ZX_TIME_INFINITE);
        if (op_context.status != ZX_OK) {
            return op_context.status;
        }
        completion_reset(&completion);
    }

    return ZX_OK;
}

zx_status_t SkipBlockDevice::Write(const skip_block_rw_operation_t& op) {
    auto status = ValidateVmo(op);
    if (status != ZX_OK) {
        return status;
    }

    completion_t completion;
    BlockOperationContext op_context = {
        .op = op,
        .nand_info = &nand_info_,
        .nand = &nand_,
#if OLD_NAND_PROTO
        .current_page = 0,
#endif
        .completion_event = &completion,
        .status = ZX_OK,
    };
    for (uint32_t logical_block = op.block; logical_block < op.block_count; logical_block++) {
        uint32_t physical_block;
        status = ltop_.GetPhysical(logical_block, &physical_block);
        if (status != ZX_OK) {
            return status;
        }

        auto* nand_op = reinterpret_cast<nand_op_t*>(nand_op_.get());
        nand_op->erase.command = NAND_OP_ERASE;
        nand_op->erase.first_block = physical_block;
        nand_op->erase.num_blocks = 1;
        // The erase callback will enqueue the writes.
        nand_op->completion_cb = EraseCompletionCallback;
        nand_op->cookie = &op_context;
        nand_.Queue(nand_op);

        // Wait on completion.
        completion_wait(&completion, ZX_TIME_INFINITE);
        if (op_context.status != ZX_OK) {
            zxlogf(ERROR, "Failed to erase/write block %u, marking bad\n", physical_block);
            status = bad_block_.MarkBlockBad(physical_block);
            if (status != ZX_OK) {
                zxlogf(ERROR, "skip-block: Failed to mark block bad\n");
                return status;
            }
            // LtoP has changed, so we need to re-initialize it.
            ltop_ = fbl::move(LogicalToPhysicalMap());
            fbl::Array<uint32_t> bad_blocks;
            const auto status = GetBadBlockList(&bad_blocks);
            if (status) {
                zxlogf(ERROR, "skip-block: Failed to get bad block list\n");
                return status;
            }
            ltop_ = fbl::move(LogicalToPhysicalMap(nand_info_.num_blocks, fbl::move(bad_blocks)));
            if (status != ZX_OK) {
                return status;
            }
            return ZX_ERR_INTERNAL_INTR_RETRY;
        }
        completion_reset(&completion);
    }

    return ZX_OK;
}

zx_status_t SkipBlockDevice::DdkIoctl(uint32_t op, const void* in_buf, size_t in_len,
                                      void* out_buf, size_t out_len, size_t* out_actual) {
    fbl::AutoLock lock(&lock_);

    switch (op) {
    case IOCTL_SKIP_BLOCK_GET_PARTITION_INFO:
        if (out_buf == NULL || out_len < sizeof(skip_block_partition_info_t)) {
            return ZX_ERR_INVALID_ARGS;
        }
        *out_actual = sizeof(skip_block_partition_info_t);
        return GetPartitionInfo(static_cast<skip_block_partition_info_t*>(out_buf));

    case IOCTL_SKIP_BLOCK_READ:
        if (out_buf == NULL || out_len < sizeof(skip_block_rw_operation_t)) {
            return ZX_ERR_INVALID_ARGS;
        }
        *out_actual = sizeof(skip_block_rw_operation_t);
        return Read(static_cast<skip_block_rw_operation_t*>(out_buf));

    case IOCTL_SKIP_BLOCK_WRITE:
        if (in_buf == NULL || in_len < sizeof(skip_block_rw_operation_t)) {
            return ZX_ERR_INVALID_ARGS;
        }
        return Write(*static_cast<const skip_block_rw_operation_t*>(in_buf));

    default:
        return ZX_ERR_NOT_SUPPORTED;
    }
}

} // namespace nand

extern "C" zx_status_t skip_block_bind(void* ctx, zx_device_t* parent) {
    return nand::SkipBlockDevice::Create(parent);
}
