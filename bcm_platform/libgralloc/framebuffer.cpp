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
#include "gr.h"

#include "NexusSurface.h"
#include "nexus_base_mmap.h"

using namespace android;
using android::Vector;

/*****************************************************************************/

// numbers of buffers for page flipping
#define NUM_BUFFERS 2

enum {
    PAGE_FLIP = 0x00000001,
    LOCKED = 0x00000002
};

struct fb_context_t {
    framebuffer_device_t  device;
};

/*****************************************************************************/

static int fb_setSwapInterval(struct framebuffer_device_t* dev,
            int interval)
{
    fb_context_t* ctx = (fb_context_t*)dev;
    if (interval < dev->minSwapInterval || interval > dev->maxSwapInterval)
        return -EINVAL;
    // FIXME: implement fb_setSwapInterval
    return 0;
}

static int fb_setUpdateRect(struct framebuffer_device_t* dev,
        int l, int t, int w, int h)
{
    if (((w|h) <= 0) || ((l|t)<0))
        return -EINVAL;
    return 0;
}


static int fb_post(struct framebuffer_device_t* dev, buffer_handle_t buffer)
{
    if (private_handle_t::validate(buffer) < 0)
        return -EINVAL;

    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
    NexusSurface *nexSurf = reinterpret_cast<NexusSurface *>(m->nexSurf);

    nexSurf->flip();

    return 0;
}

/*****************************************************************************/


/*
* MAPFRAME buffer is called from fb_device_open and also from 
* gralloc_alloc_framebuffer_locked function if the frame buffer
* device is not already mapped.
* 
* 
*/
int mapFrameBuffer(struct private_module_t* module)
{
    int fd = -1;
    pthread_mutex_lock(&module->lock);

    // already initialized...
    if (module->framebuffer)
    {
        BCM_DEBUG_ERRMSG("BRCM-GRALLOC: Framebuffer Already Initialized=Not Doing Anything=\n");
        pthread_mutex_unlock(&module->lock);
        return 0;
    }

    NexusSurface *nexSurf = new NexusSurface();
    nexSurf->init();
    module->nexSurf = reinterpret_cast<void *>(nexSurf);

    fd = (int)nexSurf->surface[0].surfHnd;

    /*
     * map the framebuffer
     */
    module->framebuffer = new private_handle_t(fd, nexSurf->lineLen * nexSurf->Yres, 0);
    module->numBuffers = 2;
    module->bufferMask = 0;
    module->framebuffer->nxSurfacePhysicalAddress = NEXUS_AddrToOffset(nexSurf->surface[0].surfBuf.buffer);
    pthread_mutex_unlock(&module->lock);

    return 0;
}


/*****************************************************************************/

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
    BCM_DEBUG_MSG("=====BRCM-GRALLOC: fb_device_open called====\n");

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

        status = mapFrameBuffer(m);
        if (status >= 0) {
            NexusSurface *nexSurf = reinterpret_cast<NexusSurface *>(m->nexSurf);
            const_cast<uint32_t&>(dev->device.flags) = 0;
            const_cast<uint32_t&>(dev->device.width) = nexSurf->Xres;
            const_cast<uint32_t&>(dev->device.height) = nexSurf->Yres;
            const_cast<int&>(dev->device.stride) = nexSurf->lineLen / 4;
            const_cast<int&>(dev->device.format) = HAL_PIXEL_FORMAT_RGBA_8888;
            const_cast<float&>(dev->device.xdpi) = nexSurf->Xdpi;
            const_cast<float&>(dev->device.ydpi) = nexSurf->Ydpi;
            const_cast<float&>(dev->device.fps) = nexSurf->fps;
            const_cast<int&>(dev->device.minSwapInterval) = 1;
            const_cast<int&>(dev->device.maxSwapInterval) = 1;
            *device = &dev->device.common;
        }
    }
    return status;
}
