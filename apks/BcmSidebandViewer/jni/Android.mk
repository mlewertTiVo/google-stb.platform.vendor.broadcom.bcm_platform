LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE:= libbcmsidebandviewer_jni
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libbcmsideband/include
LOCAL_SRC_FILES:= com_broadcom_sideband_BcmSidebandViewer.cpp
LOCAL_SHARED_LIBRARIES := \
    libnativehelper \
    libutils \
    liblog \
    libandroid \
    libcutils \
    libbcmsideband \
    libbcmsidebandplayer
include $(BUILD_SHARED_LIBRARY)
