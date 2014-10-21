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
#include <cutils/atomic.h>
#include <utils/Vector.h>

#if HAVE_ANDROID_OS
#include <linux/fb.h>
#endif

#include "gralloc_priv.h"
#include "cutils/properties.h"

using namespace android;
using android::Vector;

/* note: matching other parts of the integration, we
 *       want to default product resolution to 1080p.
 */
#define DISPLAY_DEFAULT_HD_RES       "1080p"
#define DISPLAY_HD_RES_PROP          "ro.hd_output_format"

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
      alloc_device_t* gralloc_device;
      BCM_DEBUG_MSG("BRCM-GRALLOC: fb_device_open Calling Gralloc Open\n");
      status = gralloc_open(module, &gralloc_device);
      if (status < 0)
         return status;

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

      BCM_DEBUG_MSG("BRCM-GRALLOC: fb_device_open Directly Calling MapFrameBuffer\n");

      if (status >= 0) {
         const_cast<uint32_t&>(dev->device.flags) = 0;
         const_cast<uint32_t&>(dev->device.width) = 1280;
         const_cast<uint32_t&>(dev->device.height) = 720;
         if (property_get(DISPLAY_HD_RES_PROP, value, DISPLAY_DEFAULT_HD_RES)) {
            if (!strncmp((char *) value, DISPLAY_DEFAULT_HD_RES, 5)) {
               const_cast<uint32_t&>(dev->device.width) = 1920;
               const_cast<uint32_t&>(dev->device.height) = 1080;
            }
         }
         const_cast<int&>(dev->device.stride) = dev->device.width * 4;

         // those are hardcoded already.  note that dpi is overtaken by the ro.lcd_density anyways.
         const_cast<int&>(dev->device.format) = HAL_PIXEL_FORMAT_RGBA_8888;
         const_cast<float&>(dev->device.xdpi) = 160;
         const_cast<float&>(dev->device.ydpi) = 160;
         const_cast<float&>(dev->device.fps) = 60;
         const_cast<int&>(dev->device.minSwapInterval) = 1;
         const_cast<int&>(dev->device.maxSwapInterval) = 1;
         *device = &dev->device.common;
      }
   }
   return status;
}
