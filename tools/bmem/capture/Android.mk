LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnxclient

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES) \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/tools/bmemperf/include

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += -DUSE_BOXMODES

BMEM_PERF_SRC_ROOT := ../../../../refsw/BSEAV/tools/bmemperf
LOCAL_SRC_FILES := \
   $(BMEM_PERF_SRC_ROOT)/capture/bmemperf_capture.c \
   $(BMEM_PERF_SRC_ROOT)/common/bmemperf_utils.c \
   $(BMEM_PERF_SRC_ROOT)/common/bmemperf_lib.c \
   bmemperf_info.auto.c

BCHP_VER_LOWER := $(shell echo ${BCHP_VER} | tr [:upper:] [:lower:])
BOXMODE_FILES := $(shell ls -1v $(NEXUS_TOP)/../magnum/commonutils/box/src/$(BCHP_CHIP)/$(BCHP_VER_LOWER)/bbox_memc_box*_config.c)

define generate-capture-info-module
	awk -f $(NEXUS_TOP)/../BSEAV/tools/bmemperf/include/bmemperf_info_pre.awk $(NEXUS_TOP)/../BSEAV/tools/bmemperf/capture/Makefile > $(NEXUS_TOP)/../../bcm_platform/tools/bmem/capture/bmemperf_info.auto.c
	$(foreach myfile,$(BOXMODE_FILES), \
		awk -f $(NEXUS_TOP)/../BSEAV/tools/bmemperf/include/bmemperf_info.awk $(myfile) >> $(NEXUS_TOP)/../../bcm_platform/tools/bmem/capture/bmemperf_info.auto.c;) >/dev/null
	awk -f $(NEXUS_TOP)/../BSEAV/tools/bmemperf/include/bmemperf_info_post.awk $(NEXUS_TOP)/../../bcm_platform/tools/bmem/capture/bmemperf_info.auto.c > $(NEXUS_TOP)/../../bcm_platform/tools/bmem/capture/bmemperf_info.auto.tmp
	cat $(NEXUS_TOP)/../../bcm_platform/tools/bmem/capture/bmemperf_info.auto.tmp >> $(NEXUS_TOP)/../../bcm_platform/tools/bmem/capture/bmemperf_info.auto.c
	rm $(NEXUS_TOP)/../../bcm_platform/tools/bmem/capture/bmemperf_info.auto.tmp
endef

$(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/bmem/capture/bmemperf_info.auto.c:
	$(hide) $(call generate-capture-info-module)

LOCAL_MODULE := bmemperf_capture
LOCAL_MODULE_SUFFIX := .cgi
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/nexus/bmem

LOCAL_POST_INSTALL_CMD := \
	rm $(NEXUS_TOP)/../../bcm_platform/tools/bmem/capture/bmemperf_info.auto.c

include $(BUILD_EXECUTABLE)

