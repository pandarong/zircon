// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/debug.h>
#include <ddk/protocol/platform-defs.h>
#include <ddk/protocol/usb-mode-switch.h>
#include <hw/reg.h>

#include <soc/aml-common/aml-usb-phy.h>
#include <soc/aml-s912/s912-hw.h>
#include <unistd.h>

#include "vim.h"

#define BIT_MASK(start, count) (((1 << (count)) - 1) << (start))
#define SET_BITS(dest, start, count, value) \
        ((dest & ~BIT_MASK(start, count)) | (((value) << (start)) & BIT_MASK(start, count)))

/*
static const pbus_mmio_t xhci_mmios[] = {
    {
        .base = S912_USB0_BASE,
        .length = S912_USB0_LENGTH,
    },
};

static const pbus_irq_t xhci_irqs[] = {
    {
        .irq = S912_USBH_IRQ,
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
*/

static const pbus_mmio_t dwc2_mmios[] = {
    {
        .base = S912_USB1_BASE,
        .length = S912_USB1_LENGTH,
    },
};

static const pbus_irq_t dwc2_irqs[] = {
    {
        .irq = S912_USBD_IRQ,
        .mode = ZX_INTERRUPT_MODE_LEVEL_HIGH,
    },
};

static const pbus_bti_t dwc2_btis[] = {
    {
        .iommu_index = 0,
        .bti_id = BTI_USB_DWC2,
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

static zx_status_t vim_get_initial_mode(void* ctx, usb_mode_t* out_mode) {
    *out_mode = USB_MODE_DEVICE;
    return ZX_OK;
}

static zx_status_t vim_set_mode(void* ctx, usb_mode_t mode) {
/*
    vim_bus_t* bus = ctx;

    // add or remove XHCI device
    pbus_device_enable(&bus->pbus, PDEV_VID_GENERIC, PDEV_PID_GENERIC, PDEV_DID_USB_XHCI,
                       mode == USB_MODE_HOST);
*/
    return ZX_OK;
}

usb_mode_switch_protocol_ops_t usb_mode_switch_ops = {
    .get_initial_mode = vim_get_initial_mode,
    .set_mode = vim_set_mode,
};


static void print_regs(volatile uint32_t* regs) {
    uint32_t addr = 0xd0078000;
    
    while (addr < 0xd00780a0) {
        printf("%08x: %08x\n", addr, *regs);
        addr += 4;
        regs++;
    }
}

#if 0
static void vim_usb_phy_init(volatile void* regs) {
    volatile void* addr;
    uint32_t temp;

printf("BEFORE\n");
print_regs(regs);

    // amlogic_new_usb2_init
    for (int i = 0; i < 4; i++) {
        addr = regs + (i * PHY_REGISTER_SIZE) + U2P_R0_OFFSET;
        temp = readl(addr);
        temp |= U2P_R0_POR;
        temp |= U2P_R0_DMPULLDOWN;
        temp |= U2P_R0_DPPULLDOWN;
        if (i == 1) {
            temp |= U2P_R0_IDPULLUP;
        }
        writel(temp, addr);
        zx_nanosleep(zx_deadline_after(ZX_USEC(500)));
        temp = readl(addr);
        temp &= ~U2P_R0_POR;
        writel(temp, addr);
    }

/*
    // amlogic_new_usb3_init
    addr = regs + (4 * PHY_REGISTER_SIZE);

    temp = readl(addr + USB_R1_OFFSET);
    temp = SET_BITS(temp, USB_R1_U3H_FLADJ_30MHZ_REG_START, USB_R1_U3H_FLADJ_30MHZ_REG_BITS, 0x20);
    writel(temp, addr + USB_R1_OFFSET);

    temp = readl(addr + USB_R5_OFFSET);
    temp |= USB_R5_IDDIG_EN0;
    temp |= USB_R5_IDDIG_EN1;
    temp = SET_BITS(temp, USB_R5_IDDIG_TH_START, USB_R5_IDDIG_TH_BITS, 255);
    writel(temp, addr + USB_R5_OFFSET);
*/

    if (1 /*device_mode*/) {
        addr = regs + (1 * PHY_REGISTER_SIZE) + U2P_R0_OFFSET;

        temp = readl(addr);
        temp &= ~U2P_R0_DMPULLDOWN;
        temp &= ~U2P_R0_DPPULLDOWN;
        writel(temp, addr);

        addr = regs + (4 * PHY_REGISTER_SIZE) + USB_R0_OFFSET;
        temp = readl(addr);
        temp |= USB_R0_U2D_ACT;
        writel(temp, addr);
  
        addr = regs + (4 * PHY_REGISTER_SIZE) + USB_R4_OFFSET;
        temp = readl(addr);
        temp |= USB_R4_P21_SLEEPM0;
        writel(temp, addr);
  
        addr = regs + (4 * PHY_REGISTER_SIZE) + USB_R1_OFFSET;
        temp = readl(addr);
        temp &= ~(0xf << USB_R1_U3H_HOST_U2_PORT_DISABLE_START);
        temp |= (2 << USB_R1_U3H_HOST_U2_PORT_DISABLE_START);
        writel(temp, addr);
    }


printf("AFTER\n");
print_regs(regs);
}
#endif

typedef struct u2p_aml_regs {
	volatile uint32_t u2p_r0;
	volatile uint32_t u2p_r1;
	volatile uint32_t u2p_r2;
} u2p_aml_regs_t;

typedef union u2p_r0 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned bypass_sel:1;
		unsigned bypass_dm_en:1;
		unsigned bypass_dp_en:1;
		unsigned txbitstuffenh:1;
		unsigned txbitstuffen:1;
		unsigned dmpulldown:1;
		unsigned dppulldown:1;
		unsigned vbusvldextsel:1;
		unsigned vbusvldext:1;
		unsigned adp_prb_en:1;
		unsigned adp_dischrg:1;
		unsigned adp_chrg:1;
		unsigned drvvbus:1;
		unsigned idpullup:1;
		unsigned loopbackenb:1;
		unsigned otgdisable:1;
		unsigned commononn:1;
		unsigned fsel:3;
		unsigned refclksel:2;
		unsigned por:1;
		unsigned vatestenb:2;
		unsigned set_iddq:1;
		unsigned ate_reset:1;
		unsigned fsv_minus:1;
		unsigned fsv_plus:1;
		unsigned bypass_dm_data:1;
		unsigned bypass_dp_data:1;
		unsigned not_used:1;
	} b;
} u2p_r0_t;

typedef struct usb_aml_regs {
	volatile uint32_t usb_r0;
	volatile uint32_t usb_r1;
	volatile uint32_t usb_r2;
	volatile uint32_t usb_r3;
	volatile uint32_t usb_r4;
	volatile uint32_t usb_r5;
	volatile uint32_t usb_r6;
} usb_aml_regs_t;

typedef union usb_r0 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned p30_fsel:6;
		unsigned p30_phy_reset:1;
		unsigned p30_test_powerdown_hsp:1;
		unsigned p30_test_powerdown_ssp:1;
		unsigned p30_acjt_level:5;
		unsigned p30_tx_vboost_lvl:3;
		unsigned p30_lane0_tx2rx_loopbk:1;
		unsigned p30_lane0_ext_pclk_req:1;
		unsigned p30_pcs_rx_los_mask_val:10;
		unsigned u2d_ss_scaledown_mode:2;
		unsigned u2d_act:1;
	} b;
} usb_r0_t;

typedef union usb_r4 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned p21_PORTRESET0:1;
		unsigned p21_SLEEPM0:1;
		unsigned mem_pd:2;
		unsigned reserved4:28;
	} b;
} usb_r4_t;

static void vim_usb_phy_init(volatile void* regs) {
	u2p_aml_regs_t * u2p_aml_regs = (u2p_aml_regs_t * )(regs + 0x20);
	usb_aml_regs_t * usb_aml_regs = (usb_aml_regs_t * )(regs + 0x80);

	u2p_r0_t u2p_r0;
	usb_r0_t usb_r0;
	usb_r4_t usb_r4;

/*
	*P_RESET1_REGISTER = (1<<2);

	*P_AO_RTC_ALT_CLK_CNTL0 |= (1<<31)|(1<<30);
	*P_AO_RTI_PWR_CNTL_REG0 |= (4<<10);
*/

	u2p_r0.d32 = u2p_aml_regs->u2p_r0;
	u2p_r0.b.fsel = 2;
	u2p_r0.b.por = 1;
	u2p_r0.b.dppulldown = 0;
	u2p_r0.b.dmpulldown = 0;
	u2p_aml_regs->u2p_r0 = u2p_r0.d32;

	u2p_r0.d32 = u2p_aml_regs->u2p_r0;
	u2p_r0.b.por = 0;
	u2p_aml_regs->u2p_r0 = u2p_r0.d32;

	usb_r0.d32 = usb_aml_regs->usb_r0;
	usb_r0.b.u2d_act = 1;
	usb_aml_regs->usb_r0 = usb_r0.d32;

	usb_r4.d32 = usb_aml_regs->usb_r4;
	usb_r4.b.p21_SLEEPM0 = 1;
	usb_aml_regs->usb_r4 = usb_r4.d32;

	usleep(500);
}


zx_status_t vim_usb_init(vim_bus_t* bus) {
    zx_status_t status;

    usb_mode_switch_protocol_t usb_mode_switch;
    usb_mode_switch.ops = &usb_mode_switch_ops;
    usb_mode_switch.ctx = &bus;
    status = pbus_set_protocol(&bus->pbus, ZX_PROTOCOL_USB_MODE_SWITCH, &usb_mode_switch);
    if (status != ZX_OK) {
        return status;
    }

    zx_handle_t bti;
    status = iommu_get_bti(&bus->iommu, 0, BTI_BOARD, &bti);
    if (status != ZX_OK) {
        zxlogf(ERROR, "vim_bus_bind: iommu_get_bti failed: %d\n", status);
        return status;
    }
    io_buffer_t usb_phy;
    status = io_buffer_init_physical(&usb_phy, bti, S912_USB_PHY_BASE, S912_USB_PHY_LENGTH,
                                     get_root_resource(),
                                     ZX_CACHE_POLICY_UNCACHED_DEVICE);
    if (status != ZX_OK) {
        zxlogf(ERROR, "vim_usb_init io_buffer_init_physical failed %d\n", status);
        zx_handle_close(bti);
        return status;
    }

    volatile void* regs = io_buffer_virt(&usb_phy);
    vim_usb_phy_init(regs);




    io_buffer_release(&usb_phy);
    zx_handle_close(bti);
/*
    if ((status = pbus_device_add(&bus->pbus, &xhci_dev, 0)) != ZX_OK) {
        zxlogf(ERROR, "vim_usb_init could not add xhci_dev: %d\n", status);
        return status;
    }
*/
    if ((status = pbus_device_add(&bus->pbus, &dwc2_dev, 0)) != ZX_OK) {
        zxlogf(ERROR, "vim_usb_init could not add dwc2_dev: %d\n", status);
        return status;
    }

    return ZX_OK;
}
