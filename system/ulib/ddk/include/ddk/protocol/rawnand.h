// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <assert.h>
#include <stdint.h>

// nand_info_t is used to retrieve various parameters describing the
// geometry of the underlying NAND chip(s). This is retrieved using
// the query api in nand_protocol_ops.

typedef struct nand_info nand_info_t;

struct nand_info {
    uint32_t    erase_blocksize;            // in bytes
    uint32_t    pagesize;                   // in bytes
    uint64_t    device_size;                // in bytes
    // # of ECC bits per ECC page - in other words, the # of bitflips
    // that can be corrected per ECC page.
    uint32_t    ecc_bits;
};

// nand_op_t's are submitted for processing via the queue() method
// of the nand_protocol.  Once submitted, the contents of the nand_op_t
// may be modified while it's being processed
//
// The completion_cb() must eventually be called upon success or failure and
// at that point the cookie field must contain whatever value was in it when
// the nand_op_t was originally queued.

#define NAND_OP_READ_DATA               0x00000001
#define NAND_OP_WRITE_DATA              0x00000002
#define NAND_OP_ERASE                   0x00000003
// TBD NAND_OP_READ_OOB, NAND_OP_WRITE_OOB

typedef struct nand_op nand_op_t;

struct nand_op {
    // All Commands
    uint32_t command;
    union {
        // NAND_OP_READ_DATA, NAND_OP_WRITE_DATA
        struct {
            uint32_t command;
            // command
            zx_handle_t vmo;
            // vmo of data to read or write
            uint32_t length;
            // transfer length in bytes
            // (0 is invalid).
            // MUST BE A MULTIPLE OF NAND PAGESIZE
            uint64_t offset_nand;
            // offset into nand in bytes.
            // MUST BE NAND PAGESIZE aligned
            uint64_t offset_vmo;
            // vmo offset in bytes
            uint64_t* pages;
            // optional physical page list
            // Return value from READ_DATA, max corrected bit flips in
            // any underlying ECC page read. The above layer(s) can
            // compare this returned value against the bits-per-ECC-page
            // to decide whether the underlying NAND erase block needs
            // to be moved
            uint32_t max_bitflips_ecc_page;
        } rw;

        // NAND_OP_ERASE
        struct {
            uint64_t offset_nand;
            // offset into nand in bytes
            // MUST BE NAND ERASEBLOCKSIZE aligned
            uint32_t length;
            // erase length in bytes(0 is invalid).
            // MUST BE NAND ERASEBLOCKSIZE multiple
        } erase;
    } u;

    // The completion_cb() will be called when the nand operation
    // succeeds or fails, and cookie will be whatever was set when
    // the nand_op was initially queue()'d.
    void (*completion_cb)(nand_op_t *nop, zx_status_t status);
    void *cookie;
};

typedef struct nand_protocol_ops {
    // Obtain the parameters of the nand device (nand_info_t) and
    // the required size of nand_txn_t.  The nand_txn_t's submitted
    // via queue() must have nand_op_size_out - sizeof(nand_op_t) bytes
    // available at the end of the structure for the use of the driver.
    void (*query)(void *ctx, nand_info_t *info_out, size_t *nand_op_size_out);

    // Submit an IO request for processing.  Success or failure will
    // be reported via the completion_cb() in the nand_op_t. This
    // callback may be called before the queue() method returns.
    void (*queue)(void *ctx, nand_op_t *op);

    // Get list of bad NAND blocks (queried on startup)
    // Returns the number of bad blocks found (could be 0).
    // Internally the function allocates a table to hold the addresses of
    // each bad block found. The returned table needs to be freed by the
    // caller. The size of the table returned is
    // (num_bad_blocks * sizeof(uint64_t))
    void (*get_bad_block_list)(void *ctx, size_t *num_bad_blocks,
                               uint64_t **blocklist);
} nand_protocol_ops_t;


typedef struct nand_protocol {
    nand_protocol_ops_t* ops;
    void* ctx;
} nand_protocol_t;
