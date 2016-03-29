LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(TARGET_2ND_ARCH),arm)
  LOCAL_MODULE_RELATIVE_PATH := hw
else
  LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
endif
LOCAL_CFLAGS := -fno-short-enums -DHAVE_CONFIG_H
# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_C_INCLUDES := \
	external/jpeg \
        system/media/camera/include

LOCAL_SRC_FILES := \
	CameraFactory.cpp \
	CameraHal.cpp \
	CameraHardware.cpp \
	Converter.cpp \
	SurfaceDesc.cpp \
	SurfaceSize.cpp \
	Utils.cpp \
	V4L2Camera.cpp \

LOCAL_SHARED_LIBRARIES := \
	libcamera_client \
	libcutils \
	libjpeg \
	liblog \
	libui \
	libutils \

LOCAL_MODULE := camera.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
