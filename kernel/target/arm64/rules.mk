# Copyright 2018 The Fuchsia Authors
#
# Use of this source code is governed by a MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT

LOCAL_DIR := $(GET_LOCAL_DIR)

PLATFORM := generic-arm

# Some boards need gzipped kernel image
OUT_ZIRCON_ZIMAGE := $(BUILDDIR)/z$(LKNAME).bin

$(OUT_ZIRCON_ZIMAGE): $(OUTLKBIN)
	$(call BUILDECHO,gzipping image $@)
	$(NOECHO)gzip -c $< > $@

GENERATED += $(OUT_ZIRCON_ZIMAGE)
EXTRA_BUILDDEPS += $(OUT_ZIRCON_ZIMAGE)

# generate board list for the fuchsia build based on our list of subdirectories
EXTRA_BUILDDEPS += $(OUT_ZIRCON_ZIMAGE)
SUBDIRS := $(wildcard $(LOCAL_DIR)/*/.)
BOARDS := $(patsubst $(LOCAL_DIR)/%/.,%,$(SUBDIRS))
# convert spaces to newlines so we have one board per line
BOARDS := $(subst $(SPACE),\\n,$(BOARDS))

ZIRCON_BOARD_LIST := $(BUILDDIR)/boards.list
$(ZIRCON_BOARD_LIST): FORCE
	$(call BUILDECHO,generating $@)
	@$(MKDIR)
	$(NOECHO)echo $(BOARDS) > $@

packages: $(ZIRCON_BOARD_LIST)

GENERATED += $(OUT_ZIRCON_ZIMAGE) $(ZIRCON_BOARD_LIST)
EXTRA_BUILDDEPS += $(OUT_ZIRCON_ZIMAGE) $(ZIRCON_BOARD_LIST)

# include rules for our various arm64 boards
include $(LOCAL_DIR)/*/rules.mk
