// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/platform-defs.h>

#include <zircon/process.h>
#include <zircon/syscalls.h>
#include <zircon/assert.h>

#include "a113-bus.h"
#include "a113-clocks.h"
#include "a113-hw.h"
#include "aml-i2c.h"
#include <hw/reg.h>

static zx_status_t a113_get_initial_mode(void* ctx, usb_mode_t* out_mode) {
    *out_mode = USB_MODE_HOST;
    return ZX_OK;
}

static zx_status_t a113_set_mode(void* ctx, usb_mode_t mode) {
    a113_bus_t* bus = ctx;
    return a113_usb_set_mode(bus, mode);
}

usb_mode_switch_protocol_ops_t usb_mode_switch_ops = {
    .get_initial_mode = a113_get_initial_mode,
    .set_mode = a113_set_mode,
};

static zx_status_t a113_bus_get_protocol(void* ctx, uint32_t proto_id, void* out) {
    a113_bus_t* bus = ctx;

    switch (proto_id) {
    case ZX_PROTOCOL_USB_MODE_SWITCH:
        memcpy(out, &bus->usb_mode_switch, sizeof(bus->usb_mode_switch));
        return ZX_OK;
    default:
        return ZX_ERR_NOT_SUPPORTED;
    }
}

static pbus_interface_ops_t a113_bus_bus_ops = {
    .get_protocol = a113_bus_get_protocol,
};

static void a113_bus_release(void* ctx) {
    a113_bus_t* bus = ctx;
    free(bus);
}

static zx_protocol_device_t a113_bus_device_protocol = {
    .version = DEVICE_OPS_VERSION,
    .release = a113_bus_release,
};

static void i2chandler(aml_i2c_txn_t *txn) {
    uint64_t *temp = (uint64_t*)txn->rx_buff;
    printf("handler got back %016lx\n",*temp);
    *temp = 0;
}

static uint8_t txbuff[8];
static uint8_t rxbuff[8];

static int i2c_test_thread(void *arg) {
    aml_i2c_connection_t *conn = arg;

    printf("test thread\n");
    while(1) {
        txbuff[0] = 0x00;
        printf("writing async\n");
        aml_i2c_wr_rd_async(conn,txbuff,1,rxbuff,8,&i2chandler);
        sleep(1);
    }
    return 0;
}

static zx_status_t a113_bus_bind(void* ctx, zx_device_t* parent, void** cookie) {
    a113_bus_t* bus = calloc(1, sizeof(a113_bus_t));
    if (!bus) {
        return ZX_ERR_NO_MEMORY;
    }

    if (device_get_protocol(parent, ZX_PROTOCOL_PLATFORM_BUS, &bus->pbus) != ZX_OK) {
        free(bus);
        return ZX_ERR_NOT_SUPPORTED;
    }

    bus->usb_mode_switch.ops = &usb_mode_switch_ops;
    bus->usb_mode_switch.ctx = bus;

    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "a113-bus",
        .ctx = bus,
        .ops = &a113_bus_device_protocol,
        .flags = DEVICE_ADD_NON_BINDABLE,
    };

    zx_status_t status = device_add(parent, &args, NULL);
    if (status != ZX_OK) {
        goto fail;
    }

    pbus_interface_t intf;
    intf.ops = &a113_bus_bus_ops;
    intf.ctx = bus;
    pbus_set_interface(&bus->pbus, &intf);

    if ((status = a113_usb_init(bus)) != ZX_OK) {
        dprintf(ERROR, "a113_bus_bind failed: %d\n", status);
    }
    if ((status = a113_audio_init(bus)) != ZX_OK) {
        dprintf(ERROR, "a113_audio_init failed: %d\n", status);
    }

    // Initialize Pin mux subsystem.
    status = a113_init_pinmux(bus);
    if (status != ZX_OK) {
        dprintf(ERROR, "a113_bus_bind: failed to initialize pinmux subsystem, "
                "rc = %d\n", status);
        // Don't think this is worthy of returning failure in bind.
    }

    printf("A113: setting pinmux for i2c-b\n");
    a113_config_pinmux(bus, A113_GPIOZ(8), 1);
    a113_config_pinmux(bus, A113_GPIOZ(9), 1);

    aml_i2c_dev_t *i2cb_dev;

    status = aml_i2c_init(&i2cb_dev, bus, AML_I2C_B);
    if (status != ZX_OK) {
        printf("Could not initialize i2c device!\n");
    }
    //aml_i2c_set_slave_addr(i2cb_dev,0x18);
    aml_i2c_dumpstate(i2cb_dev);
    aml_i2c_connection_t *conn1;
    aml_i2c_connection_t *conn2;
    aml_i2c_connection_t *conn3;


    aml_i2c_connect(&conn1,i2cb_dev,0x18,7);
    aml_i2c_connect(&conn2,i2cb_dev,0x18,7);
    aml_i2c_connect(&conn3,i2cb_dev,0x10,7);

    thrd_t thrd;
    thrd_create_with_name(&thrd, i2c_test_thread, conn1, "i2c_test_thread");


    //aml_i2c_write(i2cb_dev, NULL, 2);
/*
    printf("A113: register block base address (virt) = %p\n",reg);
    printf("A113: register block base address (phys) = %lx\nsize=%lu\n",bus->i2c_b_regs.phys,
                                                                        bus->i2c_b_regs.size);
    reg[0] = reg[0] |  (1 << 22);
    reg[0] = reg[0] & ~(3 << 23);

    printf("A113: regs[0] - 0x%08x\n",reg[0]);
    printf("A113: regs[1] - 0x%08x\n",reg[1]);
    printf("A113: regs[2] - 0x%08x\n",reg[2]);
    printf("A113: regs[3] - 0x%08x\n",reg[3]);

*/
#if 0
    aml_i2c_test(i2cb_dev);

    a113_clk_dev_t *clk_dev;

    status = a113_clk_init(&clk_dev, bus);
    if (status != ZX_OK) {
        printf("Could not initialize i2c device!\n");
    }
    uint32_t clkreg = a113_clk_get_reg(clk_dev,0x5d);
    clkreg = (clkreg & ~(0x7f | (0x7 << 12))) | 5 | (0x7 << 12);
    a113_clk_set_reg(clk_dev, 0x5d, clkreg);

    printf("clk reg 0x5d = %08x\n", a113_clk_get_reg(clk_dev,0x5d));
    printf("clk reg 0xa0 = %08x\n", a113_clk_get_reg(clk_dev,0xa0));
    printf("clk reg 0xa1 = %08x\n", a113_clk_get_reg(clk_dev,0xa1));
    printf("clk reg 0xa2 = %08x\n", a113_clk_get_reg(clk_dev,0xa2));
    printf("clk reg 0xa3 = %08x\n", a113_clk_get_reg(clk_dev,0xa3));
    printf("clk reg 0xa4 = %08x\n", a113_clk_get_reg(clk_dev,0xa4));
    printf("clk reg 0xa5 = %08x\n", a113_clk_get_reg(clk_dev,0xa5));
    printf("clk reg 0xa6 = %08x\n", a113_clk_get_reg(clk_dev,0xa6));
    printf("clk reg 0xa7 = %08x\n", a113_clk_get_reg(clk_dev,0xa7));
    printf("clk reg 0xa8 = %08x\n", a113_clk_get_reg(clk_dev,0xa8));
    printf("clk reg 0xa9 = %08x\n", a113_clk_get_reg(clk_dev,0xa9));
    printf("clk reg 0xb8 = %08x\n", a113_clk_get_reg(clk_dev,0xb8));
    printf("clk reg 0xb9 = %08x\n", a113_clk_get_reg(clk_dev,0xb9));
#endif

    return ZX_OK;

fail:
    printf("a113_bus_bind failed %d\n", status);
    a113_bus_release(bus);
    return status;
}

static zx_driver_ops_t a113_bus_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = a113_bus_bind,
};

ZIRCON_DRIVER_BEGIN(a113_bus, a113_bus_driver_ops, "zircon", "0.1", 3)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_PLATFORM_BUS),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_AMLOGIC),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_PID, PDEV_PID_AMLOGIC_A113),
ZIRCON_DRIVER_END(a113_bus)
