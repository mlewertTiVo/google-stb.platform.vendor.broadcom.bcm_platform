LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_C_INCLUDES += ${LOCAL_PATH}/include
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/libs/arect/include
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/libs/nativewindow/include
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/libs/nativebase/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/dspsvcevt/1.0
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_HEADER_LIBRARIES := liblog_headers
# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_MODULE := libbcmsideband
LOCAL_SHARED_LIBRARIES := libutils libcutils liblog
LOCAL_SRC_FILES := bcmsideband.cpp
LOCAL_SHARED_LIBRARIES += bcm.hardware.dspsvcext@1.0 libhidlbase libhidltransport

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

include $(BUILD_SHARED_LIBRARY)
