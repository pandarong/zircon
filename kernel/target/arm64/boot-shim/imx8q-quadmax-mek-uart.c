// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include "debug.h"

#define LPUART_STAT                 (0x14)
#define LPUART_STAT_TDRE            (1 << 23)
#define LPUART_DATA                 (0x1c)


#define UARTREG(reg) (*(volatile uint32_t*)(0x5a060000 + (reg)))

void uart_pputc(char c) {
    while (!(UARTREG(LPUART_STAT) & LPUART_STAT_TDRE))
        ;
    UARTREG(LPUART_DATA) = c;
}
