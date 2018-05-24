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
#include <ddk/protocol/nand.h>
#include <ddk/io-buffer.h>
#include <hw/reg.h>

#include <zircon/assert.h>
#include <zircon/threads.h>
#include <zircon/types.h>
#include <zircon/status.h>
#include <sync/completion.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>

#include <string.h>

#include "nand.h"

#include <unittest/unittest.h>

static uint32_t num_bad_blocks = 0;
static bool bad_blocks[4096];
static uint32_t bbt_start_block = 0;

/*
 * WARNING : The tests here depend on being able to create files in
 * /tmp (for validation checking). To be able to create files in /tmp
 * you need a devmgr change (which is committed as part of the NAND
 * driver), so you will need to patch in that devmgr change first.
 * See the README for more details.
 */

static bool erased(uint8_t*data, uint32_t len);

/*
 * Lookup the bad block list, and return whether a block is bad
 */
static bool is_block_bad(uint32_t nandblock)
{
    if (nandblock >= num_bad_blocks) {
            printf("%s: Out of range nandblock (%u), this is FATAL\n",
                   __func__, nandblock);
            exit(1);
    }
    return (bad_blocks[nandblock]);
}

#define WRITEBUF_SIZE           (512*1024)

/*
 * Compare the data in 2 files, returns true if files are
 * identical.
 */
static bool compare_files(const char *file1, const char *file2)
{
    bool ret = false;
    int fd1, fd2;
    char *buf1, *buf2;
    struct stat stat1, stat2;

    fd1 = open(file1, 0);
    if (fd1 < 0) {
        printf("%s: Could not open file %s\n",
               __func__, file1);
        return ret;
    }
    fd2 = open(file2, 0);
    if (fd1 < 0) {
        printf("%s: Could not open file %s\n",
               __func__, file1);
        close(fd1);
        return ret;
    }
    if (fstat(fd1, &stat1) < 0 || fstat(fd2, &stat2) < 0) {
        printf("%s: Could not stat files\n",
               __func__);
        goto out1;
    }
    if (stat1.st_size != stat2.st_size) {
        printf("%s: File sizes differ %llu vs %llu\n",
               __func__, stat1.st_size, stat2.st_size);
        goto out1;
    }
    buf1 = malloc(WRITEBUF_SIZE);
    if (buf1 == NULL) {
        printf("%s: Could not malloc buf1\n",
               __func__);
        exit(1);
    }
    buf2 = malloc(WRITEBUF_SIZE);
    if (buf2 == NULL) {
        printf("%s: Could not malloc buf2\n",
               __func__);
        exit(1);
    }
    uint64_t size = stat1.st_size;

    while (size > 0) {
        int to_write = MIN(size, WRITEBUF_SIZE);

        if ((read(fd1, buf1, to_write) != to_write) ||
            (read(fd2, buf2, to_write) != to_write)) {
            printf("%s: Could not read data from files\n",
                   __func__);
            goto out;
        }
        if (memcmp(buf1, buf2, to_write)) {
            printf("%s: Files differ\n",
                   __func__);
            goto out;
        }
        size -= to_write;
    }
    ret = true;
out:
    free(buf1);
    free(buf2);
out1:
    close(fd1);
    close(fd2);
    return ret;
}

/*
 * Has the entire range been erased properly ?
 * Validates that by comparing the data we snapped and
 * and saved to /tmp
 */
static bool validate_erased_range(const char *file)
{
    bool ret = false;
    int fd;
    char *buf;
    struct stat stat;

    fd = open(file, 0);
    if (fd < 0) {
        printf("%s: Could not open file %s\n",
               __func__, file);
        return ret;
    }
    if (fstat(fd, &stat) < 0) {
        printf("%s: Could not stat file\n",
               __func__);
        close(fd);
        return ret;
    }
    buf = malloc(WRITEBUF_SIZE);
    if (buf == NULL) {
        printf("%s: Could not malloc buf\n",
               __func__);
        exit(1);
    }
    uint64_t size = stat.st_size;
    while (size > 0) {
        int to_read = MIN(size, WRITEBUF_SIZE);

        if (read(fd, buf, to_read) != to_read) {
            printf("%s: Could not read data from file\n",
                   __func__);
            goto out;
        }
        if (erased((uint8_t *)buf, (int)to_read) == false) {
            printf("%s: File not erased properly\n",
                   __func__);
            goto out;
        }
        size -= to_read;
    }
    ret = true;
out:
    free(buf);
    close(fd);
    return ret;
}

/*
 * This creates the data that we will write to the NAND device.
 * The file is created in /tmp. The idea here is that we can scp
 * this file out and then compare the data we wrote to NAND.
 */
static zx_status_t create_data_file(uint64_t size)
{
    int fd;
    char *data;
    zx_status_t status;
    uint8_t *p;
    uint64_t to_read;

    fd = open("/tmp/nand-data-to-write", O_CREAT | O_WRONLY);
    if (fd < 0) {
        printf("%s: Could not open file /tmp/nand-data-to-write\n",
               __func__);
        return ZX_ERR_IO;
    }
    data = malloc(WRITEBUF_SIZE);
    if (data == NULL) {
        printf("%s: Could not malloc data\n",
               __func__);
        exit(1);
    }
    srand(zx_ticks_get());
    while (size > 0) {
        int write_bytes;

        write_bytes = to_read = MIN(WRITEBUF_SIZE, size);
        p = (uint8_t *)data;
        while (to_read > 0) {
            if (to_read >= sizeof(uint32_t)) {
                *(uint32_t *)p = random();
                to_read -= sizeof(uint32_t);
                p += sizeof(uint32_t);
            } else {
                *p++ = (uint8_t)random();
                to_read--;
            }
        }
        if (write(fd, data, write_bytes) != write_bytes) {
            printf("%s: Could not open file /tmp/nand-data-to-write\n",
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

/*
 * Erase all blocks in the given range, *not including* the
 * end_erase_block.
 */
static zx_status_t erase_blocks_range(int fd, nand_info_t *nand_info,
                                      uint64_t start_erase_block,
                                      uint64_t end_erase_block)
{
    nandtest_cmd_erase_block_t cmd;
    nandtest_resp_t resp;
    int rc;

    for (uint64_t cur = start_erase_block; cur < end_erase_block; cur++) {
        if (!is_block_bad(cur)) {
            cmd.nandblock = cur;
            rc = fdio_ioctl(fd, IOCTL_NAND_ERASE_BLOCK, &cmd, sizeof(cmd),
                            &resp, sizeof(resp));
            if (rc <= 0 || resp.status != ZX_OK) {
                printf("%s: Got response %d from fdio_ioctl, status = %d\n",
                       __func__, rc, resp.status);
                return ZX_ERR_IO;
            }
            if (((cur - start_erase_block) % 40) == 0)
                printf("\n");
            printf(".");
        } else
            printf("%s: SKIPPING erase of bad block Count @%lu\n",
                   __func__, cur);
    }
    printf("\n");
    printf("Successfully erased blocks %lu -> %lu\n",
           start_erase_block, end_erase_block);
    return ZX_OK;
}

/*
 * Read the data+oob for a single given page.
 * the "page0" argument is used to specify reads of a page0 page.
 */
static zx_status_t read_single_page_data_oob(int fd, nand_info_t *nand_info,
                                             uint32_t nand_page, char *data,
                                             char *oob)
{
    nandtest_rw_page_data_oob_t cmd;
    nandtest_resp_t resp_hdr;
    int rc;
    zx_status_t status;
    zx_handle_t vmo_data;
    zx_handle_t vmo_oob;
    char *vaddr_data, *vaddr_oob;

    status = zx_vmo_create(nand_info->page_size,
                           0,
                           &vmo_data);
    if (status != ZX_OK) {
        printf("%s: vmo_create failed for data %d\n", __func__, status);
        return status;
    }
    if (data) {
        status = zx_vmar_map(zx_vmar_root_self(),
                             0,
                             vmo_data,
                             0,
                             nand_info->page_size,
                             ZX_VM_FLAG_PERM_READ | ZX_VM_FLAG_PERM_WRITE,
                             (uintptr_t*)&vaddr_data);
        if (status != ZX_OK) {
            printf("nand read page: Cannot map data vmo\n");
            return status;
        }
    }
    status = zx_vmo_create(nand_info->oob_size,
                           0,
                           &vmo_oob);
    if (status != ZX_OK) {
        zx_handle_close(vmo_data);
        printf("%s: vmo_create failed for oob %d\n", __func__, status);
        return status;
    }
    if (oob) {
        status = zx_vmar_map(zx_vmar_root_self(),
                             0,
                             vmo_oob,
                             0,
                             nand_info->oob_size,
                             ZX_VM_FLAG_PERM_READ | ZX_VM_FLAG_PERM_WRITE,
                             (uintptr_t*)&vaddr_oob);
        if (status != ZX_OK) {
            printf("nand read page: Cannot map oob vmo\n");
            return status;
        }
    }
    cmd.nand_page = nand_page;
    if (data)
        cmd.data_len = 1;
    else
        cmd.data_len = 0;
    if (oob)
        cmd.oob_len = nand_info->oob_size;
    else
        cmd.oob_len = 0;
    cmd.data = vmo_data;
    cmd.oob = vmo_oob;
    rc = fdio_ioctl(fd, IOCTL_NAND_READ_PAGE_DATA_OOB, &cmd, sizeof(cmd),
                    &resp_hdr, sizeof(resp_hdr));
    if (rc <= 0) {
        printf("%s: Got response %d from fdio_ioctl\n", __func__, rc);
        status = ZX_ERR_IO;
        goto out;
    }
    if (resp_hdr.status != ZX_OK) {
        printf("%s: Got ZX error from PAGE read %d\n", __func__,
               resp_hdr.status);
        status = resp_hdr.status;
        goto out;
    }
    /* Copy data out */
    if (data) {
        memcpy(data, vaddr_data, nand_info->page_size);
        status = zx_vmar_unmap(zx_vmar_root_self(),
                               (uintptr_t)vaddr_data, nand_info->page_size);
    }
    /* Copy OOB out */
    if (oob) {
        memcpy(oob, vaddr_oob, nand_info->page_size);
        status = zx_vmar_unmap(zx_vmar_root_self(),
                               (uintptr_t)vaddr_oob, nand_info->oob_size);
    }
out:
    zx_handle_close(vmo_data);
    zx_handle_close(vmo_oob);
    return status;
}

/*
 * building block used by read_eraseblock_range() below. this function reds the data and 
 * the oob for a range of pages
 */
static zx_status_t read_range_data_oob(int fd, nand_info_t *nand_info,
                                       uint32_t start_page, uint32_t numpages,
                                       char *data, char *oob)
{
    for (uint32_t i = 0; i < numpages; i++) {
        char *oob_next;
        char *data_next;

        if (oob)
            oob_next = oob + i * nand_info->oob_size;
        else
            oob_next = NULL;
        if (data)
            data_next = data + i * nand_info->page_size;
        else
            data_next = NULL;
        zx_status_t status;
        status = read_single_page_data_oob(fd, nand_info,
                                           start_page + i,
                                           data_next,
                                           oob_next);
        if (status != ZX_OK) {
            printf("%s: Read failed at @%u\n",
                   __func__, start_page + i);
            return status;
        }
     }
    return ZX_OK;
}

/*
 * Read one eraseblock at a time, verify the data therein, and write the
 * data out
 */
static zx_status_t read_eraseblock_range(int fd, nand_info_t *nand_info,
                                         const char *filename,
                                         uint64_t start_erase_block,
                                         uint64_t end_erase_block,
                                         bool (*verify_pattern)(uint64_t cur_erase_block,
                                                                uint64_t oob_data))
{
    int fd1;
    char *data, *oob;
    uint64_t *oob_long;
    zx_status_t status = ZX_OK;
    ssize_t eraseblock_bytes = nand_info->pages_per_block * nand_info->page_size;

    if (filename != NULL) {
        fd1 = open(filename, O_CREAT | O_WRONLY);
        if (fd1 < 0) {
            printf("%s: Could not open file %s\n",
                   __func__, filename);
            return ZX_ERR_IO;
        }
    } else
        fd1 = -1;       /* Force errors if IO is attempted */

    data = malloc(eraseblock_bytes);
    if (data == NULL) {
        printf("%s: Could not malloc data\n",
               __func__);
        exit(1);
    }
    oob = malloc(nand_info->pages_per_block * nand_info->oob_size);
    if (oob == NULL) {
        printf("%s: Could not malloc oob\n",
               __func__);
        exit(1);
    }
    for (uint64_t cur = start_erase_block; cur < end_erase_block; cur++) {
        if (is_block_bad(cur))
            continue;
        fflush(stdout);
        status = read_range_data_oob(fd, nand_info, cur * nand_info->pages_per_block,
                                     nand_info->pages_per_block, data, oob);
        if (status != ZX_OK) {
            printf("%s: Count not Read block @%lu from %s\n",
                   __func__, cur, filename);
            goto out;
        }
        /* Compare the 8 bytes of OOB per page to the pattern passed in */
        oob_long = (uint64_t *)oob;
        for (uint64_t i = 0; i < nand_info->pages_per_block; i++) {
            if (!verify_pattern(cur, oob_long[i])) {
                printf("%s: OOB post-Erase failed at %lu ! found %lu\n",
                       __func__, cur, oob_long[i]);
                status = ZX_ERR_IO;
                goto out;
            }
        }
        if (filename != NULL) {
            /* Write the entire eraseblock out to the file */
            if (write(fd1, data, eraseblock_bytes) != eraseblock_bytes) {
                printf("%s: Could not write to %s\n",
                       __func__, filename);
                status = ZX_ERR_IO;
                goto out;
            }
        } else {
            if (erased((uint8_t *)data, (uint32_t)eraseblock_bytes) == false) {
                printf("%s: Did not erase block %lu properly\n",
                       __func__, cur);
            }
        }
        if (((cur - start_erase_block) % 40) == 0)
            printf("\n");
        printf(".");
        fflush(stdout);
    }
    printf("\n");
out:
    free(data);
    free(oob);
    if (filename != NULL)
        close(fd1);
    return status;
}

/*
 * After a block has been erased, OOB bytes should be all 0xff
 */
static bool verify_pattern_erased(uint64_t curblock, uint64_t oob_data)
{
    if (oob_data == ~((uint64_t)0))
        return true;
    return false;
}

/*
 * We write 8 OOB bytes for every page with the index of the *eraseblock*
 * this verifies the signature.
 */
static bool verify_pattern_written(uint64_t cur_erase_block,
                                   uint64_t oob_data)
{
    if (oob_data == cur_erase_block)
        return true;
    return false;
}

/*
 * Write out the data+oob for a single given page.
 */
static zx_status_t write_single_page_data_oob(int fd, nand_info_t *nand_info,
                                              uint32_t nand_page, char *data,
                                              char *oob)
{
    nandtest_rw_page_data_oob_t cmd;
    nandtest_resp_t resp_hdr;
    int rc;
    zx_status_t status;
    zx_handle_t vmo_data, vmo_oob;

    status = zx_vmo_create(nand_info->page_size,
                           0,
                           &vmo_data);
    if (status != ZX_OK) {
        printf("%s: vmo_create failed for data%d\n", __func__, status);
        return status;
    }
    status = zx_vmo_create(nand_info->oob_size,
                           0,
                           &vmo_oob);
    if (status != ZX_OK) {
        zx_handle_close(vmo_data);
        printf("%s: vmo_create failed for oob%d\n", __func__, status);
        return status;
    }
    if (data) {
        status = zx_vmo_write(vmo_data,
                              data,
                              0,
                              nand_info->page_size);
        if (status != ZX_OK) {
            printf("%s: vmo_write (data) failed (%d)\n", __func__,
                   status);
            zx_handle_close(vmo_data);
            zx_handle_close(vmo_oob);
            return status;
        }
    }
    if (oob) {
        status = zx_vmo_write(vmo_oob,
                              oob,
                              0,
                              nand_info->oob_size);
        if (status != ZX_OK) {
            printf("%s: vmo_write (oob) failed (%d)\n", __func__,
                   status);
            zx_handle_close(vmo_data);
            zx_handle_close(vmo_oob);
            return status;
        }
    }
    cmd.nand_page = nand_page;
    if (data)
        cmd.data_len = 1;
    else
        cmd.data_len = 0;
    if (oob)
        cmd.oob_len = nand_info->oob_size;
    else
        cmd.oob_len = 0;
    cmd.data = vmo_data;
    cmd.oob = vmo_oob;
    rc = fdio_ioctl(fd, IOCTL_NAND_WRITE_PAGE_DATA_OOB, &cmd, sizeof(cmd),
                    &resp_hdr, sizeof(resp_hdr));
    if (rc <= 0) {
        printf("%s: Got response %d from fdio_ioctl\n", __func__, rc);
        status = ZX_ERR_IO;
        goto out;
    }
    status = resp_hdr.status;
    if (resp_hdr.status != ZX_OK) {
        printf("%s: Got ZX error from PAGE write %d\n", __func__,
               resp_hdr.status);
        status = resp_hdr.status;
    }
out:
    if (data)
        zx_handle_close(vmo_data);
    if (oob)
        zx_handle_close(vmo_oob);
    return status;
}

/*
 * building block used by write_range() below. this function reds the data and the oob
 * for a range of pages
 */
static zx_status_t write_range_data_oob(int fd, nand_info_t *nand_info,
                                        uint32_t start_page, uint32_t numpages,
                                        char *data, char *oob)
{
    for (uint32_t i = 0; i < numpages; i++) {
        char *oob_next;
        char *data_next;

        if (oob)
            oob_next = oob + i * nand_info->oob_size;
        else
            oob_next = NULL;
        if (data)
            data_next = data + i * nand_info->page_size;
        else
            data_next = NULL;
        zx_status_t status;
        status = write_single_page_data_oob(fd, nand_info,
                                            start_page + i,
                                            data_next,
                                            oob_next);
        if (status != ZX_OK) {
            printf("%s: Write failed at @%u\n",
                   __func__, start_page + i);
            return status;
        }
    }
    return ZX_OK;
}

/*
 * Write one eraseblock at a time, with a known pattern for both data and
 * OOB.
 */
static zx_status_t write_eraseblock_range(int fd, nand_info_t *nand_info,
                                          const char *filename,
                                          uint64_t start_erase_block,
                                          uint64_t end_erase_block)
{
    int fd1;
    char *data, *oob;
    uint64_t *oob_long;
    zx_status_t status = ZX_OK;
    ssize_t eraseblock_bytes = nand_info->pages_per_block * nand_info->page_size;

    fd1 = open(filename, 0);
    if (fd1 < 0) {
        printf("%s: Could not open file %s\n",
               __func__, filename);
        return ZX_ERR_IO;
    }
    data = malloc(eraseblock_bytes);
    if (data == NULL) {
        printf("%s: Could not malloc data\n",
               __func__);
        exit(1);
    }
    oob = malloc(nand_info->pages_per_block * nand_info->oob_size);
    if (oob == NULL) {
        printf("%s: Could not malloc oob\n",
               __func__);
        exit(1);
    }
    for (uint64_t cur = start_erase_block; cur < end_erase_block; cur++) {
        if (is_block_bad(cur))
            continue;
        if (read(fd1, data, eraseblock_bytes) != eraseblock_bytes) {
            printf("%s: Could not read file %s\n",
                   __func__, filename);
            status = ZX_ERR_IO;
            goto out;
        }
        oob_long = (uint64_t *)oob;
        for (uint64_t i = 0; i < nand_info->pages_per_block; i++)
            oob_long[i] = cur; /* All 8 oob bytes for every page have same signature */
        status = write_range_data_oob(fd, nand_info, cur * nand_info->pages_per_block,
                                      nand_info->pages_per_block, data, oob);
        if (status != ZX_OK) {
            printf("%s: Count not write block @%lu\n",
                   __func__, cur);
            goto out;
        }
        if (((cur - start_erase_block) % 40) == 0)
            printf("\n");
        printf(".");
    }
    printf("\n");
out:
    free(data);
    free(oob);
    close(fd1);
    return status;
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
void erase_write_test(int fd, nand_info_t *nand_info,
                      uint64_t start_byte,
                      uint64_t end_byte)
{
    uint64_t start_erase_block, end_erase_block;
    uint64_t num_erase_blocks;
    zx_status_t status;
    uint64_t erasesize = nand_info->pages_per_block * nand_info->page_size;

    /*
     * start_byte and end_byte must be erase block aligned.
     * If they are not, make them so.
     */
    if (start_byte % erasesize)
        start_byte += erasesize - (start_byte % erasesize);
    if (end_byte % erasesize)
        end_byte -= (end_byte % erasesize);
    start_erase_block = start_byte / erasesize;
    end_erase_block = end_byte / erasesize;
    printf("%s: start erase block = %lu\n",
           __func__, start_erase_block);
    printf("%s: end erase block = %lu\n",
           __func__, end_erase_block);
    /*
     * How many bad blocks are in the range of erase blocks we
     * are going to erase + write to ?
     */
    num_erase_blocks = end_erase_block - start_erase_block;
    for (uint64_t cur = start_erase_block; cur < end_erase_block; cur++)
        if (is_block_bad(cur)) {
            printf("%s: Skipping erase block %lu\n",
                   __func__, cur);
            num_erase_blocks--;
        }
    printf("%s: num erase block = %lu\n",
           __func__, num_erase_blocks);
    /*
     * Create a file with known data for the size we are going
     * to write out.
     */
    status = create_data_file(num_erase_blocks * erasesize);
    if (status != ZX_OK) {
        printf("%s: Count not create data file \n",
               __func__);
        return;
    }
    /*
     * Next erase every block in the range (skipping over known bad
     * blocks).
     */
    printf("%s: Erasing blocks between %lu -> %lu\n",
           __func__, start_erase_block, end_erase_block);
    status = erase_blocks_range(fd, nand_info, start_erase_block, end_erase_block);
    if (status != ZX_OK)
        return;
    /*
     * At this point, the data in the range should be all 0xff !
     * Let's read the data and save it away (so we can verify).
     */
    printf("%s: Reading and Validating erased blocks between %lu -> %lu\n",
           __func__, start_erase_block, end_erase_block);
    status = read_eraseblock_range(fd, nand_info, NULL,
                                   start_erase_block, end_erase_block,
                                   verify_pattern_erased);
    if (status != ZX_OK) {
        printf("%s: Reading blocks before write (after erase) failed\n",
               __func__);
        return;
    }
    /*
     * Write blocks in the range reading data from the given file.
     */
    printf("%s: Writing blocks between %lu -> %lu\n",
           __func__, start_erase_block, end_erase_block);
    status = write_eraseblock_range(fd, nand_info, "/tmp/nand-data-to-write",
                                    start_erase_block, end_erase_block);
    if (status != ZX_OK) {
        printf("%s: Writing of blocks failed\n",
               __func__);
        return;
    }
    /*
     * At this point, the data in the range has been completely written
     * Let's read the data and save it away (so we can verify).
     */
    printf("%s: Reading and Validating written blocks between %lu -> %lu\n",
           __func__, start_erase_block, end_erase_block);
    status = read_eraseblock_range(fd, nand_info, "/tmp/nand-dump-after-write",
                                   start_erase_block, end_erase_block,
                                   verify_pattern_written);
    if (status != ZX_OK)
        printf("%s: Reading blocks after write failed\n",
               __func__);
    if (compare_files("/tmp/nand-dump-after-write",
                      "/tmp/nand-data-to-write") == false) {
        printf("%s: Data written does not match what we expect, failed writes ?\n",
               __func__);
    } else {
        printf("%s: Successful test, validated writes and erases for the entire range\n",
               __func__);
    }
}

typedef struct _nand_cmd {
    u_int8_t type;
    u_int8_t val;
} nand_cmd_t;

typedef struct nand_setup {
    union {
	uint32_t d32;
	struct {
	    unsigned cmd:22;
	    unsigned large_page:1;  // 22
	    unsigned no_rb:1;	    // 23 from efuse
	    unsigned a2:1;	    // 24
	    unsigned reserved25:1;  // 25
	    unsigned page_list:1;   // 26
	    unsigned sync_mode:2;   // 27 from efuse
	    unsigned size:2;        // 29 from efuse
	    unsigned active:1;	    // 31
	} b;
    } cfg;
    uint16_t id;
    uint16_t max;    // id:0x100 user, max:0 disable.
} nand_setup_t;

typedef struct _ext_info {
    uint32_t read_info;   	//nand_read_info;
    uint32_t new_type;    	//new_nand_type;
    uint32_t page_per_blk;   	//pages_in_block;
    uint32_t xlc;		//slc=1, mlc=2, tlc=3.
    uint32_t ce_mask;
    /* copact mode: boot means whole uboot
     * it's easy to understood that copies of
     * bl2 and fip are the same.
     * discrete mode, boot means the fip only
     */
    uint32_t boot_num;
    uint32_t each_boot_pages;
    /* for comptible reason */
    uint32_t bbt_occupy_pages;
    uint32_t bbt_start_block;
} ext_info_t;

typedef struct _nand_page0 {
    nand_setup_t nand_setup;		//8
    unsigned char page_list[16];	//16
    nand_cmd_t retry_usr[32];		//64 (32 cmd max I/F)
    ext_info_t ext_info;		//64
} nand_page0_t;				//384 bytes max.

/*
 * Gets nand_info from nand device, this information is needed by all tests.
 */
static zx_status_t get_nand_info(int fd, nand_info_t *nand_info)
{
    uint8_t resp_buf[8192];
    int rc;
    nandtest_resp_t *resp_hdr;

    rc = fdio_ioctl(fd, IOCTL_NAND_GET_NAND_INFO, NULL, 0,
                    resp_buf, 8192);
    if (rc <= 0) {
        printf("%s: Got response %d from fdio_ioctl\n", __func__, rc);
        return ZX_ERR_IO;
    }
    resp_hdr = (nandtest_resp_t *)resp_buf;
    if (resp_hdr->status != ZX_OK) {
        printf("%s: Got ZX error from GET_NAND_INFO %d\n", __func__,
               resp_hdr->status);
        return resp_hdr->status;
    }
    memcpy(nand_info, resp_buf + sizeof(nandtest_resp_t),
           sizeof(nand_info_t));
    return ZX_OK;
}

/*
 * Read the given page0 page. helper function that allows the called to loop
 * over all of the page0 pages, and read all of them.
 */
static zx_status_t read_one_page0_data(int fd, uint32_t nand_page,
                                       nand_info_t *nand_info)
{
    nand_page0_t *page0;
    zx_status_t status;
    char *data, *oob;

    data = malloc(nand_info->page_size);
    if (data == NULL) {
        printf("%s: Could not malloc data\n",
               __func__);
        exit(1);
    }
    oob = malloc(nand_info->oob_size);
    if (oob == NULL) {
        printf("%s: Could not malloc oob\n",
               __func__);
        exit(1);
    }
    status = read_single_page_data_oob(fd, nand_info,
                                       nand_page, data,
                                       oob);
    if (status != ZX_OK) {
        free(data);
        free(oob);
        printf("%s: read_single_page_data_oob() returned %d\n",
               __func__, status);
        return status;
    }

    page0 = (nand_page0_t *)data;
    printf("cfg.d32 = 0x%x\n", page0->nand_setup.cfg.d32);
    uint32_t val = page0->nand_setup.cfg.d32 & 0x3f;
    printf("ecc_step = %u\n", val);
    val = (page0->nand_setup.cfg.d32 >> 6) & 0x7f;
    printf("pagesize = %u\n", val);

    printf("ext_info.read_info = 0x%x\n", page0->ext_info.read_info);
    printf("ext_info.page_per_blk = 0x%x\n", page0->ext_info.page_per_blk);
    printf("ext_info.boot_num = 0x%x\n", page0->ext_info.boot_num);
    printf("ext_info.each_boot_pages = 0x%x\n", page0->ext_info.each_boot_pages);
    printf("ext_info.bbt_occupy_pages = 0x%x\n", page0->ext_info.bbt_occupy_pages);
    printf("ext_info.bbt_start_block = 0x%x\n", page0->ext_info.bbt_start_block);

    printf("OOB bytes = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
           oob[0], oob[1], oob[2], oob[3], oob[4], oob[5], oob[6], oob[7]);

    if (bbt_start_block == 0)
        bbt_start_block = page0->ext_info.bbt_start_block;

    free(data);
    free(oob);
    return ZX_OK;
}

/*
 * Read and dump all of the page0 pages.
 */
static zx_status_t read_all_page0(int fd, nand_info_t *nand_info)
{
    for (uint32_t i = 0; i < 7; i++) {
        printf("Reading Page0 data at %u:\n", 128 * i);
        zx_status_t status;
        status = read_one_page0_data(fd, 128 * i, nand_info);
        if (status != ZX_OK) {
            printf("nandtest: Read of Page0 failed@%u %d\n", 128 * i, status);
            return status;
        }
    }
    return ZX_OK;
}

static zx_status_t read_pages_data(int fd, uint32_t nand_page, uint32_t numpages,
                                   nand_info_t *nand_info, void *buf)
{
    return read_range_data_oob(fd, nand_info, nand_page, numpages,
                               buf, NULL);
}

/*
 * Test that reads the bbt pages at the fixed bbt offset and dumps out
 * the bad blocks. This also initializes the bbt table.
 */
static zx_status_t read_bbt(int fd, nand_info_t *nand_info)
{
    int fd1;
    char filename[512];
    char *buf;
    /* In bbt, 1 byte per eraseblock to store state */
    uint32_t bbt_size_in_pages = nand_info->num_blocks / nand_info->page_size;

    if (bbt_start_block == 0) {
        printf("%s: bbt_start_block uninitialized, have page0 reads happened ?\n",
               __func__);
        printf("%s: FATAL ERROR: Cannot run nandtest\n",
               __func__);
        exit(1);
    }
    if (bbt_size_in_pages == 0)
        bbt_size_in_pages = 1;
    printf("%s: bbt_size_in_pages = %u\n", __func__, bbt_size_in_pages);
    buf = malloc(bbt_size_in_pages * nand_info->page_size);
    if (buf == NULL) {
        printf("%s: Could not malloc buf\n",
               __func__);
        exit(1);
    }
    sprintf(filename, "/tmp/bbt");
    fd1 = open(filename, O_CREAT | O_WRONLY);
    if (fd1 < 0) {
	printf("%s: Could not open file /tmp/nand-dump\n",
	       __func__);
        free(buf);
	return ZX_ERR_IO;
    }
    zx_status_t status;
    status = read_pages_data(fd,
                             bbt_start_block * nand_info->pages_per_block,
                             bbt_size_in_pages, nand_info, buf);
    if (status != ZX_OK) {
        free(buf);
	return status;
    }
    if (write(fd1, buf, bbt_size_in_pages * nand_info->page_size) !=
        bbt_size_in_pages * nand_info->page_size) {
	printf("%s: Could not write to file %s\n", __func__, filename);
        free(buf);
        close(fd1);
        return ZX_ERR_IO;
    }
    fsync(fd1);
    close(fd1);
    fd1 = open(filename, O_RDONLY);
    if (fd1 < 0) {
	printf("%s: Could not open file /tmp/nand-dump\n",
	       __func__);
        free(buf);
	return ZX_ERR_IO;
    }
    if (read(fd1, buf, bbt_size_in_pages * nand_info->page_size) !=
        bbt_size_in_pages * nand_info->page_size) {
	printf("%s: Could not read file %s\n", __func__, filename);
        free(buf);
        close(fd1);
	return ZX_ERR_IO;
    }
    char *p = buf;
    for (uint32_t i = 0;
         i < bbt_size_in_pages * nand_info->page_size;
         i++) {
        if (*p++ != 0) {
            printf("%s: Bad block at %u\n", __func__, i);
            bad_blocks[num_bad_blocks] = true;
        } else {
            bad_blocks[num_bad_blocks] = false;
        }
        num_bad_blocks++;
    }
    close(fd1);
    free(buf);
    return ZX_OK;
}

static bool erased(uint8_t*data, uint32_t len)
{
    uint32_t i;

    for (i = 0; i < len; i++)
        if (data[i] != 0xff)
            return false;
    return true;
}

/*
 * test: Erase a block, read it back and make sure entire block has been
 * erased. The function finds the last block we can erase (that is not in the
 * bad block table) and erases that block.
 */
zx_status_t erasetest_one_block(int fd, nand_info_t *nand_info,
                                uint32_t *block_erased)
{
    uint32_t cur_nand_page;
    zx_status_t status = ZX_ERR_IO;
    int fd1;
    uint32_t nand_block;
    char *data, *oob;

    /* Let's pick the very last erase block */
    nand_block = nand_info->num_blocks - 1;
    while (nand_block > 0) {
        if (!is_block_bad(nand_block)) {
            printf("%s: About to erase block @%u\n",
                   __func__, nand_block);
            break;
        }
        nand_block--;
    }
    if (nand_block == 0) {
        printf("%s: Could not find a block to erase\n",
               __func__);
        return ZX_ERR_IO;
    }
    data = malloc(nand_info->page_size);
    if (data == NULL) {
        printf("%s: Could not malloc data\n",
               __func__);
        exit(1);
    }
    oob = malloc(nand_info->oob_size);
    if (oob == NULL) {
        printf("%s: Could not malloc oob\n",
               __func__);
        exit(1);
    }
    /*
     * Read existing data and dump that out to a file
     */
    fd1 = open("/tmp/nand-dump-before-erase", O_CREAT | O_WRONLY);
    if (fd1 < 0) {
        printf("%s: Could not open file /tmp/nand-dump-before-erase\n",
               __func__);
        goto out;
    }
    cur_nand_page = nand_block * nand_info->pages_per_block;
    for (uint32_t i = 0 ; i < nand_info->pages_per_block ; i++) {
        status = read_pages_data(fd, cur_nand_page + i, 1, nand_info, data);
        if (status != ZX_OK) {
            printf("%s: Read failed at %u\n",
                   __func__, cur_nand_page + i);
            goto out_close_free;
        }
        if (write(fd1, data, nand_info->page_size) != nand_info->page_size) {
            printf("%s: Could not write to file /tmp/nand-dump-before-erase\n",
                   __func__);
        }
    }
    fsync(fd1);
    close(fd1);
     printf("%s: About to erase block %u\n", __func__, nand_block);
    /*
     * Do the erase operation, then re-read the pages, storing it in "after".
     */
    status = erase_blocks_range(fd, nand_info, nand_block, nand_block + 1);
    if (status != ZX_OK) {
        printf("%s: Erase block failed %u\n",
               __func__, nand_block);
        goto out;
    }
    fd1 = open("/tmp/nand-dump-after-erase", O_CREAT | O_WRONLY);
    if (fd < 0) {
        printf("%s: Could not open file /tmp/nand-dump-after-erase\n",
               __func__);
        goto out;
    }
    for (uint32_t i = 0 ; i < nand_info->pages_per_block ; i++) {
        status = read_single_page_data_oob(fd, nand_info,
                                           cur_nand_page + i, data, oob);
        if (status != ZX_OK) {
            printf("%s: Read failed at %u\n",
                   __func__, cur_nand_page + i);
            goto out_close_free;
        }
        if ((erased((uint8_t *)data, nand_info->page_size) == false) ||
            (erased((uint8_t *)oob, nand_info->oob_size) == false)) {
            printf("%s: Bad data/oob, post-erase at page %u\n",
                   __func__, cur_nand_page + i);
        }
        if (write(fd1, data, nand_info->page_size) != nand_info->page_size) {
            printf("%s: Could not write to file /tmp/nand-dump-after-erase\n",
                   __func__);
        }
    }
    fsync(fd1);
    status = ZX_OK;
    *block_erased = nand_block;

out_close_free:
    close(fd1);
out:
    free(data);
    free(oob);
    return status;
}

/*
 * test: Write the given page and read back the data before and after the
 * write. It is assumed the page to be written has already been erased.
 * The page passed in must be eraseblock aligned.
 * The function snapshots the contents before and after the write.
 */
void writetest_one_eraseblock(int fd, nand_info_t *nand_info, uint32_t nand_block)
{
    zx_status_t status;
    char *data, *oob;
    int fd1;
    uint32_t cur_nand_page;
    uint64_t erasesize = nand_info->pages_per_block * nand_info->page_size;

    if (is_block_bad(nand_block)) {
        printf("%s: Cannot write to @%u - part of bad block\n",
               __func__, nand_block);
        return;
    }
    data = malloc(nand_info->page_size);
    if (data == NULL) {
        printf("%s: Could not malloc data\n",
               __func__);
        exit(1);
    }
    oob = malloc(nand_info->oob_size);
    if (oob == NULL) {
        printf("%s: Could not malloc oob\n",
               __func__);
        exit(1);
    }
    /*
     * Read existing data and dump that out to a file
     */
    fd1 = open("/tmp/nand-dump-before-write", O_CREAT | O_WRONLY);
    if (fd1 < 0) {
        printf("%s: Could not open file /tmp/nand-dump-before-write\n",
               __func__);
        goto out;
    }
    cur_nand_page = nand_block * nand_info->pages_per_block;
    for (uint32_t i = 0 ; i < nand_info->pages_per_block ; i++) {
        status = read_single_page_data_oob(fd, nand_info, cur_nand_page + i,
                                           data, oob);
        if (status != ZX_OK) {
            printf("%s: Read failed at %u\n",
                   __func__, cur_nand_page + i);
            goto out_close_free;
        }
        if ((erased((uint8_t *)data, nand_info->page_size) == false) ||
            (erased((uint8_t *)oob, nand_info->oob_size) == false)) {
            printf("%s: Bad data/oob, post-erase at page %u\n",
                   __func__, cur_nand_page + i);
            printf("%s: Cannot write to block %u, erase it first\n",
                   __func__, nand_block);
            goto out_close_free;
        }
        if (write(fd1, data, nand_info->page_size) != nand_info->page_size) {
            printf("%s: Could not write to file /tmp/nand-dump-before-write\n",
                   __func__);
        }
    }
    fsync(fd1);
    close(fd1);
    if (create_data_file(erasesize) != ZX_OK) {
        printf("%s: Could not create data file /tmp/nand-data-to-write\n",
               __func__);
        goto out;
    }
    /*
     * Do the program operation, then re-read the pages, storing it in "after".
     */
    fd1 = open("/tmp/nand-data-to-write", 0);
    if (fd1 < 0) {
        printf("%s: Could not open file /tmp/nand-data-to-write\n",
               __func__);
        goto out;
    }
    for (uint32_t i = 0 ; i < nand_info->pages_per_block ; i++) {
        if (read(fd1, data, nand_info->page_size) != nand_info->page_size) {
            printf("%s: Could not read file /tmp/nand-data-to-write\n",
                   __func__);
        }
        *(uint64_t *)oob = (cur_nand_page + i);
        status = write_single_page_data_oob(fd, nand_info, cur_nand_page + i, data, oob);
        if (status != ZX_OK) {
            printf("%s: Write failed to page %u\n",
                   __func__, cur_nand_page + i);
            goto out_close_free;
        }
    }
    close(fd1);
    fd1 = open("/tmp/nand-dump-after-write", O_CREAT | O_WRONLY);
    if (fd1 < 0) {
        printf("%s: Could not open file /tmp/nand-dump-after-write\n",
               __func__);
        goto out;
    }
    for (uint32_t i = 0 ; i < nand_info->pages_per_block ; i++) {
        status = read_single_page_data_oob(fd, nand_info, cur_nand_page + i,
                                           data, oob);
        if (status != ZX_OK) {
            printf("%s: Read failed at %u\n",
                   __func__, cur_nand_page + i);
            goto out_close_free;
        }
        if (write(fd1, data, nand_info->page_size) != nand_info->page_size) {
            printf("%s: Could not write to file /tmp/nand-dump-after-write\n",
                   __func__);
        }
        if (*(uint64_t *)oob != (cur_nand_page + i)) {
            printf("%s: Bad write at %u OOB is %lu, must be %lu\n",
                   __func__, cur_nand_page + i, *(uint64_t *)oob,
                   (uint64_t)(cur_nand_page + i));
        }
    }
    fsync(fd1);
    close(fd1);
    free(data);
    free(oob);
    if (compare_files("/tmp/nand-dump-after-write",
                      "/tmp/nand-data-to-write") == false) {
        printf("%s: Written data differs from what we wrote - bad Write\n",
               __func__);
    }
    printf("Successfully validated (erase), wrote and validated write for erase block %u\n",
           nand_block);
    return;

out_close_free:
    close(fd1);
out:
    free(data);
    free(oob);
}

static void test_init(int *fd, nand_info_t *nand_info)
{
    *fd = open("/dev/class/nand/000", 0);
    if (*fd < 0) {
        printf("nandtest: Could not open file /dev/class/nand/000 error = %d\n",
               errno);
        exit(1);
    }
    zx_status_t status;
    status = get_nand_info(*fd, nand_info);
    if (status != ZX_OK) {
        printf("nandtest: Could not GET NAND INFO %d\n",
               status);
        close(*fd);
        exit(1);
    }
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
void range_erase_write_test(int fd, nand_info_t *nand_info,
                            uint64_t start_byte,
                            uint64_t end_byte)
{
    uint64_t start_erase_block, end_erase_block;
    uint64_t num_erase_blocks;
    zx_status_t status;
    uint64_t erasesize = nand_info->pages_per_block * nand_info->page_size;

    /*
     * start_byte and end_byte must be erase block aligned.
     * If they are not, make them so.
     */
    if (start_byte % erasesize)
        start_byte += erasesize - (start_byte % erasesize);
    if (end_byte % erasesize)
        end_byte -= (end_byte % erasesize);
    start_erase_block = start_byte / erasesize;
    end_erase_block = end_byte / erasesize;
    printf("%s: start erase block = %lu\n",
           __func__, start_erase_block);
    printf("%s: end erase block = %lu\n",
           __func__, end_erase_block);
    /*
     * How many bad blocks are in the range of erase blocks we
     * are going to erase + write to ?
     */
    num_erase_blocks = end_erase_block - start_erase_block;
    for (uint64_t cur = start_erase_block; cur < end_erase_block; cur++)
        if (is_block_bad(cur)) {
            printf("%s: Skipping erase block %lu\n",
                   __func__, cur);
            num_erase_blocks--;
        }
    printf("%s: num erase block = %lu\n",
           __func__, num_erase_blocks);
    /*
     * Create a file with known data for the size we are going
     * to write out.
     */
    status = create_data_file(num_erase_blocks * erasesize);
    if (status != ZX_OK) {
        printf("%s: Count not create data file \n",
               __func__);
        return;
    }
    /*
     * Next erase every block in the range (skipping over known bad
     * blocks).
     */
    status = erase_blocks_range(fd, nand_info,
                                start_erase_block, end_erase_block);
    if (status != ZX_OK)
        return;

    /*
     * At this point, the data in the range should be all 0xff !
     * Let's read the data and save it away (so we can verify).
     */
    status = read_eraseblock_range(fd, nand_info, "/tmp/nand-dump-before-write",
                                   start_erase_block, end_erase_block,
                                   verify_pattern_erased);
    if (status != ZX_OK) {
        printf("%s: Reading blocks before write (after erase) failed\n",
               __func__);
        return;
    }
    /*
     * Write blocks in the range reading data from the given file.
     */
    status = write_eraseblock_range(fd, nand_info, "/tmp/nand-data-to-write",
                                    start_erase_block, end_erase_block);
    if (status != ZX_OK) {
        printf("%s: Writing of blocks failed\n",
               __func__);
        return;
    }
    /*
     * At this point, the data in the range has been completely written
     * Let's read the data and save it away (so we can verify).
     */
    status = read_eraseblock_range(fd, nand_info, "/tmp/nand-dump-after-write",
                                   start_erase_block, end_erase_block,
                                   verify_pattern_written);
    if (status != ZX_OK)
        printf("%s: Reading blocks after write failed\n",
               __func__);
}

static void usage(char *pname)
{
    printf("usage: %s -b | -r\n", pname);
    printf("Where -b erases and writes one block, -r erases and writes a range of blocks\n");
    printf("-r requires 2 start-of-range and length-of-range (in bytes) to be passed in as integer arguments\n");
    printf("Example: B : nandtest -b -r 281018368 536870912\n");
    printf("Example: E : nandtest -b -r 288358400 536870912\n");
    exit(1);
}

int options = 0;
bool erase_write_one_block = false;
bool erase_write_range = false;

/*
 * For B :
 * 0x000010c00000 -> 0x000020000000 (281018368 -> 536870912)
 * are the byte ranges for the /cache partition. This is a good
 * range to test the erase+write on.
 * For E :
 * 0x000011300000 -> 0x000020000000 (288358400 - 536870912)
 * are the byte ranges for the /cache partition. This is a good
 * range to test the erase+write on.
 */

int main(int argc, char** argv) {
    int fd;
    zx_status_t status;
    nand_info_t nand_info;
    int c;
    extern int optind;
    uint64_t start_range = 0, length = 0;
    char *pname;

    pname = argv[0];
    while ((c = getopt(argc, argv, "br")) != -1) {
        switch (c) {
        case 'b':
            erase_write_one_block = true;
            options++;
            break;
        case 'r':
            erase_write_range = true;
            options++;
            break;
        default:
            usage(pname);
        }
    }

    if (options == 0)
        usage(pname);

    if (erase_write_range) {
        if (optind == argc)
            usage(pname);
	start_range = atoll(argv[optind++]);
        if (optind == argc)
            usage(pname);
	length = atoll(argv[optind]);
        printf("Erasing and Writing Range starting at %lu (bytes), length (%lu) bytes\n",
               start_range, length);
    }

    test_init(&fd, &nand_info);
    status = read_all_page0(fd, &nand_info);
    if (status != ZX_OK) {
        printf("nandtest: Page0 read test failure %d\n",
               status);
        close(fd);
        exit(1);
    }

    printf("nandtest: Dumping BBT Table\n");
    status = read_bbt(fd, &nand_info);
    if (status != ZX_OK) {
        printf("nandtest: bbt read test failure %d\n",
               status);
        close(fd);
        exit(1);
    }

    if (erase_write_one_block) {
        uint32_t nand_block;

        printf("Erasing and Writing the Last possible block\n");
        if (erasetest_one_block(fd, &nand_info, &nand_block) == ZX_OK) {
            printf("Successfully erased block %u, proceeding to write\n",
                   nand_block);
            writetest_one_eraseblock(fd, &nand_info, nand_block);
        }
    }

    if (erase_write_range) {
        printf("Erasing and Writing Range starting at %lu (bytes), length (%lu) bytes\n",
               start_range, length);
        erase_write_test(fd, &nand_info, start_range, length);
    }
}

