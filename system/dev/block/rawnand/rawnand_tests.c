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

#ifdef AML_RAWNAND_TEST
uint32_t num_bad_blocks = 0;
uint32_t bad_blocks[4096];

static bool
is_block_bad(uint32_t nandblock)
{
    uint32_t i;

    for (i = 0 ; i < num_bad_blocks; i++) {
        if (nandblock == bad_blocks[i]) {
            zxlogf(ERROR, "%s: Found bad block @%u, skipping\n",
                   __func__, nandblock);
            return true;
        }
    }
    return false;
}

#define WRITEBUF_SIZE           (512*1024)

/*
 * This file creates the data that we will write to the NAND device.
 * The file is created in /tmp. The idea here is that we can scp 
 * this file out and then compare the data we wrote to NAND.
 */
static zx_status_t
create_data_file(aml_rawnand_t* rawnand, uint64_t size)
{
    int fd;
    char *data;
    zx_status_t status;
    uint8_t *p;
    uint64_t to_write;
    
    fd = open("/tmp/nand-data-to-write", O_CREAT | O_WRONLY);
    if (fd < 0) {
        zxlogf(ERROR, "%s: Could not open file /tmp/nand-data-to-write\n",
               __func__);
        return ZX_ERR_IO;
    }
    data = malloc(WRITEBUF_SIZE);
    srand(zx_ticks_get());
    while (size > 0) {
        int write_bytes;
        
        write_bytes = to_write = MIN(WRITEBUF_SIZE, size);
        p = (uint8_t *)data;
        while (to_write > 0) {
            if (to_write >= sizeof(uint32_t)) {
                *(uint32_t *)p = random();
                to_write -= sizeof(uint32_t);
                p += sizeof(uint32_t);
            } else {
                *p++ = (uint8_t)random();
                to_write--;
            }
        }
        if (write(fd, data, write_bytes) != write_bytes) {
            zxlogf(ERROR, "%s: Could not open file /tmp/nand-data-to-write\n",
                   __func__);
            status = ZX_ERR_IO;
            break;
        } else
            status = ZX_OK;
        size -= write_bytes;
    }
    free(data);    
    fsync(fd);
    close(fd);
    return status;
}

static zx_status_t
erase_blocks_range(aml_rawnand_t* rawnand,
                   uint64_t start_erase_block,
                   uint64_t end_erase_block)
{
    uint64_t cur;
    zx_status_t status;
    
    for (cur = start_erase_block; cur < end_erase_block; cur++) {
        if (!is_block_bad(cur)) {
            status = nand_erase_block(rawnand,
                                      cur * rawnand->erasesize_pages);
            if (status != ZX_OK) {
                zxlogf(ERROR, "%s: Count not ERASE block @%lu\n",
                       __func__, cur);
                return status;
            }
            zxlogf(ERROR, "%s: ERASED block @%lu successfully\n",
                       __func__, cur);
        } else
            zxlogf(ERROR, "%s: SKIPPING erase of bad block Count @%lu\n",
                   __func__, cur);
            
    }
    return ZX_OK;
}

static zx_status_t
read_single_erase_block(aml_rawnand_t* rawnand,
                        uint64_t erase_block,
                        char *data,
                        char *oob)
{
    uint64_t *oob_long = (uint64_t *)oob;
    uint64_t start_page = erase_block * rawnand->erasesize_pages;
    zx_status_t status;
    int ecc_correct;
    uint64_t i;
    
    for (i = 0; i < rawnand->erasesize_pages; i++) {
        status = nand_read_page(rawnand,
                                data + (i * rawnand->writesize),
                                &oob_long[i],
                                start_page + i,
                                &ecc_correct, 3);
        if (status != ZX_OK) {
            zxlogf(ERROR, "%s: Read failed at @%lu\n",
                   __func__, start_page + i);        
            return status;
        }
        if (ecc_correct > 0)
            zxlogf(ERROR,
                   "%s: Corrected ECC errors %d on reading at %lu\n",
                   __func__, ecc_correct, start_page + i);
    }
    return ZX_OK;
}

static zx_status_t
read_blocks_range(aml_rawnand_t* rawnand,
                  const char *filename,
                  uint64_t start_erase_block,
                  uint64_t end_erase_block,
                  bool (*verify_pattern)(uint64_t cur_erase_block, uint64_t oob_data))
{
    int fd;
    uint64_t cur;
    char *data, *oob;
    uint64_t *oob_long;
    zx_status_t status = ZX_OK;
    uint64_t i;
    
    fd = open(filename, O_CREAT | O_WRONLY);
    if (fd < 0) {
        zxlogf(ERROR, "%s: Could not open file %s\n",
               __func__, filename);
        return ZX_ERR_IO;
    }
    data = malloc(rawnand->erasesize);
    oob = malloc(rawnand->erasesize_pages * 8); /* XXX 8 bytes OOB per page */
    for (cur = start_erase_block; cur < end_erase_block; cur++) {
        if (is_block_bad(cur))
            continue;
        status = read_single_erase_block(rawnand, cur, data, oob);
        if (status != ZX_OK) {
            zxlogf(ERROR, "%s: Count not Read block @%lu from %s\n",
                   __func__, cur, filename);
            goto out;
        }
        /* Compare the 8 bytes of OOB per page to the pattern passed in */
        oob_long = (uint64_t *)oob;
        for (i = 0; i < rawnand->erasesize_pages; i++) {
            if (!verify_pattern(cur, oob_long[i])) {
                zxlogf(ERROR, "%s: OOB post-Erase failed at %lu ! found %lu\n",
                       __func__, cur, oob_long[i]);
                status = ZX_ERR_IO;
                goto out;
            }
        }
        /* Write the entire eraseblock out to the file */
        if (write(fd, data, rawnand->erasesize) != rawnand->erasesize) {
            zxlogf(ERROR, "%s: Could not write to %s\n",
                   __func__, filename);
            status = ZX_ERR_IO;
            goto out;            
        }
    }
out:
    free(data);
    free(oob);
    close(fd);
    return status;
}

static zx_status_t
write_single_erase_block(aml_rawnand_t* rawnand,
                         uint64_t erase_block, char *data,
                         char *oob)
{
    uint64_t *oob_long = (uint64_t *)oob;
    uint64_t start_page = erase_block * rawnand->erasesize_pages;
    zx_status_t status;
    uint64_t i;
    
    for (i = 0; i < rawnand->erasesize_pages; i++) {
        status = nand_write_page(rawnand,
                                 data + (i * rawnand->writesize),
                                 &oob_long[i],                                 
                                 start_page + i);
        if (status != ZX_OK) {
            zxlogf(ERROR, "%s: Write failed at @%lu\n",
                   __func__, start_page + i);        
            return status;
        }
    }
    return ZX_OK;
}


static zx_status_t
write_blocks_range(aml_rawnand_t* rawnand,
                   const char *filename,
                   uint64_t start_erase_block,
                   uint64_t end_erase_block)
{
    int fd;
    uint64_t cur;
    char *data, *oob;
    uint64_t *oob_long;
    zx_status_t status = ZX_OK;
    uint64_t i;
    
    fd = open(filename, 0);
    if (fd < 0) {
        zxlogf(ERROR, "%s: Could not open file %s\n",
               __func__, filename);
        return ZX_ERR_IO;
    }
    data = malloc(rawnand->erasesize);
    oob = malloc(rawnand->erasesize_pages * 8); /* XXX 8 bytes OOB per page */    
    for (cur = start_erase_block; cur < end_erase_block; cur++) {
        if (is_block_bad(cur))
            continue;
        if (read(fd, data, rawnand->erasesize) != rawnand->erasesize) {
            zxlogf(ERROR, "%s: Could not read file %s\n",
                   __func__, filename);
            status = ZX_ERR_IO;
            goto out;
        }
        oob_long = (uint64_t *)oob;
        for (i = 0; i < rawnand->erasesize_pages; i++)
            oob_long[i] = cur; /* All 8 oob bytes for every page have same signature */
        status = write_single_erase_block(rawnand, cur, data, oob);
        if (status != ZX_OK) {
            zxlogf(ERROR, "%s: Count not write block @%lu\n",
                   __func__, cur);
            goto out;
        }
    }
out:
    free(data);
    free(oob);
    close(fd);
    return status;
}

/*
 * After a block has been erased, OOB bytes should be all 0xff
 */
static bool
verify_pattern_erased(uint64_t curblock, uint64_t oob_data)
{
    if (oob_data == ~((uint64_t)0)) {
#if 0
        zxlogf(ERROR, "%s: Erase Block %lu OOB signature OK !\n",
               __func__, curblock);
#endif
        return true;
    }
    return false;
}

/*
 * We write 8 OOB bytes for every page with the index of the *eraseblock*
 * this verifies the signature.
 */
static bool
verify_pattern_written(uint64_t cur_erase_block, uint64_t oob_data)
{
    if (oob_data == cur_erase_block)
        return true;
    return false;
}

/*
 * Given the range. Erase every block in the range and write the entire
 * range. Once the range is written, read the range back and save the 
 * data so we can compare the data that we wrote and the data we read back
 * 
 * This function creates 2 files in tmp: one file is the data we are writing
 * the range, and the second file is the data we read back (post write) for
 * the range. We expect these to be identical. As a side effect, the write
 * also encodes the (8 byte) page offset in the OOB bytes for every page
 * written and sanity checks this on the read.
 * 
 * Finally, need to figure out how many bad blocks are in the range, and 
 * we need to skip those.
 */
void rawnand_erase_write_test(aml_rawnand_t* rawnand,
                              uint64_t start_byte,
                              uint64_t end_byte)
{
    uint64_t start_erase_block, end_erase_block;
    uint64_t cur;
    uint64_t num_erase_blocks;
    zx_status_t status;

    /* Populate the bad block list, so we don't stomp on it */
    rawnand_dump_bbt(rawnand);
    /*
     * start_byte and end_byte must be erase block aligned.
     * If they are not, make them so.
     */
    if (start_byte % rawnand->erasesize)
        start_byte += rawnand->erasesize -
            (start_byte % rawnand->erasesize);
    if (end_byte % rawnand->erasesize)
        end_byte -= (end_byte % rawnand->erasesize);
    start_erase_block = start_byte / rawnand->erasesize;
    end_erase_block = end_byte / rawnand->erasesize;
    zxlogf(ERROR, "%s: start erase block = %lu\n",
           __func__, start_erase_block);
    zxlogf(ERROR, "%s: end erase block = %lu\n",
           __func__, end_erase_block);
    /*
     * How many bad blocks are in the range of erase blocks we
     * are going to erase + write to ?
     */
    num_erase_blocks = end_erase_block - start_erase_block;
    for (cur = start_erase_block; cur < end_erase_block; cur++)
        if (is_block_bad(cur)) {
            zxlogf(ERROR, "%s: Skipping erase block %lu\n",
                   __func__, cur);
            num_erase_blocks--;
        }
    zxlogf(ERROR, "%s: num erase block = %lu\n",
           __func__, num_erase_blocks);
    /* 
     * Create a file with known data for the size we are going
     * to write out.
     */
    status = create_data_file(rawnand,
                              num_erase_blocks * rawnand->erasesize);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: Count not create data file \n",
               __func__);
        return;
    }
#if 0
    /*
     * Next erase every block in the range (skipping over known bad 
     * blocks).
     */
    status = erase_blocks_range(rawnand, start_erase_block, end_erase_block);
    if (status != ZX_OK)
        return;

    /*
     * At this point, the data in the range should be all 0xff !
     * Let's read the data and save it away (so we can verify).
     */
    status = read_blocks_range(rawnand, "/tmp/nand-dump-before-write",
                               start_erase_block, end_erase_block,
                               verify_pattern_erased);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: Reading blocks before write (after erase) failed\n",
               __func__);
        return;
    }
    /*
     * Write blocks in the range reading data from the given file.
     */
    status = write_blocks_range(rawnand, "/tmp/nand-data-to-write",
                                start_erase_block, end_erase_block);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: Writing of blocks failed\n",
               __func__);
        return;
    }        
#endif
    /*
     * At this point, the data in the range has been completely written
     * Let's read the data and save it away (so we can verify).
     */
    status = read_blocks_range(rawnand, "/tmp/nand-dump-after-write",
                               start_erase_block, end_erase_block,
                               verify_pattern_written);
    if (status != ZX_OK)
        zxlogf(ERROR, "%s: Reading blocks after write failed\n",
               __func__);
}

/* 
 * Write the given page and read back the data before and after the
 * write.
 * It is assumed the page to be written has already been erased.
 * The page passed in must be eraseblock aligned.
 * The function snapshots the contents before and after the write.
 */
void rawnand_writetest_one_eraseblock(aml_rawnand_t* rawnand,
                                      uint32_t nandpage)
{
    int ecc_correct;
    zx_status_t status;
    char *data, *oob;
    uint32_t i;
    int fd;
    int ecc_pages_per_nand_page;
    uint32_t ecc_pagesize;

    /* nandpage must be aligned to a eraseblock boundary */
    if (nandpage % rawnand->erasesize_pages) {
        zxlogf(ERROR, "%s: NAND block %u must be a erasesize_pages (%u) multiple\n",
               __func__, nandpage, rawnand->erasesize_pages);
        return;
    }
    /* Populate the bad block list, so we don't stomp on it */
    rawnand_dump_bbt(rawnand);
    ecc_pagesize = aml_get_ecc_pagesize(rawnand, AML_ECC_BCH8);
    ecc_pages_per_nand_page =
        rawnand->writesize / ecc_pagesize;
    if (aml_check_write_protect(rawnand)) {
        zxlogf(ERROR, "%s: Device is Write Protected, Cannot Erase\n",
               __func__);
        return;
    } else
        zxlogf(ERROR, "%s: Device is not-Write Protected\n",
               __func__);
    if (is_block_bad(nandpage)) {
        zxlogf(ERROR, "%s: Cannot write to @%u - part of bad block\n",
                   __func__, nandpage);
        return;
    }
    data = malloc(rawnand->writesize);
    oob = malloc(rawnand->writesize);    
    /* 
     * Read existing data and dump that out to a file 
     */
    fd = open("/tmp/nand-dump-before-write", O_CREAT | O_WRONLY);
    if (fd < 0) {
        zxlogf(ERROR, "%s: Could not open file /tmp/nand-dump-before-write\n",
               __func__);
        return;
    }
    for (i = 0 ; i < rawnand->erasesize_pages ; i++) {
        ecc_correct = 0;
        memset((char *)rawnand->data_buf, 0, rawnand->writesize);
        memset((char *)rawnand->info_buf, 0,
               ecc_pages_per_nand_page * sizeof(struct aml_info_format));
        status = nand_read_page(rawnand, data, oob, nandpage + i,
                                &ecc_correct, 3);
        if (status != ZX_OK) {
            zxlogf(ERROR, "%s: Read failed at %u\n",
                   __func__, nandpage + i);
            return;
        }
        zxlogf(ERROR,
               "%s: OOB bytes for page %u: \n", __func__, nandpage + i);
        for (int j = 0; j < 8; j++)
            zxlogf(ERROR, " 0x%x", oob[j]);
        zxlogf(ERROR, "\n");            
        if (ecc_correct > 0)
            zxlogf(ERROR,
                   "%s: Corrected ECC errors %d on reading at %u\n",
                   __func__, ecc_correct, nandpage);
        if (write(fd, data, rawnand->writesize) != rawnand->writesize) {
            zxlogf(ERROR, "%s: Could not write to file /tmp/nand-dump-before-write\n",
                   __func__);
        }
    }
    fsync(fd);    
    close(fd);
    if (create_data_file(rawnand, rawnand->writesize * rawnand->erasesize_pages) != ZX_OK) {
        zxlogf(ERROR, "%s: Could not create data file /tmp/nand-data-to-write\n",
               __func__);
        free(data);
        free(oob);
        return;
    }
    /*
     * Do the program operation, then re-read the pages, storing it in "after".
     */
    fd = open("/tmp/nand-data-to-write", 0);
    if (fd < 0) {
        zxlogf(ERROR, "%s: Could not open file /tmp/nand-data-to-write\n",
               __func__);
        return;
    }
    for (i = 0 ; i < rawnand->erasesize_pages ; i++) {
        if (read(fd, data, rawnand->writesize) != rawnand->writesize) {
            zxlogf(ERROR, "%s: Could not read file /tmp/nand-data-to-write\n",
                   __func__);
        }
        memset((char *)rawnand->info_buf, 0,
               ecc_pages_per_nand_page * sizeof(struct aml_info_format));
        *(uint32_t *)oob = (nandpage + i);
        status = nand_write_page(rawnand, data, oob, nandpage + i);
        if (status != ZX_OK) {
            zxlogf(ERROR, "%s: Write failed to page %u\n",
                   __func__, nandpage + i);
            return;
        }
    }
    close(fd);
    fd = open("/tmp/nand-dump-after-write", O_CREAT | O_WRONLY);
    if (fd < 0) {
        zxlogf(ERROR, "%s: Could not open file /tmp/nand-dump-after-write\n",
               __func__);
        return;
    }
    for (i = 0 ; i < rawnand->erasesize_pages ; i++) {
        ecc_correct = 0;
        memset((char *)rawnand->data_buf, 0, rawnand->writesize);
        memset((char *)rawnand->info_buf, 0,
               ecc_pages_per_nand_page * sizeof(struct aml_info_format));
        status = nand_read_page(rawnand, data, oob, nandpage + i,
                                &ecc_correct, 3);
        if (status != ZX_OK) {
            zxlogf(ERROR, "%s: Read failed at %u\n",
                   __func__, nandpage + i);
            return;
        }
        if (ecc_correct > 0)
            zxlogf(ERROR,
                   "%s: Corrected ECC errors %d on reading at %u\n",
                   __func__, ecc_correct, nandpage + i);
        if (write(fd, data, rawnand->writesize) != rawnand->writesize) {
            zxlogf(ERROR, "%s: Could not write to file /tmp/nand-dump-after-write\n",
                   __func__);
        }
        zxlogf(ERROR, "%s: OOB bytes for page @%u = %u\n",
               __func__, nandpage + i, *(uint32_t *)oob);
    }
    fsync(fd);
    close(fd);
    free(data);
    free(oob);
}

/*
 * Erase a block, read it back and make sure entire block has been
 * erased.
 * The function finds the last block we can erase (that is not in the 
 * bad block table) and erases that block.
 */
void rawnand_erasetest_one_block(aml_rawnand_t* rawnand)
{
    uint32_t num_nand_pages;
    uint32_t cur_nand_page;
    int ecc_correct;
    zx_status_t status;
    char *data, *oob;
    uint32_t i;
    int fd;
    int ecc_pages_per_nand_page;
    uint32_t ecc_pagesize;

    /* Populate the bad block list, so we don't stomp on it */
    rawnand_dump_bbt(rawnand);
    ecc_pagesize = aml_get_ecc_pagesize(rawnand, AML_ECC_BCH8);
    if (aml_check_write_protect(rawnand)) {
        zxlogf(ERROR, "%s: Device is Write Protected, Cannot Erase\n",
               __func__);
        return;
    } else
        zxlogf(ERROR, "%s: Device is not-Write Protected\n",
               __func__);
    ecc_pages_per_nand_page =
        rawnand->writesize / ecc_pagesize;
    num_nand_pages = (rawnand->chipsize * (1024 * 1024)) /
        rawnand->writesize;
    zxlogf(ERROR, "%s: num_nand_pages = %u\n",
           __func__, num_nand_pages);        
    /* Let's pick the very last erase block */
    cur_nand_page = num_nand_pages - 1;
    zxlogf(ERROR, "%s: rawnand->erasesize_pages = %u\n",
           __func__, rawnand->erasesize_pages);
    zxlogf(ERROR, "%s: Number of Bad blocks %u\n", __func__,
           num_bad_blocks);    
    for (i = 0; i < num_bad_blocks; i++)
        zxlogf(ERROR, "%s: Bad block at %u\n", __func__, bad_blocks[i]);
    while (cur_nand_page > 0) {
        uint32_t nandblock = cur_nand_page / rawnand->erasesize_pages;
        
        cur_nand_page &= ~(rawnand->erasesize_pages - 1);
        if (!is_block_bad(nandblock)) {
            zxlogf(ERROR, "%s: About to erase block @%u\n",
                   __func__, nandblock);
            break;
        }
        cur_nand_page -= MIN(rawnand->erasesize_pages, cur_nand_page);
    }
    if (cur_nand_page == 0) {
        zxlogf(ERROR, "%s: Could not find a block to erase\n",
               __func__);
        return;
    }
    data = malloc(rawnand->writesize);
    oob = malloc(rawnand->writesize);    
    /* 
     * Read existing data and dump that out to a file 
     */
    fd = open("/tmp/nand-dump-before-erase", O_CREAT | O_WRONLY);
    if (fd < 0) {
        zxlogf(ERROR, "%s: Could not open file /tmp/nand-dump-before-erase\n",
               __func__);
        return;
    }
    for (i = 0 ; i < rawnand->erasesize_pages ; i++) {
        ecc_correct = 0;
        memset((char *)rawnand->data_buf, 0, rawnand->writesize);
        memset((char *)rawnand->info_buf, 0,
               ecc_pages_per_nand_page * sizeof(struct aml_info_format));
        status = nand_read_page(rawnand, data, NULL, cur_nand_page + i,
                                &ecc_correct, 3);
        if (status != ZX_OK) {
            zxlogf(ERROR, "%s: Read failed at %u\n",
                   __func__, cur_nand_page + i);
            return;
        }
        if (ecc_correct > 0)
            zxlogf(ERROR,
                   "%s: Corrected ECC errors %d on reading at %u\n",
                   __func__, ecc_correct, cur_nand_page);
        if (write(fd, data, rawnand->writesize) != rawnand->writesize) {
            zxlogf(ERROR, "%s: Could not write to file /tmp/nand-dump-before-erase\n",
                   __func__);
        }
    }
    fsync(fd);    
    close(fd);
    /*
     * Do the erase operation, then re-read the pages, storing it in "after".
     */
    status = nand_erase_block(rawnand, cur_nand_page);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: Erase block failed %u\n",
               __func__, cur_nand_page);
        return;
    }
    fd = open("/tmp/nand-dump-after-erase", O_CREAT | O_WRONLY);
    if (fd < 0) {
        zxlogf(ERROR, "%s: Could not open file /tmp/nand-dump-after-erase\n",
               __func__);
        return;
    }
    for (i = 0 ; i < rawnand->erasesize_pages ; i++) {
        ecc_correct = 0;
        memset((char *)rawnand->data_buf, 0, rawnand->writesize);        
        memset((char *)rawnand->info_buf, 0,
               ecc_pages_per_nand_page * sizeof(struct aml_info_format));
        memset(oob, 0, rawnand->writesize);        
        status = nand_read_page(rawnand, data, oob, cur_nand_page + i,
                                &ecc_correct, 3);
        if (status != ZX_OK) {
            zxlogf(ERROR, "%s: Read failed at %u\n",
                   __func__, cur_nand_page + i);
            return;
        }
        if (ecc_correct > 0)
            zxlogf(ERROR,
                   "%s: Corrected ECC errors %d on reading at %u\n",
                   __func__, ecc_correct, cur_nand_page + i);
        zxlogf(ERROR,
               "%s: OOB bytes for page %u: \n", __func__, cur_nand_page + i);
        for (int j = 0; j < 8; j++)
            zxlogf(ERROR, " 0x%x", oob[j]);
        zxlogf(ERROR, "\n");        
        if (write(fd, data, rawnand->writesize) != rawnand->writesize) {
            zxlogf(ERROR, "%s: Could not write to file /tmp/nand-dump-after-erase\n",
                   __func__);
        }
    }
    fsync(fd);
    close(fd);
    free(data);
    free(oob);    
}

static void rawnand_test_page0_sub(aml_rawnand_t *rawnand,
                                   uint32_t nandpage)
{
    int ecc_correct;
    zx_status_t status;
    char *data;
    uint8_t *oob;
    nand_page0_t *page0;
    
    data = malloc(rawnand->writesize);
    memset(data, 0xff, rawnand->writesize);
    oob = malloc(rawnand->writesize);
    memset(oob, 0xff, rawnand->writesize);    
    zxlogf(ERROR, "%s: Reading page0@%u \n", __func__, nandpage);    
    status = nand_read_page0(rawnand, data, oob, nandpage, &ecc_correct, 3);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: Read page0 failed\n", __func__);
        goto out;
    }
    zxlogf(ERROR, "%s: Read page0 succeeded\n", __func__);
    if (ecc_correct < 0) {
        zxlogf(ERROR,
               "%s: Un-Correctable ECC errors on reading page0\n",
               __func__);
        goto out;
    }
    if (ecc_correct > 0)
        zxlogf(ERROR,
               "%s: Corrected ECC errors %d on reading page0\n",
               __func__, ecc_correct);
    if (memcmp(data, rawnand->data_buf, 384)) {
        zxlogf(ERROR, "%s: Something went wrong reading page0\n",
               __func__);
        goto out;
    }
    page0 = (nand_page0_t *)data;
    zxlogf(ERROR, "cfg.d32 = 0x%x\n", page0->nand_setup.cfg.d32);
    uint32_t val = page0->nand_setup.cfg.d32 & 0x3f;
    zxlogf(ERROR, "ecc_step = %u\n", val);
    val = (page0->nand_setup.cfg.d32 >> 6) & 0x7f;
    zxlogf(ERROR, "pagesize = %u\n", val);
    
    zxlogf(ERROR, "ext_info.read_info = 0x%x\n", page0->ext_info.read_info);
    zxlogf(ERROR, "ext_info.page_per_blk = 0x%x\n", page0->ext_info.page_per_blk);
    zxlogf(ERROR, "ext_info.boot_num = 0x%x\n", page0->ext_info.boot_num);
    zxlogf(ERROR, "ext_info.each_boot_pages = 0x%x\n", page0->ext_info.each_boot_pages);
    zxlogf(ERROR, "ext_info.bbt_occupy_pages = 0x%x\n", page0->ext_info.bbt_occupy_pages);
    zxlogf(ERROR, "ext_info.bbt_start_block = 0x%x\n", page0->ext_info.bbt_start_block);
    zxlogf(ERROR, "OOB bytes =  0x%x 0x%x\n", *oob, *(oob + 1));
    
out:
    free(data);
    free(oob);
}

/*
 * Read all copies of page0 and dump them out 
 */
void rawnand_test_page0(aml_rawnand_t* rawnand)
{
    uint32_t i;

    for (i = 0; i < 7; i++)
        rawnand_test_page0_sub(rawnand, 128 * i);
}

struct bbt_oobinfo {
    char name[4];
    int16_t ec;
    unsigned timestamp: 15;
    unsigned status_page: 1;
};

/*
 * Dump pages 1024/1025/1026 to see if the Linux MTD bbt is present
 * among those pages.
 */
void rawnand_dump_bbt(aml_rawnand_t *rawnand)
{
    uint32_t cur_nand_page;
    int ecc_correct;
    zx_status_t status;
    char *data;
    uint8_t *oob;
    int ecc_pages_per_nand_page;
    int fd;
    char filename[512];
    uint32_t ecc_pagesize;
    struct bbt_oobinfo *bbt_oob;
    char name[5];
    
    ecc_pagesize = aml_get_ecc_pagesize(rawnand, AML_ECC_BCH8);
    ecc_pages_per_nand_page =
        rawnand->writesize / ecc_pagesize;
    data = malloc(rawnand->writesize);
    oob = malloc(rawnand->writesize);
    cur_nand_page = (20*64);
    sprintf(filename, "/tmp/bbt");
    fd = open(filename, O_CREAT | O_WRONLY);
    if (fd < 0) {
	zxlogf(ERROR, "%s: Could not open file /tmp/nand-dump\n",
	       __func__);
	return;
    }
    memset(oob, 0xff, rawnand->writesize);
    ecc_correct = 0;
    memset((char *)rawnand->info_buf, 0,
	   ecc_pages_per_nand_page * sizeof(struct aml_info_format));
    status = nand_read_page(rawnand, data, oob, cur_nand_page,
			   &ecc_correct, 3);
    if (status != ZX_OK) {
	zxlogf(ERROR, "%s: Read failed at %u\n",
	       __func__, cur_nand_page);
	return;
    }
    if (write(fd, data, rawnand->writesize) != rawnand->writesize) {
	zxlogf(ERROR, "%s: Could not write to file %s\n", __func__, filename);
    }
    if (ecc_correct > 0)
	zxlogf(ERROR,
	       "%s: Corrected ECC errors %d on reading at %u\n",
	       __func__, ecc_correct, cur_nand_page);

    cur_nand_page++;
    memset(oob, 0xff, rawnand->writesize);    
    ecc_correct = 0;
    memset((char *)rawnand->info_buf, 0,
	   ecc_pages_per_nand_page * sizeof(struct aml_info_format));
    status = nand_read_page(rawnand, data, oob, cur_nand_page,
                            &ecc_correct, 3);
    if (status != ZX_OK) {
	zxlogf(ERROR, "%s: Read failed at %u\n",
	       __func__, cur_nand_page);
	return;
    }
    if (write(fd, data, rawnand->writesize) != rawnand->writesize) {
	zxlogf(ERROR, "%s: Could not write to file %s\n", __func__, filename);
    }
    if (ecc_correct > 0)
	zxlogf(ERROR,
	       "%s: Corrected ECC errors %d on reading at %u\n",
	       __func__, ecc_correct, cur_nand_page);

    bbt_oob = (struct bbt_oobinfo *)oob;
    strncpy(name, bbt_oob->name, 4);
    name[4] = '\0';
    zxlogf(ERROR, "%s: bbt_oob: name=%s ec=%d timestamp=%d status_page=%d\n",
	   __func__, name, bbt_oob->ec, bbt_oob->timestamp, bbt_oob->status_page);
    
    fsync(fd);    
    close(fd);
    free(data);
    free(oob);
    data = malloc(4096);
    fd = open(filename, O_CREAT | O_RDONLY);
    if (fd < 0) {
	zxlogf(ERROR, "%s: Could not open file /tmp/nand-dump\n",
	       __func__);
	return;
    }
    if (read(fd, data, 4096) != 4096) {
	zxlogf(ERROR, "%s: Could not write to file %s\n", __func__, filename);
    }
    char *p = data;
    for (uint32_t i = 0; i < 4096; i++) {
        if (*p++ != 0) {
            zxlogf(ERROR, "%s: Bad block at %u\n", __func__, i);
            bad_blocks[num_bad_blocks++] = i;
        }
    }
    close(fd);
    free(data);        
}

/* 
 * Read all pages and dump them all out to a file for analysis
 */
void rawnand_read_device_test(aml_rawnand_t* rawnand)
{
    uint32_t num_nand_pages;
    uint32_t cur_nand_page;
    int ecc_correct;
    zx_status_t status;
    char *data;
    uint32_t num_pages_read = 0;
    uint32_t ecc_pages_per_nand_page;
    uint64_t checksum = 0;
    int fd;
    uint32_t ecc_pagesize;

    fd = open("/tmp/nand-dump", O_CREAT | O_WRONLY);
    if (fd < 0) {
        zxlogf(ERROR, "%s: Could not open file /tmp/nand-dump\n",
               __func__);
        return;
    }
    ecc_pagesize = aml_get_ecc_pagesize(rawnand,
                    rawnand->controller_params.bch_mode);
    ecc_pages_per_nand_page =
        rawnand->writesize / ecc_pagesize;
    zxlogf(ERROR, "%s: ECC pages per NAND page = %d\n",
           __func__, ecc_pages_per_nand_page);
    num_nand_pages = (rawnand->chipsize * (1024 * 1024)) /
        rawnand->writesize;
    data = malloc(rawnand->writesize);
    for (cur_nand_page = 0;
         cur_nand_page < num_nand_pages;
         cur_nand_page++) {
        ecc_correct = 0;
        memset(data, 0xff, rawnand->writesize);
        memset((char *)rawnand->info_buf, 0,
               ecc_pages_per_nand_page * sizeof(struct aml_info_format));
        if ((cur_nand_page <= 896) && ((cur_nand_page % 128) == 0)) {
            status = nand_read_page0(rawnand, data, NULL, cur_nand_page,
                                     &ecc_correct, 3);
            /* Fill up the rest of the page with 0xff */
            memset(data + 384, 0xff, rawnand->writesize - 384);
        } else {
            status = nand_read_page(rawnand, data, NULL, cur_nand_page,
                                    &ecc_correct, 3);
        }
        /*
         * If the write fails, we write 0xff for that page, instead of
         * skipping that write, so the offsets don't get messed up.
         */
        if (status != ZX_OK) {
            zxlogf(ERROR, "%s: Read failed at %u\n",
                   __func__, cur_nand_page);
            memset(data, 0xff, rawnand->writesize);
        }
        uint32_t i;
        uint64_t *p;
            
        num_pages_read++;
        p = (uint64_t *)rawnand->data_buf;
        for (i = 0; i < rawnand->writesize/sizeof(uint64_t); i++)
            checksum += *p++;
        if (write(fd, data, rawnand->writesize) != rawnand->writesize) {
            zxlogf(ERROR, "%s: Could not write to file /tmp/nand-dump\n",
                   __func__);
        }
        if (status == ZX_OK && ecc_correct > 0)
            zxlogf(ERROR,
                   "%s: Corrected ECC errors %d on reading at %u\n",
                   __func__, ecc_correct, cur_nand_page);
    }
    zxlogf(ERROR, "%s: pages_read = %u\n", __func__, num_pages_read);
    zxlogf(ERROR, "%s: checksum = 0x%lx\n", __func__, checksum);
    fsync(fd);    
    close(fd);
}
#endif
