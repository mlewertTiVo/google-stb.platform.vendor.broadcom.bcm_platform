LOCAL_PATH := $(call my-dir)

LOCAL_PATH := ${REFSW_BASE_DIR}
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    BSEAV/lib/security/sage/utility/examples/rsa/utility_rsa_host.c

LOCAL_C_INCLUDES := \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libsecurity/bdbg2alog \
    bionic

LOCAL_SHARED_LIBRARIES += \
    liblog \
    libnexus \
    libnxclient \
    libsrai \
    librsa_tl

LOCAL_MODULE := rsa_tl_example

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += -DNXCLIENT_SUPPORT
LOCAL_CFLAGS += -DBDBG2ALOG_ENABLE_LOGS=1
LOCAL_CFLAGS += -DDRM_INLINING_SUPPORTED=1 -DDRM_BUILD_PROFILE=900 -DTARGET_LITTLE_ENDIAN=1 -DTARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS=0
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/common_drm/include
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/srai/include
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/magnum/syslib/sagelib/include
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/platforms/include
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/utility/src
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/utility/include
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/common_drm/include/tl
LOCAL_CFLAGS += -I$(ANDROID_BUILD_TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/include/

LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
