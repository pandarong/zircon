// Copyright 2016 The Fuchsia Authors
// Copyright (c) 2009 Corey Tabaka
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <stdarg.h>
#include <reg.h>
#include <bits.h>
#include <stdio.h>
#include <kernel/thread.h>
#include <kernel/timer.h>
#include <vm/physmap.h>
#include <lk/init.h>
#include <arch/x86.h>
#include <arch/x86/apic.h>
#include <lib/cbuf.h>
#include <dev/interrupt.h>
#include <kernel/cmdline.h>
#include <platform.h>
#include <platform/pc.h>
#include <platform/pc/bootloader.h>
#include <platform/console.h>
#include <platform/debug.h>
#include <trace.h>
#include <zircon/types.h>

#include "platform_p.h"

static const int uart_baud_rate = 115200;
static int uart_io_port = 0x3f8;
static uint64_t uart_mem_addr = 0;
static uint32_t uart_irq = ISA_IRQ_SERIAL1;

cbuf_t console_input_buf;
static bool output_enabled = false;
static uint32_t uart_fifo_depth;
static uint32_t uart_fifo_tx_count;

static uint8_t uart_read(uint8_t reg) {
    if (uart_mem_addr) {
        return (uint8_t)readl(uart_mem_addr + 4 * reg);
    } else {
        return (uint8_t)inp((uint16_t)(uart_io_port + reg));
    }
}

static void uart_write(uint8_t reg, uint8_t val) {
    if (uart_mem_addr) {
        writel(val, uart_mem_addr + 4 * reg);
    } else {
        outp((uint16_t)(uart_io_port + reg), val);
    }
}

static enum handler_return platform_drain_debug_uart_rx() {
    unsigned char c;
    bool resched = false;

    // see why we have gotten an irq
    for (;;) {
        uint8_t iir = uart_read(2);
        if (BIT(iir, 0))
            break; // no valid interrupt

        // 3 bit identification field
        uint ident = BITS(iir, 3, 0);
        switch (ident) {
            case 0b0100:
            case 0b1100:
                // rx fifo is non empty, drain it
                c = uart_read(0);
                cbuf_write_char(&console_input_buf, c, false);
                resched = true;
                break;
            default:
                printf("UART: unhandled ident %#x\n", ident);
        }
    }

    return resched ? INT_RESCHEDULE : INT_NO_RESCHEDULE;
}

static enum handler_return uart_irq_handler(void *arg) {
    return platform_drain_debug_uart_rx();
}

// for devices where the uart rx interrupt doesn't seem to work
static enum handler_return uart_rx_poll(struct timer *t, zx_time_t now, void *arg) {
    timer_set(t, now + ZX_MSEC(10), TIMER_SLACK_CENTER, ZX_MSEC(1), uart_rx_poll, NULL);
    return platform_drain_debug_uart_rx();
}

// also called from the pixel2 quirk file
void platform_debug_start_uart_timer();

void platform_debug_start_uart_timer() {
    static timer_t uart_rx_poll_timer;
    static bool started = false;

    if (!started) {
        started = true;
        timer_init(&uart_rx_poll_timer);
        timer_set(&uart_rx_poll_timer, current_time() + ZX_MSEC(10),
            TIMER_SLACK_CENTER, ZX_MSEC(1), uart_rx_poll, NULL);
    }
}

static void init_uart() {
    /* configure the uart */
    int divisor = 115200 / uart_baud_rate;

    /* get basic config done so that tx functions */
    uart_write(1, 0); // mask all irqs
    uart_write(3, 0x80); // set up to load divisor latch
    uart_write(0, static_cast<uint8_t>(divisor)); // lsb
    uart_write(1, static_cast<uint8_t>(divisor >> 8)); // msb
    uart_write(3, 3); // 8N1
    // enable FIFO, rx reset, tx reset, 16750 64 byte fifo enable, 14-byte threshold;
    uart_write(2, (1<<0) | (1<<1) | (1<<2) | (1<<5) | (3<<6));

    /* figure out the fifo depth */
    uint8_t fcr = uart_read(2);
    if (BITS_SHIFT(fcr, 7, 6) == 3 && BIT(fcr, 5)) {
        // this is a 16750
        uart_fifo_depth = 64;
    } else if (BITS_SHIFT(fcr, 7, 6) == 3) {
        // this is a 16550A
        uart_fifo_depth = 16;
    } else {
        uart_fifo_depth = 1;
    }
    uart_fifo_tx_count = 0;
}

void platform_init_debug_early() {
    switch (bootloader.uart.type) {
    case BOOTDATA_UART_PC_PORT:
        uart_io_port = static_cast<uint32_t>(bootloader.uart.base);
        uart_irq = bootloader.uart.irq;
        break;
    case BOOTDATA_UART_PC_MMIO:
        uart_mem_addr = (uint64_t)paddr_to_physmap(bootloader.uart.base);
        uart_irq = bootloader.uart.irq;
        break;
    }

    init_uart();

    output_enabled = true;

    dprintf(INFO, "UART: FIFO depth %u\n", uart_fifo_depth);
}

void platform_init_debug() {
    /* finish uart init to get rx going */
    cbuf_initialize(&console_input_buf, 1024);

    if ((uart_irq == 0) || cmdline_get_bool("kernel.debug_uart_poll", false)) {
        printf("debug-uart: polling enabled\n");
        platform_debug_start_uart_timer();
    } else {
        uart_irq = apic_io_isa_to_global(static_cast<uint8_t>(uart_irq));
        register_int_handler(uart_irq, uart_irq_handler, NULL);
        unmask_interrupt(uart_irq);

        uart_write(1, (1<<0)); // enable receive data available interrupt

        // modem control register: Axiliary Output 2 is another IRQ enable bit
        const uint8_t mcr = uart_read(4);
        uart_write(4, mcr | 0x8);
    }
}

void platform_suspend_debug() {
    output_enabled = false;
}

void platform_resume_debug() {
    init_uart();
    output_enabled = true;
}

// polling versions of debug uart read/write
static int debug_uart_getc_poll(char *c) {
    // if there is a character available, read it
    if (uart_read(5) & (1<<0)) {
        *c = uart_read(0);
        return 0;
    }

    return -1;
}

static void debug_uart_putc_poll(char c) {
    // while the fifo is non empty, spin
    while (!(uart_read(5) & (1<<6))) {
        arch_spinloop_pause();
    }
    uart_write(0, c);
}

static void debug_uart_putc(char c)
{
    if (unlikely(!output_enabled))
        return;

    for (;;) {
        // if we know there is space in the tx fifo for sure
        if (uart_fifo_tx_count < uart_fifo_depth) {
            uart_write(0, c);
            uart_fifo_tx_count++;
            return;
        }

        // while the fifo is non empty, spin
        while ((uart_read(5) & (1<<6)) == 0) {
            arch_spinloop_pause();
        }

        // now we can shove a fifo's full of chars in it before spinning again
        uart_fifo_tx_count = 0;
    }
}

void platform_dputs(const char* str, size_t len)
{
    while (len-- > 0) {
        char c = *str++;
        if (c == '\n') {
            debug_uart_putc('\r');
#if WITH_LEGACY_PC_CONSOLE
            cputc(c);
#endif
        }
        debug_uart_putc(c);
#if WITH_LEGACY_PC_CONSOLE
        cputc(c);
#endif
    }
}

int platform_dgetc(char *c, bool wait) {
    return static_cast<int>(cbuf_read_char(&console_input_buf, c, wait));
}

// panic time polling IO for the panic shell
void platform_pputc(char c) {
    if (c == '\n')
        debug_uart_putc_poll('\r');
    debug_uart_putc_poll(c);
}

int platform_pgetc(char *c, bool wait) {
    return debug_uart_getc_poll(c);
}
