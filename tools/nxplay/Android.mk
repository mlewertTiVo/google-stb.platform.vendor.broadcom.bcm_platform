LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_PATH := $(subst ${ANDROID}/,,$(LOCAL_PATH))

include $(NEXUS_TOP)/nxclient/include/nxclient.inc
LOCAL_PRELINK_MODULE := false

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(NEXUS_TOP)/nxclient/apps/utils
LOCAL_C_INCLUDES += $(NEXUS_TOP)/utils
LOCAL_C_INCLUDES += $(BSEAV_TOP)/lib/bcmplayer/include
LOCAL_C_INCLUDES += $(BSEAV_TOP)/lib/tshdrbuilder
LOCAL_C_INCLUDES += $(BSEAV_TOP)/opensource/glob
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += $(NEXUS_CFLAGS)
LOCAL_CFLAGS += $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS))
LOCAL_CFLAGS += $(addprefix -D,$(NEXUS_APP_DEFINES))

LOCAL_SHARED_LIBRARIES := libnexus libnxclient

LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/
NXCLIENT_APPS_UTILS_SRC := \
   refsw/nexus/nxclient/apps/utils/bfile_crypto.c \
   refsw/nexus/nxclient/apps/utils/bfont.c \
   refsw/nexus/nxclient/apps/utils/bgui.c \
   refsw/nexus/nxclient/apps/utils/binput.c \
   refsw/nexus/nxclient/apps/utils/brecord_gui.c \
   refsw/nexus/nxclient/apps/utils/dvr_crypto.c \
   refsw/nexus/nxclient/apps/utils/live_source.c \
   refsw/nexus/nxclient/apps/utils/media_player.c \
   refsw/nexus/nxclient/apps/utils/media_player_ip.c \
   refsw/nexus/nxclient/apps/utils/media_player_bip.c \
   refsw/nexus/nxclient/apps/utils/media_probe.c \
   refsw/nexus/utils/namevalue.c \
   refsw/nexus/nxclient/apps/utils/nxapps_cmdline.c \
   refsw/BSEAV/opensource/glob/glob.c

LOCAL_SRC_FILES := refsw/nexus/nxclient/apps/play.c
LOCAL_SRC_FILES += $(NXCLIENT_APPS_UTILS_SRC)

LOCAL_MODULE := nxplay
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
