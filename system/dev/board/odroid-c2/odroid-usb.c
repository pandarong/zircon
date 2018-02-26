// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/debug.h>
#include <ddk/io-buffer.h>
#include <ddk/protocol/gpio.h>
#include <ddk/protocol/platform-defs.h>
#include <hw/reg.h>

#include <soc/aml-s905/s905-gpio.h>

#include <limits.h>
#include <unistd.h>

#include "odroid.h"

#define GPIO_HUB_RESET  S905_GPIOAO(4)
#define GPIO_VBUS       S905_GPIOAO(5)

static const pbus_mmio_t usb_host_mmios[] = {
    {
        .base = 0xc9100000,
        .length = 0x40000,
    },
};

static const pbus_irq_t usb_host_irqs[] = {
    {
        .irq = 31 + 32,
        .mode = ZX_INTERRUPT_MODE_EDGE_HIGH,
    },
};

static const pbus_dev_t usb_host_dev = {
    .name = "dwc2-host",
    .vid = PDEV_VID_GENERIC,
    .pid = PDEV_PID_GENERIC,
    .did = PDEV_DID_USB_DWC2,
    .mmios = usb_host_mmios,
    .mmio_count = countof(usb_host_mmios),
    .irqs = usb_host_irqs,
    .irq_count = countof(usb_host_irqs),
};

zx_status_t odroid_usb_init(odroid_t* odroid) {
    zx_status_t status;
    volatile uint32_t* regs;
    uint32_t temp;

/* clocks appear to be already set
    io_buffer_t clocks;
    if ((status = io_buffer_init_physical(&clocks, 0xc883c000, PAGE_SIZE, get_root_resource(),
                                                 ZX_CACHE_POLICY_UNCACHED_DEVICE)) != ZX_OK) {
        return status;
    }

    regs = io_buffer_virt(&clocks);
    
    temp = readl(regs + (0x144 / 4));
printf("before %08x\n", temp);
    temp |= (1 << 26); // USB General
    temp |= (1 << 21); // USB 0
    temp |= (1 << 22); // USB 1
    writel(temp, regs + (0x144 / 4));
printf("after %08x\n", temp);

    temp = readl(regs + (0x148 / 4));
printf("before %08x\n", temp);
    temp |= (1 << 9); // USB0 to DDR
    temp |= (1 << 8); // USB0 to DDR
    writel(temp, regs + (0x148 / 4));
printf("after %08x\n", temp);

    io_buffer_release(&clocks);
*/

    io_buffer_t phy;
    if ((status = io_buffer_init_physical(&phy, 0xc0000000, PAGE_SIZE, get_root_resource(),
                                                 ZX_CACHE_POLICY_UNCACHED_DEVICE)) != ZX_OK) {
        return status;
    }

    regs = io_buffer_virt(&phy);
    temp = readl(regs + 8); // config
    temp |= (1 << 16); // clk_32k_alt_sel
    writel(temp, regs + 8);

    temp = readl(regs + 9); // ctrl
    temp &= ~(7 << 22); // fsel
    temp |= (5 << 22);
    temp |= (1 << 15); // por
    writel(temp, regs + 9);
    usleep(500);
    temp &= ~(1 << 15); // por
    writel(temp, regs + 9);
    usleep(500);
    temp = readl(regs + 9);
    usleep(500);

    // only for port b
    temp = readl(regs + 11); // adp_bc
    temp |= (1 << 16); // aca_enable
    writel(temp, regs + 11);
    usleep(50);
    temp = readl(regs + 11); // adp_bc

/*
    regs = io_buffer_virt(&phy);
    for (int i = 0; i < 2; i++) {
        printf("config:    %08x\n", readl(regs++));
        printf("ctrl:      %08x\n", readl(regs++));
        printf("endp_intr: %08x\n", readl(regs++));
        printf("adp_bc:    %08x\n", readl(regs++));
        printf("dbg_uart:  %08x\n", readl(regs++));
        printf("test:      %08x\n", readl(regs++));
        printf("tune:      %08x\n", readl(regs++));
        printf("reserved:  %08x\n", readl(regs++));
    }
*/
    io_buffer_release(&phy);

    gpio_config(&odroid->gpio, GPIO_HUB_RESET, GPIO_DIR_OUT);
    gpio_write(&odroid->gpio, GPIO_HUB_RESET, 0);
    usleep(20 * 1000);
    gpio_write(&odroid->gpio, GPIO_HUB_RESET, 1);
    usleep(20 * 1000);
sleep(1);

    gpio_config(&odroid->gpio, GPIO_VBUS, GPIO_DIR_OUT);
    gpio_write(&odroid->gpio, GPIO_VBUS, 1);

    if ((status = pbus_device_add(&odroid->pbus, &usb_host_dev, 0)) != ZX_OK) {
        zxlogf(ERROR, "odroid_usb_init could not add usb_host_dev: %d\n", status);
        return status;
    }

    return ZX_OK;
}
