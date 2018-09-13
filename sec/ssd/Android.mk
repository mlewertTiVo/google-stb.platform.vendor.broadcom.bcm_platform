LOCAL_PATH := $(call my-dir)
ifneq ($(ANDROID_SUPPORTS_RPMB),n)
ifneq ($(SAGE_SUPPORT),n)

include $(CLEAR_VARS)

include ${REFSW_BASE_DIR}/magnum/syslib/sagelib/bsagelib_public.inc
include ${REFSW_BASE_DIR}/magnum/syslib/sagelib/bsagelib_tools.inc
include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_SRC_FILES := \
   ssd_platform.c \
   ssd_rpmb.c \
   ssd_tl.c \
   ssd_vfs.c

LOCAL_C_INCLUDES := \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/srai/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/platforms/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_drm/include/tl \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/prop \
    $(BSAGELIB_INCLUDES)
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))
LOCAL_HEADER_LIBRARIES := liblog_headers
LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += -DLOG_TAG=\"bcm-ssd\"

LOCAL_MULTILIB := 32
# LOCAL_MULTILIB := both
LOCAL_SHARED_LIBRARIES := \
   libnexus \
   libsrai \
   libcutils \
   libnxclient \
   liblog
LOCAL_MODULE := libsagessd
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
include $(BUILD_SHARED_LIBRARY)

endif
endif
