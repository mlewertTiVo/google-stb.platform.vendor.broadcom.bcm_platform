LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)

B_LIB_TOP := $(NEXUS_TOP)/lib
B_REFSW_OS ?= linuxuser
include $(NEXUS_TOP)/lib/playback_ip/b_playback_ip_lib.inc
LOCAL_C_INCLUDES += $(subst $(B_LIB_TOP),$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/lib,$(B_PLAYBACK_IP_LIB_PUBLIC_INCLUDES) $(B_PLAYBACK_IP_LIB_PRIVATE_INCLUDES))
LOCAL_CFLAGS += $(addprefix -D,$(B_PLAYBACK_IP_LIB_DEFINES))
include $(NEXUS_TOP)/lib/bip/bip_lib.inc
LOCAL_C_INCLUDES += $(subst $(B_LIB_TOP),$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/lib,$(BIP_BASE_LIB_PUBLIC_INCLUDES) $(BIP_BASE_LIB_PRIVATE_INCLUDES))
LOCAL_CFLAGS += $(addprefix -D,$(BIP_BASE_LIB_DEFINES))
LOCAL_CFLAGS += -Drindex=strrchr

LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/player/
LOCAL_SRC_FILES := \
	source/Media.cpp \
	source/MediaPlayer.cpp \
	source/SimpleDecoderNxclient.cpp \
	source/BaseSource.cpp \
	source/NetworkSource.cpp \
	source/PushTSSource.cpp \
	source/PushESSource.cpp \
	source/FileSource.cpp \
	source/HdmiSource.cpp \
	source/LiveSource.cpp \
	source/TimeshiftSource.cpp \
	source/MediaProbe.cpp \
	source/MediaStream.cpp \
	source/AudioPlayback.cpp

LOCAL_CFLAGS += -DTRELLIS_BUILD
LOCAL_CFLAGS += -DLINUX
LOCAL_CFLAGS += -DTRELLIS_HAS_TV

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/player/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/player/source/mediasource
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/player/source/mediasource/chromium
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/shared/jsonrpccpp
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/shared
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/common_drm/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/playready/2.5/inc
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/common_crypto/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/srai/include
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_SHARED_LIBRARIES := libcutils
LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnxclient
LOCAL_SHARED_LIBRARIES += libjsonrpccpp-client
LOCAL_SHARED_LIBRARIES += libjsonrpccpp-server
LOCAL_SHARED_LIBRARIES += libuv
LOCAL_SHARED_LIBRARIES += libTrellisCore
LOCAL_SHARED_LIBRARIES += libb_os
LOCAL_SHARED_LIBRARIES += libb_bip_base
LOCAL_SHARED_LIBRARIES += libb_bip_client
LOCAL_SHARED_LIBRARIES += libb_bip_server
LOCAL_SHARED_LIBRARIES += libb_dtcp_ip
LOCAL_SHARED_LIBRARIES += libb_psip
LOCAL_SHARED_LIBRARIES += libbcrypt
LOCAL_SHARED_LIBRARIES += libcmndrm
LOCAL_SHARED_LIBRARIES += libb_playback_ip

LOCAL_WHOLE_STATIC_LIBRARIES := libbdvb
LOCAL_WHOLE_STATIC_LIBRARIES += libjsoncpp

LOCAL_MODULE := libTrellisMedia
include $(BUILD_SHARED_LIBRARY)
