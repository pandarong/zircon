# Copyright 2016 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_TYPE := driver

MODULE_SRCS += \
    $(LOCAL_DIR)/dwc2.c \
    $(LOCAL_DIR)/dwc2-endpoints.c \
    $(LOCAL_DIR)/dwc2-irq.c \

MODULE_STATIC_LIBS := system/ulib/ddk \

MODULE_LIBS := system/ulib/driver \
               system/ulib/c \
               system/ulib/zircon \

include make/module.mk
