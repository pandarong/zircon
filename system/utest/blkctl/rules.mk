# Copyright 2018 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_TYPE := usertest

MODULE_USERTEST_GROUP := misc

MODULE_NAME := blkctl-test

MODULE_SRCS := \
    $(LOCAL_DIR)/main.cpp \
    $(LOCAL_DIR)/utils.cpp \

MODULE_SRCS += \
    $(LOCAL_DIR)/command.cpp \
    $(LOCAL_DIR)/ramdisk.cpp \
    $(LOCAL_DIR)/fvm.cpp \

MODULE_LIBS := \
    system/ulib/blkctl \
    system/ulib/c \
    system/ulib/digest \
    system/ulib/fdio \
    system/ulib/fs-management \
    system/ulib/unittest \
    system/ulib/zircon \

MODULE_STATIC_LIBS := \
    system/ulib/fbl \
    system/ulib/fvm \
    system/ulib/fs \
    system/ulib/gpt \
    system/ulib/zx \

include make/module.mk
