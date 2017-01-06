/*
 * Copyright 2016 The Android Open Source Project
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

#define LOG_NDEBUG 0

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <fcntl.h>
#include <math.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <utils/threads.h>
#include <cutils/sched_policy.h>
#include <inttypes.h>
/* hwc2 - including string functions. */
#define HWC2_INCLUDE_STRINGIFICATION
#define HWC2_USE_CPP11
#include <hardware/hwcomposer2.h>
#undef HWC2_INCLUDE_STRINGIFICATION
#undef HWC2_USE_CPP11
/* nexus integration. */
#include "nexus_ipc_client_factory.h"
#include "nexus_surface_client.h"
#include "nexus_core_utils.h"
#include "nxclient.h"
#include "nxclient_config.h"
#include "bfifo.h"
/* sync framework/fences. */
#include "sync/sync.h"
#include "sw_sync.h"
/* binder integration.*/
#include "HwcCommon.h"
#include "Hwc.h"
#include "HwcListener.h"
#include "IHwc.h"
#include "HwcSvc.h"
/* last but not least... */
#include "hwcutils.h"
#include "hwc2.h"

const NEXUS_BlendEquation hwc2_color_pre_multi = {
   NEXUS_BlendFactor_eSourceColor,
   NEXUS_BlendFactor_eOne,
   false,
   NEXUS_BlendFactor_eDestinationColor,
   NEXUS_BlendFactor_eInverseSourceAlpha,
   false,
   NEXUS_BlendFactor_eZero
};

const NEXUS_BlendEquation hwc2_alpha_pre_multi = {
   NEXUS_BlendFactor_eSourceAlpha,
   NEXUS_BlendFactor_eOne,
   false,
   NEXUS_BlendFactor_eDestinationAlpha,
   NEXUS_BlendFactor_eInverseSourceAlpha,
   false,
   NEXUS_BlendFactor_eZero
};

struct hwc2_bcm_device_t {
   hwc2_device_t        base;
   uint32_t             magic;
   char                 dump[HWC2_MAX_DUMP];

   char                 memif[PROPERTY_VALUE_MAX];
   int                  memfd;

   BKNI_EventHandle     recycle;  /* from nexus */
   BKNI_EventHandle     recycled; /* to waiter, after recycling */
   pthread_t            recycle_task;
   int                  recycle_exit;

   BKNI_EventHandle     vsync;
   pthread_t            vsync_task;
   int                  vsync_exit;

   pthread_mutex_t      mtx_pwr;

   Hwc2HP_sp            *hp;
   Hwc2DC_sp            *dc;
   HwcBinder_wrap       *hb;

   NexusIPCClientBase   *nxipc;
   NexusClientContext   *nxcc;

   struct hwc2_reg_cb_t regCb[HWC2_MAX_REG_CB];
   struct hwc2_dsp_t    *vd;
   struct hwc2_dsp_t    *ext;

   BKNI_EventHandle             g2dchk;
   NEXUS_Graphics2DHandle       hg2d;
   pthread_mutex_t              mtx_g2d;
   NEXUS_Graphics2DCapabilities g2dc;
};

static hwc2_dsp_t *hwc2_hnd2dsp(
   struct hwc2_bcm_device_t *hwc2,
   hwc2_display_t display,
   uint32_t *kind) {

   struct hwc2_dsp_t *dsp = NULL;

   if (hwc2->ext != NULL || hwc2->ext == (struct hwc2_dsp_t *)(intptr_t)display) {
      *kind = HWC2_DSP_EXT;
      return hwc2->ext;
   }
   if (hwc2->vd != NULL || hwc2->vd == (struct hwc2_dsp_t *)(intptr_t)display) {
      *kind = HWC2_DSP_VD;
      return hwc2->vd;
   }

   return NULL;
}

status_t Hwc2HP::onHdmiHotplugEventReceived(
   int32_t portId,
   bool connected) {
   (void) portId;

   if (cb) {
      cb(cb_data, connected);
   }
   return NO_ERROR;
}

status_t Hwc2DC::onDisplaySettingsChangedEventReceived(
   int32_t portId) {
   (void) portId;

   if (cb) {
      cb(cb_data);
   }
   return NO_ERROR;
}

void HwcBinder::notify(
   int msg,
   struct hwc_notification_info &ntfy) {

   if (cb)
      cb(cb_data, msg, ntfy);
}

static int64_t hwc2_tick(void) {
   struct timespec t;
   t.tv_sec = t.tv_nsec = 0;
   clock_gettime(CLOCK_MONOTONIC, &t);
   return (int64_t)(t.tv_sec) * 1000000000LL + t.tv_nsec;
}

static int32_t hwc2_pwrMode(
   hwc2_device_t* device,
   hwc2_display_t display,
   int32_t /*hwc2_power_mode_t*/ mode) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   uint32_t kind;

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   if (kind == HWC2_DSP_EXT) {
      if (!pthread_mutex_lock(&hwc2->mtx_pwr)) {
         ALOGI("[ext][%" PRIu64 "]: pmode %s -> %s",
               display,
               getPowerModeName(dsp->pmode),
               getPowerModeName((hwc2_power_mode_t)mode));
         dsp->pmode = (hwc2_power_mode_t)mode;
         pthread_mutex_unlock(&hwc2->mtx_pwr);
      }
   }

out:
   return ret;
}

static bool hwc2_stdby_mon(
   void *context)
{
   bool standby = false;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)context;
   hwc2_power_mode_t pmode;

   if (pthread_mutex_lock(&hwc2->mtx_pwr)) {
      goto out;
   }
   pmode = hwc2->ext->pmode; /* applies to 'ext' display only. */
   pthread_mutex_unlock(&hwc2->mtx_pwr);

   if (pmode == HWC2_POWER_MODE_ON) {
      hwc2_pwrMode((hwc2_device_t*)context,
                   (hwc2_display_t)(intptr_t)hwc2->ext,
                   HWC2_POWER_MODE_OFF);
   } else {
      standby = (pmode == HWC2_POWER_MODE_OFF);
   }

out:
   return standby;
}

static void hwc2_chkpt_cb(
   void *context,
   int param) {
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)context;
   BSTD_UNUSED(param);
   BKNI_SetEvent(hwc2->g2dchk);
}

static int hwc2_chkpt_l(
   struct hwc2_bcm_device_t *hwc2)
{
   NEXUS_Error rc;
   rc = NEXUS_Graphics2D_Checkpoint(hwc2->hg2d, NULL);
   switch (rc) {
   case NEXUS_SUCCESS:
   break;
   case NEXUS_GRAPHICS2D_QUEUED:
      rc = BKNI_WaitForEvent(hwc2->g2dchk, HWC2_CHKPT_TO);
      if (rc) {
         return -1;
      }
   break;
   default:
   return -1;
   }
   return 0;
}

static int hwc2_chkpt(
   struct hwc2_bcm_device_t *hwc2)
{
   int rc;
   if (pthread_mutex_lock(&hwc2->mtx_g2d)) {
      return -1;
   }
   rc = hwc2_chkpt_l(hwc2);
   pthread_mutex_unlock(&hwc2->mtx_g2d);
   return rc;
}

static void hwc2_recycle_cb(
   void *context,
   int param) {
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)context;
   BSTD_UNUSED(param);
   BKNI_SetEvent(hwc2->recycle);
}

static void hwc2_vsync_cb(
   void *context,
   int param) {
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)context;
   BSTD_UNUSED(param);
   BKNI_SetEvent(hwc2->vsync);
}

static void hwc2_ext_fbs(
   struct hwc2_bcm_device_t *hwc2,
   enum hwc2_cbs_e cb) {
   NEXUS_Error rc;
   NEXUS_ClientConfiguration cCli;
   NEXUS_SurfaceClientSettings sCli;
   int i;

   NEXUS_Platform_GetClientConfiguration(&cCli);

   if (hwc2->ext->u.ext.cb != cb) {
      if (hwc2->ext->u.ext.bfb) {
         hwc2->ext->u.ext.bfb = false;
         pthread_mutex_lock(&hwc2->ext->u.ext.mtx_fbs);
         for (i = 0; i < HWC2_FBS_NUM; i++) {
            if (hwc2->ext->u.ext.fbs[i].fd != HWC2_INVALID) {
               close(hwc2->ext->u.ext.fbs[i].fd);
               hwc2->ext->u.ext.fbs[i].fd = HWC2_INVALID;
            }
            if (hwc2->ext->u.ext.fbs[i].s) {
               NEXUS_Surface_Destroy(hwc2->ext->u.ext.fbs[i].s);
            }
         }
         pthread_mutex_unlock(&hwc2->ext->u.ext.mtx_fbs);
      }
      NEXUS_SurfaceClient_GetSettings(hwc2->ext->u.ext.sch, &sCli);
      sCli.recycled.callback      = hwc2_recycle_cb;
      sCli.recycled.context       = (void *)hwc2;
      sCli.vsync.callback         = hwc2_vsync_cb;
      sCli.vsync.context          = (void *)hwc2;
      sCli.allowCompositionBypass = cb;
      NEXUS_SurfaceClient_SetSettings(hwc2->ext->u.ext.sch, &sCli);
   }

   for (i = 0; i < HWC2_FBS_NUM; i++) {
      NEXUS_SurfaceCreateSettings scs;
      NEXUS_MemoryBlockHandle bh = NULL;
      bool dh = false;
      NEXUS_Surface_GetDefaultCreateSettings(&scs);
      scs.width       = hwc2->ext->cfgs->w;
      scs.height      = hwc2->ext->cfgs->h;
      scs.pitch       = scs.width * 4;
      scs.pixelFormat = NEXUS_PixelFormat_eA8_B8_G8_R8;
      if (cb) {
         scs.heap = NEXUS_Platform_GetFramebufferHeap(0);
      } else {
         scs.heap = cCli.heap[NXCLIENT_DYNAMIC_HEAP];
         dh = true;
      }
      hwc2->ext->u.ext.fbs[i].s = NULL;
      bh = hwc_block_create(&scs, hwc2->memif, dh, &hwc2->ext->u.ext.fbs[i].fd);
      if (bh != NULL) {
         scs.pixelMemory = bh;
         scs.heap = NULL;
         hwc2->ext->u.ext.fbs[i].s = hwc_surface_create(&scs, dh);
      }
      ALOGI("[ext]: fb:%d::%dx%d::%s -> %p (%d::%p)",
            i, scs.width, scs.height, dh?"d-cma":"gfx",
            hwc2->ext->u.ext.fbs[i].s, hwc2->ext->u.ext.fbs[i].fd, bh);
   }
   hwc2->ext->u.ext.bfb = true;

   pthread_mutex_lock(&hwc2->ext->u.ext.mtx_fbs);
   BFIFO_INIT(&hwc2->ext->u.ext.fb, hwc2->ext->u.ext.fbs, HWC2_FBS_NUM);
   BFIFO_WRITE_COMMIT(&hwc2->ext->u.ext.fb, HWC2_FBS_NUM);
   pthread_mutex_unlock(&hwc2->ext->u.ext.mtx_fbs);
   hwc2->ext->u.ext.cb = cb;
}

static int hwc2_ext_fb_valid_l(
   struct hwc2_bcm_device_t *hwc2,
   NEXUS_SurfaceHandle s) {

   int i;
   for (i = 0; i < HWC2_FBS_NUM; i++) {
      if (hwc2->ext->u.ext.fbs[i].s == s) {
         return hwc2->ext->u.ext.fbs[i].fd;
      }
   }
   return HWC2_INVALID;
}

static int32_t hwc2_ret_fence(
   struct hwc2_dsp_t *dsp) {

   struct sync_fence_info_data info;
   int f = HWC2_INVALID;

   if (dsp->cmp_tl == HWC2_INVALID) {
      return HWC2_INVALID;
   }

   snprintf(info.name, sizeof(info.name), "hwc2_%s_%llu", dsp->name, dsp->pres);
   f = sw_sync_fence_create(dsp->cmp_tl, info.name, dsp->pres);
   if (f < 0) {
      return HWC2_INVALID;
   }
   return f;
}

static void hwc2_ret_inc(
   struct hwc2_dsp_t *dsp,
   unsigned int inc) {

   if (dsp->cmp_tl == HWC2_INVALID) {
      return;
   }
   sw_sync_timeline_inc(dsp->cmp_tl, inc);
}

static void hwc2_recycle(
   struct hwc2_bcm_device_t *hwc2) {

   NEXUS_SurfaceHandle s[HWC2_FBS_NUM];
   NEXUS_Error rc;
   int fd;
   size_t ns = 0, i;

   NEXUS_SurfaceClient_RecycleSurface(hwc2->ext->u.ext.sch, s, HWC2_FBS_NUM, &ns);
   if (ns) {
      pthread_mutex_lock(&hwc2->ext->u.ext.mtx_fbs);
      for (i = 0; i < ns; i++) {
         fd = hwc2_ext_fb_valid_l(hwc2, s[i]);
         if (fd != HWC2_INVALID) {
            struct hwc2_fb_t *e = BFIFO_WRITE(&hwc2->ext->u.ext.fb);
            e->s  = s[i];
            e->fd = fd;
            BFIFO_WRITE_COMMIT(&hwc2->ext->u.ext.fb, 1);
         }
      }
      pthread_mutex_unlock(&hwc2->ext->u.ext.mtx_fbs);
   }

   hwc2_ret_inc(hwc2->ext, (unsigned int)ns);
}

static void *hwc2_recycle_task(
   void *argv) {
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)argv;

   hwc2->recycle_exit = 0;
   do {
      BKNI_WaitForEvent(hwc2->recycle, BKNI_INFINITE);
      hwc2_recycle(hwc2);
      BKNI_SetEvent(hwc2->recycled);
   } while(!hwc2->recycle_exit);

   return NULL;
}

static void *hwc2_vsync_task(
   void *argv) {
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)argv;

   hwc2->vsync_exit = 0;
   do {
      /* this sync event applies to the 'ext' display only. */
      if (hwc2->ext == NULL)
         continue;
      pthread_mutex_lock(&hwc2->mtx_pwr);
      if (hwc2->ext->pmode != HWC2_POWER_MODE_OFF) {
         pthread_mutex_unlock(&hwc2->mtx_pwr);
         BKNI_WaitForEvent(hwc2->vsync, BKNI_INFINITE);
         if ((hwc2->ext->u.ext.vsync == HWC2_VSYNC_ENABLE) &&
             (hwc2->regCb[HWC2_CALLBACK_VSYNC-1].func != NULL)) {
            HWC2_PFN_VSYNC f_vsync = (HWC2_PFN_VSYNC) hwc2->regCb[HWC2_CALLBACK_VSYNC-1].func;
            f_vsync(hwc2->regCb[HWC2_CALLBACK_VSYNC-1].data,
                    (hwc2_display_t)(intptr_t)hwc2->ext,
                    hwc2_tick());
         }
      } else {
         pthread_mutex_unlock(&hwc2->mtx_pwr);
         BKNI_Sleep(16); /* pas beau. */
      }
   } while(!hwc2->vsync_exit);

   return NULL;
}

static NEXUS_SurfaceHandle hwc2_ext_fb_get(
   struct hwc2_bcm_device_t *hwc2) {
   int nsc = 0;
   struct hwc2_fb_t *e = NULL;

   while (true) {
      pthread_mutex_lock(&hwc2->ext->u.ext.mtx_fbs);
      if (BFIFO_READ_PEEK(&hwc2->ext->u.ext.fb) != 0) {
         pthread_mutex_unlock(&hwc2->ext->u.ext.mtx_fbs);
         goto out_read;
      } else {
         pthread_mutex_unlock(&hwc2->ext->u.ext.mtx_fbs);
         hwc2_recycle(hwc2);
         if (BFIFO_READ_PEEK(&hwc2->ext->u.ext.fb) != 0) {
            goto out_read;
         }
      }
      if (BKNI_WaitForEvent(hwc2->recycled, HWC2_FB_WAIT_TO)) {
         nsc++;
         if (nsc == HWC2_FB_RETRY) {
            return NULL;
         }
      }
   }

out_read:
   pthread_mutex_lock(&hwc2->ext->u.ext.mtx_fbs);
   e = BFIFO_READ(&hwc2->ext->u.ext.fb);
   BFIFO_READ_COMMIT(&hwc2->ext->u.ext.fb, 1);
   pthread_mutex_unlock(&hwc2->ext->u.ext.mtx_fbs);
out:
   return (e != NULL ? e->s : NULL);
}

static hwc2_lyr_t *hwc2_hnd2lyr(
   struct hwc2_dsp_t *dsp,
   hwc2_layer_t layer) {

   struct hwc2_lyr_t *lyr = dsp->lyr;

   while (lyr != NULL) {
      if (lyr == (struct hwc2_lyr_t *)(intptr_t)layer) {
         return lyr;
      }
      lyr = lyr->next;
   }

   return NULL;
}

static void hwc2_getCaps(
   struct hwc2_device* device,
   uint32_t* outCount,
   int32_t* /*hwc2_capability_t*/ outCapabilities) {

   if (device == NULL) {
      return;
   }

   if (outCount != NULL) {
      *outCount = 1;
   }
   if (outCapabilities != NULL) {
      outCapabilities[0] = HWC2_CAPABILITY_SIDEBAND_STREAM;
   }
   return;
}

static int32_t hwc2_regCb(
   hwc2_device_t* device,
   int32_t /*hwc2_callback_descriptor_t*/ descriptor,
   hwc2_callback_data_t callbackData,
   hwc2_function_pointer_t pointer) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_PARAMETER;
      goto out;
   }
   if (descriptor < HWC2_CALLBACK_HOTPLUG || descriptor > HWC2_CALLBACK_VSYNC) {
      ret = HWC2_ERROR_BAD_PARAMETER;
      goto out;
   }

   hwc2->regCb[descriptor-1].desc = (hwc2_callback_descriptor_t)descriptor;
   hwc2->regCb[descriptor-1].func = pointer;
   hwc2->regCb[descriptor-1].data = callbackData;

   /* report initial state of the hotplug-able display. */
   if (descriptor == HWC2_CALLBACK_HOTPLUG) {
      if (hwc2->ext != NULL) {
         NEXUS_HdmiOutputHandle hdmi;
         NEXUS_HdmiOutputStatus hstatus;
         hdmi = NEXUS_HdmiOutput_Open(0+NEXUS_ALIAS_ID, NULL);
         NEXUS_HdmiOutput_GetStatus(hdmi, &hstatus);
         if (hwc2->regCb[HWC2_CALLBACK_HOTPLUG-1].func != NULL) {
            ALOGV("[ext]: report hotplug %s\n", hstatus.connected?"CONNECTED":"DISCONNECTED");
            HWC2_PFN_HOTPLUG f_hp = (HWC2_PFN_HOTPLUG) hwc2->regCb[HWC2_CALLBACK_HOTPLUG-1].func;
            f_hp(hwc2->regCb[HWC2_CALLBACK_HOTPLUG-1].data,
                 (hwc2_display_t)(intptr_t)hwc2->ext,
                 hstatus.connected?(int)HWC2_CONNECTION_CONNECTED:(int)HWC2_CONNECTION_DISCONNECTED);
         }
      }
   }

out:
   return ret;
}

static uint32_t hwc2_vdCnt(
   hwc2_device_t* device) {

   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   if (device == NULL) {
      return 0;
   }
   return HWC2_VD_MAX_NUM;
}

static int32_t hwc2_dzSup(
   hwc2_device_t* device,
   hwc2_display_t display,
   int32_t* outSupport) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_PARAMETER;
      goto out;
   }

   if (display == HWC2_DISPLAY_TYPE_PHYSICAL) {
      if (outSupport != NULL) {
         *outSupport = 0; /* TODO: support DOZE. */
      } else {
         ret = HWC2_ERROR_BAD_PARAMETER;
      }
   } else if (display == HWC2_DISPLAY_TYPE_VIRTUAL) {
      if (outSupport != NULL) {
         *outSupport = 0;
      } else {
         ret = HWC2_ERROR_BAD_PARAMETER;
      }
   } else {
      ret = HWC2_ERROR_BAD_DISPLAY;
   }

out:
   return ret;
}

static int32_t hwc2_vdAdd(
   hwc2_device_t* device,
   uint32_t width,
   uint32_t height,
   hwc2_display_t* outDisplay) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;

   ALOGV("-> %s\n",
     getFunctionDescriptorName(HWC2_FUNCTION_CREATE_VIRTUAL_DISPLAY));

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_NO_RESOURCES;
      goto out;
   }
   if (hwc2->vd != NULL) {
      ret = HWC2_ERROR_NO_RESOURCES;
      goto out;
   }
   if (outDisplay == NULL) {
      ret = HWC2_ERROR_NO_RESOURCES;
      goto out;
   }
   if (width > HWC2_VD_MAX_W || height > HWC2_VD_MAX_H) {
      ret = HWC2_ERROR_UNSUPPORTED;
      goto out;
   }

   hwc2->vd = (struct hwc2_dsp_t*)malloc(sizeof(*(hwc2->vd)));
   if (hwc2->vd == NULL) {
      ret = HWC2_ERROR_NO_RESOURCES;
      goto out;
   }
   snprintf(hwc2->vd->name, strlen(hwc2->vd->name), "stbVD0");
   hwc2->vd->type = HWC2_DISPLAY_TYPE_VIRTUAL;
   hwc2->vd->u.vd.w = width;
   hwc2->vd->u.vd.h = height;

   *outDisplay = (hwc2_display_t)(intptr_t)hwc2->vd;

out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_CREATE_VIRTUAL_DISPLAY),
      *outDisplay, getErrorName(ret));
   return ret;
}

static int32_t hwc2_vdRem(
   hwc2_device_t* device,
   hwc2_display_t display) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_DESTROY_VIRTUAL_DISPLAY),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }
   if (hwc2->vd == NULL || hwc2->vd != (hwc2_dsp_t *)(intptr_t)display) {
      ret = HWC2_ERROR_BAD_PARAMETER;
      goto out;
   }

   free(hwc2->vd);
   hwc2->vd = NULL;

out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_DESTROY_VIRTUAL_DISPLAY),
      display, getErrorName(ret));
   return ret;
}

static size_t hwc2_dump_gen(
   struct hwc2_bcm_device_t *hwc2) {

   size_t max = sizeof(hwc2->dump);
   size_t current = 0;

   memset((void *)hwc2->dump, 0, sizeof(hwc2->dump));

   current += snprintf(hwc2->dump, max-current, "hwc2-bcm\n");
   if (hwc2->vd != NULL) {
      if (max-current > 0) {
         current += snprintf(hwc2->dump, max-current, "\tvd:%" PRIu32 "x%" PRIu32 "\n", hwc2->vd->u.vd.w, hwc2->vd->u.vd.h);
      }
   }
   // TODO: add more dump data.

   return current;
}

static void hwc2_dump(
   hwc2_device_t* device,
   uint32_t* outSize,
   char* outBuffer) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      return;
   }
   if (outBuffer == NULL) {
      size_t dump = hwc2_dump_gen(hwc2);
      *outSize = dump;
   } else {
      size_t max = (*outSize < HWC2_MAX_DUMP) ? *outSize : HWC2_MAX_DUMP;
      memcpy((void *)outBuffer,
             (void *)hwc2->dump,
             max);
      *outSize = max;
   }
}

static int32_t hwc2_dspAckChg(
   hwc2_device_t* device,
   hwc2_display_t display) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   uint32_t kind;
   struct hwc2_lyr_t *lyr = NULL;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_ACCEPT_DISPLAY_CHANGES),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }
   if (!dsp->validated) {
      ret = HWC2_ERROR_NOT_VALIDATED;
      goto out;
   }

   lyr = dsp->lyr;
   while (lyr != NULL) {
      if (lyr->cCli != lyr->cDev) {
         lyr->cCli = lyr->cDev;
      }
      lyr = lyr->next;
   }

out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_ACCEPT_DISPLAY_CHANGES),
      display, getErrorName(ret));
   return ret;
}

static void hwc2_lyr_tl_set(
   struct hwc2_dsp_t *dsp,
   uint32_t kind,
   struct hwc2_lyr_t *lyr) {

   size_t num;
   int free = HWC2_INVALID;
   if (kind == HWC2_DSP_EXT) {
      for (num = 0 ; num < HWC2_MAX_TL ; num++) {
         if (free == HWC2_INVALID &&
             dsp->u.ext.rtl[num].hdl == NULL &&
             dsp->u.ext.rtl[num].tl != HWC2_INVALID) {
            free = (int)num;
         }
         if (dsp->u.ext.rtl[num].hdl == lyr) {
            return;
         }
      }

      if (free != HWC2_INVALID) {
         dsp->u.ext.rtl[free].hdl = lyr;
      }
   }
}

static void hwc2_lyr_tl_unset(
   struct hwc2_dsp_t *dsp,
   uint32_t kind,
   struct hwc2_lyr_t *lyr) {

   size_t num;
   if (kind == HWC2_DSP_EXT) {
      for (num = 0 ; num < HWC2_MAX_TL ; num++) {
         if (dsp->u.ext.rtl[num].hdl == lyr) {
            dsp->u.ext.rtl[num].hdl = NULL;
         }
      }
   }
}

static int32_t hwc2_lyr_tl_add(
   struct hwc2_dsp_t *dsp,
   uint32_t kind,
   struct hwc2_lyr_t *lyr) {

   int32_t f = HWC2_INVALID;
   struct sync_fence_info_data info;
   size_t num;
   if (kind == HWC2_DSP_EXT) {
      for (num = 0 ; num < HWC2_MAX_TL ; num++) {
         if (dsp->u.ext.rtl[num].hdl == lyr) {
            dsp->u.ext.rtl[num].ix++;
            snprintf(info.name, sizeof(info.name),
                     "hwc2_%p_ext_%llu", (void *)lyr, dsp->u.ext.rtl[num].ix);
            f = sw_sync_fence_create(dsp->u.ext.rtl[num].tl,
                                     info.name,
                                     dsp->u.ext.rtl[num].ix);
            if (f < 0) {
               f = HWC2_INVALID;
            }
            return f;
         }
      }
   }

   return HWC2_INVALID;
}

static void hwc2_lyr_tl_inc(
   struct hwc2_dsp_t *dsp,
   uint32_t kind,
   struct hwc2_lyr_t *lyr) {

   size_t num;
   if (kind == HWC2_DSP_EXT) {
      for (num = 0 ; num < HWC2_MAX_TL ; num++) {
         if (dsp->u.ext.rtl[num].hdl == lyr) {
            sw_sync_timeline_inc(dsp->u.ext.rtl[num].tl, 1);
            return;
         }
      }
   }
}

static int32_t hwc2_lyrAdd(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_layer_t* outLayer) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_lyr_t *lyr = NULL;
   struct hwc2_lyr_t *lnk = NULL, *nxt = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_CREATE_LAYER),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   lyr = (hwc2_lyr_t *)malloc(sizeof(*lyr));
   if (lyr == NULL) {
      ret = HWC2_ERROR_NO_RESOURCES;
      goto out;
   }
   memset(lyr, 0, sizeof(*lyr));

   hwc2_lyr_tl_set(dsp, kind, lyr);

   nxt = dsp->lyr;
   if (nxt == NULL) {
      dsp->lyr = lyr;
   } else {
      while (nxt != NULL) {
         lnk = nxt;
         nxt = lnk->next;
      }
      lnk = lyr;
   }
   *outLayer = (hwc2_layer_t)(intptr_t)lyr;

out:
   ALOGV("<- %s:%" PRIu64 " (%s) -> %" PRIu64 "\n",
      getFunctionDescriptorName(HWC2_FUNCTION_CREATE_LAYER),
      display, getErrorName(ret), *outLayer);
   return ret;
}

static int32_t hwc2_lyrRem(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_layer_t layer) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_lyr_t *lyr = NULL;
   struct hwc2_lyr_t *prv = NULL, *nxt = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 ":%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_DESTROY_LAYER),
     display, layer);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   nxt = dsp->lyr;
   prv = NULL;
   while (nxt != NULL) {
      if (nxt == (struct hwc2_lyr_t *)(intptr_t)layer) {
         lyr = nxt;
         break;
      } else {
         prv = nxt;
         nxt = nxt->next;
      }
   }
   if (lyr == NULL) {
      ret = HWC2_ERROR_BAD_LAYER;
      goto out;
   }

   if (prv == NULL) {
      dsp->lyr = lyr->next;
   } else {
      prv->next = lyr->next;
   }
   lyr->next = NULL;
   hwc2_lyr_tl_unset(dsp, kind, lyr);
   free(lyr);
   lyr = NULL;

out:
   ALOGV("<- %s:%" PRIu64 " (%s):%" PRIu64 "\n",
      getFunctionDescriptorName(HWC2_FUNCTION_DESTROY_LAYER),
      display, getErrorName(ret), layer);
   return ret;
}

static int32_t hwc2_gActCfg(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_config_t* outConfig) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_GET_ACTIVE_CONFIG),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   if (dsp->aCfg == NULL) {
      ret = HWC2_ERROR_BAD_CONFIG;
      *outConfig = (hwc2_config_t)0;
      return ret;
   } else {
      *outConfig = (hwc2_config_t)(intptr_t)dsp->aCfg;
   }

out:
   ALOGV("<- %s:%" PRIu64 " (%s) -> %u\n",
      getFunctionDescriptorName(HWC2_FUNCTION_GET_ACTIVE_CONFIG),
      display, getErrorName(ret), *outConfig);
   return ret;
}

static int32_t hwc2_getDevCmp(
   hwc2_device_t* device,
   hwc2_display_t display,
   uint32_t* outNumElements,
   hwc2_layer_t* outLayers,
   int32_t* /*hwc2_composition_t*/ outTypes) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_lyr_t *lyr = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_GET_CHANGED_COMPOSITION_TYPES),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }
   if (!dsp->validated) {
      ret = HWC2_ERROR_NOT_VALIDATED;
      goto out;
   }

   *outNumElements = 0;
   lyr = dsp->lyr;
   while (lyr != NULL) {
      if (lyr->cDev != lyr->cCli) {
         if (outLayers != NULL && outTypes != NULL) {
            outLayers[*outNumElements] = (hwc2_layer_t)(intptr_t)lyr;
            outTypes[*outNumElements] = lyr->cDev;
         }
         *outNumElements += 1;
      }
      lyr = lyr->next;
   }

out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_GET_CHANGED_COMPOSITION_TYPES),
      display, getErrorName(ret));
   return ret;
}

static int32_t hwc2_fmtSupp(
   int32_t format) {

   hwc2_error_t ret = HWC2_ERROR_NONE;

   switch(format) {
   case HAL_PIXEL_FORMAT_RGBA_8888:
   case HAL_PIXEL_FORMAT_RGBX_8888:
   case HAL_PIXEL_FORMAT_RGB_888:
   case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
   case HAL_PIXEL_FORMAT_RGB_565:
   case HAL_PIXEL_FORMAT_YV12:
   case HAL_PIXEL_FORMAT_YCbCr_420_888:
   break;
   default:
      ALOGE("unsupported client format: %" PRId32 "", format);
      ret = HWC2_ERROR_UNSUPPORTED;
   }
   return ret;
}

static int32_t hwc2_dspcSupp(
   int32_t dataspace) {

   hwc2_error_t ret = HWC2_ERROR_NONE;

   switch(dataspace) {
   case HAL_DATASPACE_UNKNOWN:
   case HAL_DATASPACE_V0_BT601_525:
   case HAL_DATASPACE_V0_BT709:
   break;
   default:
      ALOGE("unsupported client dataspace: %" PRId32 "", dataspace);
      ret = HWC2_ERROR_UNSUPPORTED;
   }
   return ret;
}

static int32_t hwc2_sclSupp(
   struct hwc2_bcm_device_t *hwc2,
   struct hwc2_dsp_t *dsp,
   uint32_t width,
   uint32_t height) {

   hwc2_error_t ret = HWC2_ERROR_NONE;

   if (dsp->aCfg &&
       dsp->aCfg->w &&
       width &&
       (dsp->aCfg->w / width) >= hwc2->g2dc.maxHorizontalDownScale) {
      ret = HWC2_ERROR_UNSUPPORTED;
   }

   if (dsp->aCfg &&
       dsp->aCfg->h &&
       height &&
       (dsp->aCfg->h / height) >= hwc2->g2dc.maxVerticalDownScale) {
      ret = HWC2_ERROR_UNSUPPORTED;
   }

   return ret;
}

static int32_t hwc2_ackCliTgt(
   hwc2_device_t* device,
   hwc2_display_t display,
   uint32_t width,
   uint32_t height,
   int32_t /*android_pixel_format_t*/ format,
   int32_t /*android_dataspace_t*/ dataspace) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_GET_CLIENT_TARGET_SUPPORT),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL || dsp->aCfg == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   if (dsp->aCfg->w == width &&
       dsp->aCfg->h == height &&
       format == HAL_PIXEL_FORMAT_RGBA_8888 &&
       dataspace == HAL_DATASPACE_UNKNOWN) {
      goto out;
   }

   ret = (hwc2_error_t)hwc2_fmtSupp(format);
   if (ret != HWC2_ERROR_NONE) {
      goto out;
   }
   ret = (hwc2_error_t)hwc2_dspcSupp(dataspace);
   if (ret != HWC2_ERROR_NONE) {
      goto out;
   }
   ret = (hwc2_error_t)hwc2_sclSupp(hwc2, dsp, width, height);
   if (ret != HWC2_ERROR_NONE) {
      goto out;
   }

out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_GET_CLIENT_TARGET_SUPPORT),
      display, getErrorName(ret));
   return ret;
}

static int32_t hwc2_clrMds(
   hwc2_device_t* device,
   hwc2_display_t display,
   uint32_t* outNumModes,
   int32_t* /*android_color_mode_t*/ outModes) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_GET_COLOR_MODES),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   *outNumModes = 1;
   if (outModes != NULL) {
      outModes[0] = 0; /*HAL_COLOR_MODE_NATIVE*/
   }

out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_GET_COLOR_MODES),
      display, getErrorName(ret));
   return ret;
}

static int32_t hwc2_dspAttr(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_config_t config,
   int32_t /*hwc2_attribute_t*/ attribute,
   int32_t* outValue) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_dsp_cfg_t *cfg = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 ":%u:%s\n",
     getFunctionDescriptorName(HWC2_FUNCTION_GET_DISPLAY_ATTRIBUTE),
     display, config, getAttributeName((hwc2_attribute_t)attribute));

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   cfg = (struct hwc2_dsp_cfg_t *)(intptr_t)config;
   if (cfg == NULL) {
      ret = HWC2_ERROR_BAD_CONFIG;
      goto out;
   }

   switch(attribute) {
   case HWC2_ATTRIBUTE_WIDTH:        *outValue = cfg->w; break;
   case HWC2_ATTRIBUTE_HEIGHT:       *outValue = cfg->h; break;
   case HWC2_ATTRIBUTE_VSYNC_PERIOD: *outValue = cfg->vsync; break;
   case HWC2_ATTRIBUTE_DPI_X:        *outValue = cfg->xdp; break;
   case HWC2_ATTRIBUTE_DPI_Y:        *outValue = cfg->ydp; break;
   default:                          *outValue = HWC2_INVALID;
   }

out:
   ALOGV("<- %s:%" PRIu64 ":%u:%s (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_GET_DISPLAY_ATTRIBUTE),
      display, config, getAttributeName((hwc2_attribute_t)attribute), getErrorName(ret));
   return ret;
}

static int32_t hwc2_hdrCap(
   hwc2_device_t* device,
   hwc2_display_t display,
   uint32_t* outNumTypes,
   int32_t* /*android_hdr_t*/ outTypes,
   float* outMaxLuminance,
   float* outMaxAverageLuminance,
   float* outMinLuminance) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_GET_HDR_CAPABILITIES),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   // TODO - HDR caps hook up.
   *outNumTypes = 0;

   (void)outTypes;
   (void)outMaxLuminance;
   (void)outMaxAverageLuminance;
   (void)outMinLuminance;

out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_GET_HDR_CAPABILITIES),
      display, getErrorName(ret));
   return ret;
}

static int32_t hwc2_dspCfg(
   hwc2_device_t* device,
   hwc2_display_t display,
   uint32_t* outNumConfigs,
   hwc2_config_t* outConfigs) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_dsp_cfg_t *cfg = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_GET_DISPLAY_CONFIGS),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   *outNumConfigs = 0;
   cfg = dsp->cfgs;
   while (cfg != NULL) {
      if (outConfigs != NULL) {
         outConfigs[*outNumConfigs] = (hwc2_config_t)(intptr_t)cfg;
      }
      *outNumConfigs += 1;
      cfg = cfg->next;
   }

out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_GET_DISPLAY_CONFIGS),
      display, getErrorName(ret));
   return ret;
}

static int32_t hwc2_dspName(
   hwc2_device_t* device,
   hwc2_display_t display,
   uint32_t* outSize,
   char* outName) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_GET_DISPLAY_NAME),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   *outSize = strlen(dsp->name);
   if (outName != NULL) {
      strncpy(outName, dsp->name, *outSize);
   }

out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_GET_DISPLAY_NAME),
      display, getErrorName(ret));
   return ret;
}

static int32_t hwc2_dspType(
   hwc2_device_t* device,
   hwc2_display_t display,
   int32_t* /*hwc2_display_type_t*/ outType) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_dsp_cfg_t *cfg = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_GET_DISPLAY_TYPE),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   *outType = dsp->type;

out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_GET_DISPLAY_TYPE),
      display, getErrorName(ret));
   return ret;
}

static int32_t hwc2_lyrBlend(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_layer_t layer,
   int32_t /*hwc2_blend_mode_t*/ mode) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_lyr_t *lyr = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 ":%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_BLEND_MODE),
     display, layer);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   lyr = hwc2_hnd2lyr(dsp, layer);
   if (lyr == NULL) {
      ret = HWC2_ERROR_BAD_LAYER;
      goto out;
   }

   if (mode <= HWC2_BLEND_MODE_INVALID || mode > HWC2_BLEND_MODE_COVERAGE) {
      ret = HWC2_ERROR_BAD_PARAMETER;
      goto out;
   }

   lyr->bm = (hwc2_blend_mode_t)mode;

out:
   ALOGV("<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_BLEND_MODE),
      display, layer, getErrorName(ret));
   return ret;
}

static int32_t hwc2_lyrBuf(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_layer_t layer,
   buffer_handle_t buffer,
   int32_t acquireFence) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_lyr_t *lyr = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 ":%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_BUFFER),
     display, layer);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   lyr = hwc2_hnd2lyr(dsp, layer);
   if (lyr == NULL) {
      ret = HWC2_ERROR_BAD_LAYER;
      goto out;
   }

   if (lyr->cDev == HWC2_COMPOSITION_SOLID_COLOR ||
       lyr->cDev == HWC2_COMPOSITION_SIDEBAND ||
       lyr->cDev == HWC2_COMPOSITION_CLIENT) {
      lyr->bh = NULL;
      lyr->af = HWC2_INVALID;
   } else {
      // TODO: validate buffer handle?
      lyr->bh = buffer;
      lyr->af = acquireFence;
   }

out:
   ALOGV("<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_BUFFER),
      display, layer, getErrorName(ret));
   return ret;
}

static int32_t hwc2_lyrCol(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_layer_t layer,
   hwc_color_t color) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_lyr_t *lyr = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 ":%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_COLOR),
     display, layer);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   lyr = hwc2_hnd2lyr(dsp, layer);
   if (lyr == NULL) {
      ret = HWC2_ERROR_BAD_LAYER;
      goto out;
   }

   if (lyr->cDev != HWC2_COMPOSITION_SOLID_COLOR) {
      goto out;
   }

   memcpy(&lyr->sc, &color, sizeof(color));

out:
   ALOGV("<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_COLOR),
      display, layer, getErrorName(ret));
   return ret;
}

static int32_t hwc2_lyrComp(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_layer_t layer,
   int32_t /*hwc2_composition_t*/ type) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_lyr_t *lyr = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 ":%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_COMPOSITION_TYPE),
     display, layer);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   lyr = hwc2_hnd2lyr(dsp, layer);
   if (lyr == NULL) {
      ret = HWC2_ERROR_BAD_LAYER;
      goto out;
   }

   if (type <= HWC2_COMPOSITION_INVALID || type > HWC2_COMPOSITION_SIDEBAND) {
      ret = HWC2_ERROR_BAD_PARAMETER;
      goto out;
   }

   if (type == HWC2_COMPOSITION_CURSOR) {
      ret = HWC2_ERROR_UNSUPPORTED;
      goto out;
   }

   lyr->cCli = (hwc2_composition_t)type;

out:
   ALOGV("<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_COMPOSITION_TYPE),
      display, layer, getErrorName(ret));
   return ret;
}

static int32_t hwc2_lyrDSpace(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_layer_t layer,
   int32_t /*android_dataspace_t*/ dataspace) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_lyr_t *lyr = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 ":%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_DATASPACE),
     display, layer);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   lyr = hwc2_hnd2lyr(dsp, layer);
   if (lyr == NULL) {
      ret = HWC2_ERROR_BAD_LAYER;
      goto out;
   }

   lyr->dsp = dataspace;

out:
   ALOGV("<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_DATASPACE),
      display, layer, getErrorName(ret));
   return ret;
}

static int32_t hwc2_lyrFrame(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_layer_t layer,
   hwc_rect_t frame) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_lyr_t *lyr = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 ":%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_DISPLAY_FRAME),
     display, layer);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   lyr = hwc2_hnd2lyr(dsp, layer);
   if (lyr == NULL) {
      ret = HWC2_ERROR_BAD_LAYER;
      goto out;
   }

   memcpy(&lyr->fr, &frame, sizeof(frame));

out:
   ALOGV("<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_DISPLAY_FRAME),
      display, layer, getErrorName(ret));
   return ret;
}

static int32_t hwc2_lyrAlpha(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_layer_t layer,
   float alpha) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_lyr_t *lyr = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 ":%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_PLANE_ALPHA),
     display, layer);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   lyr = hwc2_hnd2lyr(dsp, layer);
   if (lyr == NULL) {
      ret = HWC2_ERROR_BAD_LAYER;
      goto out;
   }

   lyr->al = alpha;

out:
   ALOGV("<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_PLANE_ALPHA),
      display, layer, getErrorName(ret));
   return ret;
}

static int32_t hwc2_lyrSbStr(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_layer_t layer,
   const native_handle_t* stream) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_lyr_t *lyr = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 ":%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_SIDEBAND_STREAM),
     display, layer);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   lyr = hwc2_hnd2lyr(dsp, layer);
   if (lyr == NULL) {
      ret = HWC2_ERROR_BAD_LAYER;
      goto out;
   }

   if (lyr->cDev != HWC2_COMPOSITION_SIDEBAND) {
      goto out;
   }

   if (stream->data[1] == 0) {
      ret = HWC2_ERROR_BAD_PARAMETER;
      goto out;
   }

   lyr->sbh = (native_handle_t*)stream;

out:
   ALOGV("<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_SIDEBAND_STREAM),
      display, layer, getErrorName(ret));
   return ret;
}

static int32_t hwc2_lyrCrop(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_layer_t layer,
   hwc_frect_t crop) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_lyr_t *lyr = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 ":%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_SOURCE_CROP),
     display, layer);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   lyr = hwc2_hnd2lyr(dsp, layer);
   if (lyr == NULL) {
      ret = HWC2_ERROR_BAD_LAYER;
      goto out;
   }

   lyr->crp.left   = (int) ceilf(crop.left);
   lyr->crp.top    = (int) ceilf(crop.top);
   lyr->crp.right  = (int) floorf(crop.right);
   lyr->crp.bottom = (int) floorf(crop.bottom);

out:
   ALOGV("<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_SOURCE_CROP),
      display, layer, getErrorName(ret));
   return ret;
}

static int32_t hwc2_lyrSfcDam(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_layer_t layer,
   hwc_region_t damage) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_lyr_t *lyr = NULL;
   uint32_t kind;
   hwc_rect_t empty;

   ALOGV("-> %s:%" PRIu64 ":%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_SURFACE_DAMAGE),
     display, layer);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   lyr = hwc2_hnd2lyr(dsp, layer);
   if (lyr == NULL) {
      ret = HWC2_ERROR_BAD_LAYER;
      goto out;
   }

   // TODO: to support surface damage, we need v3d to also do, which it does
   //       not right now, so we ignore this, except we check if the surface
   //       is the same as the prior composition cycle.

   memset(&empty, 0, sizeof(empty));
   if (damage.numRects == 1 &&
       damage.rects != NULL &&
       !memcmp(damage.rects, &empty, sizeof(empty))) {
      lyr->sfd = false;
   } else {
      lyr->sfd = true;
   }

out:
   ALOGV("<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_SURFACE_DAMAGE),
      display, layer, getErrorName(ret));
   return ret;
}

static int32_t hwc2_lyrTrans(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_layer_t layer,
   int32_t /*hwc_transform_t*/ transform) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_lyr_t *lyr = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 ":%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_TRANSFORM),
     display, layer);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   lyr = hwc2_hnd2lyr(dsp, layer);
   if (lyr == NULL) {
      ret = HWC2_ERROR_BAD_LAYER;
      goto out;
   }

   lyr->tr = (hwc_transform_t)transform;

out:
   ALOGV("<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_TRANSFORM),
      display, layer, getErrorName(ret));
   return ret;
}

static int32_t hwc2_lyrRegion(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_layer_t layer,
   hwc_region_t visible) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_lyr_t *lyr = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 ":%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_VISIBLE_REGION),
     display, layer);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   lyr = hwc2_hnd2lyr(dsp, layer);
   if (lyr == NULL) {
      ret = HWC2_ERROR_BAD_LAYER;
      goto out;
   }

   if (visible.numRects > 1) {
      ret = HWC2_ERROR_BAD_PARAMETER;
      goto out;
   }

   if (visible.numRects) {
      memcpy(&lyr->vis, visible.rects, sizeof(lyr->vis));
   }

out:
   ALOGV("<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_VISIBLE_REGION),
      display, layer, getErrorName(ret));
   return ret;
}

static int32_t hwc2_lyrZ(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_layer_t layer,
   uint32_t z) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_lyr_t *lyr = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 ":%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_Z_ORDER),
     display, layer);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   lyr = hwc2_hnd2lyr(dsp, layer);
   if (lyr == NULL) {
      ret = HWC2_ERROR_BAD_LAYER;
      goto out;
   }

   lyr->z = z;

out:
   ALOGV("<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_Z_ORDER),
      display, layer, getErrorName(ret));
   return ret;
}

static int32_t hwc2_outBuf(
   hwc2_device_t* device,
   hwc2_display_t display,
   buffer_handle_t buffer,
   int32_t releaseFence) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_OUTPUT_BUFFER),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   if (kind != HWC2_DSP_VD) {
      ret = HWC2_ERROR_UNSUPPORTED;
      goto out;
   }

   // TODO: validate buffer is proper.

   dsp->u.vd.output = buffer;
   dsp->u.vd.wrFence = releaseFence;

out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_OUTPUT_BUFFER),
      display, getErrorName(ret));
   return ret;
}

static int32_t hwc2_vsyncSet(
   hwc2_device_t* device,
   hwc2_display_t display,
   int32_t /*hwc2_vsync_t*/ enabled) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 ":%s\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_VSYNC_ENABLED),
     display, getVsyncName((hwc2_vsync_t)enabled));

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   if (kind == HWC2_DSP_VD) {
      goto out;
   }

   if (enabled < HWC2_VSYNC_INVALID ||
       enabled > HWC2_VSYNC_DISABLE) {
      ret = HWC2_ERROR_BAD_PARAMETER;
      goto out;
   }

   dsp->u.ext.vsync = (hwc2_vsync_t)enabled;

out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_VSYNC_ENABLED),
      display, getErrorName(ret));
   return ret;
}

static int32_t hwc2_cursorPos(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_layer_t layer,
   int32_t x,
   int32_t y) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   struct hwc2_lyr_t *lyr = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 ":%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_CURSOR_POSITION),
     display, layer);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   lyr = hwc2_hnd2lyr(dsp, layer);
   if (lyr == NULL) {
      ret = HWC2_ERROR_BAD_LAYER;
      goto out;
   }

   if (lyr->cCli != HWC2_COMPOSITION_CURSOR) {
      ret = HWC2_ERROR_BAD_LAYER;
      goto out;
   }

   lyr->cx = x;
   lyr->cy = y;

out:
   ALOGV("<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_CURSOR_POSITION),
      display, layer, getErrorName(ret));
   return ret;
}

static int32_t hwc2_clrTrs(
   hwc2_device_t* device,
   hwc2_display_t display,
   const float* matrix,
   int32_t /*android_color_transform_t*/ hint) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_COLOR_TRANSFORM),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   if (hint < HAL_COLOR_TRANSFORM_IDENTITY ||
       hint > HAL_COLOR_TRANSFORM_CORRECT_TRITANOPIA) {
      ret = HWC2_ERROR_BAD_PARAMETER;
      goto out;
   }

   if (kind == HWC2_DSP_VD) {
      dsp->u.vd.cm.type = (android_color_transform_t)hint;
      memcpy((void *)matrix, dsp->u.vd.cm.mtx, sizeof(dsp->u.vd.cm.mtx));
   } else {
      dsp->u.ext.cm.type = (android_color_transform_t)hint;
      memcpy((void *)matrix, dsp->u.ext.cm.mtx, sizeof(dsp->u.ext.cm.mtx));
   }

out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_COLOR_TRANSFORM),
      display, getErrorName(ret));
   return ret;
}

static int32_t hwc2_clrMode(
   hwc2_device_t* device,
   hwc2_display_t display,
   int32_t /*android_color_mode_t*/ mode) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_COLOR_MODE),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   // TODO: the android_color_mode_t is not defined in the N code... an
   //       oversight... ignoring for now and say okay.
   (void)mode;

out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_COLOR_MODE),
      display, getErrorName(ret));
   return ret;
}

static int32_t hwc2_cliTgt(
   hwc2_device_t* device,
   hwc2_display_t display,
   buffer_handle_t target,
   int32_t acquireFence,
   int32_t /*android_dataspace_t*/ dataspace,
   hwc_region_t damage) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   uint32_t kind;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_CLIENT_TARGET),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   if (kind == HWC2_DSP_VD) {
      dsp->u.vd.ct.tgt = target;
      dsp->u.vd.ct.rdf = acquireFence;
      dsp->u.vd.ct.dsp = (android_dataspace_t)dataspace;
      dsp->u.vd.ct.dmg_n = damage.numRects;
      if (dsp->u.vd.ct.dmg_n) {
         if (dsp->u.vd.ct.dmg_n > 1) {
            ALOGW("[vd]: client target with more than one damage region.\n");
         } else {
            memcpy(&dsp->u.vd.ct.dmg_r, damage.rects, sizeof(dsp->u.vd.ct.dmg_r));
         }
      }
   } else {
      dsp->u.ext.ct.tgt = target;
      dsp->u.ext.ct.rdf = acquireFence;
      dsp->u.ext.ct.dsp = (android_dataspace_t)dataspace;
      dsp->u.ext.ct.dmg_n = damage.numRects;
      if (dsp->u.ext.ct.dmg_n) {
         if (dsp->u.ext.ct.dmg_n > 1) {
            ALOGW("[ext]: client target with more than one damage region.\n");
         } else {
            memcpy(&dsp->u.ext.ct.dmg_r, damage.rects, sizeof(dsp->u.ext.ct.dmg_r));
         }
      }
   }

out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_CLIENT_TARGET),
      display, getErrorName(ret));
   return ret;
}

static int32_t hwc2_sActCfg(
   hwc2_device_t* device,
   hwc2_display_t display,
   hwc2_config_t config) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   uint32_t kind;
   struct hwc2_dsp_cfg_t *cfg = NULL;

   ALOGV("-> %s:%" PRIu64 ":%u\n",
     getFunctionDescriptorName(HWC2_FUNCTION_SET_ACTIVE_CONFIG),
     display, config);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   cfg = dsp->cfgs;
   while (cfg != NULL) {
      if (config == (hwc2_config_t)(intptr_t)cfg)
         break;
      cfg = cfg->next;
   };
   if (cfg == NULL) {
      ret = HWC2_ERROR_BAD_PARAMETER;
      goto out;
   } else {
      dsp->aCfg = cfg;
   }

out:
   ALOGV("<- %s:%" PRIu64 ":%u (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_ACTIVE_CONFIG),
      display, config, getErrorName(ret));
   return ret;
}

static int32_t hwc2_relFences(
   hwc2_device_t* device,
   hwc2_display_t display,
   uint32_t* outNumElements,
   hwc2_layer_t* outLayers,
   int32_t* outFences) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   uint32_t kind;
   struct hwc2_lyr_t *lyr = NULL;
   size_t num;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_GET_RELEASE_FENCES),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   if (outLayers == NULL || outFences == NULL) {
      *outNumElements = 0;
      lyr = dsp->lyr;
      while (lyr != NULL) {
         if (lyr->cDev == HWC2_COMPOSITION_DEVICE) {
            *outNumElements += 1;
         }
         lyr = lyr->next;
      }
   } else {
      num = 0;
      lyr = dsp->lyr;
      while (lyr != NULL) {
         if (lyr->cDev == HWC2_COMPOSITION_DEVICE) {
            lyr->rf = hwc2_lyr_tl_add(dsp, kind, lyr);
            outLayers[num] = (hwc2_layer_t)(intptr_t)lyr;
            outFences[num] = lyr->rf;
            num++;
         }
         lyr = lyr->next;
      }
   }

out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_GET_RELEASE_FENCES),
      display, getErrorName(ret));
   return ret;
}

static int32_t hwc2_dspReqs(
   hwc2_device_t* device,
   hwc2_display_t display,
   int32_t* /*hwc2_display_request_t*/ outDisplayRequests,
   uint32_t* outNumElements,
   hwc2_layer_t* outLayers,
   int32_t* /*hwc2_layer_request_t*/ outLayerRequests) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   uint32_t kind;
   struct hwc2_lyr_t *lyr = NULL;
   int32_t i = 0;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_GET_DISPLAY_REQUESTS),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   if (!dsp->validated) {
      ret = HWC2_ERROR_NOT_VALIDATED;
      goto out;
   }

   lyr = dsp->lyr;
   *outDisplayRequests = HWC2_DISPLAY_REQUEST_FLIP_CLIENT_TARGET;

   if (outLayers == NULL || outLayerRequests == NULL) {
      *outNumElements = 0;
      while (lyr != NULL) {
         if (lyr->rp) {
            *outNumElements += 1;
         }
         lyr = lyr->next;
      }
   } else {
      while (lyr != NULL) {
         if (lyr->rp) {
            *(outLayers+i) = (hwc2_layer_t)(intptr_t)lyr;
            *(outLayerRequests+i) = HWC2_LAYER_REQUEST_CLEAR_CLIENT_TARGET;
            lyr->rp = false;
            i++;
         }
         lyr = lyr->next;
      }
   }

out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_GET_DISPLAY_REQUESTS),
      display, getErrorName(ret));
   return ret;
}

static int32_t hwc2_valDsp(
   hwc2_device_t* device,
   hwc2_display_t display,
   uint32_t* outNumTypes,
   uint32_t* outNumRequests) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   uint32_t kind;
   struct hwc2_lyr_t *lyr = NULL;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_VALIDATE_DISPLAY),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   lyr = dsp->lyr;
   *outNumTypes = 0;
   *outNumRequests = 0;

   while (lyr != NULL) {
      if (lyr->rp) {
         *outNumRequests += 1;
      }
      if (lyr->cCli != lyr->cDev) {
         *outNumTypes += 1;
      }
      lyr = lyr->next;
   }

   dsp->validated = true;

out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_VALIDATE_DISPLAY),
      display, getErrorName(ret));
   return ret;
}

static int32_t hwc2_cntLyr(
   struct hwc2_dsp_t *dsp) {

   int cnt = 0;
   struct hwc2_lyr_t *lyr = NULL;

   lyr = dsp->lyr;
   while (lyr != NULL) {
      lyr = lyr->next;
   };

   return cnt;
}

static int32_t hwc2_preDsp(
   hwc2_device_t* device,
   hwc2_display_t display,
   int32_t* outRetireFence) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;
   struct hwc2_dsp_t *dsp = NULL;
   uint32_t kind;
   struct hwc2_lyr_t *lyr = NULL;
   int32_t frame_size, cnt;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_PRESENT_DISPLAY),
     display);

   *outRetireFence = -1;

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   if (!dsp->validated) {
      ret = HWC2_ERROR_NOT_VALIDATED;
      goto out;
   }

   if ((kind == HWC2_DSP_VD) && (dsp->u.vd.output == NULL)) {
      ret = HWC2_ERROR_NO_RESOURCES;
      goto out;
   }

   dsp->pres++;

   if (kind == HWC2_DSP_EXT) {
      struct hwc2_frame_t *frame = NULL;
      struct hwc2_lyr_t *clyr = NULL;

      pthread_mutex_lock(&hwc2->mtx_pwr);
      if (hwc2->ext->pmode == HWC2_POWER_MODE_OFF) {
         pthread_mutex_unlock(&hwc2->mtx_pwr);
         goto out_error;
      }
      pthread_mutex_unlock(&hwc2->mtx_pwr);

      cnt = hwc2_cntLyr(dsp);
      frame_size = sizeof(struct hwc2_frame_t) + (cnt * sizeof(struct hwc2_lyr_t));
      frame = (struct hwc2_frame_t *) calloc(1, frame_size);
      if (frame == NULL) {
         goto out_error;
      }
      frame->pres = dsp->pres;
      frame->cnt  = cnt;
      lyr = dsp->lyr;
      clyr = &frame->lyr[0];
      for (frame_size = 0 ; frame_size < cnt ; frame_size++) {
         memcpy(clyr, lyr, sizeof(struct hwc2_lyr_t));
         lyr = lyr->next;
         clyr->next = &frame->lyr[frame_size+1];
         clyr = &frame->lyr[frame_size+1];
      }
      if (!pthread_mutex_lock(&dsp->mtx_cmp_wl)) {
         if (dsp->cmp_wl == NULL) {
            dsp->cmp_wl = frame;
         } else {
            struct hwc2_frame_t *item, *last;
            last = dsp->cmp_wl;
            while (last != NULL) { item = last; last = last->next; }
            item->next = frame;
         }
         pthread_mutex_unlock(&dsp->mtx_cmp_wl);
      } else {
         free(frame);
         goto out_error;
      }

      *outRetireFence = hwc2_ret_fence(dsp);
      BKNI_SetEvent(dsp->cmp_evt);
      goto out;
   } else if (kind == HWC2_DSP_VD) {
      *outRetireFence = -1;
      goto out;
   }

out_error:
   if (kind == HWC2_DSP_EXT) {
      cnt = hwc2_cntLyr(dsp);
      lyr = dsp->lyr;
      for (frame_size = 0 ; frame_size < cnt ; frame_size++) {
         if (lyr->rf != HWC2_INVALID) {
            hwc2_lyr_tl_inc(dsp, kind, lyr);
         }
         lyr = lyr->next;
      }
   }
out:
   ALOGV("<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_PRESENT_DISPLAY),
      display, getErrorName(ret));
   return ret;
}

typedef struct hwc2_fncElem {
   hwc2_function_descriptor_t desc;
   hwc2_function_pointer_t func;
} hwc2_fncElem_t;

static hwc2_fncElem_t hwc2_fncsMap[] = {
   {HWC2_FUNCTION_ACCEPT_DISPLAY_CHANGES,           (hwc2_function_pointer_t)&hwc2_dspAckChg},
   {HWC2_FUNCTION_CREATE_LAYER,                     (hwc2_function_pointer_t)&hwc2_lyrAdd},
   {HWC2_FUNCTION_CREATE_VIRTUAL_DISPLAY,           (hwc2_function_pointer_t)&hwc2_vdAdd},
   {HWC2_FUNCTION_DESTROY_LAYER,                    (hwc2_function_pointer_t)&hwc2_lyrRem},
   {HWC2_FUNCTION_DESTROY_VIRTUAL_DISPLAY,          (hwc2_function_pointer_t)&hwc2_vdRem},
   {HWC2_FUNCTION_DUMP,                             (hwc2_function_pointer_t)&hwc2_dump},
   {HWC2_FUNCTION_GET_ACTIVE_CONFIG,                (hwc2_function_pointer_t)&hwc2_gActCfg},
   {HWC2_FUNCTION_GET_CHANGED_COMPOSITION_TYPES,    (hwc2_function_pointer_t)&hwc2_getDevCmp},
   {HWC2_FUNCTION_GET_CLIENT_TARGET_SUPPORT,        (hwc2_function_pointer_t)&hwc2_ackCliTgt},
   {HWC2_FUNCTION_GET_COLOR_MODES,                  (hwc2_function_pointer_t)&hwc2_clrMds},
   {HWC2_FUNCTION_GET_DISPLAY_ATTRIBUTE,            (hwc2_function_pointer_t)&hwc2_dspAttr},
   {HWC2_FUNCTION_GET_DISPLAY_CONFIGS,              (hwc2_function_pointer_t)&hwc2_dspCfg},
   {HWC2_FUNCTION_GET_DISPLAY_NAME,                 (hwc2_function_pointer_t)&hwc2_dspName},
   {HWC2_FUNCTION_GET_DISPLAY_REQUESTS,             (hwc2_function_pointer_t)&hwc2_dspReqs},
   {HWC2_FUNCTION_GET_DISPLAY_TYPE,                 (hwc2_function_pointer_t)&hwc2_dspType},
   {HWC2_FUNCTION_GET_DOZE_SUPPORT,                 (hwc2_function_pointer_t)&hwc2_dzSup},
   {HWC2_FUNCTION_GET_HDR_CAPABILITIES,             (hwc2_function_pointer_t)&hwc2_hdrCap},
   {HWC2_FUNCTION_GET_MAX_VIRTUAL_DISPLAY_COUNT,    (hwc2_function_pointer_t)&hwc2_vdCnt},
   {HWC2_FUNCTION_GET_RELEASE_FENCES,               (hwc2_function_pointer_t)&hwc2_relFences},
   {HWC2_FUNCTION_PRESENT_DISPLAY,                  (hwc2_function_pointer_t)&hwc2_preDsp},
   {HWC2_FUNCTION_REGISTER_CALLBACK,                (hwc2_function_pointer_t)&hwc2_regCb},
   {HWC2_FUNCTION_SET_ACTIVE_CONFIG,                (hwc2_function_pointer_t)&hwc2_sActCfg},
   {HWC2_FUNCTION_SET_CLIENT_TARGET,                (hwc2_function_pointer_t)&hwc2_cliTgt},
   {HWC2_FUNCTION_SET_COLOR_MODE,                   (hwc2_function_pointer_t)&hwc2_clrMode},
   {HWC2_FUNCTION_SET_COLOR_TRANSFORM,              (hwc2_function_pointer_t)&hwc2_clrTrs},
   {HWC2_FUNCTION_SET_CURSOR_POSITION,              (hwc2_function_pointer_t)&hwc2_cursorPos},
   {HWC2_FUNCTION_SET_LAYER_BLEND_MODE,             (hwc2_function_pointer_t)&hwc2_lyrBlend},
   {HWC2_FUNCTION_SET_LAYER_BUFFER,                 (hwc2_function_pointer_t)&hwc2_lyrBuf},
   {HWC2_FUNCTION_SET_LAYER_COLOR,                  (hwc2_function_pointer_t)&hwc2_lyrCol},
   {HWC2_FUNCTION_SET_LAYER_COMPOSITION_TYPE,       (hwc2_function_pointer_t)&hwc2_lyrComp},
   {HWC2_FUNCTION_SET_LAYER_DATASPACE,              (hwc2_function_pointer_t)&hwc2_lyrDSpace},
   {HWC2_FUNCTION_SET_LAYER_DISPLAY_FRAME,          (hwc2_function_pointer_t)&hwc2_lyrFrame},
   {HWC2_FUNCTION_SET_LAYER_PLANE_ALPHA,            (hwc2_function_pointer_t)&hwc2_lyrAlpha},
   {HWC2_FUNCTION_SET_LAYER_SIDEBAND_STREAM,        (hwc2_function_pointer_t)&hwc2_lyrSbStr},
   {HWC2_FUNCTION_SET_LAYER_SOURCE_CROP,            (hwc2_function_pointer_t)&hwc2_lyrCrop},
   {HWC2_FUNCTION_SET_LAYER_SURFACE_DAMAGE,         (hwc2_function_pointer_t)&hwc2_lyrSfcDam},
   {HWC2_FUNCTION_SET_LAYER_TRANSFORM,              (hwc2_function_pointer_t)&hwc2_lyrTrans},
   {HWC2_FUNCTION_SET_LAYER_VISIBLE_REGION,         (hwc2_function_pointer_t)&hwc2_lyrRegion},
   {HWC2_FUNCTION_SET_LAYER_Z_ORDER,                (hwc2_function_pointer_t)&hwc2_lyrZ},
   {HWC2_FUNCTION_SET_OUTPUT_BUFFER,                (hwc2_function_pointer_t)&hwc2_outBuf},
   {HWC2_FUNCTION_SET_POWER_MODE,                   (hwc2_function_pointer_t)&hwc2_pwrMode},
   {HWC2_FUNCTION_SET_VSYNC_ENABLED,                (hwc2_function_pointer_t)&hwc2_vsyncSet},
   {HWC2_FUNCTION_VALIDATE_DISPLAY,                 (hwc2_function_pointer_t)&hwc2_valDsp},

   {HWC2_FUNCTION_INVALID,                          (hwc2_function_pointer_t)NULL},
};

static hwc2_function_pointer_t hwc2_getFncs(
   struct hwc2_device* device,
   int32_t /*hwc2_function_descriptor_t*/ descriptor) {

   int32_t i = 0;

   if (device == NULL) {
      return NULL;
   }

   while (hwc2_fncsMap[i].desc != HWC2_FUNCTION_INVALID) {
      if (hwc2_fncsMap[i].desc == (hwc2_function_descriptor_t)descriptor) {
         return hwc2_fncsMap[i].func;
      }
      i++;
   }

   return NULL;
}

static void hwc2_close_memif(
   struct hwc2_bcm_device_t *hwc2)
{
   if (hwc2->memfd != HWC2_INVALID) {
      close(hwc2->memfd);
      hwc2->memfd = HWC2_INVALID;
   }
}

static void hwc2_bcm_close(
   struct hwc2_bcm_device_t *hwc2) {

   struct hwc2_lyr_t *lyr, *lyr2 = NULL;
   struct hwc2_dsp_cfg_t *cfg, *cfg2 = NULL;
   size_t num;

   if (hwc2->vd) {
      if (hwc2->vd->cfgs) {
         cfg = hwc2->vd->cfgs;
         while (cfg != NULL) {
            cfg2 = cfg->next;
            free(cfg);
            cfg = cfg2;
         }
      }
      if (hwc2->vd->lyr) {
         lyr = hwc2->vd->lyr;
         while (lyr != NULL) {
            lyr2 = lyr->next;
            free(lyr);
            lyr = lyr2;
         }
      }
      free(hwc2->vd);
   }

   if (hwc2->ext) {
      if (hwc2->ext->cfgs) {
         cfg = hwc2->ext->cfgs;
         while (cfg != NULL) {
            cfg2 = cfg->next;
            free(cfg);
            cfg = cfg2;
         }
      }
      if (hwc2->ext->lyr) {
         lyr = hwc2->ext->lyr;
         while (lyr != NULL) {
            lyr2 = lyr->next;
            free(lyr);
            lyr = lyr2;
         }
      }
      NEXUS_SurfaceClient_Release(hwc2->ext->u.ext.sch);
      NxClient_Free(&hwc2->ext->u.ext.nxa);
      hwc2->ext->cmp_active = 0;
      pthread_join(hwc2->ext->cmp, NULL);
      pthread_mutex_destroy(&hwc2->ext->mtx_cmp_wl);
      pthread_mutex_destroy(&hwc2->ext->u.ext.mtx_fbs);
      for (num = 0 ; num < HWC2_MAX_TL ; num++) {
         close(hwc2->ext->u.ext.rtl[num].tl);
      }
      free(hwc2->ext);
   }

   hwc2->recycle_exit = 1;
   pthread_join(hwc2->recycle_task, NULL);
   hwc2->vsync_exit = 1;
   pthread_join(hwc2->vsync_task, NULL);

   BKNI_DestroyEvent(hwc2->recycle);
   BKNI_DestroyEvent(hwc2->recycled);
   BKNI_DestroyEvent(hwc2->vsync);
   BKNI_DestroyEvent(hwc2->g2dchk);

   NEXUS_Graphics2D_Close(hwc2->hg2d);

   pthread_mutex_destroy(&hwc2->mtx_pwr);
   pthread_mutex_destroy(&hwc2->mtx_g2d);

   hwc2->nxipc->destroyClientContext(hwc2->nxcc);
   if (hwc2->hp) {
      hwc2->nxipc->removeHdmiHotplugEventListener(0, hwc2->hp->get());
      delete hwc2->hp;
   }
   if (hwc2->dc) {
      hwc2->nxipc->removeDisplaySettingsChangedEventListener(0, hwc2->dc->get());
      delete hwc2->dc;
   }
   delete hwc2->nxipc;
   if (hwc2->hb) {
      delete hwc2->hb;
   }

   hwc2_close_memif(hwc2);
}

static int hwc2_close(
   struct hw_device_t *dev) {

   struct hwc2_bcm_device_t* hwc2 = (struct hwc2_bcm_device_t *)(dev);

   hwc2_bcm_close(hwc2);
   hwc2->magic = 0;

   if (hwc2) {
      free(hwc2);
   }
   return 0;
}

static void hwc2_setup_memif(
   struct hwc2_bcm_device_t *hwc2)
{
   char device[PROPERTY_VALUE_MAX];

   if (strlen(hwc2->memif) == 0) {
      memset(device, 0, sizeof(device));
      memset(hwc2->memif, 0, sizeof(hwc2->memif));

      property_get(HWC2_MEMIF_DEV, device, NULL);
      if (strlen(device)) {
         strcpy(hwc2->memif, "/dev/");
         strcat(hwc2->memif, device);
      }
   }
}

static int hwc2_open_memif(
   struct hwc2_bcm_device_t *hwc2)
{
   int fd = HWC2_INVALID;

   if (hwc2->memfd >= 0) {
      goto out;
   }
   hwc2_setup_memif(hwc2);
   hwc2->memfd = open(hwc2->memif, O_RDWR, 0);

out:
   return hwc2->memfd;
}

//pierre
static void hwc2_ext_cmp_frame(
   struct hwc2_bcm_device_t* hwc2,
   struct hwc2_frame_t *f) {

   NEXUS_SurfaceHandle d = NULL;

   /* setup and grab a destination buffer for this frame. */
   if (hwc2->ext->u.ext.cbs) {
      enum hwc2_cbs_e wcb = hwc2->ext->u.ext.cb;
      if (wcb == cbs_e_none || wcb == cbs_e_nscfb) {
         wcb = cbs_e_bypass;
      } else {
         wcb = cbs_e_nscfb;
      }
      hwc2_ext_fbs(hwc2, wcb);
      hwc2->ext->u.ext.cbs = false;
   }
   d = hwc2_ext_fb_get(hwc2);
   if (d == NULL) {
      hwc2_ret_inc(hwc2->ext, 1);
      return;
   }

   /* compose the layers from the frame into the destination; wait on fence as needed. */
   (void) f;

   return;
}

static void *hwc2_ext_cmp(
   void *argv) {
   struct hwc2_bcm_device_t* hwc2 = (struct hwc2_bcm_device_t *)(argv);
   struct hwc2_frame_t *frame = NULL;
   static int hwc2_cmp_threshold = 2;

   setpriority(PRIO_PROCESS, 0, PRIORITY_URGENT_DISPLAY + 2*PRIORITY_MORE_FAVORABLE);
   set_sched_policy(0, SP_FOREGROUND);

   do {
      if (pthread_mutex_lock(&hwc2->mtx_pwr)) {
         LOG_ALWAYS_FATAL("[ext]: composer failure on power mutex.");
         return NULL;
      }
      if (hwc2->ext->pmode != HWC2_POWER_MODE_OFF) {
         bool composing = true;
         int composed = 0;
         pthread_mutex_unlock(&hwc2->mtx_pwr);
         BKNI_WaitForEvent(hwc2->ext->cmp_evt, BKNI_INFINITE);
         do {
            frame = NULL;
            if (!pthread_mutex_lock(&hwc2->ext->mtx_cmp_wl)) {
               frame = hwc2->ext->cmp_wl;
               if (frame == NULL) {
                  composing = false;
               } else {
                  hwc2->ext->cmp_wl = frame->next;
                  if (++composed > hwc2_cmp_threshold) {
                     composing = false;
                  }
               }
               pthread_mutex_unlock(&hwc2->ext->mtx_cmp_wl);
            } else {
               composing = false;
            }

            if (frame != NULL) {
               hwc2_ext_cmp_frame(hwc2, frame);
               free(frame);
               frame = NULL;
            }
         } while (composing);
      } else {
         pthread_mutex_unlock(&hwc2->mtx_pwr);
         BKNI_Sleep(16); /* pas beau. */
      }
   } while(hwc2->ext->cmp_active);

   return NULL;
}

static enum hwc2_cbs_e hwc2_want_comp_bypass(
   NxClient_DisplaySettings *settings) {

   NxClient_GetDisplaySettings(settings);
   switch (settings->format) {
   case NEXUS_VideoFormat_e720p:
   case NEXUS_VideoFormat_e720p50hz:
   case NEXUS_VideoFormat_e720p30hz:
   case NEXUS_VideoFormat_e720p25hz:
   case NEXUS_VideoFormat_e720p24hz:
   case NEXUS_VideoFormat_ePal:
   case NEXUS_VideoFormat_eSecam:
   case NEXUS_VideoFormat_eVesa640x480p60hz:
   case NEXUS_VideoFormat_eVesa800x600p60hz:
   case NEXUS_VideoFormat_eVesa1024x768p60hz:
   case NEXUS_VideoFormat_eVesa1280x768p60hz:
      return hwc2_cbs_e::cbs_e_nscfb;
   break;
   default:
   break;
   }
   return hwc2_cbs_e::cbs_e_bypass;
}

static void hwc2_setup_ext(
   struct hwc2_bcm_device_t *hwc2) {

   NxClient_AllocSettings alloc;
   struct hwc2_dsp_t *ext;
   NEXUS_Error rc;
   pthread_attr_t attr;
   pthread_mutexattr_t mattr;
   NEXUS_SurfaceComposition composition;
   NEXUS_InterfaceName interfaceName;
   NEXUS_PlatformObjectInstance objects[HWC2_NX_DSP_OBJ];
   size_t num = HWC2_NX_DSP_OBJ;
   NEXUS_HdmiOutputHandle hdmi;
   NEXUS_HdmiOutputStatus hstatus;

   ext = (struct hwc2_dsp_t *)malloc(sizeof(*ext));
   if (ext == NULL) {
      LOG_ALWAYS_FATAL("failed to allocate external display (memory).");
      return;
   }
   memset(ext, 0, sizeof(*ext));
   hwc2->ext = ext;

   snprintf(hwc2->ext->name, strlen(hwc2->ext->name), "stbHD0");
   hwc2->ext->type    = HWC2_DISPLAY_TYPE_PHYSICAL;

   hwc2->ext->cfgs = (struct hwc2_dsp_cfg_t *)malloc(sizeof(struct hwc2_dsp_cfg_t));
   if (hwc2->ext->cfgs) {
      hwc2->ext->cfgs->next  = NULL;
      hwc2->ext->cfgs->w     = 1920; /* hardcode 1080p */
      hwc2->ext->cfgs->h     = 1080; /* hardcode 1080p */
      hwc2->ext->cfgs->vsync = 16666667; /* hardcode 60fps */
      hwc2->ext->cfgs->xdp   = 0;
      hwc2->ext->cfgs->ydp   = 0;
   }
   hwc2->ext->aCfg = hwc2->ext->cfgs;

   NxClient_GetDefaultAllocSettings(&alloc);
   alloc.surfaceClient = 1; /* single surface client for 'ext' display. */
   rc = NxClient_Alloc(&alloc, &hwc2->ext->u.ext.nxa);
   if (rc) {
      LOG_ALWAYS_FATAL("failed to allocate external display (nexus resources).");
      return;
   }
   hwc2->ext->u.ext.sch = NEXUS_SurfaceClient_Acquire(hwc2->ext->u.ext.nxa.surfaceClient[0].id);

   BKNI_CreateEvent(&hwc2->ext->cmp_evt);
   hwc2->ext->cmp_active = 1;
   hwc2->ext->cmp_wl = NULL;
   pthread_mutexattr_init(&mattr);
   pthread_mutex_init(&hwc2->ext->mtx_cmp_wl, &mattr);
   pthread_mutex_init(&hwc2->ext->u.ext.mtx_fbs, &mattr);
   pthread_mutexattr_destroy(&mattr);
   pthread_attr_init(&attr);
   pthread_create(&hwc2->ext->cmp, &attr, hwc2_ext_cmp, (void *)hwc2);
   pthread_attr_destroy(&attr);
   pthread_setname_np(hwc2->ext->cmp, "hwc2_ext_cmp");

   hwc2->ext->cmp_tl = sw_sync_timeline_create();
   if (hwc2->ext->cmp_tl < 0) {
      hwc2->ext->cmp_tl = HWC2_INVALID;
   }

   NxClient_GetSurfaceClientComposition(hwc2->ext->u.ext.nxa.surfaceClient[0].id, &composition);
   composition.virtualDisplay.width  = hwc2->ext->cfgs->w;
   composition.virtualDisplay.height = hwc2->ext->cfgs->h;
   composition.position.x            = 0;
   composition.position.y            = 0;
   composition.position.width        = composition.virtualDisplay.width;
   composition.position.height       = composition.virtualDisplay.height;
   composition.zorder                = 5; /* above any video or sideband layer(s). */
   composition.visible               = true;
   composition.colorBlend            = hwc2_color_pre_multi;
   composition.alphaBlend            = hwc2_alpha_pre_multi;
   NxClient_SetSurfaceClientComposition(hwc2->ext->u.ext.nxa.surfaceClient[0].id, &composition);

   strcpy(interfaceName.name, "NEXUS_Display");
   rc = NEXUS_Platform_GetObjects(&interfaceName, &objects[0], num, &num);
   if (!rc) {
      hwc2->ext->u.ext.hdsp = (NEXUS_DisplayHandle)objects[0].object;
   }

   hdmi = NEXUS_HdmiOutput_Open(0+NEXUS_ALIAS_ID, NULL);
   NEXUS_HdmiOutput_GetStatus(hdmi, &hstatus);
   if (hstatus.connected) {
      NxClient_DisplaySettings settings;
      enum hwc2_cbs_e wcb = hwc2_want_comp_bypass(&settings);
      hwc2_ext_fbs(hwc2, wcb);
      if (hwc2->regCb[HWC2_CALLBACK_HOTPLUG-1].func != NULL) {
         ALOGV("[ext]: initial hotplug CONNECTED\n");
         HWC2_PFN_HOTPLUG f_hp = (HWC2_PFN_HOTPLUG) hwc2->regCb[HWC2_CALLBACK_HOTPLUG-1].func;
         f_hp(hwc2->regCb[HWC2_CALLBACK_HOTPLUG-1].data,
              (hwc2_display_t)(intptr_t)hwc2->ext,
              (int)HWC2_CONNECTION_CONNECTED);
      }
   }

   for (num = 0 ; num < HWC2_MAX_TL ; num++) {
      hwc2->ext->u.ext.rtl[num].tl = sw_sync_timeline_create();
      if (hwc2->ext->u.ext.rtl[num].tl < 0) {
         hwc2->ext->u.ext.rtl[num].tl = HWC2_INVALID;
      }
      hwc2->ext->u.ext.rtl[num].hdl = NULL;
      hwc2->ext->u.ext.rtl[num].ix = 0;
   }
}

static void hwc2_hb_ntfy(
   void *dev,
   int msg,
   struct hwc_notification_info &ntfy) {
   struct hwc2_bcm_device_t* hwc2 = (struct hwc2_bcm_device_t *)(dev);

   switch (msg) {
   case HWC_BINDER_NTFY_CONNECTED:
       hwc2->hb->connected(true);
   break;
   case HWC_BINDER_NTFY_DISCONNECTED:
       hwc2->hb->connected(false);
   break;
   case HWC_BINDER_NTFY_VIDEO_SURFACE_ACQUIRED:
   case HWC_BINDER_NTFY_SIDEBAND_SURFACE_ACQUIRED:
      (void)ntfy;
   break;
   case HWC_BINDER_NTFY_OVERSCAN:
   break;
   default:
   break;
   }
}

static void hwc2_dc_ntfy(
   void *dev) {
   struct hwc2_bcm_device_t* hwc2 = (struct hwc2_bcm_device_t *)(dev);
   NxClient_DisplaySettings settings;

   enum hwc2_cbs_e wcb = hwc2_want_comp_bypass(&settings);
   if (wcb != hwc2->ext->u.ext.cb) {
      hwc2->ext->u.ext.cbs = true;
   } else {
      hwc2->ext->u.ext.cbs = false;
   }
}

static void hwc2_hp_ntfy(
   void *dev,
   bool connected) {

   struct hwc2_bcm_device_t* hwc2 = (struct hwc2_bcm_device_t *)(dev);
   NxClient_DisplaySettings settings;

   if (connected) {
      enum hwc2_cbs_e wcb = hwc2_want_comp_bypass(&settings);
      NEXUS_HdmiOutputHandle handle;
      NEXUS_HdmiOutputBasicEdidData edid;
      NEXUS_VideoFormatInfo info;
      NEXUS_Error errCode;

      if (wcb != hwc2->ext->u.ext.cb) {
         hwc2->ext->u.ext.cbs = true;
      } else {
         hwc2->ext->u.ext.cbs = false;
      }

      if (hwc2->regCb[HWC2_CALLBACK_HOTPLUG-1].func != NULL) {
         ALOGV("[ext]: notify hotplug CONNECTED\n");
         HWC2_PFN_HOTPLUG f_hp = (HWC2_PFN_HOTPLUG) hwc2->regCb[HWC2_CALLBACK_HOTPLUG-1].func;
         f_hp(hwc2->regCb[HWC2_CALLBACK_HOTPLUG-1].data,
              (hwc2_display_t)(intptr_t)hwc2->ext,
              (int)HWC2_CONNECTION_CONNECTED);
      }

      if (hwc2->regCb[HWC2_CALLBACK_REFRESH-1].func != NULL) {
         HWC2_PFN_REFRESH f_refresh = (HWC2_PFN_REFRESH) hwc2->regCb[HWC2_CALLBACK_REFRESH-1].func;
         f_refresh(hwc2->regCb[HWC2_CALLBACK_REFRESH-1].data,
                   (hwc2_display_t)(intptr_t)hwc2->ext);
      }

      NEXUS_PlatformConfiguration *pConfig = (NEXUS_PlatformConfiguration *)BKNI_Malloc(sizeof(*pConfig));
      if (pConfig) {
         NEXUS_Platform_GetConfiguration(pConfig);
         handle = pConfig->outputs.hdmi[0]; /* always first output. */
         BKNI_Free(pConfig);
         errCode = NEXUS_HdmiOutput_GetBasicEdidData(handle, &edid);
         if (!errCode) {
            NEXUS_VideoFormat_GetInfo(settings.format, &info);
            int w = hwc2->ext->cfgs->w;
            int h = hwc2->ext->cfgs->h;
            float xdp = edid.maxHorizSize ? (w * 1000.0 / ((float)edid.maxHorizSize * 0.39370)) : 0.0;
            float ydp = edid.maxVertSize  ? (h * 1000.0 / ((float)edid.maxVertSize * 0.39370)) : 0.0;
            hwc2->ext->cfgs->xdp = (int)xdp;
            hwc2->ext->cfgs->ydp = (int)ydp;
         }
      }
   } else /* disconnected */ {
      if (hwc2->regCb[HWC2_CALLBACK_HOTPLUG-1].func != NULL) {
         ALOGV("[ext]: notify hotplug DISCONNECTED\n");
         HWC2_PFN_HOTPLUG f_hp = (HWC2_PFN_HOTPLUG) hwc2->regCb[HWC2_CALLBACK_HOTPLUG-1].func;
         f_hp(hwc2->regCb[HWC2_CALLBACK_HOTPLUG-1].data,
              (hwc2_display_t)(intptr_t)hwc2->ext,
              (int)HWC2_CONNECTION_DISCONNECTED);
      }
   }
}

static void hwc2_bcm_open(
   struct hwc2_bcm_device_t *hwc2) {

   pthread_attr_t attr;
   pthread_mutexattr_t mattr;
   NEXUS_MemoryStatus status;
   NEXUS_ClientConfiguration nxCliCfg;
   char buf[128];
   b_refsw_client_client_configuration cliCfg;
   NEXUS_Graphics2DOpenSettings g2dOCfg;
   NEXUS_Graphics2DSettings g2dCfg;
   NEXUS_Error rc;

   hwc2_setup_memif(hwc2);

   NEXUS_Platform_GetClientConfiguration(&nxCliCfg);
   NEXUS_Heap_GetStatus(NEXUS_Platform_GetFramebufferHeap(0), &status);
   NEXUS_Heap_ToString(&status, buf, sizeof(buf));
   ALOGI("[hwc2]: fb0 heap: %s", buf);
   NEXUS_Heap_GetStatus(nxCliCfg.heap[NXCLIENT_DYNAMIC_HEAP], &status);
   NEXUS_Heap_ToString(&status, buf, sizeof(buf));
   ALOGI("[hwc2]: d-cma heap: %s", buf);

   pthread_mutexattr_init(&mattr);
   pthread_mutex_init(&hwc2->mtx_pwr, &mattr);
   pthread_mutex_init(&hwc2->mtx_g2d, &mattr);
   pthread_mutexattr_destroy(&mattr);

   BKNI_CreateEvent(&hwc2->recycled);
   BKNI_CreateEvent(&hwc2->recycle);
   pthread_attr_init(&attr);
   pthread_create(&hwc2->recycle_task, &attr, hwc2_recycle_task, (void *)hwc2);
   pthread_attr_destroy(&attr);
   pthread_setname_np(hwc2->recycle_task, "hwc2_rec");

   BKNI_CreateEvent(&hwc2->vsync);
   pthread_attr_init(&attr);
   pthread_create(&hwc2->vsync_task, &attr, hwc2_vsync_task, (void *)hwc2);
   pthread_attr_destroy(&attr);
   pthread_setname_np(hwc2->vsync_task, "hwc2_syn");

   hwc2->hp = new Hwc2HP_sp;
   if (hwc2->hp != NULL) {
      hwc2->hp->get()->register_notify(&hwc2_hp_ntfy, (void *)hwc2);
   }
   hwc2->dc = new Hwc2DC_sp;
   if (hwc2->dc != NULL) {
      hwc2->dc->get()->register_notify(&hwc2_dc_ntfy, (void *)hwc2);
   }
   hwc2->hb = new HwcBinder_wrap;
   if (hwc2->hb != NULL) {
      hwc2->hb->get()->register_notify(&hwc2_hb_ntfy, (void *)hwc2);
   }

   hwc2->nxipc = NexusIPCClientFactory::getClient("hwc2");
   if (hwc2->nxipc == NULL) {
      LOG_ALWAYS_FATAL("failed to instantiate nexus ipc.");
      return;
   }
   memset(&cliCfg, 0, sizeof(cliCfg));
   cliCfg.standbyMonitorCallback = hwc2_stdby_mon;
   cliCfg.standbyMonitorContext  = (void *)hwc2;
   hwc2->nxcc = hwc2->nxipc->createClientContext(&cliCfg);
   if (hwc2->nxcc == NULL) {
      LOG_ALWAYS_FATAL("failed to instantiate nexus client context.");
      return;
   }

   hwc2_setup_ext(hwc2);

   if (hwc2->hp) {
      hwc2->nxipc->addHdmiHotplugEventListener(0, hwc2->hp->get());
   }
   if (hwc2->dc) {
      hwc2->nxipc->addDisplaySettingsChangedEventListener(0, hwc2->dc->get());
   }

   BKNI_CreateEvent(&hwc2->g2dchk);
   NEXUS_Graphics2D_GetDefaultOpenSettings(&g2dOCfg);
   g2dOCfg.compatibleWithSurfaceCompaction = false;
   hwc2->hg2d = NEXUS_Graphics2D_Open(NEXUS_ANY_ID, &g2dOCfg);
   if (hwc2->hg2d == NULL) {
      LOG_ALWAYS_FATAL("failed to instantiate g2d context.");
      return;
   }
   NEXUS_Graphics2D_GetSettings(hwc2->hg2d, &g2dCfg);
   g2dCfg.pollingCheckpoint           = false;
   g2dCfg.checkpointCallback.callback = hwc2_chkpt_cb;
   g2dCfg.checkpointCallback.context  = (void *)hwc2;
   rc = NEXUS_Graphics2D_SetSettings(hwc2->hg2d, &g2dCfg);
   if (rc) {
      LOG_ALWAYS_FATAL("failed to instantiate composition engine.");
      return;
   }
   NEXUS_Graphics2D_GetCapabilities(hwc2->hg2d, &hwc2->g2dc);
}

static int hwc2_open(
   const struct hw_module_t* module,
   const char* name,
   struct hw_device_t** device) {
   if (!strcmp(name, HWC_HARDWARE_COMPOSER)) {
      struct hwc2_bcm_device_t *dev;
      dev = (struct hwc2_bcm_device_t*)malloc(sizeof(*dev));
      memset(dev, 0, sizeof(*dev));

      dev->base.common.tag      = HARDWARE_DEVICE_TAG;
      dev->base.common.version  = HWC_DEVICE_API_VERSION_2_0;
      dev->base.common.module   = const_cast<hw_module_t*>(module);
      dev->base.common.close    = hwc2_close;
      dev->base.getCapabilities = hwc2_getCaps;
      dev->base.getFunction     = hwc2_getFncs;
      dev->magic                = HWC2_MAGIC;

      hwc2_bcm_open(dev);

      *device = &dev->base.common;
      return 0;
   } else {
      return -EINVAL;
   }
}

static struct hw_module_methods_t hwc2_mod_fncs = {
   .open  = hwc2_open,
};

typedef struct hwc2_module {
   struct hw_module_t common;
} hwc2_module_t;

hwc2_module_t HAL_MODULE_INFO_SYM = {
   .common = {
      .tag                = HARDWARE_MODULE_TAG,
      .module_api_version = HWC_MODULE_API_VERSION_0_1,
      .hal_api_version    = HARDWARE_HAL_API_VERSION,
      .id                 = HWC_HARDWARE_MODULE_ID,
      .name               = "hwcomposer2 for set-top-box platforms",
      .author             = "broadcom",
      .methods            = &hwc2_mod_fncs,
      .dso                = 0,
      .reserved           = {0}
   }
};
