LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc
include $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/magnum/basemodules/dtu/bdtu.inc

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)

LOCAL_SHARED_LIBRARIES := libnexus

PREFIX_RELATIVE_PATH := ../../../
LOCAL_SRC_FILES := $(PREFIX_RELATIVE_PATH)refsw/rockford/applications/dtu/dtutool.c
LOCAL_SRC_FILES += $(PREFIX_RELATIVE_PATH)refsw/magnum/basemodules/dtu/src/bdtu.c
LOCAL_SRC_FILES += $(PREFIX_RELATIVE_PATH)refsw/magnum/basemodules/reg/breg_mem.c

LOCAL_MODULE := dtutool
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_EXECUTABLE)

