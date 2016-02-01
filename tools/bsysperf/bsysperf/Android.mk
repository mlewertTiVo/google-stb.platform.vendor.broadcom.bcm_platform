LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnxclient

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES) \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/tools/bsysperf/include \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/tools/bmemperf/include

LOCAL_CFLAGS += $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS))
LOCAL_CFLAGS += $(addprefix -D,$(NEXUS_APP_DEFINES))
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += -DBSTD_CPU_ENDIAN=BSTD_ENDIAN_LITTLE
LOCAL_CFLAGS += -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DB_ANDROID_BUILD

BMEM_PERF_SRC_ROOT := ../../../../refsw/BSEAV/tools/bmemperf
BSYS_PERF_SRC_ROOT := ../../../../refsw/BSEAV/tools/bsysperf
LOCAL_SRC_FILES := \
   $(BSYS_PERF_SRC_ROOT)/bsysperf.c \
   $(BSYS_PERF_SRC_ROOT)/bheaps.c \
   $(BSYS_PERF_SRC_ROOT)/bpower.c \
   $(BMEM_PERF_SRC_ROOT)/common/bmemperf_utils.c


LOCAL_MODULE := bsysperf
LOCAL_MODULE_SUFFIX := .cgi
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/nexus/bmem

include $(BUILD_EXECUTABLE)

