LOCAL_PATH := $(call my-dir)
BSEAV_TOP  ?= ../../../../../../../../../BSEAV

include $(CLEAR_VARS)
LOCAL_MODULE := libnexus
LOCAL_SRC_FILES := bin/libnexus.so
LOCAL_MODULE_TAGS := optional debug
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
LOCAL_MODULE := libb_playback_ip
LOCAL_SRC_FILES := bin/libb_playback_ip.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_STRIP_MODULE := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libb_psip
LOCAL_SRC_FILES := bin/libb_psip.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_STRIP_MODULE := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := host.cert
LOCAL_SRC_FILES := bin/$(LOCAL_MODULE)
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_CLASS := ETC
include $(BUILD_PREBUILT)

ifeq ($(ANDROID_SUPPORTS_NXCLIENT),y)
include $(CLEAR_VARS)
LOCAL_MODULE := libnxclient
LOCAL_SRC_FILES := bin/libnxclient.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_STRIP_MODULE := true
include $(BUILD_PREBUILT)
endif

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

#
# If the original "BSEAV/lib/livemedia/live" source directory exists, then we are using older
# reference software that does not support building liveMedia components as shared libaries.
#
LIVEMEDIA_ORIGINAL_SRC_DIR_EXISTS := $(shell test -d ${BSEAV_TOP}/lib/livemedia/live && echo "y")

ifneq ($(LIVEMEDIA_ORIGINAL_SRC_DIR_EXISTS),y)
ifeq ($(LIVEMEDIA_SUPPORT),y)

include $(CLEAR_VARS)
LOCAL_MODULE := libBasicUsageEnvironment
LOCAL_SRC_FILES := bin/libBasicUsageEnvironment.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_STRIP_MODULE := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libgroupsock
LOCAL_SRC_FILES := bin/libgroupsock.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_STRIP_MODULE := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libliveMedia
LOCAL_SRC_FILES := bin/libliveMedia.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_STRIP_MODULE := true
include $(BUILD_PREBUILT)

endif
endif
