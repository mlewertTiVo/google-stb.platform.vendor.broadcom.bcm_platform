LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    spdif_scms_ctrl.c

LOCAL_C_INCLUDES := \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libsecurity/bdbg2alog \
    bionic

LOCAL_SHARED_LIBRARIES += \
    liblog \
    libutils \
    libnexus \
    libnxclient

LOCAL_MODULE := spdif_scms_ctrl

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += -DBDBG2ALOG_ENABLE_LOGS=1
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/include

LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
