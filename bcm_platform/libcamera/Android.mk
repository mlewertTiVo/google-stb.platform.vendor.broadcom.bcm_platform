LOCAL_PATH:= $(call my-dir)
REFSW_PATH :=${LOCAL_PATH}/../brcm_nexus

include $(CLEAR_VARS)

ifeq ($(NEXUS_MODE),proxy)
NEXUS_LIB=libnexus
else
ifeq ($(NEXUS_WEBCPU),core1_server)
NEXUS_LIB=libnexus_webcpu
else
NEXUS_LIB=libnexus_client
endif
endif

# HAL module implemenation, not prelinked and stored in
# hw/<OVERLAY_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)

$(warning NX-BUILD: CFLAGS ${NEXUS_CFLAGS}, CLI-CFLAGS: ${NXCLIENT_CFLAGS}, CLI-INC: ${NXCLIENT_INCLUDES})

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libEGL libui libjpeg libcamera_client libcutils libdl libbinder libutils $(NEXUS_LIB)
LOCAL_CFLAGS:= $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID -DLOG_TAG=\"CameraDevHAL\"

HARDWARE_CAMERA_HAL_SRC := \
	CameraHAL_Mod.cpp \
	V4L2Camera.cpp              \
	CameraDevHAL.cpp
LOCAL_SRC_FILES:= \
	$(HARDWARE_CAMERA_HAL_SRC)

LOCAL_C_INCLUDES += external/jpeg frameworks/native/include
LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include			
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := camera.bcm_platform

include $(BUILD_SHARED_LIBRARY)
