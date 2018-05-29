# Copyright 2018 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_TYPE := driver

MODULE_SRCS += \
    $(LOCAL_DIR)/astro-display.c \
    $(LOCAL_DIR)/backlight.c \
    $(LOCAL_DIR)/canvas.c \
    $(LOCAL_DIR)/dsi.c \
    $(LOCAL_DIR)/lcd.c \
    $(LOCAL_DIR)/display_debug.c \
    $(LOCAL_DIR)/display_config.c \
    $(LOCAL_DIR)/display_clock.c \
    $(LOCAL_DIR)/mipi_dsi.c \

MODULE_STATIC_LIBS := system/ulib/ddk system/ulib/sync

MODULE_LIBS := system/ulib/driver system/ulib/zircon system/ulib/c

include make/module.mk
