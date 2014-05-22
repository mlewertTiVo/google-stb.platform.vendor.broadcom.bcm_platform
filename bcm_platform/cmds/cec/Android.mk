REFSW_PATH := $(TOP)/vendor/broadcom/bcm_platform/brcm_nexus
LOCAL_PATH := $(call my-dir)

#
# Executable send_cec
#
include $(CLEAR_VARS)
include $(REFSW_PATH)/bin/include/platform_app.inc

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

LOCAL_CFLAGS := $(NEXUS_CFLAGS) -DANDROID

ifeq ($(TARGET_ARCH),mips)
LOCAL_CFLAGS += -DTARGET_ARCH_MIPS
endif

LOCAL_CFLAGS += -Wno-multichar -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGI=ALOGI -DLOGV=ALOGV

include $(BUILD_EXECUTABLE)

#
# Executable get_cec
#
include $(CLEAR_VARS)
include $(REFSW_PATH)/bin/include/platform_app.inc

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

LOCAL_CFLAGS := $(NEXUS_CFLAGS) -DANDROID

ifeq ($(TARGET_ARCH),mips)
LOCAL_CFLAGS += -DTARGET_ARCH_MIPS
endif

LOCAL_CFLAGS += -Wno-multichar -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGI=ALOGI -DLOGV=ALOGV

include $(BUILD_EXECUTABLE)
