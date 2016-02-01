LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnxclient

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES) \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/apps/utils \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/utils

LOCAL_CFLAGS += $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS))
LOCAL_CFLAGS += $(addprefix -D,$(NEXUS_APP_DEFINES))
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += -DBSTD_CPU_ENDIAN=BSTD_ENDIAN_LITTLE

LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/
LOCAL_SRC_FILES := bcm_platform/nxdispfmt/nxdispfmt.c \
                   refsw/nexus/utils/namevalue.c

LOCAL_MODULE := nxdispfmt
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

