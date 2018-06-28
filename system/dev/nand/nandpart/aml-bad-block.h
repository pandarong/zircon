// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <stddef.h>

#include <fbl/array.h>
#include <fbl/ref_ptr.h>
#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>
#include <zircon/device/bad-block.h>
#include <zircon/types.h>

#include "bad-block.h"

namespace nand {

// Bad block implementation for NAND using Amlogic u-boot style bad block tables.
class AmlBadBlock : public BadBlock {
public:
    static zx_status_t Create(Config config, fbl::RefPtr<BadBlock>* out);

    zx_status_t GetBadBlockList(fbl::Array<uint32_t>* bad_blocks, uint32_t first_block,
                                uint32_t last_block) override;
    zx_status_t IsBlockBad(uint32_t block, bool* is_bad) override;
    zx_status_t MarkBlockBad(uint32_t block) override;

private:
    friend class fbl::RefPtr<AmlBadBlock>;
    friend class fbl::internal::MakeRefCountedHelper<AmlBadBlock>;

    typedef uint8_t BlockStatus;
    constexpr static BlockStatus kNandBlockGood = 0;
    constexpr static BlockStatus kNandBlockBad = 1;
    constexpr static BlockStatus kNandBlockFactoryBad = 2;

    constexpr static size_t kBlockListMax = 8;

    struct BlockListEntry {
        uint32_t block;
        int16_t program_erase_cycles;
        bool valid;
    };

    struct OobMetadata {
        // Identifer value.
        uint32_t magic;
        // Number of times the block has been programmed and erased.
        int16_t program_erase_cycles;
        // Iteration of the bad block table. Each time a new one is programmed, this
        // should be incremented. Used to identify the newest copy.
        uint16_t generation;
    };

    AmlBadBlock(zx::vmo data_vmo, zx::vmo oob_vmo, fbl::Array<uint8_t> nand_op,
                Config config, nand_info_t nand_info, BlockStatus* table, uint32_t table_len,
                OobMetadata* oob)
        : BadBlock(fbl::move(data_vmo), fbl::move(oob_vmo), fbl::move(nand_op)),
          config_(config.bad_block_config), nand_proto_(config.nand_proto), nand_(&nand_proto_),
          nand_info_(nand_info), block_entry_(nullptr), page_(0),
          generation_(0), found_(false), oob_(oob), table_(table), table_len_(table_len) {}

    ~AmlBadBlock() override {
        zx::vmar::root_self().unmap(reinterpret_cast<uintptr_t>(oob_), sizeof(OobMetadata));
        zx::vmar::root_self().unmap(reinterpret_cast<uintptr_t>(table_), table_len_);
    }

    // Synchronously erase a block.
    zx_status_t EraseBlock(uint32_t block);

    // Synchronously write a page into NAND.
    zx_status_t WritePage(uint32_t nand_page, uint64_t data_offset);

    // Synchronously read a page from NAND.
    zx_status_t ReadPage(uint32_t nand_page, uint64_t data_offset);

    // Looks for a valid block to write BBT to.
    zx_status_t GetNewBlock(void);

    // Writes in memory copy of BBT to the device.
    zx_status_t WriteBadBlockTable(bool use_new_block);

    // Finds BBT and reads it into memory from NAND.
    zx_status_t FindBadBlockTable(void);

    // Top level config.
    bad_block_config_t config_;
    // Parent nand protocol implementation.
    nand_protocol_t nand_proto_;
    ddk::NandProtocolProxy nand_;
    nand_info_t nand_info_;
    // Information about blocks which store BBT entries.
    BlockListEntry block_list_[kBlockListMax];
    // Block with most recent valid BBT entry.
    BlockListEntry* block_entry_;
    // The first page for the last valid BBT entry in above block.
    uint32_t page_;
    // Generation ID of newest BBT entry.
    uint16_t generation_;
    // Whether the table has been found or not.
    bool found_;
    // OOB metadata appended to end of table. Backed by oob_vmo.
    OobMetadata* oob_;
    // Copy of latest BBT. Each byte 1:1 maps to a block. Backed by data_vmo.
    BlockStatus* table_;
    // Number of entries in table. Should be equal to number of blocks.
    uint32_t table_len_;
};
} // namespace nand
