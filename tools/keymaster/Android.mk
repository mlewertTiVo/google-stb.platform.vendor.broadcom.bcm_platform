ifeq ($(SAGE_SUPPORT),y)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

SOURCE_RELATIVE_PATH := ../../../refsw/BSEAV/lib/security/sage/keymaster/tests
LOCAL_SRC_FILES := \
    $(SOURCE_RELATIVE_PATH)/keymaster_test.c \
    $(SOURCE_RELATIVE_PATH)/keymaster_key_params.c \
    $(SOURCE_RELATIVE_PATH)/keymaster_keygen.c \
    $(SOURCE_RELATIVE_PATH)/keymaster_crypto_utils.c \
    $(SOURCE_RELATIVE_PATH)/keymaster_crypto_aes.c \
    $(SOURCE_RELATIVE_PATH)/keymaster_crypto_hmac.c \
    $(SOURCE_RELATIVE_PATH)/keymaster_crypto_rsa.c

LOCAL_C_INCLUDES := \
    $(TOP)/bionic \
    $(TOP)/external/stlport/stlport \
    $(TOP)/external/openssl/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/sage/keymaster/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/nexus/nxclient/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/sage/srai/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/magnum/syslib/sagelib/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/sage/platforms/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/sage/keymaster/src \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/sage/keymaster/include

LOCAL_SHARED_LIBRARIES += \
    liblog \
    libutils \
    libnexus \
    libnxclient \
    libkmtl \
    libssl \
    libcrypto \
    libsrai

LOCAL_MODULE := keymaster_test

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += -DLINUX -DB_REFSW_ANDROID

LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
endif # SAGE_SUPPORT

