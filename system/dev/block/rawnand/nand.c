// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <bits/limits.h>
#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/protocol/platform-bus.h>
#include <ddk/protocol/platform-defs.h>
#include <ddk/protocol/platform-device.h>
#include <ddk/protocol/rawnand.h>
#include <ddk/io-buffer.h>
#include <hw/reg.h>

#include <zircon/assert.h>
#include <zircon/threads.h>
#include <zircon/types.h>
#include <zircon/status.h>
#include <sync/completion.h>

#include <string.h>

#include <soc/aml-common/aml-rawnand.h>
#include "nand.h"

#include "aml_rawnand_api.h"

/*
 * Database of settings for the NAND flash devices we support 
 */
struct nand_chip_table nand_chip_table[] = {
    { 0xEC, 0xDC, "Samsung", "K9F4G08U0F", { 25, 20, 15 }, true, 512,
      0, 0, 0, 0},
};

struct nand_chip_table default_chip =
{
    0x00, 0x00, "DEFAULT", "UNKNOWN", { 25, 20, 15 }, true, 512,
    0, 0, 0, 0
};

#define NAND_CHIP_TABLE_SIZE					\
    (sizeof(nand_chip_table)/sizeof(struct nand_chip_table))

/*
 * Find the entry in the NAND chip table database based on manufacturer
 * id and device id
 */
struct nand_chip_table *
find_nand_chip_table(uint8_t manuf_id, uint8_t device_id)
{
    uint32_t i;

    for (i = 0 ; i < NAND_CHIP_TABLE_SIZE ; i++)
        if (manuf_id == nand_chip_table[i].manufacturer_id &&
            device_id == nand_chip_table[i].device_id)
            return &nand_chip_table[i];
    return NULL;
}

/*
 * Generic wait function used by both program (write) and erase
 * functionality.
 */
zx_status_t nand_wait(aml_rawnand_t *rawnand, uint32_t timeout_ms)
{
    uint64_t total_time = 0;
    uint8_t cmd_status;

#if 0
    nand_command(rawnand, NAND_CMD_STATUS, -1, -1);
#else
    aml_cmd_ctrl(rawnand, NAND_CMD_STATUS,
                 NAND_CTRL_CLE | NAND_CTRL_CHANGE);
    aml_cmd_ctrl(rawnand, NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);
#endif
    while (!((cmd_status = aml_read_byte(rawnand)) & NAND_STATUS_READY)) {
            usleep(10);
            total_time += 10;
            if (total_time > (timeout_ms * 1000)) {
                break;
            }
    }
#if 0
    zxlogf(ERROR, "%s: waited %lu usecs\n", __func__, total_time);
    zxlogf(ERROR, "%s: cmd_status = %u\n", __func__, cmd_status);
#endif
    if (!(cmd_status & NAND_STATUS_READY)) {
        zxlogf(ERROR, "nand command wait timed out\n");
        return ZX_ERR_TIMED_OUT;
    }
    if (cmd_status & NAND_STATUS_FAIL) {
        zxlogf(ERROR, "%s: nand command returns error\n", __func__);
        return ZX_ERR_IO;
    }
    return ZX_OK;
}

/* 
 * Send command down to the controller.
 */
void nand_command(aml_rawnand_t *rawnand, unsigned int command,
                  int column, int page_addr)
{
    /* Emulate NAND_CMD_READOOB */
    if (command == NAND_CMD_READOOB) {
        column += rawnand->writesize;
        command = NAND_CMD_READ0;
    }

    /* Command latch cycle */
    aml_cmd_ctrl(rawnand, command, NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
    if (column != -1 || page_addr != -1) {
        int ctrl = NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE;

        /* Serially input address */
        if (column != -1) {
            /* Adjust columns for 16 bit buswidth */
            if (rawnand->controller_params.options & NAND_BUSWIDTH_16)
                column >>= 1;
            aml_cmd_ctrl(rawnand, column, ctrl);
            ctrl &= ~NAND_CTRL_CHANGE;
            aml_cmd_ctrl(rawnand, column >> 8, ctrl);
        }
        if (page_addr != -1) {
            aml_cmd_ctrl(rawnand, page_addr, ctrl);
            aml_cmd_ctrl(rawnand, page_addr >> 8,
                         NAND_NCE | NAND_ALE);
            /* One more address cycle for devices > 128MiB */
            if (rawnand->chipsize > 128)
                aml_cmd_ctrl(rawnand, page_addr >> 16,
                             NAND_NCE | NAND_ALE);
        }
    }
    aml_cmd_ctrl(rawnand, NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);

    switch (command) {
    case NAND_CMD_ERASE1:
    case NAND_CMD_ERASE2:
    case NAND_CMD_SEQIN:
    case NAND_CMD_PAGEPROG:
        return;
    case NAND_CMD_RESET:
        usleep(rawnand->controller_delay);
        aml_cmd_ctrl(rawnand, NAND_CMD_STATUS,
                     NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
        aml_cmd_ctrl(rawnand, NAND_CMD_NONE,
                     NAND_NCE | NAND_CTRL_CHANGE);
        while (!(aml_read_byte(rawnand) & NAND_STATUS_READY))
            ;
        return;

    case NAND_CMD_READ0:
        aml_cmd_ctrl(rawnand, NAND_CMD_READSTART,
                     NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
        aml_cmd_ctrl(rawnand, NAND_CMD_NONE,
                       NAND_NCE | NAND_CTRL_CHANGE);

        /* This applies to read commands */
    default:
        usleep(rawnand->controller_delay);
        return;
    }
}

/*
 * Main Read entry point into the NAND. Calls controller specific
 * read function.
 * data, oob: pointers to user oob/data buffers.
 * nandpage : NAND page address to read.
 * ecc_correct : Number of ecc corrected bitflips (< 0 indicates
 * ecc could not correct all bitflips - caller needs to check that).
 * retries : I see read errors (read not completing) reported occasionally
 * on different NAND pages. Retry of 3 works around this, in
 * fact the first retry addresses the vast majority of the read
 * non-completions.
 */
zx_status_t nand_read_page(aml_rawnand_t *rawnand,
                           void *data,
                           void *oob,
                           uint32_t nandpage,
                           int *ecc_correct,
                           int retries)
{
    zx_status_t status;
    
    retries++;
    do {
        status = aml_read_page_hwecc(rawnand, data, oob, nandpage,
                                     ecc_correct, false);
        if (status != ZX_OK)
            zxlogf(ERROR, "%s: Retrying Read@%u\n",
                   __func__, nandpage);        
    } while (status != ZX_OK && --retries > 0);
    if (status != ZX_OK)
        zxlogf(ERROR, "%s: Read error\n", __func__);
    return status;
}

zx_status_t nand_read_page0(aml_rawnand_t *rawnand,
                            void *data,
                            void *oob,
                            uint32_t nandpage,                                  
                            int *ecc_correct,
                            int retries)
{
    zx_status_t status;
    
    retries++;
    do {
        status = aml_read_page_hwecc(rawnand, data, oob,
                                     nandpage, ecc_correct, true);
    } while (status != ZX_OK && --retries > 0);
    if (status != ZX_OK)
        zxlogf(ERROR, "%s: Read error\n", __func__);                
    return status;
}

/*
 * Main Write entry point into the NAND. Calls controller specific 
 * write function.
 * data, oob: pointers to user oob/data buffers.
 * nandpage : NAND page address to read.
 */
zx_status_t nand_write_page(aml_rawnand_t *rawnand,
                            void *data,
                            void *oob,
                            uint32_t nandpage)
{
    zx_status_t status;
    
    if (aml_check_write_protect(rawnand)) {
        zxlogf(ERROR, "%s: Device is Write Protected, Cannot Erase\n",
               __func__);
        /* Zircon doesn't have EROFS, this seems closest */
        status = ZX_ERR_ACCESS_DENIED;
    } else
        status = aml_write_page_hwecc(rawnand, data, oob, nandpage, false);
    return status;
}

/*
 * Main Erase entry point into NAND. Calls controller specific 
 * erase function.
 * nandblock : NAND erase block address.
 */
zx_status_t nand_erase_block(aml_rawnand_t *rawnand,
                             uint32_t nandpage)
{
    zx_status_t status;

    if (aml_check_write_protect(rawnand)) {
        zxlogf(ERROR, "%s: Device is Write Protected, Cannot Erase\n",
               __func__);
        /* Zircon doesn't have EROFS, this seems closest */
        status = ZX_ERR_ACCESS_DENIED;
    } else
        status = aml_erase_block(rawnand, nandpage);
    return status;
}

