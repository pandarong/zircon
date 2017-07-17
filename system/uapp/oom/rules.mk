# Copyright 2017 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR).ranker

MODULE_NAME := oom-ranker

MODULE_TYPE := userapp

MODULE_SRCS += \
    $(LOCAL_DIR)/canned_jobs.cpp \
    $(LOCAL_DIR)/job.cpp \
    $(LOCAL_DIR)/ranker.cpp \
    $(LOCAL_DIR)/resources.c

# Tests
MODULE_SRCS += \
    $(LOCAL_DIR)/fake_syscalls.cpp

MODULE_STATIC_LIBS := \
    system/ulib/task-utils \
    system/ulib/zxcpp \
    system/ulib/zx \
    system/ulib/fbl

MODULE_LIBS := \
    system/ulib/fdio \
    system/ulib/zircon \
    system/ulib/c

include make/module.mk
