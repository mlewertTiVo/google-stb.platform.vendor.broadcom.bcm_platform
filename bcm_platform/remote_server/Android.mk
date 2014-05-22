LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	RemoteServer.cpp \
	AndroidRemoteServer.cpp

LOCAL_SHARED_LIBRARIES := libc libcutils libbinder libwebsocket libutils

LOCAL_MODULE := remote_server
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -DANDROID
LOCAL_C_INCLUDES += external/libwebsocket/lib

LOCAL_PRELINK_MODULE := false

include $(BUILD_EXECUTABLE)


