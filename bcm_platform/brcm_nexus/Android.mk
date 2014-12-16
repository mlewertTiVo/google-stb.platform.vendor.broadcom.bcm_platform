LOCAL_PATH := $(call my-dir)
BSEAV_TOP  ?= ../../../../../../../../../BSEAV

include $(CLEAR_VARS)
LOCAL_MODULE := libnexus
LOCAL_SRC_FILES := bin/libnexus.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_STRIP_MODULE := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libb_os
LOCAL_SRC_FILES := bin/libb_os.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_STRIP_MODULE := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libnxclient
LOCAL_SRC_FILES := bin/libnxclient.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_STRIP_MODULE := true
include $(BUILD_PREBUILT)

ifeq ($(NEXUS_MODE),client)
include $(CLEAR_VARS)
ifeq ($(NEXUS_WEBCPU),core1_server)
LOCAL_MODULE := libnexus_webcpu
LOCAL_SRC_FILES := bin/libnexus_webcpu.so
else
LOCAL_MODULE := libnexus_client
LOCAL_SRC_FILES := bin/libnexus_client.so
endif
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_STRIP_MODULE := true
include $(BUILD_PREBUILT)
endif
