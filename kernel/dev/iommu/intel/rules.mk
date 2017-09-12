# Copyright 2017 The Fuchsia Authors
#
# Use of this source code is governed by a MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT

LOCAL_DIR := $(GET_LOCAL_DIR)
MODULE := $(LOCAL_DIR)

MODULE_SRCS := \
    $(LOCAL_DIR)/context_table_state.cpp \
    $(LOCAL_DIR)/device_context.cpp \
    $(LOCAL_DIR)/domain_allocator.cpp \
    $(LOCAL_DIR)/intel_iommu.cpp \
    $(LOCAL_DIR)/iommu_impl.cpp \
    $(LOCAL_DIR)/iommu_page.cpp \
    $(LOCAL_DIR)/second_level_pt.cpp \

MODULE_DEPS := \
	kernel/arch/x86/page_tables \
	kernel/dev/pcie \
	kernel/lib/bitmap \
	kernel/lib/hwreg \
	kernel/lib/fbl \
	third_party/lib/safeint \

include make/module.mk
