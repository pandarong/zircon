// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddk/protocol/platform-bus.h>
#include <ddk/io-buffer.h>

#include <sync/completion.h>
#include <zircon/listnode.h>

#include "a113-bus.h"


#define I2CB_BASE_PHYS 0xffd1e000
#define I2CB_BASE_PAGE ((~0xFFF) & I2CB_BASE_PHYS)
#define I2CB_BASE_PAGE_OFFSET (I2CB_BASE_PHYS - I2CB_BASE_PAGE)

typedef struct aml_i2c_dev aml_i2c_dev_t;

typedef enum {
    AML_I2C_B,
} aml_i2c_port_t;

typedef enum {
    TOKEN_END,
    TOKEN_START,
    TOKEN_SLAVE_ADDR_WR,
    TOKEN_SLAVE_ADDR_RD,
    TOKEN_DATA,
    TOKEN_DATA_LAST,
    TOKEN_STOP
} aml_i2c_token_t;

typedef volatile struct {
    uint32_t    control;
    uint32_t    slave_addr;
    uint32_t    token_list_0;
    uint32_t    token_list_1;
    uint32_t    token_wdata_0;
    uint32_t    token_wdata_1;
    uint32_t    token_rdata_0;
    uint32_t    token_rdata_1;
} aml_i2c_regs_t;

typedef struct {
    list_node_t    node;
    uint32_t       slave_addr;
    uint32_t       addr_bits;
    uint32_t       timeout;
    aml_i2c_dev_t  *dev;
} aml_i2c_connection_t;

/*
    We have separate tx and rx buffs since a common need with i2c
    is the ability to do a write,read sequence without having another
    transaction on the bus in between the write/read.
*/
typedef struct {
    list_node_t            node;
    uint8_t                tx_buff[8];
    uint32_t               tx_len;
    uint8_t                rx_buff[8];
    uint32_t               rx_len;
    aml_i2c_connection_t   *conn;
    void                   *cb;
} aml_i2c_txn_t;

struct aml_i2c_dev {
    a113_bus_t     *host_bus;
    io_buffer_t    regs_iobuff;
    aml_i2c_regs_t *virt_regs;
    uint32_t       bitrate;
    list_node_t    connections;
    list_node_t    txn_list;
    list_node_t    free_txn_list;
    completion_t   txn_active;
    mtx_t          mutex;
};



zx_status_t aml_i2c_init(aml_i2c_dev_t **device, a113_bus_t *host_bus,
                                                 aml_i2c_port_t portnum);
zx_status_t aml_i2c_dumpstate(aml_i2c_dev_t *dev);
zx_status_t aml_i2c_read(aml_i2c_dev_t *dev, uint8_t *buff, uint32_t len);
zx_status_t aml_i2c_write(aml_i2c_dev_t *dev, uint8_t *buff, uint32_t len);
zx_status_t aml_i2c_set_slave_addr(aml_i2c_dev_t *dev, uint16_t addr);
zx_status_t aml_i2c_start_xfer(aml_i2c_dev_t *dev);
zx_status_t aml_i2c_test(aml_i2c_dev_t *dev);

zx_status_t aml_i2c_connect(aml_i2c_connection_t ** conn,
                             aml_i2c_dev_t *dev,
                             uint32_t i2c_addr,
                             uint32_t num_addr_bits);

zx_status_t aml_i2c_wr_async(aml_i2c_connection_t *conn, uint8_t *buff, uint32_t len, void* cb);
zx_status_t aml_i2c_rd_async(aml_i2c_connection_t *conn, uint8_t *buff, uint32_t len, void* cb);