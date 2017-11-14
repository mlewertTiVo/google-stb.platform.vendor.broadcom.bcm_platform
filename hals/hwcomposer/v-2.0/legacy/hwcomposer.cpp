/*
 * Copyright 2016-2017 The Android Open Source Project
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

// #define LOG_NDEBUG 0

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
/* hwc2 - including string functions. */
#define HWC2_INCLUDE_STRINGIFICATION
#define HWC2_USE_CPP11
#include <hardware/hwcomposer2.h>
#undef HWC2_INCLUDE_STRINGIFICATION
#undef HWC2_USE_CPP11
/* nexus integration. */
#include "nxwrap.h"
#include "nexus_surface_client.h"
#include "nexus_core_utils.h"
#include "nxclient.h"
#include "nxclient_config.h"
#include "bfifo.h"
#include "INxHpdEvtSrc.h"
#include "INxDspEvtSrc.h"
#include "nexus_platform_client.h"
#include "nexus_platform.h"
#include "nexus_display_dynrng.h"
/* sync framework/fences. */
#include "sync/sync.h"
#include "sw_sync.h"
/* binder integration.*/
#include "HwcCommon.h"
#include "Hwc.h"
#include "HwcListener.h"
#include "IHwc.h"
#include "HwcSvc.h"
/* buffer memory interface. */
#include "gralloc_priv.h"
#include "nx_ashmem.h"
/* last but not least... */
#include "hwcutils.h"
#include "legacy/hwc2.h"

const NEXUS_BlendEquation hwc2_a2n_col_be[4 /*hwc2_blend_mode_t*/] = {
   /* HWC2_BLEND_MODE_INVALID */ {NEXUS_BlendFactor_eZero, NEXUS_BlendFactor_eZero, false,
                                  NEXUS_BlendFactor_eZero, NEXUS_BlendFactor_eZero, false,
                                  NEXUS_BlendFactor_eZero},
   /* HWC2_BLEND_MODE_NONE */ {NEXUS_BlendFactor_eSourceColor, NEXUS_BlendFactor_eOne, false,
                               NEXUS_BlendFactor_eZero, NEXUS_BlendFactor_eZero, false,
                               NEXUS_BlendFactor_eZero},
   /* HWC2_BLEND_MODE_PREMULTIPLIED */ {NEXUS_BlendFactor_eSourceColor, NEXUS_BlendFactor_eOne, false,
                                        NEXUS_BlendFactor_eDestinationColor, NEXUS_BlendFactor_eInverseSourceAlpha, false,
                                        NEXUS_BlendFactor_eZero},
   /* HWC2_BLEND_MODE_COVERAGE */ {NEXUS_BlendFactor_eSourceColor, NEXUS_BlendFactor_eSourceAlpha, false,
                                   NEXUS_BlendFactor_eDestinationColor, NEXUS_BlendFactor_eInverseSourceAlpha, false,
                                   NEXUS_BlendFactor_eZero}
};

const NEXUS_BlendEquation hwc2_a2n_al_be[4 /*hwc2_blend_mode_t*/] = {
   /* HWC2_BLEND_MODE_INVALID */ {NEXUS_BlendFactor_eZero, NEXUS_BlendFactor_eZero, false,
                                  NEXUS_BlendFactor_eZero, NEXUS_BlendFactor_eZero, false,
                                  NEXUS_BlendFactor_eZero},
   /* HWC2_BLEND_MODE_NONE */ {NEXUS_BlendFactor_eSourceAlpha, NEXUS_BlendFactor_eOne, false,
                               NEXUS_BlendFactor_eZero, NEXUS_BlendFactor_eZero, false,
                               NEXUS_BlendFactor_eZero},
   /* HWC2_BLEND_MODE_PREMULTIPLIED */ {NEXUS_BlendFactor_eSourceAlpha, NEXUS_BlendFactor_eOne, false,
                                        NEXUS_BlendFactor_eDestinationAlpha, NEXUS_BlendFactor_eInverseSourceAlpha, false,
                                        NEXUS_BlendFactor_eZero},
   /* HWC2_BLEND_MODE_COVERAGE */ {NEXUS_BlendFactor_eSourceAlpha, NEXUS_BlendFactor_eOne, false,
                                   NEXUS_BlendFactor_eDestinationAlpha, NEXUS_BlendFactor_eInverseSourceAlpha, false,
                                   NEXUS_BlendFactor_eZero}
};

const NEXUS_BlendEquation hwc2_a2n_fal_be[4 /*hwc2_blend_mode_t*/] = {
   /* HWC2_BLEND_MODE_INVALID */ {NEXUS_BlendFactor_eZero, NEXUS_BlendFactor_eZero, false,
                                  NEXUS_BlendFactor_eZero, NEXUS_BlendFactor_eZero, false,
                                  NEXUS_BlendFactor_eZero},
   /* HWC2_BLEND_MODE_NONE */ {NEXUS_BlendFactor_eConstantAlpha, NEXUS_BlendFactor_eOne, false,
                               NEXUS_BlendFactor_eZero, NEXUS_BlendFactor_eZero, false,
                               NEXUS_BlendFactor_eZero},
   /* HWC2_BLEND_MODE_PREMULTIPLIED */ {NEXUS_BlendFactor_eConstantAlpha, NEXUS_BlendFactor_eOne, false,
                                        NEXUS_BlendFactor_eDestinationAlpha, NEXUS_BlendFactor_eInverseConstantAlpha, false,
                                        NEXUS_BlendFactor_eZero},
   /* HWC2_BLEND_MODE_COVERAGE */ {NEXUS_BlendFactor_eConstantAlpha, NEXUS_BlendFactor_eOne, false,
                                   NEXUS_BlendFactor_eSourceAlpha, NEXUS_BlendFactor_eInverseConstantAlpha, false,
                                   NEXUS_BlendFactor_eZero}
};

const NEXUS_BlendEquation hwc2_apmc_be = {
   NEXUS_BlendFactor_eSourceColor, NEXUS_BlendFactor_eConstantAlpha, false,
   NEXUS_BlendFactor_eZero, NEXUS_BlendFactor_eZero, false,
   NEXUS_BlendFactor_eZero
};

const NEXUS_BlendEquation hwc2_apma_be = {
   NEXUS_BlendFactor_eSourceAlpha, NEXUS_BlendFactor_eConstantAlpha, false,
   NEXUS_BlendFactor_eZero, NEXUS_BlendFactor_eZero, false,
   NEXUS_BlendFactor_eZero
};

struct hwc2_bcm_device_t {
   hwc2_device_t        base;
   uint32_t             magic;
   char                 dump[HWC2_MAX_DUMP];

   char                 memif[PROPERTY_VALUE_MAX];
   int                  memfd;
   bool                 rlpf[HWC2_VID_WIN];

   int32_t              lm;

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
   bool                 con;

   NxWrap               *nxipc;

   struct hwc2_reg_cb_t regCb[HWC2_MAX_REG_CB];
   struct hwc2_dsp_t    *vd;
   struct hwc2_dsp_t    *ext;

   BKNI_EventHandle             g2dchk;
   NEXUS_Graphics2DHandle       hg2d;
   pthread_mutex_t              mtx_g2d;
   NEXUS_Graphics2DCapabilities g2dc;
   bool                         sgl;
};

struct hwc2_dump_data_t {
   uint64_t          cmp;
   bool              final;
   int               lyr;
   uint16_t          h;
   uint16_t          w;
   NEXUS_PixelFormat fmt;
   uint8_t           *ptr;
   size_t            size;
};

static int hwc2_blit_yv12(
   struct hwc2_bcm_device_t* hwc2,
   NEXUS_SurfaceHandle d,
   struct hwc2_lyr_t *lyr,
   PSHARED_DATA shared,
   struct hwc2_dsp_t *dsp,
   enum hwc2_seeding_e *ms);

static int hwc2_blit_gpx(
   struct hwc2_bcm_device_t* hwc2,
   NEXUS_SurfaceHandle d,
   struct hwc2_lyr_t *lyr,
   PSHARED_DATA shared,
   struct hwc2_dsp_t *dsp,
   enum hwc2_seeding_e *ms,
   size_t cnt);

static uint32_t hwc2_fps2vsync(int fps) {
   switch (fps) {
   case 60: return 16666667;
   case 50: return 20000000;
   case 30: return 33333333;
   case 25: return 40000000;
   case 24: return 41666667;
   default: ALOGE("[hwc2_fps2vsync]: invalid fps: %d", fps); return 0;
   }
}

static int hwc2_vsync2fps(uint32_t vsync) {
   switch (vsync) {
   case 16666667: return 60;
   case 20000000: return 50;
   case 33333333: return 30;
   case 40000000: return 25;
   case 41666667: return 24;
   default: ALOGE("[hwc2_vsync2fps]: invalid vsync: %u", vsync); return 0;
   }
}

static void hwc2_eval_log(
   struct hwc2_bcm_device_t *hwc2) {

  if (hwc2) {
     hwc2->lm = property_get_int32(HWC2_LOG_GLB, 0);
  }
  if (hwc2->ext) {
     hwc2->ext->lm = property_get_int32(HWC2_LOG_EXT, 0);
  }
  if (hwc2->vd) {
     hwc2->vd->lm = property_get_int32(HWC2_LOG_VD, 0);
  }
  return;
}

static bool hwc2_enabled(
   enum hwc2_tweaks_e tweak) {

   bool r = false;

   switch (tweak) {
   case hwc2_tweak_fb_compressed:
      r = (bool)property_get_bool("ro.nx.hwc2.tweak.fbcomp", 0);
   break;
   case hwc2_tweak_pip_alpha_hole:
      r = !!HWC2_PAH;
   break;
   case hwc2_tweak_bypass_disable:
      r = (bool)property_get_bool("ro.nx.hwc2.tweak.nocb", 0);
   break;
   case hwc2_tweak_plm_off:
      r = (bool)property_get_bool("dyn.nx.hwc2.tweak.plmoff", 0);
   break;
   case hwc2_tweak_scale_gles:
      r = (bool)property_get_bool("dyn.nx.hwc2.tweak.sgles", 0);
   break;
   default:
   break;
   }

   return r;
}

static int32_t hwc2_setting(
   enum hwc2_tweaks_e tweak) {

   int32_t r = 0;

   switch (tweak) {
   case hwc2_tweak_eotf:
      r = property_get_int32("dyn.nx.hwc2.tweak.eotf", 0);
   break;
   case hwc2_tweak_dump_enabled:
      r = property_get_int32(HWC2_DUMP_SET, 0);
   break;
   case hwc2_tweak_dump_this:
      r = property_get_int32(HWC2_DUMP_NOW, 0);
   break;
   default:
   break;
   }

   return r;
}

static int nx2hu_bpp(
   NEXUS_PixelFormat nx) {

   switch (nx) {
   case NEXUS_PixelFormat_eA8_B8_G8_R8: /* fall-thru */
   case NEXUS_PixelFormat_eX8_B8_G8_R8: return 32;
   case NEXUS_PixelFormat_eR5_G6_B5: /* fall-thru */
   case NEXUS_PixelFormat_eY08_Cb8_Y18_Cr8: return 16;
   default: break;
   }

   return 0;
}

static void hwc2_dump_content(
   struct hwc2_dump_data_t *dump) {

   FILE *f = NULL;
   char name[512];
   /* what - 0 (default): skip.
    *        1 record only file title.
    *        2 record all.
    *        3 record only if filter match.
    */
   int32_t what = hwc2_setting(hwc2_tweak_dump_this);

   if (!what) {
      return;
   }

   if (dump->final) {
      sprintf(name, "%s/hwc2_%" PRIu64 "_fin__w%d_h%d_b%d.raw",
         HWC2_DUMP_LOC,
         dump->cmp,
         dump->w,
         dump->h,
         nx2hu_bpp(dump->fmt));
	} else {
      sprintf(name, "%s/hwc2_%" PRIu64 "_l%d__w%d_h%d_b%d.raw",
         HWC2_DUMP_LOC,
         dump->cmp,
         dump->lyr,
         dump->w,
         dump->h,
         nx2hu_bpp(dump->fmt));
   }

   f = fopen(name, "wb+");

   if (f != NULL) {
      if (what > 1) {
         if (dump->ptr != NULL && dump->size) {
            fwrite(dump->ptr, sizeof(uint8_t), dump->size, f);
         }
      }
      fclose(f);
   }
}

static void hwc2_eotf(
   struct hwc2_dsp_t *dsp) {

   NEXUS_HdmiOutputExtraSettings s;
   NEXUS_PlatformConfiguration p;
   NEXUS_Error e;
   NEXUS_HdmiOutputHandle hdmi;
   int32_t eotf = hwc2_setting(hwc2_tweak_eotf);

   if (!eotf) {
      return;
   }

   NEXUS_Platform_GetConfiguration(&p);
   hdmi = p.outputs.hdmi[0];
   if (!hdmi) {
      ALOGE("[eotf]: invalid hdmi output.");
      return;
   }

   if (pthread_mutex_lock(&dsp->mtx_cfg)) {
      return;
   }

   if (dsp->aCfg->hdr10) {
      NEXUS_HdmiOutput_GetExtraSettings(hdmi, &s);
      switch (eotf) {
         case 1:
            ALOGI("[eotf]: hdr10.");
            s.overrideDynamicRangeMasteringInfoFrame = true;
            s.dynamicRangeMasteringInfoFrame.eotf = NEXUS_VideoEotf_eHdr10;
         break;
         case 2:
            ALOGI("[eotf]: sdr.");
            s.overrideDynamicRangeMasteringInfoFrame = true;
            s.dynamicRangeMasteringInfoFrame.eotf = NEXUS_VideoEotf_eSdr;
         break;
         case 3:
         default:
            ALOGI("[eotf]: non-specific.");
            s.overrideDynamicRangeMasteringInfoFrame = false;
            s.dynamicRangeMasteringInfoFrame.eotf = NEXUS_VideoEotf_eInvalid;
         break;
      }
      e = NEXUS_HdmiOutput_SetExtraSettings(hdmi, &s);
   }
   pthread_mutex_unlock(&dsp->mtx_cfg);
}

static void hwc2_eval_plm(
   struct hwc2_bcm_device_t *hwc2) {

   struct hwc2_dsp_t *dsp;
   dsp = hwc2->ext;
   if (dsp != NULL) {
      NEXUS_DynamicRangeProcessingSettings d;
      bool noplm = hwc2_enabled(hwc2_tweak_plm_off);
      if (pthread_mutex_lock(&dsp->mtx_cfg)) {
         return;
      }
      if (noplm != dsp->aCfg->plm) {
         dsp->aCfg->plm = noplm;
         NEXUS_Display_GetGraphicsDynamicRangeProcessingSettings(&d);
         d.processingModes[NEXUS_DynamicRangeProcessingType_ePlm] =
            noplm ? NEXUS_DynamicRangeProcessingMode_eOff : NEXUS_DynamicRangeProcessingMode_eAuto;
         d.processingModes[NEXUS_DynamicRangeProcessingType_eDolbyVision] =
            noplm ? NEXUS_DynamicRangeProcessingMode_eOff : NEXUS_DynamicRangeProcessingMode_eAuto;
         d.processingModes[NEXUS_DynamicRangeProcessingType_eTechnicolorPrime] =
            noplm ? NEXUS_DynamicRangeProcessingMode_eOff : NEXUS_DynamicRangeProcessingMode_eAuto;
         NEXUS_Display_SetGraphicsDynamicRangeProcessingSettings(&d);
         NEXUS_VideoWindow_GetDynamicRangeProcessingSettings(0, &d);
         d.processingModes[NEXUS_DynamicRangeProcessingType_ePlm] =
            noplm ? NEXUS_DynamicRangeProcessingMode_eOff : NEXUS_DynamicRangeProcessingMode_eAuto;
         d.processingModes[NEXUS_DynamicRangeProcessingType_eDolbyVision] =
            noplm ? NEXUS_DynamicRangeProcessingMode_eOff : NEXUS_DynamicRangeProcessingMode_eAuto;
         d.processingModes[NEXUS_DynamicRangeProcessingType_eTechnicolorPrime] =
            noplm ? NEXUS_DynamicRangeProcessingMode_eOff : NEXUS_DynamicRangeProcessingMode_eAuto;
         NEXUS_VideoWindow_SetDynamicRangeProcessingSettings(0, &d);
         NEXUS_VideoWindow_GetDynamicRangeProcessingSettings(1, &d);
         d.processingModes[NEXUS_DynamicRangeProcessingType_ePlm] =
            noplm ? NEXUS_DynamicRangeProcessingMode_eOff : NEXUS_DynamicRangeProcessingMode_eAuto;
         d.processingModes[NEXUS_DynamicRangeProcessingType_eDolbyVision] =
            noplm ? NEXUS_DynamicRangeProcessingMode_eOff : NEXUS_DynamicRangeProcessingMode_eAuto;
         d.processingModes[NEXUS_DynamicRangeProcessingType_eTechnicolorPrime] =
            noplm ? NEXUS_DynamicRangeProcessingMode_eOff : NEXUS_DynamicRangeProcessingMode_eAuto;
         NEXUS_VideoWindow_SetDynamicRangeProcessingSettings(1, &d);
      }
      pthread_mutex_unlock(&dsp->mtx_cfg);
   }
}

static NEXUS_PixelFormat gr2nx_pixel(
   int gr) {

   switch (gr) {
   case HAL_PIXEL_FORMAT_RGBA_8888: return NEXUS_PixelFormat_eA8_B8_G8_R8;
   case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED: /* fall through. */
   case HAL_PIXEL_FORMAT_RGBX_8888: return NEXUS_PixelFormat_eX8_B8_G8_R8;
   case HAL_PIXEL_FORMAT_RGB_888:   return NEXUS_PixelFormat_eX8_B8_G8_R8;
   case HAL_PIXEL_FORMAT_RGB_565:   return NEXUS_PixelFormat_eR5_G6_B5;
   case HAL_PIXEL_FORMAT_YV12: /* fall-thru */
   case HAL_PIXEL_FORMAT_YCbCr_420_888: return NEXUS_PixelFormat_eY08_Cb8_Y18_Cr8;
   default:                         break;
   }

   ALOGE("[gr2nx]: unsupported format %d", gr);
   return NEXUS_PixelFormat_eUnknown;
}

static hwc2_dsp_t *hwc2_hnd2dsp(
   struct hwc2_bcm_device_t *hwc2,
   hwc2_display_t display,
   uint32_t *kind) {

   struct hwc2_dsp_t *dsp = NULL;

   if ((hwc2->ext != NULL) &&
       (hwc2->ext == (struct hwc2_dsp_t *)(intptr_t)display)) {
      *kind = HWC2_DSP_EXT;
      return hwc2->ext;
   }
   if ((hwc2->vd != NULL) &&
       (hwc2->vd == (struct hwc2_dsp_t *)(intptr_t)display)) {
      *kind = HWC2_DSP_VD;
      return hwc2->vd;
   }

   return NULL;
}

static void hwc2_cfg_collect(
   struct hwc2_dsp_t *dsp,
   NxClient_DisplaySettings *s) {

   NEXUS_DisplayCapabilities caps;
   NxClient_DisplayStatus status;
   NEXUS_VideoFormatInfo vi;
   struct hwc2_dsp_cfg_t *cfg, *cfg_c = NULL;
   int i, j;

   NEXUS_VideoFormat ordered_fps[] = {
      NEXUS_VideoFormat_e4096x2160p60hz,
      NEXUS_VideoFormat_e3840x2160p60hz,
      NEXUS_VideoFormat_e1080p,
      NEXUS_VideoFormat_e720p,
      NEXUS_VideoFormat_e4096x2160p50hz,
      NEXUS_VideoFormat_e3840x2160p50hz,
      NEXUS_VideoFormat_e1080p50hz,
      NEXUS_VideoFormat_e720p50hz,
      NEXUS_VideoFormat_e4096x2160p30hz,
      NEXUS_VideoFormat_e3840x2160p30hz,
      NEXUS_VideoFormat_e1080p30hz,
      NEXUS_VideoFormat_e720p30hz,
      NEXUS_VideoFormat_e4096x2160p25hz,
      NEXUS_VideoFormat_e3840x2160p25hz,
      NEXUS_VideoFormat_e1080p25hz,
      NEXUS_VideoFormat_e720p25hz,
      NEXUS_VideoFormat_e4096x2160p24hz,
      NEXUS_VideoFormat_e3840x2160p24hz,
      NEXUS_VideoFormat_e1080p24hz,
      NEXUS_VideoFormat_e720p24hz,
      NEXUS_VideoFormat_eUnknown,
   };

   NxClient_GetDisplayStatus(&status);
   NEXUS_GetDisplayCapabilities(&caps);

   if (pthread_mutex_lock(&dsp->mtx_cfg)) {
      return;
   }

   cfg = dsp->cfgs;
   while (cfg != NULL) {
      cfg_c = cfg->next;
      free(cfg);
      cfg = cfg_c;
   }
   dsp->aCfg = NULL;
   dsp->cfgs = NULL;

   /* first, we populate the aCfg which is always the currently connected by
    * default.
    */
   cfg = (struct hwc2_dsp_cfg_t *)malloc(sizeof(struct hwc2_dsp_cfg_t));
   if (cfg) {
      NEXUS_VideoFormat_GetInfo(s->format, &vi);
      cfg->next  = NULL;
      cfg->w     = (vi.width > HWC2_FB_MAX_W) ? HWC2_FB_MAX_W : vi.width;
      cfg->ew    = vi.width;
      cfg->h     = (vi.height > HWC2_FB_MAX_H) ? HWC2_FB_MAX_H : vi.height;
      cfg->eh    = vi.height;
      cfg->vsync = hwc2_fps2vsync(vi.verticalFreq/100);

      dsp->cfgs = cfg;
      dsp->aCfg = dsp->cfgs;

      ALOGI_IF((dsp->lm & LOG_CFGS_DEBUG),
               "[%s]:[acfg]:%" PRIu64 ":%ux%u:(%ux%u):%ufps\n",
               (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext",
               (uint64_t)(intptr_t)dsp,
               cfg->w, cfg->h, cfg->ew, cfg->eh, hwc2_vsync2fps(cfg->vsync));
   }
   cfg_c = dsp->cfgs;

   /* now we grow with all the format supported part of our list.
    */
   i = 0;
   j = 0;
   while (ordered_fps[i] != NEXUS_VideoFormat_eUnknown) {
      if (status.hdmi.status.videoFormatSupported[ordered_fps[i]] &&
          caps.displayFormatSupported[ordered_fps[i]] &&
          ordered_fps[i] != s->format) {
         cfg = (struct hwc2_dsp_cfg_t *)malloc(sizeof(struct hwc2_dsp_cfg_t));
         if (cfg) {
            NEXUS_VideoFormat_GetInfo(ordered_fps[i], &vi);
            j++;
            cfg->next  = NULL;
            cfg->w     = (vi.width > HWC2_FB_MAX_W) ? HWC2_FB_MAX_W : vi.width;
            cfg->ew    = vi.width;
            cfg->h     = (vi.height > HWC2_FB_MAX_H) ? HWC2_FB_MAX_H : vi.height;
            cfg->eh    = vi.height;
            cfg->vsync = hwc2_fps2vsync(vi.verticalFreq/100);

            ALOGI_IF((dsp->lm & LOG_CFGS_DEBUG),
                     "[%s]:[cfg:%d]:%" PRIu64 ":%ux%u:(%ux%u):%ufps\n",
                     (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext",
                     j, (uint64_t)(intptr_t)dsp,
                     cfg->w, cfg->h, cfg->ew, cfg->eh, hwc2_vsync2fps(cfg->vsync));

            cfg_c->next = cfg;
            cfg_c = cfg;
         }
      }
      i++;
   }

   pthread_mutex_unlock(&dsp->mtx_cfg);
}

static void hwc2_hdmi_collect(
   struct hwc2_dsp_t *dsp,
   NEXUS_HdmiOutputHandle hdmi,
   NxClient_DisplaySettings *s) {

   NEXUS_HdmiOutputBasicEdidData bedid;
   NEXUS_HdmiOutputEdidData edid;
   NEXUS_VideoFormatInfo i;
   NEXUS_Error e;
   struct hwc2_dsp_cfg_t *cfg;

   if (pthread_mutex_lock(&dsp->mtx_cfg)) {
      return;
   }

   e = NEXUS_HdmiOutput_GetBasicEdidData(hdmi, &bedid);
   if (!e) {
      NEXUS_VideoFormat_GetInfo(s->format, &i);
      cfg = dsp->cfgs;
      while (cfg != NULL) {
         int w = cfg->w;
         int h = cfg->h;
         float xdp = bedid.maxHorizSize ? (w * 1000.0 / ((float)bedid.maxHorizSize * 0.39370)) : 0.0;
         float ydp = bedid.maxVertSize  ? (h * 1000.0 / ((float)bedid.maxVertSize * 0.39370)) : 0.0;
         cfg->xdp = (int)xdp;
         cfg->ydp = (int)ydp;
         cfg = cfg->next;
      }
   }

   e = NEXUS_HdmiOutput_GetEdidData(hdmi, &edid);
   cfg = dsp->cfgs;
   while (cfg != NULL) {
      cfg->hdr10 = false;
      cfg->hlg   = false;
      if (!e) {
         if (edid.hdrdb.valid) {
            cfg->hdr10 = edid.hdrdb.eotfSupported[NEXUS_VideoEotf_eHdr10];
            cfg->hlg   = edid.hdrdb.eotfSupported[NEXUS_VideoEotf_eHlg];
         }
      }
      cfg = cfg->next;
   }

   pthread_mutex_unlock(&dsp->mtx_cfg);
}

status_t Hwc2HP::onHpd(
   bool connected) {

   if (cb) {
      cb(cb_data, connected);
   }
   return NO_ERROR;
}

status_t Hwc2DC::onDsp() {

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
         ALOGE("[chpt]: time out waiting for %d ms", HWC2_CHKPT_TO);
         return -1;
      }
   break;
   default:
      ALOGE("[chpt]: checkpoint error: %d", rc);
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
         if (!pthread_mutex_lock(&hwc2->ext->u.ext.mtx_fbs)) {
            for (i = 0; i < HWC2_FBS_NUM; i++) {
               if (hwc2->ext->u.ext.fbs[i].fd != HWC2_INVALID) {
                  close(hwc2->ext->u.ext.fbs[i].fd);
                  hwc2->ext->u.ext.fbs[i].fd = HWC2_INVALID;
               }
               if (hwc2->ext->u.ext.fbs[i].s) {
                  NEXUS_Surface_Destroy(hwc2->ext->u.ext.fbs[i].s);
                  hwc2->ext->u.ext.fbs[i].s = NULL;
               }
            }
            pthread_mutex_unlock(&hwc2->ext->u.ext.mtx_fbs);
         }
      }
      NEXUS_SurfaceClient_GetSettings(hwc2->ext->u.ext.sch, &sCli);
      sCli.recycled.callback      = hwc2_recycle_cb;
      sCli.recycled.context       = (void *)hwc2;
      sCli.vsync.callback         = hwc2_vsync_cb;
      sCli.vsync.context          = (void *)hwc2;
      sCli.allowCompositionBypass = (cb == cbs_e_bypass) ? true : false;
      NEXUS_SurfaceClient_SetSettings(hwc2->ext->u.ext.sch, &sCli);
   }

   for (i = 0; i < HWC2_FBS_NUM; i++) {
      NEXUS_SurfaceCreateSettings scs;
      NEXUS_HeapHandle ch = NULL;
      NEXUS_MemoryBlockHandle bh = NULL;
      bool dh = false;
      NEXUS_Surface_GetDefaultCreateSettings(&scs);
      scs.width       = property_get_int32(HWC2_EXT_NFB_W, HWC2_FB_MAX_W);
      scs.height      = property_get_int32(HWC2_EXT_NFB_H, HWC2_FB_MAX_H);
      scs.pixelFormat = hwc2_enabled(hwc2_tweak_fb_compressed)?
                           NEXUS_PixelFormat_eCompressed_A8_R8_G8_B8:
                           NEXUS_PixelFormat_eA8_B8_G8_R8;
      scs.pitch       = (scs.pixelFormat == NEXUS_PixelFormat_eA8_B8_G8_R8) ?
                           scs.width * 4 : ((scs.width + 3) / 4) * 8;
      if (cb == cbs_e_bypass) {
         scs.heap = NEXUS_Platform_GetFramebufferHeap(0);
      } else {
         scs.heap = cCli.heap[NXCLIENT_DYNAMIC_HEAP];
         dh = true;
      }
      ch = scs.heap;
      hwc2->ext->u.ext.fbs[i].s = NULL;
      bh = hwc_block_create(&scs, hwc2->memif, dh, &hwc2->ext->u.ext.fbs[i].fd);
      if (bh != NULL) {
         scs.pixelMemory = bh;
         scs.heap = NULL;
         hwc2->ext->u.ext.fbs[i].s = hwc_surface_create(&scs, dh);
      }
      ALOGI("[ext]:fb:%d::%dx%d::%s:%p:%s -> %p::%d (b:%p)",
            i, scs.width, scs.height, dh?"d-cma":"gfx", ch,
            hwc2_enabled(hwc2_tweak_fb_compressed)?"comp":"full",
            hwc2->ext->u.ext.fbs[i].s, hwc2->ext->u.ext.fbs[i].fd, bh);
   }
   hwc2->ext->u.ext.bfb = true;

   if (hwc2_enabled(hwc2_tweak_fb_compressed)) {
      NEXUS_SurfaceCreateSettings scs;
      NEXUS_MemoryBlockHandle bh = NULL;
      bool dh = true;
      NEXUS_Surface_GetDefaultCreateSettings(&scs);
      scs.width       = property_get_int32(HWC2_EXT_NFB_W, HWC2_FB_MAX_W);
      scs.height      = property_get_int32(HWC2_EXT_NFB_H, HWC2_FB_MAX_H);
      scs.pitch       = scs.width * 4;
      scs.pixelFormat = NEXUS_PixelFormat_eA8_B8_G8_R8;
      scs.heap        = cCli.heap[NXCLIENT_DYNAMIC_HEAP];

      hwc2->ext->u.ext.yvi.s = NULL;
      bh = hwc_block_create(&scs, hwc2->memif, dh, &hwc2->ext->u.ext.yvi.fd);
      if (bh != NULL) {
         scs.pixelMemory = bh;
         scs.heap = NULL;
         hwc2->ext->u.ext.yvi.s = hwc_surface_create(&scs, dh);
      }
      ALOGI("[ext]:yvi::%dx%d::%s -> %p::%d (b:%p)",
            scs.width, scs.height, dh?"d-cma":"gfx",
            hwc2->ext->u.ext.yvi.s, hwc2->ext->u.ext.yvi.fd, bh);
   }

   {
      NEXUS_SurfaceCreateSettings scs;
      NEXUS_MemoryBlockHandle bh = NULL;
      bool dh = true;
      NEXUS_Surface_GetDefaultCreateSettings(&scs);
      scs.width       = property_get_int32(HWC2_EXT_NFB_W, HWC2_FB_MAX_W);
      scs.height      = property_get_int32(HWC2_EXT_NFB_H, HWC2_FB_MAX_H);
      scs.pitch       = scs.width * 4;
      scs.pixelFormat = NEXUS_PixelFormat_eA8_B8_G8_R8;
      scs.heap        = cCli.heap[NXCLIENT_DYNAMIC_HEAP];

      hwc2->ext->u.ext.icb.s = NULL;
      bh = hwc_block_create(&scs, hwc2->memif, dh, &hwc2->ext->u.ext.icb.fd);
      if (bh != NULL) {
         scs.pixelMemory = bh;
         scs.heap = NULL;
         hwc2->ext->u.ext.icb.s = hwc_surface_create(&scs, dh);
      }
      ALOGI("[ext]:icb::%dx%d::%s -> %p::%d (b:%p)",
            scs.width, scs.height, dh?"d-cma":"gfx",
            hwc2->ext->u.ext.icb.s, hwc2->ext->u.ext.icb.fd, bh);
   }

   if (!pthread_mutex_lock(&hwc2->ext->u.ext.mtx_fbs)) {
      /* bfifo uses the cached version to preserve the original mapping correctly. */
      memcpy(&hwc2->ext->u.ext.fbs_c, &hwc2->ext->u.ext.fbs, sizeof(hwc2->ext->u.ext.fbs_c));
      BFIFO_INIT(&hwc2->ext->u.ext.fb, hwc2->ext->u.ext.fbs_c, HWC2_FBS_NUM);
      BFIFO_WRITE_COMMIT(&hwc2->ext->u.ext.fb, HWC2_FBS_NUM);
      pthread_mutex_unlock(&hwc2->ext->u.ext.mtx_fbs);
   }
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
   ALOGW("[ext]:[inv-fb]: surface %p\n", s);
   return HWC2_INVALID;
}

static int32_t hwc2_ret_fence(
   struct hwc2_dsp_t *dsp) {

   struct sync_fence_info_data info;
   int f = HWC2_INVALID;

   if (dsp->cmp_tl == HWC2_INVALID) {
      return HWC2_INVALID;
   }

   snprintf(info.name, sizeof(info.name), "hwc2_%s_%" PRIu64 "", dsp->name, dsp->pres);
   f = sw_sync_fence_create(dsp->cmp_tl, info.name, dsp->pres);
   if (f < 0) {
      ALOGW("[%s]:[inv-rel-add]:%" PRIu64 ":%d:'%s' @%" PRId64 "\n",
            (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext",
            (uint64_t)(intptr_t)dsp, dsp->cmp_tl, info.name, hwc2_tick());
      return HWC2_INVALID;
   }
   ALOGI_IF((dsp->lm & LOG_FENCE_DEBUG),
            "[%s]:[rel-add]:%" PRIu64 ":%d:'%s' @%" PRId64 "\n",
            (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext",
            (uint64_t)(intptr_t)dsp, dsp->cmp_tl, info.name, hwc2_tick());
   return f;
}

static void hwc2_ret_inc(
   struct hwc2_dsp_t *dsp,
   unsigned int inc) {

   if (dsp->cmp_tl == HWC2_INVALID) {
      return;
   }
   sw_sync_timeline_inc(dsp->cmp_tl, inc);
   ALOGI_IF((dsp->lm & LOG_FENCE_DEBUG),
            "[%s]:[rel-inc]:%" PRIu64 ":%d:%d @%" PRId64 "\n",
            (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext",
            (uint64_t)(intptr_t)dsp, dsp->cmp_tl, inc, hwc2_tick());
}

static void hwc2_recycle(
   struct hwc2_bcm_device_t *hwc2) {

   NEXUS_SurfaceHandle s[HWC2_FBS_NUM];
   NEXUS_Error rc;
   int fd;
   size_t ns = 0, i;

   NEXUS_SurfaceClient_RecycleSurface(hwc2->ext->u.ext.sch, s, HWC2_FBS_NUM, &ns);
   if (ns) {
      if (!pthread_mutex_lock(&hwc2->ext->u.ext.mtx_fbs)) {
         for (i = 0; i < ns; i++) {
            fd = hwc2_ext_fb_valid_l(hwc2, s[i]);
            if (fd != HWC2_INVALID) {
               struct hwc2_fb_t *e = BFIFO_WRITE(&hwc2->ext->u.ext.fb);
               e->s  = s[i];
               e->fd = fd;
               BFIFO_WRITE_COMMIT(&hwc2->ext->u.ext.fb, 1);
            } else {
               ALOGW("[ext]: invalid recycled surface %p!", s[i]);
            }
         }
         pthread_mutex_unlock(&hwc2->ext->u.ext.mtx_fbs);
      }
   }
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
      if (!pthread_mutex_lock(&hwc2->mtx_pwr)) {
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
      }
   } while(!hwc2->vsync_exit);

   return NULL;
}

static NEXUS_SurfaceHandle hwc2_ext_fb_get(
   struct hwc2_bcm_device_t *hwc2) {
   int nsc = 0;
   struct hwc2_fb_t *e = NULL;

   while (true) {
      if (!pthread_mutex_lock(&hwc2->ext->u.ext.mtx_fbs)) {
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
      } else {
         return NULL;
      }
   }

out_read:
   if (!pthread_mutex_lock(&hwc2->ext->u.ext.mtx_fbs)) {
      e = BFIFO_READ(&hwc2->ext->u.ext.fb);
      BFIFO_READ_COMMIT(&hwc2->ext->u.ext.fb, 1);
      pthread_mutex_unlock(&hwc2->ext->u.ext.mtx_fbs);
   }
out:
   return (e != NULL ? e->s : NULL);
}

static void hwc2_ext_fb_put(
   struct hwc2_bcm_device_t *hwc2,
   NEXUS_SurfaceHandle s) {

   int fd;
   if (s) {
      if (!pthread_mutex_lock(&hwc2->ext->u.ext.mtx_fbs)) {
         fd = hwc2_ext_fb_valid_l(hwc2, s);
         if (fd != HWC2_INVALID) {
            struct hwc2_fb_t *e = BFIFO_WRITE(&hwc2->ext->u.ext.fb);
            e->s  = s;
            e->fd = fd;
            BFIFO_WRITE_COMMIT(&hwc2->ext->u.ext.fb, 1);
            ALOGV("[ext]: put unused surface %p::%d", s, fd);
         } else {
            ALOGW("[ext]: invalid put surface %p!", s);
         }
         pthread_mutex_unlock(&hwc2->ext->u.ext.mtx_fbs);
      }
   }
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

static int32_t hwc2_cntLyr(
   struct hwc2_dsp_t *dsp) {

   int cnt = 0;
   struct hwc2_lyr_t *lyr = NULL;

   lyr = dsp->lyr;
   while (lyr != NULL) {
      cnt++;
      lyr = lyr->next;
   };

   return cnt;
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

static void hwc2_close_memif(
   struct hwc2_bcm_device_t *hwc2)
{
   if (hwc2->memfd != HWC2_INVALID) {
      close(hwc2->memfd);
      hwc2->memfd = HWC2_INVALID;
   }
}

static NEXUS_Error hwc2_ext_refcnt(
   struct hwc2_bcm_device_t* hwc2,
   NEXUS_MemoryBlockHandle bh,
   int cnt) {

   int mem_if = hwc2_open_memif(hwc2);
   if (mem_if != HWC2_INVALID) {
      struct nx_ashmem_ext_refcnt ashmem_ext_refcnt;
      memset(&ashmem_ext_refcnt, 0, sizeof(struct nx_ashmem_ext_refcnt));
      ashmem_ext_refcnt.hdl = (__u64)bh;
      ashmem_ext_refcnt.cnt = cnt;
      int ret = ioctl(mem_if, NX_ASHMEM_EXT_REFCNT, &ashmem_ext_refcnt);
      if (ret < 0) {
         int err = errno;
         ALOGE("[refcnt]: failed (%d:%d) for %p, count:0x%x", ret, err, bh, cnt);
         hwc2_close_memif(hwc2);
         return NEXUS_OS_ERROR;
      }
   } else {
      return NEXUS_INVALID_PARAMETER;
   }
   return NEXUS_SUCCESS;
}

static int hwc2_mem_lock(
   struct hwc2_bcm_device_t* hwc2,
   NEXUS_MemoryBlockHandle bh,
   void **addr) {

   NEXUS_Error rc = NEXUS_SUCCESS;
   NEXUS_Error ext = NEXUS_SUCCESS;

   if (bh) {
      ext = hwc2_ext_refcnt(hwc2, bh, NX_ASHMEM_REFCNT_ADD);
      if (ext == NEXUS_SUCCESS) {
         rc = NEXUS_MemoryBlock_Lock(bh, addr);
         if (rc != NEXUS_SUCCESS) {
            hwc2_ext_refcnt(hwc2, bh, NX_ASHMEM_REFCNT_REM);
            /* unlock only if needed. */
            if (rc == BERR_NOT_SUPPORTED) {
               NEXUS_MemoryBlock_Unlock(bh);
            }
            /* return generic error to caller. */
            rc = NEXUS_NOT_INITIALIZED;
         }
      } else {
         /* lock never took place, no unlock needed. */
         rc = NEXUS_NOT_INITIALIZED;
      }
      if (rc != NEXUS_SUCCESS) {
         *addr = NULL;
      }
   } else {
      *addr = NULL;
      rc = NEXUS_NOT_INITIALIZED;
   }
   return (int)rc;
}

static int hwc2_mem_unlock(
   struct hwc2_bcm_device_t* hwc2,
   NEXUS_MemoryBlockHandle bh) {

   if (bh) {
      NEXUS_MemoryBlock_Unlock(bh);
      hwc2_ext_refcnt(hwc2, bh, NX_ASHMEM_REFCNT_REM);
   }
   return (int)NEXUS_SUCCESS;
}

static void hwc2_getCaps(
   struct hwc2_device* device,
   uint32_t* outCount,
   int32_t* /*hwc2_capability_t*/ outCapabilities) {

   if (device == NULL) {
      return;
   }

   // HWC2_CAPABILITY_SIDEBAND_STREAM               -> SUPPORTED.
   // HWC2_CAPABILITY_SKIP_CLIENT_COLOR_TRANSFORM   -> TODO??
   // HWC2_CAPABILITY_PRESENT_FENCE_IS_NOT_RELIABLE -> DO NOT USE.
   // HWC2_CAPABILITY_SKIP_VALIDATE                 -> NOT REALLY.

   if (outCount != NULL) {
      *outCount = 1;
   }
   if (outCapabilities != NULL) {
      outCapabilities[0] = HWC2_CAPABILITY_SIDEBAND_STREAM;
   }
   return;
}

static enum hwc2_cbs_e hwc2_want_comp_bypass(
   NxClient_DisplaySettings *settings) {

   NxClient_GetDisplaySettings(settings);
   switch (settings->format) {
   case NEXUS_VideoFormat_e1080i:
   case NEXUS_VideoFormat_e1080i50hz:
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
   case NEXUS_VideoFormat_e480p:
   case NEXUS_VideoFormat_e576p:
      return hwc2_cbs_e::cbs_e_nscfb;
   break;
   default:
   break;
   }
   return hwc2_enabled(hwc2_tweak_bypass_disable) ?
             hwc2_cbs_e::cbs_e_nscfb : hwc2_cbs_e::cbs_e_bypass;
}

static int hwc2_fps2igrp(
   uint32_t fps) {
   switch (fps) {
   case 60: return 0;
   case 50: return 1;
   case 30: return 2;
   case 25: return 3;
   case 24: return 4;
   default: return 5;
   }
}

#define AJOUTE_W_H(w,h) ((w)+(h))
static void hwc2_set_acfg_l(
   struct hwc2_dsp_t *dsp) {
   NxClient_DisplaySettings settings;

   NEXUS_VideoFormat fmt = NEXUS_VideoFormat_eUnknown;
   NxClient_GetDisplaySettings(&settings);

   NEXUS_VideoFormat ordered_720_grp[] = {
      NEXUS_VideoFormat_e720p,
      NEXUS_VideoFormat_e720p50hz,
      NEXUS_VideoFormat_e720p30hz,
      NEXUS_VideoFormat_e720p25hz,
      NEXUS_VideoFormat_e720p24hz,
      NEXUS_VideoFormat_eUnknown,
   };

   NEXUS_VideoFormat ordered_1080_grp[] = {
      NEXUS_VideoFormat_e1080p,
      NEXUS_VideoFormat_e1080p50hz,
      NEXUS_VideoFormat_e1080p30hz,
      NEXUS_VideoFormat_e1080p25hz,
      NEXUS_VideoFormat_e1080p24hz,
      NEXUS_VideoFormat_eUnknown,
   };

   NEXUS_VideoFormat ordered_2160_grp[] = {
      NEXUS_VideoFormat_e3840x2160p60hz,
      NEXUS_VideoFormat_e3840x2160p50hz,
      NEXUS_VideoFormat_e3840x2160p30hz,
      NEXUS_VideoFormat_e3840x2160p25hz,
      NEXUS_VideoFormat_e3840x2160p24hz,
      NEXUS_VideoFormat_eUnknown,
   };

   NEXUS_VideoFormat ordered_4k_grp[] = {
      NEXUS_VideoFormat_e4096x2160p60hz,
      NEXUS_VideoFormat_e4096x2160p50hz,
      NEXUS_VideoFormat_e4096x2160p30hz,
      NEXUS_VideoFormat_e4096x2160p25hz,
      NEXUS_VideoFormat_e4096x2160p24hz,
      NEXUS_VideoFormat_eUnknown,
   };

   int ifps = hwc2_fps2igrp(dsp->aCfg->vsync);
   switch (AJOUTE_W_H(dsp->aCfg->w, dsp->aCfg->h)) {
   case AJOUTE_W_H(1280,720): fmt = ordered_720_grp[ifps]; break;
   case AJOUTE_W_H(1920,1280): fmt = ordered_1080_grp[ifps]; break;
   case AJOUTE_W_H(3840,2160): fmt = ordered_2160_grp[ifps]; break;
   case AJOUTE_W_H(4096,2160): fmt = ordered_4k_grp[ifps]; break;
   default: break;
   }

   if (fmt != NEXUS_VideoFormat_eUnknown && settings.format != fmt) {
      ALOGI("[%s]:display-format::%d->%d",
         (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext",
         settings.format, fmt);
      settings.format = fmt;
      NxClient_SetDisplaySettings(&settings);
      /* the resulting display change callback should take care of updating
       * the composition mode.
       */
   }
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
         hstatus.connected = false;
         hdmi = NEXUS_HdmiOutput_Open(0+NEXUS_ALIAS_ID, NULL);
         if (hdmi) {
            NEXUS_HdmiOutput_GetStatus(hdmi, &hstatus);
            if (hstatus.connected) {
               NxClient_DisplaySettings settings;
               hwc2_want_comp_bypass(&settings);
               hwc2_cfg_collect(hwc2->ext, &settings);
               hwc2_hdmi_collect(hwc2->ext, hdmi, &settings);
               hwc2_eotf(hwc2->ext);
            }
            NEXUS_HdmiOutput_Close(hdmi);
         }
         if (hwc2->regCb[HWC2_CALLBACK_HOTPLUG-1].func != NULL) {
            if (!hwc2->ext->u.ext.rhpd) {
               hstatus.connected = true;
            }
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

   struct hwc2_dsp_t *dsp = NULL;
   uint32_t kind;
   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;

   ALOGV("-> %s:%" PRIu64 "\n",
     getFunctionDescriptorName(HWC2_FUNCTION_GET_DOZE_SUPPORT),
     display);

   if (device == NULL || hwc2->magic != HWC2_MAGIC) {
      ret = HWC2_ERROR_BAD_PARAMETER;
      goto out;
   }

   dsp = hwc2_hnd2dsp(hwc2, display, &kind);
   if (dsp == NULL) {
      ret = HWC2_ERROR_BAD_DISPLAY;
      goto out;
   }

   if (dsp->type == HWC2_DISPLAY_TYPE_PHYSICAL) {
      if (outSupport != NULL) {
         *outSupport = 0; /* TODO: support DOZE. */
      } else {
         ret = HWC2_ERROR_BAD_PARAMETER;
      }
   } else if (dsp->type == HWC2_DISPLAY_TYPE_VIRTUAL) {
      if (outSupport != NULL) {
         *outSupport = 0;
      } else {
         ret = HWC2_ERROR_BAD_PARAMETER;
      }
   } else {
      ret = HWC2_ERROR_BAD_DISPLAY;
   }

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_GET_DOZE_SUPPORT),
      display, getErrorName(ret));
   return ret;
}

static void hwc2_lyr_tl_inc(
   struct hwc2_dsp_t *dsp,
   uint32_t kind,
   uint64_t lyr,
   uint64_t hdl) {

   size_t num;
   if (kind == HWC2_DSP_EXT) {
      if (hdl == HWC2_MAGIC) {
         ALOGI_IF((dsp->lm & LOG_FENCE_DEBUG),
                  "[ext]:[dsp-tl-inc]:%" PRIu64 ":%d:1 @%" PRId64 "\n",
                  dsp->u.ext.ct.rtl.hdl, dsp->u.ext.ct.rtl.tl, hwc2_tick());
         sw_sync_timeline_inc(dsp->u.ext.ct.rtl.tl, 1);
         dsp->u.ext.ct.rtl.si++;
      } else {
         for (num = 0 ; num < HWC2_MAX_TL ; num++) {
            if (dsp->u.ext.rtl[num].hdl == hdl) {
               ALOGI_IF((dsp->lm & LOG_FENCE_DEBUG),
                        "[ext]:[tl-inc]:%" PRIu64 ":%" PRIu64 ":%d:1 @%" PRId64 "\n",
                        lyr, dsp->u.ext.rtl[num].hdl, dsp->u.ext.rtl[num].tl, hwc2_tick());
               sw_sync_timeline_inc(dsp->u.ext.rtl[num].tl, 1);
               dsp->u.ext.rtl[num].si++;
               return;
            }
         }
      }
   } else if (kind == HWC2_DSP_VD) {
      for (num = 0 ; num < HWC2_MAX_VTL ; num++) {
         if (dsp->u.vd.rtl[num].hdl == hdl) {
            ALOGI_IF((dsp->lm & LOG_FENCE_DEBUG),
                     "[vd]:[tl-inc]:%" PRIu64 ":%" PRIu64 ":%d:1 @%" PRId64 "\n",
                     lyr, dsp->u.vd.rtl[num].hdl, dsp->u.vd.rtl[num].tl, hwc2_tick());
            sw_sync_timeline_inc(dsp->u.vd.rtl[num].tl, 1);
            dsp->u.vd.rtl[num].si++;
            return;
         }
      }
   }
}

static void hwc2_fill_blend(
   struct hwc2_bcm_device_t* hwc2,
   NEXUS_SurfaceHandle s,
   uint32_t color,
   hwc2_blend_mode_t bm) {
   NEXUS_Error rc;
   if (s) {
      NEXUS_Graphics2DFillSettings fs;
      NEXUS_Graphics2D_GetDefaultFillSettings(&fs);
      fs.surface    = s;
      fs.color      = color;
      fs.colorOp    = NEXUS_FillOp_eBlend;
      fs.alphaOp    = NEXUS_FillOp_eUseBlendEquation;
      fs.alphaBlend = (bm == HWC2_BLEND_MODE_INVALID) ? hwc2_a2n_fal_be[HWC2_BLEND_MODE_NONE] : hwc2_a2n_fal_be[bm];
      if (pthread_mutex_lock(&hwc2->mtx_g2d)) {
         return;
      }
      NEXUS_Graphics2D_Fill(hwc2->hg2d, &fs);
      pthread_mutex_unlock(&hwc2->mtx_g2d);
      ALOGI_IF((hwc2->lm & LOG_SEED_DEBUG),
               "[seed]: %p fill/blend with color %08x\n", s, color);
   }
}

static void hwc2_fb_seed(
   struct hwc2_bcm_device_t* hwc2,
   NEXUS_SurfaceHandle s,
   uint32_t color) {
   NEXUS_Error rc;
   if (s) {
      NEXUS_Graphics2DFillSettings fs;
      NEXUS_Graphics2D_GetDefaultFillSettings(&fs);
      fs.surface = s;
      fs.color   = color;
      fs.colorOp = NEXUS_FillOp_eCopy;
      fs.alphaOp = NEXUS_FillOp_eCopy;
      if (pthread_mutex_lock(&hwc2->mtx_g2d)) {
         return;
      }
      NEXUS_Graphics2D_Fill(hwc2->hg2d, &fs);
      pthread_mutex_unlock(&hwc2->mtx_g2d);
      ALOGI_IF((hwc2->lm & LOG_SEED_DEBUG),
               "[seed]: %p with color %08x\n", s, color);
   }
}

static void hwc2_vd_cmp_frame(
   struct hwc2_bcm_device_t* hwc2,
   struct hwc2_frame_t *f) {

   NEXUS_SurfaceHandle d = NULL;
   int32_t i;
   size_t c = 0;
   struct hwc2_lyr_t *lyr;
   bool is_video;
   struct hwc2_dsp_t *dsp = hwc2->vd;
   NEXUS_Error dlrc = NEXUS_NOT_INITIALIZED, dlrcp = NEXUS_NOT_INITIALIZED;
   NEXUS_MemoryBlockHandle dbh = NULL, dbhp = NULL;
   PSHARED_DATA dshared = NULL;
   void *dmap = NULL;
   int blt, vl = 0;
   size_t ccli = 0;
   enum hwc2_seeding_e ms = hwc2_seeding_none;

   /* setup destination buffer for this frame. */
   private_handle_t::get_block_handles((private_handle_t *)f->otgt, &dbh, NULL);
   dlrc = hwc2_mem_lock(hwc2, dbh, &dmap);
   dshared = (PSHARED_DATA) dmap;
   if (dlrc || dshared == NULL) {
      ALOGE("[vd]:[out]:%" PRIu64 ":%" PRIu64 ": invalid dev-shared (obh:%p:dbh:%p)\n",
            dsp->pres, dsp->post, (private_handle_t *)f->otgt, dbh);
      if (dlrc == NEXUS_SUCCESS) {
         hwc2_mem_unlock(hwc2, dbh);
         dlrc = NEXUS_NOT_INITIALIZED;
      }
      goto out;
   }
   if (((private_handle_t *)f->otgt)->fmt_set != GR_NONE) {
      dbhp = (NEXUS_MemoryBlockHandle)dshared->container.block;
      dlrcp = hwc2_mem_lock(hwc2, dbhp, &dmap);
      if (dlrcp || dmap == NULL) {
         ALOGE("[vd]:[out]:%" PRIu64 ":%" PRIu64 ": invalid dev-phys (obh:%p:dbh:%p:dbhp:%p)\n",
               dsp->pres, dsp->post, (private_handle_t *)f->otgt, dbh, dbhp);
         if (dlrc == NEXUS_SUCCESS) {
            hwc2_mem_unlock(hwc2, dbh);
            dlrc = NEXUS_NOT_INITIALIZED;
         }
         if (dlrcp == NEXUS_SUCCESS) {
            hwc2_mem_unlock(hwc2, dbhp);
            dlrcp = NEXUS_NOT_INITIALIZED;
         }
         goto out;
      }
   }

   d = hwc_to_nsc_surface(
         dshared->container.width, dshared->container.height,
         ((private_handle_t *)f->otgt)->oglStride, gr2nx_pixel(dshared->container.format),
         dshared->container.block, 0);
   if (d == NULL) {
      ALOGE("[vd]:[out]:%" PRIu64 ":%" PRIu64 ": cannot associate output surface.\n",
             dsp->pres, dsp->post);
      goto out_error;
   }

   /* wait for output buffer to be ready to compose into. */
   if (f->oftgt >= 0) {
      vl = sync_wait(f->oftgt, HWC2_SYNC_TO);
      close(f->oftgt);
      f->oftgt = HWC2_INVALID;
   }
   if (vl == HWC2_INVALID) {
      ALOGE("[vd]:[out]:%" PRIu64 ":%" PRIu64 ": sync_wait error on output buffer.",
             dsp->pres, dsp->post);
      goto out_error;
   }

   for (i = 0; i < f->cnt; i++) {
      vl = 0;
      lyr = &f->lyr[i];
      /* [i]. wait as needed.
       */
      if (lyr->cCli == HWC2_COMPOSITION_CLIENT) {
         /* ...wait on fence as needed.
          * note: this is the client target, so the fence comes from the
          *       display client target and not the layer buffer.
          */
         if (f->tgt == NULL && f->ftgt >= 0) {
            close(f->ftgt);
         } else if (f->ftgt >= 0) {
            vl = sync_wait(f->ftgt, HWC2_SYNC_TO);
            close(f->ftgt);
            f->ftgt = HWC2_INVALID;
         }
      } else if (lyr->af >= 0) {
         vl = sync_wait(lyr->af, HWC2_SYNC_TO);
         close(lyr->af);
         lyr->af = HWC2_INVALID;
      }
      /* [ii]. compose.
       */
      switch(lyr->cCli) {
      case HWC2_COMPOSITION_SOLID_COLOR:
         {
            uint8_t a = lyr->sc.a;
            if (lyr->al != 1.0) {
               float fa = fmax(0.0, fmin(1.0, lyr->al));
               uint32_t al = floor(fa == 1.0 ? 255 : fa * 256.0);
               a = (uint8_t)al;
            }
            uint32_t color = (a<<24 | lyr->sc.r<<16 | lyr->sc.g<<8 | lyr->sc.b);
            if ((color != HWC2_TRS) &&
                   ((!c && (color == HWC2_OPQ)) /* background seeding. */
                 || (c && (color != HWC2_OPQ))) /* shading. */
               ) {
               hwc2_fill_blend(hwc2, d, color, lyr->bm);
               hwc2_chkpt(hwc2);
               /* [iii]. count of composed layers. */
               c++;
            }
            ALOGI_IF((dsp->lm & LOG_COMP_DEBUG),
                     "[vd]:[frame]:%" PRIu64 ":%" PRIu64 ": solid color (%02x:%02x:%02x:%02x):%08x (%zu)\n",
                     dsp->pres, dsp->post, lyr->sc.r, lyr->sc.g, lyr->sc.b, lyr->sc.a, color, c);
         }
      break;
      case HWC2_COMPOSITION_CLIENT:
         if (f->tgt != NULL) {
            if (ccli == 0) {
               /* client target is valid and we have not composed it yet...
                *
                * FALL THROUGH. */
               lyr->bh = f->tgt;
               ccli++;
            } else {
               ccli++;
               break;
            }
         } else {
            ccli++;
            ALOGW("[ext]:[frame]:%" PRIu64 ":%" PRIu64 ": unhandled HWC2_COMPOSITION_CLIENT\n",
                  dsp->pres, dsp->post);
            break;
         }
      case HWC2_COMPOSITION_DEVICE:
      {
         if (lyr->bh == NULL) {
            ALOGE("[vd]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ":%s (no valid buffer)\n",
                  lyr->hdl, dsp->pres, dsp->post, getCompositionName(lyr->cCli));
            break;
         }
         /* graphics or yv12 layer. */
         NEXUS_Error lrc = NEXUS_NOT_INITIALIZED, lrcp = NEXUS_NOT_INITIALIZED;
         NEXUS_MemoryBlockHandle bh = NULL, bhp = NULL;
         PSHARED_DATA shared = NULL;
         void *map = NULL;
         bool yv12 = false;

         private_handle_t::get_block_handles((private_handle_t *)lyr->bh, &bh, NULL);
         lrc = hwc2_mem_lock(hwc2, bh, &map);
         shared = (PSHARED_DATA) map;
         if (lrc || shared == NULL) {
            ALOGE("[vd]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": invalid dev-shared (lbh:%p:bh:%p)\n",
                  lyr->hdl, dsp->pres, dsp->post, (private_handle_t *)lyr->bh, bh);
            if (lrc == NEXUS_SUCCESS) {
               hwc2_mem_unlock(hwc2, bh);
               lrc = NEXUS_NOT_INITIALIZED;
            }
            break;
         }
         if (((private_handle_t *)lyr->bh)->fmt_set != GR_NONE) {
            bhp = (NEXUS_MemoryBlockHandle)shared->container.block;
            lrcp = hwc2_mem_lock(hwc2, bhp, &map);
            if (lrcp || map == NULL) {
               ALOGE("[vd]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": invalid dev-phys (lbh:%p:bh:%p:bhp:%p)\n",
                     lyr->hdl, dsp->pres, dsp->post, (private_handle_t *)lyr->bh, bh, bhp);
               if (lrcp == NEXUS_SUCCESS) {
                  hwc2_mem_unlock(hwc2, bhp);
                  lrcp = NEXUS_NOT_INITIALIZED;
               }
               if (lrc == NEXUS_SUCCESS) {
                  hwc2_mem_unlock(hwc2, bh);
                  lrc = NEXUS_NOT_INITIALIZED;
               }
               break;
            }
         }

         yv12 = ((shared->container.format == HAL_PIXEL_FORMAT_YV12) ||
                 (shared->container.format == HAL_PIXEL_FORMAT_YCbCr_420_888)) ? true : false;
         if (vl != 0) {
            ALOGW("[vd]:[frame]:%" PRIu64 ":%" PRIu64 ": skip layer on sync error.",
                  dsp->pres, dsp->post);
            blt = HWC2_INVALID;
         } else {
            if (yv12) {
               blt = hwc2_blit_yv12(hwc2, d, lyr, shared, dsp, &ms);
            } else {
               blt = hwc2_blit_gpx(hwc2, d, lyr, shared, dsp, &ms, c);
            }
         }

         if (lrcp == NEXUS_SUCCESS) {
            hwc2_mem_unlock(hwc2, bhp);
         }
         if (lrc == NEXUS_SUCCESS) {
            hwc2_mem_unlock(hwc2, bh);
         }
         /* [iii]. count of composed layers. */
         if (!blt) c++;
         ALOGI_IF(!blt && (dsp->lm & LOG_COMP_DEBUG),
                  "[vd]:[frame]:%" PRIu64 ":%" PRIu64 ": %s layer (%zu)\n",
                  dsp->pres, dsp->post, yv12?"yv12":"rgba", c);
      }
      break;
      default:
         ALOGW("[vd]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ":%s - not handled!\n",
               lyr->hdl, dsp->pres, dsp->post, getCompositionName(lyr->cCli));
      break;
      }

      if (lyr->rf != HWC2_INVALID) {
         hwc2_lyr_tl_inc(hwc2->vd, HWC2_DSP_VD, lyr->hdl, lyr->thdl);
      }
   }

   if (c > 0) {
      ALOGI_IF((dsp->lm & LOG_COMP_SUM_DEBUG),
               "[vd]:[frame]:%" PRIu64 ":%" PRIu64 ":%zu layer%scomposed\n",
               dsp->pres, dsp->post, c, c>1?"s ":" ");
   }

out_error:
   if (dlrcp == NEXUS_SUCCESS) {
      hwc2_mem_unlock(hwc2, dbhp);
   }
   if (dlrc == NEXUS_SUCCESS) {
      hwc2_mem_unlock(hwc2, dbh);
   }
out:
   if (d != NULL) {
      NEXUS_Surface_Destroy(d);
   }
   hwc2_ret_inc(hwc2->vd, 1);
   return;
}

static void *hwc2_vd_cmp(
   void *argv) {
   struct hwc2_bcm_device_t* hwc2 = (struct hwc2_bcm_device_t *)(argv);
   struct hwc2_frame_t *frame = NULL;

   setpriority(PRIO_PROCESS, 0, PRIORITY_URGENT_DISPLAY + 3*PRIORITY_MORE_FAVORABLE);
   set_sched_policy(0, SP_FOREGROUND);

   do {
      bool composing = true, syn = false;
      BKNI_WaitForEvent(hwc2->vd->cmp_evt, BKNI_INFINITE);
      do {
         frame = NULL;
         if (!pthread_mutex_lock(&hwc2->vd->mtx_cmp_wl)) {
            frame = hwc2->vd->cmp_wl;
            if (frame == NULL) {
               composing = false;
            } else {
               if (hwc2->vd->pres >= (hwc2->vd->post + HWC2_VOMP_RUN)) {
                  syn = true;
               }
               hwc2->vd->cmp_wl = frame->next;
            }
            pthread_mutex_unlock(&hwc2->vd->mtx_cmp_wl);
         } else {
            composing = false;
         }
         if (frame != NULL) {
            hwc2->vd->post++;
            hwc2_vd_cmp_frame(hwc2, frame);
            free(frame);
            frame = NULL;
         }
         if (syn) {
            syn = false;
            composing = false;
            BKNI_SetEvent(hwc2->vd->cmp_syn);
         }
      } while (composing);

   } while(hwc2->vd->cmp_active);

   return NULL;
}

static void hwc2_lyr_tl_queued(
   struct hwc2_dsp_t *dsp,
   uint32_t kind,
   uint64_t lyr,
   uint64_t hdl) {

   (void)lyr;

   size_t num;
   if (kind == HWC2_DSP_EXT) {
      for (num = 0 ; num < HWC2_MAX_TL ; num++) {
         if (dsp->u.ext.rtl[num].hdl == hdl) {
            if(!pthread_mutex_lock(&dsp->u.ext.rtl[num].mtx_lc)) {
               dsp->u.ext.rtl[num].que = true;
               pthread_mutex_unlock(&dsp->u.ext.rtl[num].mtx_lc);
            }
         }
      }
   }
}

static void hwc2_lyr_tl_unset(
   struct hwc2_dsp_t *dsp,
   uint32_t kind,
   uint64_t lyr,
   uint64_t hdl) {

   size_t num, si;
   if (kind == HWC2_DSP_EXT) {
      for (num = 0 ; num < HWC2_MAX_TL ; num++) {
         if (dsp->u.ext.rtl[num].hdl == hdl) {
            if (!pthread_mutex_lock(&dsp->u.ext.rtl[num].mtx_lc)) {
               if (dsp->u.ext.rtl[num].que) {
                  dsp->u.ext.rtl[num].rem = true;
                  pthread_mutex_unlock(&dsp->u.ext.rtl[num].mtx_lc);
                  ALOGI("[ext]:[tl-uset]:%" PRIu64 ":%" PRIu64 ":delay queued\n", lyr, hdl);
               } else {
                  pthread_mutex_unlock(&dsp->u.ext.rtl[num].mtx_lc);
                  if (dsp->u.ext.rtl[num].pt != dsp->u.ext.rtl[num].si) {
                     ALOGI("[ext]:[tl-uset]:%" PRIu64 ":%" PRIu64 ":free orphan on %" PRIu64 ":%" PRIu64 "\n",
                           lyr, hdl,
                           dsp->u.ext.rtl[num].si, dsp->u.ext.rtl[num].pt);
                     for (si = 0 ; si < dsp->u.ext.rtl[num].pt - dsp->u.ext.rtl[num].si ; si++) {
                        sw_sync_timeline_inc(dsp->u.ext.rtl[num].tl, 1);
                     }
                  }
                  dsp->u.ext.rtl[num].hdl = 0;
               }
            }
         }
      }
   } else if (kind == HWC2_DSP_VD) {
      for (num = 0 ; num < HWC2_MAX_VTL ; num++) {
         if (dsp->u.vd.rtl[num].hdl == hdl) {
            if (dsp->u.vd.rtl[num].pt != dsp->u.vd.rtl[num].si) {
               ALOGI("[vd]:[tl-uset]:%" PRIu64 ":%" PRIu64 ":free orphan on %" PRIu64 ":%" PRIu64 "\n",
                     lyr, hdl,
                     dsp->u.vd.rtl[num].si, dsp->u.vd.rtl[num].pt);
               for (si = 0 ; si < dsp->u.vd.rtl[num].pt - dsp->u.vd.rtl[num].si ; si++) {
                  sw_sync_timeline_inc(dsp->u.vd.rtl[num].tl, 1);
               }
            }
            dsp->u.vd.rtl[num].hdl = 0;
         }
      }
   }
}

static void hwc2_lyr_tl_dequeued(
   struct hwc2_dsp_t *dsp,
   uint32_t kind,
   uint64_t lyr,
   uint64_t hdl) {

   size_t num;
   if (kind == HWC2_DSP_EXT) {
      for (num = 0 ; num < HWC2_MAX_TL ; num++) {
         if (dsp->u.ext.rtl[num].hdl == hdl) {
            if (!pthread_mutex_lock(&dsp->u.ext.rtl[num].mtx_lc)) {
               dsp->u.ext.rtl[num].que = false;
               if (dsp->u.ext.rtl[num].rem) {
                  dsp->u.ext.rtl[num].rem = false;
                  pthread_mutex_unlock(&dsp->u.ext.rtl[num].mtx_lc);
                  ALOGI("[ext]:[tl-dque]:%" PRIu64 ":%" PRIu64 ":delayed removal\n", lyr, hdl);
                  hwc2_lyr_tl_unset(dsp, kind, lyr, hdl);
               } else {
                  pthread_mutex_unlock(&dsp->u.ext.rtl[num].mtx_lc);
               }
            }
         }
      }
   }
}

static void hwc2_setup_vd(
   struct hwc2_bcm_device_t *hwc2) {

   pthread_attr_t attr;
   pthread_mutexattr_t mattr;
   int num;

   hwc2->vd->lm = property_get_int32(HWC2_LOG_VD, 0);
   hwc2->vd->tlm = HWC2_TLM_MAGIC;

   if (!HWC2_VD_GLES) {
      BKNI_CreateEvent(&hwc2->vd->cmp_evt);
      BKNI_CreateEvent(&hwc2->vd->cmp_syn);
      hwc2->vd->cmp_active = 1;
      hwc2->vd->cmp_wl = NULL;
      pthread_mutexattr_init(&mattr);
      pthread_mutex_init(&hwc2->vd->mtx_cmp_wl, &mattr);
      pthread_mutexattr_destroy(&mattr);
      pthread_attr_init(&attr);
      pthread_create(&hwc2->vd->cmp, &attr, hwc2_vd_cmp, (void *)hwc2);
      pthread_attr_destroy(&attr);
      pthread_setname_np(hwc2->vd->cmp, "hwc2_vd_cmp");

      hwc2->vd->cmp_tl = sw_sync_timeline_create();
      if (hwc2->vd->cmp_tl == 0) {
         int fd = sw_sync_timeline_create();
         close(hwc2->vd->cmp_tl);
         hwc2->vd->cmp_tl = fd;
      }
      if (hwc2->vd->cmp_tl < 0) {
         hwc2->vd->cmp_tl = HWC2_INVALID;
      }
      ALOGI("[vd]: completion timeline: %d\n", hwc2->vd->cmp_tl);

      for (num = 0 ; num < HWC2_MAX_VTL ; num++) {
         hwc2->vd->u.vd.rtl[num].tl = sw_sync_timeline_create();
         if (hwc2->vd->u.vd.rtl[num].tl < 0) {
            hwc2->vd->u.vd.rtl[num].tl = HWC2_INVALID;
         }
         hwc2->vd->u.vd.rtl[num].hdl = 0;
         hwc2->vd->u.vd.rtl[num].ix = 0;
         ALOGI("[vd]: layer completion timeline (%d): %d\n", num, hwc2->vd->u.vd.rtl[num].tl);
      }
   } else {
      hwc2->vd->cmp_tl = HWC2_INVALID;
   }
}

static void hwc2_destroy_vd(
   struct hwc2_bcm_device_t *hwc2) {

   int num = 0;
   struct hwc2_lyr_t *lyr, *lyr2 = NULL;
   struct hwc2_dsp_cfg_t *cfg, *cfg2 = NULL;

   hwc2->vd->cmp_active = 0;
   if (!HWC2_VD_GLES) {
      BKNI_SetEvent(hwc2->vd->cmp_evt);
      pthread_join(hwc2->vd->cmp, NULL);
      pthread_mutex_destroy(&hwc2->vd->mtx_cmp_wl);
      BKNI_DestroyEvent(hwc2->vd->cmp_evt);
      BKNI_DestroyEvent(hwc2->vd->cmp_syn);
   }

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
         if (!HWC2_VD_GLES) hwc2_lyr_tl_unset(hwc2->vd, HWC2_DSP_VD, lyr->hdl, lyr->thdl);
         free(lyr);
         lyr = lyr2;
      }
   }

   if (!HWC2_VD_GLES) {
      close(hwc2->vd->cmp_tl);
      for (num = 0 ; num < HWC2_MAX_VTL ; num++) {
         close(hwc2->vd->u.vd.rtl[num].tl);
      }
   }
}

static int32_t hwc2_vdAdd(
   hwc2_device_t* device,
   uint32_t width,
   uint32_t height,
   int32_t* /*android_pixel_format_t*/ format,
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
   if (width > HWC2_VD_MAX_SZ || height > HWC2_VD_MAX_SZ) {
      ret = HWC2_ERROR_UNSUPPORTED;
      goto out;
   }

   hwc2->vd = (struct hwc2_dsp_t*)malloc(sizeof(*(hwc2->vd)));
   if (hwc2->vd == NULL) {
      ret = HWC2_ERROR_NO_RESOURCES;
      goto out;
   }
   memset(hwc2->vd, 0, sizeof(*(hwc2->vd)));
   snprintf(hwc2->vd->name, sizeof(hwc2->vd->name), "stbVD0");
   hwc2->vd->type = HWC2_DISPLAY_TYPE_VIRTUAL;

   hwc2->vd->cfgs = (struct hwc2_dsp_cfg_t *)malloc(sizeof(struct hwc2_dsp_cfg_t));
   if (hwc2->vd->cfgs != NULL) {
      hwc2->vd->cfgs->next  = NULL;
      hwc2->vd->cfgs->w     = width;
      hwc2->vd->cfgs->h     = height;
      hwc2->vd->cfgs->vsync = hwc2_fps2vsync(60); /* hardcode 60fps */
      hwc2->vd->cfgs->xdp   = 160;
      hwc2->vd->cfgs->ydp   = 160;
   } else {
      free(hwc2->vd);
      hwc2->vd = NULL;
      ret = HWC2_ERROR_NO_RESOURCES;
      goto out;
   }
   hwc2->vd->aCfg = hwc2->vd->cfgs;

   /* setup the rest of the virtual display processing. */
   hwc2_setup_vd(hwc2);

   /* only support this format as output. */
   *format = HAL_PIXEL_FORMAT_RGBA_8888;

   ALOGI("[vd]:[new]:%" PRIu64 ": created[%s]:%" PRIu32 "x%" PRIu32 "\n",
         (hwc2_display_t)(intptr_t)hwc2->vd, HWC2_VD_GLES?"gl":"m2m",
         hwc2->vd->aCfg->w, hwc2->vd->aCfg->h);
   *outDisplay = (hwc2_display_t)(intptr_t)hwc2->vd;

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu32 "x%" PRIu32 ":%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_CREATE_VIRTUAL_DISPLAY),
      width, height, *outDisplay, getErrorName(ret));
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

   hwc2_destroy_vd(hwc2);

   ALOGI("[vd]:[rem]:%" PRIu64 ": destroyed\n", (hwc2_display_t)(intptr_t)hwc2->vd);
   free(hwc2->vd);
   hwc2->vd = NULL;

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_DESTROY_VIRTUAL_DISPLAY),
      display, getErrorName(ret));
   return ret;
}

static size_t hwc2_dump_gen(
   struct hwc2_bcm_device_t *hwc2) {

   size_t max = sizeof(hwc2->dump);
   size_t current = 0;
   struct hwc2_dsp_t *dsp;
   struct hwc2_lyr_t *lyr;

   hwc2->sgl = hwc2_enabled(hwc2_tweak_scale_gles);
   hwc2_eval_log(hwc2);
   hwc2_eval_plm(hwc2);
   if (hwc2->ext != NULL) {
      hwc2->ext->dmp =
         (enum hwc2_record_dump_e)hwc2_setting(hwc2_tweak_dump_enabled);
   }

   memset((void *)hwc2->dump, 0, sizeof(hwc2->dump));

   current += snprintf(&hwc2->dump[current], max-current, "hwc2-bcm\n");
   if (hwc2->vd != NULL) {
      dsp = hwc2->vd;
      if (max-current > 0) {
         current += snprintf(&hwc2->dump[current], max-current,
            "\t[vd][%s]:%" PRIu64 ":%s:%" PRIu32 "x%" PRIu32 ":%" PRIu64 ":%" PRIu64 "\n",
            HWC2_VD_GLES?"gles":"m2mc",
            (hwc2_display_t)(intptr_t)dsp, dsp->name,
            dsp->aCfg->w, dsp->aCfg->h,
            dsp->pres, dsp->post);
      }

      lyr = dsp->lyr;
      while (lyr != NULL) {
         if (max-current > 0) {
            current += snprintf(&hwc2->dump[current], max-current, "\t[lyr]:%" PRIu64 "{%u,%d,%d,%.02f,%d}:{%d,%d,%dx%d}:{%d,%d,%dx%d}\n",
               lyr->hdl, lyr->z, lyr->bm, lyr->cCli, lyr->al, lyr->tr,
               lyr->crp.left, lyr->crp.top, (lyr->crp.right - lyr->crp.left), (lyr->crp.bottom - lyr->crp.top),
               lyr->fr.left, lyr->fr.top, (lyr->fr.right - lyr->fr.left), (lyr->fr.bottom - lyr->fr.top));
         }
         lyr = lyr->next;
      }
   }
   if (hwc2->ext != NULL) {
      dsp = hwc2->ext;
      if (max-current > 0) {
         NEXUS_DynamicRangeProcessingSettings d;
         current += snprintf(&hwc2->dump[current], max-current,
            "\t[ext][%s]:%" PRIu64 ":%s:%" PRIu32 "x%" PRIu32 ":(%" PRIu32 "x%" PRIu32 "):%" PRIu32 ",%" PRIu32 ":op{%d,%d,%dx%d}:%" PRIu64 ":%" PRIu64 "\n",
            dsp->u.ext.gles?"gles":"m2mc",
            (hwc2_display_t)(intptr_t)dsp, dsp->name,
            dsp->aCfg->w, dsp->aCfg->h, dsp->aCfg->ew, dsp->aCfg->eh, dsp->aCfg->xdp, dsp->aCfg->ydp,
            hwc2->ext->op.x, hwc2->ext->op.y, hwc2->ext->op.w, hwc2->ext->op.h,
            dsp->pres, dsp->post);
         NEXUS_Display_GetGraphicsDynamicRangeProcessingSettings(&d);
         current += snprintf(&hwc2->dump[current], max-current,
            "\t[ext]:hdr-%c,hlg-%c::[dyn]:plm-%c,dbv-%c,tch-%c\n",
            dsp->aCfg->hdr10?'o':'x', dsp->aCfg->hlg?'o':'x',
            d.processingModes[NEXUS_DynamicRangeProcessingType_ePlm] == NEXUS_DynamicRangeProcessingMode_eOff?'x':'o',
            d.processingModes[NEXUS_DynamicRangeProcessingType_eDolbyVision] == NEXUS_DynamicRangeProcessingMode_eOff?'x':'o',
            d.processingModes[NEXUS_DynamicRangeProcessingType_eTechnicolorPrime] == NEXUS_DynamicRangeProcessingMode_eOff?'x':'o');
      }

      lyr = dsp->lyr;
      while (lyr != NULL) {
         if (max-current > 0) {
            current += snprintf(&hwc2->dump[current], max-current, "\t[lyr]:%" PRIu64 "{%u,%d,%d,%.02f,%d}:{%d,%d,%dx%d}:{%d,%d,%dx%d}\n",
               lyr->hdl, lyr->z, lyr->bm, lyr->cCli, lyr->al, lyr->tr,
               lyr->crp.left, lyr->crp.top, (lyr->crp.right - lyr->crp.left), (lyr->crp.bottom - lyr->crp.top),
               lyr->fr.left, lyr->fr.top, (lyr->fr.right - lyr->fr.left), (lyr->fr.bottom - lyr->fr.top));
         }
         lyr = lyr->next;
      }
   }

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
   /* validate must have been invoked first. */
   if (!dsp->validated) {
      ret = HWC2_ERROR_NOT_VALIDATED;
      goto out;
   }

   lyr = dsp->lyr;
   while (lyr != NULL) {
      if ((lyr->cDev != HWC2_COMPOSITION_INVALID) &&
          (lyr->cCli != lyr->cDev)) {
         lyr->cCli = lyr->cDev;
      }
      lyr = lyr->next;
   }

   /* redundant - but effectively 'revalidates'. */
   dsp->validated = true;

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_ACCEPT_DISPLAY_CHANGES),
      display, getErrorName(ret));
   return ret;
}

static void hwc2_lyr_tl_set(
   struct hwc2_dsp_t *dsp,
   uint32_t kind,
   uint64_t lyr,
   uint64_t hdl) {

   size_t num;
   int free = HWC2_INVALID;
   if (kind == HWC2_DSP_EXT) {
      for (num = 0 ; num < HWC2_MAX_TL ; num++) {
         if (free == HWC2_INVALID &&
             dsp->u.ext.rtl[num].hdl == 0 &&
             dsp->u.ext.rtl[num].tl != HWC2_INVALID) {
            free = (int)num;
         }
         if (dsp->u.ext.rtl[num].hdl == hdl) {
            return;
         }
      }

      if (free != HWC2_INVALID) {
         dsp->u.ext.rtl[free].hdl = hdl;
         dsp->u.ext.rtl[free].pt  = 0;
         dsp->u.ext.rtl[free].si  = 0;
         dsp->u.ext.rtl[free].que  = false;
         dsp->u.ext.rtl[free].rem  = false;
      } else {
         ALOGE("[tl-set]:%" PRIu64 ":no resources!\n", lyr);
      }
   } else if (kind == HWC2_DSP_VD) {
      for (num = 0 ; num < HWC2_MAX_VTL ; num++) {
         if (free == HWC2_INVALID &&
             dsp->u.vd.rtl[num].hdl == 0 &&
             dsp->u.vd.rtl[num].tl != HWC2_INVALID) {
            free = (int)num;
         }
         if (dsp->u.vd.rtl[num].hdl == hdl) {
            return;
         }
      }

      if (free != HWC2_INVALID) {
         dsp->u.vd.rtl[free].hdl = hdl;
         dsp->u.vd.rtl[free].pt  = 0;
         dsp->u.vd.rtl[free].si  = 0;
      } else {
         ALOGE("[vd]:[tl-set]:%" PRIu64 ":no resources!\n", lyr);
      }
   }
}

static int32_t hwc2_lyr_tl_add(
   struct hwc2_dsp_t *dsp,
   uint32_t kind,
   uint64_t lyr,
   uint64_t hdl) {

   int32_t f = HWC2_INVALID;
   struct sync_fence_info_data info;
   size_t num;
   if (kind == HWC2_DSP_EXT) {
      if (hdl == HWC2_MAGIC) {
         dsp->u.ext.ct.rtl.ix++;
         snprintf(info.name, sizeof(info.name),
                  "hwc2_dsp_ext_%" PRIu64 "", dsp->u.ext.ct.rtl.ix);
         f = sw_sync_fence_create(dsp->u.ext.ct.rtl.tl,
                                  info.name,
                                  dsp->u.ext.ct.rtl.ix);
         if (f < 0) {
            f = HWC2_INVALID;
         } else {
            dsp->u.ext.ct.rtl.pt++;
         }
         ALOGI_IF((dsp->lm & LOG_FENCE_DEBUG),
                  "[ext]:[dsp-tl-add]:%" PRIu64 ":%" PRIu64 ":%d:'%s' @%" PRId64 "\n",
                  lyr, dsp->u.ext.ct.rtl.hdl, dsp->u.ext.ct.rtl.tl, info.name, hwc2_tick());
         return f;
      } else {
         for (num = 0 ; num < HWC2_MAX_TL ; num++) {
            if (dsp->u.ext.rtl[num].hdl == hdl) {
               dsp->u.ext.rtl[num].ix++;
               snprintf(info.name, sizeof(info.name),
                        "hwc2_%zu_ext_%" PRIu64 "", num, dsp->u.ext.rtl[num].ix);
               f = sw_sync_fence_create(dsp->u.ext.rtl[num].tl,
                                        info.name,
                                        dsp->u.ext.rtl[num].ix);
               if (f < 0) {
                  f = HWC2_INVALID;
               } else {
                  dsp->u.ext.rtl[num].pt++;
               }
               ALOGI_IF((dsp->lm & LOG_FENCE_DEBUG),
                        "[ext]:[tl-add]:%" PRIu64 ":%" PRIu64 ":%d:'%s' @%" PRId64 "\n",
                        lyr, dsp->u.ext.rtl[num].hdl, dsp->u.ext.rtl[num].tl, info.name, hwc2_tick());
               return f;
            }
         }
      }
      ALOGE("[ext]:[tl-add]:%" PRIu64 ":orphaned, failed to create\n", lyr);
   } else if (kind == HWC2_DSP_VD) {
      for (num = 0 ; num < HWC2_MAX_VTL ; num++) {
         if (dsp->u.vd.rtl[num].hdl == hdl) {
            dsp->u.vd.rtl[num].ix++;
            snprintf(info.name, sizeof(info.name),
                     "hwc2_%zu_vd_%" PRIu64 "", num, dsp->u.vd.rtl[num].ix);
            f = sw_sync_fence_create(dsp->u.vd.rtl[num].tl,
                                     info.name,
                                     dsp->u.vd.rtl[num].ix);
            if (f < 0) {
               f = HWC2_INVALID;
            } else {
               dsp->u.vd.rtl[num].pt++;
            }
            ALOGI_IF((dsp->lm & LOG_FENCE_DEBUG),
                     "[vd]:[tl-add]:%" PRIu64 ":%" PRIu64 ":%d:'%s' @%" PRId64 "\n",
                     lyr, dsp->u.vd.rtl[num].hdl, dsp->u.vd.rtl[num].tl, info.name, hwc2_tick());
            return f;
         }
      }
      ALOGE("[vd]:[tl-add]:%" PRIu64 ":orphaned, failed to create\n", lyr);
   }

   return HWC2_INVALID;
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
   size_t cnt = 0;

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
   lyr->hdl = (uint64_t)(intptr_t)lyr;
   lyr->rf = HWC2_INVALID;
   lyr->al = 1.0;
   if ((kind != HWC2_DSP_VD) ||
       ((kind == HWC2_DSP_VD) && !HWC2_VD_GLES)) {
      if (dsp->tlm < (uint64_t)HWC2_TLM_MAGIC+0x1000ULL) {
         dsp->tlm++;
      } else {
         dsp->tlm = (uint64_t)HWC2_TLM_MAGIC;
      }
      lyr->thdl = dsp->tlm;
      hwc2_lyr_tl_set(dsp, kind, lyr->hdl, lyr->thdl);
   }

   if (!pthread_mutex_lock(&dsp->mtx_lyr)) {
      nxt = dsp->lyr;
      if (nxt == NULL) {
         dsp->lyr = lyr;
      } else {
         while (nxt != NULL) {
            lnk = nxt;
            nxt = nxt->next;
            cnt++;
         }
         lnk->next = lyr;
      }
      pthread_mutex_unlock(&dsp->mtx_lyr);
   } else {
      free(lyr);
      ret = HWC2_ERROR_NO_RESOURCES;
      goto out;
   }

   *outLayer = (hwc2_layer_t)(intptr_t)lyr;
   cnt++;
   ALOGI_IF((hwc2->lm & LOG_AR_DEBUG),
            "[%s]:[lyr+]:%" PRIu64 ": %zu layers\n",
            (kind==HWC2_DSP_VD)?"vd":"ext", lyr->hdl, cnt);

   /* change in layer, invalidate current display. */
   dsp->validated = false;

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s) -> %" PRIu64 "\n",
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
   uint64_t hdl;
   size_t cnt = 0;

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

   if (!pthread_mutex_lock(&dsp->mtx_lyr)) {
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
         pthread_mutex_unlock(&dsp->mtx_lyr);
         ret = HWC2_ERROR_BAD_LAYER;
         goto out;
      }

      hdl = lyr->hdl;
      if (prv == NULL) {
         dsp->lyr = lyr->next;
      } else {
         prv->next = lyr->next;
      }
      lyr->next = NULL;
      if ((kind != HWC2_DSP_VD) ||
          ((kind == HWC2_DSP_VD) && !HWC2_VD_GLES)) {
         hwc2_lyr_tl_unset(dsp, kind, lyr->hdl, lyr->thdl);
      }
      free(lyr);
      lyr = NULL;

      lyr = dsp->lyr;
      while (lyr != NULL) {
         cnt++;
         lyr = lyr->next;
      }
      pthread_mutex_unlock(&dsp->mtx_lyr);
   } else {
      ret = HWC2_ERROR_BAD_LAYER;
      goto out;
   }

   ALOGI_IF((hwc2->lm & LOG_AR_DEBUG),
            "[%s]:[lyr-]:%" PRIu64 ": %zu layers\n",
            (kind==HWC2_DSP_VD)?"vd":"ext", hdl, cnt);

   /* change in layer, invalidate current display. */
   dsp->validated = false;

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s):%" PRIu64 "\n",
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

   if (!pthread_mutex_lock(&dsp->mtx_cfg)) {
      if (dsp->aCfg == NULL) {
         pthread_mutex_unlock(&dsp->mtx_cfg);
         ret = HWC2_ERROR_BAD_CONFIG;
         *outConfig = (hwc2_config_t)0;
         return ret;
      } else {
         *outConfig = (hwc2_config_t)(intptr_t)dsp->aCfg;
         pthread_mutex_unlock(&dsp->mtx_cfg);
      }
   } else {
      ret = HWC2_ERROR_BAD_CONFIG;
      *outConfig = (hwc2_config_t)0;
      return ret;
   }

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s) -> %u\n",
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
      if ((lyr->cDev != HWC2_COMPOSITION_INVALID) &&
          (lyr->cCli != lyr->cDev)) {
         if (outLayers != NULL && outTypes != NULL) {
            outLayers[*outNumElements] = (hwc2_layer_t)(intptr_t)lyr;
            outTypes[*outNumElements] = lyr->cDev;
         }
         *outNumElements += 1;
      }
      lyr = lyr->next;
   }

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
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
   hwc_rect_t *src,
   hwc_rect_t *dst) {

   NEXUS_Rect s, d;
   hwc2_error_t ret = HWC2_ERROR_NONE;

   s.x = (int16_t)src->left;
   s.y = (int16_t)src->top;
   s.width = (uint16_t)(src->right - src->left);
   s.height = (uint16_t)(src->bottom - src->top);

   d.x = (int16_t)dst->left;
   d.y = (int16_t)dst->top;
   d.width = (uint16_t)(dst->right - dst->left);
   d.height = (uint16_t)(dst->bottom - dst->top);

   /* forcing gles composition if we cannot handle scaling in g2d (would
    * require intermediate buffer and 2-steps composition).
    */
   if (s.width && d.width &&
       (s.width / d.width) >= hwc2->g2dc.maxHorizontalDownScale) {
      ALOGI_IF((hwc2->lm & LOG_OFFLD_DEBUG), "horizontal:%d->%d::max:%d",
               s.width, d.width, hwc2->g2dc.maxHorizontalDownScale);
      ret = HWC2_ERROR_UNSUPPORTED;
   }
   /* forcing gles composition if we cannot handle scaling in g2d (would
    * require intermediate buffer and 2-steps composition).
    */
   if (s.height && d.height &&
       (s.height / d.height) >= hwc2->g2dc.maxVerticalDownScale) {
      ALOGI_IF((hwc2->lm & LOG_OFFLD_DEBUG), "vertical:%d->%d::max:%d",
               s.height, d.height, hwc2->g2dc.maxVerticalDownScale);
      ret = HWC2_ERROR_UNSUPPORTED;
   }
   /* forced gles composition if scaling involved (debug tweak). */
   if (hwc2->sgl) {
      if (s.width && d.width && (s.width != d.width)) {
         ALOGI_IF((hwc2->lm & LOG_OFFLD_DEBUG), "horizontal:%d->%d::forced",
                  s.width, d.width);
         ret = HWC2_ERROR_UNSUPPORTED;
      }
      if (s.height && d.height && (s.height != d.height)) {
         ALOGI_IF((hwc2->lm & LOG_OFFLD_DEBUG), "vertical:%d->%d::forced",
                  s.height, d.height);
         ret = HWC2_ERROR_UNSUPPORTED;
      }
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

   if (!pthread_mutex_lock(&dsp->mtx_cfg)) {
      uint32_t w, h;
      if (dsp->aCfg == NULL) {
         pthread_mutex_unlock(&dsp->mtx_cfg);
         ret = HWC2_ERROR_BAD_DISPLAY;
         goto out;
      }
      w = (dsp->aCfg->w > HWC2_FB_MAX_W) ? HWC2_FB_MAX_W : dsp->aCfg->w;
      h = (dsp->aCfg->h > HWC2_FB_MAX_H) ? HWC2_FB_MAX_H : dsp->aCfg->h;
      if (w == width &&
          h == height &&
          format == HAL_PIXEL_FORMAT_RGBA_8888 &&
          dataspace == HAL_DATASPACE_UNKNOWN) {
         pthread_mutex_unlock(&dsp->mtx_cfg);
         goto out;
      }
      pthread_mutex_unlock(&dsp->mtx_cfg);
   }

   ret = (hwc2_error_t)hwc2_fmtSupp(format);
   if (ret != HWC2_ERROR_NONE) {
      goto out;
   }
   ret = (hwc2_error_t)hwc2_dspcSupp(dataspace);
   if (ret != HWC2_ERROR_NONE) {
      goto out;
   }

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
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
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
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
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 ":%u:%s (%s)\n",
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

   /* 'zero' value is legal, we do not have the actual data exposed through
    * the middleware anyways to calculate that from the edid.
    */
   *outMaxLuminance = 0;
   *outMaxAverageLuminance = 0;
   *outMinLuminance = 0;

   if (pthread_mutex_lock(&dsp->mtx_cfg)) {
      *outNumTypes = 0;
      goto out;
   }
   if (!dsp->aCfg->hdr10 && !dsp->aCfg->hlg) {
      pthread_mutex_unlock(&dsp->mtx_cfg);
      *outNumTypes = 0;
      goto out;
   }
   if (dsp->aCfg->hdr10 && dsp->aCfg->hlg) {
      if (outTypes == NULL) {
         *outNumTypes = 2;
      } else {
         *outNumTypes = 2;
         outTypes[0] = HAL_HDR_HDR10;
         outTypes[1] = HAL_HDR_HLG;
      }
   } else if (dsp->aCfg->hdr10 || dsp->aCfg->hlg) {
      if (outTypes == NULL) {
         *outNumTypes = 1;
      } else {
         *outNumTypes = 1;
         if (dsp->aCfg->hdr10) {
            outTypes[0] = HAL_HDR_HDR10;
         } else if (dsp->aCfg->hlg) {
            outTypes[0] = HAL_HDR_HLG;
         }
      }
   }
   pthread_mutex_unlock(&dsp->mtx_cfg);

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
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
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
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
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
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
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
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
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_BLEND_MODE),
      display, layer, getErrorName(ret));
   return ret;
}

static void hwc2_bh_block(
   struct hwc2_bcm_device_t *hwc2,
   struct hwc2_lyr_t *lyr,
   NEXUS_MemoryBlockHandle *blk) {

   NEXUS_Error lrc = NEXUS_NOT_INITIALIZED;
   NEXUS_MemoryBlockHandle bh = NULL;
   PSHARED_DATA shared = NULL;
   void *map = NULL;

   *blk = NULL;

   if (lyr->bh == NULL) {
      return;
   }

   private_handle_t::get_block_handles((private_handle_t *)lyr->bh, &bh, NULL);
   lrc = hwc2_mem_lock(hwc2, bh, &map);
   shared = (PSHARED_DATA) map;
   if (lrc || shared == NULL) {
      goto out;
   }
   *blk = shared->container.block;

out:
   if (lrc == NEXUS_SUCCESS) {
      hwc2_mem_unlock(hwc2, bh);
   }
   return;
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

   if (lyr->cCli == HWC2_COMPOSITION_SOLID_COLOR ||
       lyr->cCli == HWC2_COMPOSITION_SIDEBAND ||
       lyr->cCli == HWC2_COMPOSITION_CLIENT) {
      lyr->bh = NULL;
      lyr->af = HWC2_INVALID;
   } else {
      NEXUS_MemoryBlockHandle blk = NULL;
      lyr->bh = buffer;
      lyr->af = acquireFence;
      // TODO: experimental: to be continued.
      // hwc2_bh_block(hwc2, lyr, &blk);
      if (blk && (lyr->lblk == blk)) {
          lyr->scmp = true;
          lyr->lblk = blk;
      } else {
          lyr->scmp = false;
          lyr->lblk = blk;
      }
   }

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_BUFFER),
      display, layer, getErrorName(ret));
   if (ret != HWC2_ERROR_NONE) {
      if (acquireFence >= 0) {
         close(acquireFence);
      }
   }
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

   if (lyr->cCli != HWC2_COMPOSITION_SOLID_COLOR) {
      goto out;
   }

   memcpy(&lyr->sc, &color, sizeof(color));

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
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

   /* composition type offered by surface flinger. */
   lyr->cCli = (hwc2_composition_t)type;
   /* ...reset our prefered composition type for this layer. */
   if (dsp->type == HWC2_DISPLAY_TYPE_VIRTUAL) {
      lyr->cDev = HWC2_COMPOSITION_CLIENT;
   } else {
      if (dsp->u.ext.gles) {
         lyr->cDev = HWC2_COMPOSITION_CLIENT;
      } else {
         lyr->cDev = HWC2_COMPOSITION_INVALID;
      }
   }

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
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
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
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

   ALOGI_IF(HWC2_INBOUND_DBG,
      "%s:%" PRIu64 ":%" PRIu64 " cmp:%" PRIu64 ":%" PRIu64 ": frame:{%d,%d,%d,%d}",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_DISPLAY_FRAME),
      display, layer, dsp->pres, dsp->post,
      frame.left, frame.top, frame.right, frame.bottom);

   memcpy(&lyr->fr, &frame, sizeof(frame));

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
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
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
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

   if (lyr->cCli != HWC2_COMPOSITION_SIDEBAND) {
      goto out;
   }

   if ((stream->data[1] < HWC2_SDB_MAGIC) ||
       (stream->data[1] >= HWC2_SDB_MAGIC+HWC_BINDER_SIDEBAND_SURFACE_SIZE)) {
      ret = HWC2_ERROR_BAD_PARAMETER;
      goto out;
   }

   lyr->sbh = (native_handle_t*)stream;

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
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

   ALOGI_IF(HWC2_INBOUND_DBG,
      "%s:%" PRIu64 ":%" PRIu64 " cmp:%" PRIu64 ":%" PRIu64 ": crop:{%f,%f,%f,%f}",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_SOURCE_CROP),
      display, layer, dsp->pres, dsp->post,
      crop.left, crop.top, crop.right, crop.bottom);

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
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
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
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
   if (lyr->tr) {
      ALOGV("[%s]:[xform]:%" PRIu64 ":%s",
            (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext",
            lyr->hdl, getTransformName(lyr->tr));
   }

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
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
      ALOGV("-> %s:%" PRIu64 ":%" PRIu64 "\n",
        getFunctionDescriptorName(HWC2_FUNCTION_SET_LAYER_VISIBLE_REGION),
        display, layer);
      // ret = HWC2_ERROR_BAD_PARAMETER;
      // goto out;
   }

   // TODO: support more than first one...
   if (visible.numRects) {
      memcpy(&lyr->vis, visible.rects, sizeof(lyr->vis));
   }

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
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
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
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

   dsp->u.vd.output = buffer;
   dsp->u.vd.wrFence = releaseFence;

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_OUTPUT_BUFFER),
      display, getErrorName(ret));
   if (ret != HWC2_ERROR_NONE) {
      if (releaseFence >= 0) {
         close(releaseFence);
      }
   }
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
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
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
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 ":%" PRIu64 " (%s)\n",
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
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
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
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
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

   /* valid to have a NULL client target when there is no
    * HWC2_COMPOSITION_CLIENT layer.
    */
   if (target == NULL) {
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
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_SET_CLIENT_TARGET),
      display, getErrorName(ret));
   if (ret != HWC2_ERROR_NONE) {
      if (acquireFence >= 0) {
         close(acquireFence);
      }
   }
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

   if (pthread_mutex_lock(&dsp->mtx_cfg)) {
      ret = HWC2_ERROR_BAD_PARAMETER;
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
      /* act on the new configuration requested. */
      hwc2_set_acfg_l(dsp);
   }
   pthread_mutex_unlock(&dsp->mtx_cfg);

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 ":%u (%s)\n",
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
         if (lyr->rf != HWC2_INVALID) {
            *outNumElements += 1;
         }
         lyr = lyr->next;
      }
   } else {
      num = 0;
      lyr = dsp->lyr;
      while (lyr != NULL) {
         if (lyr->rf != HWC2_INVALID) {
            outLayers[num] = (hwc2_layer_t)(intptr_t)lyr;
            outFences[num] = lyr->rf;
            num++;
         }
         lyr = lyr->next;
      }
   }

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
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

   if (dsp->type == HWC2_DISPLAY_TYPE_VIRTUAL) {
      *outDisplayRequests = HWC2_DISPLAY_REQUEST_WRITE_CLIENT_TARGET_TO_OUTPUT;
      *outNumElements = 0;
   } else {
      *outDisplayRequests = HWC2_DISPLAY_REQUEST_FLIP_CLIENT_TARGET;
      lyr = dsp->lyr;
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
   }

out:
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_GET_DISPLAY_REQUESTS),
      display, getErrorName(ret));
   return ret;
}

static uint32_t hwc2_comp_validate(
   struct hwc2_bcm_device_t *hwc2,
   struct hwc2_dsp_t *dsp) {

   struct hwc2_lyr_t *lyr = NULL;
   uint32_t cnt = 0;

   lyr = dsp->lyr;
   while (lyr != NULL) {
      cnt++;
      if (lyr->cCli == HWC2_COMPOSITION_CURSOR) {
         ALOGW("lyr[%" PRIu64 "]:%s->%s (cursor not supported)", lyr->hdl,
               getCompositionName(HWC2_COMPOSITION_CURSOR),
               getCompositionName(HWC2_COMPOSITION_DEVICE));
         lyr->cDev = HWC2_COMPOSITION_DEVICE;
      } else if (lyr->cCli == HWC2_COMPOSITION_DEVICE) {
         hwc2_error_t ret = (hwc2_error_t)hwc2_sclSupp(hwc2, &lyr->crp, &lyr->fr);
         if (ret != HWC2_ERROR_NONE) {
            ALOGI_IF((hwc2->lm & LOG_OFFLD_DEBUG),
               "lyr[%" PRIu64 "]:%s->%s (scaling out of bounds)",
               lyr->hdl,
               getCompositionName(HWC2_COMPOSITION_DEVICE),
               getCompositionName(HWC2_COMPOSITION_CLIENT));
            lyr->cDev = HWC2_COMPOSITION_CLIENT;
         }
      }
      lyr = lyr->next;
   };

   return cnt;
}

static void hwc2_z_order(
   struct hwc2_bcm_device_t *hwc2,
   struct hwc2_dsp_t *dsp,
   uint32_t cnt) {

   struct hwc2_lyr_t **lyr = NULL, *cur = NULL;
   uint32_t i, j;

   lyr = (struct hwc2_lyr_t **) malloc(cnt * sizeof(struct hwc2_lyr_t *));
   if (lyr == NULL) {
      ALOGE("[%s]:[hwc2]: failed z-order (%u) array",
            (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext", cnt);
      return;
   }

   cur = dsp->lyr;
   for (i = 0 ; i < cnt ; i++) {
      lyr[i] = cur;
      cur = cur->next;
   }

   for (i = 0 ; i < cnt ; i++) {
      lyr[i]->next = NULL;
   }
   for (i = 0 ; i < cnt ; i++) {
      for (j = i+1 ; j < cnt ; j++) {
         if (lyr[i]->z > lyr[j]->z) {
            ALOGI_IF((hwc2->lm & LOG_Z_DEBUG),
                     "[%s]:[z-swap]:%" PRIu64 ":%u && %" PRIu64 ":%u",
                     (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext",
                     lyr[i]->hdl, lyr[i]->z, lyr[j]->hdl, lyr[j]->z);
            cur =  lyr[i];
            lyr[i] = lyr[j];
            lyr[j] = cur;
         }
      }
   }

   dsp->lyr = lyr[0];
   for (i = 1 ; i < cnt ; i++) {
      lyr[i-1]->next = lyr[i];
   }
   lyr[cnt-1]->next = NULL;

   free(lyr);
}

static void hwc2_connect(
   struct hwc2_bcm_device_t *hwc2,
   struct hwc2_dsp_t *dsp) {

   size_t i;

   if (!hwc2->con) {
      if (hwc2->hb) {
         int w = 0, h = 0;
         if (!pthread_mutex_lock(&dsp->mtx_cfg)) {
            w = (dsp->aCfg->w > HWC2_FB_MAX_W) ? HWC2_FB_MAX_W : (int)dsp->aCfg->w;
            h = (dsp->aCfg->h > HWC2_FB_MAX_H) ? HWC2_FB_MAX_H : (int)dsp->aCfg->h;
            pthread_mutex_unlock(&dsp->mtx_cfg);
         }
         hwc2->hb->connect();
         for (i = 0; i < HWC_BINDER_VIDEO_SURFACE_SIZE; i++) {
            hwc2->hb->setvideo(i, HWC2_VID_MAGIC+i, w, h);
         }
         for (i = 0; i < HWC_BINDER_SIDEBAND_SURFACE_SIZE; i++) {
            hwc2->hb->setsideband(i, HWC2_SDB_MAGIC+i, w, h);
         }
         hwc2->con = true;
      }
   }
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
   uint32_t cnt = 0;
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

   /* internal validation, this is where we may change the
    * layer composition associated with each layers to be composed.
    */
   hwc2_connect(hwc2, dsp);
   cnt = hwc2_comp_validate(hwc2, dsp);
   if (cnt > 1) {
      hwc2_z_order(hwc2, dsp, cnt);
   }

   lyr = dsp->lyr;
   *outNumTypes = 0;
   *outNumRequests = 0;

   while (lyr != NULL) {
      if (lyr->rp) {
         *outNumRequests += 1;
      }
      if ((lyr->cDev != HWC2_COMPOSITION_INVALID) &&
          (lyr->cCli != lyr->cDev)) {
         *outNumTypes += 1;
      }
      lyr = lyr->next;
   }

   if (*outNumTypes > 0) {
      ret = HWC2_ERROR_HAS_CHANGES;
   }

   dsp->validated = true;

out:
   ALOGE_IF(!((ret==HWC2_ERROR_NONE)||(ret==HWC2_ERROR_HAS_CHANGES))||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
      getFunctionDescriptorName(HWC2_FUNCTION_VALIDATE_DISPLAY),
      display, getErrorName(ret));
   return ret;
}

static bool hwc2_is_video(
   struct hwc2_bcm_device_t *hwc2,
   struct hwc2_lyr_t *lyr,
   int *vid) {

   bool video = false;
   NEXUS_Error lrc = NEXUS_NOT_INITIALIZED;
   NEXUS_MemoryBlockHandle bh = NULL;
   PSHARED_DATA shared = NULL;
   void *map = NULL;

   *vid = -1;
   if (lyr->bh == NULL) {
      return video;
   }

   private_handle_t::get_block_handles((private_handle_t *)lyr->bh, &bh, NULL);
   lrc = hwc2_mem_lock(hwc2, bh, &map);
   shared = (PSHARED_DATA) map;
   if (lrc || shared == NULL) {
      goto out;
   }
   *vid = android_atomic_acquire_load(&(shared->videoWindow.windowIdPlusOne));
   if (*vid > 0) {
      if (shared->videoWindow.nexusClientContext) {
         video = true;
         lyr->oob = true;
      }
   }

out:
   if (lrc == NEXUS_SUCCESS) {
      hwc2_mem_unlock(hwc2, bh);
   }
   return video;
}

static bool hwc2_lyr_adj(
   struct hwc2_dsp_t *dsp,
   NEXUS_Rect *c,
   NEXUS_Rect *p,
   NEXUS_SurfaceStatus *ds) {

   struct hwc_position o = {0,0,0,0};
   int adj;
   int w = 0, h = 0;

   if (!pthread_mutex_lock(&dsp->mtx_cfg)) {
      w = (dsp->aCfg->w > HWC2_FB_MAX_W) ? HWC2_FB_MAX_W : (int)dsp->aCfg->w;
      h = (dsp->aCfg->h > HWC2_FB_MAX_H) ? HWC2_FB_MAX_H : (int)dsp->aCfg->h;
      pthread_mutex_unlock(&dsp->mtx_cfg);
   }
   if (!w && !h) {
      return true;
   }

   if (memcmp(&o, &dsp->op, sizeof(o))) {
      if ((ds == NULL) ||
          ((ds != NULL) && (dsp->u.ext.cb == cbs_e_bypass))) {
         p->x += (int16_t)dsp->op.x;
         p->y += (int16_t)dsp->op.y;
         p->width = (uint16_t)((int)p->width + (((int)p->width * dsp->op.w)/w));
         p->height = (uint16_t)((int)p->height + (((int)p->height * dsp->op.h)/h));
      }

      if (ds == NULL) {
         return true;
      }

      if (p->x < 0) {
         adj = -((int)p->x);
         p->x = 0;
         p->width = (adj<=(int)p->width) ? (uint16_t)((int)p->width-adj) : 0;
         c->x += (int16_t)adj;
         c->width = (adj<=(int)c->width) ? (uint16_t)((int)c->width-adj) : 0;
      }
      if (p->y < 0) {
         adj = -((int)p->y);
         p->y = 0;
         p->height = (adj<=(int)p->height) ? (uint16_t)((int)p->height-adj) : 0;
         c->y += adj;
         c->height = (adj<=(int)c->height) ? (uint16_t)((int)c->height-adj) : 0;
      }
      if ((int)p->x+(int)p->width > (int)ds->width) {
         adj = ((int)p->x+(int)p->width) - (int)ds->width;
         p->width = (adj<=(int)p->width) ? (uint16_t)((int)p->width-adj) : 0;
         c->width = (adj<=(int)c->width) ? (uint16_t)((int)c->width-adj) : 0;
      }
      if ((int)p->y+(int)p->height > (int)ds->height) {
         adj = ((int)p->y+(int)p->height) - (int)ds->height;
         p->height = (adj<=(int)p->height) ? (uint16_t)((int)p->height-adj) : 0;
         c->height = (adj<=(int)c->height) ? (uint16_t)((int)c->height-adj) : 0;
      }
      if (!((p->width > 0) &&
            (p->height > 0) &&
            (c->width > 0) &&
            (c->height > 0))) {
         return false;
      }
   }

   return true;
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
   int32_t frame_size, cnt = 0, vcnt = 0, scnt = 0;
   int ccli = 0, vid = -1;
   bool ivid;
   bool dc = false;

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

      if (pthread_mutex_lock(&hwc2->mtx_pwr)) {
         ALOGW("[ext]:[pres]:%" PRIu64 ":%" PRIu64 ": failed to acquire power mutex", dsp->pres, dsp->post);
         ret = HWC2_ERROR_NO_RESOURCES;
         dsp->post++;
         goto out_error;
      }
      if (hwc2->ext->pmode == HWC2_POWER_MODE_OFF) {
         pthread_mutex_unlock(&hwc2->mtx_pwr);
         ALOGW("[ext]:[pres]:%" PRIu64 ":%" PRIu64 ": invalid composition while power-off", dsp->pres, dsp->post);
         ret = HWC2_ERROR_NO_RESOURCES;
         dsp->post++;
         goto out_error;
      }
      pthread_mutex_unlock(&hwc2->mtx_pwr);

      cnt = hwc2_cntLyr(dsp);
      if (cnt == 0) {
         ALOGW("[ext]:[pres]:%" PRIu64 ":%" PRIu64 ": no layer to compose", dsp->pres, dsp->post);
         dsp->post++;
         *outRetireFence = hwc2_ret_fence(dsp);
         goto out_signal;
      }
      frame_size = sizeof(struct hwc2_frame_t) + (cnt * sizeof(struct hwc2_lyr_t));
      frame = (struct hwc2_frame_t *) calloc(1, frame_size);
      if (frame == NULL) {
         ret = HWC2_ERROR_NO_RESOURCES;
         dsp->post++;
         goto out_error;
      }
      frame->cnt  = cnt;
      lyr = dsp->lyr;
      clyr = &frame->lyr[0];
      for (frame_size = 0 ; frame_size < cnt ; frame_size++) {
         lyr->oob = false;
         if (lyr->cCli == HWC2_COMPOSITION_CLIENT) {
            if (!ccli) {
               if (hwc2->ext->u.ext.ct.tgt) {
                  frame->tgt = hwc2->ext->u.ext.ct.tgt;
               } else {
                  frame->tgt = NULL;
               }
               frame->ftgt = hwc2->ext->u.ext.ct.rdf;
            }
            ccli++;
         } else if (lyr->cCli == HWC2_COMPOSITION_SIDEBAND) {
            scnt++;
         } else if (hwc2_is_video(hwc2, lyr, &vid)) {
            vcnt++;
            if ((vid <= 0) || (vid > HWC2_VID_WIN)) {
               ivid = true;
               ALOGW("[oob]:[pres]:%" PRIu64 ":%" PRIu64 ": invalid vid=%d, skip!",
                     dsp->pres, dsp->post, vid);
            } else {
               ivid = false;
            }
            /* signal to the video display the frame to be presented. */
            if (!ivid && hwc2->hb) {
               NEXUS_Error lrc = NEXUS_NOT_INITIALIZED;
               NEXUS_MemoryBlockHandle bh = NULL;
               NEXUS_Rect c, p;
               struct hwc_position fr, cl;
               int z = 0;
               PSHARED_DATA shared = NULL;
               void *map = NULL;
               private_handle_t::get_block_handles((private_handle_t *)lyr->bh, &bh, NULL);
               lrc = hwc2_mem_lock(hwc2, bh, &map);
               shared = (PSHARED_DATA) map;
               if ((lrc == NEXUS_SUCCESS) && (shared != NULL)) {
                  if (hwc2->rlpf[vid-1]) {
                     lyr->lpf = HWC2_RLPF;
                  }
                  c = {(int16_t)lyr->crp.left,
                       (int16_t)lyr->crp.top,
                       (uint16_t)(lyr->crp.right - lyr->crp.left),
                       (uint16_t)(lyr->crp.bottom - lyr->crp.top)};
                  p = {(int16_t)lyr->fr.left,
                       (int16_t)lyr->fr.top,
                       (uint16_t)(lyr->fr.right - lyr->fr.left),
                       (uint16_t)(lyr->fr.bottom - lyr->fr.top)};
                  if ((lyr->lpf == HWC2_RLPF) ||
                      memcmp((void *)&lyr->crp, (void *)&dsp->u.ext.vid[vid-1].crp, sizeof(dsp->u.ext.vid[vid-1].crp)) != 0 ||
                      memcmp((void *)&lyr->fr, (void *)&dsp->u.ext.vid[vid-1].fr, sizeof(dsp->u.ext.vid[vid-1].fr)) != 0) {
                     memcpy((void *)&dsp->u.ext.vid[vid-1].fr, (void *)&lyr->fr, sizeof(dsp->u.ext.vid[vid-1].fr));
                     memcpy((void *)&dsp->u.ext.vid[vid-1].crp, (void *)&lyr->crp, sizeof(dsp->u.ext.vid[vid-1].crp));
                     hwc2_lyr_adj(dsp, &c, &p, NULL);
                     fr.x = p.x;
                     fr.y = p.y;
                     fr.w = p.width;
                     fr.h = p.height;
                     cl.x = c.x;
                     cl.y = c.y;
                     cl.w = c.width == (uint16_t)HWC2_INVALID ? 0 : c.width;
                     cl.h = c.height == (uint16_t)HWC2_INVALID ? 0 : c.height;
                     // TODO: associate relative z-order from android z-layer.
                     z = (vid==1)?2:(vid==2)?3:1; /* main is on 5-3, pip is on 5-2, others on 1. */
                     hwc2->hb->setgeometry(HWC_BINDER_OMX, vid-1, fr, cl, z, 1);
                     ALOGI_IF((hwc2->lm & LOG_OOB_DEBUG),
                              "[oob]:%" PRIu64 ":%" PRIu64 ": geometry {%d,%d,%dx%d}, {%d,%d,%dx%d}\n",
                              dsp->pres, dsp->post, fr.x, fr.y, fr.w, fr.h, cl.x, cl.y, cl.w, cl.h);
                  }
                  if (lyr->lpf != shared->videoFrame.status.serialNumber) {
                     lyr->lpf = shared->videoFrame.status.serialNumber;
                     /* vid - 1 for main, 2 for pip, other not assigned yet. */
                     hwc2->hb->setframe(HWC2_VID_MAGIC+(vid-1), lyr->lpf);
                     ALOGI_IF((hwc2->lm & LOG_OOB_DEBUG),
                              "[oob]:[%s]:%" PRIu64 ":%" PRIu64 ": signal-frame %" PRIu32 " (%" PRIu32 ")\n",
                              (vid==1)?"full":(vid==2)?"pip":"mos", dsp->pres, dsp->post, lyr->lpf, shared->videoFrame.status.serialNumber);
                  }
                  dc = true;
               }
               if (lrc == NEXUS_SUCCESS) {
                  hwc2_mem_unlock(hwc2, bh);
                  lrc = NEXUS_NOT_INITIALIZED;
               }
            }
         }
         lyr->rf = HWC2_INVALID;
         if (((lyr->cDev == HWC2_COMPOSITION_INVALID) &&
               (lyr->cCli == HWC2_COMPOSITION_DEVICE)) ||
             (lyr->cDev == HWC2_COMPOSITION_DEVICE)) {
            if (lyr->oob) {
               lyr->rf = HWC2_INVALID;
            } else {
               lyr->rf = hwc2_lyr_tl_add(dsp, kind, lyr->hdl, lyr->thdl);
            }
         } else if (lyr->cCli == HWC2_COMPOSITION_CLIENT) {
            if (frame->tgt != NULL && ccli == 1) {
               lyr->rf = hwc2_lyr_tl_add(dsp, kind, lyr->hdl, HWC2_MAGIC);
            }
         }
         /* modification to the layer content may take place prior to copy. */
         if ((lyr->rf != HWC2_INVALID) &&
             (lyr->cCli != HWC2_COMPOSITION_CLIENT)) {
            hwc2_lyr_tl_queued(dsp, kind, lyr->hdl, lyr->thdl);
         }
         memcpy(clyr, lyr, sizeof(struct hwc2_lyr_t));
         lyr = lyr->next;
         clyr->next = &frame->lyr[frame_size+1];
         clyr = &frame->lyr[frame_size+1];
      }
      frame->vcnt = vcnt;
      frame->scnt = scnt;
      if (vcnt) {
         int i;
         for (i = 0; i < HWC2_VID_WIN; i++) {
            if (hwc2->rlpf[i]) {
               hwc2->rlpf[i] = false;
            }
         }
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
         ret = HWC2_ERROR_NO_RESOURCES;
         dsp->post++;
         goto out_error;
      }

      *outRetireFence = hwc2_ret_fence(dsp);
      if (dc) {
         hwc2_ret_inc(dsp, 1);
      }
      BKNI_SetEvent(dsp->cmp_evt);
      if (dsp->pres >= (dsp->post + HWC2_COMP_RUN)) {
         BKNI_WaitForEvent(dsp->cmp_syn, BKNI_INFINITE);
      }
      goto out;
   } else if (kind == HWC2_DSP_VD) {
      struct hwc2_frame_t *frame = NULL;
      struct hwc2_lyr_t *clyr = NULL;

      if (HWC2_VD_GLES) {
         *outRetireFence = -1;
         if (dsp->u.vd.wrFence >= 0) {
            close(dsp->u.vd.wrFence);
            dsp->u.vd.wrFence = HWC2_INVALID;
         }
         if (hwc2->vd->u.vd.ct.rdf >= 0) {
            close(hwc2->vd->u.vd.ct.rdf);
            hwc2->vd->u.vd.ct.rdf = HWC2_INVALID;
         }
         cnt = hwc2_cntLyr(dsp);
         lyr = dsp->lyr;
         for (frame_size = 0 ; frame_size < cnt ; frame_size++) {
            lyr->rf = HWC2_INVALID;
            if (lyr->af >= 0) {
               close(lyr->af);
               lyr->af = HWC2_INVALID;
            }
            lyr = lyr->next;
         }
         dsp->post++;
         goto out;
      }

      cnt = hwc2_cntLyr(dsp);
      if (cnt == 0) {
         ALOGW("[vd]:[pres]:%" PRIu64 ":%" PRIu64 ": no layer to compose", dsp->pres, dsp->post);
         dsp->post++;
         *outRetireFence = hwc2_ret_fence(dsp);
         goto out_signal;
      }
      frame_size = sizeof(struct hwc2_frame_t) + (cnt * sizeof(struct hwc2_lyr_t));
      frame = (struct hwc2_frame_t *) calloc(1, frame_size);
      if (frame == NULL) {
         ret = HWC2_ERROR_NO_RESOURCES;
         dsp->post++;
         goto out_error;
      }
      frame->cnt = cnt;
      frame->otgt = dsp->u.vd.output;
      frame->oftgt = dsp->u.vd.wrFence;
      lyr = dsp->lyr;
      clyr = &frame->lyr[0];
      for (frame_size = 0 ; frame_size < cnt ; frame_size++) {
         if (lyr->cCli == HWC2_COMPOSITION_CLIENT) {
            if (!ccli) {
               if (hwc2->vd->u.vd.ct.tgt) {
                  frame->tgt = hwc2->vd->u.vd.ct.tgt;
               } else {
                  frame->tgt = NULL;
               }
               frame->ftgt = hwc2->vd->u.vd.ct.rdf;
            }
            ccli++;
         }
         lyr->rf = HWC2_INVALID;
         if (((lyr->cDev == HWC2_COMPOSITION_INVALID) &&
               (lyr->cCli == HWC2_COMPOSITION_DEVICE)) ||
             (lyr->cDev == HWC2_COMPOSITION_DEVICE)) {
            lyr->rf = hwc2_lyr_tl_add(dsp, kind, lyr->hdl, lyr->thdl);
         }
         /* modification to the layer content may take place prior to copy. */
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
         ret = HWC2_ERROR_NO_RESOURCES;
         dsp->post++;
         goto out_error;
      }

      *outRetireFence = hwc2_ret_fence(dsp);
      BKNI_SetEvent(dsp->cmp_evt);
      if (dsp->pres >= (dsp->post + HWC2_VOMP_RUN)) {
         BKNI_WaitForEvent(dsp->cmp_syn, BKNI_INFINITE);
      }
      goto out;
   }

out_error:
   cnt = hwc2_cntLyr(dsp);
   lyr = dsp->lyr;
   for (frame_size = 0 ; frame_size < cnt ; frame_size++) {
      if (lyr->af >= 0) {
         close(lyr->af);
         lyr->af = HWC2_INVALID;
      }
      if (lyr->rf != HWC2_INVALID) {
         if ((kind == HWC2_DSP_EXT) && (lyr->cCli == HWC2_COMPOSITION_CLIENT)) {
            hwc2_lyr_tl_inc(dsp, kind, lyr->hdl, HWC2_MAGIC);
         } else {
            hwc2_lyr_tl_inc(dsp, kind, lyr->hdl, lyr->thdl);
            hwc2_lyr_tl_dequeued(dsp, kind, lyr->hdl, lyr->thdl);
         }
      }
      lyr = lyr->next;
   }
   goto out;
out_signal:
   hwc2_ret_inc(dsp, 1);
out:
   cnt = hwc2_cntLyr(dsp);
   lyr = dsp->lyr;
   for (frame_size = 0 ; frame_size < cnt ; frame_size++) {
      lyr->af = HWC2_INVALID;
      lyr = lyr->next;
   }
   ALOGE_IF((ret!=HWC2_ERROR_NONE)||HWC2_LOGRET_ALWAYS,
      "<- %s:%" PRIu64 " (%s)\n",
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

static void hwc2_bcm_close(
   struct hwc2_bcm_device_t *hwc2) {

   struct hwc2_lyr_t *lyr, *lyr2 = NULL;
   struct hwc2_dsp_cfg_t *cfg, *cfg2 = NULL;
   size_t num;

   if (hwc2->vd) {
      ALOGW("[hwc2]: removing virtual-display: %" PRIu64 "\n",
            (hwc2_display_t)(intptr_t)hwc2->vd);
      hwc2_destroy_vd(hwc2);
      free(hwc2->vd);
   }

   if (hwc2->ext) {
      ALOGW("[hwc2]: removing external-display: %" PRIu64 "\n",
            (hwc2_display_t)(intptr_t)hwc2->vd);

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
            hwc2_lyr_tl_unset(hwc2->vd, HWC2_DSP_EXT, lyr->hdl, lyr->thdl);
            free(lyr);
            lyr = lyr2;
         }
      }
      NEXUS_SurfaceClient_Release(hwc2->ext->u.ext.sch);
      NxClient_Free(&hwc2->ext->u.ext.nxa);
      hwc2->ext->cmp_active = 0;
      BKNI_SetEvent(hwc2->ext->cmp_evt);
      pthread_join(hwc2->ext->cmp, NULL);
      pthread_mutex_destroy(&hwc2->ext->mtx_cmp_wl);
      pthread_mutex_destroy(&hwc2->ext->mtx_lyr);
      pthread_mutex_destroy(&hwc2->ext->u.ext.mtx_fbs);
      pthread_mutex_destroy(&hwc2->ext->mtx_cfg);
      BKNI_DestroyEvent(hwc2->ext->cmp_evt);
      BKNI_DestroyEvent(hwc2->ext->cmp_syn);
      for (num = 0 ; num < HWC2_MAX_TL ; num++) {
         pthread_mutex_destroy(&hwc2->ext->u.ext.rtl[num].mtx_lc);
         close(hwc2->ext->u.ext.rtl[num].tl);
      }
      close(hwc2->ext->u.ext.ct.rtl.tl);
      close(hwc2->ext->cmp_tl);
      for (num = 0 ; num < HWC2_FBS_NUM ; num++) {
         close(hwc2->ext->u.ext.fbs[num].fd);
         NEXUS_Surface_Destroy(hwc2->ext->u.ext.fbs[num].s);
      }
      if (hwc2_enabled(hwc2_tweak_fb_compressed)) {
         close(hwc2->ext->u.ext.yvi.fd);
         NEXUS_Surface_Destroy(hwc2->ext->u.ext.yvi.s);
      }
      close(hwc2->ext->u.ext.icb.fd);
      NEXUS_Surface_Destroy(hwc2->ext->u.ext.icb.s);
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

   if (hwc2->hp) {
      hwc2->nxipc->unregHpdEvt(hwc2->hp->get());
      delete hwc2->hp;
   }
   if (hwc2->dc) {
      hwc2->nxipc->unregDspEvt(hwc2->dc->get());
      delete hwc2->dc;
   }
   hwc2->nxipc->leave();
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

static void hwc2_dim(
   struct hwc2_bcm_device_t* hwc2,
   NEXUS_SurfaceHandle s,
   struct hwc2_lyr_t *lyr) {
   NEXUS_Error rc;
   if (s) {
      NEXUS_Graphics2DFillSettings fs;
      float fa = fmax(0.0, fmin(1.0, lyr->al));
      uint32_t al = floor(fa == 1.0 ? 255 : fa * 256.0);
      NEXUS_Graphics2D_GetDefaultFillSettings(&fs);
      fs.surface = s;
      fs.color   = (NEXUS_Pixel)al<<HWC2_ASHIFT;
      fs.colorOp = NEXUS_FillOp_eBlend;
      fs.alphaOp = NEXUS_FillOp_eIgnore;
      if (pthread_mutex_lock(&hwc2->mtx_g2d)) {
         return;
      }
      NEXUS_Graphics2D_Fill(hwc2->hg2d, &fs);
      pthread_mutex_unlock(&hwc2->mtx_g2d);
      ALOGI_IF((hwc2->lm & LOG_DIM_DEBUG),
               "[dim]: %p with color %08x\n", s, fs.color);
   }
}

static void hwc2_pah(
   struct hwc2_bcm_device_t* hwc2,
   NEXUS_SurfaceHandle s,
   NEXUS_Rect *pah) {
   if (s) {
      NEXUS_Graphics2DFillSettings fs;
      NEXUS_Graphics2D_GetDefaultFillSettings(&fs);
      fs.surface = s;
      fs.colorOp = NEXUS_FillOp_eBlend;
      fs.alphaOp = NEXUS_FillOp_eCopy;
      fs.color   = HWC2_TRS;
      fs.rect.x  = pah->x;
      fs.rect.y  = pah->y;
      fs.rect.width  = pah->width;
      fs.rect.height = pah->height;
      if (pthread_mutex_lock(&hwc2->mtx_g2d)) {
         return;
      }
      NEXUS_Graphics2D_Fill(hwc2->hg2d, &fs);
      pthread_mutex_unlock(&hwc2->mtx_g2d);
   }
}

#define SHIFT_FACTOR 10
static NEXUS_Graphics2DColorMatrix g_hwc2_ai32_Matrix_YCbCrtoRGB = {
   SHIFT_FACTOR, {
   (int32_t) ( 1.164f * (1 << SHIFT_FACTOR)),   /*  Y factor for R */
   (int32_t) 0,                                 /* Cb factor for R */
   (int32_t) ( 1.596f * (1 << SHIFT_FACTOR)),   /* Cr factor for R */
   (int32_t) 0,                                 /*  A factor for R */
   (int32_t) (-223 * (1 << SHIFT_FACTOR)),      /* Increment for R */
   (int32_t) ( 1.164f * (1 << SHIFT_FACTOR)),   /*  Y factor for G */
   (int32_t) (-0.391f * (1 << SHIFT_FACTOR)),   /* Cb factor for G */
   (int32_t) (-0.813f * (1 << SHIFT_FACTOR)),   /* Cr factor for G */
   (int32_t) 0,                                 /*  A factor for G */
   (int32_t) (134 * (1 << SHIFT_FACTOR)),       /* Increment for G */
   (int32_t) ( 1.164f * (1 << SHIFT_FACTOR)),   /*  Y factor for B */
   (int32_t) ( 2.018f * (1 << SHIFT_FACTOR)),   /* Cb factor for B */
   (int32_t) 0,                                 /* Cr factor for B */
   (int32_t) 0,                                 /*  A factor for B */
   (int32_t) (-277 * (1 << SHIFT_FACTOR)),      /* Increment for B */
   (int32_t) 0,                                 /*  Y factor for A */
   (int32_t) 0,                                 /* Cb factor for A */
   (int32_t) 0,                                 /* Cr factor for A */
   (int32_t) (1 << SHIFT_FACTOR),               /*  A factor for A */
   (int32_t) 0,                                 /* Increment for A */
   }
};

int hwc2_blit_yv12(
   struct hwc2_bcm_device_t* hwc2,
   NEXUS_SurfaceHandle d,
   struct hwc2_lyr_t *lyr,
   PSHARED_DATA shared,
   struct hwc2_dsp_t *dsp,
   enum hwc2_seeding_e *ms) {

   NEXUS_SurfaceHandle cb = NULL, cr = NULL, y = NULL, yuv = NULL, rgba;
   void *buffer, *next, *slock;
   BM2MC_PACKET_Plane pcb, pcr, py, pycbcr, prgba;
   size_t size;
   unsigned int cs = 0, cr_o = 0, cb_o = 0;
   NEXUS_Rect c, p, oa;
   NEXUS_Error rc;
   int blt = 0;
   NEXUS_SurfaceStatus ss;
   int align = 16; /* hardcoded for format. */

   BM2MC_PACKET_Blend cbClr = {BM2MC_PACKET_BlendFactor_eSourceColor, BM2MC_PACKET_BlendFactor_eOne, false,
                               BM2MC_PACKET_BlendFactor_eDestinationColor, BM2MC_PACKET_BlendFactor_eOne, false,
                               BM2MC_PACKET_BlendFactor_eZero};
   BM2MC_PACKET_Blend cpClr = {BM2MC_PACKET_BlendFactor_eSourceColor, BM2MC_PACKET_BlendFactor_eOne, false,
                               BM2MC_PACKET_BlendFactor_eZero, BM2MC_PACKET_BlendFactor_eZero, false,
                               BM2MC_PACKET_BlendFactor_eZero};
   BM2MC_PACKET_Blend cpAlp = {BM2MC_PACKET_BlendFactor_eSourceAlpha, BM2MC_PACKET_BlendFactor_eOne, false,
                               BM2MC_PACKET_BlendFactor_eZero, BM2MC_PACKET_BlendFactor_eZero, false,
                               BM2MC_PACKET_BlendFactor_eZero};

   c = {(int16_t)lyr->crp.left,
        (int16_t)lyr->crp.top,
        (uint16_t)(lyr->crp.right - lyr->crp.left),
        (uint16_t)(lyr->crp.bottom - lyr->crp.top)};
   p = {(int16_t)lyr->fr.left,
        (int16_t)lyr->fr.top,
        (uint16_t)(lyr->fr.right - lyr->fr.left),
        (uint16_t)(lyr->fr.bottom - lyr->fr.top)};

   rgba = d;
   NEXUS_Surface_GetStatus(rgba, &ss);
   if (ss.pixelFormat == NEXUS_PixelFormat_eCompressed_A8_R8_G8_B8) {
      rgba = hwc2->ext->u.ext.yvi.s;
      NEXUS_Surface_GetStatus(rgba, &ss);
   }

   if (!hwc2_lyr_adj(dsp, &c, &p, &ss)) {
      blt = HWC2_INVALID;
      goto out;
   }
   oa = p;

   /* first blit check if we need to seed. */
   if (*ms != hwc2_seeding_none) {
      if (!dsp->sfb) {
         if ((*ms == hwc2_seeding_vid) ||
             ((*ms == hwc2_seeding_gfx) &&
              ((oa.x+oa.width < ss.width) || (oa.y+oa.height < ss.height)))) {
            hwc2_fb_seed(hwc2, d, (*ms == hwc2_seeding_vid) ? HWC2_TRS : HWC2_OPQ);
            hwc2_chkpt(hwc2);
            ALOGI_IF((dsp->lm & LOG_COMP_DEBUG),
                     "[%s]:[frame]:%" PRIu64 ":%" PRIu64 ": seed (%s)\n",
                     (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext",
                     dsp->pres, dsp->post, (*ms == hwc2_seeding_vid) ? "transparent" : "opaque");
         }
      }
      *ms = hwc2_seeding_none;
   }

   y = hwc_to_nsc_surface(shared->container.width, shared->container.height,
                          shared->container.stride, NEXUS_PixelFormat_eY8,
                          shared->container.block, 0);
   NEXUS_Surface_Lock(y, &slock);
   NEXUS_Surface_Flush(y);

   cs = (shared->container.stride/2 + (align-1)) & ~(align-1);
   cr_o = shared->container.stride * shared->container.height;
   cb_o = cr_o + ((shared->container.height/2) * ((shared->container.stride/2 + (align-1)) & ~(align-1)));

   cr = hwc_to_nsc_surface(shared->container.width/2, shared->container.height/2,
                           cs, NEXUS_PixelFormat_eCr8,
                           shared->container.block, cr_o);
   NEXUS_Surface_Lock(cr, &slock);
   NEXUS_Surface_Flush(cr);

   cb = hwc_to_nsc_surface(shared->container.width/2, shared->container.height/2,
                           cs, NEXUS_PixelFormat_eCb8,
                           shared->container.block, cb_o);
   NEXUS_Surface_Lock(cb, &slock);
   NEXUS_Surface_Flush(cb);

   yuv = hwc_to_nsc_surface(shared->container.width, shared->container.height,
                            shared->container.width * shared->container.bpp,
                            NEXUS_PixelFormat_eY08_Cb8_Y18_Cr8,
                            0, 0);
   NEXUS_Surface_Lock(yuv, &slock);
   NEXUS_Surface_Flush(yuv);

   if (pthread_mutex_lock(&hwc2->mtx_g2d)) {
      ALOGE("[%s]:[yv12]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": failed g2d (packet buffer).\n",
            (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext", lyr->hdl, dsp->pres, dsp->post);
      blt = HWC2_INVALID;
      goto out;
   } else {
      NEXUS_Graphics2D_GetPacketBuffer(hwc2->hg2d, &buffer, &size, 1024);
      pthread_mutex_unlock(&hwc2->mtx_g2d);

      NEXUS_Surface_LockPlaneAndPalette(y,    &py,     NULL);
      NEXUS_Surface_LockPlaneAndPalette(cb,   &pcb,    NULL);
      NEXUS_Surface_LockPlaneAndPalette(cr,   &pcr,    NULL);
      NEXUS_Surface_LockPlaneAndPalette(yuv,  &pycbcr, NULL);
      NEXUS_Surface_LockPlaneAndPalette(rgba, &prgba,  NULL);

      next = buffer;
      {
      BM2MC_PACKET_PacketFilterEnable *pPacket = (BM2MC_PACKET_PacketFilterEnable *)next;
      BM2MC_PACKET_INIT(pPacket, FilterEnable, false );
      pPacket->enable          = 0;
      next = ++pPacket;
      }{
      BM2MC_PACKET_PacketSourceFeeders *pPacket = (BM2MC_PACKET_PacketSourceFeeders *)next;
      BM2MC_PACKET_INIT(pPacket, SourceFeeders, false );
      pPacket->plane0          = pcb;
      pPacket->plane1          = pcr;
      pPacket->color           = 0;
      next = ++pPacket;
      }{
      BM2MC_PACKET_PacketDestinationFeeder *pPacket = (BM2MC_PACKET_PacketDestinationFeeder *)next;
      BM2MC_PACKET_INIT(pPacket, DestinationFeeder, false );
      pPacket->plane           = py;
      pPacket->color           = 0;
      next = ++pPacket;
      }{
      BM2MC_PACKET_PacketOutputFeeder *pPacket = (BM2MC_PACKET_PacketOutputFeeder *)next;
      BM2MC_PACKET_INIT(pPacket, OutputFeeder, false);
      pPacket->plane           = pycbcr;
      next = ++pPacket;
      }{
      BM2MC_PACKET_PacketBlend *pPacket = (BM2MC_PACKET_PacketBlend *)next;
      BM2MC_PACKET_INIT( pPacket, Blend, false );
      pPacket->color_blend     = cbClr;
      pPacket->alpha_blend     = cpAlp;
      pPacket->color           = 0;
      next = ++pPacket;
      }{
      BM2MC_PACKET_PacketScaleBlendBlit *pPacket = (BM2MC_PACKET_PacketScaleBlendBlit *)next;
      BM2MC_PACKET_INIT(pPacket, ScaleBlendBlit, true);
      pPacket->src_rect.x      = 0;
      pPacket->src_rect.y      = 0;
      pPacket->src_rect.width  = pcb.width;
      pPacket->src_rect.height = pcb.height;
      pPacket->out_rect.x      = 0;
      pPacket->out_rect.y      = 0;
      pPacket->out_rect.width  = pycbcr.width;
      pPacket->out_rect.height = pycbcr.height;
      pPacket->dst_point.x     = 0;
      pPacket->dst_point.y     = 0;
      next = ++pPacket;
      }{
      BM2MC_PACKET_PacketSourceFeeder *pPacket = (BM2MC_PACKET_PacketSourceFeeder *)next;
      BM2MC_PACKET_INIT(pPacket, SourceFeeder, false );
      pPacket->plane           = pycbcr;
      pPacket->color           = 0xFF000000; /* add alpha, opaque. */
      next = ++pPacket;
      }{
      BM2MC_PACKET_PacketSourceControl *pPacket = (BM2MC_PACKET_PacketSourceControl *)next;
      BM2MC_PACKET_INIT(pPacket, SourceControl, false);
      pPacket->chroma_filter   = true;
      next = ++pPacket;
      }{
      BM2MC_PACKET_PacketDestinationNone *pPacket = (BM2MC_PACKET_PacketDestinationNone *)next;
      BM2MC_PACKET_INIT(pPacket, DestinationNone, false);
      next = ++pPacket;
      }{
      BM2MC_PACKET_PacketOutputFeeder *pPacket = (BM2MC_PACKET_PacketOutputFeeder *)next;
      BM2MC_PACKET_INIT(pPacket, OutputFeeder, false);
      pPacket->plane           = prgba;
      next = ++pPacket;
      }{
      BM2MC_PACKET_PacketOutputControl *pPacket = (BM2MC_PACKET_PacketOutputControl *)next;
      BM2MC_PACKET_INIT(pPacket, OutputControl, false);
      pPacket->chroma_filter    = true;
      next = ++pPacket;
      }{
      BM2MC_PACKET_PacketBlend *pPacket = (BM2MC_PACKET_PacketBlend *)next;
      BM2MC_PACKET_INIT(pPacket, Blend, false );
      pPacket->color_blend     = cpClr;
      pPacket->alpha_blend     = cpAlp;
      pPacket->color           = 0;
      next = ++pPacket;
      }{
      BM2MC_PACKET_PacketFilterEnable *pPacket = (BM2MC_PACKET_PacketFilterEnable *)next;
      BM2MC_PACKET_INIT(pPacket, FilterEnable, false);
      pPacket->enable          = 1;
      next = ++pPacket;
      }{
      BM2MC_PACKET_PacketSourceColorMatrixEnable *pPacket = (BM2MC_PACKET_PacketSourceColorMatrixEnable *)next;
      BM2MC_PACKET_INIT(pPacket, SourceColorMatrixEnable, false);
      pPacket->enable          = 1;
      next = ++pPacket;
      }{
      BM2MC_PACKET_PacketSourceColorMatrix *pPacket = (BM2MC_PACKET_PacketSourceColorMatrix *)next;
      BM2MC_PACKET_INIT(pPacket, SourceColorMatrix, false);
      NEXUS_Graphics2D_ConvertColorMatrix(&g_hwc2_ai32_Matrix_YCbCrtoRGB, &pPacket->matrix);
      next = ++pPacket;
      }{
      BM2MC_PACKET_PacketScaleBlit *pPacket = (BM2MC_PACKET_PacketScaleBlit *)next;
      BM2MC_PACKET_INIT(pPacket, ScaleBlit, true);
      pPacket->src_rect.x       = 0;
      pPacket->src_rect.y       = 0;
      pPacket->src_rect.width   = pycbcr.width;
      pPacket->src_rect.height  = pycbcr.height;
      pPacket->out_rect.x       = oa.x;
      pPacket->out_rect.y       = oa.y;
      pPacket->out_rect.width   = oa.width;
      pPacket->out_rect.height  = oa.height;
      next = ++pPacket;
      }{
      BM2MC_PACKET_PacketSourceColorMatrixEnable *pPacket = (BM2MC_PACKET_PacketSourceColorMatrixEnable *)next;
      BM2MC_PACKET_INIT(pPacket, SourceColorMatrixEnable, false);
      pPacket->enable          = 0;
      next = ++pPacket;
      }

      if (pthread_mutex_lock(&hwc2->mtx_g2d)) {
         ALOGE("[%s]:[yv12]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": failed g2d (complete).\n",
               (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext", lyr->hdl, dsp->pres, dsp->post);
         NEXUS_Surface_UnlockPlaneAndPalette(cb);
         NEXUS_Surface_UnlockPlaneAndPalette(cr);
         NEXUS_Surface_UnlockPlaneAndPalette(y);
         NEXUS_Surface_UnlockPlaneAndPalette(yuv);
         NEXUS_Surface_UnlockPlaneAndPalette(rgba);
         blt = HWC2_INVALID;
         goto out;
      } else {
         rc = NEXUS_Graphics2D_PacketWriteComplete(hwc2->hg2d, (uint8_t*)next - (uint8_t*)buffer);
         rc = hwc2_chkpt_l(hwc2);
         if (rgba != d) {
            NEXUS_Graphics2DBlitSettings bs;
            NEXUS_Graphics2D_GetDefaultBlitSettings(&bs);
            bs.source.surface = rgba;
            bs.source.rect    = oa;
            bs.dest.surface   = d;
            bs.output.surface = d;
            bs.output.rect    = oa;
            rc = NEXUS_Graphics2D_Blit(hwc2->hg2d, &bs);
            rc = hwc2_chkpt_l(hwc2);
         }
         pthread_mutex_unlock(&hwc2->mtx_g2d);
      }
      NEXUS_Surface_UnlockPlaneAndPalette(cb);
      NEXUS_Surface_UnlockPlaneAndPalette(cr);
      NEXUS_Surface_UnlockPlaneAndPalette(y);
      NEXUS_Surface_UnlockPlaneAndPalette(yuv);
      NEXUS_Surface_UnlockPlaneAndPalette(rgba);
   }

out:
   if (cb != NULL) {
      NEXUS_Surface_Unlock(cb);
      NEXUS_Surface_Destroy(cb);
   }
   if (cr != NULL) {
      NEXUS_Surface_Unlock(cr);
      NEXUS_Surface_Destroy(cr);
   }
   if (y != NULL) {
      NEXUS_Surface_Unlock(y);
      NEXUS_Surface_Destroy(y);
   }
   if (yuv != NULL) {
      NEXUS_Surface_Unlock(yuv);
      NEXUS_Surface_Destroy(yuv);
   }
   return blt;
}

int hwc2_blit_gpx_pm(
   struct hwc2_bcm_device_t* hwc2,
   NEXUS_SurfaceHandle s,
   NEXUS_SurfaceHandle d,
   struct hwc2_lyr_t *lyr,
   struct hwc2_dsp_t *dsp,
   NEXUS_Rect sa,
   uint32_t al) {

   int blt = 0;
   NEXUS_Error rc;
   NEXUS_Graphics2DBlitSettings bs;

   ALOGI_IF((dsp->lm & LOG_ICB_DEBUG),
            "[%s]:[blit-pm]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": {%d,%08x} {%d,%d,%dx%d,%p} out:{%p}\n",
            (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext", lyr->hdl, dsp->pres, dsp->post,
            lyr->bm, al<<HWC2_ASHIFT,
            sa.x, sa.y, sa.width, sa.height, s, d);

   NEXUS_Graphics2D_GetDefaultBlitSettings(&bs);
   bs.source.surface = s;
   bs.source.rect    = sa;
   bs.dest.surface   = d;
   bs.dest.rect      = sa;
   bs.output.surface = d;
   bs.output.rect    = sa;
   bs.alphaOp        = NEXUS_BlitAlphaOp_eUseBlendEquation;
   bs.alphaBlend     = hwc2_apma_be;
   bs.constantColor  = (NEXUS_Pixel)al<<HWC2_ASHIFT;
   if (lyr->bm == HWC2_BLEND_MODE_PREMULTIPLIED) {
      bs.colorOp     = NEXUS_BlitColorOp_eUseBlendEquation;
      bs.colorBlend  = hwc2_apmc_be;
   } else {
      bs.colorOp     = NEXUS_BlitColorOp_eCopySource;
   }

   if (pthread_mutex_lock(&hwc2->mtx_g2d)) {
      ALOGE("[%s]:[blit-pm]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": failed g2d mutex.\n",
            (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext", lyr->hdl, dsp->pres, dsp->post);
      blt = HWC2_INVALID;
      goto out;
   } else {
      rc = NEXUS_Graphics2D_Blit(hwc2->hg2d, &bs);
      if (rc == NEXUS_GRAPHICS2D_QUEUE_FULL) {
         rc = hwc2_chkpt_l(hwc2);
         if (!rc) {
            rc = NEXUS_Graphics2D_Blit(hwc2->hg2d, &bs);
         }
      } else {
         rc = hwc2_chkpt_l(hwc2);
      }
      if (rc) {
         ALOGE("[%s]:[blit-pm]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": failure to blit.\n",
               (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext", lyr->hdl, dsp->pres, dsp->post);
         blt = HWC2_INVALID;
      }
      pthread_mutex_unlock(&hwc2->mtx_g2d);
   }

out:
   return blt;
}

int hwc2_blit_gpx(
   struct hwc2_bcm_device_t* hwc2,
   NEXUS_SurfaceHandle d,
   struct hwc2_lyr_t *lyr,
   PSHARED_DATA shared,
   struct hwc2_dsp_t *dsp,
   enum hwc2_seeding_e *ms,
   size_t cnt) {

   NEXUS_SurfaceHandle s = NULL, icb = NULL;
   NEXUS_SurfaceStatus ds;
   NEXUS_Graphics2DBlitSettings bs;
   NEXUS_Rect c, p, sa, da, oa, n, ct;
   NEXUS_Error rc;
   int blt = 0;

   float fa = fmax(0.0, fmin(1.0, lyr->al));
   uint32_t al = floor(fa == 1.0 ? 255 : fa * 256.0);

   NEXUS_Surface_GetStatus(d, &ds);

   c = {(int16_t)lyr->crp.left,
        (int16_t)lyr->crp.top,
        (uint16_t)(lyr->crp.right - lyr->crp.left),
        (uint16_t)(lyr->crp.bottom - lyr->crp.top)};
   p = {(int16_t)lyr->fr.left,
        (int16_t)lyr->fr.top,
        (uint16_t)(lyr->fr.right - lyr->fr.left),
        (uint16_t)(lyr->fr.bottom - lyr->fr.top)};
   oa = {(int16_t)0,  /* use 0's to mean 'whole surface'. */
         (int16_t)0,
         (uint16_t)0,
         (uint16_t)0};
   n = {(int16_t)0,
        (int16_t)0,
        (uint16_t)0,
        (uint16_t)0};

   if (!hwc2_lyr_adj(dsp, &c, &p, &ds)) {
      blt = HWC2_INVALID;
      goto out;
   }
   sa = c;
   da = p;
   if (!dsp->sfb) {
      oa = da;
   }

   if (lyr->scmp) {
      /* don't blit anything if we can skip this layer. */
      blt = HWC2_INVALID;
      goto out;
   }

   if (lyr->cCli == HWC2_COMPOSITION_CLIENT) {
      ct = {(int16_t)0,
            (int16_t)0,
            (uint16_t)shared->container.width,
            (uint16_t)shared->container.height};
      sa = ct;
      da = ct;
      oa = ct;
   }

   /* first blit check if we need to seed. */
   if (*ms != hwc2_seeding_none) {
      if (!dsp->sfb) {
         if ((*ms == hwc2_seeding_vid) ||
             ((*ms == hwc2_seeding_gfx) &&
              ((oa.x+oa.width < ds.width) || (oa.y+oa.height < ds.height)))) {
            hwc2_fb_seed(hwc2, d, (*ms == hwc2_seeding_vid) ? HWC2_TRS : HWC2_OPQ);
            hwc2_chkpt(hwc2);
            ALOGI_IF((dsp->lm & LOG_COMP_DEBUG),
                     "[%s]:[frame]:%" PRIu64 ":%" PRIu64 ": seed (%s)\n",
                     (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext",
                     dsp->pres, dsp->post, (*ms == hwc2_seeding_vid) ? "transparent" : "opaque");
         }
      }
      *ms = hwc2_seeding_none;
   }

   if (!memcmp(&sa, &n, sizeof(n)) &&
       !memcmp(&da, &n, sizeof(n))) {
      /* don't blit anything if passed in 0's. */
      blt = HWC2_INVALID;
      goto out;
   }

   if (sa.x+sa.width > (int16_t)shared->container.width ||
       sa.y+sa.height > (int16_t)shared->container.height) {
      ALOGE("[%s]:[blit]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": rejecting {%d,%d:%d}{%d,%d:%d}.\n",
            (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext", lyr->hdl, dsp->pres, dsp->post,
            sa.x, sa.width, shared->container.width,
            sa.y, sa.height, shared->container.height);
      blt = HWC2_INVALID;
      goto out;
   }

   if ((ds.pixelFormat == NEXUS_PixelFormat_eCompressed_A8_R8_G8_B8) &&
       (((sa.x+sa.width)<=1) || ((sa.y+sa.height)<=1))) {
      ALOGW("[%s]:[blit]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": ignoring {%d,%d:%d}{%d,%d:%d}.\n",
            (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext", lyr->hdl, dsp->pres, dsp->post,
            sa.x, sa.width, shared->container.width,
            sa.y, sa.height, shared->container.height);
      blt = HWC2_INVALID;
      goto out;
   }

   s = hwc_to_nsc_surface(
         shared->container.width,
         shared->container.height,
         ((private_handle_t *)lyr->bh)->oglStride,
         gr2nx_pixel(shared->container.format),
         shared->container.block, 0);
   if (s == NULL) {
      ALOGE("[%s]:[blit]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": failed to get surface.\n",
            (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext", lyr->hdl, dsp->pres, dsp->post);
      blt = HWC2_INVALID;
      goto out;
   }

   if (al<<HWC2_ASHIFT != HWC2_OPQ) {
      icb = hwc2->ext->u.ext.icb.s;
      blt = hwc2_blit_gpx_pm(hwc2, s, icb, lyr, dsp, sa, al);
      if (blt == HWC2_INVALID) {
         icb = NULL;
      }
   }

   if (hwc2->ext->dmp == hwc2_record_dump_inter ||
       hwc2->ext->dmp == hwc2_record_dump_both) {
      NEXUS_SurfaceStatus ss;
      void *vaddr;
      struct hwc2_dump_data_t dmp;

      NEXUS_Surface_GetStatus(s, &ss);
      dmp.final = false;
      dmp.lyr   = cnt+1;
      dmp.cmp   = dsp->post;
      dmp.w     = ss.width;
      dmp.h     = ss.height;
      dmp.fmt   = ss.pixelFormat;
      dmp.size  = (size_t)(ss.height*ss.pitch);
      NEXUS_MemoryBlock_Lock(shared->container.block, &vaddr);
      dmp.ptr   = (uint8_t *)vaddr;
      hwc2_dump_content(&dmp);
      NEXUS_MemoryBlock_Unlock(shared->container.block);
   }

   ALOGI_IF((dsp->lm & LOG_RGBA_DEBUG),
            "[%s]:[blit]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": {%d,%08x,%s} {%d,%d,%dx%d,%p} out:{%d,%d,%dx%d,%p} dst:{%d,%d} blk:%p::p-blk:%p\n",
            (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext", lyr->hdl, dsp->pres, dsp->post,
            lyr->bm, al<<HWC2_ASHIFT, getTransformName(lyr->tr),
            sa.x, sa.y, sa.width, sa.height, icb?icb:s,
            oa.x, oa.y, oa.width, oa.height, d,
            ds.width, ds.height,
            lyr->bh, shared->container.block);

   NEXUS_Graphics2D_GetDefaultBlitSettings(&bs);
   bs.source.surface = icb?icb:s;
   bs.source.rect    = sa;
   bs.dest.surface   = d;
   bs.dest.rect      = da;
   bs.output.surface = d;
   bs.output.rect    = oa;
   bs.colorOp        = NEXUS_BlitColorOp_eUseBlendEquation;
   bs.alphaOp        = NEXUS_BlitAlphaOp_eUseBlendEquation;
   bs.colorBlend     = (lyr->bm == HWC2_BLEND_MODE_INVALID) ? hwc2_a2n_col_be[HWC2_BLEND_MODE_NONE] : hwc2_a2n_col_be[lyr->bm];
   bs.alphaBlend     = (lyr->bm == HWC2_BLEND_MODE_INVALID) ? hwc2_a2n_al_be[HWC2_BLEND_MODE_NONE] : hwc2_a2n_al_be[lyr->bm];
   if (HWC2_MEMC_ROT) {
      if (lyr->tr == HWC_TRANSFORM_ROT_180 || lyr->tr == HWC_TRANSFORM_ROT_270) {
         bs.mirrorOutputVertically = true;
      } else if (lyr->tr == HWC_TRANSFORM_ROT_90) {
         bs.mirrorOutputHorizontally = true;
      }
   }

   if (pthread_mutex_lock(&hwc2->mtx_g2d)) {
      ALOGE("[%s]:[blit]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": failed g2d mutex.\n",
            (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext", lyr->hdl, dsp->pres, dsp->post);
      blt = HWC2_INVALID;
      goto out;
   } else {
      rc = NEXUS_Graphics2D_Blit(hwc2->hg2d, &bs);
      if (rc == NEXUS_GRAPHICS2D_QUEUE_FULL) {
         rc = hwc2_chkpt_l(hwc2);
         if (!rc) {
            rc = NEXUS_Graphics2D_Blit(hwc2->hg2d, &bs);
         }
      } else {
         rc = hwc2_chkpt_l(hwc2);
      }
      if (rc) {
         ALOGE("[%s]:[blit]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": failure to blit.\n",
               (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext", lyr->hdl, dsp->pres, dsp->post);
         blt = HWC2_INVALID;
      }
      pthread_mutex_unlock(&hwc2->mtx_g2d);
   }

out:
   if (s) {
      NEXUS_Surface_Destroy(s);
      s = NULL;
   }
   return blt;
}

static void hwc2_sdb(
   struct hwc2_bcm_device_t* hwc2,
   struct hwc2_lyr_t *lyr,
   struct hwc2_dsp_t *dsp) {

   NEXUS_Rect c, p;
   struct hwc_position fr, cl;
   int index = -1;
   native_handle_t *nh = lyr->sbh;

   if (nh) {
      index = (nh->data[1] - HWC2_SDB_MAGIC);
   }
   if (index < 0 || index >= HWC_BINDER_SIDEBAND_SURFACE_SIZE) {
      ALOGE("[%s]:[sdb]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": invalid stream index: %d.\n",
            (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext", lyr->hdl, dsp->pres, dsp->post, index);
      return;
   }

   c = {(int16_t)lyr->crp.left,
        (int16_t)lyr->crp.top,
        (uint16_t)(lyr->crp.right - lyr->crp.left),
        (uint16_t)(lyr->crp.bottom - lyr->crp.top)};
   p = {(int16_t)lyr->fr.left,
        (int16_t)lyr->fr.top,
        (uint16_t)(lyr->fr.right - lyr->fr.left),
        (uint16_t)(lyr->fr.bottom - lyr->fr.top)};

   hwc2_lyr_adj(dsp, &c, &p, NULL);

   fr.x = p.x;
   fr.y = p.y;
   fr.w = p.width;
   fr.h = p.height;

   cl.x = c.x;
   cl.y = c.y;
   cl.w = c.width == (uint16_t)HWC2_INVALID ? 0 : c.width;
   cl.h = c.height == (uint16_t)HWC2_INVALID ? 0 : c.height;

   ALOGI_IF((dsp->lm & LOG_SDB_DEBUG),
            "[%s]:[sdb:%d]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": c:{%d,%d,%dx%d} f:{%d,%d,%dx%d}\n",
            (dsp->type==HWC2_DISPLAY_TYPE_VIRTUAL)?"vd":"ext", index, lyr->hdl, dsp->pres, dsp->post,
            c.x, c.y, c.width, c.height,
            p.x, p.y, p.width, p.height);

   hwc2->hb->setgeometry(HWC_BINDER_SDB, index, fr, cl, 3 /*i.e. (5-2)*/, 1);
}

static void hwc2_ext_cmp_frame(
   struct hwc2_bcm_device_t* hwc2,
   struct hwc2_frame_t *f) {

   NEXUS_Error nx;
   NEXUS_SurfaceHandle d = NULL;
   int32_t i;
   size_t c = 0;
   struct hwc2_lyr_t *lyr;
   bool is_video;
   hwc2_blend_mode_t lbm = HWC2_BLEND_MODE_INVALID;
   struct hwc2_dsp_t *dsp = hwc2->ext;
   int blt, vl = 0;
   size_t ccli = 0;
   NEXUS_Rect pah = {0,0,0,0};
   enum hwc2_seeding_e ms = hwc2_seeding_none;
   uint32_t aw = 0;
   uint32_t ah = 0;

   /* setup (if needed) then grab a destination buffer for this frame. */
   if (hwc2->ext->u.ext.cbs) {
      enum hwc2_cbs_e wcb = hwc2->ext->u.ext.cb;
      if (wcb == cbs_e_none || wcb == cbs_e_nscfb) {
         wcb = cbs_e_bypass;
      } else {
         wcb = cbs_e_nscfb;
      }
      hwc2_ext_fbs(hwc2, wcb);
      hwc2->ext->u.ext.cbs = false;
      ALOGI("[ext]:[frame]:%" PRIu64 ":%" PRIu64 ":switching into '%s' mode\n",
            dsp->pres, dsp->post, (wcb==cbs_e_bypass)?"bypass":"ncsfb");
   }
   d = hwc2_ext_fb_get(hwc2);
   if (d == NULL) {
      for (i = 0; i < f->cnt; i++) {
         lyr = &f->lyr[i];
         if (lyr->cCli == HWC2_COMPOSITION_CLIENT) {
            if (f->ftgt >= 0) {
               close(f->ftgt);
               f->ftgt = HWC2_INVALID;
            }
         } else if (lyr->cCli == HWC2_COMPOSITION_DEVICE) {
            if (lyr->af >= 0) {
               close(lyr->af);
               lyr->af = HWC2_INVALID;
            }
         }
         if (lyr->rf != HWC2_INVALID) {
            if (lyr->cCli == HWC2_COMPOSITION_CLIENT) {
               if (ccli == 0) {
                  hwc2_lyr_tl_inc(hwc2->ext, HWC2_DSP_EXT, lyr->hdl, HWC2_MAGIC);
               }
               ccli++;
            } else {
               hwc2_lyr_tl_inc(hwc2->ext, HWC2_DSP_EXT, lyr->hdl, lyr->thdl);
               hwc2_lyr_tl_dequeued(hwc2->ext, HWC2_DSP_EXT, lyr->hdl, lyr->thdl);
            }
         }
      }
      hwc2_ret_inc(hwc2->ext, 1);
      /* should this be fatal? */
      ALOGE("[ext]:[frame]:%" PRIu64 ":%" PRIu64 ":grabbing framebuffer FAILED!\n",
            dsp->pres, dsp->post);
      return;
   }

   ALOGI_IF((hwc2->lm & LOG_GLOB_COMP_DEBUG),
            "[ext]:[frame]:%" PRIu64 ":%" PRIu64 ": new frame composition (f:%d, v:%s::%d:%d)\n",
            dsp->pres, dsp->post, f->cnt, (f->vcnt || f->scnt) ? "yes" : "no", f->vcnt, f->scnt);

   /* may have to seed background. */
   if (f->cnt > (f->vcnt + f->scnt)) {
      ms = (f->vcnt + f->scnt) ? hwc2_seeding_vid : hwc2_seeding_gfx;
   }

   if (!pthread_mutex_lock(&dsp->mtx_cfg)) {
      aw = (dsp->aCfg->w > HWC2_FB_MAX_W) ? HWC2_FB_MAX_W : (int)dsp->aCfg->w;
      ah = (dsp->aCfg->h > HWC2_FB_MAX_H) ? HWC2_FB_MAX_H : (int)dsp->aCfg->h;
      pthread_mutex_unlock(&dsp->mtx_cfg);
   }

   for (i = 0; i < f->cnt; i++) {
      NEXUS_Error lrc = NEXUS_NOT_INITIALIZED, lrcp = NEXUS_NOT_INITIALIZED;
      NEXUS_MemoryBlockHandle bh = NULL, bhp = NULL;
      PSHARED_DATA shared = NULL;
      void *map = NULL;

      lyr = &f->lyr[i];
      is_video = lyr->oob;
      vl = 0;
      if (is_video && lyr->af >= 0) {
         close(lyr->af);
         lyr->af = HWC2_INVALID;
      }
      /* [i]. validate buffer as needed.
       */
      if (lyr->cCli == HWC2_COMPOSITION_DEVICE && !is_video) {
         bool lyr_err = false;
         if (lyr->bh == NULL) {
            ALOGE("[ext]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ":%s (no valid buffer)\n",
                  lyr->hdl, dsp->pres, dsp->post, getCompositionName(lyr->cCli));
            continue;
         }
         private_handle_t::get_block_handles((private_handle_t *)lyr->bh, &bh, NULL);
         lrc = hwc2_mem_lock(hwc2, bh, &map);
         shared = (PSHARED_DATA) map;
         if (lrc || shared == NULL) {
            ALOGE("[ext]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": invalid dev-shared (lbh:%p:bh:%p)\n",
                  lyr->hdl, dsp->pres, dsp->post, (private_handle_t *)lyr->bh, bh);
            if (lrc == NEXUS_SUCCESS) {
               hwc2_mem_unlock(hwc2, bh);
               lrc = NEXUS_NOT_INITIALIZED;
            }
            lyr_err = true;
         }
         if (!lyr_err && (((private_handle_t *)lyr->bh)->fmt_set != GR_NONE)) {
            bhp = (NEXUS_MemoryBlockHandle)shared->container.block;
            lrcp = hwc2_mem_lock(hwc2, bhp, &map);
            if (lrcp || map == NULL) {
               ALOGE("[ext]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": invalid dev-phys (lbh:%p:bh:%p:bhp:%p)\n",
                     lyr->hdl, dsp->pres, dsp->post, (private_handle_t *)lyr->bh, bh, bhp);
               if (lrcp == NEXUS_SUCCESS) {
                  hwc2_mem_unlock(hwc2, bhp);
                  lrcp = NEXUS_NOT_INITIALIZED;
               }
               if (lrc == NEXUS_SUCCESS) {
                  hwc2_mem_unlock(hwc2, bh);
                  lrc = NEXUS_NOT_INITIALIZED;
               }
               lyr_err = true;
            }
         }
         if (lyr_err) {
            if (lyr->af >= 0) {
               close(lyr->af);
               lyr->af = HWC2_INVALID;
            }
            continue;
         }
      }
      /* [ii]. wait as needed.
       */
      if (lyr->cCli == HWC2_COMPOSITION_SOLID_COLOR ||
          lyr->cCli == HWC2_COMPOSITION_SIDEBAND) {
         /* ...no wait on those layers. */
      } else if (lyr->cCli == HWC2_COMPOSITION_CLIENT) {
         /* ...wait on fence as needed.
          * note: this is the client target, so the fence comes from the
          *       display client target and not the layer buffer.
          * note: when a device composition is turned into client composition,
          *       we may see some 'rogue' acquire fence set for the layer,
          *       ignore those.
          */
         if (f->tgt == NULL && f->ftgt >= 0) {
            close(f->ftgt);
            f->ftgt = HWC2_INVALID;
         } else if (f->ftgt >= 0) {
            vl = sync_wait(f->ftgt, HWC2_SYNC_TO);
            close(f->ftgt);
            f->ftgt = HWC2_INVALID;
         }
      } else if (!is_video) {
         /* ...wait on fence as needed. */
         if (lyr->bh == NULL && lyr->af >= 0) {
            close(lyr->af);
            lyr->af = HWC2_INVALID;
         } else if (lyr->af >= 0) {
            vl = sync_wait(lyr->af, HWC2_SYNC_TO);
            close(lyr->af);
            lyr->af = HWC2_INVALID;
         }
      }
      /* [iii]. compose.
       */
      switch(lyr->cCli) {
      case HWC2_COMPOSITION_SOLID_COLOR:
         {
            uint8_t a = lyr->sc.a;
            if (lyr->al != 1.0) {
               float fa = fmax(0.0, fmin(1.0, lyr->al));
               uint32_t al = floor(fa == 1.0 ? 255 : fa * 256.0);
               a = (uint8_t)al;
            }
            uint32_t color = (a<<24 | lyr->sc.r<<16 | lyr->sc.g<<8 | lyr->sc.b);
            if ((color != HWC2_TRS) &&
                   ((!c && (color == HWC2_OPQ) && !f->scnt) /* background seeding. */
                 || (c && (color != HWC2_OPQ))) /* shading. */
               ) {
               hwc2_fill_blend(hwc2, d, color, lyr->bm);
               hwc2_chkpt(hwc2);
               /* [iv]. count of composed layers. */
               ms = hwc2_seeding_none;
               c++;
            }
            ALOGI_IF((dsp->lm & LOG_COMP_DEBUG),
                     "[ext]:[frame]:%" PRIu64 ":%" PRIu64 ": solid color (%02x:%02x:%02x:%02x):%08x (%zu)\n",
                     dsp->pres, dsp->post, lyr->sc.r, lyr->sc.g, lyr->sc.b, lyr->sc.a, color, c);
         }
      break;
      case HWC2_COMPOSITION_CLIENT:
         if (f->tgt != NULL) {
            if (ccli == 0) {
               bool lyr_err = false;
               /* client target is valid and we have not composed it yet...
                *
                * FALL THROUGH. */
               lyr->bh = f->tgt;
               ccli++;

               private_handle_t::get_block_handles((private_handle_t *)lyr->bh, &bh, NULL);
               lrc = hwc2_mem_lock(hwc2, bh, &map);
               shared = (PSHARED_DATA) map;
               if (lrc || shared == NULL) {
                  ALOGE("[ext][ccli]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": invalid dev-shared (lbh:%p:bh:%p)\n",
                        lyr->hdl, dsp->pres, dsp->post, (private_handle_t *)lyr->bh, bh);
                  if (lrc == NEXUS_SUCCESS) {
                     hwc2_mem_unlock(hwc2, bh);
                     lrc = NEXUS_NOT_INITIALIZED;
                  }
                  lyr_err = true;
               }
               if (!lyr_err && (((private_handle_t *)lyr->bh)->fmt_set != GR_NONE)) {
                  bhp = (NEXUS_MemoryBlockHandle)shared->container.block;
                  lrcp = hwc2_mem_lock(hwc2, bhp, &map);
                  if (lrcp || map == NULL) {
                     ALOGE("[ext][ccli]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ": invalid dev-phys (lbh:%p:bh:%p:bhp:%p)\n",
                           lyr->hdl, dsp->pres, dsp->post, (private_handle_t *)lyr->bh, bh, bhp);
                     if (lrcp == NEXUS_SUCCESS) {
                        hwc2_mem_unlock(hwc2, bhp);
                        lrcp = NEXUS_NOT_INITIALIZED;
                     }
                     if (lrc == NEXUS_SUCCESS) {
                        hwc2_mem_unlock(hwc2, bh);
                        lrc = NEXUS_NOT_INITIALIZED;
                     }
                     lyr_err = true;
                  }
               }
               if (lyr_err) {
                  break;
               }
            } else {
               ccli++;
               break;
            }
         } else if (lyr->crp.left == 0 &&
                    lyr->crp.top == 0 &&
                    lyr->crp.right == HWC2_INVALID &&
                    lyr->crp.bottom == HWC2_INVALID) {
            /* dim layer. */
            hwc2_dim(hwc2, d, lyr);
            hwc2_chkpt(hwc2);
            /* [iv]. count of composed layers. */
            ms = hwc2_seeding_none;
            c++;
            ALOGI_IF((dsp->lm & LOG_COMP_DEBUG),
                     "[ext]:[frame]:%" PRIu64 ":%" PRIu64 ": dim layer (%zu)\n",
                     dsp->pres, dsp->post, c);
            break;
         } else {
            ccli++;
            ALOGW("[ext]:[frame]:%" PRIu64 ":%" PRIu64 ": unhandled HWC2_COMPOSITION_CLIENT\n",
                  dsp->pres, dsp->post);
            break;
         }
      case HWC2_COMPOSITION_DEVICE:
         if (is_video) {
            /* offlined video pipeline through bvn, nothing to do as we signalled already
             * the frame expected to be released on display.
             */
            if (hwc2_enabled(hwc2_tweak_pip_alpha_hole)) {
               if ((uint16_t)(lyr->fr.right - lyr->fr.left) <= aw/HWC2_PAH_DIV &&
                   (uint16_t)(lyr->fr.bottom - lyr->fr.top) <= ah/HWC2_PAH_DIV) {
                  pah = {(int16_t)lyr->fr.left,
                         (int16_t)lyr->fr.top,
                         (uint16_t)(lyr->fr.right - lyr->fr.left),
                         (uint16_t)(lyr->fr.bottom - lyr->fr.top)};
                  ALOGI_IF((dsp->lm & LOG_PAH_DEBUG),
                           "[ext]:[pip-alpha-hole]:%" PRIu64 ":%" PRIu64 ": below threshold (%dx%d)\n",
                           dsp->pres, dsp->post, aw/HWC2_PAH_DIV, ah/HWC2_PAH_DIV);
               }
            }
            ALOGI_IF((dsp->lm & LOG_COMP_DEBUG),
                     "[ext]:[frame]:%" PRIu64 ":%" PRIu64 ": oob video (%zu)\n",
                     dsp->pres, dsp->post, c);
            break;
         } else {
            /* graphics or yv12 layer. */
            bool yv12 = false;
            yv12 = ((shared->container.format == HAL_PIXEL_FORMAT_YV12) ||
                    (shared->container.format == HAL_PIXEL_FORMAT_YCbCr_420_888)) ? true : false;
            if (vl != 0) {
               blt = HWC2_INVALID;
               ALOGW("[ext]:[frame]:%" PRIu64 ":%" PRIu64 ": skip on sync_wait failure.",
                     dsp->pres, dsp->post);
            } else {
               if (yv12) {
                  blt = hwc2_blit_yv12(hwc2, d, lyr, shared, dsp, &ms);
               } else {
                  blt = hwc2_blit_gpx(hwc2, d, lyr, shared, dsp, &ms, c);
               }
            }
            if (lrcp == NEXUS_SUCCESS) {
               hwc2_mem_unlock(hwc2, bhp);
               lrcp = NEXUS_NOT_INITIALIZED;
            }
            if (lrc == NEXUS_SUCCESS) {
               hwc2_mem_unlock(hwc2, bh);
               lrc = NEXUS_NOT_INITIALIZED;
            }
            /* [iv]. count of composed layers. */
            if (!blt) c++;
            ALOGI_IF(!blt && (dsp->lm & LOG_COMP_DEBUG),
                     "[ext]:[frame]:%" PRIu64 ":%" PRIu64 ": %s layer (%zu)\n",
                     dsp->pres, dsp->post, yv12?"yv12":"rgba", c);
         }
      break;
      case HWC2_COMPOSITION_SIDEBAND:
         if (hwc2_enabled(hwc2_tweak_pip_alpha_hole)) {
            if ((uint16_t)(lyr->fr.right - lyr->fr.left) <= aw/HWC2_PAH_DIV &&
                (uint16_t)(lyr->fr.bottom - lyr->fr.top) <= ah/HWC2_PAH_DIV) {
               pah = {(int16_t)lyr->fr.left,
                      (int16_t)lyr->fr.top,
                      (uint16_t)(lyr->fr.right - lyr->fr.left),
                      (uint16_t)(lyr->fr.bottom - lyr->fr.top)};
               ALOGI_IF((dsp->lm & LOG_PAH_DEBUG),
                        "[ext]:[pip-alpha-hole]:%" PRIu64 ":%" PRIu64 ": below threshold (%dx%d)\n",
                        dsp->pres, dsp->post, aw/HWC2_PAH_DIV, ah/HWC2_PAH_DIV);
            }
         }

         if (hwc2->hb) {
            hwc2_sdb(hwc2, lyr, dsp);
            ALOGI_IF((dsp->lm & LOG_COMP_DEBUG),
                     "[ext]:[frame]:%" PRIu64 ":%" PRIu64 ": sideband layer (%zu)\n",
                     dsp->pres, dsp->post, c);
         }
      break;
      case HWC2_COMPOSITION_CURSOR:
      default:
         ALOGW("[ext]:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ":%s - not handled!\n",
               lyr->hdl, dsp->pres, dsp->post, getCompositionName(lyr->cCli));
      break;
      }

      if (lyr->rf != HWC2_INVALID) {
         if (lyr->cCli == HWC2_COMPOSITION_CLIENT) {
            if (ccli == 1) {
               hwc2_lyr_tl_inc(hwc2->ext, HWC2_DSP_EXT, lyr->hdl, HWC2_MAGIC);
            }
         } else {
            hwc2_lyr_tl_inc(hwc2->ext, HWC2_DSP_EXT, lyr->hdl, lyr->thdl);
            hwc2_lyr_tl_dequeued(hwc2->ext, HWC2_DSP_EXT, lyr->hdl, lyr->thdl);
         }
      }
   }

   /* video present */
   if (f->vcnt || f->scnt) {
      /* ... make sure we seed background. */
      if (!c && (dsp->u.ext.bg == HWC2_OPQ)) {
         hwc2_fb_seed(hwc2, d, HWC2_TRS);
         dsp->u.ext.bg = HWC2_TRS;
         hwc2_chkpt(hwc2);
         ALOGI_IF((dsp->lm & LOG_COMP_DEBUG),
                  "[ext]:[frame]:%" PRIu64 ":%" PRIu64 ": seed (%s)\n",
                  dsp->pres, dsp->post, "transparent");
         c++;
      } else {
         /* ... cannot assume composition preserved background. */
         if (c) {
            dsp->u.ext.bg = HWC2_OPQ;
         }
         /* ... need to punch alpha hole for pip? */
         if ((c > 1) &&
              hwc2_enabled(hwc2_tweak_pip_alpha_hole) &&
              (pah.width && pah.height)) {
            hwc2_pah(hwc2, d, &pah);
            hwc2_chkpt(hwc2);
            ALOGI_IF((dsp->lm & LOG_PAH_DEBUG),
                     "[ext]:[pip-alpha-hole]:%" PRIu64 ":%" PRIu64 ":@{%d,%d,%dx%d}\n",
                     dsp->pres, dsp->post, pah.x, pah.y, pah.width, pah.height);
         }
      }
   } else {
      dsp->u.ext.bg = HWC2_OPQ;
   }

   if (c > 0) {
      /* [v]. push composition to display. */
      if (hwc2->ext->dmp == hwc2_record_dump_final ||
          hwc2->ext->dmp == hwc2_record_dump_both) {
         NEXUS_SurfaceStatus ss;
         NEXUS_SurfaceMemory sm;
         struct hwc2_dump_data_t dmp;

         NEXUS_Surface_GetStatus(d, &ss);
         dmp.final = true;
         dmp.cmp   = dsp->post;
         dmp.w     = ss.width;
         dmp.h     = ss.height;
         dmp.fmt   = ss.pixelFormat;
         dmp.size  = (size_t)(ss.height*ss.pitch);
         NEXUS_Surface_GetMemory(d, &sm);
         dmp.ptr   = (uint8_t *)sm.buffer;
         NEXUS_Surface_Flush(d);
         hwc2_dump_content(&dmp);
      }
      nx = NEXUS_SurfaceClient_PushSurface(hwc2->ext->u.ext.sch, d, NULL, false);
      if (nx) {
         dsp->u.ext.bg = HWC2_OPQ;
         hwc2_ext_fb_put(hwc2, d);
         /* should this be fatal? */
         ALOGE("[ext]:[frame]:%" PRIu64 ":%" PRIu64 ":push to display FAILED (%d)!\n",
               dsp->pres, dsp->post, nx);
      } else {
         ALOGI_IF((dsp->lm & LOG_COMP_SUM_DEBUG),
                  "[ext]:[frame]:%" PRIu64 ":%" PRIu64 ":%zu layer%scomposed\n",
                  dsp->pres, dsp->post, c, c>1?"s ":" ");
      }
   } else {
      /* [v]. ... or re-queue if nothing took place. */
      hwc2_ext_fb_put(hwc2, d);
      ALOGI_IF((dsp->lm & LOG_COMP_SUM_DEBUG),
               "[ext]:[frame]:%" PRIu64 ":%" PRIu64 ":no composition\n",
               dsp->pres, dsp->post);
   }
   if (!f->vcnt) {
      hwc2_ret_inc(hwc2->ext, 1);
   }
   return;
}

static void *hwc2_ext_cmp(
   void *argv) {
   struct hwc2_bcm_device_t* hwc2 = (struct hwc2_bcm_device_t *)(argv);
   struct hwc2_frame_t *frame = NULL;

   setpriority(PRIO_PROCESS, 0, PRIORITY_URGENT_DISPLAY + 2*PRIORITY_MORE_FAVORABLE);
   set_sched_policy(0, SP_FOREGROUND);

   do {
      if (pthread_mutex_lock(&hwc2->mtx_pwr)) {
         LOG_ALWAYS_FATAL("[ext]: composer failure on power mutex.");
         return NULL;
      }
      if (hwc2->ext->pmode != HWC2_POWER_MODE_OFF) {
         bool composing = true, syn = false;
         pthread_mutex_unlock(&hwc2->mtx_pwr);
         BKNI_WaitForEvent(hwc2->ext->cmp_evt, BKNI_INFINITE);
         do {
            frame = NULL;
            if (!pthread_mutex_lock(&hwc2->ext->mtx_cmp_wl)) {
               frame = hwc2->ext->cmp_wl;
               if (frame == NULL) {
                  composing = false;
               } else {
                  if (hwc2->ext->pres >= (hwc2->ext->post + HWC2_COMP_RUN)) {
                     syn = true;
                  }
                  hwc2->ext->cmp_wl = frame->next;
               }
               pthread_mutex_unlock(&hwc2->ext->mtx_cmp_wl);
            } else {
               composing = false;
            }
            if (frame != NULL) {
               hwc2->ext->post++;
               hwc2_ext_cmp_frame(hwc2, frame);
               free(frame);
               frame = NULL;
            }
            if (syn) {
               syn = false;
               BKNI_SetEvent(hwc2->ext->cmp_syn);
            }
         } while (composing);
      } else {
         pthread_mutex_unlock(&hwc2->mtx_pwr);
         BKNI_Sleep(10); /* pas beau. */
      }
   } while(hwc2->ext->cmp_active);

   return NULL;
}

static void hwc2_setup_ext(
   struct hwc2_bcm_device_t *hwc2) {

   NxClient_AllocSettings alloc;
   struct hwc2_dsp_t *ext;
   NEXUS_Error rc;
   pthread_attr_t attr;
   pthread_mutexattr_t mattr;
   NEXUS_SurfaceComposition c;
   NEXUS_InterfaceName interfaceName;
   NEXUS_PlatformObjectInstance objects[HWC2_NX_DSP_OBJ];
   size_t num = HWC2_NX_DSP_OBJ;
   NEXUS_HdmiOutputHandle hdmi;
   NEXUS_HdmiOutputStatus hstatus;
   NxClient_DisplaySettings settings;
   uint32_t cw, ch;

   ext = (struct hwc2_dsp_t *)malloc(sizeof(*ext));
   if (ext == NULL) {
      LOG_ALWAYS_FATAL("failed to allocate external display (memory).");
      return;
   }
   memset(ext, 0, sizeof(*ext));
   hwc2->ext = ext;
   snprintf(hwc2->ext->name, sizeof(hwc2->ext->name), "stbHD0");
   hwc2->ext->tlm = HWC2_TLM_MAGIC;
   hwc2->ext->type = HWC2_DISPLAY_TYPE_PHYSICAL;
   hwc2->ext->lm = property_get_int32(HWC2_LOG_EXT, 0);
   hwc2->ext->u.ext.gles = property_get_bool(HWC2_EXT_GLES, 0);
   if (hwc2->ext->u.ext.gles) {
      ALOGI("[ext]: fallback to gles composition\n");
   }

   hwc2->ext->cfgs = (struct hwc2_dsp_cfg_t *)malloc(sizeof(struct hwc2_dsp_cfg_t));
   if (hwc2->ext->cfgs) {
      hwc2->ext->cfgs->next  = NULL;
      hwc2->ext->cfgs->w     = property_get_int32(HWC2_EXT_AFB_W, HWC2_FB_MAX_W);
      hwc2->ext->cfgs->h     = property_get_int32(HWC2_EXT_AFB_H, HWC2_FB_MAX_H);
      hwc2->ext->cfgs->vsync = hwc2_fps2vsync(60); /* hardcode 60fps */
      hwc2->ext->cfgs->xdp   = 160; /* default if not connected. */
      hwc2->ext->cfgs->ydp   = 160; /* default if not connected. */
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
   BKNI_CreateEvent(&hwc2->ext->cmp_syn);
   hwc2->ext->cmp_active = 1;
   hwc2->ext->cmp_wl = NULL;
   pthread_mutexattr_init(&mattr);
   pthread_mutex_init(&hwc2->ext->mtx_cmp_wl, &mattr);
   pthread_mutex_init(&hwc2->ext->u.ext.mtx_fbs, &mattr);
   pthread_mutex_init(&hwc2->ext->mtx_lyr, &mattr);
   pthread_mutex_init(&hwc2->ext->mtx_cfg, &mattr);
   pthread_mutexattr_destroy(&mattr);
   pthread_attr_init(&attr);
   pthread_create(&hwc2->ext->cmp, &attr, hwc2_ext_cmp, (void *)hwc2);
   pthread_attr_destroy(&attr);
   pthread_setname_np(hwc2->ext->cmp, "hwc2_ext_cmp");

   hwc2->ext->cmp_tl = sw_sync_timeline_create();
   if (hwc2->ext->cmp_tl < 0) {
      hwc2->ext->cmp_tl = HWC2_INVALID;
   }
   ALOGI("[ext]: completion timeline: %d\n", hwc2->ext->cmp_tl);

   NxClient_GetSurfaceClientComposition(hwc2->ext->u.ext.nxa.surfaceClient[0].id, &c);
   c.virtualDisplay.width  = property_get_int32(HWC2_EXT_NFB_W, HWC2_FB_MAX_W);
   c.virtualDisplay.height = property_get_int32(HWC2_EXT_NFB_H, HWC2_FB_MAX_H);
   c.position.x            = 0;
   c.position.y            = 0;
   c.position.width        = c.virtualDisplay.width;
   c.position.height       = c.virtualDisplay.height;
   c.zorder                = 5; /* above any video or sideband layer(s). */
   c.visible               = true;
   c.colorBlend            = hwc2_a2n_col_be[HWC2_BLEND_MODE_PREMULTIPLIED];
   c.alphaBlend            = hwc2_a2n_al_be[HWC2_BLEND_MODE_PREMULTIPLIED];
   NxClient_SetSurfaceClientComposition(hwc2->ext->u.ext.nxa.surfaceClient[0].id, &c);

   cw = (hwc2->ext->aCfg->w > HWC2_FB_MAX_W) ? HWC2_FB_MAX_W : (int)hwc2->ext->aCfg->w;
   ch = (hwc2->ext->aCfg->h > HWC2_FB_MAX_H) ? HWC2_FB_MAX_H : (int)hwc2->ext->aCfg->h;
   if ((c.virtualDisplay.width != cw) &&
       (c.virtualDisplay.height != ch)) {
      hwc2->ext->sfb = true;
   } else {
      hwc2->ext->sfb = false;
   }

   strcpy(interfaceName.name, "NEXUS_Display");
   rc = NEXUS_Platform_GetObjects(&interfaceName, &objects[0], num, &num);
   if (!rc) {
      hwc2->ext->u.ext.hdsp = (NEXUS_DisplayHandle)objects[0].object;
   }

   /* always setup the default configuration to run hwc properly regardless
    * of the connected display.
    */
   enum hwc2_cbs_e wcb = hwc2_want_comp_bypass(&settings);
   hwc2_ext_fbs(hwc2, wcb);
   hdmi = NEXUS_HdmiOutput_Open(0+NEXUS_ALIAS_ID, NULL);
   if (hdmi) {
      NEXUS_HdmiOutput_GetStatus(hdmi, &hstatus);
      if (hstatus.connected) {
         hwc2_cfg_collect(hwc2->ext, &settings);
         hwc2_hdmi_collect(hwc2->ext, hdmi, &settings);
         hwc2_eotf(hwc2->ext);
      }
      NEXUS_HdmiOutput_Close(hdmi);
   }
   if (hwc2->regCb[HWC2_CALLBACK_HOTPLUG-1].func != NULL) {
      ALOGV("[ext]: initial hotplug CONNECTED\n");
      HWC2_PFN_HOTPLUG f_hp = (HWC2_PFN_HOTPLUG) hwc2->regCb[HWC2_CALLBACK_HOTPLUG-1].func;
      f_hp(hwc2->regCb[HWC2_CALLBACK_HOTPLUG-1].data,
           (hwc2_display_t)(intptr_t)hwc2->ext,
           (int)HWC2_CONNECTION_CONNECTED);
   }
   hwc2->ext->u.ext.rhpd = false;

   hwc2->ext->u.ext.ct.rtl.tl = sw_sync_timeline_create();
   if (hwc2->ext->u.ext.ct.rtl.tl < 0) {
      hwc2->ext->u.ext.ct.rtl.tl = HWC2_INVALID;
   }
   hwc2->ext->u.ext.ct.rtl.hdl = (uint64_t)(intptr_t)hwc2->ext;
   hwc2->ext->u.ext.ct.rtl.ix = 0;
   ALOGI("[ext]: client target completion timeline: %d\n", hwc2->ext->u.ext.ct.rtl.tl);
   for (num = 0 ; num < HWC2_MAX_TL ; num++) {
      hwc2->ext->u.ext.rtl[num].tl = sw_sync_timeline_create();
      if (hwc2->ext->u.ext.rtl[num].tl < 0) {
         hwc2->ext->u.ext.rtl[num].tl = HWC2_INVALID;
      }
      hwc2->ext->u.ext.rtl[num].hdl = 0;
      hwc2->ext->u.ext.rtl[num].ix = 0;
      pthread_mutexattr_init(&mattr);
      pthread_mutex_init(&hwc2->ext->u.ext.rtl[num].mtx_lc, &mattr);
      pthread_mutexattr_destroy(&mattr);
      ALOGI("[ext]: layer completion timeline (%zu): %d\n", num, hwc2->ext->u.ext.rtl[num].tl);
   }
}

static void hwc2_op(
   struct hwc2_dsp_t *dsp,
   struct hwc_notification_info *ntfy) {

   NEXUS_Error rc;
   NEXUS_SurfaceComposition c;
   uint32_t w, h;

   if (memcmp(&ntfy->frame, &dsp->op, sizeof(dsp->op))) {
      memcpy(&dsp->op, &ntfy->frame, sizeof(dsp->op));

      if (dsp->type == HWC2_DISPLAY_TYPE_VIRTUAL) {
         /* nothing to do, will need to be applied on demand in output buffer. */
         return;
      }

      w = h = 0;
      if (!pthread_mutex_lock(&dsp->mtx_cfg)) {
         w = (dsp->aCfg->w > HWC2_FB_MAX_W) ? HWC2_FB_MAX_W : (int)dsp->aCfg->w;
         h = (dsp->aCfg->h > HWC2_FB_MAX_H) ? HWC2_FB_MAX_H : (int)dsp->aCfg->h;
         pthread_mutex_unlock(&dsp->mtx_cfg);
      }

      /* applies to ext(ernal) display from there on. */
      NxClient_GetSurfaceClientComposition(dsp->u.ext.nxa.surfaceClient[0].id, &c);
      ALOGI("[ext]:[cur-pos]:{%d,%d;%dx%d}\n",
         c.position.x, c.position.y, c.position.width, c.position.height);
      c.position.x      = dsp->op.x;
      c.position.y      = dsp->op.y;
      c.position.width  = w + dsp->op.w;
      c.position.height = h + dsp->op.h;
      NxClient_SetSurfaceClientComposition(dsp->u.ext.nxa.surfaceClient[0].id, &c);
      ALOGI("[ext]:[new-pos]:{%d,%d;%dx%d}:op{%d,%d;%dx%d}\n",
         dsp->op.x, dsp->op.y, w + dsp->op.w, h + dsp->op.h,
         dsp->op.x, dsp->op.y, dsp->op.w, dsp->op.h);
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
   case HWC_BINDER_NTFY_VIDEO_SURFACE_ACQUIRED: {
      int i = (ntfy.surface_hdl-HWC2_VID_MAGIC);
      if (i >= 0 && i < HWC2_VID_WIN) {
         hwc2->rlpf[i] = true;
      }
   }
   break;
   case HWC_BINDER_NTFY_OVERSCAN:
      hwc2_op(hwc2->ext, &ntfy);
   break;
   case HWC_BINDER_NTFY_SIDEBAND_SURFACE_ACQUIRED:
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
      NEXUS_HdmiOutputHandle hdmi;

      if (wcb != hwc2->ext->u.ext.cb) {
         hwc2->ext->u.ext.cbs = true;
      } else {
         hwc2->ext->u.ext.cbs = false;
      }

      if (hwc2->ext->u.ext.rhpd &&
          hwc2->regCb[HWC2_CALLBACK_HOTPLUG-1].func != NULL) {
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

      hdmi = NEXUS_HdmiOutput_Open(0+NEXUS_ALIAS_ID, NULL);
      if (hdmi) {
         hwc2_cfg_collect(hwc2->ext, &settings);
         hwc2_hdmi_collect(hwc2->ext, hdmi, &settings);
         hwc2_eotf(hwc2->ext);
         NEXUS_HdmiOutput_Close(hdmi);
      }
   } else /* disconnected */ {
      if (hwc2->ext->u.ext.rhpd &&
          hwc2->regCb[HWC2_CALLBACK_HOTPLUG-1].func != NULL) {
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
   NEXUS_Graphics2DOpenSettings g2dOCfg;
   NEXUS_Graphics2DSettings g2dCfg;
   NEXUS_Error rc;

   hwc2_setup_memif(hwc2);

   hwc2->nxipc = new NxWrap("hwc2");
   if (hwc2->nxipc == NULL) {
      LOG_ALWAYS_FATAL("failed to instantiate nexus wrap.");
      return;
   }
   hwc2->nxipc->join(hwc2_stdby_mon, (void *)hwc2);

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

   hwc2_setup_ext(hwc2);

   if (hwc2->hp) {
      hwc2->nxipc->regHpdEvt(hwc2->hp->get());
      hwc2->con = false;
   }
   if (hwc2->dc) {
      hwc2->nxipc->regDspEvt(hwc2->dc->get());
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

      dev->lm = property_get_int32(HWC2_LOG_GLB, 0);
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
