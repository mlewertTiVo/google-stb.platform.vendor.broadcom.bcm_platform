LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnxclient

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES) \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/tools/bmemperf/include \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/tools/bmemconfig
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += -DUSE_BOXMODES
LOCAL_CFLAGS += -DBMEMCONFIG_READ32_SUPPORTED

BMEM_CONFIG_SRC_ROOT := ../../../../refsw/BSEAV/tools/bmemconfig
BMEM_PERF_SRC_ROOT := ../../../../refsw/BSEAV/tools/bmemperf
LOCAL_SRC_FILES := \
   $(BMEM_CONFIG_SRC_ROOT)/bmemconfig.c \
   $(BMEM_CONFIG_SRC_ROOT)/${NEXUS_PLATFORM}/boxmodes.c \
   $(BMEM_CONFIG_SRC_ROOT)/memusage.c \
   $(BMEM_PERF_SRC_ROOT)/common/bmemperf_utils.c \
   $(BMEM_PERF_SRC_ROOT)/common/bmemperf_lib.c \
   bmemconfig_box_info.auto.c


BCHP_VER_LOWER := $(shell echo ${BCHP_VER} | tr [:upper:] [:lower:])
BOXMODE_FILES := $(shell ls -1v $(NEXUS_TOP)/../magnum/commonutils/box/src/$(BCHP_CHIP)/$(BCHP_VER_LOWER)/bbox_memc_box*_config.c)

define generate-config-box-info-module
	awk -f $(NEXUS_TOP)/../BSEAV/tools/bmemconfig/bmemconfig_box_info_pre.awk $(NEXUS_TOP)/../BSEAV/tools/bmemconfig/Makefile > $(NEXUS_TOP)/../../bcm_platform/tools/bmem/config/bmemconfig_box_info.auto.c
	echo ",\"unknown DDR SCB\"}" >> $(NEXUS_TOP)/../../bcm_platform/tools/bmem/config/bmemconfig_box_info.auto.c
	$(foreach myfile,$(BOXMODE_FILES), \
		awk -f $(NEXUS_TOP)/../BSEAV/tools/bmemconfig/bmemconfig_box_info.awk $(myfile) >> $(NEXUS_TOP)/../../bcm_platform/tools/bmem/config/bmemconfig_box_info.auto.c;) >/dev/null
	awk -f $(NEXUS_TOP)/../BSEAV/tools/bmemconfig/bmemconfig_box_info_post.awk $(NEXUS_TOP)/../../bcm_platform/tools/bmem/config/bmemconfig_box_info.auto.c > $(NEXUS_TOP)/../../bcm_platform/tools/bmem/config/bmemconfig_box_info.auto.tmp
	cat $(NEXUS_TOP)/../../bcm_platform/tools/bmem/config/bmemconfig_box_info.auto.tmp >> $(NEXUS_TOP)/../../bcm_platform/tools/bmem/config/bmemconfig_box_info.auto.c
	rm $(NEXUS_TOP)/../../bcm_platform/tools/bmem/config/bmemconfig_box_info.auto.tmp
endef

$(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/bmem/config/bmemconfig_box_info.auto.c:
	$(hide) $(call generate-config-box-info-module)


LOCAL_MODULE := bmemconfig
LOCAL_MODULE_SUFFIX := .cgi
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/nexus/bmem

include $(BUILD_EXECUTABLE)

