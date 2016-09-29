LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnxclient
LOCAL_SHARED_LIBRARIES += libb_os
LOCAL_SHARED_LIBRARIES += libb_bip_base
LOCAL_SHARED_LIBRARIES += libb_bip_client
LOCAL_SHARED_LIBRARIES += libb_bip_server
LOCAL_SHARED_LIBRARIES += libb_dtcp_ip
LOCAL_SHARED_LIBRARIES += libb_psip
LOCAL_SHARED_LIBRARIES += libbcrypt
LOCAL_SHARED_LIBRARIES += libcmndrm
LOCAL_SHARED_LIBRARIES += libb_playback_ip

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)

include $(NEXUS_TOP)/lib/bip/bip_lib.inc
LOCAL_CFLAGS += ${BIP_ALL_LIB_CFLAGS}

LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/
LOCAL_SRC_FILES := refsw/nexus/lib/bip/examples/http_media_server/http_file_streamer.c

LOCAL_MODULE := http_file_streamer
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

