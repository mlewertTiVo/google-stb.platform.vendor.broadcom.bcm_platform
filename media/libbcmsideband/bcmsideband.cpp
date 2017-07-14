/******************************************************************************
 * (c) 2017 Broadcom
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

#include <utils/Log.h>
#include <system/window.h>
#include <android/native_window_jni.h>
#include <nxwrap.h>

#include "bcmsideband.h"
#include "bcmsideband_hwcbinder.h"

using namespace android;

struct bcmsideband_ctx {
    NxWrap *nx_wrap;
    uint64_t nexus_client;
    ANativeWindow *native_window;
    native_handle_t *native_handle;
    BcmSidebandBinder_wrap *bcmSidebandHwcBinder;
    sb_geometry_cb geometry_cb;
    void *geometry_cb_ctx;
    int surfaceClientId;
};

void BcmSidebandBinder::notify(int msg, struct hwc_notification_info &ntfy)
{
   ALOGV( "%s: notify received: msg=%u", __FUNCTION__, msg);

   if (cb)
      cb(cb_data, msg, ntfy);
}

static void BcmSidebandBinderNotify(void *cb_data, int msg, struct hwc_notification_info &ntfy)
{
    struct bcmsideband_ctx *ctx = (struct bcmsideband_ctx *)cb_data;

    switch (msg)
    {
    case HWC_BINDER_NTFY_SIDEBAND_SURFACE_GEOMETRY_UPDATE:
    {
       NEXUS_Rect position, clip;
       position.x = ntfy.frame.x;
       position.y = ntfy.frame.y;
       position.width = ntfy.frame.w;
       position.height = ntfy.frame.h;
       clip.x = ntfy.clipped.x;
       clip.y = ntfy.clipped.y;
       clip.width = ntfy.clipped.w;
       clip.height = ntfy.clipped.h;
       if (ctx->geometry_cb)
          (*ctx->geometry_cb)(ctx->geometry_cb_ctx, position.x, position.y, position.width, position.height);
       ALOGE("%s: frame:{%d,%d,%d,%d}, clipped:{%d,%d,%d,%d}, display:{%d,%d}, zorder:%d", __FUNCTION__,
             position.x, position.y, position.width, position.height,
             clip.x, clip.y, clip.width, clip.height,
             ntfy.display_width, ntfy.display_height, ntfy.zorder);
    }
    break;

    default:
        break;
    }
}

struct bcmsideband_ctx * libbcmsideband_init_sideband(int index, /* index in [0..HWC_BINDER_SIDEBAND_SURFACE_SIZE[ must be managed by user. */
    ANativeWindow *native_window, int */*video_id*/, int */*audio_id*/, int */*surface_id*/,
    sb_geometry_cb cb)
{
    struct bcmsideband_ctx *ctx;

    ctx = (struct bcmsideband_ctx *)malloc(sizeof (*ctx));
    if (!ctx)
        return NULL;

    NxWrap *pNxWrap = new NxWrap("bcmsideband");
    if (pNxWrap == NULL) {
        free(ctx);
        ALOGE("cannot create NxWrap!");
        return NULL;
    }
    pNxWrap->join();

    // connect to the HWC binder.
    ctx->bcmSidebandHwcBinder = new BcmSidebandBinder_wrap;
    if ( NULL == ctx->bcmSidebandHwcBinder )
    {
        ALOGE("Unable to connect to HwcBinder");
        return NULL;
    }
    ctx->bcmSidebandHwcBinder->get()->register_notify(&BcmSidebandBinderNotify, (void *)ctx);
    ctx->bcmSidebandHwcBinder->getsideband(index, ctx->surfaceClientId);

    uint64_t nexus_client = pNxWrap->client();
    if (!nexus_client) {
        pNxWrap->leave();
        delete pNxWrap;
        free(ctx);
        ALOGE("createClientContext failed");
        return NULL;
    }

    native_handle_t *native_handle = native_handle_create(0, 2);
    if (!native_handle) {
        pNxWrap->leave();
        delete pNxWrap;
        free(ctx);

        ALOGE("failed to allocate native handle");
        return NULL;
    }

    native_handle->data[0] = 2;
    native_handle->data[1] = ctx->surfaceClientId;
    native_window_set_sideband_stream(native_window, native_handle);

    ctx->native_window = native_window;
    ctx->native_handle = native_handle;
    ctx->nx_wrap = pNxWrap;
    ctx->nexus_client = nexus_client;
    ctx->geometry_cb = cb;
    ctx->geometry_cb_ctx = NULL;
    return ctx;
}

struct bcmsideband_ctx * libbcmsideband_init_sideband_tif(int index, /* index in [0..HWC_BINDER_SIDEBAND_SURFACE_SIZE[ must be managed by user. */
    native_handle_t **p_native_handle, int */*video_id*/, int */*audio_id*/, int */*surface_id*/,
    sb_geometry_cb cb, void *cb_ctx)
{
    struct bcmsideband_ctx *ctx;

    ctx = (struct bcmsideband_ctx *)malloc(sizeof (*ctx));
    if (!ctx)
        return NULL;

    NxWrap *pNxWrap = new NxWrap("bcmsideband");
    if (pNxWrap == NULL) {
        free(ctx);
        ALOGE("cannot create NxWrap!");
        return NULL;
    }

    // connect to the HWC binder.
    ctx->bcmSidebandHwcBinder = new BcmSidebandBinder_wrap;
    if ( NULL == ctx->bcmSidebandHwcBinder )
    {
        ALOGE("Unable to connect to HwcBinder");
        return NULL;
    }
    ctx->bcmSidebandHwcBinder->get()->register_notify(&BcmSidebandBinderNotify, (void *)ctx);
    ctx->bcmSidebandHwcBinder->getsideband(index, ctx->surfaceClientId);

    uint64_t nexus_client = pNxWrap->client();
    if (!nexus_client) {
        pNxWrap->leave();
        delete pNxWrap;
        free(ctx);
        ALOGE("createClientContext failed");
        return NULL;
    }

    native_handle_t *native_handle = native_handle_create(0, 2);
    if (!native_handle) {
        pNxWrap->leave();
        delete pNxWrap;
        free(ctx);

        ALOGE("failed to allocate native handle");
        return NULL;
    }

    native_handle->data[0] = 2;
    native_handle->data[1] = ctx->surfaceClientId;

    ctx->native_window = NULL;
    ctx->native_handle = native_handle;
    ctx->nx_wrap = pNxWrap;
    ctx->nexus_client = nexus_client;
    ctx->geometry_cb = cb;
    ctx->geometry_cb_ctx = cb_ctx;
    *p_native_handle = native_handle;
    return ctx;
}

void libbcmsideband_release(struct bcmsideband_ctx *ctx)
{
    if (ctx->native_window)
        native_window_set_sideband_stream(ctx->native_window, NULL);
    native_handle_delete(ctx->native_handle);
    ctx->nx_wrap->leave();
    delete ctx->nx_wrap;
    if (ctx->bcmSidebandHwcBinder)
        delete ctx->bcmSidebandHwcBinder;
    free(ctx);
}
