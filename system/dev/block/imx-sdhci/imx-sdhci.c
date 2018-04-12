// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Standard Includes
#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

// DDK Includes
#include <ddk/binding.h>
#include <ddk/device.h>
#include <ddk/debug.h>
#include <ddk/io-buffer.h>
#include <ddk/phys-iter.h>
#include <ddk/protocol/platform-defs.h>
#include <ddk/protocol/platform-device.h>
#include <ddk/protocol/platform-bus.h>
#include <ddk/protocol/sdmmc.h>
#include <ddk/protocol/sdhci.h>
#include <hw/sdmmc.h>
#include <zircon/types.h>


// Zircon Includes
#include <fdio/watcher.h>
#include <zircon/threads.h>
#include <zircon/assert.h>
#include <sync/completion.h>
#include <pretty/hexdump.h>

#include "imx-sdhci.h"

#define SDHCI_ERROR(fmt, ...)       zxlogf(ERROR, "[%s %d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define SDHCI_INFO(fmt, ...)        zxlogf(ERROR, "[%s %d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define SDHCI_TRACE(fmt, ...)       zxlogf(ERROR, "[%s %d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define SDHCI_FUNC_ENTRY_LOG        zxlogf(ERROR, "[%s %d]\n", __func__, __LINE__)

#define SD_FREQ_SETUP_HZ  400000

#define MAX_TUNING_COUNT 40

#define PAGE_MASK   (PAGE_SIZE - 1ull)

#define HI32(val)   (((val) >> 32) & 0xffffffff)
#define LO32(val)   ((val) & 0xffffffff)
#define SDMMC_COMMAND(c) ((c) << 24)

typedef struct sdhci_adma64_desc {
    union {
        struct {
            uint8_t valid : 1;
            uint8_t end   : 1;
            uint8_t intr  : 1;
            uint8_t rsvd0 : 1;
            uint8_t act1  : 1;
            uint8_t act2  : 1;
            uint8_t rsvd1 : 2;
            uint8_t rsvd2;
        } __PACKED;
        uint16_t attr;
    } __PACKED;
    uint16_t length;
    uint64_t address;
} __PACKED sdhci_adma64_desc_t;

static_assert(sizeof(sdhci_adma64_desc_t) == 12, "unexpected ADMA2 descriptor size");

// 64k max per descriptor
#define ADMA2_DESC_MAX_LENGTH   0x10000 // 64k
// for 2M max transfer size for fully discontiguous
// also see SDMMC_PAGES_COUNT in ddk/protocol/sdmmc.h
#define DMA_DESC_COUNT          512

typedef struct imx_sdhci_device {
    platform_device_protocol_t  pdev;
    platform_bus_protocol_t     pbus;
    zx_device_t*                zxdev;
    io_buffer_t                 mmios;
    zx_handle_t                 irq_handle;

    volatile imx_sdhci_regs_t*  regs;
    uint64_t                    regs_size;
    zx_handle_t                 regs_handle;
    zx_handle_t                 bti_handle;

    // DMA descriptors
    io_buffer_t iobuf;
    sdhci_adma64_desc_t* descs;

    // Held when a command or action is in progress.
    mtx_t mtx;

    // Current command request
    sdmmc_req_t* cmd_req;
    // Current data line request
    sdmmc_req_t* data_req;
    // Current block id to transfer (PIO)
    uint16_t data_blockid;
    uint16_t reserved;
    // Set to true if the data stage completed before the command stage
    bool data_done;
    // used to signal request complete
    completion_t req_completion;

    // Controller info
    sdmmc_host_info_t info;

    // Controller specific quirks
    uint64_t quirks;

    // Base clock rate
    uint32_t base_clock;

} imx_sdhci_device_t;

static const uint32_t error_interrupts = (
    IMX_SDHC_INT_STAT_DMAE  |
    IMX_SDHC_INT_STAT_TNE   |
    IMX_SDHC_INT_STAT_AC12E |
    IMX_SDHC_INT_STAT_DEBE  |
    IMX_SDHC_INT_STAT_DCE   |
    IMX_SDHC_INT_STAT_DTOE  |
    IMX_SDHC_INT_STAT_CIE   |
    IMX_SDHC_INT_STAT_CEBE  |
    IMX_SDHC_INT_STAT_CCE   |
    IMX_SDHC_INT_STAT_CTOE
);

static const uint32_t normal_interrupts = (
    IMX_SDHC_INT_STAT_BRR   |
    IMX_SDHC_INT_STAT_BWR   |
    IMX_SDHC_INT_STAT_DINT  |
    IMX_SDHC_INT_STAT_BGE   |
    IMX_SDHC_INT_STAT_TC    |
    IMX_SDHC_INT_STAT_CC
);

static bool imx_sdmmc_cmd_rsp_busy(uint32_t cmd) {
    uint32_t resp = cmd & SDMMC_RESP_MASK;
    return resp == SDMMC_RESP_LEN_48B;
}

static bool imx_sdmmc_has_data(uint32_t resp_type) {
    return resp_type & SDMMC_RESP_DATA_PRESENT;
}


static bool imx_sdmmc_supports_adma2_64bit(imx_sdhci_device_t* dev) {
    return (0); // TODO: iMX8 doesn't support 64bit ADMA2
}

static zx_status_t imx_sdhci_wait_for_reset(imx_sdhci_device_t* dev,
                                            const uint32_t mask, zx_time_t timeout) {
    zx_time_t deadline = zx_clock_get(ZX_CLOCK_MONOTONIC) + timeout;
    while (true) {
        if (((dev->regs->sys_ctrl) & mask) == 0) {
            break;
        }
        if (zx_clock_get(ZX_CLOCK_MONOTONIC) > deadline) {
            SDHCI_ERROR("time out while waiting for reset\n");
            return ZX_ERR_TIMED_OUT;
        }
    }
    return ZX_OK;
}

static void imx_sdhci_complete_request_locked(imx_sdhci_device_t* dev, sdmmc_req_t* req,
                                                zx_status_t status) {
    SDHCI_TRACE("complete cmd 0x%08x status %d\n", req->cmd, status);

    // Disable interrupts when no pending transfer
    dev->regs->int_signal_en = 0;

    dev->cmd_req = NULL;
    dev->data_req = NULL;
    dev->data_blockid = 0;
    dev->data_done = false;

    req->status = status;
    completion_signal(&dev->req_completion);
}

static void imx_sdhci_cmd_stage_complete_locked(imx_sdhci_device_t* dev) {
    SDHCI_TRACE("Got CC interrupt\n");

    if (!dev->cmd_req) {
        SDHCI_TRACE("Spurious CC interupt\n");
        return;
    }

    sdmmc_req_t* req = dev->cmd_req;
    volatile struct imx_sdhci_regs* regs = dev->regs;
    uint32_t cmd = SDMMC_COMMAND(req->cmd) | req->resp_type;

    // Read the response data
    if (cmd & SDMMC_RESP_LEN_136) {
        // TODO: need any quirks??
        req->response[0] = (regs->cmd_rsp0 << 8);
        req->response[1] = (regs->cmd_rsp1 << 8) | ((regs->cmd_rsp0 >> 24) & 0xFF);
        req->response[2] = (regs->cmd_rsp2 << 8) | ((regs->cmd_rsp1 >> 24) & 0xFF);
        req->response[3] = (regs->cmd_rsp3 << 8) | ((regs->cmd_rsp2 >> 24) & 0xFF);

    } else if (cmd & (SDMMC_RESP_LEN_48 | SDMMC_RESP_LEN_48B)) {
        req->response[0] = regs->cmd_rsp0;
        req->response[1] = regs->cmd_rsp1;
    }

    // We're done if the command has no data stage or if the data stage completed early
    if (!dev->data_req || dev->data_done) {
        imx_sdhci_complete_request_locked(dev, dev->cmd_req, ZX_OK);
    } else {
        dev->cmd_req = NULL;
    }
}

static void imx_sdhci_data_stage_read_ready_locked(imx_sdhci_device_t* dev) {
    SDHCI_TRACE("Got BRR Interrupts\n");

    if (!dev->data_req || !imx_sdmmc_has_data(dev->data_req->resp_type)) {
        SDHCI_TRACE("Spurious BRR Interrupt\n");
        return;
    }

    sdmmc_req_t* req = dev->data_req;

    if (dev->data_req->cmd == MMC_SEND_TUNING_BLOCK) {
        // tuing commnad is done here
        imx_sdhci_complete_request_locked(dev, dev->data_req, ZX_OK);
    } else {
        // Sequentially read each block
        for (size_t byteid = 0; byteid < req->blocksize; byteid += 4) {
            const size_t offset = dev->data_blockid * req->blocksize + byteid;
            uint32_t* wrd = req->virt + offset;
            *wrd = dev->regs->data_buff_acc_port; //TODO: Can't read this if DMA is enabled!
        }
        dev->data_blockid += 1;
    }
}

static void imx_sdhci_data_stage_write_ready_locked(imx_sdhci_device_t* dev) {
    SDHCI_TRACE("Got BWR Interrupt\n");

    if (!dev->data_req || !imx_sdmmc_has_data(dev->data_req->resp_type)) {
        SDHCI_TRACE("Spurious BWR Interrupt\n");
        return;
    }

    sdmmc_req_t* req = dev->data_req;

    // Sequentially write each block
    for (size_t byteid = 0; byteid < req->blocksize; byteid += 4) {
        const size_t offset = dev->data_blockid * req->blocksize + byteid;
        uint32_t* wrd = req->virt + offset;
        dev->regs->data_buff_acc_port = *wrd; //TODO: Can't write if DMA is enabled
    }
    dev->data_blockid += 1;
}

static void imx_sdhci_transfer_complete_locked(imx_sdhci_device_t* dev) {
    SDHCI_TRACE("Got TC Interrupt\n");
    if (!dev->data_req) {
        SDHCI_TRACE("Spurious TC Interrupt\n");
        return;
    }

    if (dev->cmd_req) {
        dev->data_done = true;
    } else {
        imx_sdhci_complete_request_locked(dev, dev->data_req, ZX_OK);
    }
}

static void imx_sdhci_error_recovery_locked(imx_sdhci_device_t* dev) {
    // Reset internal state machines
    dev->regs->sys_ctrl |= IMX_SDHC_SYS_CTRL_RSTC;
    imx_sdhci_wait_for_reset(dev, IMX_SDHC_SYS_CTRL_RSTC, ZX_SEC(1));
    dev->regs->sys_ctrl |= IMX_SDHC_SYS_CTRL_RSTD;
    imx_sdhci_wait_for_reset(dev, IMX_SDHC_SYS_CTRL_RSTD, ZX_SEC(1));

    // Complete any pending txn with error status
    if (dev->cmd_req != NULL) {
        imx_sdhci_complete_request_locked(dev, dev->cmd_req, ZX_ERR_IO);
    } else if (dev->data_req != NULL) {
        imx_sdhci_complete_request_locked(dev, dev->data_req, ZX_ERR_IO);
    }
}

static uint32_t get_clock_divider(const uint32_t base_clock, const uint32_t target_rate) {
    uint32_t pre_div = 1;
    uint32_t div = 1;

    if (target_rate >= base_clock) {
        // A clock divider of 0 means "don't divide the clock"
        // If the base clock is already slow enough to use as the SD clock then
        // we don't need to divide it any further.
        return 0;
    }

    SDHCI_TRACE("base %d, pre_div %d, div = %d, target_rate %d\n",
        base_clock, pre_div, div, target_rate);
    while (base_clock / pre_div / 16 > target_rate && pre_div < 256) {
        SDHCI_TRACE("base %d, pre_div %d, div = %d, target_rate %d\n",
            base_clock, pre_div, div, target_rate);
        pre_div *= 2;
    }

    while (base_clock / pre_div / div > target_rate && div < 16) {
        SDHCI_TRACE("base %d, pre_div %d, div = %d, target_rate %d\n",
            base_clock, pre_div, div, target_rate);
        div++;
    }

    SDHCI_TRACE("base %d, pre_div %d, div = %d, target_rate %d\n",
        base_clock, pre_div, div, target_rate);

    pre_div >>= 1;
    div -= 1;

    return (((pre_div & 0xFF) << 16)| (div & 0xF));
}


static int imx_sdhci_irq_thread(void *args) {
    zx_status_t wait_res;
    imx_sdhci_device_t* dev = (imx_sdhci_device_t*)args;
    volatile struct imx_sdhci_regs* regs = dev->regs;
    zx_handle_t irq_handle = dev->irq_handle;

    while(true) {
        uint64_t slots;
        wait_res = zx_interrupt_wait(irq_handle, &slots);
        if (wait_res != ZX_OK) {
            SDHCI_ERROR("Interrupt_wait failed with retcode %d\n", wait_res);
            break;
        }

        const uint32_t irq = regs->int_status;
        SDHCI_TRACE("got irq 0x%08x 0x%08x en 0x%08x sig 0x%08x\n", regs->int_status, irq,
                                                    regs->int_status_en, regs->int_signal_en);

        // Acknowledge the IRQs that we stashed.
        regs->int_status = irq;

        mtx_lock(&dev->mtx);
        if (irq & IMX_SDHC_INT_STAT_CC) {
            imx_sdhci_cmd_stage_complete_locked(dev);
        }
        if (irq & IMX_SDHC_INT_STAT_BRR) {
            imx_sdhci_data_stage_read_ready_locked(dev);
        }
        if (irq & IMX_SDHC_INT_STAT_BWR) {
            imx_sdhci_data_stage_write_ready_locked(dev);
        }
        if (irq & IMX_SDHC_INT_STAT_TC) {
            imx_sdhci_transfer_complete_locked(dev);
        }
        if (irq & error_interrupts) {
            if (irq & IMX_SDHC_INT_STAT_DMAE) {
                SDHCI_TRACE("ADMA error 0x%x ADMAADDR0 0x%x\n",
                regs->adma_err_status, regs->adma_sys_addr);
            }
            imx_sdhci_error_recovery_locked(dev);
        }
        mtx_unlock(&dev->mtx);
    }
    return ZX_OK;
}

static zx_status_t imx_sdhci_build_dma_desc(imx_sdhci_device_t* dev, sdmmc_req_t* req) {
    SDHCI_FUNC_ENTRY_LOG;
    return ZX_OK;
}

static zx_status_t imx_sdhci_start_req_locked(imx_sdhci_device_t* dev, sdmmc_req_t* req) {
    volatile struct imx_sdhci_regs* regs = dev->regs;
    const uint32_t arg = req->arg;
    const uint16_t blkcnt = req->blockcount;
    const uint16_t blksiz = req->blocksize;
    uint32_t cmd = SDMMC_COMMAND(req->cmd) | req->resp_type;
    bool has_data = imx_sdmmc_has_data(req->resp_type);

    if (req->use_dma && !imx_sdmmc_supports_adma2_64bit(dev)) {
        SDHCI_INFO("Host does not support 64BIT DMA\n");
        // iMX8 support ADMA2 32bit!! why not use that!
        return ZX_ERR_NOT_SUPPORTED;
    }

    if (req->use_dma) {
        SDHCI_INFO("we don't support dma yet\t");
        return ZX_ERR_NOT_SUPPORTED;
    }

    SDHCI_TRACE("start_req cmd=0x%08x (data %d dma %d bsy %d) blkcnt %u blksiz %u\n",
                  cmd, has_data, req->use_dma, imx_sdmmc_cmd_rsp_busy(cmd), blkcnt, blksiz);

    // Every command requires that the Commnad Inhibit bit is unset
    uint32_t inhibit_mask = IMX_SDHC_PRES_STATE_CIHB;

    // Busy type commands must also wait for the DATA Inhibit to be 0 unless it's an abort
    // command which can be issued with the data lines active
    if (((cmd & SDMMC_RESP_LEN_48B) == SDMMC_RESP_LEN_48B) &&
        ((cmd & SDMMC_CMD_TYPE_ABORT) == 0)) {
        inhibit_mask |= IMX_SDHC_PRES_STATE_CDIHB;
    }

    // Wait for the inhibit masks from above to become 0 before issueing the command
    while(regs->pres_state & inhibit_mask) {
        zx_nanosleep(zx_deadline_after(ZX_MSEC(1)));
    }

    // zx_status_t status = ZX_OK;
    if (has_data) {
        if (cmd & SDMMC_CMD_MULTI_BLK) {
            cmd |= SDMMC_CMD_AUTO12;
        }
    }

    regs->blk_att = (blksiz | (blkcnt << 16));

    regs->cmd_arg = arg;

    // Clear any pending interrupts before starting the transaction
    regs->int_status = regs->int_signal_en;

    // Unmask and enable interrupts
    regs->int_signal_en = error_interrupts | normal_interrupts;
    regs->int_status_en = error_interrupts | normal_interrupts;

    // Start command
    regs->cmd_xfr_typ = cmd;

    dev->cmd_req = req;

    if (has_data || imx_sdmmc_cmd_rsp_busy(cmd)) {
        dev->data_req = req;
    } else {
        dev->data_req = NULL;
    }
    dev->data_blockid = 0;
    dev->data_done = false;
    return ZX_OK;
// err:
//     return status;
}

static zx_status_t imx_sdhci_finish_req(imx_sdhci_device_t* dev, sdmmc_req_t* req) {
    zx_status_t status = ZX_OK;

    if (req->use_dma && req->pmt != ZX_HANDLE_INVALID) {
        status = zx_pmt_unpin(req->pmt);
        if (status != ZX_OK) {
            SDHCI_ERROR("error %d in pmt_unpin\n", status);
        }
        req->pmt = ZX_HANDLE_INVALID;
    }
    return status;
}

/* SDMMC PROTOCOL Implementations: host_info */
static zx_status_t imx_sdhci_host_info(void* ctx, sdmmc_host_info_t* info) {
    SDHCI_FUNC_ENTRY_LOG;
    return ZX_OK;
}

/* SDMMC PROTOCOL Implementations: set_signal_voltage */
static zx_status_t imx_sdhci_set_signal_voltage(void* ctx, sdmmc_voltage_t voltage) {
    SDHCI_FUNC_ENTRY_LOG;
    return ZX_ERR_NOT_SUPPORTED;
}

/* SDMMC PROTOCOL Implementations: set_bus_width */
static zx_status_t imx_sdhci_set_bus_width(void* ctx, uint32_t bus_width) {
    SDHCI_FUNC_ENTRY_LOG;
    if (bus_width >= SDMMC_BUS_WIDTH_MAX) {
        return ZX_ERR_INVALID_ARGS;
    }
    zx_status_t status = ZX_OK;
    imx_sdhci_device_t* dev = ctx;

    mtx_lock(&dev->mtx);

    if ((bus_width == SDMMC_BUS_WIDTH_8) && !(dev->info.caps & SDMMC_HOST_CAP_BUS_WIDTH_8)) {
        SDHCI_TRACE("8-bit bus width not supported\n");
        status = ZX_ERR_NOT_SUPPORTED;
        goto unlock;
    }

    switch (bus_width) {
        case SDMMC_BUS_WIDTH_1:
            dev->regs->prot_ctrl &= ~IMX_SDHC_PROT_CTRL_DTW_MASK;
            dev->regs->prot_ctrl |= IMX_SDHC_PROT_CTRL_DTW_1;
            break;
        case SDMMC_BUS_WIDTH_4:
            dev->regs->prot_ctrl &= ~IMX_SDHC_PROT_CTRL_DTW_MASK;
            dev->regs->prot_ctrl |= IMX_SDHC_PROT_CTRL_DTW_4;
            break;
        case SDMMC_BUS_WIDTH_8:
            dev->regs->prot_ctrl &= ~IMX_SDHC_PROT_CTRL_DTW_MASK;
            dev->regs->prot_ctrl |= IMX_SDHC_PROT_CTRL_DTW_8;
            break;
        default:
            break;
    }

    SDHCI_TRACE("set bus width to %d\n", bus_width);

unlock:
    mtx_unlock(&dev->mtx);
    return status;
}

/* SDMMC PROTOCOL Implementations: set_bus_freq */
static zx_status_t imx_sdhci_set_bus_freq(void* ctx, uint32_t bus_freq) {
    SDHCI_FUNC_ENTRY_LOG;
    zx_status_t status = ZX_OK;
    imx_sdhci_device_t* dev = ctx;

    mtx_lock(&dev->mtx);

    const uint32_t divider = get_clock_divider(dev->base_clock, bus_freq);
    const uint8_t pre_div = (divider >> 16) & 0xFF;
    const uint8_t div = (divider & 0xF);
    SDHCI_TRACE("divider %d, pre_div %d, div = %d\n",
        divider, pre_div, div);

    volatile struct imx_sdhci_regs* regs = dev->regs;

    uint32_t iterations = 0;
    while (regs->pres_state & (IMX_SDHC_PRES_STATE_CIHB | IMX_SDHC_PRES_STATE_CDIHB)) {
        if (++iterations > 1000) {
            status = ZX_ERR_TIMED_OUT;
            goto unlock;
        }
        zx_nanosleep(zx_deadline_after(ZX_MSEC(1)));
    }

    regs->vend_spec &= ~(IMX_SDHC_VEND_SPEC_FRC_SDCLK_ON);

    // turn off clocks
    regs->sys_ctrl &= ~(IMX_SDHC_SYS_CTRL_CLOCK_PEREN   |
                        IMX_SDHC_SYS_CTRL_CLOCK_HCKEN   |
                        IMX_SDHC_SYS_CTRL_CLOCK_IPGEN   |
                        IMX_SDHC_SYS_CTRL_CLOCK_MASK);

    regs->sys_ctrl |= (IMX_SDHC_SYS_CTRL_CLOCK_PEREN    |
                        IMX_SDHC_SYS_CTRL_CLOCK_HCKEN   |
                        IMX_SDHC_SYS_CTRL_CLOCK_IPGEN   |
                        (pre_div << IMX_SDHC_SYS_CTRL_PREDIV_SHIFT) |
                        (div << IMX_SDHC_SYS_CTRL_DIVIDER_SHIFT));

    regs->vend_spec |= (IMX_SDHC_VEND_SPEC_FRC_SDCLK_ON);
    zx_nanosleep(zx_deadline_after(ZX_MSEC(2)));

    SDHCI_TRACE("desired freq = %d, actual = %d, (%d, %d. %d)\n",
        bus_freq, dev->base_clock / (pre_div<<1) / (div+1), dev->base_clock, pre_div, div);

unlock:
    mtx_unlock(&dev->mtx);
    return status;
}

/* SDMMC PROTOCOL Implementations: set_timing */
static zx_status_t imx_sdhci_set_timing(void* ctx, sdmmc_timing_t timing) {
    SDHCI_FUNC_ENTRY_LOG;
    return ZX_OK;
}

/* SDMMC PROTOCOL Implementations: hw_reset */
static void imx_sdhci_hw_reset(void* ctx) {
    SDHCI_FUNC_ENTRY_LOG;
}

/* SDMMC PROTOCOL Implementations: perform_tuning */
static zx_status_t imx_sdhci_perform_tuning(void* ctx) {
    SDHCI_FUNC_ENTRY_LOG;
    return ZX_OK;
}

/* SDMMC PROTOCOL Implementations: request */
static zx_status_t imx_sdhci_request(void* ctx, sdmmc_req_t* req) {
    SDHCI_FUNC_ENTRY_LOG;
    zx_status_t status = ZX_OK;
    imx_sdhci_device_t* dev = ctx;

    mtx_lock(&dev->mtx);

    // one command at a time
    if ((dev->cmd_req != NULL) || (dev->data_req != NULL)) {
        status = ZX_ERR_SHOULD_WAIT;
        goto unlock_out;
    }

    status = imx_sdhci_start_req_locked(dev, req);
    if (status != ZX_OK) {
        goto unlock_out;
    }

    mtx_unlock(&dev->mtx);

    completion_wait(&dev->req_completion, ZX_TIME_INFINITE);

    imx_sdhci_finish_req(dev, req);

    completion_reset(&dev->req_completion);

    return req->status;

unlock_out:
    mtx_unlock(&dev->mtx);
    imx_sdhci_finish_req(dev, req);
    return status;
}


static sdmmc_protocol_ops_t sdmmc_proto = {
    .host_info = imx_sdhci_host_info,
    .set_signal_voltage = imx_sdhci_set_signal_voltage,
    .set_bus_width = imx_sdhci_set_bus_width,
    .set_bus_freq = imx_sdhci_set_bus_freq,
    .set_timing = imx_sdhci_set_timing,
    .hw_reset = imx_sdhci_hw_reset,
    .perform_tuning = imx_sdhci_perform_tuning,
    .request = imx_sdhci_request,
};

static void imx_sdhci_unbind(void* ctx) {
    imx_sdhci_device_t* dev = ctx;
    device_remove(dev->zxdev);
}

static void imx_sdhci_release(void* ctx) {
    imx_sdhci_device_t* dev = ctx;
    if (dev->regs != NULL) {
        zx_handle_close(dev->regs_handle);
    }
    zx_handle_close(dev->bti_handle);
    free(dev);
}

static zx_protocol_device_t imx_sdhci_device_proto = {
    .version = DEVICE_OPS_VERSION,
    .unbind = imx_sdhci_unbind,
    .release = imx_sdhci_release,

};


static zx_status_t imx_sdhci_bind(void* ctx, zx_device_t* parent) {
    zx_status_t status;

    imx_sdhci_device_t* dev = calloc(1, sizeof(imx_sdhci_device_t));
    if (!dev) {
        return ZX_ERR_NO_MEMORY;
    }

    status = device_get_protocol(parent, ZX_PROTOCOL_PLATFORM_DEV, &dev->pdev);
    if (status != ZX_OK) {
        SDHCI_ERROR("ZX_PROTOCOL_PLATFORM_DEV not available %d \n", status);
        goto fail;
    }

    status = pdev_map_mmio_buffer(&dev->pdev, 0, ZX_CACHE_POLICY_UNCACHED_DEVICE,
                                    &dev->mmios);
    if (status != ZX_OK) {
        SDHCI_ERROR("pdev_map_mmio_buffer failed %d\n", status);
        goto fail;
    }

    // hook up mmio to dev->regs
    dev->regs = io_buffer_virt(&dev->mmios);

    status = pdev_get_bti(&dev->pdev, 0, &dev->bti_handle);
    if (status != ZX_OK) {
        SDHCI_ERROR("Could not get BTI handle %d\n", status);
        goto fail;
    }

    status = pdev_map_interrupt(&dev->pdev, 0, &dev->irq_handle);
    if (status != ZX_OK) {
        SDHCI_ERROR("pdev_map_interrupt failed %d\n", status);
        goto fail;
    }

    thrd_t irq_thread;
    if (thrd_create_with_name(&irq_thread, imx_sdhci_irq_thread,
                                        dev, "imx_sdhci_irq_thread") != thrd_success) {
        SDHCI_ERROR("Failed to create irq thread\n");
    }
    thrd_detach(irq_thread);

    dev->base_clock = 200000000; // TODO: Better way of doing this obviously

    uint32_t caps0 = dev->regs->host_ctrl_cap;
    SDHCI_ERROR("caps = 0x%x\n", caps0);

    dev->info.caps |= SDMMC_HOST_CAP_BUS_WIDTH_8;
    if (caps0 & SDHCI_CORECFG_3P3_VOLT_SUPPORT) {
        dev->info.caps |= SDMMC_HOST_CAP_VOLTAGE_330;
    }


    // Reset host controller
    dev->regs->sys_ctrl |= IMX_SDHC_SYS_CTRL_RSTA;
    if (imx_sdhci_wait_for_reset(dev, IMX_SDHC_SYS_CTRL_RSTA, ZX_SEC(1)) != ZX_OK) {
        SDHCI_ERROR("Did not recover from reset 0x%x\n", dev->regs->sys_ctrl);
        goto fail;
    }

    // RSTA does not reset MMC_BOOT register. so do it manually
    dev->regs->mmc_boot = 0;

    // Reset MIX_CTRL and CLK_TUNE_CTRL regs to 0 as well
    dev->regs->mix_ctrl = 0;
    dev->regs->clk_tune_ctrl_status = 0;

    // disable DLL_CTRL delay lie
    dev->regs->dll_ctrl = 0;

    // dev->regs->prot_ctrl = 0x00000020

    // no dma for now
    dev->info.max_transfer_size = 0;

    // enable clocks
    imx_sdhci_set_bus_freq(dev, SD_FREQ_SETUP_HZ);

    dev->regs->sys_ctrl |= (0xe << 16);

    zx_nanosleep(zx_deadline_after(ZX_MSEC(100)));

    // Disable all interrupts
    dev->regs->int_signal_en = 0;
    dev->regs->int_status = 0xffffffff;


    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "imx-sdhci",
        .ctx = dev,
        .ops = &imx_sdhci_device_proto,
        .proto_id = ZX_PROTOCOL_SDMMC,
        .proto_ops = &sdmmc_proto,
    };

    status = device_add(parent, &args, &dev->zxdev);
    if (status != ZX_OK) {
        SDHCI_ERROR("device_add failed %d\n", status);
        goto fail;
    }

    return ZX_OK;

fail:
    imx_sdhci_release(dev);
    return status;
}

static zx_driver_ops_t imx_sdhci_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = imx_sdhci_bind,
};

ZIRCON_DRIVER_BEGIN(imx_sdhci, imx_sdhci_driver_ops, "zircon", "0.1", 4)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_PLATFORM_DEV),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_NXP),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_DID, PDEV_DID_IMX_SDHCI),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_PID, PDEV_PID_IMX8MEVK),
ZIRCON_DRIVER_END(imx_sdhci)
