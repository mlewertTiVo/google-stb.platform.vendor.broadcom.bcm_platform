# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# gralloc version: 0.x (default) for pre-O android; 1.0 starting with O.
# in O, both v-0.x and v-1.0 continue to be supported.
ifeq ($(HAL_GR_VERSION),)
HAL_GR_VERSION := v-0.x
endif

LOCAL_PRELINK_MODULE := false
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_SHARED_LIBRARIES := libnexus \
                          liblog \
                          libcutils \
                          libbinder \
                          libutils \
                          libdl \
                          libnxwrap \
                          libnxbinder \
                          libnxevtsrc \
                          libnxclient

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/drivers/nx_ashmem
ifeq ($(V3D_VARIANT),)
V3D_VARIANT := v3d
endif
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/gpu/$(V3D_VARIANT)/driver/interface/khronos/include
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/libs/arect/include
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/libs/nativewindow/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/gralloc/${HAL_GR_VERSION}

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += -DLOG_TAG=\"bcm-gr\"
LOCAL_CFLAGS += -DV3D_VARIANT_$(V3D_VARIANT)
# fix warnings!
LOCAL_CFLAGS += -Werror
ifeq ($(LOCAL_ARM_AARCH64),y)
ifeq ($(LOCAL_ARM_AARCH64_NOT_ABI_COMPATIBLE),y)
LOCAL_CFLAGS += -DV3D_DLOPEN_PATH=\"/vendor/lib/egl/\"
else
LOCAL_CFLAGS += -DV3D_DLOPEN_PATH=\"/vendor/lib64/egl/\"
endif
else
LOCAL_CFLAGS += -DV3D_DLOPEN_PATH=\"/vendor/lib/egl/\"
endif

LOCAL_SRC_FILES := $(HAL_GR_VERSION)/gralloc.cpp
ifeq ($(HAL_GR_VERSION),v-0.x)
LOCAL_SRC_FILES += $(HAL_GR_VERSION)/framebuffer.cpp
LOCAL_SRC_FILES += $(HAL_GR_VERSION)/mapper.cpp
LOCAL_SRC_FILES += $(HAL_GR_VERSION)/gralloc_destripe.cpp
endif

LOCAL_MODULE := gralloc.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
