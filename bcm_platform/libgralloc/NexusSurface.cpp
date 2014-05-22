/******************************************************************************
 *    (c)2010-2013 Broadcom Corporation
 * 
 * This program is the proprietary software of Broadcom Corporation and/or its licensors,
 * and may only be used, duplicated, modified or distributed pursuant to the terms and
 * conditions of a separate, written license agreement executed between you and Broadcom
 * (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 * no license (express or implied), right to use, or waiver of any kind with respect to the
 * Software, and Broadcom expressly reserves all rights in and to the Software and all
 * intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 * HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 * NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.  
 *  
 * Except as expressly set forth in the Authorized License,
 *  
 * 1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 * secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 * and to use this information only in connection with your use of Broadcom integrated circuit products.
 *  
 * 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS" 
 * AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR 
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO 
 * THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES 
 * OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, 
 * LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION 
 * OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF 
 * USE OR PERFORMANCE OF THE SOFTWARE.
 * 
 * 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS 
 * LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR 
 * EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR 
 * USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT 
 * ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE 
 * LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF 
 * ANY LIMITED REMEDY.
 *
 *****************************************************************************/
#define LOG_TAG "BcmGralloc"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "NexusSurface.h"
#include "nexus_base_mmap.h"
#include "nexus_memory.h"
#include "nexus_video_window.h"
#include <cutils/log.h>
#include "cutils/properties.h"
#include "nexus_ipc_client_factory.h"

#if ANDROID_USES_TRELLIS_WM
#include "Broker.h"
#include "StandaloneApplication.h"
#include "AndroidApplicationFactory.h"
#include "AndroidWindowManager.h"
#include "IAndroidBAMEventListener.h"
using namespace Trellis;
using namespace Trellis::Application;
#endif 

extern "C" void *v3d_get_nexus_client_context(void);

NexusSurface::NexusSurface()
{
    ref_cnt = 0;
    lineLen = 0;
    numOfSurf = 0;
    Xres = 0;
    Yres = 0;
    Xdpi = 0;
    Ydpi = 0;
    fps = 0;
    bpp=0;
    currentSurf = 0;
    lastSurf = 0;
    blit_client = NULL;
    inited=false;
    frameBuffHeapHandle = NULL;
    gfxHeapHandle = NULL;
}

NEXUS_SurfaceClientHandle       g_blit_client;

static NexusIPCClientBase      *gIpcClient = NexusIPCClientFactory::getClient("Android-Gralloc");

#if ANDROID_USES_TRELLIS_WM
extern "C" void initOrb(IApplicationContainer* app);

class bamLiteObjects : public StandaloneApplication
{
public:
    bamLiteObjects(IAndroidBAMEventListener *androidBAMEventListener):StandaloneApplication(0,NULL)
    {
        StandaloneApplication::init();
        Util::Param& p = getParameters();
        p.add("server", "AndroidApplicationDelegate0");
        Broker::registerServerLib<IWindowManager>();
        applicationFactory = new AndroidApplicationFactory();
        windowManager = new AndroidWindowManager(0, applicationFactory, 0/*argc*/, NULL/*argv*/, androidBAMEventListener);
        initOrb(this);
        windowManager->init();
    }

    ~bamLiteObjects()
    {
        delete windowManager;
        delete applicationFactory;
    }

private:
    AndroidApplicationFactory *applicationFactory;
    AndroidWindowManager *windowManager;
};

static bamLiteObjects *androidBamLite = NULL;
#endif /* ANDROID_USES_TRELLIS_WM */

static BKNI_EventHandle flipDone;
/* 2nd Event which is used in hwcomposer as a vsync */
static BKNI_EventHandle hwcomposer_flipDone;

void hwcomposer_wait_vsync(void)
{
    /* In terms of a producer, this needs to be 16ms.   Android stops making frame
       updates when no activity happens, but this heartbeat needs to still be alive.
       This should be either 1) real callback from NSC (the vsync as near as can be) or
       2) a 16ms timer */
    BKNI_WaitForEvent(hwcomposer_flipDone, 16);
}

static void bcm_flip_done(void *context, int param)
{
    NEXUS_SurfaceHandle surface;
    size_t num;
    NEXUS_SurfaceClient_RecycleSurface(g_blit_client, &surface, 1, &num);
    BKNI_SetEvent((BKNI_EventHandle)context);

    /* Trigger vsync in libhwcomposer */
    BKNI_SetEvent((BKNI_EventHandle)param);
}

static void bcm_generic_done(void *context, int param)
{
   /* Simply trigger the event */
   BSTD_UNUSED(param);
   BKNI_SetEvent((BKNI_EventHandle)context);
}

static void
checkNexusMemStatus(size_t size,
                    NEXUS_HeapHandle heap,
                    const char *caller)
{
    NEXUS_MemoryStatus    memStatus;
    NEXUS_Heap_GetStatus(heap, &memStatus);
    const int oneK = 1024;
    const int oneMeg = oneK*oneK;
    int requestedSize = size/oneK;
    char unit = 'K';

    if(requestedSize>oneK) {
        requestedSize = size/oneMeg;
        unit = 'M';
    }

    if(size > memStatus.largestFreeBlock)
    {
        while(1)
        {
            BCM_DEBUG_ERRMSG("MemStatus: Caller: %s FATAL ERROR !! Request:%d%c LBA:%dM Used: %dM",
                             caller,
                             requestedSize,unit,
                             memStatus.largestFreeBlock/oneMeg,
                             (memStatus.size-memStatus.free)/oneMeg);
            usleep(500000);
        }
    }

    BCM_DEBUG_ERRMSG("MemStatus: Caller:%s Request:%d%c LBA:%dM Used: %dM",
                     caller,
                     requestedSize,unit,
                     memStatus.largestFreeBlock/oneMeg,
                     (memStatus.size-memStatus.free)/oneMeg);
}

void
NexusSurface::initHeapHandles()
{
    /*Currently we are only supporting one display.*/
    frameBuffHeapHandle = getNexusHeap();
    gfxHeapHandle = getNexusHeap();

    if(frameBuffHeapHandle  == NULL || gfxHeapHandle == NULL)
        BCM_DEBUG_ERRMSG("Invalid Heap Handles Recieved!!");

    LOGD("NexusSurface[3]:: GOT BOTH HEAP HANDLES AS FrameBuffHeap=%p gfxHeap=%p\n",
        frameBuffHeapHandle,
        gfxHeapHandle);

    return;
}

void NexusSurface::init(void)
{
    NEXUS_SurfaceCreateSettings     createSettings;
    NEXUS_GraphicsSettings          graphicsSettings;
    NEXUS_VideoFormatInfo           videoFormatInfo;
    NEXUS_Error                     rc;
    NEXUS_PlatformSettings          platformSettings;
    unsigned int                    surface_pitch;
    unsigned int                    surface_mem_size;
    void                           *framebuffer_mem;
    NEXUS_MemoryAllocationSettings  allocSettings;
    NEXUS_Graphics2DOpenSettings    openGraphicsSettings;
    NEXUS_Graphics2DSettings        gfxSettings;
    NEXUS_ClientConfiguration       clientConfig;
    NEXUS_PlatformConfiguration     platformConfig;

    int err =0;
    int i =0;
    int ret;
    FILE *hd;
    int enable_offset,xoff,yoff,width,height;
    char value[PROPERTY_VALUE_MAX];
    uint32_t fmt_width, fmt_height;
    NEXUS_SurfaceClientSettings client_settings;
    unsigned client_id = 0;    
    NexusClientContext *nexus_client = NULL; 
    nexus_client = (reinterpret_cast<NexusClientContext *>(v3d_get_nexus_client_context()));
    if(nexus_client)
        LOGI("%s:%d recvd client_handle = %p",__FUNCTION__,__LINE__, (void *)nexus_client);
    else
        while(1) LOGE("%s:%d got NULL client handle",__FUNCTION__,__LINE__);

    gIpcClient->addGraphicsWindow(nexus_client);

    BCM_DEBUG_MSG("NexusSurface::::: process's UID=%d AND GID=%d ProcessID:%x \n", getuid(), getgid(),getpid());
    BCM_DEBUG_ERRMSG("NexusSurface Constructor called [INTEGRATED-VERSION]\n");

    initHeapHandles();
    NEXUS_Platform_GetConfiguration(&platformConfig);

    /* Init Surface */
    NEXUS_Surface_GetDefaultCreateSettings(&createSettings);

    createSettings.pixelFormat = NEXUS_PixelFormat_eA8_B8_G8_R8;

    createSettings.heap = getFrameBufferHeap();

    NEXUS_SurfaceComposition composition;

    gIpcClient->getClientComposition(nexus_client, &composition);

    xoff = composition.position.x;
    yoff = composition.position.y;
    width = composition.position.width;
    height = composition.position.height;
    createSettings.width = width;
    createSettings.height = height;

    /* allocate 2x frame buffer memory, for preventing memory gap between 2 frame buffers */
    surface_pitch = createSettings.width * NEXUS_GRALLOC_BYTES_PER_PIXEL;
    surface_mem_size = createSettings.height * surface_pitch; /* size for single frame buffer */
    NEXUS_Memory_GetDefaultAllocationSettings(&allocSettings);
    allocSettings.alignment = 1 << createSettings.alignment;
    allocSettings.heap = getFrameBufferHeap();

    checkNexusMemStatus(surface_mem_size*2,allocSettings.heap,__FUNCTION__);
    NEXUS_Memory_Allocate(surface_mem_size*2, &allocSettings, &framebuffer_mem );
    if(framebuffer_mem == NULL) {
        BCM_DEBUG_ERRMSG("NEXUSSURFACE.CPP: Nexus Framebuffer memory allocation failed\n");
    }
    surface[0].surfBuf.buffer = framebuffer_mem;
    surface[1].surfBuf.buffer = (void *)((unsigned int)framebuffer_mem + surface_mem_size);
    LOGI("Framebuffers created at [0]%p and [1]%p surface_mem_size=%x",surface[0].surfBuf.buffer,surface[1].surfBuf.buffer,surface_mem_size);

    for(i=0;i<2;i++)
    {
        createSettings.pMemory = surface[i].surfBuf.buffer;
        surface[i].surfHnd = NEXUS_Surface_Create(&createSettings);
        if(surface[i].surfHnd == NULL)
        {
            BCM_DEBUG_ERRMSG("NEXUSSURFACE.CPP: Nexus Framebuffer surface creation failed\n");
        }
        NEXUS_Surface_GetMemory(surface[i].surfHnd, &surface[i].surfBuf);
        BCM_DEBUG_MSG("NEXUSSURFACE.CPP: == On Init Nexus Surface [WithHandle: %p] Creation Successful ==",surface[i].surfHnd);
    }

    BKNI_CreateEvent(&flipDone);
    BKNI_CreateEvent(&hwcomposer_flipDone);
    b_refsw_client_client_info client_info;
    gIpcClient->getClientInfo(nexus_client, &client_info);
    client_id = client_info.surfaceClientId;
    LOGD("%s:%d NSC client ID returned by the server dynamically is %d",__FUNCTION__,__LINE__,client_id);

#if ANDROID_USES_TRELLIS_WM
    androidBamLite = new bamLiteObjects(gIpcClient);
#endif

    blit_client = NEXUS_SurfaceClient_Acquire(client_id);
    if (!blit_client) 
    {
        while(1) LOGE("%s:%d NEXUS_SurfaceClient_Acquire %d failed",__FUNCTION__,__LINE__, client_id);
        goto err_graphics;
    }

    g_blit_client = blit_client;

    NEXUS_SurfaceClient_GetSettings(blit_client, &client_settings);
    client_settings.recycled.callback = bcm_flip_done;
    client_settings.recycled.context = (void *)flipDone;
    client_settings.recycled.param = (int)hwcomposer_flipDone;
    rc = NEXUS_SurfaceClient_SetSettings(blit_client, &client_settings);
    if (rc) {
        while(1) LOGE("%s:%d NEXUS_SurfaceClient_SetSettings %d failed",__FUNCTION__,__LINE__, client_id);
        goto err_graphics;
    }

/* Init Graphics to quickly fill the two framebuffers */
#ifndef UINT32_C
#define UINT32_C (unsigned int)
#endif

    NEXUS_Graphics2D_GetDefaultOpenSettings(&openGraphicsSettings);
    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    openGraphicsSettings.heap = clientConfig.heap[1];

    gfxHandle = NEXUS_Graphics2D_Open(NEXUS_ANY_ID, &openGraphicsSettings);

    if(!gfxHandle) {goto err_graphics;}

    BKNI_CreateEvent(&checkpointEvent);
    BKNI_CreateEvent(&packetSpaceAvailableEvent);

    /* setup the callbacks to make the NEXUS_Graphics2D_Checkpoint actually do something */
    NEXUS_Graphics2D_GetSettings(gfxHandle, &gfxSettings);
    gfxSettings.checkpointCallback.callback = bcm_generic_done;
    gfxSettings.checkpointCallback.context = checkpointEvent;
    gfxSettings.packetSpaceAvailable.callback = bcm_generic_done;
    gfxSettings.packetSpaceAvailable.context = packetSpaceAvailableEvent;
    NEXUS_Graphics2D_SetSettings(gfxHandle, &gfxSettings);

    while (1)
    {
        /* NEXUS_Graphics2D_Memset32 count is in words, not in bytes) */
        rc = NEXUS_Graphics2D_Memset32(gfxHandle, framebuffer_mem, 0, ((surface_mem_size * 2) / sizeof(uint32_t)));
        if (rc == NEXUS_GRAPHICS2D_QUEUE_FULL)
            BKNI_WaitForEvent(packetSpaceAvailableEvent, BKNI_INFINITE);
        else
            break;
    }

    rc = NEXUS_Graphics2D_Checkpoint(gfxHandle, NULL);
    if (rc == NEXUS_GRAPHICS2D_QUEUED)
        BKNI_WaitForEvent(checkpointEvent, BKNI_INFINITE);

    NEXUS_SurfaceClient_PushSurface(blit_client,surface[1].surfHnd, NULL, false);

    currentSurf = 0;
    lineLen = surface[0].surfBuf.pitch;
    numOfSurf = 2;
    Xres = createSettings.width;
    Yres = createSettings.height;
    Xdpi = 160;
    Ydpi = 160;
    fps = 60;
    bpp = NEXUS_GRALLOC_BYTES_PER_PIXEL*8;
    inited= true;

done:
    ref_cnt++;
    return;

err_graphics: //fall-thru
    while(1) {
        LOGE("ABOUT TO EXIT PROCESS WHICH SHOULD NOT HAPPEN.");
        sleep(1);
    }
    _exit(1);
}

void NexusSurface::flip(void)
{
    NEXUS_SurfaceClient_PushSurface(blit_client, surface[currentSurf].surfHnd,NULL,false);
    currentSurf = currentSurf ^ 0x1;
    BKNI_WaitForEvent(flipDone, BKNI_INFINITE);
}

void 
printHandleInfo(private_handle_t *privHnd)
{
    BCM_TRACK_ALLOCATIONS("\n ====== HANDLE INFORMATION ========== \n Privatehandle:%p Flags: %x Sz:%d AllocPid:%d",
        privHnd,privHnd->flags,privHnd->size,privHnd->pid);

#if 0
    if(privHnd->sharedDataPhyAddr)
    {
        PSHARED_DATA pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(privHnd->sharedDataPhyAddr);

        BCM_TRACK_ALLOCATIONS("refCnt:%d w:%d h:%d fmt:%d bpp:%d SharedDataPhyAddr=%p nxSurfPhyAddr:%p LockedCnt:%d",pSharedData->refCnt,
            pSharedData->width,pSharedData->height,pSharedData->format,
            pSharedData->bpp,privHnd->sharedDataPhyAddr, pSharedData->nxSurfacePhysicalAddress,pSharedData->lockedCnt);

        if(pSharedData->regBuffPidCnt)
        {
            BCM_TRACK_ALLOCATIONS("Buffer Registered With %d Processes With Pids: ",pSharedData->regBuffPidCnt);
            for(unsigned i=0; i < pSharedData->regBuffPidCnt; i++) BCM_TRACK_ALLOCATIONS(" %d ",pSharedData->regBuffPids[i]);
        }
    }
#endif
}

NEXUS_HeapHandle getNexusHeap(void)
{
#if ((ANDROID_CLIENT_SECURITY_MODE == 0) || (ANDROID_CLIENT_SECURITY_MODE==1)) 
    NEXUS_HeapHandle heap = NULL;
    NEXUS_MemoryStatus status;
    heap = NEXUS_Platform_GetFramebufferHeap(0);
    int rc = NEXUS_Heap_GetStatus(heap, &status);
    BDBG_ASSERT(!rc);
    BCM_DEBUG_MSG("%s[%d]: client heap:%p MEMC-index %d, offset %#x, addr %p, size %d (%#x), alignment %d\n",
        __FUNCTION__,__LINE__, heap, status.memcIndex, status.offset, status.addr,
        status.size, status.size, status.alignment);

    return heap;
#else
    NEXUS_ClientConfiguration clientConfig;
    NEXUS_HeapHandle heap = NULL;

    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    /* Return the first heap - TODO: do we have to return a specific heap? */
    for (int i=0;i<NEXUS_MAX_HEAPS;i++) 
    {
        NEXUS_MemoryStatus status;
        heap = clientConfig.heap[i];
        if (heap)
        {
            int rc = NEXUS_Heap_GetStatus(heap, &status);
            BDBG_ASSERT(!rc);
            BCM_DEBUG_MSG("%s[%d]: client heap:%p [%d]: MEMC %d, offset %#x, addr %p, size %d (%#x), alignment %d\n",
                __FUNCTION__,__LINE__, heap, i, status.memcIndex, status.offset, status.addr,
                status.size, status.size, status.alignment);
            break;
        }
    }
    return heap;
#endif

}

unsigned int
AllocateNexusMemory(size_t     size,        // Size of the memory that you want to allocate
                    size_t     alignment,   // set to 0 for using default alignment
                    void       **alloced)
{
    int rc=0;
    NEXUS_MemoryAllocationSettings settings;
    NEXUS_Memory_GetDefaultAllocationSettings(&settings);

    settings.alignment = alignment;
    settings.heap = getNexusHeap();

    rc = NEXUS_Memory_Allocate(size,&settings,alloced);
    BDBG_ASSERT(!rc);
    if(*alloced == NULL)
    {
        BCM_DEBUG_ERRMSG("%s[%d]: Nexus Memory Allocation Failed [Sz: %d  Alignment:%d]",
                __FUNCTION__,__LINE__,
                size, alignment);

        BDBG_ASSERT(0);
        return 0;
    }

    memset(*alloced,0,size);
    return NEXUS_AddrToOffset(*alloced);
}

void FreeNexusMemory(void * pMemory)
{
    NEXUS_Memory_Free(pMemory);
}

void FreeNexusMemory(unsigned int phyAddr)
{
    NEXUS_Memory_Free(NEXUS_OffsetToCachedAddr(phyAddr));
}



