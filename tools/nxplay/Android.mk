LOCAL_PATH := $(NEXUS_TOP)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc
LOCAL_PRELINK_MODULE := false

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(NEXUS_TOP)/nxclient/apps/utils
LOCAL_C_INCLUDES += $(NEXUS_TOP)/utils
LOCAL_C_INCLUDES += $(BSEAV_TOP)/lib/tshdrbuilder
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += $(NEXUS_CFLAGS)
LOCAL_CFLAGS += $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS))
LOCAL_CFLAGS += $(addprefix -D,$(NEXUS_APP_DEFINES))

LOCAL_SHARED_LIBRARIES := libnexus libnxclient
LOCAL_SRC_FILES := \
    nxclient/apps/play.c \
    nxclient/apps/utils/media_player.c \
    nxclient/apps/utils/media_player_bip.c \
    nxclient/apps/utils/media_player_ip.c \
    nxclient/apps/utils/media_probe.c \
    nxclient/apps/utils/bfile_crypto.c \
    nxclient/apps/utils/dvr_crypto.c \
    nxclient/apps/utils/nxapps_cmdline.c \
    nxclient/apps/utils/live_source.c \
    nxclient/apps/utils/binput.c \
    nxclient/apps/utils/bfont.c \
    nxclient/apps/utils/bgui.c \
    utils/namevalue.c

LOCAL_MODULE := nxplay
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
