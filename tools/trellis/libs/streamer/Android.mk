include $(CLEAR_VARS)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)

LOCAL_CFLAGS += -fexceptions

B_LIB_TOP := $(NEXUS_TOP)/lib
B_REFSW_OS ?= linuxuser
include $(NEXUS_TOP)/lib/playback_ip/b_playback_ip_lib.inc
LOCAL_C_INCLUDES += $(subst $(B_LIB_TOP),$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/lib,$(B_PLAYBACK_IP_LIB_PUBLIC_INCLUDES) $(B_PLAYBACK_IP_LIB_PRIVATE_INCLUDES))
LOCAL_CFLAGS += $(addprefix -D,$(B_PLAYBACK_IP_LIB_DEFINES))
LOCAL_CFLAGS += -DTRELLIS_HAS_TV

FILTER_OUT_LOCAL_CFLAGS := -DB_HAS_DTCP_IP
FILTER_OUT_LOCAL_CFLAGS += -DDTCP_IP_SUPPORT
FILTER_OUT_LOCAL_CFLAGS += -DB_DTCP_IP_HW_DECRYPTION
FILTER_OUT_LOCAL_CFLAGS += -DB_DTCP_IP_HW_ENCRYPTION
FILTER_OUT_LOCAL_CFLAGS += -DB_DTCP_IP_DATA_BRCM
FILTER_OUT_LOCAL_CFLAGS += -DB_DTCP_IP_COMMON_DRM_CONTENT_SUPPORT
LOCAL_CFLAGS := $(filter-out $(FILTER_OUT_LOCAL_CFLAGS), $(LOCAL_CFLAGS))

LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/server
LOCAL_SRC_FILES := \
	source/BaseStreamer.cpp \
	source/MediaStreamer.cpp \
	source/TVStreamer.cpp

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/server/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/shared
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/player/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/tshdrbuilder

LOCAL_SHARED_LIBRARIES :=         \
        libcutils                 \
        libutils                  \
        libnexus                  \
        libjsonrpccpp-client      \
        libjsonrpccpp-server      \
        libuv                     \
        libTrellisCore            \
        libTrellisMedia

LOCAL_WHOLE_STATIC_LIBRARIES := libbdvb
LOCAL_WHOLE_STATIC_LIBRARIES += libjsoncpp

LOCAL_MODULE := libTrellisStreamer
include $(BUILD_STATIC_LIBRARY)
