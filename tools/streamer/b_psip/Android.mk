LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(NEXUS_TOP)/lib/os/include \
                    $(NEXUS_TOP)/lib/os/include/linuxuser \
                    $(TOP)/external/boringssl/include
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)

B_REFSW_OS ?= linuxuser
B_PSIP_TOP := $(NEXUS_TOP)/../rockford/lib/psip
B_MPEG2_TS_PARSE := ${NEXUS_TOP}/../BSEAV/lib/mpeg2_ts_parse
B_PSIP_LIB_SOURCES :=
include $(ROCKFORD_TOP)/lib/psip/b_psip_lib.inc
LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw
INTERMEDIATE_FILES := $(subst $(B_PSIP_TOP),rockford/lib/psip,$(B_PSIP_LIB_SOURCES))
INTERMEDIATE_FILES2 := $(subst $(B_MPEG2_TS_PARSE),BSEAV/lib/mpeg2_ts_parse,$(INTERMEDIATE_FILES))
LOCAL_SRC_FILES := $(INTERMEDIATE_FILES2)

LOCAL_CFLAGS += -DLINUX
LOCAL_C_INCLUDES += $(subst $(B_PSIP_TOP),$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/rockford/lib/psip,$(B_PSIP_LIB_PUBLIC_INCLUDES) $(B_PSIP_LIB_PRIVATE_INCLUDES))

LOCAL_SHARED_LIBRARIES :=         \
        libb_os                   \
        libcutils                 \
        libutils

LOCAL_MODULE := libb_psip
include $(BUILD_SHARED_LIBRARY)
