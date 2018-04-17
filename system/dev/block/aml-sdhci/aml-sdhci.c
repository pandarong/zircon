// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <bits/limits.h>
#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/io-buffer.h>
#include <ddk/protocol/platform-bus.h>
#include <ddk/protocol/platform-defs.h>
#include <ddk/protocol/platform-device.h>
#include <ddk/protocol/gpio.h>
#include <ddk/io-buffer.h>
#include <hw/reg.h>
#include <pretty/hexdump.h>
#include <soc/aml-common/aml-sdhci.h>
#include <ddk/protocol/sdmmc.h>
#include <hw/sdmmc.h>

#include <zircon/assert.h>
#include <zircon/types.h>

#define DMA_DESC_COUNT          512
#define SDHCI_TRACE(fmt, ...) zxlogf(TRACE, "%s: " fmt, __func__, ##__VA_ARGS__)
#define SDHCI_INFO(fmt, ...) zxlogf(INFO, "%s: "fmt, __func__, ##__VA_ARGS__)
#define SDHCI_ERROR(fmt, ...) zxlogf(ERROR, "%s: " fmt, __func__, ##__VA_ARGS__)

static void aml_sdhci_dump_status(uint32_t status);
static void aml_sdhci_dump_cfg(uint32_t config);
static void aml_sdhci_dump_clock(uint32_t clock);

static void aml_sdhci_dump_regs(aml_sdhci_t* sdhci) {
    aml_sdhci_regs_t* regs = (aml_sdhci_regs_t*)io_buffer_virt(&sdhci->mmio);
    SDHCI_TRACE("sd_emmc_clock : 0x%x\n", regs->sd_emmc_clock);
    aml_sdhci_dump_clock(regs->sd_emmc_clock);
    SDHCI_TRACE("sd_emmc_delay1 : 0x%x\n", regs->sd_emmc_delay1);
    SDHCI_TRACE("sd_emmc_delay2 : 0x%x\n", regs->sd_emmc_delay2);
    SDHCI_TRACE("sd_emmc_adjust : 0x%x\n", regs->sd_emmc_adjust);
    SDHCI_TRACE("sd_emmc_calout : 0x%x\n", regs->sd_emmc_calout);
    SDHCI_TRACE("sd_emmc_start : 0x%x\n", regs->sd_emmc_start);
    SDHCI_TRACE("sd_emmc_cfg : 0x%x\n", regs->sd_emmc_cfg);
    aml_sdhci_dump_cfg(regs->sd_emmc_cfg);
    SDHCI_TRACE("sd_emmc_status : 0x%x\n", regs->sd_emmc_status);
    aml_sdhci_dump_status(regs->sd_emmc_status);
    SDHCI_TRACE("sd_emmc_irq_en : 0x%x\n", regs->sd_emmc_irq_en);
    SDHCI_TRACE("sd_emmc_cmd_cfg : 0x%x\n", regs->sd_emmc_cmd_cfg);
    SDHCI_TRACE("sd_emmc_cmd_arg : 0x%x\n", regs->sd_emmc_cmd_arg);
    SDHCI_TRACE("sd_emmc_cmd_dat : 0x%x\n", regs->sd_emmc_cmd_dat);
    SDHCI_TRACE("sd_emmc_cmd_rsp : 0x%x\n", regs->sd_emmc_cmd_rsp);
    SDHCI_TRACE("sd_emmc_cmd_rsp1 : 0x%x\n", regs->sd_emmc_cmd_rsp1);
    SDHCI_TRACE("sd_emmc_cmd_rsp2 : 0x%x\n", regs->sd_emmc_cmd_rsp2);
    SDHCI_TRACE("sd_emmc_cmd_rsp3 : 0x%x\n", regs->sd_emmc_cmd_rsp3);
    SDHCI_TRACE("bus_err : 0x%x\n", regs->bus_err);
    SDHCI_TRACE("sd_emmc_curr_cfg: 0x%x\n", regs->sd_emmc_curr_cfg);
    SDHCI_TRACE("sd_emmc_curr_arg: 0x%x\n", regs->sd_emmc_curr_arg);
    SDHCI_TRACE("sd_emmc_curr_dat: 0x%x\n", regs->sd_emmc_curr_dat);
    SDHCI_TRACE("sd_emmc_curr_rsp: 0x%x\n", regs->sd_emmc_curr_rsp);
    SDHCI_TRACE("sd_emmc_next_cfg: 0x%x\n", regs->sd_emmc_curr_cfg);
    SDHCI_TRACE("sd_emmc_next_arg: 0x%x\n", regs->sd_emmc_curr_arg);
    SDHCI_TRACE("sd_emmc_next_dat: 0x%x\n", regs->sd_emmc_curr_dat);
    SDHCI_TRACE("sd_emmc_next_rsp: 0x%x\n", regs->sd_emmc_curr_rsp);
    SDHCI_TRACE("sd_emmc_rxd : 0x%x\n", regs->sd_emmc_rxd);
    SDHCI_TRACE("sd_emmc_txd : 0x%x\n", regs->sd_emmc_txd);
    SDHCI_TRACE("sramDesc : %p\n",regs->sramDesc);
    SDHCI_TRACE("ping : %p\n", regs->ping);
    SDHCI_TRACE("pong : %p\n", regs->pong);
}

static void aml_sdhci_dump_status(uint32_t status) {
    uint32_t rxd_err = get_bits(status, SD_EMMC_STATUS_RXD_ERR_MASK, SD_EMMC_STATUS_RXD_ERR_LOC);
    SDHCI_TRACE("Dumping sd_emmc_status 0x%0x\n", status);
    SDHCI_TRACE("    RXD_ERR: %d\n", rxd_err);
    SDHCI_TRACE("    TXD_ERR: %d\n", get_bit(status, SD_EMMC_STATUS_TXD_ERR));
    SDHCI_TRACE("    DESC_ERR: %d\n", get_bit(status, SD_EMMC_STATUS_DESC_ERR));
    SDHCI_TRACE("    RESP_ERR: %d\n", get_bit(status, SD_EMMC_STATUS_RESP_ERR));
    SDHCI_TRACE("    RESP_TIMEOUT: %d\n", get_bit(status, SD_EMMC_STATUS_RESP_TIMEOUT));
    SDHCI_TRACE("    DESC_TIMEOUT: %d\n", get_bit(status, SD_EMMC_STATUS_DESC_TIMEOUT));
    SDHCI_TRACE("    END_OF_CHAIN: %d\n", get_bit(status, SD_EMMC_STATUS_END_OF_CHAIN));
    SDHCI_TRACE("    DESC_IRQ: %d\n", get_bit(status, SD_EMMC_STATUS_RESP_STATUS));
    SDHCI_TRACE("    IRQ_SDIO: %d\n", get_bit(status, SD_EMMC_STATUS_IRQ_SDIO));
    SDHCI_TRACE("    DAT_I: %d\n", get_bits(status, SD_EMMC_STATUS_DAT_I_MASK,
                                            SD_EMMC_STATUS_DAT_I_LOC));
    SDHCI_TRACE("    CMD_I: %d\n", get_bit(status, SD_EMMC_STATUS_CMD_I));
    SDHCI_TRACE("    DS: %d\n", get_bit(status, SD_EMMC_STATUS_DS));
    SDHCI_TRACE("    BUS_FSM: %d\n", get_bits(status, SD_EMMC_STATUS_BUS_FSM_MASK,
                                              SD_EMMC_STATUS_BUS_FSM_LOC));
    SDHCI_TRACE("    BUS_DESC_BUSY: %d\n", get_bit(status, SD_EMMC_STATUS_BUS_DESC_BUSY));
    SDHCI_TRACE("    CORE_RDY: %d\n", get_bit(status, SD_EMMC_STATUS_BUS_CORE_BUSY));
}

static void aml_sdhci_dump_cfg(uint32_t config) {
    SDHCI_TRACE("Dumping sd_emmc_cfg 0x%0x\n", config);
    SDHCI_TRACE("    BUS_WIDTH: %d\n", get_bits(config, SD_EMMC_CFG_BUS_WIDTH_MASK,
                                                SD_EMMC_CFG_BUS_WIDTH_LOC));
    SDHCI_TRACE("    DDR: %d\n", get_bit(config, SD_EMMC_CFG_DDR));
    SDHCI_TRACE("    DC_UGT: %d\n", get_bit(config, SD_EMMC_CFG_DC_UGT));
    SDHCI_TRACE("    BLOCK LEN: %d\n", get_bits(config, SD_EMMC_CFG_BL_LEN_MASK,
                                                SD_EMMC_CFG_BL_LEN_LOC));
}

static void aml_sdhci_dump_clock(uint32_t clock) {
    SDHCI_TRACE("Dumping clock 0x%0x\n", clock);
    SDHCI_TRACE("   DIV: %x\n", get_bits(clock, SD_EMMC_CLOCK_CFG_DIV_MASK,
                                            SD_EMMC_CLOCK_CFG_DIV_LOC));
    SDHCI_TRACE("   SRC: %d\n", get_bits(clock, SD_EMMC_CLOCK_CFG_SRC_MASK,
                                            SD_EMMC_CLOCK_CFG_SRC_LOC));
    SDHCI_TRACE("   CORE_PHASE: %d\n", get_bits(clock, SD_EMMC_CLOCK_CFG_CO_PHASE_MASK,
                                                   SD_EMMC_CLOCK_CFG_CO_PHASE_LOC));
    SDHCI_TRACE("   TX_PHASE: %d\n", get_bits(clock, SD_EMMC_CLOCK_CFG_TX_PHASE_MASK,
                                                 SD_EMMC_CLOCK_CFG_TX_PHASE_LOC));
    SDHCI_TRACE("   RX_PHASE: %d\n", get_bits(clock, SD_EMMC_CLOCK_CFG_RX_PHASE_MASK,
                                                 SD_EMMC_CLOCK_CFG_RX_PHASE_LOC));
    SDHCI_TRACE("   TX_DELAY: %d\n", get_bits(clock, SD_EMMC_CLOCK_CFG_TX_DELAY_MASK,
                                                 SD_EMMC_CLOCK_CFG_TX_DELAY_LOC));
    SDHCI_TRACE("   RX_DELAY: %d\n", get_bits(clock, SD_EMMC_CLOCK_CFG_RX_DELAY_MASK,
                                                 SD_EMMC_CLOCK_CFG_RX_DELAY_LOC));
    SDHCI_TRACE("   ALWAYS_ON: %d\n", get_bit(clock, SD_EMMC_CLOCK_CFG_ALWAYS_ON));
}

static void aml_sdhci_release(void* ctx) {
    aml_sdhci_t* sdhci = ctx;
    io_buffer_release(&sdhci->mmio);
    zx_handle_close(sdhci->bti);
    zx_handle_close(sdhci->irq_handle);
    free(sdhci);
}

static zx_status_t aml_sdhci_host_info(void* ctx, sdmmc_host_info_t* info) {
    aml_sdhci_t *sdhci = (aml_sdhci_t *)ctx;
    memcpy(info, &sdhci->info, sizeof(sdhci->info));
    return ZX_OK;
}

static zx_status_t aml_sdhci_set_bus_width(void* ctx, uint32_t bw) {
    aml_sdhci_t *sdhci = (aml_sdhci_t *)ctx;
    aml_sdhci_regs_t* regs = (aml_sdhci_regs_t*)io_buffer_virt(&sdhci->mmio);
    uint32_t config = regs->sd_emmc_cfg;

    switch (bw) {
    case SDMMC_BUS_WIDTH_1:
        update_bits(&config, SD_EMMC_CFG_BUS_WIDTH_MASK, SD_EMMC_CFG_BUS_WIDTH_LOC,
                    SD_EMMC_CFG_BUS_WIDTH_1BIT);
        break;
    case SDMMC_BUS_WIDTH_4:
        update_bits(&config, SD_EMMC_CFG_BUS_WIDTH_MASK, SD_EMMC_CFG_BUS_WIDTH_LOC,
                    SD_EMMC_CFG_BUS_WIDTH_4BIT);
        break;
    case SDMMC_BUS_WIDTH_8:
        update_bits(&config, SD_EMMC_CFG_BUS_WIDTH_MASK, SD_EMMC_CFG_BUS_WIDTH_LOC,
                    SD_EMMC_CFG_BUS_WIDTH_8BIT);
        break;
    default:
        return ZX_ERR_OUT_OF_RANGE;
    }

    regs->sd_emmc_cfg = config;
    return ZX_OK;
}

static zx_status_t aml_sdhci_perform_tuning(void* ctx) {
    //TODO: Do the tuning here
    return ZX_OK;
}

static zx_status_t aml_sdhci_set_bus_freq(void* ctx, uint32_t freq) {
    aml_sdhci_t *sdhci = (aml_sdhci_t *)ctx;
    aml_sdhci_regs_t* regs = (aml_sdhci_regs_t*)io_buffer_virt(&sdhci->mmio);

    uint32_t clk = 0, clk_src = 0, clk_div = 0;
    uint32_t clk_val = regs->sd_emmc_clock;

    uint32_t config = regs->sd_emmc_cfg;

    if (freq == 0) {
        //TODO: Disable clock here
    } else if (freq < 20000000) {
        clk_src = 0;
        clk = CLK_SRC_24MHZ;
    } else {
        clk_src = 1;
        clk = CLK_SRC_FCLK_DIV2;
    }

    if (freq > sdhci->f_max)
        freq = sdhci->f_max;
    if (freq < sdhci->f_min)
        freq = sdhci->f_min;

    clk_div = clk/freq;

    if (get_bit(config, SD_EMMC_CFG_DDR)) {
        if (clk_div & 0x01) {
            clk_div++;
        }
        clk_div /= 2;
    }

    update_bits(&clk_val, SD_EMMC_CLOCK_CFG_DIV_MASK, SD_EMMC_CLOCK_CFG_DIV_LOC, clk_div);
    update_bits(&clk_val, SD_EMMC_CLOCK_CFG_SRC_MASK, SD_EMMC_CLOCK_CFG_SRC_LOC, clk_src);
    update_bits(&clk_val, SD_EMMC_CLOCK_CFG_CO_PHASE_MASK, SD_EMMC_CLOCK_CFG_CO_PHASE_LOC, 2);
    update_bits(&clk_val, SD_EMMC_CLOCK_CFG_RX_PHASE_MASK, SD_EMMC_CLOCK_CFG_RX_PHASE_LOC, 0);
    //update_bits(&clk_val, SD_EMMC_CLOCK_CFG_TX_PHASE_MASK, SD_EMMC_CLOCK_CFG_TX_PHASE_LOC, 2);
    update_bits(&clk_val, SD_EMMC_CLOCK_CFG_RX_DELAY_MASK, SD_EMMC_CLOCK_CFG_RX_DELAY_LOC, 0);
    clk_val |= SD_EMMC_CLOCK_CFG_ALWAYS_ON;

    regs->sd_emmc_clock = clk_val;
    return ZX_OK;
}

static void aml_sdhci_hw_reset(void* ctx) {
    aml_sdhci_t *sdhci = (aml_sdhci_t *)ctx;
    aml_sdhci_regs_t* regs = (aml_sdhci_regs_t*)io_buffer_virt(&sdhci->mmio);
    if (sdhci->gpio_count == 1) {
        //Currently we only have 1 gpio
        gpio_config(&sdhci->gpio, 0, GPIO_DIR_OUT);
        gpio_write(&sdhci->gpio, 0, 0);
        usleep(10 * 1000);
        gpio_write(&sdhci->gpio, 0, 1);
    }
    aml_sdhci_set_bus_width(ctx, 0);
    aml_sdhci_set_bus_freq(ctx, 1);
    uint32_t config = regs->sd_emmc_cfg;

    update_bits(&config, SD_EMMC_CFG_BL_LEN_MASK, SD_EMMC_CFG_BL_LEN_LOC, 9);
    update_bits(&config, SD_EMMC_CFG_RESP_TIMEOUT_MASK, SD_EMMC_CFG_RESP_TIMEOUT_LOC, 8);
    update_bits(&config, SD_EMMC_CFG_RC_CC_MASK, SD_EMMC_CFG_RC_CC_LOC, 4);
}

static zx_status_t aml_sdhci_set_bus_timing(void* ctx, sdmmc_timing_t timing) {
    aml_sdhci_t* sdhci = ctx;
    aml_sdhci_regs_t* regs = (aml_sdhci_regs_t*)io_buffer_virt(&sdhci->mmio);

    uint32_t config = regs->sd_emmc_cfg;
    uint32_t clk_val = regs->sd_emmc_clock;

    zxlogf(INFO, "Setting bus timing to %d\n", timing);
    if (timing == SDMMC_TIMING_HS400 || timing == SDMMC_TIMING_HSDDR) {
        if (timing == SDMMC_TIMING_HS400) {
            config |= SD_EMMC_CFG_CHK_DS;
        } else {
            config &= ~SD_EMMC_CFG_CHK_DS;
        }
        config |= SD_EMMC_CFG_DDR;
        uint32_t clk_div = get_bits(clk_val, SD_EMMC_CLOCK_CFG_DIV_MASK, SD_EMMC_CLOCK_CFG_DIV_LOC);
        if (clk_div & 0x01) {
            clk_div++;
        }
        clk_div /= 2;
        update_bits(&clk_val, SD_EMMC_CLOCK_CFG_DIV_MASK, SD_EMMC_CLOCK_CFG_DIV_LOC, clk_div);
    } else {
        config &= ~SD_EMMC_CFG_DDR;
    }

    regs->sd_emmc_cfg = config;
    return ZX_OK;
}

static zx_status_t aml_sdhci_set_signal_voltage(void* ctx, sdmmc_voltage_t voltage) {
    //Amlogic controller does not allow to modify voltage
    return ZX_OK;
}

zx_status_t aml_sdhci_request(void *ctx, sdmmc_req_t* req) {
    zx_status_t status = ZX_OK;
    uint32_t status_irq;
    aml_sdhci_t *sdhci = (aml_sdhci_t *)ctx;
    aml_sdhci_desc_t* desc = calloc(1, sizeof(aml_sdhci_desc_t));
    uint32_t cmd = 0;

    aml_sdhci_regs_t* regs = (aml_sdhci_regs_t*)io_buffer_virt(&sdhci->mmio);

    desc->cmd_arg = req->arg;
    if (req->cmd_flags == 0) {
        cmd |= SD_EMMC_CMD_INFO_NO_RESP;
    } else {
        if (req->cmd_flags & SDMMC_RESP_LEN_136) {
            cmd |= SD_EMMC_CMD_INFO_RESP_128;
        }

        if (!(req->cmd_flags & SDMMC_RESP_CRC_CHECK)){
            cmd |= SD_EMMC_CMD_INFO_RESP_NO_CRC;
        }

        if (req->cmd_flags & SDMMC_RESP_BUSY) {
            cmd |= SD_EMMC_CMD_INFO_R1B;
        }

        desc->resp_addr = (unsigned long)req->response;
        cmd &= ~SD_EMMC_CMD_INFO_RESP_NUM;
    }

    if (req->cmd_flags & SDMMC_RESP_DATA_PRESENT) {
        cmd |= SD_EMMC_CMD_INFO_DATA_IO;
        status = io_buffer_init(&sdhci->desc_buffer, sdhci->bti, 2 * PAGE_SIZE,
                                IO_BUFFER_RW | IO_BUFFER_CONTIG);
        if (status != ZX_OK) {
            goto fail;
        }

        zx_paddr_t buffer_phys = io_buffer_phys(&sdhci->desc_buffer);
        io_buffer_cache_flush_invalidate(&sdhci->desc_buffer, 0, 2 * PAGE_SIZE);

        if (!(req->cmd_flags & SDMMC_CMD_READ)) {
            cmd |= SD_EMMC_CMD_INFO_DATA_WR;
            memcpy((void *)(io_buffer_virt(&sdhci->desc_buffer)), req->virt,
                   req->blockcount * req->blocksize);
        }

        if (req->blockcount > 1) {
            cmd |= SD_EMMC_CMD_INFO_BLOCK_MODE;
            update_bits(&cmd, SD_EMMC_CMD_INFO_LEN_MASK, SD_EMMC_CMD_INFO_LEN_LOC, req->blockcount);
        } else{
            update_bits(&cmd, SD_EMMC_CMD_INFO_LEN_MASK, SD_EMMC_CMD_INFO_LEN_LOC, req->blocksize);
        }
        desc->data_addr = (uint32_t)buffer_phys;
        /* DDR ADDRESS */
        desc->data_addr &= ~(1<<0);
    }

    update_bits(&cmd, SD_EMMC_CMD_INFO_CMD_IDX_MASK, SD_EMMC_CMD_INFO_CMD_IDX_LOC,
                (0x80 | req->cmd_idx));
    cmd |= SD_EMMC_CMD_INFO_OWNER;
    cmd |= SD_EMMC_CMD_INFO_END_OF_CHAIN;
    desc->cmd_info = cmd;

    // TODO(ravoorir): Use DMA descriptors to queue multiple commands
    SDHCI_TRACE("SUBMIT cmd_idx: %d cmd_cfg: 0x%x cmd_dat: 0x%x cmd_arg: 0x%x\n",
                get_bits(cmd, SD_EMMC_CMD_INFO_CMD_IDX_MASK, SD_EMMC_CMD_INFO_CMD_IDX_LOC),
                desc->cmd_info, desc->data_addr, desc->cmd_arg);
    regs->sd_emmc_status = AML_SDHCI_IRQ_ALL_CLEAR;
    regs->sd_emmc_cmd_cfg = desc->cmd_info;
    regs->sd_emmc_cmd_dat = desc->data_addr;
    regs->sd_emmc_cmd_arg = desc->cmd_arg;

    // TODO(ravoorir): Complete requests asynchronously on a different thread.
    while (1) {
        status_irq = regs->sd_emmc_status;
        if (status_irq & SD_EMMC_STATUS_END_OF_CHAIN) {
            break;
        }
    }

    uint32_t rxd_err = get_bits(status_irq, SD_EMMC_STATUS_RXD_ERR_MASK, SD_EMMC_STATUS_RXD_ERR_LOC);
    if (rxd_err) {
        SDHCI_ERROR("RX Data CRC Error cmd%d, status=0x%x, RXD_ERR:%d\n", req->cmd_idx, status_irq,
                    rxd_err);
        //aml_sdhci_dump_regs(sdhci);
        status = ZX_ERR_IO_DATA_INTEGRITY;
        goto fail;
    }
    if (status_irq & SD_EMMC_STATUS_TXD_ERR) {
        SDHCI_ERROR("TX Data CRC Error, cmd%d, status=0x%x TXD_ERR\n", req->cmd_idx, status_irq);
        //aml_sdhci_dump_regs(sdhci);
        status = ZX_ERR_IO_DATA_INTEGRITY;
        goto fail;
    }
    if (status_irq & SD_EMMC_STATUS_DESC_ERR) {
        SDHCI_ERROR("Controller does not own the descriptor, cmd%d, status=0x%x\n", req->cmd_idx,
                    status_irq);
        //aml_sdhci_dump_regs(sdhci);
        status = ZX_ERR_IO_INVALID;
        goto fail;
    }
    if (status_irq & SD_EMMC_STATUS_RESP_ERR) {
        SDHCI_ERROR("Response CRC Error, cmd%d, status=0x%x\n", req->cmd_idx, status_irq);
        status = ZX_ERR_IO_DATA_INTEGRITY;
        //aml_sdhci_dump_regs(sdhci);
        goto fail;
    }
    if (status_irq & SD_EMMC_STATUS_RESP_TIMEOUT) {
        SDHCI_ERROR("No response reived before time limit, cmd%d, status=0x%x\n",
                req->cmd_idx, status_irq);
        //aml_sdhci_dump_regs(sdhci);
        status = ZX_ERR_TIMED_OUT;
        goto fail;
    }
    if (status_irq & SD_EMMC_STATUS_DESC_TIMEOUT) {
        SDHCI_ERROR("Descriptor execution timed out, cmd%d, status=0x%x\n", req->cmd_idx,
                    status_irq);
        //aml_sdhci_dump_regs(sdhci);
        status = ZX_ERR_TIMED_OUT;
        goto fail;
    }
    if (status_irq & SD_EMMC_STATUS_BUS_CORE_BUSY) {
        SDHCI_ERROR("Core is busy, cmd%d, status=0x%x\n", req->cmd_idx, status_irq);
        //aml_sdhci_dump_regs(sdhci);
        status = ZX_ERR_SHOULD_WAIT;
        goto fail;
    }

    if (req->cmd_flags & SDMMC_RESP_LEN_136) {
        req->response[0] = regs->sd_emmc_cmd_rsp;
        req->response[1] = regs->sd_emmc_cmd_rsp1;
        req->response[2] = regs->sd_emmc_cmd_rsp2;
        req->response[3] = regs->sd_emmc_cmd_rsp3;
    } else {
        req->response[0] = regs->sd_emmc_cmd_rsp;
    }

    if (req->cmd_flags & SDMMC_CMD_READ) {
        memcpy(req->virt, (void *)(io_buffer_virt(&sdhci->desc_buffer)),
               req->blockcount * req->blocksize);
        //hexdump8_ex(req->virt, 4096, 0);
        //aml_sdhci_dump_status(regs->sd_emmc_status);
    }

fail:
    free(desc);
    io_buffer_release(&sdhci->desc_buffer);
    return status;
}

static zx_protocol_device_t sdhci_device_proto = {
    .version = DEVICE_OPS_VERSION,
    .release = aml_sdhci_release,
};

static sdmmc_protocol_ops_t aml_sdmmc_proto = {
    .host_info = aml_sdhci_host_info,
    .set_signal_voltage = aml_sdhci_set_signal_voltage,
    .set_bus_width = aml_sdhci_set_bus_width,
    .set_bus_freq = aml_sdhci_set_bus_freq,
    .set_timing = aml_sdhci_set_bus_timing,
    .hw_reset = aml_sdhci_hw_reset,
    .perform_tuning = aml_sdhci_perform_tuning,
    .request = aml_sdhci_request,
};

static zx_status_t aml_sdhci_bind(void* ctx, zx_device_t* parent) {
    aml_sdhci_t* sdhci = calloc(1, sizeof(aml_sdhci_t));
    if (!sdhci) {
        zxlogf(ERROR, "aml-sdhci_bind: out of memory\n");
        return ZX_ERR_NO_MEMORY;
    }

    zx_status_t status = ZX_OK;
    if ((status = device_get_protocol(parent, ZX_PROTOCOL_PLATFORM_DEV, &sdhci->pdev)) != ZX_OK) {
        zxlogf(ERROR, "aml_sdhci_bind: ZX_PROTOCOL_PLATFORM_DEV not available\n");
        goto fail;
    }

    if ((status = device_get_protocol(parent, ZX_PROTOCOL_GPIO, &sdhci->gpio)) != ZX_OK) {
        zxlogf(ERROR, "aml_sdhci_bind: ZX_PROTOCOL_GPIO not available\n");
        goto fail;
    }

    pdev_device_info_t info;
    status = pdev_get_device_info(&sdhci->pdev, &info);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_sdhci_bind: pdev_get_device_info failed\n");
        goto fail;
    }
    if (info.mmio_count != info.irq_count) {
         zxlogf(ERROR, "aml_sdhci_bind: mmio_count %u does not match irq_count %u\n",
               info.mmio_count, info.irq_count);
        status = ZX_ERR_INVALID_ARGS;
        goto fail;
    }

    sdhci->gpio_count = info.gpio_count;
    sdhci->info.caps = SDMMC_HOST_CAP_BUS_WIDTH_8 | SDMMC_HOST_CAP_VOLTAGE_330;
    sdhci->info.max_transfer_size = 1024;
    sdhci->f_min = AML_SDHCI_MIN_FREQ;
    sdhci->f_max = AML_SDHCI_MAX_FREQ;

    status = pdev_get_bti(&sdhci->pdev, 0, &sdhci->bti);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_sdhci_bind: pdev_get_bti failed\n");
        goto fail;
    }

    status = pdev_map_mmio_buffer(&sdhci->pdev, 0, ZX_CACHE_POLICY_UNCACHED_DEVICE, &sdhci->mmio);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_sdhci_bind: pdev_map_mmio_buffer failed %d\n", status);
        goto fail;
    }

    status = pdev_map_interrupt(&sdhci->pdev, 0, &sdhci->irq_handle);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_sdhci_bind: pdev_map_interrupt failed %d\n", status);
        return status;
    }

    // Create the device.
    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "aml-sdhci",
        .ctx = sdhci,
        .ops = &sdhci_device_proto,
        .proto_id = ZX_PROTOCOL_SDMMC,
        .proto_ops = &aml_sdmmc_proto,
    };

    status = device_add(parent, &args, &sdhci->zxdev);
    if (status != ZX_OK) {
        goto fail;
    }
    return ZX_OK;

fail:
    aml_sdhci_release(sdhci);
    return status;
}


static zx_driver_ops_t aml_sdhci_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = aml_sdhci_bind,
};

ZIRCON_DRIVER_BEGIN(aml_sdhci, aml_sdhci_driver_ops, "zircon", "0.1", 3)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_PLATFORM_DEV),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_AMLOGIC),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_DID, PDEV_DID_AMLOGIC_SDHCI),
ZIRCON_DRIVER_END(aml_sdhci)
