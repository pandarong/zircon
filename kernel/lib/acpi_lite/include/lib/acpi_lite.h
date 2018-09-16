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

struct acpi_madt_table {
    acpi_sdt_header header;

    uint32_t local_int_controller_address;
    uint32_t flags;
} __PACKED;

#define ACPI_MADT_SIG "APIC"

struct acpi_sub_table_header {
    uint8_t type;
    uint8_t length;
} __PACKED;

struct acpi_generic_address_ {
    uint8_t address_space_id;
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t access_size;
    uint64_t address;
} __PACKED;

struct acpi_hpet_table {
    acpi_sdt_header header;
    uint32_t id;
    acpi_generic_address_ address;
    uint8_t sequence;
    uint16_t minimum_tick;
    uint8_t flags;
} __PACKED;

zx_status_t acpi_lite_init(zx_paddr_t rsdt);
void acpi_lite_dump_tables();

const acpi_sdt_header *acpi_get_table_by_sig(const char *sig);
const acpi_sdt_header *acpi_get_table_at_index(size_t index);


// MADT table describes processors and interrupt controllers
template <typename T, typename C>
static inline zx_status_t acpi_process_madt_entries(const uint8_t search_type, C callback) {
    const acpi_madt_table *madt = reinterpret_cast<const acpi_madt_table *>(acpi_get_table_by_sig(ACPI_MADT_SIG));
    if (!madt) {
        return ZX_ERR_NOT_FOUND;
    }

    // bytewise array of the same table
    const uint8_t *madt_array = (const uint8_t *)madt;

    // walk the table off the end of the header, looking for the requested type
    size_t off = sizeof(*madt);
    while (off < madt->header.length) {
        uint8_t type = madt_array[off];
        uint8_t length = madt_array[off + 1];

        if (type == search_type) {
            callback(reinterpret_cast<const T *>(&madt_array[off]));
        }

        off += length;
    }

    return ZX_OK;
}

// type 0: local apic
struct acpi_madt_local_apic_entry {
    acpi_sub_table_header header;
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __PACKED;

#define ACPI_MADT_FLAG_ENABLED 0x1

// type 1: io apic
struct acpi_madt_io_apic_entry {
    acpi_sub_table_header header;
    uint8_t io_apic_id;
    uint8_t reserved;
    uint32_t io_apic_address;
    uint32_t global_system_interrupt_base;
} __PACKED;

// type 2: interrupt source override
struct acpi_madt_int_source_override_entry {
    acpi_sub_table_header header;
    uint8_t bus;
    uint8_t source;
    uint32_t global_sys_interrupt;
    uint16_t flags;
} __PACKED;

#define ACPI_MADT_FLAG_POLARITY_CONFORMS    0b00
#define ACPI_MADT_FLAG_POLARITY_HIGH        0b01
#define ACPI_MADT_FLAG_POLARITY_LOW         0b11
#define ACPI_MADT_FLAG_POLARITY_MASK        0b11

#define ACPI_MADT_FLAG_TRIGGER_CONFORMS     0b0000
#define ACPI_MADT_FLAG_TRIGGER_EDGE         0b0100
#define ACPI_MADT_FLAG_TRIGGER_LEVEL        0b1100
#define ACPI_MADT_FLAG_TRIGGER_MASK         0b1100

template <typename C>
static inline zx_status_t acpi_process_madt_local_apic_entries(C callback) {
    return acpi_process_madt_entries<acpi_madt_local_apic_entry, C>(0, callback);
}

template <typename C>
static inline zx_status_t acpi_process_madt_io_apic_entries(C callback) {
    return acpi_process_madt_entries<acpi_madt_io_apic_entry, C>(1, callback);
}

template <typename C>
static inline zx_status_t acpi_process_madt_int_source_override_entries(C callback) {
    return acpi_process_madt_entries<acpi_madt_int_source_override_entry, C>(2, callback);
}
