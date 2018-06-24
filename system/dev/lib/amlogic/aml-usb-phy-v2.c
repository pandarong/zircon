// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/debug.h>
#include <ddk/io-buffer.h>
#include <hw/reg.h>
#include <soc/aml-common/aml-usb-phy-v2.h>
#include <soc/aml-common/aml-usb-phy-v2-regs.h>
#include <soc/aml-s905d2/s905d2-hw.h>

#include <threads.h>
#include <unistd.h>

// from mesong12a.dtsi
#define PLL_SETTING_0   0x09400414
#define PLL_SETTING_1   0x927E0000
#define PLL_SETTING_2   0xac5f49e5

// set_usb_pll() in phy_aml_new_usb2_v2.c
static zx_status_t set_usb_pll(void* regs) {
printf("set_usb_pll\n");

printf("pll write %08x to %p\n", (0x30000000 | PLL_SETTING_0), regs + 0x40);
    writel((0x30000000 | PLL_SETTING_0), regs + 0x40);
printf("pll write %08x to %p\n", PLL_SETTING_1, regs + 0x44);
    writel(PLL_SETTING_1, regs + 0x44);
printf("pll write %08x to %p\n", PLL_SETTING_2, regs + 0x48);
    writel(PLL_SETTING_2, regs + 0x48);
    zx_nanosleep(zx_deadline_after(ZX_USEC(100)));
printf("pll write %08x to %p\n", (0x10000000 | PLL_SETTING_0), regs + 0x40);
    writel((0x10000000 | PLL_SETTING_0), regs + 0x40);

    return ZX_OK;
}

static void set_mode(volatile void* usbctrl_regs, bool host) {
    // phy-aml-new-usb3-v2.c set_mode()
    volatile void* u2p_r0 = usbctrl_regs + (1 * PHY_REGISTER_SIZE) + U2P_R0_OFFSET;
    volatile void* usb_r0 = usbctrl_regs + USB_R0_OFFSET;
    volatile void* usb_r1 = usbctrl_regs + USB_R1_OFFSET;
    volatile void* usb_r4 = usbctrl_regs + USB_R4_OFFSET;
    uint32_t temp;

printf("set_mode %s\n", (host ? "HOST" : "DEVICE"));
    temp = readl(usb_r0);
    if (host) {
        temp &= ~USB_R0_U2D_ACT;
    } else {
        temp |= USB_R0_U2D_ACT;
        temp &= ~USB_R0_U2D_SS_SCALEDOWN_MODE;
    }
printf("USB_R0_U2D_ACT etc write %08x to %p\n", temp, usb_r0);
    writel(temp, usb_r0);

    temp = readl(usb_r4);
    if (host) {
        temp &= ~USB_R4_P21_SLEEPM0;
    } else {
        temp |= USB_R4_P21_SLEEPM0;
    }
printf("USB_R4_P21_SLEEPM0 write %08x to %p\n", temp, usb_r4);
    writel(temp, usb_r4);

    temp = readl(usb_r1);
    temp &= ~(3 << USB_R1_U3H_HOST_U2_PORT_DISABLE_SHIFT);
    if (!host) {
        temp |= (2 << USB_R1_U3H_HOST_U2_PORT_DISABLE_SHIFT);
	}
    writel(temp, usb_r1);


    temp = readl(u2p_r0);
    if (host) {
        temp |= U2P_R0_HOST_DEVICE;
    } else {
        temp &= ~U2P_R0_HOST_DEVICE;
    }
    temp &= ~U2P_R0_POR;
printf("u2p_r0 write %08x to %p\n", temp, u2p_r0);
    writel(temp, u2p_r0);
}

static int usb_iddig_irq_thread(void* arg) {
    aml_usb_phy_v2_t* phy = (aml_usb_phy_v2_t*)arg;
    volatile void* usbctrl_regs = phy->usbctrl_regs;
    volatile uint32_t* usb_r5 = (uint32_t *)(usbctrl_regs + USB_R5_OFFSET);
    uint32_t temp;

    printf("XXX usb_iddig_irq_thread start\n");

    while (1) {
        zx_status_t wait_res = zx_interrupt_wait(phy->iddig_irq_handle, NULL);
        if (wait_res != ZX_OK) {
            zxlogf(ERROR, "dwc_usb: irq wait failed, retcode = %d\n", wait_res);
        }

        printf("XXX usb_iddig_irq_thread got interrupt\n");

        // clear the interrupt
        temp = readl(usb_r5);
        temp &= ~USB_R5_USB_IDDIG_IRQ;
printf("XXX usb_r1 write %08x to %p\n", temp, usb_r5);
        writel(temp, usb_r5);

        // amlogic_gxl_work()
        
        temp = readl(usb_r5);
    	if (temp & USB_R5_IDDIG_CURR) {
    	    printf("DEVICE_MODE\n");
    		set_mode(usbctrl_regs, false);
    	} else {
    	    printf("HOST_MODE\n");
    		set_mode(usbctrl_regs, true);
    	}
    	
        temp &= ~USB_R5_USB_IDDIG_IRQ;
printf("XXX usb_r1 write %08x to %p\n", temp, usb_r5);
        writel(temp, usb_r5);    
    }
    return 0;
}

zx_status_t aml_usb_phy_v2_init(aml_usb_phy_v2_t* phy, zx_handle_t bti, bool host) {
    zx_status_t status;
    uint32_t temp;

    status = io_buffer_init_physical(&phy->usbctrl_buf, bti, S905D2_USBCTRL_BASE, S905D2_USBCTRL_LENGTH,
                                     get_root_resource(),
                                     ZX_CACHE_POLICY_UNCACHED_DEVICE);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_usb_init io_buffer_init_physical failed %d\n", status);
        return status;
    }
    status = io_buffer_init_physical(&phy->reset_buf, bti, S905D2_RESET_BASE, S905D2_RESET_LENGTH,
                                     get_root_resource(),
                                     ZX_CACHE_POLICY_UNCACHED_DEVICE);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_usb_init io_buffer_init_physical failed %d\n", status);
        return status;
    }
    status = io_buffer_init_physical(&phy->phy20_buf, bti, S905D2_USBPHY20_BASE, S905D2_USBPHY20_LENGTH,
                                     get_root_resource(),
                                     ZX_CACHE_POLICY_UNCACHED_DEVICE);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_usb_init io_buffer_init_physical failed %d\n", status);
        return status;
    }
    status = io_buffer_init_physical(&phy->phy21_buf, bti, S905D2_USBPHY21_BASE, S905D2_USBPHY21_LENGTH,
                                     get_root_resource(),
                                     ZX_CACHE_POLICY_UNCACHED_DEVICE);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_usb_init io_buffer_init_physical failed %d\n", status);
        return status;
    }

    volatile void* reset_regs = io_buffer_virt(&phy->reset_buf);
    volatile void* usbctrl_regs = io_buffer_virt(&phy->usbctrl_buf);
    phy->usbctrl_regs = usbctrl_regs;

printf("reset_regs: %p phys %p\n", reset_regs, (void*)io_buffer_phys(&phy->reset_buf));
printf("usbctrl_regs: %p phys %p\n", usbctrl_regs, (void*)io_buffer_phys(&phy->usbctrl_buf));

    // first reset USB
    uint32_t val = readl(reset_regs + 0x21 * 4);
printf("reset write %08x to %p\n", (val | (0x3 << 16)), reset_regs + 0x21 * 4);
    writel((val | (0x3 << 16)), reset_regs + 0x21 * 4);

    // amlogic_new_usbphy_reset_v2()
    volatile uint32_t* reset_1 = (uint32_t *)(reset_regs + S905D2_RESET1_REGISTER);
printf("reset_1 write %08x to %p\n", readl(reset_1) | S905D2_RESET1_USB, reset_1);
    writel(readl(reset_1) | S905D2_RESET1_USB, reset_1);
    // FIXME(voydanoff) this delay is very long, but it is what the Amlogic Linux kernel is doing.
    zx_nanosleep(zx_deadline_after(ZX_MSEC(500)));

    // amlogic_new_usb2_init()
    for (int i = 0; i < 2; i++) {
        volatile void* addr = usbctrl_regs + U2P_R0_OFFSET;
        temp = readl(addr);
        temp |= U2P_R0_POR;
        temp |= U2P_R0_HOST_DEVICE;
        if (i == 1) {
            temp |= U2P_R0_IDPULLUP0;
            temp |= U2P_R0_DRVVBUS0;
        }
printf("i: %d por etc write %08x to %p\n", i, temp, addr);
        writel(temp, addr);

        zx_nanosleep(zx_deadline_after(ZX_USEC(10)));

        // amlogic_new_usbphy_reset_phycfg_v2()
printf("i: %d reset_1 write %08x to %p\n", i, readl(reset_1) | (1 << (16 + 0 /*i is always zero here */)), reset_1);
        writel(readl(reset_1) | (1 << (16 + 0 /*i is always zero here */)), reset_1);

        zx_nanosleep(zx_deadline_after(ZX_USEC(50)));

        addr = usbctrl_regs + (i * PHY_REGISTER_SIZE) + U2P_R1_OFFSET;

        temp = readl(addr);
        int cnt = 0;
        while (!(temp & U2P_R1_PHY_RDY)) {
            temp = readl(addr);
            // wait phy ready max 1ms, common is 100us
            if (cnt > 200) {
                zxlogf(ERROR, "aml_usb_init U2P_R1_PHY_RDY wait failed\n");
                break;
            }

            cnt++;
            zx_nanosleep(zx_deadline_after(ZX_USEC(5)));
        }
    }

    // set up PLLs
    set_usb_pll(io_buffer_virt(&phy->phy20_buf));
    set_usb_pll(io_buffer_virt(&phy->phy21_buf));

    // amlogic_new_usb3_init()
    volatile uint32_t* usb_r1 = (uint32_t *)(usbctrl_regs + USB_R1_OFFSET);
    volatile uint32_t* usb_r5 = (uint32_t *)(usbctrl_regs + USB_R5_OFFSET);

    temp = readl(usb_r1);
    temp &= ~(0x3f << USB_R1_U3H_FLADJ_30MHZ_REG_SHIFT);
    temp |= (0x26 << USB_R1_U3H_FLADJ_30MHZ_REG_SHIFT);
printf("usb_r1 write %08x to %p\n", temp, usb_r1);
    writel(temp, usb_r1);

    // enable interrupt? do we want this?
    temp = readl(usb_r5);
    temp |= USB_R5_IDDIG_EN0;
    temp |= USB_R5_IDDIG_EN1;
    temp |= (255 << USB_R5_IDDIG_TH_SHIFT);
printf("usb_r1 write %08x to %p\n", temp, usb_r5);
    writel(temp, usb_r5);
    
    sleep(1);
    set_mode(usbctrl_regs, false);

/*
    status = zx_interrupt_create(get_root_resource(), S905D2_USB_IDDIG_IRQ,
                    ZX_INTERRUPT_MODE_LEVEL_HIGH, &phy->iddig_irq_handle);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_usb_phy_v2_init zx_interrupt_create failed %d\n", status);
        return status;
    }

    thrd_create_with_name(&phy->iddig_irq_thread, usb_iddig_irq_thread, phy, "usb_iddig_irq_thread");

    printf("let interrupt fire\n");
    sleep(5);
*/
    return ZX_OK;
}
