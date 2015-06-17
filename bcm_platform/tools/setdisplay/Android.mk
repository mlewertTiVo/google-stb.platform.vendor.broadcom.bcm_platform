LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnxclient

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES) \
                    $(TOP)/vendor/broadcom/stb/refsw/nexus/nxclient/apps/utils \
                    $(TOP)/vendor/broadcom/stb/refsw/nexus/utils

LOCAL_CFLAGS += $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS))
LOCAL_CFLAGS += $(addprefix -D,$(NEXUS_APP_DEFINES))
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += -DBSTD_CPU_ENDIAN=BSTD_ENDIAN_LITTLE

LOCAL_SRC_FILES := setdisplay.c \
                   ../../../refsw/nexus/nxclient/apps/utils/nxapps_cmdline.c \
                   ../../../refsw/nexus/utils/namevalue.c

LOCAL_MODULE := nxsetdisp
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_OPTIONAL_EXECUTABLES)

include $(BUILD_EXECUTABLE)

