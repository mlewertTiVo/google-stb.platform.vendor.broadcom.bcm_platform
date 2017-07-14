LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE:= libbcmsidebandviewer_jni
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libbcmsideband/include \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libbcmsidebandplayer/include/
LOCAL_SRC_FILES:= com_broadcom_sideband_BcmSidebandViewer.cpp
LOCAL_SHARED_LIBRARIES := \
    libnativehelper \
    libutils \
    liblog \
    libandroid \
    libcutils \
    libbcmsideband \
    libbcmsidebandplayer

LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)
