LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnxclient

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES) \
                    $(TOP)/vendor/broadcom/refsw/BSEAV/tools/bmemperf/include \
                    $(TOP)/vendor/broadcom/refsw/BSEAV/tools/bmemconfig

LOCAL_CFLAGS += $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS))
LOCAL_CFLAGS += $(addprefix -D,$(NEXUS_APP_DEFINES))
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += -DBSTD_CPU_ENDIAN=BSTD_ENDIAN_LITTLE -DUSE_BOXMODES

BMEM_CONFIG_SRC_ROOT := ../../../../refsw/BSEAV/tools/bmemconfig
LOCAL_SRC_FILES := \
   $(BMEM_CONFIG_SRC_ROOT)/bmemconfig.c \
   $(BMEM_CONFIG_SRC_ROOT)/${NEXUS_PLATFORM}/boxmodes.c \
   $(BMEM_CONFIG_SRC_ROOT)/memusage.c \
   bmemperf_utils_mod.c

LOCAL_MODULE := bmemconfig
LOCAL_MODULE_SUFFIX := .cgi
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/nexus/bmem

include $(BUILD_EXECUTABLE)

