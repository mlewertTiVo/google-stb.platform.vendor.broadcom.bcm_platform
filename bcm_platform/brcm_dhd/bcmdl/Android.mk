
ifneq ($(BROADCOM_ANDROID_VERSION),l)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SHARED_LIBRARIES := liblog libcutils

LOCAL_C_INCLUDES += external/zlib $(LOCAL_PATH)/include external/libusb-compat/libusb

LOCAL_SRC_FILES := 	\
	bcmdl.c usb_linux.c bcmutils.c
	 
LOCAL_SHARED_LIBRARIES := libusb2 libusb-compat libz

LOCAL_MODULE := bcmdl
LOCAL_MODULE_TAGS := optional debug

LOCAL_CFLAGS := -DBCMTRXV2

LOCAL_PRELINK_MODULE := false
include $(BUILD_EXECUTABLE)
endif

