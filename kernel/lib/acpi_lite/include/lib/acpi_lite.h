// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#pragma once

#include <stdint.h>
#include <zircon/types.h>

zx_status_t acpi_lite_init(zx_paddr_t rsdt);
void acpi_lite_dump_tables();
