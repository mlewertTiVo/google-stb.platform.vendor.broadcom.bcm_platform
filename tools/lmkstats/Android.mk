LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := main.c
LOCAL_CFLAGS += -Wall -Wextra -D_GNU_SOURCE

LOCAL_MODULE := lmkstats
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_EXECUTABLE)

