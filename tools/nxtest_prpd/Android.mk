LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc
include $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/thirdparty/playready/http/prdyhttp.inc
include $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/common_crypto/common_crypto.inc

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libsecurity/bdbg2alog

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/apps/utils
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/utils
LOCAL_C_INCLUDES += $(TOP)/external/boringssl/src/include
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS += -DBDBG2ALOG_ENABLE_LOGS=1 -DBDBG_NO_MSG=1

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += -DLINUX -DB_REFSW_ANDROID

# SAGE
LOCAL_CFLAGS += $(addprefix -I,$(COMMON_CRYPTO_INCLUDES))
LOCAL_CFLAGS += -I$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/drmrootfs
LOCAL_CFLAGS += -I$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/thirdparty/playready/2.5/inc
LOCAL_CFLAGS += -I$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/srai/include
LOCAL_CFLAGS += -I$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/common_drm/include
LOCAL_CFLAGS += -DSAGE_ENABLE

# Playready SDK flags
DRM_BUILD_PROFILE   := 900
LOCAL_CFLAGS += -DDRM_BUILD_PROFILE=$(DRM_BUILD_PROFILE)
LOCAL_CFLAGS += -DTARGET_LITTLE_ENDIAN=1
LOCAL_CFLAGS += -DTARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS=1


LOCAL_SHARED_LIBRARIES := \
   liblog \
   libcutils \
   libnexus \
   libnxclient \
   libsrai \
   libcmndrmprdy \
   libcmndrm \
   libdrmrootfs

LOCAL_STATIC_LIBRARIES := \
    libplayreadypk_host

REFSW_RELATIVE_PATH := ../../../refsw
LOCAL_SRC_FILES := $(REFSW_RELATIVE_PATH)/BSEAV/lib/security/common_drm/examples/playready/test_prpd.c

LOCAL_MODULE := nxtest_prpd
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)