LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnxclient

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES) \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/apps/utils

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)

LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/
LOCAL_SRC_FILES := refsw/nexus/nxclient/apps/config.c \
                   refsw/nexus/nxclient/apps/utils/nxapps_cmdline.c

LOCAL_MODULE := nxcfg
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_OPTIONAL_EXECUTABLES)

include $(BUILD_EXECUTABLE)

