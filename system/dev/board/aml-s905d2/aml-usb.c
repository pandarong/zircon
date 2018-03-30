// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/debug.h>
#include <ddk/protocol/platform-defs.h>
#include <hw/reg.h>

#include <soc/aml-common/aml-usb-phy-v2.h>
#include <soc/aml-s905d2/s905d2-gpio.h>
#include <soc/aml-s905d2/s905d2-hw.h>

#include "aml.h"

static const pbus_mmio_t xhci_mmios[] = {
    {
        .base = S905D2_USB0_BASE,
        .length = S905D2_USB0_LENGTH,
    },
};

static const pbus_irq_t xhci_irqs[] = {
    {
        .irq = S905D2_USBH_IRQ,
        .mode = ZX_INTERRUPT_MODE_EDGE_HIGH,
    },
};

static const pbus_bti_t xhci_btis[] = {
    {
        .iommu_index = 0,
        .bti_id = BTI_USB_XHCI,
    },
};

static const pbus_dev_t xhci_dev = {
    .name = "xhci",
    .vid = PDEV_VID_GENERIC,
    .pid = PDEV_PID_GENERIC,
    .did = PDEV_DID_USB_XHCI,
    .mmios = xhci_mmios,
    .mmio_count = countof(xhci_mmios),
    .irqs = xhci_irqs,
    .irq_count = countof(xhci_irqs),
    .btis = xhci_btis,
    .bti_count = countof(xhci_btis),
};

zx_status_t aml_usb_init(aml_bus_t* bus) {
    zx_status_t status;
    io_buffer_t reset_buf;
    io_buffer_t usbctrl_buf;
    zx_handle_t bti;

    // FIXME - move to board hardware header
    gpio_config(&bus->gpio, S905D2_GPIOH(6), GPIO_DIR_OUT);
    gpio_write(&bus->gpio, S905D2_GPIOH(6), 1);

    status = iommu_get_bti(&bus->iommu, 0, BTI_BOARD, &bti);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_usb_init: iommu_get_bti failed: %d\n", status);
        return status;
    }

    status = io_buffer_init_physical(&reset_buf, bti, S905D2_RESET_BASE, S905D2_RESET_LENGTH,
                                     get_root_resource(),
                                     ZX_CACHE_POLICY_UNCACHED_DEVICE);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_usb_init io_buffer_init_physical failed %d\n", status);
        zx_handle_close(bti);
        return status;
    }

    status = io_buffer_init_physical(&usbctrl_buf, bti, S905D2_USBCTRL_BASE, S905D2_USBCTRL_LENGTH,
                                     get_root_resource(),
                                     ZX_CACHE_POLICY_UNCACHED_DEVICE);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_usb_init io_buffer_init_physical failed %d\n", status);
        io_buffer_release(&reset_buf);
        zx_handle_close(bti);
        return status;
    }

    volatile void* reset_regs = io_buffer_virt(&reset_buf);
    volatile void* usbctrl_regs = io_buffer_virt(&usbctrl_buf);

    // first reset USB
	uint32_t val = readl(reset_regs + 0x21 * 4);
	writel((val | (0x3 << 16)), reset_regs + 0x21 * 4);

    volatile uint32_t* reset_1 = (uint32_t *)(reset_regs + S905D2_RESET1_REGISTER);
    writel(readl(reset_1) | S905D2_RESET1_USB, reset_1);
    // FIXME(voydanoff) this delay is very long, but it is what the Amlogic Linux kernel is doing.
    zx_nanosleep(zx_deadline_after(ZX_MSEC(500)));

    // amlogic_new_usb2_init
    for (int i = 0; i < 2; i++) {
        volatile void* addr = usbctrl_regs + (i * PHY_REGISTER_SIZE) + U2P_R0_OFFSET;
        uint32_t temp = readl(addr);
        temp |= U2P_R0_POR;
        temp |= U2P_R0_HOST_DEVICE;
        if (i == 1) {
            temp |= U2P_R0_IDPULLUP0;
            temp |= U2P_R0_DRVVBUS0;
        }
        writel(temp, addr);

        zx_nanosleep(zx_deadline_after(ZX_USEC(10)));

        writel(readl(reset_1) | (1 << (16 + i)), reset_1);

        zx_nanosleep(zx_deadline_after(ZX_USEC(50)));

        addr = usbctrl_regs + (i * PHY_REGISTER_SIZE) + USB_R1_OFFSET;

        temp = readl(addr);
        int cnt = 0;
        while (!(temp & U2P_R1_PHY_RDY)) {
            temp = readl(addr);
            //we wait phy ready max 1ms, common is 100us
            if (cnt > 200) {
printf("XXXXXX usb loop failed!\n");
                break;
            }

            cnt++;
            zx_nanosleep(zx_deadline_after(ZX_USEC(5)));
        }        
    }

    io_buffer_release(&reset_buf);
    io_buffer_release(&usbctrl_buf);
    zx_handle_close(bti);

    return pbus_device_add(&bus->pbus, &xhci_dev, 0);
}
