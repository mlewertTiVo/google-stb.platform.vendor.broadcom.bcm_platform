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
$(warning NX-BUILD: CFG: ${DEVICE_REFSW_BUILD_CONFIG}, CFLAGS ${NEXUS_CFLAGS}, CLI-CFLAGS: ${NXCLIENT_CFLAGS}, CLI-INC: ${NXCLIENT_INCLUDES})

# Nexus multi-process, client-server related CFLAGS
MP_CFLAGS = -DANDROID_CLIENT_SECURITY_MODE=$(ANDROID_CLIENT_SECURITY_MODE)

ifeq ($(ANDROID_USES_TRELLIS_WM),y)
MP_CFLAGS += -DANDROID_USES_TRELLIS_WM=1 
MP_CFLAGS += -DTRELLIS_HAS_NEXUS -DB_HAS_SSL -DB_HAS_HLS_PROTOCOL_SUPPORT -DB_HAS_HTTP_AES_SUPPORT -DTRELLIS_NSC_WINDOWMANAGER
MP_CFLAGS += -DTRELLIS_MULTI_INPUTDRIVER -DTRELLIS_IPC_UNIX_SOCKET_PATH="/system/etc/trellis.socketipc."
MP_CFLAGS += -DTRELLIS_HAS_BME -DTRELLIS_HAS_BPM -DTRELLIS_HAS_3D_GFX -DTRELLIS_USES_BOOST_TR1 -DTRELLIS_ANDROID_BUILD

else
MP_CFLAGS += -DANDROID_USES_TRELLIS_WM=0
endif

ifeq ($(ANDROID_SUPPORTS_NXCLIENT),y)
MP_CFLAGS += -DANDROID_SUPPORTS_NXCLIENT

ifeq ($(ANDROID_SUPPORTS_NXCLIENT_PICTURE_QUALITY_SETTINGS),y)
MP_CFLAGS += -DANDROID_SUPPORTS_NXCLIENT_PICTURE_QUALITY_SETTINGS
endif

ifeq ($(ANDROID_SUPPORTS_NXCLIENT_EXTENDED_DISPLAY_SETTINGS),y)
MP_CFLAGS += -DANDROID_SUPPORTS_NXCLIENT_EXTENDED_DISPLAY_SETTINGS
endif

ifeq ($(ANDROID_SUPPORTS_NXCLIENT_VIDEO_WINDOW_TYPE),y)
MP_CFLAGS += -DANDROID_SUPPORTS_NXCLIENT_VIDEO_WINDOW_TYPE
endif

ifeq ($(ANDROID_SUPPORTS_NXCLIENT_CONFIG),y)
MP_CFLAGS += -DANDROID_SUPPORTS_NXCLIENT_CONFIG
endif

endif

#----------------------
# libnexusipceventlistener 
#----------------------
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libbinder

LOCAL_C_INCLUDES += $(TOP)/frameworks/native/include

LOCAL_SRC_FILES := INexusHdmiCecMessageEventListener.cpp \
                   INexusHdmiHotplugEventListener.cpp

LOCAL_CFLAGS := -DANDROID
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libnexusipceventlistener

include $(BUILD_SHARED_LIBRARY)

#----------------------
# libnexusipcclient 
#----------------------
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libbinder libnexusipceventlistener $(NEXUS_LIB)

LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include \
                    $(REFSW_PATH)/../libnexusservice \
                    $(TOP)/frameworks/native/include

ifeq ($(ANDROID_USES_TRELLIS_WM),y)
LOCAL_SHARED_LIBRARIES += libstlport
LOCAL_C_INCLUDES += $(APPLIBS_TOP)/broadcom/trellis-core/common \
                    $(APPLIBS_TOP)/broadcom/trellis-core/osapi \
                    $(APPLIBS_TOP)/broadcom/trellis-core/rpc/orb \
                    $(APPLIBS_TOP)/broadcom/trellis-core/rpc \
                    $(APPLIBS_TOP)/broadcom/services/application/components/androidwindowmanager/impl \
                    $(APPLIBS_TOP)/broadcom/services/application/components/windowmanager/impl \
                    $(APPLIBS_TOP)/broadcom/services/application/components/windowmanager/include \
                    $(APPLIBS_TOP)/broadcom/services/application/components/windowmanager/proxy/generated \
                    $(APPLIBS_TOP)/broadcom/services/application/components/applicationmanager/impl \
                    $(APPLIBS_TOP)/broadcom/services/application/components/applicationmanager/include \
                    $(APPLIBS_TOP)/broadcom/services/application/components/applicationmanager/proxy/generated \
                    $(APPLIBS_TOP)/broadcom/services/application/components/application/include \
                    $(APPLIBS_TOP)/broadcom/services/application/components/application/proxy/generated \
                    $(APPLIBS_TOP)/broadcom/services/application/components/window/include \
                    $(APPLIBS_TOP)/broadcom/services/application/components/window/proxy/generated \
                    $(APPLIBS_TOP)/broadcom/services/application/components/servicemanager/include \
                    $(APPLIBS_TOP)/broadcom/services/application/components/servicemanager/proxy/generated \
                    $(APPLIBS_TOP)/broadcom/services/application/components/packagemanager/include \
                    $(APPLIBS_TOP)/broadcom/services/application/components/packagemanager/proxy/generated \
                    $(APPLIBS_TOP)/broadcom/services/application/components/usernotification/include \
                    $(APPLIBS_TOP)/broadcom/services/application/components/usernotification/proxy/generated \
                    $(APPLIBS_TOP)/opensource/boost/boost_1_52_0 \
                    $(APPLIBS_TOP)/opensource/boost/boost_1_52_0/boost \
                    external/stlport/stlport \
                    bionic
endif

ifeq ($(SBS_USES_TRELLIS_INPUT_EVENTS),y)
LOCAL_STATIC_LIBRARIES := libtrellis_application_androidwindowmanager_impl \
                          libtrellis_application_delegatewindowmanager_impl \
                          libtrellis_application_windowmanager_impl \
                          libtrellis_application_applicationmanager_impl \
                          libtrellis_application_authorizationattributesmanager_impl \
                          libtrellis_application_authorizationattributesmanager_proxy \
                          libtrellis_application_packagemanager_proxy \
                          libtrellis_application_windowmanager_proxy \
                          libtrellis_application_servicemanager_proxy \
                          libtrellis_application_applicationmanager_proxy \
                          libtrellis_application_service_proxy \
                          libtrellis_application_inputdriver_proxy \
                          libtrellis_application_window_proxy \
                          libtrellis_application_application_proxy \
                          libtrellis_application_usernotification_proxy \
                          libtrellis_trellis-core 
endif

#
# Switch between the NXclient APIs or the local server APIs
# 
LOCAL_SRC_FILES := nexus_ipc_client_factory.cpp nexus_ipc_client_base.cpp nexus_ipc_client.cpp
ifeq ($(ANDROID_SUPPORTS_NXCLIENT),y)
LOCAL_SRC_FILES += nexus_nx_client.cpp
endif

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libnexusipcclient
LOCAL_CFLAGS:= $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID $(MP_CFLAGS)
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI

ifeq ($(ANDROID_SUPPORTS_NXCLIENT),y)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_SHARED_LIBRARIES += libnxclient
endif

ifeq ($(SBS_USES_TRELLIS_INPUT_EVENTS),y)
LOCAL_CFLAGS += -DSBS_USES_TRELLIS_INPUT_EVENTS
endif


include $(BUILD_SHARED_LIBRARY)

#----------------------
# libnexuseglclient 
#----------------------
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libbinder libnexusipcclient libnexusipceventlistener $(NEXUS_LIB)

LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include \
                    $(REFSW_PATH)/../libnexusservice \
                    $(TOP)/frameworks/native/include

ifeq ($(ANDROID_USES_TRELLIS_WM),y)
LOCAL_SHARED_LIBRARIES += libstlport
LOCAL_C_INCLUDES += $(APPLIBS_TOP)/broadcom/trellis-core/common \
                    $(APPLIBS_TOP)/broadcom/trellis-core/osapi \
                    $(APPLIBS_TOP)/broadcom/trellis-core/rpc/orb \
                    $(APPLIBS_TOP)/broadcom/trellis-core/rpc \
                    $(APPLIBS_TOP)/broadcom/services/application/components/androidwindowmanager/impl \
                    $(APPLIBS_TOP)/broadcom/services/application/components/windowmanager/impl \
                    $(APPLIBS_TOP)/broadcom/services/application/components/windowmanager/include \
                    $(APPLIBS_TOP)/broadcom/services/application/components/windowmanager/proxy/generated \
                    $(APPLIBS_TOP)/broadcom/services/application/components/applicationmanager/impl \
                    $(APPLIBS_TOP)/broadcom/services/application/components/applicationmanager/include \
                    $(APPLIBS_TOP)/broadcom/services/application/components/applicationmanager/proxy/generated \
                    $(APPLIBS_TOP)/broadcom/services/application/components/application/include \
                    $(APPLIBS_TOP)/broadcom/services/application/components/application/proxy/generated \
                    $(APPLIBS_TOP)/broadcom/services/application/components/window/include \
                    $(APPLIBS_TOP)/broadcom/services/application/components/window/proxy/generated \
                    $(APPLIBS_TOP)/broadcom/services/application/components/servicemanager/include \
                    $(APPLIBS_TOP)/broadcom/services/application/components/servicemanager/proxy/generated \
                    $(APPLIBS_TOP)/broadcom/services/application/components/packagemanager/include \
                    $(APPLIBS_TOP)/broadcom/services/application/components/packagemanager/proxy/generated \
                    $(APPLIBS_TOP)/broadcom/services/application/components/usernotification/include \
                    $(APPLIBS_TOP)/broadcom/services/application/components/usernotification/proxy/generated \
                    $(APPLIBS_TOP)/opensource/boost/boost_1_52_0 \
                    $(APPLIBS_TOP)/opensource/boost/boost_1_52_0/boost \
                    external/stlport/stlport \
                    bionic
endif

LOCAL_SRC_FILES := nexus_egl_client.cpp

LOCAL_CFLAGS := $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID $(MP_CFLAGS)
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI

ifeq ($(ANDROID_SUPPORTS_NXCLIENT),y)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_SHARED_LIBRARIES += libnxclient
endif

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libnexuseglclient

include $(BUILD_SHARED_LIBRARY)
