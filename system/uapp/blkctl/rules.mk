# Copyright 2016 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_TYPE := userapp
MODULE_GROUP := misc

MODULE_SRCS := \
    $(LOCAL_DIR)/blkctl.cpp

MODULE_NAME := blkctl

MODULE_LIBS := \
    system/ulib/blkctl \
    system/ulib/c \
    system/ulib/crypto \
    system/ulib/digest \
    system/ulib/fdio \
    system/ulib/fs-management \
    system/ulib/zircon \
    system/ulib/zxcrypt \

MODULE_STATIC_LIBS := \
    system/ulib/ddk \
    system/ulib/fbl \
    system/ulib/fvm \
    system/ulib/fs \
    system/ulib/gpt \
    system/ulib/zx \
    system/ulib/zxcpp \

include make/module.mk
