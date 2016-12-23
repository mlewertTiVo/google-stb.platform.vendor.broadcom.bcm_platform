LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnxclient

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES) \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/apps/utils \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/utils \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/bcmplayer/include \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/magnum/commonutils/vlc \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/glob \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/tshdrbuilder

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += -DLINUX -DB_REFSW_ANDROID

LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/
NXCLIENT_APPS_UTILS_SRC := \
   refsw/nexus/nxclient/apps/utils/bfile_crypto.c \
   refsw/nexus/nxclient/apps/utils/bfont.c \
   refsw/nexus/nxclient/apps/utils/bgui.c \
   refsw/nexus/nxclient/apps/utils/binput.c \
   refsw/nexus/nxclient/apps/utils/brecord_gui.c \
   refsw/nexus/nxclient/apps/utils/dvr_crypto.c \
   refsw/nexus/nxclient/apps/utils/live_decode.c \
   refsw/nexus/nxclient/apps/utils/live_source.c \
   refsw/nexus/nxclient/apps/utils/media_player.c \
   refsw/nexus/nxclient/apps/utils/media_player_ip.c \
   refsw/nexus/nxclient/apps/utils/media_player_bip.c \
   refsw/nexus/nxclient/apps/utils/media_probe.c \
   refsw/nexus/utils/namevalue.c \
   refsw/nexus/nxclient/apps/utils/nxapps_cmdline.c \
   refsw/nexus/nxclient/apps/utils/picdecoder.c \
   refsw/BSEAV/lib/glob/glob.c \
   refsw/BSEAV/lib/tshdrbuilder/tshdrbuilder.c \
   refsw/nexus/nxclient/apps/utils/tspsimgr3.c \
   refsw/nexus/nxclient/apps/utils/wav_file.c

LOCAL_SRC_FILES := refsw/nexus/nxclient/apps/transcode.c \
                   refsw/BSEAV/lib/bcmplayer/src/bcmindexer.c \
                   refsw/magnum/commonutils/vlc/bvlc.c
LOCAL_SRC_FILES += $(NXCLIENT_APPS_UTILS_SRC)

LOCAL_MODULE := nxtrans
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

