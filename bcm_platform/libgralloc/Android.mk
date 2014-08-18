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
ifeq ($(GOOGLE_TREE_BUILD),n)
NEXUS_TOP := $(LOCAL_PATH)/../../../../../../../../../nexus
endif

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

include $(REFSW_PATH)/bin/include/platform_app.inc

# Nexus multi-process, client-server related 
MP_CFLAGS = -DANDROID_CLIENT_SECURITY_MODE=$(ANDROID_CLIENT_SECURITY_MODE)

ifeq ($(ANDROID_USES_TRELLIS_WM),y)
MP_CFLAGS += -DANDROID_USES_TRELLIS_WM=1
MP_CFLAGS += -DTRELLIS_HAS_NEXUS -DB_HAS_SSL -DB_HAS_HLS_PROTOCOL_SUPPORT -DB_HAS_HTTP_AES_SUPPORT -DTRELLIS_NSC_WINDOWMANAGER
MP_CFLAGS += -DTRELLIS_MULTI_INPUTDRIVER
MP_CFLAGS += -DTRELLIS_HAS_BME -DTRELLIS_HAS_BPM -DTRELLIS_HAS_3D_GFX -DTRELLIS_USES_BOOST_TR1 -DTRELLIS_ANDROID_BUILD
else
MP_CFLAGS += -DANDROID_USES_TRELLIS_WM=0
endif

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_SHARED_LIBRARIES := $(NEXUS_LIB) liblog libcutils libbinder libutils libnexusipcclient libdl

LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include \
                    $(REFSW_PATH)/../libnexusservice
LOCAL_C_INCLUDES += $(REFSW_PATH)/../libnexusipc
LOCAL_C_INCLUDES += $(REFSW_PATH)/../../drivers/nx_ashmem

REMOVE_NEXUS_CFLAGS := -Wstrict-prototypes -march=armv7-a
MANGLED_NEXUS_CFLAGS := $(filter-out $(REMOVE_NEXUS_CFLAGS), $(NEXUS_CFLAGS))

LOCAL_CFLAGS:= -DLOG_TAG=\"gralloc\" $(MANGLED_NEXUS_CFLAGS) -DANDROID $(MP_CFLAGS)
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI

ifeq ($(ANDROID_USES_TRELLIS_WM),y)
LOCAL_SHARED_LIBRARIES += libstlport

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

LOCAL_SRC_FILES := \
        gralloc.cpp \
        framebuffer.cpp \
        mapper.cpp \
        NexusSurface.cpp

LOCAL_MODULE := gralloc.bcm_platform
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
