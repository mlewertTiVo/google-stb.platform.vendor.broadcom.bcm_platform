LOCAL_PATH := $(call my-dir)
include $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/include/nxclient.inc
include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libbinder
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/include

LOCAL_SRC_FILES := INxHpdEvtSrc.cpp \
                   INxDspEvtSrc.cpp

# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libnxevtsrc
include $(BUILD_SHARED_LIBRARY)

