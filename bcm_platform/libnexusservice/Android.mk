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
NEXUS_TOP ?= $(LOCAL_PATH)/../../../../../../../../../nexus

ifeq ($(NEXUS_MODE),proxy)
NEXUS_LIB=libnexus
else
ifeq ($(NEXUS_WEBCPU),core1_server)
NEXUS_LIB=libnexus_webcpu
else
NEXUS_LIB=libnexus_client
endif
endif

include $(NEXUS_TOP)/nxclient/include/nxclient.inc
$(warning NX-BUILD: CFLAGS ${NEXUS_CFLAGS}, CLI-CFLAGS: ${NXCLIENT_CFLAGS}, CLI-INC: ${NXCLIENT_INCLUDES})

# Nexus multi-process, client-server related CFLAGS
MP_CFLAGS = -DANDROID_CLIENT_SECURITY_MODE=$(ANDROID_CLIENT_SECURITY_MODE)

ifeq ($(ANDROID_SUPPORTS_SD_DISPLAY),y)
MP_CFLAGS += -DANDROID_SUPPORTS_SD_DISPLAY=1
else
MP_CFLAGS += -DANDROID_SUPPORTS_SD_DISPLAY=0
endif

MP_CFLAGS += -DANDROID_SUPPORTS_NXCLIENT
ifeq ($(ANDROID_SUPPORTS_EMBEDDED_NXSERVER),y)
MP_CFLAGS += -DANDROID_SUPPORTS_EMBEDDED_NXSERVER=1
else
MP_CFLAGS += -DANDROID_SUPPORTS_EMBEDDED_NXSERVER=0
endif

ifeq ($(ANDROID_SUPPORTS_NXCLIENT_HDMI_STATUS),y)
MP_CFLAGS += -DANDROID_SUPPORTS_NXCLIENT_HDMI_STATUS
endif

ifeq ($(ANDROID_SUPPORTS_NXCLIENT_VIDEO_WINDOW_TYPE),y)
MP_CFLAGS += -DANDROID_SUPPORTS_NXCLIENT_VIDEO_WINDOW_TYPE
endif

ifeq ($(ANDROID_SUPPORTS_ANALOG_INPUT),y)
MP_CFLAGS += -DANDROID_SUPPORTS_ANALOG_INPUT
endif

ifeq ($(ANDROID_ENABLE_HDMI_LEGACY),y)
MP_CFLAGS += -DANDROID_SUPPORTS_HDMI_LEGACY=1
else
MP_CFLAGS += -DANDROID_SUPPORTS_HDMI_LEGACY=0
endif

MP_CFLAGS += -DANDROID_USES_TRELLIS_WM=0

ifeq ($(ANDROID_ENABLE_HDMI_HDCP),y)
MP_CFLAGS += -DANDROID_ENABLE_HDMI_HDCP=1
else
MP_CFLAGS += -DANDROID_ENABLE_HDMI_HDCP=0
endif

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libbinder libnexusipceventlistener $(NEXUS_LIB) libstagefright_foundation

ifeq ($(ANDROID_USES_TRELLIS_WM),y)
LOCAL_SHARED_LIBRARIES += libstlport
endif

LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include
LOCAL_C_INCLUDES += $(REFSW_PATH)/../libnexusipc

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
ifeq ($(ANDROID_SUPPORTS_EMBEDDED_NXSERVER),y)
LOCAL_SHARED_LIBRARIES += libnxserver libnxclient_local
else
LOCAL_SHARED_LIBRARIES += libnxclient
endif

LOCAL_SRC_FILES := \
    nexusservice.cpp

ifneq ($(findstring NEXUS_HAS_CEC,$(NEXUS_APP_DEFINES)),)
LOCAL_SRC_FILES += \
    nexuscecservice.cpp \
    nexusnxcecservice.cpp
endif

LOCAL_SRC_FILES += nexusnxservice.cpp

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libnexusservice
LOCAL_CFLAGS:= $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID $(MP_CFLAGS)
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI

ifeq ($(ANDROID_ENABLE_BCM_OMX_PROTOTYPE),y)
LOCAL_CFLAGS += -DENABLE_BCM_OMX_PROTOTYPE
endif

include $(BUILD_SHARED_LIBRARY)
