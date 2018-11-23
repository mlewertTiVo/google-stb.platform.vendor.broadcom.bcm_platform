LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc

ifeq ($(ANDROID_SUPPORTS_KEYMASTER),y)
ifeq ($(SAGE_SUPPORT),y)

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_C_INCLUDES := \
    $(TOP)/bionic \
    $(TOP)/system/core/liblog/include \
    $(TOP)/system/keymaster/include \
    $(TOP)/external/stlport/stlport \
    $(TOP)/external/openssl/include \
    $(TOP)/hardware/libhardware/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/sage/manufacturing/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/sage/keymaster/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/sage/srai/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/magnum/syslib/sagelib/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/sage/platforms/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/sage/keymaster/src \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/drivers/nx_ashmem
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/pmlibservice
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/prop
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_SHARED_LIBRARIES := libcutils libutils liblog libnexus
LOCAL_SHARED_LIBRARIES += libnxclient libnxwrap
LOCAL_SHARED_LIBRARIES += bcm.hardware.nexus@1.1
LOCAL_SHARED_LIBRARIES += libmfgtl
LOCAL_SHARED_LIBRARIES += libkmtl
LOCAL_SHARED_LIBRARIES += libssl
LOCAL_SHARED_LIBRARIES += libsrai
LOCAL_SHARED_LIBRARIES += libcrypto
LOCAL_SHARED_LIBRARIES += libkeymaster_messages

LOCAL_SRC_FILES := keymaster.cpp
LOCAL_MODULE := keystore.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_RELATIVE_PATH := hw
include $(BUILD_SHARED_LIBRARY)

endif
endif
