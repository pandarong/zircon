# Copyright 2017 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_TYPE := userapp

MODULE_NAME := make-zxcrypt

MODULE_GROUP := core

MODULE_SRCS := \
    $(LOCAL_DIR)/main.cpp \

MODULE_LIBS := \
    system/ulib/c \
    system/ulib/zircon \
    system/ulib/fdio \
    system/ulib/crypto \
    system/ulib/fs-management \
    system/ulib/zxcrypt \

MODULE_STATIC_LIBS := \
    third_party/ulib/cryptolib \
    third_party/ulib/uboringssl \
    system/ulib/ddk \
    system/ulib/fvm \
    system/ulib/fs \
    system/ulib/sync \
    system/ulib/zx \
    system/ulib/zxcpp \
    system/ulib/fbl \

include make/module.mk
