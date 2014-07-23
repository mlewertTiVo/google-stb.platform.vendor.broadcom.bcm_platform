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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

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

#include "nx_ashmem.h"

extern "C" void *v3d_get_nexus_client_context(void);

NexusSurface::NexusSurface()
{
   lineLen = 0;
   numOfSurf = 0;
   Xres = 0;
   Yres = 0;
   Xdpi = 0;
   Ydpi = 0;
   fps = 0;
   bpp =0;
   currentSurf = 0;
   lastSurf = 0;
   blitClient = NULL;
}

NEXUS_SurfaceClientHandle       gBlitClient;

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
#endif // ANDROID_USES_TRELLIS_WM

static BKNI_EventHandle flipDone;
// 2nd Event which is used in hwcomposer as a vsync
static BKNI_EventHandle hwcomposer_flipDone;

void hwcomposer_wait_vsync(void)
{
   // In terms of a producer, this needs to be 16ms.   Android stops making frame
   // updates when no activity happens, but this heartbeat needs to still be alive.
   // This should be either 1) real callback from NSC (the vsync as near as can be) or
   // 2) a 16ms timer
   BKNI_WaitForEvent(hwcomposer_flipDone, 16);
}

static void bcm_flip_done(void *context, int param)
{
   NEXUS_SurfaceHandle surface;
   size_t num;
   NEXUS_SurfaceClient_RecycleSurface(gBlitClient, &surface, 1, &num);
   BKNI_SetEvent((BKNI_EventHandle)context);

   // Trigger vsync in libhwcomposer
   BKNI_SetEvent((BKNI_EventHandle)param);
}

void NexusSurface::init(void)
{
   NEXUS_SurfaceCreateSettings     createSettings;
   NEXUS_GraphicsSettings          graphicsSettings;
   NEXUS_VideoFormatInfo           videoFormatInfo;
   NEXUS_Error                     rc;
   NEXUS_PlatformSettings          platformSettings;
   unsigned int                    surfacePitch;
   unsigned int                    surfaceSize;
   void                           *vAddr;
   unsigned int                    phyAddr;
   NEXUS_PlatformConfiguration     platformConfig;

   int ret;
   int enable_offset,xoff,yoff,width,height;
   char value[PROPERTY_VALUE_MAX];
   uint32_t fmt_width, fmt_height;
   NEXUS_SurfaceClientSettings clientSettings;
   unsigned client_id = 0;
   NexusClientContext *nexus_client = NULL;
   nexus_client = (reinterpret_cast<NexusClientContext *>(v3d_get_nexus_client_context()));
   if (nexus_client)
      LOGI("%s:%d recvd client_handle = %p", __FUNCTION__, __LINE__, (void *)nexus_client);
   else
      while(1) LOGE("%s:%d got NULL client handle", __FUNCTION__, __LINE__);

   gIpcClient->addGraphicsWindow(nexus_client);

   BCM_DEBUG_MSG("NexusSurface::::: process's UID=%d AND GID=%d ProcessID:%x \n", getuid(), getgid(),getpid());
   BCM_DEBUG_ERRMSG("NexusSurface Constructor called [INTEGRATED-VERSION]\n");

   NEXUS_Platform_GetConfiguration(&platformConfig);

   // Init Surface
   NEXUS_Surface_GetDefaultCreateSettings(&createSettings);
   createSettings.pixelFormat = NEXUS_PixelFormat_eA8_B8_G8_R8;
   // nx_ashmem works out of this heap
   createSettings.heap = NEXUS_Platform_GetFramebufferHeap(NEXUS_OFFSCREEN_SURFACE);

   NEXUS_SurfaceComposition composition;
   gIpcClient->getClientComposition(nexus_client, &composition);

   xoff = composition.position.x;
   yoff = composition.position.y;
   width = composition.position.width;
   height = composition.position.height;
   createSettings.width = width;
   createSettings.height = height;

   // allocate 2x frame buffer memory, for preventing memory gap between 2 frame buffers
   surfacePitch = createSettings.width * NEXUS_GRALLOC_BYTES_PER_PIXEL;
   surfaceSize = createSettings.height * surfacePitch; // size for single frame buffer

   fd = open("/dev/nx_ashmem", O_RDWR, 0);
   if ((fd == -1) || (!fd)) {
      LOGE("Open to /dev/nx_ashmem failed 0x%x\n", fd);
      goto err_graphics;
   }

   // nx_ashmem always aligns to 4k.  Add an additional 4k block at the start for shared memory.
   // This needs reference counting in the same way as the regular blocks
   ret = ioctl(fd, NX_ASHMEM_SET_SIZE, surfaceSize * 2);
   if (ret < 0) {
      LOGE("Setting size of buffer failed\n");
      goto err_graphics;
   };

   // This is similar to the mmap call in ashmem, but nexus doesnt work in this way, so tunnel
   // via IOCTL
   phyAddr = (NEXUS_Addr)ioctl(fd, NX_ASHMEM_GETMEM);
   if (phyAddr == 0) {
      BCM_DEBUG_ERRMSG("NEXUSSURFACE.CPP: Nexus Framebuffer memory allocation failed\n");
      goto err_graphics;
   }

   vAddr = NEXUS_OffsetToCachedAddr(phyAddr);

   surface[0].surfBuf.buffer = vAddr;
   surface[1].surfBuf.buffer = (void *)((unsigned int)vAddr + surfaceSize);
   LOGI("Framebuffers created at %p and %p surfaceSize %d",
      surface[0].surfBuf.buffer,
      surface[1].surfBuf.buffer,
      surfaceSize);

   for(int i=0; i < 2; i++)
   {
      createSettings.pMemory = surface[i].surfBuf.buffer;
      surface[i].surfHnd = NEXUS_Surface_Create(&createSettings);
      if(surface[i].surfHnd == NULL)
      {
         BCM_DEBUG_ERRMSG("NEXUSSURFACE.CPP: Nexus Framebuffer surface creation failed\n");
      }
      NEXUS_Surface_GetMemory(surface[i].surfHnd, &surface[i].surfBuf);
      BCM_DEBUG_MSG("NEXUSSURFACE.CPP: == On Init Nexus Surface [WithHandle: %p] Creation Successful ==", surface[i].surfHnd);
   }

   BKNI_CreateEvent(&flipDone);
   BKNI_CreateEvent(&hwcomposer_flipDone);
   b_refsw_client_client_info client_info;
   gIpcClient->getClientInfo(nexus_client, &client_info);
   client_id = client_info.surfaceClientId;
   LOGD("%s:%d NSC client ID returned by the server dynamically is %d", __FUNCTION__, __LINE__, client_id);

#if ANDROID_USES_TRELLIS_WM
   androidBamLite = new bamLiteObjects(gIpcClient);
#endif

   blitClient = NEXUS_SurfaceClient_Acquire(client_id);
   if (!blitClient)
   {
      while(1) LOGE("%s:%d NEXUS_SurfaceClient_Acquire %d failed", __FUNCTION__, __LINE__, client_id);
      goto err_graphics;
   }

   gBlitClient = blitClient;

   NEXUS_SurfaceClient_GetSettings(blitClient, &clientSettings);
   clientSettings.recycled.callback = bcm_flip_done;
   clientSettings.recycled.context = (void *)flipDone;
   clientSettings.recycled.param = (int)hwcomposer_flipDone;
   rc = NEXUS_SurfaceClient_SetSettings(blitClient, &clientSettings);
   if (rc) {
      while(1) LOGE("%s:%d NEXUS_SurfaceClient_SetSettings %d failed",__FUNCTION__,__LINE__, client_id);
      goto err_graphics;
   }

   NEXUS_SurfaceClient_PushSurface(blitClient,surface[1].surfHnd, NULL, false);

   currentSurf = 0;
   lineLen = surface[0].surfBuf.pitch;
   numOfSurf = 2;
   Xres = createSettings.width;
   Yres = createSettings.height;
   Xdpi = 160;
   Ydpi = 160;
   fps = 60;
   bpp = NEXUS_GRALLOC_BYTES_PER_PIXEL*8;

done:
   return;

err_graphics: // fall-thru
   while(1) {
      LOGE("ABOUT TO EXIT PROCESS WHICH SHOULD NOT HAPPEN.");
      sleep(1);
   }
   _exit(1);
}

void NexusSurface::flip(void)
{
   NEXUS_SurfaceClient_PushSurface(blitClient, surface[currentSurf].surfHnd, NULL, false);
   currentSurf = currentSurf ^ 0x1;
   BKNI_WaitForEvent(flipDone, BKNI_INFINITE);
}
