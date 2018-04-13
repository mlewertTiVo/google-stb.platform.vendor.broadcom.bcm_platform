LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_C_INCLUDES += ${LOCAL_PATH}/include
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/libs/arect/include
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/libs/nativewindow/include
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/libs/nativebase/include
ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/dspsvcevt/1.0
else
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/utils
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/hwcomposer/common/blib
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxbinder
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxevtsrc
endif
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_MODULE := libbcmsideband
LOCAL_SHARED_LIBRARIES := libutils libcutils libhwcbinder liblog
ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_SRC_FILES := treble/bcmsideband.cpp
LOCAL_CFLAGS += -DBCM_FULL_TREBLE
LOCAL_SHARED_LIBRARIES += bcm.hardware.dspsvcext@1.0 libhidlbase libhidltransport
else
LOCAL_SRC_FILES := legacy/bcmsideband.cpp
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_SHARED_LIBRARIES += libnxwrap libnxbinder libnxevtsrc liblog libbinder
endif

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

include $(BUILD_SHARED_LIBRARY)
