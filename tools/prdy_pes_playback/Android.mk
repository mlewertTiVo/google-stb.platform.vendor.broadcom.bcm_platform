LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    pr_piff_playback.c \
    bpiff.c \
    piff_parser.c

LOCAL_C_INCLUDES := \
    bionic

LOCAL_SHARED_LIBRARIES += \
    liblog \
    libutils \
    libnexus \
    libcmndrmprdy \
    libnxclient \
    libsrai

LOCAL_MODULE := prdy_pes_playback_nxclient

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += -DDRM_INLINING_SUPPORTED=1 -DDRM_BUILD_PROFILE=900 -DTARGET_LITTLE_ENDIAN=1 -DTARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS=0
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/extensions/security/usercmd/include/40nm
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/extensions/security/otpmsp/include/40nm
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/extensions/security/keyladder/include/40nm
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/extensions/security/securersa/include
# drmtypes.h
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/playready/2.5/inc
# common_crypto.h
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/common_crypto/include
# drm_prdy_http.h
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/common_drm/include
# sage_srai.h
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/srai/include
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/magnum/syslib/sagelib/include

# bmp4_util.h
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/media
# bioatom.h
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/utils
# bmp4_fragment_make_segment.c
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/media/build
# nxclient.h
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/include

LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
