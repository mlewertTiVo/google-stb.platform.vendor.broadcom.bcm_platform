LOCAL_PATH := $(call my-dir)

ifneq ($(SAGE_SUPPORT),n)

include $(CLEAR_VARS)
LOCAL_PATH := $(TOP)/${REFSW_BASE_DIR}/BSEAV/lib/security/sage/secure_log
LOCAL_PATH := $(subst ${ANDROID}/,,$(LOCAL_PATH))

include ${REFSW_BASE_DIR}/magnum/syslib/sagelib/bsagelib_public.inc
include ${REFSW_BASE_DIR}/magnum/syslib/sagelib/bsagelib_tools.inc

LOCAL_SRC_FILES := src/secure_log_tl.c
LOCAL_C_INCLUDES := \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/secure_log/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/srai/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/platforms/include \
    $(BSAGELIB_INCLUDES)
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))
LOCAL_CFLAGS += -DNDEBUG
LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)

LOCAL_MULTILIB := 32
# LOCAL_MULTILIB := both
LOCAL_SHARED_LIBRARIES := libnexus libsrai
LOCAL_MODULE := libsageseclog
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_PATH := $(TOP)/${REFSW_BASE_DIR}/BSEAV/lib/security/sage/secure_log
LOCAL_PATH := $(subst ${ANDROID}/,,$(LOCAL_PATH))

include $(NEXUS_TOP)/nxclient/include/nxclient.inc
include ${REFSW_BASE_DIR}/magnum/syslib/sagelib/bsagelib_public.inc
include ${REFSW_BASE_DIR}/magnum/syslib/sagelib/bsagelib_tools.inc

LOCAL_SRC_FILES := examples/secure_log_app.c
LOCAL_C_INCLUDES := \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/secure_log/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/srai/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/platforms/include \
    $(BSAGELIB_INCLUDES) \
    $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))
LOCAL_CFLAGS += -DNDEBUG
LOCAL_CFLAGS += -DRUN_AS_NXCLIENT
LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)

LOCAL_SHARED_LIBRARIES := libnexus libsrai libsageseclog libcutils libnxclient

LOCAL_MODULE := sageseclog
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := EXECUTABLES
include $(BUILD_EXECUTABLE)

endif

