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
APPLIBS_TOP ?=$(LOCAL_PATH)/../../../../../../../..
NEXUS_TOP ?= $(LOCAL_PATH)/../../../../../../../../../nexus

# Nexus multi-process, client-server related CFLAGS
MP_CFLAGS = -DANDROID_CLIENT_SECURITY_MODE=$(ANDROID_CLIENT_SECURITY_MODE)

ifeq ($(ANDROID_SUPPORTS_NXCLIENT),y)
MP_CFLAGS += -DANDROID_SUPPORTS_NXCLIENT

ifeq ($(NEXUS_MODE),proxy)
NEXUS_LIB=libnexus
else
ifeq ($(NEXUS_WEBCPU),core1_server)
NEXUS_LIB=libnexus_webcpu
else
NEXUS_LIB=libnexus_client
endif
endif

else
NEXUS_LIB=libnexus
endif

ifeq ($(ANDROID_UNDER_LXC),y)
MP_CFLAGS += -DANDROID_UNDER_LXC=1
else
MP_CFLAGS += -DANDROID_UNDER_LXC=0
endif

ifeq ($(ANDROID_SUPPORTS_FRONTEND_SERVICE),y)
MP_CFLAGS += -DANDROID_SUPPORTS_FRONTEND_SERVICE=1
else
MP_CFLAGS += -DANDROID_SUPPORTS_FRONTEND_SERVICE=0
endif

ifeq ($(ATP_BUILD),y)
MP_CFLAGS += -DATP_BUILD=1
else
MP_CFLAGS += -DATP_BUILD=0
endif

include $(CLEAR_VARS)

ifeq ($(ANDROID_ENABLE_REMOTEA),y)
LOCAL_CFLAGS += -DANDROID_ENABLE_REMOTEA
endif

include $(NEXUS_TOP)/nxclient/include/nxclient.inc
$(warning NX-BUILD: CFLAGS ${NEXUS_CFLAGS}, CLI-CFLAGS: ${NXCLIENT_CFLAGS}, CLI-INC: ${NXCLIENT_INCLUDES})

LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := liblog libcutils libbinder libutils libnexusipcclient libnexusservice $(NEXUS_LIB)

ifeq ($(ANDROID_SUPPORTS_FRONTEND_SERVICE),y)
LOCAL_SHARED_LIBRARIES += libnexusfrontendservice
LOCAL_C_INCLUDES += $(REFSW_PATH)/../libnexusfrontendservice
endif 

LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include \
                    $(REFSW_PATH)/../libnexusservice
LOCAL_C_INCLUDES += $(REFSW_PATH)/../libnexusipc

ifeq ($(ANDROID_SUPPORTS_NXCLIENT),y)
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
endif

LOCAL_SRC_FILES := \
                nexusinit.cpp

LOCAL_MODULE := nexusinit
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID $(MP_CFLAGS)
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI
ifeq ($(SBS_USES_TRELLIS_INPUT_EVENTS),y)
LOCAL_CFLAGS += -DSBS_USES_TRELLIS_INPUT_EVENTS
endif
ifeq ($(BCM_OMX_SUPPORT_ENCODER),y)
LOCAL_CFLAGS += -DBCM_OMX_SUPPORT_ENCODER
endif

include $(BUILD_EXECUTABLE)
