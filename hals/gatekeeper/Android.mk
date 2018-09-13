LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc

ifeq ($(ANDROID_SUPPORTS_KEYMASTER),y)
ifeq ($(SAGE_SUPPORT),y)

LOCAL_SRC_FILES := gk.cpp

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += -DLOG_TAG=\"bcm-gk\"
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror
ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_CFLAGS += -DBCM_FULL_TREBLE
endif

LOCAL_C_INCLUDES += \
    $(TOP)/hardware/libhardware/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/drivers/nx_ashmem \
    $(NXCLIENT_INCLUDES)\
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/sage/manufacturing/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/sage/keymaster/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/sage/srai/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/magnum/syslib/sagelib/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/sage/platforms/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/sage/keymaster/src \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/sage/keymaster/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap
ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/nexus/1.0/default
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/pmlibservice
else
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxbinder
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxevtsrc
endif
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/prop
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_HEADER_LIBRARIES := liblog_headers
LOCAL_SHARED_LIBRARIES := libcutils libutils liblog libnexus
LOCAL_SHARED_LIBRARIES += libnxclient libnxwrap
ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_SHARED_LIBRARIES += bcm.hardware.nexus@1.0
else
LOCAL_SHARED_LIBRARIES += libbinder
endif
LOCAL_SHARED_LIBRARIES += libmfgtl
LOCAL_SHARED_LIBRARIES += libkmtl
LOCAL_SHARED_LIBRARIES += libsrai

LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE := gatekeeper.$(TARGET_BOARD_PLATFORM)

include $(BUILD_SHARED_LIBRARY)

endif
endif

