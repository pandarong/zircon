# Copyright 2016 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_TYPE := usertest

MODULE_NAME := dispatcher-isolate-test

MODULE_SRCS += \
	$(LOCAL_DIR)/main.cpp \
	kernel/object/dispatcher.cpp \
	kernel/object/channel_dispatcher.cpp \
	kernel/object/event_dispatcher.cpp \
	kernel/object/message_packet.cpp \
	kernel/object/handle.cpp \
	kernel/object/state_tracker.cpp

MODULE_LIBS := \
    system/ulib/unittest \
    system/ulib/fdio \
    system/ulib/zircon \
    system/ulib/c

MODULE_STATIC_LIBS := \
    system/ulib/zxcpp \
    system/ulib/fbl

MODULE_COMPILEFLAGS := \
  -Ikernel/object/include \
  -Isystem/ulib/fbl/include \
  -I$(LOCAL_DIR)/stub-include

include make/module.mk
