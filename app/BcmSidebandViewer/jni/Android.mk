LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE:= libbcmsidebandviewer_jni
LOCAL_SRC_FILES:= com_broadcom_sideband_BcmSidebandViewer.cpp
ifeq ($(NEXUS_HDMI_INPUT_SUPPORT),y)
LOCAL_CFLAGS += -DNEXUS_HAS_HDMI_INPUT=1 # Since we don't include nexus flags...
endif
LOCAL_SHARED_LIBRARIES := \
    libnativehelper \
    libutils \
    liblog \
    libandroid \
    libcutils \
    libbcmsideband \
    libbcmsidebandplayer
include $(BUILD_SHARED_LIBRARY)
