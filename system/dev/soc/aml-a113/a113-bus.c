// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/platform-defs.h>

#include <zircon/assert.h>
#include <zircon/process.h>
#include <zircon/syscalls.h>

#include <soc/aml-a113/a113-bus.h>
#include <soc/aml-a113/a113-hw.h>

#include "a113-bus.h"
#include "a113-hw.h"
#include "aml-i2c.h"
#include "aml-tdm.h"
#include "gauss-hw.h"
#include <hw/reg.h>


zx_status_t a113_bus_init(a113_bus_t** out) {

    return status;
}

void a113_bus_release(a113_bus_t* bus) {
    a113_gpio_release(bus);
    free(bus);
}
