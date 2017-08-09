# Copyright 2017 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_TYPE := userapp

MODULE_SRCS += \
    $(LOCAL_DIR)/taskgrinder.cpp

MODULE_NAME := taskgrinder

MODULE_LIBS := \
    system/ulib/fdio \
    system/ulib/zircon \
    system/ulib/c

MODULE_STATIC_LIBS := \
    system/ulib/zx \
    system/ulib/fbl \
    system/ulib/zxcpp

include make/module.mk
