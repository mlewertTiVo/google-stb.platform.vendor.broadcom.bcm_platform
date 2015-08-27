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
include $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/include/nxclient.inc

LOCAL_MODULE := power.bcm_platform

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := libbinder \
                          libcutils \
                          libdl \
                          liblog \
                          libnexusipcclient \
                          libnexusir \
                          libnxclient \
                          libpmlibservice \
                          libpower \
                          libutils \
                          $(NEXUS_LIB)

ifneq ($(findstring NEXUS_HAS_GPIO, $(NEXUS_APP_DEFINES)),)
LOCAL_C_INCLUDES += $(NEXUS_GPIO_PUBLIC_INCLUDES)
endif

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/libnexusservice \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/libnexusipc \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/libnexusir \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/pmlibservice \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/drivers/droid_pm \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/linux/driver
                    
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)

LOCAL_CFLAGS:= $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID $(MP_CFLAGS)

LOCAL_SRC_FILES := power.cpp \
                   nexus_power.cpp

ifeq ($(findstring NEXUS_HAS_GPIO, $(NEXUS_APP_DEFINES)),)
LOCAL_SRC_FILES += nexus_gpio_stubs.cpp
endif

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
