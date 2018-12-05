LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifneq ($(ANDROID_SUPPORTS_RPMB),n)
ifneq ($(SAGE_SUPPORT),n)

LOCAL_C_INCLUDES := ${BCM_VENDOR_STB_ROOT}/bcm_platform/sec/ssd
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap
LOCAL_C_INCLUDES += ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir
LOCAL_C_INCLUDES += ${BCM_VENDOR_STB_ROOT}/bcm_platform/prop
LOCAL_C_INCLUDES += ${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/pmlibservice
LOCAL_C_INCLUDES += system/core/base/include
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))
LOCAL_HEADER_LIBRARIES := liblog_headers
LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_MULTILIB := 32
# LOCAL_MULTILIB := both
LOCAL_SHARED_LIBRARIES := \
   libsagessd \
   libnexus \
   libsrai \
   libcutils \
   libnxclient \
   liblog \
   libnxwrap
LOCAL_SHARED_LIBRARIES += \
   bcm.hardware.nexus@1.1

LOCAL_SRC_FILES := nxssd.cpp
LOCAL_INIT_RC := nxssd.rc
LOCAL_MODULE := nxssd
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := EXECUTABLES

include $(BUILD_EXECUTABLE)

endif
endif

