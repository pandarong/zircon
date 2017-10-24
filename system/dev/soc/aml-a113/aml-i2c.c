// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <threads.h>
#include <unistd.h>

#include <bits/limits.h>
#include <ddk/debug.h>
#include <hw/reg.h>

#include <zircon/assert.h>
#include <zircon/types.h>

#include "aml-i2c.h"
#include "a113-bus.h"

//static aml_i2c_dev_t* i2c_b;
//static aml_i2c_dev_t* i2c_a;

//tester reads from an accelerometer on the i2c bus  @addr 0x18
static int i2c_test_thread(void *arg) {
    aml_i2c_dev_t *dev = arg;
//completion_wait(&completion, ZX_TIME_INFINITE);
    aml_i2c_set_slave_addr(dev,0x18);

    //uint8_t buff[8] = {0,1,2,3,4,5,6,7};

    while (1) {

       // aml_i2c_wr_async(     );//

//        aml_i2c_read(dev,buff,8);

//        uint64_t *data = (uint64_t*)buff;
//        printf("acc data : 0x%016lx\n",*data);
        sleep(1);
    }

    return 0;
}

static int aml_i2c_thread(void *arg) {

    aml_i2c_dev_t *dev = arg;
    aml_i2c_txn_t *txn;

    while (1) {
        while (!list_is_empty(&dev->txn_list)) {
            mtx_lock(&dev->mutex);
            txn = list_remove_tail_type(&dev->txn_list,aml_i2c_txn_t,node);
            mtx_unlock(&dev->mutex);
            uint64_t* temp = (uint64_t*)txn->tx_buff;

            aml_i2c_write(dev,txn->tx_buff,txn->tx_len);
            printf("Worker Got %016lx\n",*temp);
            txn->tx_len=0;
            mtx_lock(&dev->mutex);
            list_add_head(&dev->free_txn_list,&txn->node);
            mtx_unlock(&dev->mutex);
        }
        printf("List empty...waiting\n");
        completion_wait(&dev->txn_active, ZX_TIME_INFINITE);
        completion_reset(&dev->txn_active);
    }
    return 0;
}

zx_status_t aml_i2c_test(aml_i2c_dev_t *dev) {

    thrd_t thrd;
    thrd_create_with_name(&thrd, i2c_test_thread, dev, "i2c_test_thread");
    return ZX_OK;
}

zx_status_t aml_i2c_dumpstate(aml_i2c_dev_t *dev) {

    printf("control reg      : %08x\n",dev->virt_regs->control);
    printf("slave addr  reg  : %08x\n",dev->virt_regs->slave_addr);
    printf("token list0 reg  : %08x\n",dev->virt_regs->token_list_0);
    printf("token list1 reg  : %08x\n",dev->virt_regs->token_list_1);
    printf("token wdata0     : %08x\n",dev->virt_regs->token_wdata_0);
    printf("token wdata1     : %08x\n",dev->virt_regs->token_wdata_1);
    printf("token rdata0     : %08x\n",dev->virt_regs->token_rdata_0);
    printf("token rdata1     : %08x\n",dev->virt_regs->token_rdata_1);

    return ZX_OK;
}

zx_status_t aml_i2c_start_xfer(aml_i2c_dev_t *dev) {

    dev->virt_regs->control &= ~0x00000001;
    dev->virt_regs->control |= 0x00000003;
    return ZX_OK;
}

zx_status_t aml_i2c_wr_async(aml_i2c_connection_t *conn, uint8_t *buff, uint32_t len, void* cb) {

    ZX_DEBUG_ASSERT(conn);
    ZX_DEBUG_ASSERT(len <= 8);

    aml_i2c_txn_t *txn;
    aml_i2c_dev_t *dev = conn->dev;
    ZX_DEBUG_ASSERT(dev);

    mtx_lock(&dev->mutex);

    txn = list_remove_head_type(&dev->free_txn_list, aml_i2c_txn_t, node);
    if (!txn) {
        printf("new txn\n");
        txn = calloc(1, sizeof(aml_i2c_txn_t));
        if (!txn) {
            mtx_unlock(&dev->mutex);
            return ZX_ERR_NO_MEMORY;
        }
    } else {
        printf("recycled txn\n");
    }

    for (uint32_t i = 0; i < len; i++)
        txn->tx_buff[i] = buff[i];
    txn->tx_len = len;
    txn->rx_len = 0;
    txn->cb = cb;
    txn->conn = conn;
//need mutex
    list_add_head(&dev->txn_list, &txn->node);
    mtx_unlock(&dev->mutex);

    return ZX_OK;
}

zx_status_t aml_i2c_write(aml_i2c_dev_t *dev, uint8_t *buff, uint32_t len) {

    ZX_DEBUG_ASSERT(len<=8);  //temporary hack, only transactions that can fit in hw buffer

    uint32_t token_num = 0;
    uint64_t token_reg = 0;

    token_reg |= (uint64_t)TOKEN_START << (4*(token_num++));
    token_reg |= (uint64_t)TOKEN_SLAVE_ADDR_WR << (4*(token_num++));

    for (uint32_t i=0; i < len; i++) {
        token_reg |= (uint64_t)TOKEN_DATA << (4*(token_num++));
    }
    token_reg |= (uint64_t)TOKEN_STOP << (4*(token_num++));

    dev->virt_regs->token_list_0 = (uint32_t)(token_reg & 0xffffffff);
    dev->virt_regs->token_list_1 = (uint32_t)((token_reg >> 32) & 0xffffffff);

    uint64_t wdata = 0;
    for (uint32_t i=0; i < len; i++) {
        wdata |= (uint64_t)buff[i] << (8*i);
    }

    dev->virt_regs->token_wdata_0 = (uint32_t)(wdata & 0xffffffff);
    dev->virt_regs->token_wdata_1 = (uint32_t)((wdata >> 32) & 0xffffffff);

    aml_i2c_start_xfer(dev);

    while (dev->virt_regs->control & 0x4) ;;    // wait for idle

    return ZX_OK;
}

zx_status_t aml_i2c_read(aml_i2c_dev_t *dev, uint8_t *buff, uint32_t len) {

    ZX_DEBUG_ASSERT(len<=8);  //temporary hack, only transactions that can fit in hw buffer

    uint32_t token_num = 0;
    uint64_t token_reg = 0;

    token_reg |= (uint64_t)TOKEN_START << (4*(token_num++));
    token_reg |= (uint64_t)TOKEN_SLAVE_ADDR_RD << (4*(token_num++));

    for (uint32_t i=0; i < (len - 1); i++) {
        token_reg |= (uint64_t)TOKEN_DATA << (4*(token_num++));
    }
    token_reg |= (uint64_t)TOKEN_DATA_LAST << (4*(token_num++));
    token_reg |= (uint64_t)TOKEN_STOP << (4*(token_num++));

    dev->virt_regs->token_list_0 = (uint32_t)(token_reg & 0xffffffff);
    token_reg = token_reg >> 32;
    dev->virt_regs->token_list_1 = (uint32_t)(token_reg);

    //clear registers to prevent data leaking from last xfer
    dev->virt_regs->token_rdata_0 = 0;
    dev->virt_regs->token_rdata_1 = 0;

    aml_i2c_start_xfer(dev);

    while (dev->virt_regs->control & 0x4) ;;    // wait for idle

    uint64_t rdata;
    rdata = dev->virt_regs->token_rdata_0;
    rdata |= (uint64_t)(dev->virt_regs->token_rdata_1) << 32;

    for (uint32_t i=0; i < sizeof(rdata); i++) {
        buff[i] = (uint8_t)((rdata >> (8*i) & 0xff));
    }

    return ZX_OK;
}

zx_status_t aml_i2c_set_slave_addr(aml_i2c_dev_t *dev, uint16_t addr) {

    addr &= 0x7f;
    uint32_t reg = dev->virt_regs->slave_addr;
    reg = reg & 0xff;
    reg = reg | ((addr << 1) & 0xff);
    dev->virt_regs->slave_addr = reg;

    return ZX_OK;
}

zx_status_t aml_i2c_connect(aml_i2c_connection_t **connection,
                             aml_i2c_dev_t *dev,
                             uint32_t i2c_addr,
                             uint32_t num_addr_bits) {

    if ((num_addr_bits != 7) && (num_addr_bits != 10))
        return ZX_ERR_INVALID_ARGS;


    aml_i2c_connection_t *conn;
    // Check if the i2c channel is already in use
    if (!list_is_empty(&dev->connections)) {
        list_for_every_entry(&dev->connections, conn, aml_i2c_connection_t, node) {
            if (conn->slave_addr == i2c_addr) {
                printf("i2c slave address already in use!\n");
                return ZX_ERR_INVALID_ARGS;
            }
        }
    }

    conn = calloc(1, sizeof(aml_i2c_connection_t));
    if (!conn) {
        return ZX_ERR_NO_MEMORY;
    }
    conn->timeout = 1000;
    conn->slave_addr = i2c_addr;
    conn->addr_bits = num_addr_bits;
    conn->dev = dev;

    list_add_head(&dev->connections, &conn->node );
    printf("Added connection for channel %x\n",i2c_addr);
    *connection = conn;
    return ZX_OK;
}

/* create instance of aml_i2c_t and do basic initialization.  There will
be one of these instances for each of the soc i2c ports.
*/
zx_status_t aml_i2c_init(aml_i2c_dev_t **device, a113_bus_t *host_bus,
                                                 aml_i2c_port_t portnum) {

    *device = calloc(1, sizeof(aml_i2c_dev_t));
    if (!(*device)) {
        return ZX_ERR_NO_MEMORY;
    }

    //Initialize the connection list;
    list_initialize(&(*device)->connections);
    list_initialize(&(*device)->txn_list);
    list_initialize(&(*device)->free_txn_list);

    (*device)->txn_active =  COMPLETION_INIT;

    (*device)->host_bus = host_bus;  // TODO - might not need this

    zx_handle_t resource = get_root_resource();
    zx_status_t status;

    status = io_buffer_init_physical(&(*device)->regs_iobuff, I2CB_BASE_PAGE, PAGE_SIZE,
                                     resource, ZX_CACHE_POLICY_UNCACHED_DEVICE);

    if (status != ZX_OK) {
        dprintf(ERROR, "aml_i2c_init: io_buffer_init_physical failed %d\n", status);
        goto init_fail;
    }

    (*device)->virt_regs = (aml_i2c_regs_t*)(io_buffer_virt(&(*device)->regs_iobuff));

    thrd_t thrd;
    thrd_create_with_name(&thrd, aml_i2c_thread, *device, "i2c_thread");

    return ZX_OK;

init_fail:
    if (*device) {
        io_buffer_release(&(*device)->regs_iobuff);
        free(*device);
     };
    return status;
}
