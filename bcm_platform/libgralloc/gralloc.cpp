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

#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <dlfcn.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>
#include <utils/Vector.h>

#include "gralloc_priv.h"
#include "gr.h"

#include "nexus_base_mmap.h"

// default platform layer to render to nexus
#include "EGL/egl.h"
#include "converter.h"

#include "NexusSurface.h"

using namespace android;
using android::Vector;

extern "C" void BEGLint_BufferGetRequirements(BEGL_PixmapInfo *, BEGL_BufferSettings *);

/*****************************************************************************/

struct gralloc_context_t {
    alloc_device_t  device;
};

static int 
gralloc_alloc_buffer(alloc_device_t* dev,int w, int h,
       int format, int usage, buffer_handle_t* pHandle,int *pStride);
/*****************************************************************************/

int fb_device_open(const hw_module_t* module, const char* name,
        hw_device_t** device);

static int gralloc_device_open(const hw_module_t* module, const char* name,
        hw_device_t** device);

extern int gralloc_lock(gralloc_module_t const* module,
        buffer_handle_t handle, int usage,
        int l, int t, int w, int h,
        void** vaddr);

extern int gralloc_unlock(gralloc_module_t const* module, 
        buffer_handle_t handle);

extern int gralloc_register_buffer(gralloc_module_t const* module,
        buffer_handle_t handle);

extern int gralloc_unregister_buffer(gralloc_module_t const* module,
        buffer_handle_t handle);

NEXUS_PixelFormat getNexusPixelFormat(int pixelFmt, 
                                      int *bpp);

/*****************************************************************************/

static struct hw_module_methods_t gralloc_module_methods = {
        open: gralloc_device_open
};

struct private_module_t HAL_MODULE_INFO_SYM = {
   base: {
      common: {
         tag: HARDWARE_MODULE_TAG,
         version_major: 1,
         version_minor: 0,
         id: GRALLOC_HARDWARE_MODULE_ID,
         name: "Graphics Memory Allocator Module",
         author: "The Android Open Source Project",
         methods: &gralloc_module_methods,
         dso: NULL,
         reserved: {0}
      },
      registerBuffer: gralloc_register_buffer,
      unregisterBuffer: gralloc_unregister_buffer,
      lock: gralloc_lock,
      unlock: gralloc_unlock,
      perform: NULL,
      lock_ycbcr: NULL,
      reserved_proc: {0}
   },
   framebuffer: 0,
   numBuffers: 0,
   bufferMask: 0,
   lock: PTHREAD_MUTEX_INITIALIZER,
   nexSurf: NULL
};

/*****************************************************************************/

/**
 * Returns the virtual address of the allocated memroy.
 * Also fills in the physical address in the passed in parameter.
 */
static unsigned int
allocSharedData(void **alloced)
{
   unsigned phyAddr;

   phyAddr = AllocateNexusMemory(sizeof(SHARED_DATA), 0, alloced);
   if (*alloced == NULL)
   {
      BCM_DEBUG_ERRMSG("BRCM-GRALLOC: failed to allocated Shared Data surface\n");
      return 0;
   }

   memset(*alloced, 0, sizeof(PSHARED_DATA));

   return phyAddr;
}

static void
freeSharedData(void *allocAddr)
{
    NEXUS_Memory_Free(allocAddr);
}

NEXUS_PixelFormat getNexusPixelFormat(int pixelFmt,
                                      int *bpp)
{
    NEXUS_PixelFormat pf;
    int b;
    switch (pixelFmt) {
        //case HAL_PIXEL_FORMAT_BGRA_8888:  
        case HAL_PIXEL_FORMAT_RGBA_8888:    b = 4;   pf = NEXUS_PixelFormat_eA8_B8_G8_R8;   break;
        case HAL_PIXEL_FORMAT_RGBX_8888:    b = 4;   pf = NEXUS_PixelFormat_eX8_R8_G8_B8;   break;
        case HAL_PIXEL_FORMAT_RGB_888:      b = 4;   pf = NEXUS_PixelFormat_eX8_R8_G8_B8;   break;
        case HAL_PIXEL_FORMAT_RGB_565:      b = 2;   pf = NEXUS_PixelFormat_eR5_G6_B5;      break;
#if 0
        case HAL_PIXEL_FORMAT_RGBA_5551:    b = 2;   pf = NEXUS_PixelFormat_eR5_G5_B5_A1;   break;
        case HAL_PIXEL_FORMAT_RGBA_4444:    b = 2;   pf = NEXUS_PixelFormat_eR4_G4_B4_A4;   break;
#endif
        default:
            {
                b = 0;   pf = NEXUS_PixelFormat_eUnknown;       
                LOGE("%s %d FORMAT [ %d ] NOT SUPPORTED ",__FUNCTION__,__LINE__,pixelFmt);
                break;
            }
    }

    *bpp = b;
    return pf;
}

static int gralloc_alloc_framebuffer_locked(alloc_device_t* dev,
                                            int w, int h,
                                            int format,
                                            int usage,
                                            buffer_handle_t* pHandle,
                                            int *pStride)
{
    uint32_t i=0;
    private_module_t* m = reinterpret_cast<private_module_t*>(
            dev->common.module);

    BCM_DEBUG_MSG("BRCM-GRALLOC: gralloc_alloc_framebuffer_locked called\n");
    if (m->framebuffer == NULL) 
    {
        /* 
         * If we hit this condition nothing would work for us anyway 
         * because nexus initialization did not happen. This code is 
         */
        BCM_DEBUG_ERRMSG("BRCM-GRALLOC: Mapping framebuffer device now...Calling mapFrameBuffer \n");
        return -1;
    }


    size_t size, stride;

    int align = 4;
    int bpp = 0;

    getNexusPixelFormat(format, &bpp);

    size_t bpr = (w*bpp + (align-1)) & ~(align-1);
    size = bpr * h;
    *pStride = bpr / bpp;

    NexusSurface *nexSurf = reinterpret_cast<NexusSurface *>(m->nexSurf);

    const uint32_t bufferMask = m->bufferMask;
    const uint32_t numBuffers = m->numBuffers;
    const size_t bufferSize = nexSurf->lineLen * nexSurf->Yres;

    if (bufferMask >= ((1LU<<numBuffers)-1))
    {
        // We ran out of buffers.
        LOGE("We ran out of buffers \n");
        return -ENOMEM;
    }

    // create a "fake" handles for it
    unsigned vaddr = m->framebuffer->nxSurfacePhysicalAddress;

    // find a free slot
    for (i=0 ; i<numBuffers ; i++) 
    {
        if ((bufferMask & (1LU<<i)) == 0) 
        {
            m->bufferMask |= (1LU<<i);
            break;
        }
        vaddr += bufferSize;
    }

    int fd = (int) nexSurf->surface[i].surfHnd;
    private_handle_t* hnd = new private_handle_t(fd, size,
            PRIV_FLAGS_FRAMEBUFFER);

    hnd->nxSurfacePhysicalAddress = vaddr;

/*
 * We are not using the shared data for FRAMEBUFFERS
 * But only for the off-screen surfaces. We only allocate
 * it so that the code can be same for frame-buffer or
 * off-screen surfaces.
 */
    {
        PSHARED_DATA alloced=NULL;
        unsigned int phyAddr=0;
        phyAddr = allocSharedData((void **)&alloced);
        if(alloced == NULL)
        {
            BCM_TRACK_ALLOCATIONS("%s: FAILED TO ALLOCATE SHARED DATA:0x%8x 0x%08x",
                    __FUNCTION__,
                    hnd->nxSurfacePhysicalAddress,
                    phyAddr);
        }
/**
 * Doing this just to be consistent with offscreen surfaces.
 **/
        hnd->sharedDataPhyAddr = phyAddr;

        BCM_TRACK_ALLOCATIONS("%s: Allocated Shared Data [PhyAddr :0x%08x] For Surface [PhyAddr:0x%08x]",
                                __FUNCTION__,
                                hnd->sharedDataPhyAddr,
                                hnd->nxSurfacePhysicalAddress);
    }

    *pHandle = hnd;
    return 0;
}

static int gralloc_alloc_framebuffer(alloc_device_t* dev,
                                     int w, int h,
                                     int format,
                                     int usage,
                                     buffer_handle_t* pHandle,
                                     int *pStride)
{
    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
    pthread_mutex_lock(&m->lock);
    BCM_DEBUG_MSG("BRCM-GRALLOC: gralloc_alloc_framebuffer called\n");    
    int err = gralloc_alloc_framebuffer_locked(dev, w, h, format, usage, pHandle, pStride);
    pthread_mutex_unlock(&m->lock);

    BCM_DEBUG_MSG("BRCM-GRALLOC: gralloc_alloc_framebuffer return\n");    
    return err;
}

static
unsigned int allocGLSuitableBuffer(private_handle_t * allocContext,
                      int width, 
                      int height,
                                   int format,
                                   void **alloced)
{
    BEGL_PixmapInfo bufferRequirements;
    BEGL_BufferSettings bufferConstrainedRequirements;
    unsigned int phyAddr = 0;

    if (!allocContext) {
        BCM_DEBUG_ERRMSG("%s : invalid handle", __FUNCTION__);
        return -EINVAL;
    }

    bufferRequirements.width = width;
    bufferRequirements.height = height;

    // WARNING! GL only has three renderable surfaces, premote 888 to X888
    switch (format)
    {
    case HAL_PIXEL_FORMAT_RGBA_8888:  bufferRequirements.format = BEGL_BufferFormat_eA8B8G8R8_TFormat;     break;
    case HAL_PIXEL_FORMAT_RGBX_8888:  bufferRequirements.format = BEGL_BufferFormat_eX8B8G8R8_TFormat;     break;
    case HAL_PIXEL_FORMAT_RGB_888:    bufferRequirements.format = BEGL_BufferFormat_eX8B8G8R8_TFormat;     break;
    case HAL_PIXEL_FORMAT_RGB_565:    bufferRequirements.format = BEGL_BufferFormat_eR5G6B5_TFormat;       break;
#if 0
    case HAL_PIXEL_FORMAT_RGBA_5551:  bufferRequirements.format = BEGL_BufferFormat_eR5G5B5A1_TFormat;     break;
    case HAL_PIXEL_FORMAT_RGBA_4444:  bufferRequirements.format = BEGL_BufferFormat_eR4G4B4A4_TFormat;     break;
#endif
    default:                          bufferRequirements.format = BEGL_BufferFormat_INVALID;               break;
    }

    //
    // this surface may not necessarily be used by GL,
    // so it can still succeed, it will however fail at eglCreateImageKHR()
    //
    if (bufferRequirements.format != BEGL_BufferFormat_INVALID) {
        // We need to call the V3D driver buffer get requirements function in order to
        // get the parameters of the shadow buffer that we are creating.
        BEGLint_BufferGetRequirements(&bufferRequirements, &bufferConstrainedRequirements);
        phyAddr = AllocateNexusMemory(bufferConstrainedRequirements.totalByteSize,
                                        bufferConstrainedRequirements.alignment,
                                        alloced);

        allocContext->oglStride = bufferConstrainedRequirements.pitchBytes;
        allocContext->oglFormat = bufferConstrainedRequirements.format;
        allocContext->oglSize   = bufferConstrainedRequirements.totalByteSize;
    }
    else {
        BCM_DEBUG_MSG("=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+");
        BCM_DEBUG_MSG("             BRCM-GRALLOC: [OGL-INTEGRATION] ASSIGNING OGL PARAMS=0");
        BCM_DEBUG_MSG("=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+");
        allocContext->oglStride = 0;
        allocContext->oglFormat = 0;
        allocContext->oglSize   = 0;
    }

    return phyAddr;
}

static int 
gralloc_alloc_buffer(alloc_device_t* dev,
                    int w, int h,
                    int format,
                    int usage,
                    buffer_handle_t* pHandle,
                    int *pStride)
{
    int bpp=0,fd=-1;
    char * basePtr=NULL;
    private_handle_t *grallocPrivateHandle = NULL;
    unsigned sharedDataPhyAddr=0;
    PSHARED_DATA sharedData = NULL;
    void * allocedSurface = NULL;

    NEXUS_PixelFormat nxFormat = getNexusPixelFormat(format,&bpp);
    size_t size, stride;

    // test 16byte alignment for TLB conversion in HW
    int align = 16;
    size_t bpr = (w*bpp + (align-1)) & ~(align-1);
    size = bpr * h;
    *pStride = bpr / bpp;

    if (nxFormat == NEXUS_PixelFormat_eUnknown) {
        LOGE("%s %d : UnSupported Format In Gralloc",__FUNCTION__,__LINE__);
        return -EINVAL;
    }

    fd = open("/dev/zero",O_RDWR,0);
    if ((fd == -1) || (!fd)) {
        LOGE("Open to /dev/zero failed 0x%x\n",fd);
        return -EINVAL;
    }

    sharedDataPhyAddr = allocSharedData((void **) &sharedData);
    if (sharedData == NULL) {
        LOGE("%s: Failed to allocate shared data for Surface:0x%08x",
             __FUNCTION__, grallocPrivateHandle->nxSurfacePhysicalAddress);
        *pHandle = NULL;
        close(fd);
        return -ENOMEM;
    }

    grallocPrivateHandle = new private_handle_t(fd, size, 0);
    grallocPrivateHandle->width = w;
    grallocPrivateHandle->height = h;
    grallocPrivateHandle->bpp = bpp;
    grallocPrivateHandle->format = format;
    grallocPrivateHandle->sharedDataPhyAddr = sharedDataPhyAddr;
    grallocPrivateHandle->nxSurfacePhysicalAddress =
        allocGLSuitableBuffer(grallocPrivateHandle, w, h, format, &allocedSurface);

    BCM_TRACK_ALLOCATIONS("%s: Alloc Called for handle:%p Surface:0x%08x SharedData:0x%08x Size:%dx%d Usage:0x%08x",
                            __FUNCTION__,
                            grallocPrivateHandle,
                            grallocPrivateHandle->nxSurfacePhysicalAddress,
                            grallocPrivateHandle->sharedDataPhyAddr,
                            grallocPrivateHandle->width,
                            grallocPrivateHandle->height,
                            grallocPrivateHandle->flags);

    LOGV("%s: %dx%d bpp=%d format=%d, usage=0x%08x, phys addr=0x%08x", __FUNCTION__,
            w, h, bpp, format, usage,
            grallocPrivateHandle->nxSurfacePhysicalAddress);

    if (allocedSurface == NULL) {
        //Surface Allocation Failed...clean and reutrn
        LOGE("%s: Failed to allocate the surface...",__FUNCTION__);
        freeSharedData(sharedData);
        *pHandle = NULL;
        delete grallocPrivateHandle;
        close(fd);
        return -EINVAL;
    }

    *pHandle = grallocPrivateHandle;
    return 0;
}

// function is implemented in mapper.c.  It would ordinarily called on register/unregister
// Chrome, however, uses it for it's internal composition.
void destroyHwBacking(private_handle_t* hnd);

static int
grallocFreeHandle(private_handle_t	*handleToFree)
{
    PSHARED_DATA pSharedData=NULL;

    if (!handleToFree) {
        LOGE("%s:!!!! Invalid Arguments, Cannot Process Free!!", __FUNCTION__);
        return -1;
    }

    BCM_TRACK_ALLOCATIONS("%s: Free Called for handle:%p Surface:0x%08x SharedData:0x%08x Size:%dx%d Usage:0x%08x",
                            __FUNCTION__,
                            handleToFree,
                            handleToFree->nxSurfacePhysicalAddress,
                            handleToFree->sharedDataPhyAddr,
                            handleToFree->width,
                            handleToFree->height,
                            handleToFree->flags);

    pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(handleToFree->sharedDataPhyAddr);
    if (!pSharedData) {
        LOGE("%s :!!!! SHARED MEMORY NOT AVAILABLE...FATAL ERROR!!...NOT PROCESSING FREE",
            __FUNCTION__);
        return -1;
    }

    if (handleToFree->lockTmp)
        destroyHwBacking(handleToFree);

    if (handleToFree->nxSurfacePhysicalAddress) {
        BCM_TRACK_ALLOCATIONS("%s: Freeing NxSurface [0x%08x]",
                __FUNCTION__,
                handleToFree->nxSurfacePhysicalAddress);

#if 1
        /* DEBUG code to enable fill before free.  Check for use after free in pipeline */
        int x, y;
        void *p = NEXUS_OffsetToCachedAddr(handleToFree->nxSurfacePhysicalAddress);
        if (handleToFree->bpp == 4) {
            for (y = 0; y < handleToFree->height; y++) {
                for (x = 0; x < handleToFree->width; x++) {
                    ((uint32_t *)p)[y * handleToFree->width + x] = 0xFF0000FF;
                }
            }
        }
        else {
            for (y = 0; y < handleToFree->height; y++) {
                for (x = 0; x < handleToFree->width; x++) {
                    ((uint16_t *)p)[y * handleToFree->width + x] = 0xF800;
                }
            }
        }
#endif

        FreeNexusMemory(handleToFree->nxSurfacePhysicalAddress);
    }

    if (handleToFree->sharedDataPhyAddr) {
        BCM_TRACK_ALLOCATIONS("%s: Freeing SharedData [0x%08x]",
                __FUNCTION__,
                handleToFree->sharedDataPhyAddr);

        freeSharedData(NEXUS_OffsetToCachedAddr(handleToFree->sharedDataPhyAddr));
    }

    close(handleToFree->fd);
    delete handleToFree;
    return 0;
}

static int
gralloc_free_buffer(alloc_device_t* dev, private_handle_t *hnd)
{
   private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);

   if (!hnd->sharedDataPhyAddr)
   {
      LOGE("%s:!!!! SHARED MEMORY NOT AVAILABLE...FATAL ERROR!!...NOT PROCESSING FREE",
            __FUNCTION__);
      return -1;
   }

   grallocFreeHandle(hnd);

   return 0;
}


/*****************************************************************************/

static int gralloc_alloc(alloc_device_t* dev,
        int w, int h, int format, int usage,
        buffer_handle_t* pHandle, int* pStride)
{
    if (!pHandle || !pStride) {
        LOGE("%s : condition check failed", __FUNCTION__);
            return -EINVAL;
    }

    int err;
    if (usage & GRALLOC_USAGE_HW_FB)
        err = gralloc_alloc_framebuffer(dev, w, h, format, usage, pHandle, pStride);
    else
        err = gralloc_alloc_buffer(dev, w, h, format, usage, pHandle, pStride);

    if (err < 0) {
        LOGE("%s : w=%d, h=%d, format=%d, usage=0x%08x", __FUNCTION__, w, h, format, usage);
        LOGE("%s : alloc returning error", __FUNCTION__);
    }

    return err;
}

static int gralloc_free(alloc_device_t* dev,
        buffer_handle_t handle)
{
    if (private_handle_t::validate(handle) < 0)
    {
        LOGE("gralloc_free Handle Validation Failed \n");
       return -EINVAL;
    }
    
    private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(handle);

    if (hnd->flags & PRIV_FLAGS_FRAMEBUFFER) {
        // free this buffer
        private_module_t* m = reinterpret_cast<private_module_t*>(
                dev->common.module);
        NexusSurface *nexSurf = reinterpret_cast<NexusSurface *>(m->nexSurf);
        const size_t bufferSize = nexSurf->lineLen * nexSurf->Yres;
        int index = (hnd->nxSurfacePhysicalAddress - m->framebuffer->nxSurfacePhysicalAddress) / bufferSize;
        m->bufferMask &= ~(1<<index);
        delete hnd;
    } else {
        //
        // Even if the refCnt is not decremented to 0 we will need
        // to close the FD and delete the private_handle_t. These
        // two are not going to be used by this process anymore nad 
        // this process has freed the surface. NexusMemory and surface
        // will be freed in the context where the refernece count is 
        // going to zero.
        //
        gralloc_free_buffer(dev,const_cast<private_handle_t*>(hnd));
    }

    return 0;
}

/*****************************************************************************/

static int gralloc_close(struct hw_device_t *dev)
{
    LOGD("%s [%d]: Vector Graphics Allocator [JB] Build Date[%s Time:%s]\n",
                __FUNCTION__,__LINE__,
                __DATE__,
                __TIME__);

    gralloc_context_t* ctx = reinterpret_cast<gralloc_context_t*>(dev);
    if (ctx) 
    {
        free(ctx);
    }
    return 0;
}

int gralloc_device_open(const hw_module_t* module, const char* name,
        hw_device_t** device)
{
    NEXUS_Error     rc;
    NEXUS_PlatformStatus        pStatus;
    int status = -EINVAL;


    LOGD("%s[%d]: Vector Graphics Allocator [JB] Build Date[%s Time:%s]\n",
                __FUNCTION__,__LINE__,
                __DATE__,
                __TIME__);
    
    LOGD("%s: Graphics Allocator IN CLIENT-SERVER MODE",__FUNCTION__);

#if 0
    rc = NEXUS_Platform_GetStatus(&pStatus);
    if (rc!=NEXUS_SUCCESS)
    {
        do {
            rc = NEXUS_Platform_Join();
            if(rc!=NEXUS_SUCCESS)
            {
                BCM_DEBUG_MSG("NEXUS_Platform_Join Failed\n");
                sleep(1);
            }else{
                BCM_DEBUG_MSG("NEXUS_Platform_Join Success\n");  
            }
        } while (rc!=NEXUS_SUCCESS);
    }else{
        LOGI("==========================================================================\n");
        LOGI("                     OPEN CALLED WHEN ALREADY JOINED                ......\n");
        LOGI("==========================================================================\n");
    }
#endif

    if (!strcmp(name, GRALLOC_HARDWARE_GPU0)) 
    {
        LOGI("Using Hardware GPU type device\n");
        void *alloced=NULL;
        gralloc_context_t *dev;
        dev = (gralloc_context_t*)malloc(sizeof(*dev));
        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = 0;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = gralloc_close;

        dev->device.alloc   = gralloc_alloc;
        dev->device.free    = gralloc_free;

        private_module_t* m = reinterpret_cast<private_module_t*>(dev->device.common.module);

        *device = &dev->device.common;
        status = 0;
    } else {
/*
 * The Framebuffer device is opened only once when the system starts up.
 * We will be able init the nexus surface class here.
 */
        status = fb_device_open(module, name, device);
    }

    return status;
}


