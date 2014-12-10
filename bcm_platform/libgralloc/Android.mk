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

REFSW_PATH :=vendor/broadcom/bcm_platform/brcm_nexus
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# HAL module implementation, not prelinked and stored in
# hw/<OVERLAY_HARDWARE_MODULE_ID>.<ro.product.board>.so

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_SHARED_LIBRARIES := libnexus \
                          liblog \
                          libcutils \
                          libbinder \
                          libutils \
                          libnexusipcclient \
                          libdl

LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include \
                    $(REFSW_PATH)/../libnexusservice
LOCAL_C_INCLUDES += $(REFSW_PATH)/../libnexusipc
LOCAL_C_INCLUDES += $(REFSW_PATH)/../../drivers/nx_ashmem
LOCAL_C_INCLUDES += $(REFSW_PATH)/../../refsw/rockford/middleware/v3d/interface/khronos/include

REMOVE_NEXUS_CFLAGS := -Wstrict-prototypes -march=armv7-a
MANGLED_NEXUS_CFLAGS := $(filter-out $(REMOVE_NEXUS_CFLAGS), $(NEXUS_CFLAGS))

LOCAL_CFLAGS := -DLOG_TAG=\"gralloc\" $(MANGLED_NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID $(MP_CFLAGS)
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI

LOCAL_SRC_FILES := \
        gralloc.cpp \
        framebuffer.cpp \
        mapper.cpp

LOCAL_MODULE := gralloc.bcm_platform
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
