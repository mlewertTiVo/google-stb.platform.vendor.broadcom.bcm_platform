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

ifeq ($(ANDROID_SUPPORTS_NXCLIENT),y)

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

include $(REFSW_PATH)/bin/include/platform_app.inc
ifeq ($(ANDROID_SUPPORTS_NXCLIENT),y)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc
endif

# Nexus multi-process, client-server related CFLAGS
MP_CFLAGS = -DANDROID_CLIENT_SECURITY_MODE=$(ANDROID_CLIENT_SECURITY_MODE)

ifeq ($(ANDROID_SUPPORTS_SD_DISPLAY),y)
MP_CFLAGS += -DANDROID_SUPPORTS_SD_DISPLAY=1
else
MP_CFLAGS += -DANDROID_SUPPORTS_SD_DISPLAY=0
endif

ifeq ($(ANDROID_SUPPORTS_NXCLIENT),y)

MP_CFLAGS += -DANDROID_SUPPORTS_NXCLIENT
ifeq ($(ANDROID_SUPPORTS_EMBEDDED_NXSERVER),y)
MP_CFLAGS += -DANDROID_SUPPORTS_EMBEDDED_NXSERVER=1
else
MP_CFLAGS += -DANDROID_SUPPORTS_EMBEDDED_NXSERVER=0
endif

ifeq ($(ANDROID_SUPPORTS_NXCLIENT_HDMI_STATUS),y)
MP_CFLAGS += -DANDROID_SUPPORTS_NXCLIENT_HDMI_STATUS
endif

ifeq ($(ANDROID_SUPPORTS_ANALOG_INPUT),y)
MP_CFLAGS += -DANDROID_SUPPORTS_ANALOG_INPUT
endif

endif

ifeq ($(ANDROID_ENABLE_HDMI_LEGACY),y)
MP_CFLAGS += -DANDROID_SUPPORTS_HDMI_LEGACY=1
else
MP_CFLAGS += -DANDROID_SUPPORTS_HDMI_LEGACY=0
endif

ifeq ($(ANDROID_USES_TRELLIS_WM),y)
MP_CFLAGS += -DANDROID_USES_TRELLIS_WM=1 
MP_CFLAGS += -DTRELLIS_HAS_NEXUS -DB_HAS_SSL -DB_HAS_HLS_PROTOCOL_SUPPORT -DB_HAS_HTTP_AES_SUPPORT -DTRELLIS_NSC_WINDOWMANAGER
MP_CFLAGS += -DTRELLIS_MULTI_INPUTDRIVER
MP_CFLAGS += -DTRELLIS_HAS_BME -DTRELLIS_HAS_BPM -DTRELLIS_HAS_3D_GFX -DTRELLIS_USES_BOOST_TR1 -DTRELLIS_ANDROID_BUILD
LOCAL_C_INCLUDES += $(APPLIBS_TOP)/broadcom/trellis-core/common \
                    $(APPLIBS_TOP)/broadcom/trellis-core/osapi \
                    $(APPLIBS_TOP)/broadcom/trellis-core/rpc/orb \
                    $(APPLIBS_TOP)/broadcom/trellis-core/rpc \
                    $(APPLIBS_TOP)/broadcom/services/application/components/applicationmanager/include/ \
                    $(APPLIBS_TOP)/broadcom/services/application/components/androidwindowmanager/impl/ \
                    $(APPLIBS_TOP)/broadcom/services/application/components/windowmanager/impl/ \
                    $(APPLIBS_TOP)/broadcom/services/application/components/windowmanager/include/ \
                    $(APPLIBS_TOP)/broadcom/services/application/components/applicationmanager/impl \
                    $(APPLIBS_TOP)/broadcom/services/application/components/application/include/ \
                    $(APPLIBS_TOP)/broadcom/services/application/components/application/proxy/generated \
                    $(APPLIBS_TOP)/broadcom/services/application/components/application/proxy \
                    $(APPLIBS_TOP)/broadcom/services/application/components/applicationmanager/proxy \
                    $(APPLIBS_TOP)/broadcom/services/application/components/windowmanager/proxy \
                    $(APPLIBS_TOP)/broadcom/services/application/components/window/proxy \
                    $(APPLIBS_TOP)/broadcom/services/application/components/window/include \
                    $(APPLIBS_TOP)/broadcom/services/application/components/window/proxy/generated \
                    $(APPLIBS_TOP)/broadcom/services/application/components/servicemanager/proxy/generated \
                    $(APPLIBS_TOP)/broadcom/services/application/components/servicemanager/proxy \
                    $(APPLIBS_TOP)/broadcom/services/application/components/servicemanager/include \
                    $(APPLIBS_TOP)/broadcom/services/application/components/usernotification/proxy \
                    $(APPLIBS_TOP)/broadcom/services/application/components/usernotification/include \
                    $(APPLIBS_TOP)/opensource/boost/boost_1_52_0 \
                    $(APPLIBS_TOP)/opensource/boost/boost_1_52_0/boost \
                    external/stlport/stlport \
                    bionic
else
MP_CFLAGS += -DANDROID_USES_TRELLIS_WM=0
endif

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libbinder $(NEXUS_LIB) libstagefright_foundation

ifeq ($(ANDROID_USES_TRELLIS_WM),y)
LOCAL_SHARED_LIBRARIES += libstlport
endif

LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include
LOCAL_C_INCLUDES += $(REFSW_PATH)/../libnexusipc

ifeq ($(ANDROID_SUPPORTS_NXCLIENT),y)
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
ifeq ($(ANDROID_SUPPORTS_EMBEDDED_NXSERVER),y)
LOCAL_SHARED_LIBRARIES += libnxserver libnxclient_local
else
LOCAL_SHARED_LIBRARIES += libnxclient
endif
endif

LOCAL_SRC_FILES := 	\
	nexusservice.cpp

ifneq ($(findstring NEXUS_HAS_CEC, ${NEXUS_CFLAGS}),)
LOCAL_SRC_FILES +=	\
	nexuscecservice.cpp
ifeq ($(ANDROID_SUPPORTS_NXCLIENT),y)
LOCAL_SRC_FILES +=	\
	nexusnxcecservice.cpp
endif
endif

ifeq ($(ANDROID_SUPPORTS_NXCLIENT),y)
LOCAL_SRC_FILES += nexusnxservice.cpp
endif
	
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libnexusservice
LOCAL_CFLAGS:= $(NEXUS_CFLAGS) -DANDROID $(MP_CFLAGS)
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI

ifeq ($(ANDROID_ENABLE_BCM_OMX_PROTOTYPE),y)
LOCAL_CFLAGS += -DENABLE_BCM_OMX_PROTOTYPE
endif

include $(BUILD_SHARED_LIBRARY)
