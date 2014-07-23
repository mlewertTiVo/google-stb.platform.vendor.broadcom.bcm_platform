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

include $(REFSW_PATH)/bin/include/platform_app.inc

# Nexus multi-process, client-server related CFLAGS
MP_CFLAGS = -DANDROID_CLIENT_SECURITY_MODE=$(ANDROID_CLIENT_SECURITY_MODE)

ifeq ($(ANDROID_SUPPORTS_SD_DISPLAY),y)
MP_CFLAGS += -DANDROID_SUPPORTS_SD_DISPLAY=1
else
MP_CFLAGS += -DANDROID_SUPPORTS_SD_DISPLAY=0
endif


include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libbinder $(NEXUS_LIB)
LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include

LOCAL_SRC_FILES := 	\
	nexusfrontendservice.cpp \
	nexusfrontend_base.cpp	
	
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libnexusfrontendservice
LOCAL_CFLAGS:= $(NEXUS_CFLAGS) -DANDROID $(MP_CFLAGS)
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI

include $(BUILD_SHARED_LIBRARY)

