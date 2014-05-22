LOCAL_PATH:= $(call my-dir)
REFSW_PATH :=${LOCAL_PATH}/../brcm_nexus

include $(CLEAR_VARS)

include $(REFSW_PATH)/bin/include/platform_app.inc

# HAL module implemenation, not prelinked and stored in
# hw/<OVERLAY_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)


LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libEGL libui libjpeg libcamera_client libcutils libdl libbinder libutils libnexus
LOCAL_CFLAGS:= $(NEXUS_CFLAGS) -DANDROID -DLOG_TAG=\"CameraDevHAL\"

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
