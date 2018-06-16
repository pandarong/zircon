// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/debug.h>
#include <ddk/protocol/platform-defs.h>
#include <hw/reg.h>
#include <soc/aml-common/aml-usb-phy-v2.h>
#include <soc/aml-s905d2/s905d2-hw.h>

#include "astro.h"

#define USB_DEVICE 1

#if USB_DEVICE
static const pbus_mmio_t dwc2_mmios[] = {
    {
        .base = S905D2_USB1_BASE,
        .length = S905D2_USB1_LENGTH,
    },
};

static const pbus_irq_t dwc2_irqs[] = {
    {
        .irq = S905D2_USB1_IRQ,
        .mode = ZX_INTERRUPT_MODE_EDGE_HIGH,
    },
};

static const pbus_bti_t dwc2_btis[] = {
    {
        .iommu_index = 0,
        .bti_id = BTI_USB,
    },
};

static const pbus_dev_t dwc2_dev = {
    .name = "dwc2",
    .vid = PDEV_VID_GENERIC,
    .pid = PDEV_PID_GENERIC,
    .did = PDEV_DID_USB_DWC2_DEVICE,
    .mmios = dwc2_mmios,
    .mmio_count = countof(dwc2_mmios),
    .irqs = dwc2_irqs,
    .irq_count = countof(dwc2_irqs),
    .btis = dwc2_btis,
    .bti_count = countof(dwc2_btis),
};
#else
static const pbus_mmio_t xhci_mmios[] = {
    {
        .base = S905D2_USB0_BASE,
        .length = S905D2_USB0_LENGTH,
    },
};

static const pbus_irq_t xhci_irqs[] = {
    {
        .irq = S905D2_USB0_IRQ,
        .mode = ZX_INTERRUPT_MODE_EDGE_HIGH,
    },
};

static const pbus_bti_t xhci_btis[] = {
    {
        .iommu_index = 0,
        .bti_id = BTI_USB,
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
#endif

// magic numbers for USB PHY tuning
#define PLL_SETTING_3   0xfe18
#define PLL_SETTING_4   0xfff
#define PLL_SETTING_5   0x78000
#define PLL_SETTING_6   0xe0004
#define PLL_SETTING_7   0xe000c

static zx_status_t astro_usb_tuning(zx_handle_t bti, bool host, bool default_val) {
    io_buffer_t buf;
    zx_status_t status;

    status = io_buffer_init_physical(&buf, bti, S905D2_USBPHY21_BASE, S905D2_USBPHY21_LENGTH,
                                     get_root_resource(), ZX_CACHE_POLICY_UNCACHED_DEVICE);
    if (status != ZX_OK) {
        return status;
    }

    volatile void* base = io_buffer_virt(&buf);

    if (default_val) {
        writel(0, base + 0x38);
        writel(PLL_SETTING_5, base + 0x34);
    } else {
        writel(PLL_SETTING_3, base + 0x50);
        writel(PLL_SETTING_4, base + 0x10);
        if (host) {
            writel(PLL_SETTING_6, base + 0x38);
        } else {
            writel(PLL_SETTING_7, base + 0x38);
        }
        writel(PLL_SETTING_5, base + 0x34);
    }

    io_buffer_release(&buf);
    return ZX_OK;
}

zx_status_t aml_usb_init(aml_bus_t* bus) {
    zx_handle_t bti;

    zx_status_t status = iommu_get_bti(&bus->iommu, 0, BTI_BOARD, &bti);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_usb_init: iommu_get_bti failed: %d\n", status);
        return status;
    }

#if USB_DEVICE
    const bool host = false;
#else
    const bool host = true;
#endif
    status = aml_usb_phy_v2_init(bti, host);
    if (status != ZX_OK) {
        return status;
    }

    status = astro_usb_tuning(bti, host, true);
    zx_handle_close(bti);
    if (status != ZX_OK) {
        return status;
    }

#if USB_DEVICE
    return pbus_device_add(&bus->pbus, &dwc2_dev, 0);
#else
    return pbus_device_add(&bus->pbus, &xhci_dev, 0);
#endif
}
