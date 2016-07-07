LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)


LOCAL_SRC_FILES := \
	libwebsockets.c \
	handshake.c \
	parsers.c \
	base64-decode.c \
	client-handshake.c \
	sha-1.c \
	md5.c \
	ifaddrs.c \
	extension.c \
	extension-deflate-stream.c \
	extension-x-google-mux.c
	

LOCAL_SHARED_LIBRARIES := libc libcutils libnetutils libz
LOCAL_C_INCLUDES := external/zlib
LOCAL_MODULE := libwebsocket
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -DANDROID
LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
