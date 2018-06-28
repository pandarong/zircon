// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddktl/protocol/bad-block.h>

#include <fbl/array.h>
#include <fbl/macros.h>
#include <zircon/types.h>

namespace nand {

// Logical block to physical block mapping. Provides bad block skip
// functionality.
//
// NOT THREADSAFE.
class LogicalToPhysicalMap {
public:
    LogicalToPhysicalMap()
        : block_count_(0) {}

    LogicalToPhysicalMap(uint32_t block_count, fbl::Array<uint32_t> bad_blocks)
        : block_count_(block_count), bad_blocks_(fbl::move(bad_blocks)) {}

    // Move constructor.
    LogicalToPhysicalMap(LogicalToPhysicalMap&& other)
        : block_count_(other.block_count_), bad_blocks_(fbl::move(other.bad_blocks_)) {}

    // Move assignment operator.
    LogicalToPhysicalMap& operator=(LogicalToPhysicalMap&& other) {
        if (this != &other) {
            block_count_ = other.block_count_;
            bad_blocks_ = fbl::move(other.bad_blocks_);
        }
        return *this;
    }

    zx_status_t GetPhysical(uint32_t block, uint32_t* physical_block) const {
        *physical_block = 0;
        uint32_t logical_block = 0;
        uint32_t prev_bad_block = 0;
        for (const auto bad_block : bad_blocks_) {
            const uint32_t good_blocks = bad_block - prev_bad_block - 1;
            if (logical_block + good_blocks < block) {
                *physical_block += block - logical_block;
                return ZX_OK;
            }
            *physical_block += good_blocks + 1;
            logical_block += good_blocks;
            prev_bad_block = bad_block;
        }
        const uint32_t good_blocks = block_count_ - prev_bad_block;
        if (logical_block + good_blocks < block) {
            *physical_block += block - logical_block;
            return ZX_OK;
        }

        return ZX_ERR_OUT_OF_RANGE;
    }

    uint32_t LogicalBlockCount() const {
        return block_count_ - static_cast<uint32_t>(bad_blocks_.size());
    }

private:
    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LogicalToPhysicalMap);

    uint32_t block_count_;
    fbl::Array<uint32_t> bad_blocks_;
};

} // namespace nand
