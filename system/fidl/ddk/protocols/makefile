# Copyright 2018 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_DIR := $(patsubst %/,%,$(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))
FIDLC := ../../../../build-arm64/tools/fidlc

CLANG_FORMAT :=  ../../../../prebuilt/downloads/clang/bin/clang-format --style="{Language: Cpp, ColumnLimit: 100, UseTab: Never, IndentWidth: 4, IndentCaseLabels: false, AlignAfterOpenBracket: Align, SpacesBeforeTrailingComments: 1, BreakBeforeBraces: Attach, AccessModifierOffset: -4, DerivePointerAlignment: false, PointerAlignment: Left, AllowShortFunctionsOnASingleLine: Inline, AllowShortIfStatementsOnASingleLine: false, KeepEmptyLinesAtTheStartOfBlocks: true, AlignEscapedNewlinesLeft: false, ForEachMacros: ['list_for_every_entry','list_for_every_entry_safe'], AlwaysBreakTemplateDeclarations: true}"

define run-fidl = 
$(FIDLC) --ddk-header ddk/protocols/$@.h $(patsubst %.fidl,--files %.fidl, $^) \
	--files $(LOCAL_DIR)/$@.fidl
$(FIDLC) --ddktl-header ddktl/protocols/$@.h $(patsubst %.fidl,--files %.fidl, $^) \
	--files $(LOCAL_DIR)/$@.fidl
$(CLANG_FORMAT) ddk/protocols/$@.h > ddk/protocols/$@.formatted.h
$(CLANG_FORMAT) ddktl/protocols/$@.h > ddktl/protocols/$@.formatted.h
$(CLANG_FORMAT) ddktl/protocols/$@-internal.h > ddktl/protocols/$@-internal.formatted.h
cp -f ddk/protocols/$@.formatted.h ../../../ulib/ddk/include/ddk/protocol/$@.h
cp -f ddktl/protocols/$@.formatted.h ../../../ulib/ddktl/include/ddktl/protocol/$@.h
cp -f ddktl/protocols/$@-internal.formatted.h ../../../ulib/ddktl/include/ddktl/protocol/$@-internal.h
endef

acpi:
	$(run-fidl)

amlogic-canvas:
	$(run-fidl)

bad-block:
	$(run-fidl)

block: $(LOCAL_DIR)/libs/block.fidl
	$(run-fidl)

bt-gatt-svc:
	$(run-fidl)

bt-hci:
	$(run-fidl)

clk:
	$(run-fidl)

display-controller: $(LOCAL_DIR)/libs/audio.fidl
	$(run-fidl)

ethernet: $(LOCAL_DIR)/libs/listnode.fidl
	$(run-fidl)

gpio:
	$(run-fidl)

hidbus:
	$(run-fidl)

i2c-impl:
	$(run-fidl)

i2c:
	$(run-fidl)

intel-gpu-core:
	$(run-fidl)

intel-hda-codec:
	$(run-fidl)

intel-hda-dsp:
	$(run-fidl)

iommu:
	$(run-fidl)

mailbox:
	$(run-fidl)

nand: $(LOCAL_DIR)/libs/nand.fidl
	$(run-fidl)

pci: $(LOCAL_DIR)/libs/pci.fidl
	$(run-fidl)

pciroot:
	$(run-fidl)

platform-bus:
	$(run-fidl)

platform-device:
	$(run-fidl)

platform-proxy:
	$(run-fidl)

rawnand: $(LOCAL_DIR)/libs/nand.fidl
	$(run-fidl)

scpi:
	$(run-fidl)

sdhci: $(LOCAL_DIR)/libs/sdhci.fidl
	$(run-fidl)

sdio:
	$(run-fidl)

sdmmc: $(LOCAL_DIR)/libs/listnode.fidl $(LOCAL_DIR)/libs/block.fidl $(LOCAL_DIR)/block.fidl
	$(run-fidl)

serial-impl: $(LOCAL_DIR)/serial.fidl
	$(run-fidl)

serial:
	$(run-fidl)

test:
	$(run-fidl)

usb-bus: $(LOCAL_DIR)/libs/usb.fidl $(LOCAL_DIR)/libs/usb-hub.fidl $(LOCAL_DIR)/usb-hub.fidl
	$(run-fidl)

usb-dci: $(LOCAL_DIR)/libs/listnode.fidl $(LOCAL_DIR)/libs/phys-iter.fidl \
	$(LOCAL_DIR)/libs/usb.fidl $(LOCAL_DIR)/libs/usb-hub.fidl $(LOCAL_DIR)/usb.fidl
	$(run-fidl)

usb-function: $(LOCAL_DIR)/libs/listnode.fidl $(LOCAL_DIR)/libs/phys-iter.fidl \
	$(LOCAL_DIR)/libs/usb.fidl $(LOCAL_DIR)/libs/usb-hub.fidl $(LOCAL_DIR)/usb.fidl
	$(run-fidl)

usb-hci: $(LOCAL_DIR)/libs/listnode.fidl $(LOCAL_DIR)/libs/phys-iter.fidl \
	$(LOCAL_DIR)/libs/usb.fidl $(LOCAL_DIR)/libs/usb-hub.fidl $(LOCAL_DIR)/usb.fidl \
	$(LOCAL_DIR)/usb-hub.fidl $(LOCAL_DIR)/usb-bus.fidl
	$(run-fidl)

usb-hub: $(LOCAL_DIR)/libs/usb-hub.fidl
	$(run-fidl)

usb-mode-switch: $(LOCAL_DIR)/libs/usb-device.fidl $(LOCAL_DIR)/hidbus.fidl
	$(run-fidl)

usb: $(LOCAL_DIR)/libs/listnode.fidl $(LOCAL_DIR)/libs/phys-iter.fidl \
	$(LOCAL_DIR)/libs/usb.fidl $(LOCAL_DIR)/libs/usb-hub.fidl
	$(run-fidl)

all: acpi amlogic-canvas bad-block block bt-gatt-svc bt-hci clk display-controller ethernet gpio \
	hidbus i2c-impl i2c intel-gpu-core intel-hda-codec intel-hda-dsp iommu mailbox nand pci \
	pciroot platform-bus platform-device platform-proxy rawnand scpi sdhci sdio sdmmc \
	serial-impl serial test usb-bus usb-dci usb-function usb-hci usb-hub usb-mode-switch usb

clean:
	rm -rf ddk
	rm -rf ddktl
