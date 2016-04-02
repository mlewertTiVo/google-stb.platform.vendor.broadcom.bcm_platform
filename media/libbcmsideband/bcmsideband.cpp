//#define LOG_NDEBUG 0
#define LOG_TAG "libbcmsideband"

#include <utils/Log.h>
#include <system/window.h>
#include <android/native_window_jni.h>
#include <nexus_ipc_client_factory.h>

#include "bcmsideband.h"
#include "bcmsideband_hwcbinder.h"

using namespace android;

struct bcmsideband_ctx {
    NexusIPCClientBase *ipc_client;
    NexusClientContext *nexus_client;
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

struct bcmsideband_ctx * libbcmsideband_init_sideband(ANativeWindow *native_window, int */*video_id*/, int */*audio_id*/, int */*surface_id*/, sb_geometry_cb cb)
{
    struct bcmsideband_ctx *ctx;

    ctx = (struct bcmsideband_ctx *)malloc(sizeof (*ctx));
    if (!ctx)
        return NULL;

    NexusIPCClientBase *ipc_client = NexusIPCClientFactory::getClient("libbcmsideband");
    if (ipc_client == NULL) {
        free(ctx);
        ALOGE("cannot create NexusIPCClient!");
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
    ctx->bcmSidebandHwcBinder->getsideband(0, ctx->surfaceClientId);

    NexusClientContext *nexus_client = ipc_client->createClientContext();

    if (nexus_client == NULL) {
        delete ipc_client;
        free(ctx);
        ALOGE("createClientContext failed");
        return NULL;
    }

    native_handle_t *native_handle = native_handle_create(0, 2);
    if (!native_handle) {
        ipc_client->destroyClientContext(nexus_client);
        delete ipc_client;
        free(ctx);

        ALOGE("failed to allocate native handle");
        return NULL;
    }

    native_handle->data[0] = 1;
    native_handle->data[1] = (intptr_t)nexus_client;
    native_window_set_sideband_stream(native_window, native_handle);

    ctx->native_window = native_window;
    ctx->native_handle = native_handle;
    ctx->ipc_client = ipc_client;
    ctx->nexus_client = nexus_client;
    ctx->geometry_cb = cb;
    ctx->geometry_cb_ctx = NULL;
    return ctx;
}

struct bcmsideband_ctx * libbcmsideband_init_sideband_tif(native_handle_t **p_native_handle, int */*video_id*/, int */*audio_id*/, int */*surface_id*/, sb_geometry_cb cb, void *cb_ctx)
{
    struct bcmsideband_ctx *ctx;

    ctx = (struct bcmsideband_ctx *)malloc(sizeof (*ctx));
    if (!ctx)
        return NULL;

    NexusIPCClientBase *ipc_client = NexusIPCClientFactory::getClient("libbcmsideband");
    if (ipc_client == NULL) {
        free(ctx);
        ALOGE("cannot create NexusIPCClient!");
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
    ctx->bcmSidebandHwcBinder->getsideband(0, ctx->surfaceClientId);

    NexusClientContext *nexus_client = ipc_client->createClientContext();

    if (nexus_client == NULL) {
        delete ipc_client;
        free(ctx);
        ALOGE("createClientContext failed");
        return NULL;
    }

    native_handle_t *native_handle = native_handle_create(0, 2);
    if (!native_handle) {
        ipc_client->destroyClientContext(nexus_client);
        delete ipc_client;
        free(ctx);

        ALOGE("failed to allocate native handle");
        return NULL;
    }

    native_handle->data[0] = 1;
    native_handle->data[1] = (intptr_t)nexus_client;

    ctx->native_window = NULL;
    ctx->native_handle = native_handle;
    ctx->ipc_client = ipc_client;
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
    ctx->ipc_client->destroyClientContext(ctx->nexus_client);
    delete ctx->ipc_client;
    if (ctx->bcmSidebandHwcBinder)
        delete ctx->bcmSidebandHwcBinder;
    free(ctx);
}
