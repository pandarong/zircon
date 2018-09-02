// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <ddk/debug.h>
#include <ddk/metadata.h>
#include <ddk/protocol/platform-defs.h>
#include <ddk/protocol/usb-mode-switch.h>
#include <soc/imx8m/imx8m-gpio.h>
#include <soc/imx8m/imx8m-hw.h>
#include <soc/imx8m/imx8m-sip.h>
#include <zircon/syscalls/smc.h>

#include "imx8mevk.h"

static const pbus_mmio_t dwc3_mmios[] = {
    {
        .base = IMX8M_USB1_BASE,
        .length = IMX8M_USB1_LENGTH,
    },
};

static const pbus_irq_t dwc3_irqs[] = {
    {
        .irq = IMX8M_A53_INTR_USB1,
        .mode = ZX_INTERRUPT_MODE_EDGE_HIGH,
    },
};

static const pbus_bti_t dwc3_btis[] = {
    {
        .iommu_index = 0,
        .bti_id = BTI_USB1,
    },
};

static usb_mode_t dwc3_mode = USB_MODE_HOST;

static const pbus_metadata_t dwc3_metadata[] = {
    {
        .type       = DEVICE_METADATA_USB_MODE,
        .data       = &dwc3_mode,
        .len        = sizeof(dwc3_mode),
    }
};

static const pbus_dev_t madrone_usb_children[] = {
    {
        .name = "dwc3",
        .vid = PDEV_VID_GENERIC,
        .pid = PDEV_PID_GENERIC,
        .did = PDEV_DID_USB_DWC3,
        .mmios = dwc3_mmios,
        .mmio_count = countof(dwc3_mmios),
        .irqs = dwc3_irqs,
        .irq_count = countof(dwc3_irqs),
        .btis = dwc3_btis,
        .bti_count = countof(dwc3_btis),
        .metadata = dwc3_metadata,
        .metadata_count = countof(dwc3_metadata),
    },
};

static const pbus_gpio_t madrone_usb_gpios[] = {
    {
        .gpio = IMX_GPIO_PIN(5, 4), // first argument is 1-based
    },
};

const pbus_dev_t madrone_usb_dev = {
    .name = "madrone-usb",
    .vid = PDEV_VID_GOOGLE,
    .pid = PDEV_PID_MADRONE,
    .did = PDEV_DID_MADRONE_USB,
    .gpios = madrone_usb_gpios,
    .gpio_count = countof(madrone_usb_gpios),
    .children = madrone_usb_children,
    .child_count = countof(madrone_usb_children),
};

static const pbus_mmio_t usb2_mmios[] = {
    {
        .base = IMX8M_USB2_BASE,
        .length = IMX8M_USB2_LENGTH,
    },
};
static const pbus_irq_t usb2_irqs[] = {
    {
        .irq = IMX8M_A53_INTR_USB2,
        .mode = ZX_INTERRUPT_MODE_EDGE_HIGH,
    },
};

static const pbus_bti_t usb2_btis[] = {
    {
        .iommu_index = 0,
        .bti_id = BTI_USB2,
    },
};

static usb_mode_t usb2_mode = USB_MODE_HOST;

static const pbus_metadata_t usb2_metadata[] = {
    {
        .type       = DEVICE_METADATA_USB_MODE,
        .data       = &usb2_mode,
        .len        = sizeof(usb2_mode),
    }
};

// USB2 is USB-A port, host only
static const pbus_dev_t usb2_dev = {
    .name = "dwc3-2",
    .vid = PDEV_VID_GENERIC,
    .pid = PDEV_PID_GENERIC,
    .did = PDEV_DID_USB_XHCI,
    .mmios = usb2_mmios,
    .mmio_count = countof(usb2_mmios),
    .irqs = usb2_irqs,
    .irq_count = countof(usb2_irqs),
    .btis = usb2_btis,
    .bti_count = countof(usb2_btis),
    .metadata = usb2_metadata,
    .metadata_count = countof(usb2_metadata),
};


zx_status_t madrone_usb_init(imx8mevk_bus_t* bus) {
    zx_status_t status;
    zx_handle_t bti;

    status = iommu_get_bti(&bus->iommu, 0, BTI_BOARD, &bti);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: iommu_get_bti failed %d\n", __FUNCTION__, status);
        return status;
    }

    status = imx_usb_phy_init(IMX8M_USB1_BASE, IMX8M_USB1_LENGTH, bti);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: imx_usb_phy_init failed %d\n", __FUNCTION__, status);
        zx_handle_close(bti);
        return status;
    }
    status = imx_usb_phy_init(IMX8M_USB2_BASE, IMX8M_USB2_LENGTH, bti);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: imx_usb_phy_init failed %d\n", __FUNCTION__, status);
        zx_handle_close(bti);
        return status;
    }
    zx_handle_close(bti);

    if ((status = pbus_device_add(&bus->pbus, &madrone_usb_dev)) != ZX_OK) {
        zxlogf(ERROR, "imx_usb_init could not add madrone_usb_dev: %d\n", status);
        return status;
    }
    if ((status = pbus_device_add(&bus->pbus, &usb2_dev)) != ZX_OK) {
        zxlogf(ERROR, "imx_usb_init could not add usb2_dev: %d\n", status);
        return status;
    }
    return ZX_OK;
}
