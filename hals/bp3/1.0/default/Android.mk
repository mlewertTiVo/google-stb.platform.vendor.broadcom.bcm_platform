LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc
include $(MAGNUM_TOP)/portinginterface/hsm/bhsm_defs.inc

LOCAL_MODULE := bcm.hardware.bp3@1.0-service
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_INIT_RC := bcm.hardware.bp3@1.0-service.rc

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES) \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/bp3 \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/bp3/app \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/bp3/utils \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/srai/include \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/platforms/include \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/tools/bp3
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS := -Werror
LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += -DBHSM_ZEUS_VER_MAJOR=$(BHSM_ZEUS_VER_MAJOR) -DBHSM_ZEUS_VER_MINOR=$(BHSM_ZEUS_VER_MINOR)
LOCAL_CFLAGS += -DBP3_TA_FEATURE_READ_SUPPORT

LOCAL_SRC_FILES := service.cpp
LOCAL_SRC_FILES += bp3.cpp

LOCAL_SHARED_LIBRARIES := \
    bcm.hardware.bp3@1.0 \
    libhidlbase \
    libhidltransport \
    libbase \
    liblog \
    libutils \
    libbp3_host \
    libnexus \
    libnxclient \
    libsrai \
    libcurl \
    libssl \
    libz

include $(BUILD_EXECUTABLE)
