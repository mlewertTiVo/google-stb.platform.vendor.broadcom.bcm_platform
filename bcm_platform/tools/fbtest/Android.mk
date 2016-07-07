LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES += libcutils

LOCAL_C_INCLUDES += $(TOP)/vendor/broadcom/stb/refsw/BSEAV/linux/driver/fbdev/utils/fbtest/include

FBDEV_SRC_ROOT := ../../../refsw/BSEAV/linux/driver/fbdev/utils/fbtest

LOCAL_SRC_FILES := \
   $(FBDEV_SRC_ROOT)/util.c \
   $(FBDEV_SRC_ROOT)/visual.c \
   $(FBDEV_SRC_ROOT)/drawops/cfb16.c \
   $(FBDEV_SRC_ROOT)/drawops/planar.c \
   $(FBDEV_SRC_ROOT)/drawops/cfb8.c \
   $(FBDEV_SRC_ROOT)/drawops/init.c \
   $(FBDEV_SRC_ROOT)/drawops/cfb2.c \
   $(FBDEV_SRC_ROOT)/drawops/iplan2.c \
   $(FBDEV_SRC_ROOT)/drawops/generic.c \
   $(FBDEV_SRC_ROOT)/drawops/cfb4.c \
   $(FBDEV_SRC_ROOT)/drawops/cfb32.c \
   $(FBDEV_SRC_ROOT)/drawops/cfb24.c \
   $(FBDEV_SRC_ROOT)/drawops/cfb.c \
   $(FBDEV_SRC_ROOT)/drawops/bitstream.c \
   $(FBDEV_SRC_ROOT)/visops/pseudocolor.c \
   $(FBDEV_SRC_ROOT)/visops/mono.c \
   $(FBDEV_SRC_ROOT)/visops/directcolor.c \
   $(FBDEV_SRC_ROOT)/visops/init.c \
   $(FBDEV_SRC_ROOT)/visops/truecolor.c \
   $(FBDEV_SRC_ROOT)/visops/grayscale.c \
   $(FBDEV_SRC_ROOT)/visops/ham.c \
   $(FBDEV_SRC_ROOT)/color.c \
   $(FBDEV_SRC_ROOT)/fonts/sun12x22.c \
   $(FBDEV_SRC_ROOT)/fonts/vga8x16.c \
   $(FBDEV_SRC_ROOT)/fonts/pearl8x8.c \
   $(FBDEV_SRC_ROOT)/pixmap.c \
   $(FBDEV_SRC_ROOT)/colormap.c \
   $(FBDEV_SRC_ROOT)/main.c \
   $(FBDEV_SRC_ROOT)/tests/test006.c \
   $(FBDEV_SRC_ROOT)/tests/test011.c \
   $(FBDEV_SRC_ROOT)/tests/test008.c \
   $(FBDEV_SRC_ROOT)/tests/test007.c \
   $(FBDEV_SRC_ROOT)/tests/test005.c \
   $(FBDEV_SRC_ROOT)/tests/test010.c \
   $(FBDEV_SRC_ROOT)/tests/test012.c \
   $(FBDEV_SRC_ROOT)/tests/test001.c \
   $(FBDEV_SRC_ROOT)/tests/test002.c \
   $(FBDEV_SRC_ROOT)/tests/test003.c \
   $(FBDEV_SRC_ROOT)/tests.c \
   $(FBDEV_SRC_ROOT)/fb.c \
   $(FBDEV_SRC_ROOT)/console.c \
   $(FBDEV_SRC_ROOT)/clut.c

LOCAL_MODULE := fbtest
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_OPTIONAL_EXECUTABLES)

include $(BUILD_EXECUTABLE)

