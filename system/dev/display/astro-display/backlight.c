// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "astro-display.h"

/* Table from Linux source */
/* TODO: Need to separate backlight driver from display driver */
static const uint8_t backlight_init_table[] = {
    0xa2, 0x20,
    0xa5, 0x54,
    0x00, 0xff,
    0x01, 0x05,
    0xa2, 0x20,
    0xa5, 0x54,
    0xa1, 0xb7,
    0xa0, 0xff,
    0x00, 0x80,
};

void init_backlight(astro_display_t* display) {

    // power on backlight
    gpio_config(&display->gpio, 0, GPIO_DIR_OUT);
    gpio_write(&display->gpio, 0, 1);
    usleep(1000);

    for (size_t i = 0; i < sizeof(backlight_init_table); i+=2) {
        if(i2c_transact_sync(&display->i2c, 0, &backlight_init_table[i], 2, NULL, 0) != ZX_OK) {
            DISP_ERROR("Backlight write failed: reg[0x%x]: 0x%x\n", backlight_init_table[i],
                                            backlight_init_table[i+1]);
        }
    }
}
