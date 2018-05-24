# Copyright 2018 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_DIR := $(GET_LOCAL_DIR)

#MODULE := $(LOCAL_DIR).proxy
MODULE := $(LOCAL_DIR)

MODULE_TYPE := driver

MODULE_NAME := nand

MODULE_SRCS += \
    $(LOCAL_DIR)/nand.c \
    $(LOCAL_DIR)/nand_drivertest.c \

MODULE_COMPILEFLAGS := -I$(LOCAL_DIR)/tests

MODULE_STATIC_LIBS := \
    system/ulib/ddk \
    system/ulib/sync \

MODULE_LIBS := \
    system/ulib/driver \
    system/ulib/c \
    system/ulib/zircon \

include make/module.mk

# Unit tests:

MODULE := $(LOCAL_DIR).test

MODULE_NAME := nandtest

MODULE_TYPE := usertest

TEST_DIR := $(LOCAL_DIR)/tests

MODULE_SRCS += \
    $(TEST_DIR)/nandtest.c \

MODULE_COMPILEFLAGS := -I$(LOCAL_DIR)

MODULE_STATIC_LIBS := \
    system/ulib/fbl \
    system/ulib/ddk \
    system/ulib/sync \
    system/ulib/zx \

MODULE_LIBS := \
    system/ulib/c \
    system/ulib/fdio \
    system/ulib/fs-management \
    system/ulib/unittest \
    system/ulib/zircon \

include make/module.mk
