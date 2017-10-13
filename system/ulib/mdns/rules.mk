# Copyright 2017 The Fuchsia Authors. All rights reserved.
# # Use of this source code is governed by a BSD-style license that can be
# # found in the LICENSE file.

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_TYPE := userlib

MODULE_SRCS += $(LOCAL_DIR)/mdns.c

MODULE_EXPORT := a

MODULE_HEADER_DEPS := system/ulib/inet6

MODULE_LIBS := system/ulib/inet6 system/ulib/zircon system/ulib/c

MODULE_COMPILEFLAGS := -DMDNS_USERLIB

include make/module.mk

MODULE := $(LOCAL_DIR).mdns-example

MODULE_TYPE := hostapp

MODULE_SRCS := $(LOCAL_DIR)/mdns.c $(LOCAL_DIR)/mdns-example.c

MODULE_NAME := mdns-example

MODULE_COMPILEFLAGS := -I$(LOCAL_DIR)/include -std=c11 -DTFTP_HOSTLIB

include make/module.mk

#MODULE := $(LOCAL_DIR).hostlib

#MODULE_NAME := mdns

#MODULE_TYPE := hostlib

#MODULE_SRCS := $(LOCAL_DIR)/mdns.c

#MODULE_COMPILEFLAGS := -DMDNS_HOSTLIB

#include make/module.mk
