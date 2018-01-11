ifneq ($(ANDROID_SUPPORTS_PLAYREADY), n)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc
include $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/thirdparty/playready/http/prdyhttp.inc
include $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/thirdparty/playready/piff/prdypiff.inc

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libsecurity/bdbg2alog

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/apps/utils
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/utils
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS += -DBDBG2ALOG_ENABLE_LOGS=1 -DBDBG_NO_MSG=1

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += -DLINUX -DB_REFSW_ANDROID

# SAGE
LOCAL_CFLAGS += $(addprefix -I,$(COMMON_CRYPTO_INCLUDES))
LOCAL_CFLAGS += -I$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/drmrootfs
LOCAL_CFLAGS += -I$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/thirdparty/playready/3.0/inc
LOCAL_CFLAGS += -I$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/thirdparty/playready/http/include
LOCAL_CFLAGS += -I$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/srai/include
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
   libdrmrootfs \
   libplayready30pk

REFSW_RELATIVE_PATH := ../../../refsw
PRDY_EXTRA_SRC := $(REFSW_RELATIVE_PATH)/BSEAV/thirdparty/playready/http/src/prdy_http.c

LOCAL_SRC_FILES := $(REFSW_RELATIVE_PATH)/BSEAV/thirdparty/playready/3.0/examples/prdy30/test_prpd30.c
LOCAL_SRC_FILES += $(PRDY_EXTRA_SRC)

LOCAL_MODULE := nxtest_prpd30
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

endif
