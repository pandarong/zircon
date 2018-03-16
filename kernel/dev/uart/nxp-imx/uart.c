// Copyright 2018 The Fuchsia Authors
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <reg.h>
#include <stdio.h>
#include <trace.h>
#include <lib/cbuf.h>
#include <kernel/thread.h>
#include <dev/interrupt.h>
#include <dev/uart.h>
#include <platform/debug.h>
#include <mdi/mdi.h>
#include <mdi/mdi-defs.h>
#include <pdev/driver.h>
#include <pdev/uart.h>

/* Registers */
#define UART_URXD    (0x00)
#define UART_UTXD    (0x40)

#define UART_UCR1    (0x80)
#define UART_UCR2    (0x84)
#define UART_UCR3    (0x88)
#define UART_UCR4    (0x8c)

#define UART_UFCR    (0x90)
#define UART_USR1    (0x94)
#define UART_USR2    (0x98)

#define UART_UESC    (0x9c)
#define UART_UTIM    (0xa0)
#define UART_UBIR    (0xa4)
#define UART_UBMR    (0xa8)
#define UART_UBRC    (0xac)

#define UART_ONEMS   (0xb0)
#define UART_UTS     (0xb4)
#define UART_UMCR    (0xb8)



#define UARTREG(reg)  (*REG32((uart_base)  + (reg)))

#define RXBUF_SIZE 32


// values read from MDI
static uint64_t uart_base = 0;
static uint32_t uart_irq = 0;

static cbuf_t uart_rx_buf;


static bool uart_tx_irq_enabled = false;
static event_t uart_dputc_event = EVENT_INITIAL_VALUE(uart_dputc_event, true, 0);
//static spin_lock_t uart_spinlock = SPIN_LOCK_INITIAL_VALUE;


static void imx_uart_init(mdi_node_ref_t* node, uint level)
{

    // create circular buffer to hold received data
    cbuf_initialize(&uart_rx_buf, RXBUF_SIZE);
#if 0
    // assumes interrupts are contiguous
    //zx_status_t status = register_int_handler(uart_irq, &imx_uart_irq, NULL);
    //DEBUG_ASSERT(status == ZX_OK);

    // clear all irqs
    UARTREG(uart_base, UART_ICR) = 0x3ff;

    // set fifo trigger level
    UARTREG(uart_base, UART_IFLS) = 0; // 1/8 rxfifo, 1/8 txfifo

    // enable rx interrupt
    UARTREG(uart_base, UART_IMSC) = (1 << 4 ) |  //  rxim
                                    (1 << 6);    //  rtim

    // enable receive
    UARTREG(uart_base, UART_CR) |= (1<<9); // rxen

    // enable interrupt
    unmask_interrupt(uart_irq);

#if ENABLE_KERNEL_LL_DEBUG
    uart_tx_irq_enabled = false;
#else
    /* start up tx driven output */
    printf("UART: started IRQ driven TX\n");
    uart_tx_irq_enabled = true;
#endif
#endif
}

static int imx_uart_getc(bool wait)
{
    char c;
    if (cbuf_read_char(&uart_rx_buf, &c, wait) == 1) {
        //UARTREG(uart_base, UART_IMSC) |= ((1<<4)|(1<<6)); // rxim
        return c;
    }

    return -1;
}

/* panic-time getc/putc */
static int imx_uart_pputc(char c)
{
    /* spin while fifo is full */
    while (UARTREG(UART_UTS) & (1<<4))
        ;
    UARTREG(UART_UTXD) = c;

    return 1;
}

static int imx_uart_pgetc(void)
{
    //if ((UARTREG(uart_base, UART_FR) & (1<<4)) == 0) {
    //    return UARTREG(uart_base, UART_DR);
    //} else {
    //    return -1;
   // }
    return 0;
}

static void imx_dputs(const char* str, size_t len,
                        bool block, bool map_NL)
{
#if 0
    spin_lock_saved_state_t state;
    bool copied_CR = false;

    if (!uart_tx_irq_enabled)
        block = false;
    spin_lock_irqsave(&uart_spinlock, state);
#endif
    while (len > 0) {
        imx_uart_pputc(*str++);
        len--;
    }
#if 0
        // Is FIFO Full ?
        while (UARTREG(uart_base, UART_FR) & (1<<5)) {
            if (block) {
                /* Unmask Tx interrupts before we block on the event */
                pl011_unmask_tx();
                spin_unlock_irqrestore(&uart_spinlock, state);
                event_wait(&uart_dputc_event);
            } else {
                spin_unlock_irqrestore(&uart_spinlock, state);
                arch_spinloop_pause();
            }
            spin_lock_irqsave(&uart_spinlock, state);
        }
        if (!copied_CR && map_NL && *str == '\n') {
            copied_CR = true;
            UARTREG(uart_base, UART_DR) = '\r';
        } else {
            copied_CR = false;
            UARTREG(uart_base, UART_DR) = *str++;
            len--;
        }
    }
    spin_unlock_irqrestore(&uart_spinlock, state);
#endif
}

static void imx_start_panic(void)
{
    uart_tx_irq_enabled = false;
}

static const struct pdev_uart_ops uart_ops = {
    .getc = imx_uart_getc,
    .pputc = imx_uart_pputc,
    .pgetc = imx_uart_pgetc,
    .start_panic = imx_start_panic,
    .dputs = imx_dputs,
};

static void imx_uart_init_early(mdi_node_ref_t* node, uint level) {
    uint64_t uart_base_virt = 0;
    bool got_uart_base_virt = false;
    bool got_uart_irq = false;

    mdi_node_ref_t child;
    mdi_each_child(node, &child) {
        switch (mdi_id(&child)) {
        case MDI_BASE_VIRT:
            got_uart_base_virt = !mdi_node_uint64(&child, &uart_base_virt);
            break;
        case MDI_IRQ:
            got_uart_irq = !mdi_node_uint32(&child, &uart_irq);
            break;
        }
    }

    if (!got_uart_base_virt) {
        panic("imx uart: uart_base_virt not defined\n");
    }
    if (!got_uart_irq) {
        panic("imx uart: uart_irq not defined\n");
    }

    uart_base = (uint64_t)uart_base_virt;

    //UARTREG(uart_base, UART_CR) = (1<<8)|(1<<0); // tx_enable, uarten

    pdev_register_uart(&uart_ops);
}

LK_PDEV_INIT(imx_uart_init_early, MDI_ARM_NXP_IMX_UART, imx_uart_init_early, LK_INIT_LEVEL_PLATFORM_EARLY);
LK_PDEV_INIT(imx_uart_init, MDI_ARM_NXP_IMX_UART, imx_uart_init, LK_INIT_LEVEL_PLATFORM);
