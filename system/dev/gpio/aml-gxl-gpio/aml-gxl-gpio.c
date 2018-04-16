// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <threads.h>

#include <bits/limits.h>
#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/protocol/gpio.h>
#include <ddk/protocol/platform-bus.h>
#include <ddk/protocol/platform-defs.h>
#include <ddk/protocol/platform-device.h>
#include <hw/reg.h>
#include "aml-gxl-gpio.h"
#include <zircon/assert.h>
#include <zircon/types.h>

typedef struct {
    uint32_t pin_count;
    uint32_t oen_offset;
    uint32_t input_offset;
    uint32_t output_offset;
    uint32_t output_shift;  // Used for GPIOAO block
    uint32_t mmio_index;
    uint32_t pull_offset;
    uint32_t pull_en_offset;
    uint32_t pin_start;
    mtx_t lock;
} aml_gpio_block_t;

typedef struct {
    // pinmux register offsets for the alternate functions.
    // zero means alternate function not supported.
    uint8_t regs[ALT_FUNCTION_MAX];
    // bit number to set/clear to enable/disable alternate function
    uint8_t bits[ALT_FUNCTION_MAX];
} aml_pinmux_t;

typedef struct {
    aml_pinmux_t mux[PINS_PER_BLOCK];
} aml_pinmux_block_t;

typedef struct {
    platform_device_protocol_t pdev;
    gpio_protocol_t gpio;
    zx_device_t* zxdev;
    io_buffer_t mmios[2];    // separate MMIO for AO domain
    io_buffer_t mmio_interrupt;
    aml_gpio_block_t* gpio_blocks;
    const aml_pinmux_block_t* pinmux_blocks;
    size_t block_count;
    mtx_t pinmux_lock;
    uint32_t irq_count;
    uint8_t irq_status;
} aml_gpio_t;

// MMIO indices (based on vim2_display_mmios)
enum {
    MMIO_GPIO,
    MMIO_GPIO_A0,
    MMIO_GPIO_INTERRUPTS,
};

#include "s912-blocks.h"
#include "s905x-blocks.h"
#include "s905-blocks.h"

static zx_status_t aml_pin_to_block(aml_gpio_t* gpio, const uint32_t pin,
                                    aml_gpio_block_t** out_block, uint32_t* out_pin_index) {
    ZX_DEBUG_ASSERT(out_block && out_pin_index);

    uint32_t block_index = pin / PINS_PER_BLOCK;
    if (block_index >= gpio->block_count) {
        return ZX_ERR_NOT_FOUND;
    }
    aml_gpio_block_t* block = &gpio->gpio_blocks[block_index];
    uint32_t pin_index = pin % PINS_PER_BLOCK;
    if (pin_index >= block->pin_count) {
        return ZX_ERR_NOT_FOUND;
    }
    pin_index += block->output_shift;
    *out_block = block;
    *out_pin_index = pin_index;
    return ZX_OK;
}

static zx_status_t aml_gpio_config(void* ctx, uint32_t index, uint32_t flags) {
    aml_gpio_t* gpio = ctx;
    zx_status_t status;

    aml_gpio_block_t* block;
    uint32_t pin_index;
    if ((status = aml_pin_to_block(gpio, index, &block, &pin_index)) != ZX_OK) {
        zxlogf(ERROR, "aml_gpio_config: pin not found %u\n", index);
        return status;
    }

    // Set the GPIO as IN or OUT
    volatile uint32_t* reg = (volatile uint32_t *)io_buffer_virt(&gpio->mmios[block->mmio_index]);
    reg += block->oen_offset;
    mtx_lock(&block->lock);

    uint32_t regval = readl(reg);
    uint32_t direction = flags & GPIO_DIR_MASK;
    if (direction & GPIO_DIR_OUT) {
        regval &= ~(1 << pin_index);
    } else {
        // Set the GPIO as pull-up or pull-down
        uint32_t pull = flags & GPIO_PULL_MASK;
        volatile uint32_t* pull_reg = (volatile uint32_t *)io_buffer_virt(&gpio->mmios[block->mmio_index]);
        pull_reg += block->pull_offset;
        volatile uint32_t* pull_en_reg = (volatile uint32_t *)io_buffer_virt(&gpio->mmios[block->mmio_index]);
        pull_en_reg += block->pull_en_offset;

        uint32_t pull_reg_val = readl(pull_reg);
        uint32_t pull_en_reg_val = readl(pull_en_reg);
        if (pull & GPIO_PULL_UP) {
            pull_reg_val |= (1 << pin_index);
        } else {
            pull_reg_val &= ~(1 << pin_index);
        }
        pull_en_reg_val |= (1 << pin_index);
        writel(pull_reg_val, pull_reg);
        writel(pull_en_reg_val, pull_en_reg);
        regval |= (1 << pin_index);
    }
    writel(regval, reg);
    mtx_unlock(&block->lock);

    return ZX_OK;
}

// Configure a pin for an alternate function specified by function
static zx_status_t aml_gpio_set_alt_function(void* ctx, const uint32_t pin, uint64_t function) {
    aml_gpio_t* gpio = ctx;

    if (function > ALT_FUNCTION_MAX) {
        return ZX_ERR_OUT_OF_RANGE;
    }

    uint32_t block_index = pin / PINS_PER_BLOCK;
    if (block_index >= gpio->block_count) {
        return ZX_ERR_NOT_FOUND;
    }
    const aml_pinmux_block_t* block = &gpio->pinmux_blocks[block_index];
    uint32_t pin_index = pin % PINS_PER_BLOCK;
    const aml_pinmux_t* mux = &block->mux[pin_index];

    aml_gpio_block_t* gpio_block = &gpio->gpio_blocks[block_index];
    volatile uint32_t* reg = (volatile uint32_t *)
                                    io_buffer_virt(&gpio->mmios[gpio_block->mmio_index]);

    mtx_lock(&gpio->pinmux_lock);

    for (uint i = 0; i < ALT_FUNCTION_MAX; i++) {
        uint32_t reg_index = mux->regs[i];
        //reg_index += block->output_shift;
        if (reg_index) {
            uint32_t mask = (1 << mux->bits[i]);
            uint32_t regval = readl(reg + reg_index);

            if (i == function - 1) {
                regval |= mask;
            } else {
                regval &= ~mask;
            }

            writel(regval, reg + reg_index);
        }
    }

    mtx_unlock(&gpio->pinmux_lock);

    return ZX_OK;
}

static zx_status_t aml_gpio_read(void* ctx, uint32_t pin, uint8_t* out_value) {
    aml_gpio_t* gpio = ctx;
    zx_status_t status;

    aml_gpio_block_t* block;
    uint32_t pin_index;
    if ((status = aml_pin_to_block(gpio, pin, &block, &pin_index)) != ZX_OK) {
        zxlogf(ERROR, "aml_gpio_read: pin not found %u\n", pin);
        return status;
    }
    const uint32_t readmask = 1 << pin_index;

    volatile uint32_t* reg = (volatile uint32_t *)io_buffer_virt(&gpio->mmios[block->mmio_index]);
    reg += block->input_offset;

    mtx_lock(&block->lock);

    const uint32_t regval = readl(reg);

    mtx_unlock(&block->lock);

    if (regval & readmask) {
        *out_value = 1;
    } else {
        *out_value = 0;
    }

    return ZX_OK;
}

static zx_status_t aml_gpio_write(void* ctx, uint32_t pin, uint8_t value) {
    aml_gpio_t* gpio = ctx;
    zx_status_t status;

    aml_gpio_block_t* block;
    uint32_t pin_index;
    if ((status = aml_pin_to_block(gpio, pin, &block, &pin_index)) != ZX_OK) {
        zxlogf(ERROR, "aml_gpio_write: pin not found %u\n", pin);
        return status;
    }

    volatile uint32_t* reg = (volatile uint32_t *)io_buffer_virt(&gpio->mmios[block->mmio_index]);
    reg += block->output_offset;

    mtx_lock(&block->lock);

    uint32_t regval = readl(reg);

    if (value) {
        regval |= (1 << pin_index);
    } else {
        regval &= ~(1 << pin_index);
    }

    writel(regval, reg);

    mtx_unlock(&block->lock);

    return ZX_OK;
}

static uint32_t aml_gpio_get_unsed_irq_index(uint8_t status) {
    // First isolate the rightmost 0-bit
    uint8_t zero_bit_set = ~status & (status+1);
    // Count no. of leading zeros
    return __builtin_ctz(zero_bit_set);
}

static zx_status_t aml_gpio_get_interrupt(void *ctx, uint32_t pin,
                                          uint32_t flags,
                                          zx_handle_t *out_handle) {
    aml_gpio_t* gpio = ctx;
    zx_status_t status;
    mtx_lock(&gpio->pinmux_lock);
    if (pin > MAX_GPIO_INDEX) {
        return ZX_ERR_INVALID_ARGS;
    }
    uint32_t index = aml_gpio_get_unsed_irq_index(gpio->irq_status);
    if (index > gpio->irq_count) {
        return ZX_ERR_NO_RESOURCES;
    }

    aml_gpio_block_t* block;
    uint32_t pin_index;
    if ((status = aml_pin_to_block(gpio, pin, &block, &pin_index)) != ZX_OK) {
        zxlogf(ERROR, "aml_gpio_read: pin not found %u\n", pin);
        return status;
    }

    // Create Interrupt Object
    status = pdev_get_interrupt(&gpio->pdev, index, flags,
                                    out_handle);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_gpio_get_interrupt: pdev_map_interrupt failed %d\n", status);
        return status;
    }

    // Configure GPIO interrupt
    volatile uint32_t* reg = (volatile uint32_t *)io_buffer_virt(&gpio->mmio_interrupt);
    reg  += (index>3)? S912_GPIO_4_7_PIN_SELECT: S912_GPIO_0_3_PIN_SELECT;

    // Select GPIO IRQ(index) and program it to
    // the requested GPIO PIN
    uint32_t regval = readl(reg);
    regval |= (((pin % PINS_PER_BLOCK) + block->pin_start) << (index * BITS_PER_GPIO_INTERRUPT));
    writel(regval, reg);

    // Configure GPIO Interrupt EDGE and Polarity
    volatile uint32_t* mode_reg = (volatile uint32_t *)io_buffer_virt(&gpio->mmio_interrupt);
    mode_reg += S912_GPIO_INT_EDGE_POLARITY;
    uint32_t mode_reg_val = readl(mode_reg);

    switch (flags & ZX_INTERRUPT_MODE_MASK) {
    case ZX_INTERRUPT_MODE_EDGE_LOW:
        mode_reg_val = mode_reg_val | (1 << index);
        mode_reg_val = mode_reg_val | ((1 << index) << GPIO_INTERRUPT_POLARITY_SHIFT);
        break;
    case ZX_INTERRUPT_MODE_EDGE_HIGH:
        mode_reg_val = mode_reg_val | (1 << index);
        mode_reg_val = mode_reg_val & ~((1 << index) << GPIO_INTERRUPT_POLARITY_SHIFT);
        break;
    case ZX_INTERRUPT_MODE_LEVEL_LOW:
        mode_reg_val = mode_reg_val & ~(1 << index);
        mode_reg_val = mode_reg_val | ((1 << index) << GPIO_INTERRUPT_POLARITY_SHIFT);
        break;
    case ZX_INTERRUPT_MODE_LEVEL_HIGH:
        mode_reg_val = mode_reg_val & ~(1 << index);
        mode_reg_val = mode_reg_val & ~((1 << index) << GPIO_INTERRUPT_POLARITY_SHIFT);
        break;
    default:
        return ZX_ERR_INVALID_ARGS;
    }
    writel(mode_reg_val, mode_reg);
    gpio->irq_status |= 1 << index;
    mtx_unlock(&gpio->pinmux_lock);
    return ZX_OK;
}

static gpio_protocol_ops_t gpio_ops = {
    .config = aml_gpio_config,
    .set_alt_function = aml_gpio_set_alt_function,
    .read = aml_gpio_read,
    .write = aml_gpio_write,
    .get_interrupt = aml_gpio_get_interrupt,
};

static void aml_gpio_release(void* ctx) {
    aml_gpio_t* gpio = ctx;
    io_buffer_release(&gpio->mmios[0]);
    io_buffer_release(&gpio->mmios[1]);
    io_buffer_release(&gpio->mmio_interrupt);
    free(gpio);
}


static zx_protocol_device_t gpio_device_proto = {
    .version = DEVICE_OPS_VERSION,
    .release = aml_gpio_release,
};

static zx_status_t aml_gpio_bind(void* ctx, zx_device_t* parent) {
    zx_status_t status;

    aml_gpio_t* gpio = calloc(1, sizeof(aml_gpio_t));
    if (!gpio) {
        return ZX_ERR_NO_MEMORY;
    }
    mtx_init(&gpio->pinmux_lock, mtx_plain);

    if ((status = device_get_protocol(parent, ZX_PROTOCOL_PLATFORM_DEV, &gpio->pdev)) != ZX_OK) {
        zxlogf(ERROR, "aml_gpio_bind: ZX_PROTOCOL_PLATFORM_DEV not available\n");
        goto fail;
    }

    platform_bus_protocol_t pbus;
    if ((status = device_get_protocol(parent, ZX_PROTOCOL_PLATFORM_BUS, &pbus)) != ZX_OK) {
        zxlogf(ERROR, "aml_gpio_bind: ZX_PROTOCOL_PLATFORM_BUS not available\n");
        goto fail;
    }

    status = pdev_map_mmio_buffer(&gpio->pdev, MMIO_GPIO, ZX_CACHE_POLICY_UNCACHED_DEVICE,
                                    &gpio->mmios[0]);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_gpio_bind: pdev_map_mmio_buffer failed\n");
        goto fail;
    }

    status = pdev_map_mmio_buffer(&gpio->pdev, MMIO_GPIO_A0, ZX_CACHE_POLICY_UNCACHED_DEVICE,
                                    &gpio->mmios[1]);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_gpio_bind: pdev_map_mmio_buffer failed\n");
        goto fail;
    }

    status = pdev_map_mmio_buffer(&gpio->pdev, MMIO_GPIO_INTERRUPTS, ZX_CACHE_POLICY_UNCACHED_DEVICE,
                                    &gpio->mmio_interrupt);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_gpio_bind: pdev_map_mmio_buffer failed\n");
        goto fail;
    }

    pdev_device_info_t info;
    status = pdev_get_device_info(&gpio->pdev, &info);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_gpio_bind: pdev_get_device_info failed\n");
        goto fail;
    }

    switch (info.pid) {
    case PDEV_PID_AMLOGIC_S912:
        gpio->gpio_blocks = s912_gpio_blocks;
        gpio->pinmux_blocks = s912_pinmux_blocks;
        gpio->block_count = countof(s912_gpio_blocks);
        break;
    case PDEV_PID_AMLOGIC_S905X:
        gpio->gpio_blocks = s905x_gpio_blocks;
        gpio->pinmux_blocks = s905x_pinmux_blocks;
        gpio->block_count = countof(s905x_gpio_blocks);
        break;
    case PDEV_PID_AMLOGIC_S905:
        gpio->gpio_blocks = s905_gpio_blocks;
        gpio->pinmux_blocks = s905_pinmux_blocks;
        gpio->block_count = countof(s905_gpio_blocks);
        break;
    default:
        zxlogf(ERROR, "aml_gpio_bind: unsupported SOC PID %u\n", info.pid);
        goto fail;
    }

    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "aml-gxl-gpio",
        .ctx = gpio,
        .ops = &gpio_device_proto,
        .flags = DEVICE_ADD_NON_BINDABLE,
    };

    status = device_add(parent, &args, &gpio->zxdev);
    if (status != ZX_OK) {
        zxlogf(ERROR, "aml_gpio_bind: device_add failed\n");
        goto fail;
    }

    gpio->irq_count = info.irq_count;
    gpio->irq_status = 0;
    gpio->gpio.ops = &gpio_ops;
    gpio->gpio.ctx = gpio;
    pbus_set_protocol(&pbus, ZX_PROTOCOL_GPIO, &gpio->gpio);

    return ZX_OK;

fail:
    aml_gpio_release(gpio);
    return status;
}

static zx_driver_ops_t aml_gpio_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = aml_gpio_bind,
};

ZIRCON_DRIVER_BEGIN(aml_gpio, aml_gpio_driver_ops, "zircon", "0.1", 6)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_PLATFORM_DEV),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_AMLOGIC),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_DID, PDEV_DID_AMLOGIC_GPIO),
    // we support multiple SOC variants
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_PID, PDEV_PID_AMLOGIC_S912),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_PID, PDEV_PID_AMLOGIC_S905X),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_PID, PDEV_PID_AMLOGIC_S905),
ZIRCON_DRIVER_END(aml_gpio)
