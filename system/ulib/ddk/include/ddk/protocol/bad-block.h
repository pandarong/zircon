// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <assert.h>
#include <stdint.h>

#include <zircon/types.h>

typedef struct bad_block_protocol_ops {
    // Fills in |bad_block_list| with list of bad blocks, up until
    // |bad_block_list_len|. Order of blocks is undefined.
    // |bad_block_count| will be filled in with the actual number of bad
    // blocks. It is recommended to first make call with |bad_block_list_len|
    // equal to 0 in order to determine how large the |bad_block_list| is.
    zx_status_t (*get_bad_block_list)(void* ctx, uint32_t* bad_block_list,
                                      uint32_t bad_block_list_len, uint32_t* bad_block_count);

    // Checks whether specified block is bad or not.
    zx_status_t (*is_block_bad)(void* ctx, uint32_t block, bool* is_bad);

    // Sets |block| as bad. If block is already marked bad, it has no effect.
    zx_status_t (*mark_block_bad)(void* ctx, uint32_t block);

} bad_block_protocol_ops_t;

typedef struct bad_block_protocol {
    bad_block_protocol_ops_t* ops;
    void* ctx;
} bad_block_protocol_t;

static inline zx_status_t bad_block_get_bad_block_list(bad_block_protocol_t* bb, uint32_t* bb_list,
                                                       uint32_t bb_list_len, uint32_t* bb_count) {
    return bb->ops->get_bad_block_list(bb->ctx, bb_list, bb_list_len, bb_count);
}

static inline zx_status_t bad_block_is_block_bad(bad_block_protocol_t* bb, uint32_t block,
                                                 bool* is_bad) {
    return bb->ops->is_block_bad(bb->ctx, block, is_bad);
}

static inline zx_status_t bad_block_mark_block_bad(bad_block_protocol_t* bb, uint32_t block) {
    return bb->ops->mark_block_bad(bb->ctx, block);
}
