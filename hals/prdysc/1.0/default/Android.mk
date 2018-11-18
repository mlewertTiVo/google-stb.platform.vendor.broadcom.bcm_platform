LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(SAGE_SUPPORT),y)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_MODULE := bcm.hardware.prdysc@1.0-service
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_INIT_RC := bcm.hardware.prdysc@1.0-service.rc

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES) \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/thirdparty/playready/http/include \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/thirdparty/playready/3.0/inc
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/nexus/1.0/default
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/pmlibservice
LOCAL_C_INCLUDES += $(TOP)/system/libhidl/libhidlmemory/include
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS := -Werror
LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += -DDRM_INLINING_SUPPORTED=1
LOCAL_CFLAGS += -DDRM_BUILD_PROFILE=900
LOCAL_CFLAGS += -DTARGET_LITTLE_ENDIAN=1
LOCAL_CFLAGS += -DTARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS=0

LOCAL_SRC_FILES := service.cpp
LOCAL_SRC_FILES += prdysc.cpp

LOCAL_SHARED_LIBRARIES := \
    bcm.hardware.prdysc@1.0 \
    bcm.hardware.nexus@1.0 \
    android.hidl.memory@1.0 \
    libhidlbase \
    libhidltransport \
    libhidlmemory \
    libbase \
    liblog \
    libutils \
    libcutils \
    libnexus \
    libnxclient \
    libnxwrap \
    libprdyhttp \
    libplayready30pk

include $(BUILD_EXECUTABLE)
endif

