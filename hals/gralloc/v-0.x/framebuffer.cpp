/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <sys/mman.h>
#include <dlfcn.h>
#include <cutils/ashmem.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include <cutils/log.h>
#include <utils/Vector.h>

#if HAVE_ANDROID_OS
#include <linux/fb.h>
#endif

#include <cutils/atomic.h>
#include "gralloc_priv.h"
#include "cutils/properties.h"

using namespace android;
using android::Vector;

/* note: matching other parts of the integration, we
 *       want to default product resolution to 1080p.
 */
#define GRAPHICS_RES_WIDTH_DEFAULT  1920
#define GRAPHICS_RES_HEIGHT_DEFAULT 1080
#define GRAPHICS_RES_WIDTH_PROP     "ro.nx.hwc2.afb.w"
#define GRAPHICS_RES_HEIGHT_PROP    "ro.nx.hwc2.afb.h"

struct fb_context_t {
   framebuffer_device_t device;
};

static int fb_setSwapInterval(struct framebuffer_device_t* dev,
            int interval)
{
   fb_context_t* ctx = (fb_context_t*)dev;
   if (interval < dev->minSwapInterval || interval > dev->maxSwapInterval)
      return -EINVAL;
   return 0;
}

static int fb_post(struct framebuffer_device_t* dev, buffer_handle_t buffer)
{
   (void) dev; (void)buffer;
   return 0;
}

static int fb_close(struct hw_device_t *dev)
{
   fb_context_t* ctx = (fb_context_t*)dev;
   if (ctx) {
      free(ctx);
   }
   return 0;
}

int fb_device_open(hw_module_t const* module, const char* name,
        hw_device_t** device)
{
   int status = -EINVAL;
   char value[PROPERTY_VALUE_MAX];

   if (!strcmp(name, GRALLOC_HARDWARE_FB0)) {
      /* initialize our state here */
      fb_context_t *dev = (fb_context_t*)malloc(sizeof(*dev));
      memset(dev, 0, sizeof(*dev));

      /* initialize the procs */
      dev->device.common.tag = HARDWARE_DEVICE_TAG;
      dev->device.common.version = 0;
      dev->device.common.module = const_cast<hw_module_t*>(module);
      dev->device.common.close = fb_close;
      dev->device.setSwapInterval = fb_setSwapInterval;
      dev->device.post            = fb_post;
      dev->device.setUpdateRect = 0;

      private_module_t* m = reinterpret_cast<private_module_t*>(dev->device.common.module);

      if (status >= 0) {
         const_cast<uint32_t&>(dev->device.flags) = 0;
         const_cast<uint32_t&>(dev->device.width) = property_get_int32(
                 GRAPHICS_RES_WIDTH_PROP, GRAPHICS_RES_WIDTH_DEFAULT);
         const_cast<uint32_t&>(dev->device.height) =  property_get_int32(
                 GRAPHICS_RES_HEIGHT_PROP, GRAPHICS_RES_HEIGHT_DEFAULT);
         const_cast<int&>(dev->device.stride) = dev->device.width * 4;

         // those are hardcoded already.  note that dpi is overtaken by the ro.lcd_density anyways.
         const_cast<int&>(dev->device.format) = HAL_PIXEL_FORMAT_RGBA_8888;
         const_cast<float&>(dev->device.xdpi) = 0;
         const_cast<float&>(dev->device.ydpi) = 0;
         const_cast<float&>(dev->device.fps) = 60;
         const_cast<int&>(dev->device.minSwapInterval) = 1;
         const_cast<int&>(dev->device.maxSwapInterval) = 1;
         *device = &dev->device.common;
      }
   }
   return status;
}
