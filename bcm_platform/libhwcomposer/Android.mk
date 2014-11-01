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

ifeq ($(NEXUS_MODE),proxy)
NEXUS_LIB=libnexus
else
ifeq ($(NEXUS_WEBCPU),core1_server)
NEXUS_LIB=libnexus_webcpu
else
NEXUS_LIB=libnexus_client
endif
endif

# HAL module implemenation, not prelinked and stored in
# hw/<OVERLAY_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)

# Nexus multi-process, client-server related CFLAGS
MP_CFLAGS = -DANDROID_CLIENT_SECURITY_MODE=$(ANDROID_CLIENT_SECURITY_MODE)

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libEGL libcutils libdl libbinder libutils libnexusipcclient $(NEXUS_LIB)

# Make sure gralloc is built first
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_SHARED_LIBRARIES)/hw/gralloc.bcm_platform.so
LOCAL_LDFLAGS := -l:gralloc.bcm_platform.so -Lout/target/product/bcm_platform/system/lib/hw
LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include \
					$(REFSW_PATH)/../libnexusservice
LOCAL_C_INCLUDES += $(REFSW_PATH)/../libnexusipc
LOCAL_C_INCLUDES += $(REFSW_PATH)/../libgralloc
					
LOCAL_CFLAGS += $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID -DLOG_TAG=\"hwcomposer\" $(MP_CFLAGS)
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI

LOCAL_SRC_FILES := hwcomposer.cpp
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := hwcomposer.bcm_platform

include $(BUILD_SHARED_LIBRARY)
