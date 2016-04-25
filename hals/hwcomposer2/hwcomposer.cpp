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

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <cutils/log.h>
#include <hardware/hwcomposer2.h>


#define HWC2_MAGIC      0xBC353C02

#define HWC2_INVALID    -1
#define HWC2_MAX_REG_CB 3
#define HWC2_MAX_DUMP   4096
#define HWC2_CM_SIZE    16

#define HWC2_DSP_NAME   32
#define HWC2_DSP_EXT    2001
#define HWC2_DSP_VD     3001

#define HWC2_VD_MAX_NUM 1
#define HWC2_VD_MAX_W   1920
#define HWC2_VD_MAX_H   1080

struct hwc2_reg_cb_t {
   hwc2_callback_descriptor_t desc;
   hwc2_function_pointer_t    func;
   hwc2_callback_data_t       data;
};

struct hwc2_dsp_cfg_t {
   struct hwc2_dsp_cfg_t *next;
   uint32_t              w;
   uint32_t              h;
   uint32_t              vsync;
   uint32_t              xdpi;
   uint32_t              ydpi;
};

struct hwc2_dsp_ct_t {
   buffer_handle_t tgt;
   int32_t rdf;
   android_dataspace_t dsp;
   size_t dmg_n;
   hwc_rect_t dmg_r;
};

struct hwc2_lyr_t {
   struct hwc2_lyr_t  *next;

   hwc2_composition_t cCli; /* offered */
   hwc2_composition_t cDev; /* wanted  */
   bool               rp; /* request wanted (not composition type) */

   hwc2_blend_mode_t  bm; /* blend mode */
   buffer_handle_t    bh; /* buffer handle */
   int32_t            af; /* acquire fence for buffer 'bh' */
   hwc_color_t        sc; /* solid color */
   int32_t            dsp; /* dataspace */
   hwc_rect_t         fr; /* frame */
   hwc_rect_t         crp; /* source crop (we rounded float to int) */
   float              al; /* alpha */
   native_handle_t    *sbh; /* sideband layer native handle */
   bool               sfd; /* surface damage (false means no change) */
   hwc_transform_t    tr; /* transform - unused */
   hwc_rect_t         vis; /* visible region */
   uint32_t           z; /* z-order */
   int32_t            cx; /* cursor x-position */
   int32_t            cy; /* cursor y-position */
   int32_t            rf; /* release fence for this layer current buffer */
};

struct hwc2_cm_t {
   android_color_transform_t type;
   float mtx[HWC2_CM_SIZE];
};

struct hwc2_vd_t {
   uint32_t w;
   uint32_t h;

   buffer_handle_t output;
   int32_t wrFence;
   hwc2_cm_t cm;
   hwc2_dsp_ct_t ct;
};

struct hwc2_ext_t {
   uint32_t w;
   uint32_t h;

   hwc2_vsync_t vsync;
   hwc2_cm_t cm;
   hwc2_dsp_ct_t ct;
};

struct hwc2_dsp_t {
   hwc2_display_type_t   type;
   char                  name[HWC2_DSP_NAME];
   bool                  validated;
   union {
      struct hwc2_vd_t   vd;
      struct hwc2_ext_t  ext;
   } u;
   struct hwc2_lyr_t     *lyr;
   struct hwc2_dsp_cfg_t *aCfg;
   struct hwc2_dsp_cfg_t *cfgs;

};

struct hwc2_bcm_device_t {
   hwc2_device_t       base;
   uint32_t            magic;
   char                dump[HWC2_MAX_DUMP];

   struct hwc2_reg_cb_t regCb[HWC2_MAX_REG_CB];
   struct hwc2_dsp_t    *vd;
   struct hwc2_dsp_t    *ext;
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
         *outSupport = 1;
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
   hwc2->vd->type = HWC2_DISPLAY_TYPE_VIRTUAL;
   hwc2->vd->u.vd.w = width;
   hwc2->vd->u.vd.h = height;

   *outDisplay = (hwc2_display_t)(intptr_t)hwc2->vd;

out:
   return ret;
}

static int32_t hwc2_vdRem(
   hwc2_device_t* device,
   hwc2_display_t display) {

   hwc2_error_t ret = HWC2_ERROR_NONE;
   struct hwc2_bcm_device_t *hwc2 = (struct hwc2_bcm_device_t *)device;

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
   return ret;
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
   free(lyr);
   lyr = NULL;

out:
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
   struct hwc2_dsp_t *dsp,
   uint32_t width,
   uint32_t height) {

   hwc2_error_t ret = HWC2_ERROR_NONE;

   (void)dsp;
   (void)width;
   (void)height;

   // TODO - check maximum scale factor supported.

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
   ret = (hwc2_error_t)hwc2_sclSupp(dsp, width, height);
   if (ret != HWC2_ERROR_NONE) {
      goto out;
   }

out:
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
   case HWC2_ATTRIBUTE_DPI_X:        *outValue = cfg->xdpi; break;
   case HWC2_ATTRIBUTE_DPI_Y:        *outValue = cfg->ydpi; break;
   default:                          *outValue = HWC2_INVALID;
   }

out:
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
   return ret;
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

   // TODO: apply power mode to the display, update complete before exit.
   (void)mode;

out:
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
      memcpy(&dsp->u.vd.ct.dmg_r, damage.rects, sizeof(dsp->u.vd.ct.dmg_r));
   } else {
      dsp->u.ext.ct.tgt = target;
      dsp->u.ext.ct.rdf = acquireFence;
      dsp->u.ext.ct.dsp = (android_dataspace_t)dataspace;
      dsp->u.ext.ct.dmg_n = damage.numRects;
      memcpy(&dsp->u.ext.ct.dmg_r, damage.rects, sizeof(dsp->u.ext.ct.dmg_r));
   }

out:
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
      if (lyr->cDev == HWC2_COMPOSITION_DEVICE) {
         *outNumElements += 1;
      }
      while (lyr != NULL) {
         lyr = lyr->next;
      }
   } else {
      lyr = dsp->lyr;
      if (lyr->cDev == HWC2_COMPOSITION_DEVICE) {
         // TODO: create the actual fence.
         lyr->rf = -1;
      }
      while (lyr != NULL) {
         lyr = lyr->next;
      }
   }

out:
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
   return ret;
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

   // TODO: actual composition.

   // TODO: output fence.
   *outRetireFence = -1;

out:
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

   if (hwc2->vd) {
      free(hwc2->vd);
   }
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

static void hwc2_bcm_open(
   struct hwc2_bcm_device_t *hwc2) {

   // TODO: instantiate the 'ext' display, setup display configurations, etc...

   (void)hwc2;
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
      .author             = "Broadcom",
      .methods            = &hwc2_mod_fncs,
      .dso                = 0,
      .reserved           = {0}
   }
};
