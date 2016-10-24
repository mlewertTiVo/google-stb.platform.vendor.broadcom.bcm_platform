LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)

LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/tv
LOCAL_SRC_FILES := \
	dvb/src/utils/src/getbits.c \
	dvb/src/utils/src/genericlist.c \
	dvb/src/utils/src/dvb_crc32.c \
	dvb/src/utils/src/dvb_qsort.c \
	dvb/src/si/src/dvb_si_priv.c \
	dvb/src/si/src/dvb_bat.c \
	dvb/src/si/src/dvb_descriptor.c \
	dvb/src/si/src/dvb_eit.c \
	dvb/src/si/src/dvb_nit.c \
	dvb/src/si/src/dvb_rst.c \
	dvb/src/si/src/dvb_sdt.c \
	dvb/src/si/src/dvb_smi.c \
	dvb/src/si/src/dvb_tdt.c \
	dvb/src/si/src/ca_parser.c \
	dvb/src/si/src/dvb_subtitle.c \
	dvb/src/bdvb_mgr.c \
	dvb/src/ch_map.c \
	dvb/src/filter_mgr.c \
	dvb/src/bdvb_osal.c \
	dvb/src/bdvb_osal_linux.c \
	dvb/si/si_dbg.c \
	dvb/si/si_util.c \
	dvb/ts/ts_priv.c \
	dvb/ts/ts_psi.c \
	dvb/ts/ts_pat.c \
	dvb/ts/ts_pmt.c

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/tv/dvb/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/tv/dvb/src/si/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/tv/dvb/src/utils/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/tv/dvb/si

LOCAL_CFLAGS += -DLINUX
LOCAL_CFLAGS += -DTRELLIS_BUILD
LOCAL_CFLAGS += -DCONFIG_DVB

LOCAL_SHARED_LIBRARIES :=         \
        libcutils                 \
        libutils                  \
        libnexus

LOCAL_MODULE := libbdvb
include $(BUILD_STATIC_LIBRARY)
