LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnxclient

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES) \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/apps/utils \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/utils \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/bcmplayer/include \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/magnum/commonutils/vlc \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/opensource/glob \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/tshdrbuilder
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += -DLINUX -DB_REFSW_ANDROID

PREFIX_RELATIVE_PATH := ../../../
NXCLIENT_APPS_UTILS_SRC := \
   $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/bfile_crypto.c \
   $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/bfont.c \
   $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/bgui.c \
   $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/binput.c \
   $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/brecord_gui.c \
   $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/dvr_crypto.c \
   $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/live_decode.c \
   $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/live_source.c \
   $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/media_player.c \
   $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/media_player_ip.c \
   $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/media_player_bip.c \
   $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/media_probe.c \
   $(PREFIX_RELATIVE_PATH)refsw/nexus/utils/namevalue.c \
   $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/nxapps_cmdline.c \
   $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/picdecoder.c \
   $(PREFIX_RELATIVE_PATH)refsw/BSEAV/opensource/glob/glob.c \
   $(PREFIX_RELATIVE_PATH)refsw/BSEAV/lib/tshdrbuilder/tshdrbuilder.c \
   $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/tspsimgr3.c \
   $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/wav_file.c

LOCAL_SRC_FILES := $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/transcode.c \
                   $(PREFIX_RELATIVE_PATH)refsw/BSEAV/lib/bcmplayer/src/bcmindexer.c \
                   $(PREFIX_RELATIVE_PATH)refsw/magnum/commonutils/vlc/bvlc.c
LOCAL_SRC_FILES += $(NXCLIENT_APPS_UTILS_SRC)

LOCAL_MODULE := nxtrans
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

