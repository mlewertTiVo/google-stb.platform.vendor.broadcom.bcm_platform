LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_PATH := $(subst ${ANDROID}/,,$(LOCAL_PATH))

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

SRC_PREFIX := ../../../
NXCLIENT_APPS_UTILS_SRC := \
   $(SRC_PREFIX)/refsw/nexus/nxclient/apps/utils/bfile_crypto.c \
   $(SRC_PREFIX)/refsw/nexus/nxclient/apps/utils/bfont.c \
   $(SRC_PREFIX)/refsw/nexus/nxclient/apps/utils/bgui.c \
   $(SRC_PREFIX)/refsw/nexus/nxclient/apps/utils/binput.c \
   $(SRC_PREFIX)/refsw/nexus/nxclient/apps/utils/brecord_gui.c \
   $(SRC_PREFIX)/refsw/nexus/nxclient/apps/utils/dvr_crypto.c \
   $(SRC_PREFIX)/refsw/nexus/nxclient/apps/utils/live_source.c \
   $(SRC_PREFIX)/refsw/nexus/nxclient/apps/utils/media_player.c \
   $(SRC_PREFIX)/refsw/nexus/nxclient/apps/utils/media_player_ip.c \
   $(SRC_PREFIX)/refsw/nexus/nxclient/apps/utils/media_player_bip.c \
   $(SRC_PREFIX)/refsw/nexus/nxclient/apps/utils/media_probe.c \
   $(SRC_PREFIX)/refsw/nexus/utils/namevalue.c \
   $(SRC_PREFIX)/refsw/nexus/nxclient/apps/utils/nxapps_cmdline.c \
   $(SRC_PREFIX)/refsw/BSEAV/opensource/glob/glob.c

LOCAL_SRC_FILES := $(SRC_PREFIX)/refsw/nexus/nxclient/apps/play.c
LOCAL_SRC_FILES += $(NXCLIENT_APPS_UTILS_SRC)

LOCAL_MODULE := nxplay
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
