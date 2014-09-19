REFSW_PATH := $(TOP)/vendor/broadcom/bcm_platform/brcm_nexus
LOCAL_PATH := $(call my-dir)

#
# Executable send_cec
#
include $(CLEAR_VARS)

$(warning NX-BUILD: CFLAGS ${NEXUS_CFLAGS}, CLI-CFLAGS: ${NXCLIENT_CFLAGS}, CLI-INC: ${NXCLIENT_INCLUDES})

LOCAL_SRC_FILES := \
    send_cec.cpp

LOCAL_SHARED_LIBRARIES := \
    libcutils			  \
	liblog				  \
	libcutils			  \
	libdl				  \
	libbinder			  \
	libutils			  \
	libnexusipcclient

LOCAL_MODULE := send_cec

LOCAL_MODULE_TAGS := optional tests

LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include \
					$(REFSW_PATH)/../libnexusservice \
					$(REFSW_PATH)/../libnexusipc

LOCAL_CFLAGS := $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID

ifeq ($(TARGET_ARCH),mips)
LOCAL_CFLAGS += -DTARGET_ARCH_MIPS
endif

LOCAL_CFLAGS += -Wno-multichar -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGI=ALOGI -DLOGV=ALOGV

include $(BUILD_EXECUTABLE)

ANDROID_KK_OR_OLDER := $(shell test "${BRCM_ANDROID_VERSION}" \< "l" && echo "y")

ifeq ($(ANDROID_KK_OR_OLDER),y)

#
# Executable get_cec
#
include $(CLEAR_VARS)

$(warning NX-BUILD: CFLAGS ${NEXUS_CFLAGS}, CLI-CFLAGS: ${NXCLIENT_CFLAGS}, CLI-INC: ${NXCLIENT_INCLUDES})

LOCAL_SRC_FILES := \
    get_cec.cpp

LOCAL_SHARED_LIBRARIES := \
    libcutils			  \
	liblog				  \
	libcutils			  \
	libdl				  \
	libbinder			  \
	libutils			  \
	libnexusipcclient

LOCAL_MODULE := get_cec

LOCAL_MODULE_TAGS := optional tests

LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include \
					$(REFSW_PATH)/../libnexusservice \
					$(REFSW_PATH)/../libnexusipc

LOCAL_CFLAGS := $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID

ifeq ($(TARGET_ARCH),mips)
LOCAL_CFLAGS += -DTARGET_ARCH_MIPS
endif

LOCAL_CFLAGS += -Wno-multichar -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGI=ALOGI -DLOGV=ALOGV

include $(BUILD_EXECUTABLE)

endif
