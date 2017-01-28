LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/shared/
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/server/include/
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/player/include/
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/tv/include/
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)

include $(NEXUS_TOP)/lib/bip/bip_lib.inc
LOCAL_CFLAGS += ${BIP_ALL_LIB_CFLAGS}

LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/
LOCAL_SRC_FILES := refsw/trellis/media/samples/tvRecord.cpp

LOCAL_SHARED_LIBRARIES :=         \
        libcutils                 \
        libutils                  \
        libnexus                  \
        libnxclient               \
        libjsonrpccpp-client      \
        libjsonrpccpp-server      \
        libuv                     \
        libTrellisCore            \
        libTrellisTV              \
        libTrellisMediaTranscoder \
        libb_playback_ip          \
        libb_os                   \
        libTrellisMedia

LOCAL_WHOLE_STATIC_LIBRARIES := libTrellisMediaServer
LOCAL_WHOLE_STATIC_LIBRARIES += libTrellisStreamer

LOCAL_MODULE := trellisTvRecord
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

