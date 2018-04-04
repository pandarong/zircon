// Copyright 2018 The Fuchsia Authors. All rights reserved.
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

#include <soc/imx8m/imx8m.h>
#include <soc/imx8m/imx8m-hw.h>
#include <soc/imx8m/imx8m-iomux.h>
#include <soc/imx8m/imx8m-gpio.h>
#include <hw/reg.h>
#include <zircon/assert.h>
#include <zircon/types.h>

typedef struct {
    platform_device_protocol_t  pdev;
    platform_bus_protocol_t     pbus;
    gpio_protocol_t             gpio;
    zx_device_t*                zxdev;
    io_buffer_t                 mmios[IMX_GPIO_BLOCKS];
    mtx_t                       lock[IMX_GPIO_BLOCKS];
} imx8_gpio_t;


static zx_status_t imx8_gpio_config(void* ctx, uint32_t pin, uint32_t flags) {
    uint32_t gpio_block;
    uint32_t gpio_pin;
    uint32_t regVal;
    imx8_gpio_t* gpio = ctx;

    gpio_block = IMX_NUM_TO_BLOCK(pin);
    gpio_pin = IMX_NUM_TO_BIT(pin);

    if (gpio_block >= IMX_GPIO_BLOCKS || gpio_pin >= 32) {
        zxlogf(ERROR, "%s: Invalid GPIO pin (pin = %d Block = %d, Offset = %d)\n",
            __FUNCTION__, pin, gpio_block, gpio_pin);
        return ZX_ERR_INVALID_ARGS;
    }

    mtx_lock(&gpio->lock[gpio_block]);
    volatile uint8_t* reg = (volatile uint8_t*)io_buffer_virt(&gpio->mmios[gpio_block]);
    regVal = readl(reg + IMX_GPIO_GDIR);
    regVal &= ~(1 << gpio_pin);
    if (flags & GPIO_DIR_OUT) {
        regVal |= (GPIO_OUTPUT << gpio_pin);
    } else {
        regVal |= (GPIO_INPUT << gpio_pin);
    }
    writel(regVal, reg + IMX_GPIO_GDIR);
    mtx_unlock(&gpio->lock[gpio_block]);
    return ZX_OK;
}


static zx_status_t imx8_gpio_read(void* ctx, uint32_t pin, uint8_t* out_value) {
    uint32_t gpio_block;
    uint32_t gpio_pin;
    uint32_t regVal;
    imx8_gpio_t* gpio = ctx;

    gpio_block = IMX_NUM_TO_BLOCK(pin);
    gpio_pin = IMX_NUM_TO_BIT(pin);

    if (gpio_block >= IMX_GPIO_BLOCKS || gpio_pin >= 32) {
        zxlogf(ERROR, "%s: Invalid GPIO pin (pin = %d Block = %d, Offset = %d)\n",
            __FUNCTION__, pin, gpio_block, gpio_pin);
        return ZX_ERR_INVALID_ARGS;
    }

    mtx_lock(&gpio->lock[gpio_block]);
    volatile uint8_t* reg = (volatile uint8_t*)io_buffer_virt(&gpio->mmios[gpio_block]);
    regVal = readl(reg + IMX_GPIO_DR);
    regVal >>= (gpio_pin);
    regVal &= 1;
    *out_value = regVal;
    mtx_unlock(&gpio->lock[gpio_block]);

    return ZX_OK;
}

static zx_status_t imx8_gpio_write(void* ctx, uint32_t pin, uint8_t value) {
    uint32_t gpio_block;
    uint32_t gpio_pin;
    uint32_t regVal;
    imx8_gpio_t* gpio = ctx;

    gpio_block = IMX_NUM_TO_BLOCK(pin);
    gpio_pin = IMX_NUM_TO_BIT(pin);
    if (gpio_block >= IMX_GPIO_BLOCKS || gpio_pin >= 32) {
        zxlogf(ERROR, "%s: Invalid GPIO pin (pin = %d Block = %d, Offset = %d)\n",
            __FUNCTION__, pin, gpio_block, gpio_pin);
        return ZX_ERR_INVALID_ARGS;
    }

    mtx_lock(&gpio->lock[gpio_block]);
    volatile uint8_t* reg = (volatile uint8_t*)io_buffer_virt(&gpio->mmios[gpio_block]);
    regVal = readl(reg + IMX_GPIO_DR);
    regVal &= ~(1 << gpio_pin);
    regVal |= (value << gpio_pin);
    writel(regVal, reg + IMX_GPIO_DR);
    mtx_unlock(&gpio->lock[gpio_block]);


    return ZX_OK;
}

static gpio_protocol_ops_t gpio_ops = {
    .config = imx8_gpio_config,
    .read = imx8_gpio_read,
    .write = imx8_gpio_write,
};

static void imx8_gpio_release(void* ctx)
{
    unsigned i;
    imx8_gpio_t* gpio = ctx;

    for (i = 0; i < countof(gpio->mmios); i++) {
        io_buffer_release(&gpio->mmios[i]);
    }

    free(gpio);
}

static zx_protocol_device_t gpio_device_proto = {
    .version = DEVICE_OPS_VERSION,
    .release = imx8_gpio_release,
};

static zx_status_t imx8_gpio_bind(void* ctx, zx_device_t* parent)
{
    zx_status_t status;
    unsigned i;

    imx8_gpio_t* gpio = calloc(1, sizeof(imx8_gpio_t));
    if (!gpio) {
        return ZX_ERR_NO_MEMORY;
    }

    status = device_get_protocol(parent, ZX_PROTOCOL_PLATFORM_DEV, &gpio->pdev);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: ZX_PROTOCOL_PLATFORM_DEV not available %d \n", __FUNCTION__, status);
        goto fail;
    }

    status = device_get_protocol(parent, ZX_PROTOCOL_PLATFORM_BUS, &gpio->pbus);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: ZX_PROTOCOL_PLATFORM_BUS not available %d\n", __FUNCTION__, status);
        goto fail;
    }

    for (i = 0; i < countof(gpio->mmios); i++) {
        status = pdev_map_mmio_buffer(&gpio->pdev, i, ZX_CACHE_POLICY_UNCACHED_DEVICE,
                                        &gpio->mmios[i]);
        if (status != ZX_OK) {
            zxlogf(ERROR, "%s: pdev_map_mmio_buffer failed %d\n", __FUNCTION__, status);
            goto fail;
        }

        mtx_init(&gpio->lock[i], mtx_plain);
    }

    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "imx8-gpio",
        .ctx = gpio,
        .ops = &gpio_device_proto,
        .flags = DEVICE_ADD_NON_BINDABLE,
    };

    status = device_add(parent, &args, &gpio->zxdev);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s: device_add failed! %d\n", __FUNCTION__, status);
        goto fail;
    }

    gpio->gpio.ops = &gpio_ops;
    gpio->gpio.ctx = gpio;
    pbus_set_protocol(&gpio->pbus, ZX_PROTOCOL_GPIO, &gpio->gpio);

    return ZX_OK;

fail:
    imx8_gpio_release(gpio);
    return status;

}

static zx_driver_ops_t imx8_gpio_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = imx8_gpio_bind,
};

ZIRCON_DRIVER_BEGIN(imx8_gpio, imx8_gpio_driver_ops, "zircon", "0.1", 6)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_PLATFORM_DEV),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_NXP),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_DID, PDEV_DID_IMX_GPIO),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_PID, PDEV_PID_IMX8MEVK),
ZIRCON_DRIVER_END(imx8_gpio)
