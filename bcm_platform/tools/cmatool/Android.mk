LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_PATH := $(TOP)/kernel/private/97xxx-bcm/rootfs

LOCAL_SRC_FILES := user/cmatool/cmatool.c

LOCAL_C_INCLUDES :=  $(LOCAL_PATH)/user/cmatool \
                     $(TOP)/kernel/linux/include/uapi

LOCAL_CFLAGS += -Wall -Wextra -D_GNU_SOURCE

LOCAL_MODULE := cmatool
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_OPTIONAL_EXECUTABLES)

include $(BUILD_EXECUTABLE)

