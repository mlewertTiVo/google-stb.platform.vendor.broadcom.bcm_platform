LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES) \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/tools/bmemperf/include
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)

BOA_SRC_ROOT := ../../../../refsw/BSEAV/lib/boa/
LOCAL_SRC_FILES := \
   $(BOA_SRC_ROOT)/index.c

LOCAL_MODULE := index
LOCAL_MODULE_SUFFIX := .cgi
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/nexus/bmem

include $(BUILD_EXECUTABLE)

