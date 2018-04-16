// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

// These are relative to base address 0xc1100000 and in sizeof(uint32_t)
#define S912_GPIO_INT_EDGE_POLARITY     0x2620
#define S912_GPIO_0_3_PIN_SELECT        0x2621
#define S912_GPIO_4_7_PIN_SELECT        0x2622
#define S912_GPIO_FILTER_SELECT         0x2623

#define GPIO_INTERRUPT_POLARITY_SHIFT   16
#define PINS_PER_BLOCK                  32
#define ALT_FUNCTION_MAX                6
#define MAX_GPIO_INDEX                  255
#define BITS_PER_GPIO_INTERRUPT         8
