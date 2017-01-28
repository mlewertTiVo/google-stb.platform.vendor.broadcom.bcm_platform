include $(CLEAR_VARS)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)

B_LIB_TOP := $(NEXUS_TOP)/lib
B_REFSW_OS ?= linuxuser
include $(NEXUS_TOP)/lib/playback_ip/b_playback_ip_lib.inc
LOCAL_C_INCLUDES += $(subst $(B_LIB_TOP),$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/lib,$(B_PLAYBACK_IP_LIB_PUBLIC_INCLUDES) $(B_PLAYBACK_IP_LIB_PRIVATE_INCLUDES))
LOCAL_CFLAGS += $(addprefix -D,$(B_PLAYBACK_IP_LIB_DEFINES))

LOCAL_CFLAGS += -fexceptions

LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/server
LOCAL_SRC_FILES := \
	source/MediaServer.cpp

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/server/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/tv/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/shared
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/player/include
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_SHARED_LIBRARIES :=         \
        libcutils                 \
        libutils                  \
        libnexus

LOCAL_MODULE := libTrellisMediaServer
include $(BUILD_STATIC_LIBRARY)
