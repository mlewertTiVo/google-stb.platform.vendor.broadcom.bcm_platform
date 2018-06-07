/******************************************************************************
 * (c) 2018 Broadcom
 *
 * This program is the proprietary software of Broadcom and/or its licensors,
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

//#define LOG_NDEBUG 0
#define LOG_TAG "libbcmsideband"

#include <log/log.h>
#include <utils/Mutex.h>
#include <system/window.h>
#include <cutils/native_handle.h>
#include <android/native_window.h>
#include <bcm/hardware/dspsvcext/1.0/IDspSvcExt.h>

#include "bcmsideband.h"

using namespace android;
using namespace android::hardware;
using namespace bcm::hardware::dspsvcext::V1_0;

Mutex lock;
#define ATTEMPT_PAUSE_USEC 500000
#define MAX_ATTEMPT_COUNT  4
static const sp<IDspSvcExt> idse(void) {
   static sp<IDspSvcExt> idse = NULL;
   Mutex::Autolock _l(lock);
   int c = 0;

   if (idse != NULL) {
      return idse;
   }

   do {
      idse = IDspSvcExt::getService();
      if (idse != NULL) {
         return idse;
      }
      usleep(ATTEMPT_PAUSE_USEC);
      c++;
   }
   while(c < MAX_ATTEMPT_COUNT);
   // can't get interface.
   return NULL;
}

class SdbGeomCb : public IDspSvcExtSdbGeomCb {
public:
   SdbGeomCb(sb_geometry_cb cb, void *cb_data): geom_cb(cb), geom_ctx(cb_data) {};
   sb_geometry_cb geom_cb;
   void *geom_ctx;
   Return<void> onGeom(int32_t i, const DspSvcExtGeom& geom);
};
static sp<SdbGeomCb> gSdbGeomCb = NULL;

struct bcmsideband_ctx {
   ANativeWindow *native_window;
   native_handle_t *native_handle;
   int32_t surface;
   int32_t index;
   void *caller;
};

Return<void> SdbGeomCb::onGeom(int32_t i, const DspSvcExtGeom& geom) {
   if (geom_cb) {
      struct bcmsideband_ctx *ctx = (struct bcmsideband_ctx *)geom_ctx;
      if (i != ctx->index) {
         ALOGW("SdbGeomCb::onGeom(%d), but registered for %d", i, ctx->index);
      } else {
         geom_cb(ctx->caller, geom.geom_x, geom.geom_y, geom.geom_w, geom.geom_h);
      }
   }
   return Void();
}

struct bcmsideband_ctx * libbcmsideband_init_sideband(
   int index, /* index in [0..HWC_BINDER_SIDEBAND_SURFACE_SIZE[ must be managed by user. */
   ANativeWindow *native_window,
   sb_geometry_cb cb) {

   struct bcmsideband_ctx *ctx;
   ctx = (struct bcmsideband_ctx *)malloc(sizeof (*ctx));
   if (!ctx) {
      ALOGE("failed to get allocate sideband context");
      return NULL;
   }

   if (idse() != NULL) {
      gSdbGeomCb = new SdbGeomCb(cb, ctx);
      ctx->index = index;
      ctx->surface = idse()->regSdbCb(index, NULL);
      ctx->surface = idse()->regSdbCb(index, gSdbGeomCb);
   } else {
      ALOGE("failed to get dspsvcext service");
      free(ctx);
      return NULL;
   }

   native_handle_t *native_handle = native_handle_create(0, 2);
   if (!native_handle) {
      gSdbGeomCb = NULL;
      free(ctx);
      ALOGE("failed to allocate native handle");
      return NULL;
   }
   native_handle->data[0] = 1;
   native_handle->data[1] = ctx->surface;
   native_window_set_sideband_stream(native_window, native_handle);
   ctx->native_window = native_window;
   ctx->native_handle = native_handle;
   return ctx;
}

/* Only used in HDMI TV input Hal */
struct bcmsideband_ctx *libbcmsideband_init_sideband_tif(
   int index, /* index in [0..HWC_BINDER_SIDEBAND_SURFACE_SIZE[ must be managed by user. */
   native_handle_t **p_native_handle,
   sb_geometry_cb cb,
   void *cb_ctx) {

   struct bcmsideband_ctx *ctx;
   ctx = (struct bcmsideband_ctx *)malloc(sizeof (*ctx));
   if (!ctx) {
      ALOGE("failed to get allocate sideband context");
      return NULL;
   }
   ctx->caller = cb_ctx;

   if (idse() != NULL) {
      gSdbGeomCb = new SdbGeomCb(cb, ctx);
      ctx->index = index;
      ctx->surface = idse()->regSdbCb(index, NULL);
      ctx->surface = idse()->regSdbCb(index, gSdbGeomCb);
   } else {
      ALOGE("failed to get dspsvcext service");
      free(ctx);
      return NULL;
   }

   native_handle_t *native_handle = native_handle_create(0, 2);
   if (!native_handle) {
      gSdbGeomCb = NULL;
      free(ctx);
      ALOGE("failed to allocate native handle");
      return NULL;
   }
   native_handle->data[0] = 1;
   native_handle->data[1] = ctx->surface;
   ctx->native_handle = native_handle;
   *p_native_handle = native_handle;
   return ctx;
}

void libbcmsideband_release(struct bcmsideband_ctx *ctx)
{
    ALOGD("> %s()",__FUNCTION__);
    if (ctx && ctx->native_window)
    {
        native_window_set_sideband_stream(ctx->native_window, NULL);
        native_handle_delete(ctx->native_handle);
        free(ctx);
    }

    gSdbGeomCb = NULL;
    ALOGD("< %s()",__FUNCTION__);
}
