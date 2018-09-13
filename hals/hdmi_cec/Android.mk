# Copyright (C) 2015 The Android Open Source Project
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
LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    libdl \
    libutils \
    libnexusir \
    libnxcec \
    libstagefright_foundation \
    libnexus \
    libnxwrap

ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_SHARED_LIBRARIES += \
    bcm.hardware.nexus@1.0
else
LOCAL_SHARED_LIBRARIES += \
    libbinder \
    libnxbinder \
    libnxevtsrc
endif

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxcec \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/pmlibservice
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/nexus/1.0/default
else
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxbinder \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxevtsrc
endif
LOCAL_C_INCLUDES += $(TOP)/hardware/libhardware/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/prop
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror
ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_CFLAGS += -DBCM_FULL_TREBLE
LOCAL_SRC_FILES := treble/nexus_hdmi_cec.cpp
else
LOCAL_SRC_FILES := legacy/nexus_hdmi_cec.cpp
endif

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libhdmicec
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    libdl \
    libutils \
    libnexusir \
    libnxcec \
    libstagefright_foundation \
    libnexus \
    libhdmicec \
    libnxwrap

ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_SHARED_LIBRARIES += \
    bcm.hardware.nexus@1.0
else
LOCAL_SHARED_LIBRARIES += \
    libbinder \
    libnxbinder \
    libnxevtsrc
endif

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxcec \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/pmlibservice
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/nexus/1.0/default
else
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxbinder \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxevtsrc
endif
LOCAL_C_INCLUDES += $(TOP)/hardware/libhardware/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/prop
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror
ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_CFLAGS += -DBCM_FULL_TREBLE
endif
LOCAL_SRC_FILES := hdmi_ext.cpp

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libhdmiext
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    libdl \
    libutils \
    libnexusir \
    libnxcec \
    libstagefright_foundation \
    libnexus \
    libhdmicec \
    libhdmiext \
    libnxwrap

ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_SHARED_LIBRARIES += \
    bcm.hardware.nexus@1.0
else
LOCAL_SHARED_LIBRARIES += \
    libbinder \
    libnxbinder \
    libnxevtsrc
endif

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxcec \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/pmlibservice
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/nexus/1.0/default
else
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxbinder \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxevtsrc
endif
LOCAL_C_INCLUDES += $(TOP)/hardware/libhardware/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/prop
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror
ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_CFLAGS += -DBCM_FULL_TREBLE
endif

LOCAL_SRC_FILES := hdmi_cec.cpp
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := hdmi_cec.$(TARGET_BOARD_PLATFORM)
include $(BUILD_SHARED_LIBRARY)
