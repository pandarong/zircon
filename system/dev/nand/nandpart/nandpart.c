// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/binding.h>
#include <ddk/metadata.h>
#include <ddk/protocol/nand.h>

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zircon/boot/image.h>
#include <zircon/hw/gpt.h>
#include <zircon/types.h>

#define GUID_STRLEN 40

#define TXN_SIZE 0x4000 // 128 partition entries

static const uint8_t fvm_guid[] = GUID_FVM_VALUE;

typedef struct {
    zx_device_t* zxdev;
    zx_device_t* parent;

    nand_protocol_t nand_proto;
    uint32_t erase_block_start; // First erase block for the partition.
    size_t parent_op_size;      // op_size for parent device.

    nand_info_t info;
} nandpart_device_t;

static void nandpart_query(void* ctx, nand_info_t* info_out, size_t* nand_op_size_out) {
    nandpart_device_t* dev = (nandpart_device_t *)ctx;

    memcpy(info_out, &dev->info, sizeof(*info_out));
    // add size of translated_op
    *nand_op_size_out = dev->parent_op_size + sizeof(nand_op_t);
}

static void nandpart_completion_cb(nand_op_t* op, zx_status_t status) {
    op = (nand_op_t *)op->cookie;
    op->completion_cb(op, status);
}

static void nandpart_queue(void* ctx, nand_op_t* op) {
    nandpart_device_t *dev = (nandpart_device_t *)ctx;
    nand_op_t* translated_op = (nand_op_t *)((uint8_t *)op + dev->parent_op_size);
    uint32_t command = op->command;

    // copy client's op to translated op
    memcpy(translated_op, op, sizeof(*translated_op));

    // make offset relative to full underlying device
    if (command == NAND_OP_READ || command == NAND_OP_WRITE) {
        translated_op->rw.offset_nand += (dev->erase_block_start * dev->info.pages_per_block);
    } else if (command == NAND_OP_ERASE) {
        translated_op->erase.first_block += dev->erase_block_start;
    } else {
        op->completion_cb(op, ZX_ERR_NOT_SUPPORTED);
    }

    translated_op->completion_cb = nandpart_completion_cb;
    translated_op->cookie = op;

    // call parent's queue
    dev->nand_proto.ops->queue(dev->nand_proto.ctx, translated_op);
}

static void nandpart_get_bad_block_list(void* ctx, uint32_t* bad_blocks, uint32_t bad_block_len,
                                        uint32_t* num_bad_blocks) {
    // TODO implement this
    *num_bad_blocks = 0;
}

static void nandpart_unbind(void* ctx) {
    nandpart_device_t* device = ctx;
    device_remove(device->zxdev);
}

static void nandpart_release(void* ctx) {
    nandpart_device_t* device = ctx;
    free(device);
}

static zx_off_t nandpart_get_size(void* ctx) {
    nandpart_device_t* dev = ctx;
    //TODO: use query() results, *but* fvm returns different query and getsize
    // results, and the latter are dynamic...
    return device_get_size(dev->parent);
}

static zx_protocol_device_t device_proto = {
    .version = DEVICE_OPS_VERSION,
    .get_size = nandpart_get_size,
    .unbind = nandpart_unbind,
    .release = nandpart_release,
};

static nand_protocol_ops_t nand_ops = {
    .query = nandpart_query,
    .queue = nandpart_queue,
    .get_bad_block_list = nandpart_get_bad_block_list,
};

static zx_status_t nandpart_bind(void* ctx, zx_device_t* parent) {
    nand_protocol_t nand_proto;
    uint8_t buffer[METADATA_PARTITION_MAP_MAX];
    size_t actual;

    if (device_get_protocol(parent, ZX_PROTOCOL_NAND, &nand_proto) != ZX_OK) {
        zxlogf(ERROR, "nandpart: parent device '%s': does not support nand protocol\n",
               device_get_name(parent));
        return ZX_ERR_NOT_SUPPORTED;
    }

    zx_status_t status = device_get_metadata(parent, DEVICE_METADATA_PARTITION_MAP, buffer,
                                             sizeof(buffer), &actual);
    if (status != ZX_OK) {
        zxlogf(ERROR, "nandpart: parent device '%s' has no parititon map\n",
               device_get_name(parent));
        return status;
    }

    zbi_partition_map_t* pmap = (zbi_partition_map_t*)buffer;
    if (pmap->partition_count == 0) {
        zxlogf(ERROR, "nandpart: partition_count is zero\n");
        return ZX_ERR_INTERNAL;
    }

    // Query parent to get its nand_info_t and size for nand_op_t.
    nand_info_t nand_info;
    size_t parent_op_size;
    nand_proto.ops->query(nand_proto.ctx, &nand_info, &parent_op_size);
    // Make sure parent_op_size is aligned, so we can safely add our data at the end.
    parent_op_size = (parent_op_size + 7) | ~7;

    if (pmap->block_size != nand_info.page_size) {
        zxlogf(ERROR, "nandpart: pmap block size %"PRIu64 " does not match nand page size %u\n",
               pmap->block_size, nand_info.page_size);
        return ZX_ERR_INTERNAL;
    }

    size_t erase_block_size = nand_info.page_size * nand_info.pages_per_block;

    // Sanity check partition map first.
    // All partitions must start at an erase block boundary.
    for (unsigned i = 0; i < pmap->partition_count; i++) {
        zbi_partition_t* part = &pmap->partitions[i];
        uint64_t byte_offset = part->first_block * pmap->block_size;
        uint64_t erase_block_offset = byte_offset / erase_block_size;

        if (erase_block_offset * erase_block_size != byte_offset) {
            zxlogf(ERROR, "nandpart: partition %s offset %" PRIu64
                   " is not a multiple of erase_block_size %" PRIu64 "\n",
                   part->name, erase_block_offset, erase_block_size);
            return ZX_ERR_INTERNAL;
        }
    }
 
    // Create a device for each partition
    for (unsigned i = 0; i < pmap->partition_count; i++) {
        zbi_partition_t* part = &pmap->partitions[i];
        char name[128];

        snprintf(name, sizeof(name), "part-%03u", i);

        nandpart_device_t* device = calloc(1, sizeof(nandpart_device_t));
        if (!device) {
            return ZX_ERR_NO_MEMORY;
        }

        device->parent = parent;
        device->parent_op_size = parent_op_size;
        memcpy(&device->nand_proto, &nand_proto, sizeof(device->nand_proto));
        memcpy(&device->info, &nand_info, sizeof(nand_info));
        memcpy(&device->info.partition_guid, &part->type_guid, sizeof(device->info.partition_guid));
        device->info.num_blocks = part->last_block = part->first_block + 1;

         // We only use FTL for the FVM partition.
        if (memcmp(part->type_guid, fvm_guid, sizeof(fvm_guid)) == 0) {
             device->info.nand_class = NAND_CLASS_FTL;
         } else {
             device->info.nand_class = NAND_CLASS_BBS;
         }

        zx_device_prop_t props[] = {
            { BIND_PROTOCOL, 0, ZX_PROTOCOL_NAND },
            { BIND_NAND_CLASS, 0, device->info.nand_class },
        };

        device_add_args_t args = {
            .version = DEVICE_ADD_ARGS_VERSION,
            .name = name,
            .ctx = device,
            .ops = &device_proto,
            .proto_id = ZX_PROTOCOL_NAND,
            .proto_ops = &nand_ops,
            .props = props,
            .prop_count = countof(props),
            .flags = DEVICE_ADD_INVISIBLE,
        };

        zx_status_t status = device_add(parent, &args, &device->zxdev);
        if (status != ZX_OK) {
            free(device);
            return status;
        }

        // add empty partition map metadata to prevent this driver from binding to its child devices
        status = device_add_metadata(device->zxdev, DEVICE_METADATA_PARTITION_MAP, NULL, 0);
        if (status != ZX_OK) {
            device_remove(device->zxdev);
            free(device);
            continue;
        }

        // make device visible after adding metadata
        device_make_visible(device->zxdev);
    }

    return ZX_OK;
}

static zx_driver_ops_t nandpart_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = nandpart_bind,
};

ZIRCON_DRIVER_BEGIN(nandpart, nandpart_driver_ops, "zircon", "0.1", 2)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_NAND),
    BI_MATCH_IF(EQ, BIND_NAND_CLASS, NAND_CLASS_PARTMAP),
ZIRCON_DRIVER_END(nandpart)
