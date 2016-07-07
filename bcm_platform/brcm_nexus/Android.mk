LOCAL_PATH := $(call my-dir)
BSEAV_TOP  ?= ../../../../../../../../../BSEAV

include $(CLEAR_VARS)
LOCAL_MODULE := libnexus
ifneq (,$(wildcard $(TOP)/${BCM_VENDOR_STB_ROOT}/release_prebuilts/$(LOCAL_MODULE).so))
LOCAL_SRC_FILES := ../../release_prebuilts/$(LOCAL_MODULE).so
else
LOCAL_SRC_FILES := bin/libnexus.so
endif
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_STRIP_MODULE := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libnxserver
LOCAL_SRC_FILES := bin/libnxserver.a
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .a
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_STRIP_MODULE := false
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libnexus_static
ifneq (,$(wildcard $(TOP)/${BCM_VENDOR_STB_ROOT}/release_prebuilts/$(LOCAL_MODULE).a))
LOCAL_SRC_FILES := ../../release_prebuilts/$(LOCAL_MODULE).a
else
LOCAL_SRC_FILES := bin/libnexus_static.a
endif
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .a
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_STRIP_MODULE := false
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
