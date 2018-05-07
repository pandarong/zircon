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

static const uint32_t chipsel[2] = {NAND_CE0, NAND_CE1};

struct aml_controller_params aml_params = {
    8,
    2,
    /* The 2 following values are overwritten by page0 contents */
    1,                  /* rand-mode is 1 for page0 */
    AML_ECC_BCH60_1K,   /* This is the BCH setting for page0 */
};

uint32_t
aml_get_ecc_pagesize(aml_rawnand_t *rawnand, uint32_t ecc_mode)
{
    uint32_t ecc_page;
    
    switch (ecc_mode) {
    case AML_ECC_BCH8:
        ecc_page = 512;
        break;
    case AML_ECC_BCH8_1K:        
        ecc_page = 1024;
        break;
    default:
        ecc_page = 0;
        break;
    }
    return ecc_page;
}

static void aml_cmd_idle(aml_rawnand_t *rawnand, uint32_t time)
{
    uint32_t cmd = 0;
    volatile uint8_t *reg = (volatile uint8_t*)
        io_buffer_virt(&rawnand->mmio[NANDREG_WINDOW]);

    cmd = rawnand->chip_select | AML_CMD_IDLE | (time & 0x3ff);
    writel(cmd, reg + P_NAND_CMD);
}

static zx_status_t aml_wait_cmd_finish(aml_rawnand_t *rawnand,
                                       unsigned int timeout_ms)
{
    uint32_t cmd_size = 0;
    zx_status_t ret = ZX_OK;
    uint64_t total_time = 0;
    uint32_t numcmds;
    volatile uint8_t *reg = (volatile uint8_t*)
        io_buffer_virt(&rawnand->mmio[NANDREG_WINDOW]);
    
    /* wait until cmd fifo is empty */
    while (true) {
        cmd_size = readl(reg + P_NAND_CMD);
        numcmds = (cmd_size >> 22) & 0x1f;
        if (numcmds == 0)
            break;
//        zxlogf(ERROR, "aml_wait_cmd_finish: numcmds =  %u\n",
//               numcmds);            
        usleep(10);
        total_time += 10;
        if (total_time > (timeout_ms * 1000)) {
            ret = ZX_ERR_TIMED_OUT;
            break;
        }
    }
//    zxlogf(ERROR, "aml_wait_cmd_finish: Waited for %lu usecs, numcmds = %d\n",
//           total_time, numcmds);    
    if (ret == ZX_ERR_TIMED_OUT)
        zxlogf(ERROR, "wait for empty cmd FIFO time out\n");
    return ret;
}

static void aml_cmd_seed(aml_rawnand_t *rawnand, uint32_t seed)
{       
    uint32_t cmd;
    volatile uint8_t *reg = (volatile uint8_t*)
        io_buffer_virt(&rawnand->mmio[NANDREG_WINDOW]);
    
    cmd = AML_CMD_SEED | (0xc2 + (seed & 0x7fff));
    writel(cmd, reg + P_NAND_CMD);
}

static void aml_cmd_n2m(aml_rawnand_t *rawnand, uint32_t ecc_pages,
                        uint32_t ecc_pagesize)
{
    uint32_t cmd;
    volatile uint8_t *reg = (volatile uint8_t*)
        io_buffer_virt(&rawnand->mmio[NANDREG_WINDOW]);

    cmd = CMDRWGEN(AML_CMD_N2M,
                   rawnand->controller_params.rand_mode,
                   rawnand->controller_params.bch_mode,
                   0,
		   ecc_pagesize,
                   ecc_pages);
    writel(cmd, reg + P_NAND_CMD);    
}

static void aml_cmd_m2n_page0(aml_rawnand_t *rawnand)
{
    /* TODO */
}

static void aml_cmd_m2n(aml_rawnand_t *rawnand, uint32_t ecc_pages,
                        uint32_t ecc_pagesize)
{
    uint32_t cmd;
    volatile uint8_t *reg = (volatile uint8_t*)
        io_buffer_virt(&rawnand->mmio[NANDREG_WINDOW]);

    cmd = CMDRWGEN(AML_CMD_M2N,
                   rawnand->controller_params.rand_mode,
                   rawnand->controller_params.bch_mode,
                   0, ecc_pagesize,
                   ecc_pages);
    writel(cmd, reg + P_NAND_CMD);    
}

static void aml_cmd_n2m_page0(aml_rawnand_t *rawnand)
{
    uint32_t cmd;
    volatile uint8_t *reg = (volatile uint8_t*)
        io_buffer_virt(&rawnand->mmio[NANDREG_WINDOW]);

    /*
     * For page0 reads, we must use AML_ECC_BCH60_1K,
     * and rand-mode == 1.
     */
    cmd = CMDRWGEN(AML_CMD_N2M,
                   1,                   /* force rand_mode */
                   AML_ECC_BCH60_1K,    /* force bch_mode  */
                   1,                   /* shortm == 1     */
                   384 >> 3,
                   1);
    writel(cmd, reg + P_NAND_CMD);    
}

static zx_status_t aml_wait_dma_finish(aml_rawnand_t *rawnand) 
{
    aml_cmd_idle(rawnand, 0);
    aml_cmd_idle(rawnand, 0);
    return aml_wait_cmd_finish(rawnand, DMA_BUSY_TIMEOUT);
}

/*
 * Return the aml_info_format struct corresponding to the i'th 
 * ECC page. THIS ASSUMES user_mode == 2 (2 OOB bytes per ECC page).
 */
static struct aml_info_format *aml_info_ptr(aml_rawnand_t *rawnand,
                                            int i)
{
    struct aml_info_format *p;

    p = (struct aml_info_format *)rawnand->info_buf;
    return &p[i];
}

/*
 * In the case where user_mode == 2, info_buf contains one nfc_info_format 
 * struct per ECC page on completion of a read. This 8 byte structure has 
 * the 2 OOB bytes and ECC/error status
 */
static zx_status_t aml_get_oob_byte(aml_rawnand_t *rawnand,
                                    uint8_t *oob_buf)
{
    struct aml_info_format *info;
    int count = 0;
    uint32_t i, ecc_pagesize, ecc_pages;

    ecc_pagesize = aml_get_ecc_pagesize(rawnand, AML_ECC_BCH8);
    ecc_pages = rawnand->writesize / ecc_pagesize;
    /*
     * user_mode is 2 in our case - 2 bytes of OOB for every
     * ECC page.
     */
    if (rawnand->controller_params.user_mode != 2)
        return ZX_ERR_NOT_SUPPORTED;
    for (i = 0;
         i < ecc_pages;
         i++) {
        info = aml_info_ptr(rawnand, i);
        oob_buf[count++] = info->info_bytes & 0xff;
        oob_buf[count++] = (info->info_bytes >> 8) & 0xff;
    }
    return ZX_OK;
}

static zx_status_t aml_set_oob_byte(aml_rawnand_t *rawnand,
                                    uint8_t *oob_buf,
                                    uint32_t ecc_pages)
{
    struct aml_info_format *info;
    int count = 0;
    uint32_t i;

    /*
     * user_mode is 2 in our case - 2 bytes of OOB for every
     * ECC page.
     */
    if (rawnand->controller_params.user_mode != 2)
        return ZX_ERR_NOT_SUPPORTED;
    for (i = 0; i < ecc_pages; i++) {
        info = aml_info_ptr(rawnand, i);
        info->info_bytes = oob_buf[count] | (oob_buf[count + 1] << 8);
        count += 2;
    }
    return ZX_OK;
}

/*
 * Returns the maximum bitflips corrected on this NAND page
 * (the maximum bitflips across all of the ECC pages in this page).
 */
static int aml_get_ecc_corrections(aml_rawnand_t *rawnand, int ecc_pages)
{
    struct aml_info_format *info;
    int bitflips = 0, i;
    uint8_t zero_cnt;

    for (i = 0; i < ecc_pages; i++) {
        info = aml_info_ptr(rawnand, i);
        if (info->ecc.eccerr_cnt == 0x3f) {
            /*
             * Why are we checking for zero_cnt here ?
             * Per Amlogic HW architect, this is to deal with 
             * blank NAND pages. The entire blank page is 0xff.
             * When read with scrambler, the page will be ECC
             * uncorrectable, but if the total of zeroes in this
             * page is less than a threshold, then we know this is
             * blank page.
             */
            zero_cnt = info->zero_cnt & 0x3f;
#if 0
            zxlogf(ERROR, "%s: zero_cnt = %x\n",
                   __func__, zero_cnt);
#endif
            if (rawnand->controller_params.rand_mode &&
                (zero_cnt < rawnand->controller_params.ecc_strength)) {
                zxlogf(ERROR, "%s: Returning ECC failure\n",
                       __func__);                    
                return ECC_CHECK_RETURN_FF;
            }
            rawnand->stats.failed++;
            continue;
        }
        rawnand->stats.ecc_corrected += info->ecc.eccerr_cnt;
        bitflips = MAX(bitflips, info->ecc.eccerr_cnt);
    }
    return bitflips;
}

static zx_status_t aml_check_ecc_pages(aml_rawnand_t *rawnand, int ecc_pages)
{
    struct aml_info_format *info;
    int i;

    for (i = 0; i < ecc_pages; i++) {
        info = aml_info_ptr(rawnand, i);
        if (info->ecc.completed == 0) {
#if 1
            zxlogf(ERROR, "%s: READ not completed/valid info_bytes = %x zero_cnt = %x ecc = %x\n",
                   __func__, info->info_bytes, info->zero_cnt, *(uint8_t *)&info->ecc);
#endif
            return ZX_ERR_IO;
        }
    }
    return ZX_OK;
}

static zx_status_t aml_queue_rb(aml_rawnand_t *rawnand)
{
    uint32_t cmd, cfg;
    zx_status_t status;
    volatile uint8_t *reg = (volatile uint8_t*)
        io_buffer_virt(&rawnand->mmio[NANDREG_WINDOW]);
    
    rawnand->req_completion = COMPLETION_INIT;    
    cfg = readl(reg + P_NAND_CFG);
    cfg |= (1 << 21);
    writel(cfg, reg + P_NAND_CFG);
    aml_cmd_idle(rawnand, NAND_TWB_TIME_CYCLE);
    cmd = rawnand->chip_select | AML_CMD_CLE | (NAND_CMD_STATUS & 0xff);
    writel(cmd, reg + P_NAND_CMD);
    aml_cmd_idle(rawnand, NAND_TWB_TIME_CYCLE);
    cmd = AML_CMD_RB | AML_CMD_IO6 | (1 << 16) | (0x18 & 0x1f);
    writel(cmd, reg + P_NAND_CMD);
    aml_cmd_idle(rawnand, 2);
    status = completion_wait(&rawnand->req_completion,
                             ZX_SEC(1));
    if (status == ZX_ERR_TIMED_OUT) {
        zxlogf(ERROR, "%s: Request timed out, not woken up from irq\n",
               __func__);
    }
    return status;
}

/*
 * This function just stuffs the parameters needed into the 
 * rawnand controller struct, so they can be retrieved later.
 */
static void aml_select_chip(aml_rawnand_t *rawnand, int chip)
{
    rawnand->chip_select = chipsel[chip];
}

void aml_cmd_ctrl(aml_rawnand_t *rawnand,
                  int cmd, unsigned int ctrl)
{
    volatile uint8_t *reg = (volatile uint8_t*)
        io_buffer_virt(&rawnand->mmio[NANDREG_WINDOW]);
    
    if (cmd == NAND_CMD_NONE)
        return;
    if (ctrl & NAND_CLE)
        cmd = rawnand->chip_select | AML_CMD_CLE | (cmd & 0xff);
    else
        cmd = rawnand->chip_select | AML_CMD_ALE | (cmd & 0xff);
    writel(cmd, reg + P_NAND_CMD);
}

/* Read statys byte */
uint8_t aml_read_byte(aml_rawnand_t *rawnand)
{
    uint32_t cmd;
    volatile uint8_t *reg = (volatile uint8_t*)
        io_buffer_virt(&rawnand->mmio[NANDREG_WINDOW]);
    
    cmd = rawnand->chip_select | AML_CMD_DRD | 0;
    nandctrl_send_cmd(rawnand, cmd);

    aml_cmd_idle(rawnand, NAND_TWB_TIME_CYCLE);
    
    aml_cmd_idle(rawnand, 0);
    aml_cmd_idle(rawnand, 0);
    aml_wait_cmd_finish(rawnand, 1000);
    return readb(reg + P_NAND_BUF);
}

static void aml_set_clock_rate(aml_rawnand_t* rawnand,
                               uint32_t clk_freq)
{
    uint32_t always_on = 0x1 << 24;
    uint32_t clk;
    volatile uint8_t *reg = (volatile uint8_t*)
        io_buffer_virt(&rawnand->mmio[CLOCKREG_WINDOW]);
    
    /* For Amlogic type  AXG */
    always_on = 0x1 << 28;
    switch (clk_freq) {
    case 24:
        clk = 0x80000201;
        break;
    case 112:
        clk = 0x80000249;
        break;
    case 200:
        clk = 0x80000245;
        break;
    case 250:
        clk = 0x80000244;
        break;
    default:
        clk = 0x80000245;
        break;
    } 
    clk |= always_on;
    writel(clk, reg);
}

static void aml_clock_init(aml_rawnand_t* rawnand)
{
    uint32_t sys_clk_rate, bus_cycle, bus_timing;

    sys_clk_rate = 200;
    aml_set_clock_rate(rawnand, sys_clk_rate);
    bus_cycle  = 6;
    bus_timing = bus_cycle + 1;
    nandctrl_set_cfg(rawnand, 0);
    nandctrl_set_timing_async(rawnand, bus_timing, (bus_cycle - 1));
    nandctrl_send_cmd(rawnand, 1<<31);
}

static void aml_adjust_timings(aml_rawnand_t* rawnand,
                               uint32_t tRC_min, uint32_t tREA_max,
                               uint32_t RHOH_min)
{
    int sys_clk_rate, bus_cycle, bus_timing;

    if (!tREA_max)
        tREA_max = 20;
    if (!RHOH_min)
        RHOH_min = 15;
    if (tREA_max > 30)
        sys_clk_rate = 112;
    else if (tREA_max > 16)
        sys_clk_rate = 200;
    else
        sys_clk_rate = 250;
    aml_set_clock_rate(rawnand, sys_clk_rate);
    bus_cycle  = 6;
    bus_timing = bus_cycle + 1;
    nandctrl_set_cfg(rawnand, 0);
    nandctrl_set_timing_async(rawnand, bus_timing, (bus_cycle - 1));
    nandctrl_send_cmd(rawnand, 1<<31);
}

zx_status_t aml_read_page_hwecc(aml_rawnand_t *rawnand,
                                void *data,
                                void *oob,
                                uint32_t nandpage,
                                int *ecc_correct,
                                bool page0)
{
    uint32_t cmd;
    zx_status_t status;
    uint64_t daddr = rawnand->data_buf_paddr;
    uint64_t iaddr = rawnand->info_buf_paddr;
    int ecc_c;
    volatile uint8_t *reg = (volatile uint8_t*)
        io_buffer_virt(&rawnand->mmio[NANDREG_WINDOW]);
    uint32_t ecc_pagesize, ecc_pages;

    if (!page0) {
        ecc_pagesize = aml_get_ecc_pagesize(rawnand, AML_ECC_BCH8);
        ecc_pages = rawnand->writesize / ecc_pagesize;
    } else
        ecc_pages = 1;
    aml_select_chip(rawnand, 0);
    /*
     * Flush and invalidate (only invalidate is really needed), the
     * info and data buffers before kicking off DMA into them.
     */
    io_buffer_cache_flush_invalidate(&rawnand->data_buffer, 0,
                                     rawnand->writesize);
    io_buffer_cache_flush_invalidate(&rawnand->info_buffer, 0,
           ecc_pages * sizeof(struct aml_info_format));
    /* Send the page address into the controller */
    nand_command(rawnand, NAND_CMD_READ0, 0x00, nandpage);
    cmd = GENCMDDADDRL(AML_CMD_ADL, daddr);
    writel(cmd, reg + P_NAND_CMD);
    cmd = GENCMDDADDRH(AML_CMD_ADH, daddr);
    writel(cmd, reg + P_NAND_CMD);
    cmd = GENCMDIADDRL(AML_CMD_AIL, iaddr);
    writel(cmd, reg + P_NAND_CMD);
    cmd = GENCMDIADDRH(AML_CMD_AIH, iaddr);
    writel(cmd, reg + P_NAND_CMD);
    /* page0 needs randomization. so force it for page0 */
    if (page0 || rawnand->controller_params.rand_mode)
        /*
         * Only need to set the seed if randomizing 
         * is enabled.
         */
	aml_cmd_seed(rawnand, nandpage);
    if (!page0)
        aml_cmd_n2m(rawnand, ecc_pages, ecc_pagesize);
    else
        aml_cmd_n2m_page0(rawnand);
    status = aml_wait_dma_finish(rawnand);
    if (status != ZX_OK)
        return status;
    aml_queue_rb(rawnand);
#if 0
    {
        struct aml_info_format *info_blk;

        info_blk = (struct aml_info_format *)rawnand->info_buf;
        for (uint32_t i = 0; i < ecc_pages; i++) {
            zxlogf(ERROR, "info_bytes = %x\n",
                   info_blk->info_bytes);
            zxlogf(ERROR, "zero_cnt = %x\n",
                   info_blk->zero_cnt);
            zxlogf(ERROR, "ecc = %x\n",
                   *(uint8_t *)&(info_blk->ecc));
            zxlogf(ERROR, "reserved = %x\n",
                   info_blk->reserved);
            info_blk++;
        }
    }
#endif
    status = aml_check_ecc_pages(rawnand, ecc_pages);
    if (status != ZX_OK)
        return status;
    /*
     * Finally copy out the data and oob as needed
     */
    if (data != NULL) {
        if (!page0)
            memcpy(data, rawnand->data_buf, rawnand->writesize);
        else
            memcpy(data, rawnand->data_buf, 384);         /* XXX - FIXME */
    }
    if (oob != NULL)
        status = aml_get_oob_byte(rawnand, oob);
    ecc_c = aml_get_ecc_corrections(rawnand, ecc_pages);
    if (ecc_c < 0) {
            zxlogf(ERROR, "%s: Uncorrectable ECC error on read\n",
                   __func__);
            status = ZX_ERR_IO;
    }
    *ecc_correct = ecc_c;
    return status;
}

/*
 * TODO : Right now, the driver uses a buffer for DMA, which 
 * is not needed. We should initiate DMA to/from pages passed in.
 */
zx_status_t aml_write_page_hwecc(aml_rawnand_t *rawnand,
                                 void *data,
                                 void *oob,
                                 uint32_t nandpage,
                                 bool page0)
{
    uint32_t cmd;
    uint64_t daddr = rawnand->data_buf_paddr;
    uint64_t iaddr = rawnand->info_buf_paddr;
    zx_status_t status;
    volatile uint8_t *reg = (volatile uint8_t*)
        io_buffer_virt(&rawnand->mmio[NANDREG_WINDOW]);
    uint32_t ecc_pages, ecc_pagesize;

    if (!page0) {
        ecc_pagesize = aml_get_ecc_pagesize(rawnand, AML_ECC_BCH8);
        ecc_pages = rawnand->writesize / ecc_pagesize;
    } else
        ecc_pages = 1;
    aml_select_chip(rawnand, 0);
    if (data != NULL) {
        memcpy(rawnand->data_buf, data, rawnand->writesize);
        io_buffer_cache_flush(&rawnand->data_buffer, 0,
                              rawnand->writesize);
    }
    if (oob != NULL) {
        aml_set_oob_byte(rawnand, oob, ecc_pages);
        io_buffer_cache_flush_invalidate(&rawnand->info_buffer, 0,
            ecc_pages * sizeof(struct aml_info_format));
    }
    nand_command(rawnand, NAND_CMD_SEQIN, 0x00, nandpage);
    cmd = GENCMDDADDRL(AML_CMD_ADL, daddr);
    writel(cmd, reg + P_NAND_CMD);
    cmd = GENCMDDADDRH(AML_CMD_ADH, daddr);
    writel(cmd, reg + P_NAND_CMD);
    cmd = GENCMDIADDRL(AML_CMD_AIL, iaddr);
    writel(cmd, reg + P_NAND_CMD);
    cmd = GENCMDIADDRH(AML_CMD_AIH, iaddr);
    writel(cmd, reg + P_NAND_CMD);
    /* page0 needs randomization. so force it for page0 */
    if (page0 || rawnand->controller_params.rand_mode)
        /*
         * Only need to set the seed if randomizing 
         * is enabled.
         */
	aml_cmd_seed(rawnand, nandpage);
    if (!page0)    
        aml_cmd_m2n(rawnand, ecc_pages, ecc_pagesize);
    else
        aml_cmd_m2n_page0(rawnand);
    status = aml_wait_dma_finish(rawnand);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: error from wait_dma_finish\n",
               __func__);        
        return status;
    }
    nand_command(rawnand, NAND_CMD_PAGEPROG, -1, -1);    
    return nand_wait(rawnand, 20);
}

/*
 * Erase entry point into the Amlogic driver.
 * nandblock : NAND erase block address.
 */
zx_status_t aml_erase_block(aml_rawnand_t *rawnand,
                            uint32_t nandpage)
{
    /* nandblock has to be erasesize aligned */
    if (nandpage % rawnand->erasesize_pages) {
        zxlogf(ERROR, "%s: NAND block %u must be a erasesize_pages (%u) multiple\n",
               __func__, nandpage, rawnand->erasesize_pages);
        return ZX_ERR_INVALID_ARGS;
    }
    aml_select_chip(rawnand, 0);
    nand_command(rawnand, NAND_CMD_ERASE1, -1, nandpage);
    nand_command(rawnand, NAND_CMD_ERASE2, -1, -1);
    return nand_wait(rawnand, 400);
}

static zx_status_t aml_get_flash_type(aml_rawnand_t *rawnand)
{
    uint8_t nand_maf_id, nand_dev_id;
    uint8_t id_data[8];
    struct nand_chip_table *nand_chip;
    uint32_t i;
    
    aml_select_chip(rawnand, 0);
    nand_command(rawnand,  NAND_CMD_RESET, -1, -1);
    nand_command(rawnand, NAND_CMD_READID, 0x00, -1);
    /* Read manufacturer and device IDs */
    nand_maf_id = aml_read_byte(rawnand);
    nand_dev_id = aml_read_byte(rawnand);
    /* Read again */
    nand_command(rawnand, NAND_CMD_READID, 0x00, -1);
    /* Read entire ID string */
    for (i = 0; i < 8; i++)
        id_data[i] = aml_read_byte(rawnand);
    if (id_data[0] != nand_maf_id || id_data[1] != nand_dev_id) {
        zxlogf(ERROR, "second ID read did not match %02x,%02x against %02x,%02x\n",
               nand_maf_id, nand_dev_id, id_data[0], id_data[1]);
    }

    zxlogf(ERROR, "Mfg id/Dev id = %02x %02x\n", nand_maf_id, nand_dev_id);
            
    nand_chip = find_nand_chip_table(nand_maf_id, nand_dev_id);
    if (nand_chip == NULL) {
        
#if 0
        nand_chip = &default_chip;
#else
        zxlogf(ERROR, "%s: Cound not find matching NAND chip. NAND chip unsupported."
               " This is FATAL\n",
               __func__);
        return ZX_ERR_UNAVAILABLE;
#endif
    }
#if 0
    zxlogf(ERROR, "Found matching device %s:%s\n",
           nand_chip->manufacturer_name,
           nand_chip->device_name);
#endif
    if (true /* nand_chip->extended_id_nand */) {
	/*
	 * Initialize pagesize, eraseblk size, oobsize and 
	 * buswidth from extended parameters queried just now.
	 */
	uint8_t extid = id_data[3];

        zxlogf(ERROR, "%s: Found extended id NAND extid = 0x%x\n",
               __func__, extid);
	rawnand->writesize = 1024 << (extid & 0x03);
	extid >>= 2;
	/* Calc oobsize */
	rawnand->oobsize = (8 << (extid & 0x01)) *
	    (rawnand->writesize >> 9);
	extid >>= 2;
	/* Calc blocksize. Blocksize is multiples of 64KiB */
	rawnand->erasesize = (64 * 1024) << (extid & 0x03);
	extid >>= 2;
	/* Get buswidth information */
	rawnand->bus_width = (extid & 0x01) ? NAND_BUSWIDTH_16 : 0;
        zxlogf(ERROR, "%s: writesize = %u, oobsize = %u, erasesize = %u bus_width = %u\n",
               __func__, rawnand->writesize, rawnand->oobsize,
	       rawnand->erasesize, rawnand->bus_width);
#if 0
	/*                                                              
	 * Toshiba 24nm raw SLC (i.e., not BENAND) have 32B OOB per     
	 * 512B page. For Toshiba SLC, we decode the 5th/6th byte as    
	 * follows:                                                     
	 * - ID byte 6, bits[2:0]: 100b -> 43nm, 101b -> 32nm,          
	 *                         110b -> 24nm                         
	 * - ID byte 5, bit[7]:    1 -> BENAND, 0 -> raw SLC            
	 */
	if (id_len >= 6 && id_data[0] == NAND_MFR_TOSHIBA &&
	    nand_is_slc(chip) &&
	    (id_data[5] & 0x7) == 0x6 /* 24nm */ &&
	    !(id_data[4] & 0x80) /* !BENAND */) {
	    mtd->oobsize = 32 * mtd->writesize >> 9;
	}
#endif
    } else {
	/*
	 * Initialize pagesize, eraseblk size, oobsize and 
	 * buswidth from values in table.
	 */
	rawnand->writesize = nand_chip->page_size;
	rawnand->oobsize = nand_chip->oobsize;
	rawnand->erasesize = nand_chip->erase_block_size;
	rawnand->bus_width = nand_chip->bus_width;
    }
    rawnand->erasesize_pages =
        rawnand->erasesize / rawnand->writesize;
    rawnand->chipsize = nand_chip->chipsize;
    rawnand->page_shift = ffs(rawnand->writesize) - 1;

    zxlogf(ERROR, "%s: chipsize = %lu, page_shift = %u\n",
           __func__, rawnand->chipsize, rawnand->page_shift);
    
    /*
     * We found a matching device in our database, use it to 
     * initialize. Adjust timings and set various parameters.
     */
    zxlogf(ERROR, "Adjusting timings based on datasheet values\n");
    aml_adjust_timings(rawnand,
                       nand_chip->timings.tRC_min,
                       nand_chip->timings.tREA_max, 
                       nand_chip->timings.RHOH_min);
    return ZX_OK;
}

static int aml_rawnand_irq_thread(void *arg) {
    zxlogf(INFO, "aml_rawnand_irq_thread start\n");

    aml_rawnand_t* rawnand = arg;

    while (1) {
        uint64_t slots;

        zx_status_t result = zx_interrupt_wait(rawnand->irq_handle, &slots);
        if (result != ZX_OK) {
            zxlogf(ERROR,
                   "aml_rawnand_irq_thread: zx_interrupt_wait got %d\n",
                   result);
            break;
        }
        /*
         * Wakeup blocked requester on 
         * completion_wait(&rawnand->req_completion, ZX_TIME_INFINITE);
         */
        completion_signal(&rawnand->req_completion);
    }

    return 0;
}

static void
aml_rawnand_query(void *ctx, nand_info_t *info_out,
                  size_t *nand_op_size_out)
{

}

/*
 * Queue up a read/write request to the rawnand.
 */
static void
aml_rawnand_queue(void *ctx, nand_op_t *op)
{

}

static void
aml_rawnand_bad_block_list(void *ctx, size_t *num_bad_blocks,
                           uint64_t **blocklist)
{

}

static nand_protocol_ops_t rawnand_ops = {
    .query = aml_rawnand_query,
    .queue = aml_rawnand_queue,
    .get_bad_block_list = aml_rawnand_bad_block_list
};

static void aml_rawnand_release(void* ctx) {
    aml_rawnand_t* rawnand = ctx;

    for (rawnand_addr_window_t wnd = 0 ;
         wnd < ADDR_WINDOW_COUNT ;
         wnd++)
        io_buffer_release(&rawnand->mmio[wnd]);
    zx_handle_close(rawnand->irq_handle);
    /*
     * XXX -  We need to make sure that the irq thread and
     * the worker thread exit cleanly.
     */
    free(rawnand);
}

static void aml_set_encryption(aml_rawnand_t *rawnand)
{
    uint32_t cfg;
    volatile uint8_t *reg = (volatile uint8_t*)
        io_buffer_virt(&rawnand->mmio[NANDREG_WINDOW]);
    
    cfg = readl(reg + P_NAND_CFG);
    cfg |= (1 << 17);
    writel(cfg, reg + P_NAND_CFG);
}

/* 
 * Read one of the page0 pages, and use the result to init
 * ECC algorithm and rand-mode.
 */
static zx_status_t aml_nand_init_from_page0(aml_rawnand_t* rawnand)
{
    uint32_t i;
    zx_status_t status;
    char *data;
    nand_page0_t *page0;
    int ecc_correct;
    
    data = malloc(rawnand->writesize);    
    /*
     * There are 8 copies of page0 spaced apart by 128 pages
     * starting at Page 0. Read the first we can.
     */
    for (i = 0 ; i < 7 ; i++) {
        status = nand_read_page0(rawnand, data, NULL, i * 128,
                                 &ecc_correct, 3);
        if (status == ZX_OK)
            break;
    }
    if (status != ZX_OK) {
        /* 
         * Could not read any of the page0 copies. This is a fatal
         * error.
         */
        free(data);
        zxlogf(ERROR, "%s: Page0 Read (all copies) failed\n", __func__);        
        return status;
    }
    page0 = (nand_page0_t *)data;
    rawnand->controller_params.rand_mode =
        (page0->nand_setup.cfg.d32 >> 19) & 0x1;
    rawnand->controller_params.bch_mode =
        (page0->nand_setup.cfg.d32 >> 14) & 0x7;
    zxlogf(ERROR, "%s: Initializing BCH mode (%d) and rand-mode (%d) from Page0@%u\n",
           __func__, rawnand->controller_params.bch_mode,
           rawnand->controller_params.rand_mode, i * 128);

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
    
    free(data);
    return ZX_OK;
}

static zx_status_t aml_rawnand_allocbufs(aml_rawnand_t* rawnand)
{
    zx_status_t status;

    status = pdev_get_bti(&rawnand->pdev, 0, &rawnand->bti_handle);
    if (status != ZX_OK) {
        zxlogf(ERROR, "rawnand_test_allocbufs: pdev_get_bti failed (%d)\n",
            status);
        return status;
    }
    status = io_buffer_init(&rawnand->data_buffer,
                            rawnand->bti_handle,
                            4096,
                            IO_BUFFER_RW | IO_BUFFER_CONTIG);
    if (status != ZX_OK) {
        zxlogf(ERROR,
               "rawnand_test_allocbufs: io_buffer_init(data_buffer) failed\n");
        zx_handle_close(rawnand->bti_handle);        
        return status;
    }
    status = io_buffer_init(&rawnand->info_buffer,
                            rawnand->bti_handle,
                            4096,
                            IO_BUFFER_RW | IO_BUFFER_CONTIG);
    if (status != ZX_OK) {
        zxlogf(ERROR,
               "rawnand_test_allocbufs: io_buffer_init(info_buffer) failed\n");
        io_buffer_release(&rawnand->data_buffer);
        zx_handle_close(rawnand->bti_handle);
        return status;        
    }
    rawnand->data_buf = io_buffer_virt(&rawnand->data_buffer);    
    rawnand->info_buf = io_buffer_virt(&rawnand->info_buffer);
    rawnand->data_buf_paddr = io_buffer_phys(&rawnand->data_buffer);
    rawnand->info_buf_paddr = io_buffer_phys(&rawnand->info_buffer);
    return ZX_OK;
}


static void aml_rawnand_freebufs(aml_rawnand_t* rawnand)
{
    io_buffer_release(&rawnand->data_buffer);
    io_buffer_release(&rawnand->info_buffer);
    zx_handle_close(rawnand->bti_handle);
}

static zx_status_t aml_nand_init(aml_rawnand_t* rawnand)
{
    zx_status_t status;
    
    /*
     * Do nand scan to get manufacturer and other info
     */
    status = aml_get_flash_type(rawnand);
    if (status != ZX_OK)
        return status;
    /*
     * Again: All of the following crap should be read and
     * parsed from the equivalent of the device tree.
     */
    rawnand->controller_params.ecc_strength = aml_params.ecc_strength;
    rawnand->controller_params.user_mode = aml_params.user_mode;
    rawnand->controller_params.rand_mode = aml_params.rand_mode;
    rawnand->controller_params.options = NAND_USE_BOUNCE_BUFFER;
    rawnand->controller_params.bch_mode = aml_params.bch_mode;
    rawnand->controller_delay = 200;

    /*
     * Note on OOB byte settings.
     * The default config for OOB is 2 bytes per OOB page. This is the
     * settings we use. So nothing to be done for OOB. If we ever need
     * to switch to 16 bytes of OOB per NAND page, we need to set the 
     * right bits in the CFG register/
     */

    status = aml_rawnand_allocbufs(rawnand);
    if (status != ZX_OK)
        return status;
        
    /* 
     * Read one of the copies of page0, and use that to initialize 
     * ECC algorithm and rand-mode.
     */
    status = aml_nand_init_from_page0(rawnand);
    
    return status;
}

static void aml_rawnand_unbind(void* ctx) {
    aml_rawnand_t* rawnand = ctx;

    aml_rawnand_freebufs(rawnand);
    
    /* Terminate our worker thread here */
#if 0
    mtx_lock(&rawnand->txn_lock);
    rawnand->dead = true;
    mtx_unlock(&rawnand->txn_lock);
    completion_signal(&rawnand->txn_completion);
    // wait for worker thread to finish before removing devices
    thrd_join(rawnand->worker_thread, NULL);

    /* 
     * Then similarly, terminate the irq thread 
     * and wait for it to exit
     */
#endif
    /* Then remove the device */
    device_remove(rawnand->zxdev);
}

bool aml_check_write_protect(aml_rawnand_t *rawnand)
{
    uint8_t cmd_status;

    aml_select_chip(rawnand, 0);
    nand_command(rawnand, NAND_CMD_STATUS, -1, -1);
    cmd_status = aml_read_byte(rawnand);
    if (!(cmd_status & NAND_STATUS_WP))
        /* If clear, the device is WP */
        return true;
    else
        /* If set, the device is not-WP */        
        return false;
}

static zx_protocol_device_t rawnand_device_proto = {
    .version = DEVICE_OPS_VERSION,
        .unbind = aml_rawnand_unbind,
        .release = aml_rawnand_release,
};

static zx_status_t aml_rawnand_bind(void* ctx, zx_device_t* parent)
{
    zx_status_t status;

    aml_rawnand_t* rawnand = calloc(1, sizeof(aml_rawnand_t));
    
    if (!rawnand) {
        return ZX_ERR_NO_MEMORY;
    }

    rawnand->req_completion = COMPLETION_INIT;

    if ((status = device_get_protocol(parent,
                                      ZX_PROTOCOL_PLATFORM_DEV,
                                      &rawnand->pdev)) != ZX_OK) {
        zxlogf(ERROR,
               "aml_rawnand_bind: ZX_PROTOCOL_PLATFORM_DEV not available\n");
        goto fail;
    }

    pdev_device_info_t info;
    status = pdev_get_device_info(&rawnand->pdev, &info);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_rawnand_bind: pdev_get_device_info failed\n");
        goto fail;
    }

    /* Map all of the mmio windows that we need */
    for (rawnand_addr_window_t wnd = 0 ;
         wnd < ADDR_WINDOW_COUNT ;
         wnd++) {
        status = pdev_map_mmio_buffer(&rawnand->pdev,
                                      wnd,
                                      ZX_CACHE_POLICY_UNCACHED_DEVICE,
                                      &rawnand->mmio[wnd]);
        if (status != ZX_OK) {
            zxlogf(ERROR, "aml_rawnand_bind: pdev_map_mmio_buffer failed %d\n",
                   status);
            goto fail;
        }
    }

    status = pdev_map_interrupt(&rawnand->pdev, 0, &rawnand->irq_handle);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_rawnand_bind: pdev_map_interrupt failed %d\n",
               status);
        for (rawnand_addr_window_t wnd = 0 ;
             wnd < ADDR_WINDOW_COUNT ;
             wnd++)
            io_buffer_release(&rawnand->mmio[wnd]);            
        goto fail;
    }

    int rc = thrd_create_with_name(&rawnand->irq_thread,
				   aml_rawnand_irq_thread,
                                   rawnand, "aml_rawnand_irq_thread");
    if (rc != thrd_success) {
        zx_handle_close(rawnand->irq_handle);
        for (rawnand_addr_window_t wnd = 0 ;
             wnd < ADDR_WINDOW_COUNT ;
             wnd++)
            io_buffer_release(&rawnand->mmio[wnd]);            
        status = thrd_status_to_zx_status(rc);
        goto fail;
    }

    /*
     * This creates a device that a top level (controller independent)
     * rawnand driver can bind to.
     */
    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "aml-rawnand",
        .ctx = rawnand,
        .ops = &rawnand_device_proto,
        .proto_id = ZX_PROTOCOL_NAND,
        .proto_ops = &rawnand_ops,
    };

    status = device_add(parent, &args, &rawnand->zxdev);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_rawnand_bind: device_add failed\n");
        zx_handle_close(rawnand->irq_handle);
        for (rawnand_addr_window_t wnd = 0 ;
             wnd < ADDR_WINDOW_COUNT ;
             wnd++)
            io_buffer_release(&rawnand->mmio[wnd]);
        goto fail;
    }

    /*
     * For the Toshiba chip we are using/plan to use with Estelle
     * (Toshiba TC58NVG2S0HBAI4)
     * tRC_min = 25ns
     * tREA_max = 20ns
     * RHOH_min = 25ns 
     */

    aml_clock_init(rawnand);                    
    status = aml_nand_init(rawnand);

    if (status != ZX_OK) {
        zxlogf(ERROR,
               "aml_rawnand_bind: aml_nand_init() failed - This is FATAL\n");
        goto fail;
    }

#ifdef AML_RAWNAND_TEST    
//        rawnand_dump_bbt(rawnand);
//        rawnand_test_page0(rawnand);
//        rawnand_test(rawnand);
//        rawnand_erasetest(rawnand);
//    rawnand_writetest_one_eraseblock(rawnand, 4093*64);
    rawnand_erase_write_test(rawnand, 0x000010c00000, 0x000020000000);
#endif

    return status;

fail:
    free(rawnand);
    return status;
}

static zx_driver_ops_t aml_rawnand_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = aml_rawnand_bind,
};

ZIRCON_DRIVER_BEGIN(aml_rawnand, aml_rawnand_driver_ops, "zircon", "0.1", 3)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_PLATFORM_DEV),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_AMLOGIC),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_DID, PDEV_DID_AMLOGIC_RAWNAND),
ZIRCON_DRIVER_END(aml_rawnand)
