// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#pragma once

#include <stdint.h>
#include <zircon/types.h>

// ACPI structures
struct acpi_rsdp {
    uint8_t sig[8];
    uint8_t checksum;
    uint8_t oemid[6];
    uint8_t revision;
    uint32_t rsdt_address;

    // rev 2+
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __PACKED;

#define ACPI_RSDP_SIG "RSD PTR "

// standard system description table header
struct acpi_sdt_header {
    uint8_t sig[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oemid[6];
    uint8_t oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __PACKED;
static_assert(sizeof(acpi_sdt_header) == 36, "");

struct acpi_rsdt_xsdt {
    acpi_sdt_header header;

    // array of uint32s or uint64 addresses are placed immediately afterwards
    union {
        uint32_t addr32[0];
        uint64_t addr64[0];
    };
} __PACKED;

#define ACPI_RSDT_SIG "RSDT"
#define ACPI_XSDT_SIG "XSDT"

zx_status_t acpi_lite_init(zx_paddr_t rsdt);
void acpi_lite_dump_tables();

