/*
 * Copyright (C) 2010 The Android Open Source Project
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

// Verbose messages removed
//#define LOG_NDEBUG 0

#include <hardware/hardware.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <utils/List.h>

#include <hardware/hwcomposer.h>

#include <EGL/egl.h>
#include "nexus_ipc_client_factory.h"
#include "gralloc_priv.h"

/*****************************************************************************/
#define MAX_NEXUS_PLAYER 3

typedef struct hwc_video_window_context_t {
    NexusClientContext *nexusClient;
    bool visible;
} hwc_video_window_context_t;

struct hwc_context_t {
    hwc_composer_device_1_t device;
    /* our private state goes below here */
    NexusIPCClientBase *pIpcClient;
    NexusClientContext *pNexusClientContext;
    hwc_procs_t const* procs;
    bool vsync_callback_enabled;
    int powerMode;  // Same mode as HWC_POWER_MODE_*
    pthread_t vsync_callback_thread;
    hwc_rect_t last_displayFrame[MAX_NEXUS_PLAYER];
    hwc_video_window_context_t last_video_window_visible[MAX_NEXUS_PLAYER];
    int last_video_window_zorder[MAX_NEXUS_PLAYER];
};

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);

static struct hw_module_methods_t hwc_module_methods = {
    open: hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 0,
        id: HWC_HARDWARE_MODULE_ID,
        name: "Sample hwcomposer module",
        author: "The Android Open Source Project",
        methods: &hwc_module_methods,
    }
};

/*****************************************************************************/

static void dump_layer(hwc_layer_1_t const* l) {
    ALOGD("\ttype=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x, {%d,%d,%d,%d}, {%d,%d,%d,%d}",
            l->compositionType, l->flags, l->handle, l->transform, l->blending,
            l->sourceCrop.left,
            l->sourceCrop.top,
            l->sourceCrop.right,
            l->sourceCrop.bottom,
            l->displayFrame.left,
            l->displayFrame.top,
            l->displayFrame.right,
            l->displayFrame.bottom);
}

static int hwc_prepare(hwc_composer_device_1_t *dev,
        size_t numDisplays, hwc_display_contents_1_t** displays) {

    struct hwc_context_t *ctx = (hwc_context_t*)dev;
    unsigned int iWindowIndex;
    int64_t timestamp;
    NexusIPCClientBase *pIpcClient = ctx->pIpcClient;
    b_video_window_settings window_settings;
    hwc_layer_1_t *layer;
    private_handle_t *bcmBuffer;
    bool video_window_visible[MAX_NEXUS_PLAYER];
    int video_window_zorder = 0;
    unsigned int i;
    NexusClientContext *client_context;
    PSHARED_DATA pSharedData;
    android::List<hwc_layer_1_t *> hwcList;
    android::List<hwc_layer_1_t *>::iterator it;

    if (displays && (displays[0]->flags & HWC_GEOMETRY_CHANGED)) {

        // Cover the case where a layer disappears from the list
        // meaning it should be made invisible.
        for (i = 0; i < MAX_NEXUS_PLAYER; i++) {
            video_window_visible[i] = false;
        }

        for (i = 0; i < displays[0]->numHwLayers; i++) {
            layer = &displays[0]->hwLayers[i];

            if(!layer->handle) {
                ALOGE("%s: Unable to find layer handle!!", __FUNCTION__);
                continue;
            }

            // Cast the handle to get the extra bits from Broadcom/Gralloc
            bcmBuffer = (private_handle_t *)layer->handle;

            if (layer->compositionType == HWC_SIDEBAND)
            {
                ALOGE("%s: Found a HWC_SIDEBAND layer!!", __FUNCTION__);

                // Add this layer to the list
                hwcList.push_back(layer);

                continue;
            }
            else
                layer->compositionType = HWC_FRAMEBUFFER;

            // Get shared data from Gralloc
            pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(bcmBuffer->sharedDataPhyAddr);

            // The window Id plus 1 is used to indicate whether this is a video window surface
            // or not (previously we used the usage field)
            if (android_atomic_acquire_load(&pSharedData->videoWindow.windowIdPlusOne) == 0) {
                continue;
            }
            else {
                /* We use the timestamp that the buffer was registered in Gralloc to determine the z-order for the
                   video windows.  A later time stamp means that the video window was created later and that it
                   should appear at the top of the stack (i.e. have a higher z-order value).  This is how Android
                   works when it composes views.  Views added to the RelativeLayout later are normally displayed in
                   the z-order at which they are added (i.e. later ones appear on top of earlier ones).
                */
                timestamp = pSharedData->videoWindow.timestamp;
                for (it = hwcList.begin(); it != hwcList.end(); it++) {
                    PSHARED_DATA pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(((private_handle_t *)((*it)->handle))->sharedDataPhyAddr);
                    if (pSharedData->videoWindow.timestamp > timestamp) {
                        hwcList.insert(it, layer);
                        break;
                    }
                }

                if (it == hwcList.end()) {
                    hwcList.push_back(layer);
                }
            }
        }

        /* Now run through the list of layers in order of timestamp (i.e. z-order) and set the video window parameters up */
        for (it = hwcList.begin(); it != hwcList.end(); it++, video_window_zorder++) {
            layer = *it;

            // Cast the handle to get the extra bits from Broadcom/Gralloc
            bcmBuffer = (private_handle_t *)layer->handle;

            if (layer->compositionType == HWC_SIDEBAND)
            {
                iWindowIndex = 0;
                video_window_visible[iWindowIndex] = 1;

                client_context = (NexusClientContext*)bcmBuffer->data[1];
            }

            else
            {
                // We'll handle this with a videowindow (HWC_OVERLAY), so we'll need a hole in the FB (HWC_HINT_CLEAR_FB)
                layer->compositionType = HWC_OVERLAY;
                layer->hints = HWC_HINT_CLEAR_FB;

                // Get shared data from Gralloc
                pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(bcmBuffer->sharedDataPhyAddr);

                iWindowIndex   = android_atomic_acquire_load(&pSharedData->videoWindow.windowIdPlusOne) - 1;
                timestamp      = pSharedData->videoWindow.timestamp;
                client_context = reinterpret_cast<NexusClientContext *>(pSharedData->videoWindow.nexusClientContext);
                video_window_visible[iWindowIndex] = pSharedData->videoWindow.windowVisible;

                // Indicate that this is indeed a video window on a layer
                android_atomic_release_store(video_window_zorder+1, &pSharedData->videoWindow.layerIdPlusOne);

                ALOGV("%s: window id=%d, layer=%d, visible=%d, client_context=%p sharedDataPhyAddr=0x%08x timestamp=%lldns", __FUNCTION__,
                        iWindowIndex, video_window_zorder, video_window_visible[iWindowIndex], (void *)client_context, bcmBuffer->sharedDataPhyAddr, timestamp);
            }

            ctx->last_video_window_visible[iWindowIndex].nexusClient = client_context;
			//reset the perviously stored visible flag
            for (i = 0; i < MAX_NEXUS_PLAYER; i++) {
                if (ctx->last_video_window_visible[i].nexusClient ) {
                    pIpcClient->getVideoWindowSettings(ctx->last_video_window_visible[i].nexusClient, i, &window_settings);
                    if(!window_settings.visible && ctx->last_video_window_visible[iWindowIndex].visible) {
                        ctx->last_video_window_visible[iWindowIndex].visible = false;
                    }
                }
            }

            // if the video display rectangle is changed by app, set video window according to parameters
            if ((ctx->last_displayFrame[iWindowIndex].left   != layer->displayFrame.left)   ||
                (ctx->last_displayFrame[iWindowIndex].top    != layer->displayFrame.top)    ||
                (ctx->last_displayFrame[iWindowIndex].right  != layer->displayFrame.right)  ||
                (ctx->last_displayFrame[iWindowIndex].bottom != layer->displayFrame.bottom) ||
                (ctx->last_video_window_zorder[iWindowIndex] != video_window_zorder)        ||
                (ctx->last_video_window_visible[iWindowIndex].visible != video_window_visible[iWindowIndex]))
            {
                int x, y, w, h;
                x = layer->displayFrame.left;
                y = layer->displayFrame.top;
                w = layer->displayFrame.right  - layer->displayFrame.left;
                h = layer->displayFrame.bottom - layer->displayFrame.top;

                ALOGD("%s: VideoWindow parameters [%d]: layer=%d, client_context=%p %dx%d@%d,%d visible=%d",
                        __FUNCTION__, iWindowIndex, video_window_zorder, (void *)client_context, w, h, x, y, video_window_visible[iWindowIndex]);

                if (client_context != NULL) {
                    if (w > 1 && h > 1) {
                        pIpcClient->getVideoWindowSettings(client_context, iWindowIndex, &window_settings);

                        window_settings.position.x = x;
                        window_settings.position.y = y;
                        window_settings.position.width = w;
                        window_settings.position.height = h;
                        window_settings.visible = video_window_visible[iWindowIndex];
                        window_settings.zorder = video_window_zorder;
                        
                        pIpcClient->setVideoWindowSettings(client_context, iWindowIndex, &window_settings);
                    }
                    else {
                        ALOGW("%s: Ignoring layer as size is invalid (%dx%d)!", __FUNCTION__, w, h);
                    }
                }
                ctx->last_video_window_visible[iWindowIndex].visible = video_window_visible[iWindowIndex];

                if ((window_settings.visible == false) && (layer->compositionType != HWC_SIDEBAND)) {
                    ALOGV("%s: About to signal to sharedDataPhyAddr=0x%08x that window %d has been made invisible...", __FUNCTION__,
                            bcmBuffer->sharedDataPhyAddr, iWindowIndex);
                    android_atomic_release_store(0, &pSharedData->videoWindow.windowIdPlusOne);
                }
            }

            ctx->last_displayFrame[iWindowIndex] = layer->displayFrame;
            ctx->last_video_window_zorder[iWindowIndex] = video_window_zorder;
        }

        for (i = 0; i < MAX_NEXUS_PLAYER; i++) {
            // Make windows invisible if they are removed from the layer list.
            if (ctx->last_video_window_visible[i].nexusClient &&
                video_window_visible[i] != ctx->last_video_window_visible[i].visible &&
                video_window_visible[i] == false)
            {
                ALOGD("%s: Remove VideoWindow %d client_context=%p", __FUNCTION__, i, (void *)ctx->last_video_window_visible[i].nexusClient);
                pIpcClient->getVideoWindowSettings(ctx->last_video_window_visible[i].nexusClient, i, &window_settings);
                ctx->last_video_window_visible[i].visible = false;
                window_settings.visible = false;
                pIpcClient->setVideoWindowSettings(ctx->last_video_window_visible[i].nexusClient, i, &window_settings);
                ctx->last_video_window_visible[i].nexusClient = NULL;
            }
        }
        ALOGV("%s: exit", __FUNCTION__);
    }
    return 0;
}

static int hwc_set(hwc_composer_device_1_t *dev,
        size_t numDisplays, hwc_display_contents_1_t** displays)
{
    //for (size_t i=0 ; i<displays[0]->numHwLayers ; i++) {
    //    dump_layer(&displays[0]->hwLayers[i]);
    //}
#if 0
    // TODO: Make this do somthing useful
    for (size_t i=0 ; i<displays[0]->numHwLayers ; i++) {
        if (displays[0]->hwLayers[i].compositionType == HWC_OVERLAY) {
            if ((displays[0]->hwLayers[i].blending == HWC_BLENDING_NONE) &&
                ((displays[0]->hwLayers[i].sourceCrop.right - displays[0]->hwLayers[i].sourceCrop.left) == 1280) &&
                ((displays[0]->hwLayers[i].sourceCrop.bottom - displays[0]->hwLayers[i].sourceCrop.top) == 720)) {
                // base layer

            }
            else if ((displays[0]->hwLayers[i].blending == HWC_BLENDING_PREMULT) &&
                     ((displays[0]->hwLayers[i].sourceCrop.right - displays[0]->hwLayers[i].sourceCrop.left) <= 64) &&
                     ((displays[0]->hwLayers[i].sourceCrop.bottom - displays[0]->hwLayers[i].sourceCrop.top) <= 64)) {
                // cursor layer
            }
        }
    }
#endif
    EGLBoolean sucess = eglSwapBuffers((EGLDisplay)displays[0]->dpy,
            (EGLSurface)displays[0]->sur);
    eglWaitGL();
    if (!sucess) {
        return HWC_EGL_ERROR;
    }
    return 0;
}

static int hwc_device_close(struct hw_device_t *dev)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    if (ctx != NULL) {
        if (ctx->pIpcClient != NULL) {
            if (ctx->pNexusClientContext != NULL) {
                ctx->pIpcClient->destroyClientContext(ctx->pNexusClientContext);
            }
            delete ctx->pIpcClient;
        }
        free(ctx);
    }
    return 0;
}

static int hwc_device_blank(hwc_composer_device_1* dev, int disp, int blank)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;

    // enables and disables screen blanking
    ALOGV("hwc_device_blank: disp=%d, blank=%d", disp, blank);
    ctx->powerMode = (blank) ? HWC_POWER_MODE_OFF : HWC_POWER_MODE_NORMAL;
    return 0;
}

static int hwc_device_eventControl(hwc_composer_device_1* dev, int disp, int event, int enabled)
{
    int status = -EINVAL;

    if (event == HWC_EVENT_VSYNC) {
        struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
        ctx->vsync_callback_enabled = enabled;
        status = 0;
        ALOGV("HWC HWC_EVENT_VSYNC (%s)", enabled ? "enabled":"disabled");
    }
    else {
        ALOGE("HWC unknown event control requested %d:%d", event, enabled);
    }

    return status;
}

static void hwc_registerProcs(struct hwc_composer_device_1* dev, hwc_procs_t const* procs)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    ALOGV("HWC hwc_register_procs (%p)", procs);
    ctx->procs = (hwc_procs_t *)procs;
}



/*****************************************************************************/

static int64_t VsyncSystemTime()
{
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)(t.tv_sec) * 1000000000LL + t.tv_nsec;
}

static int msleep(unsigned long milisec)
{
    struct timespec req = {0,0};
    time_t sec = (int)(milisec / 1000);
    milisec = milisec - (sec * 1000);
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    // if sleep aborts for SIGINT, continue
    while (nanosleep(&req, &req) == -1)
         continue;
    return 1;
}

/* defined in NexusSurface.cpp */
/* defined as weak, to remove linker dependency in the build */
void __attribute__((weak)) hwcomposer_wait_vsync(void);

static void * hwc_vsync_task(void *argv)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)argv;

    ALOGD("HWC Starting VSYNC thread");
    do
    {
        if (hwcomposer_wait_vsync)
            hwcomposer_wait_vsync();
        else {
            ALOGE("HWC unable to locate the hwcomposer_wait_vsync() function %p", getpid());
            msleep(16);
        }

        if (ctx->vsync_callback_enabled && ctx->procs->vsync !=NULL) {
            ALOGV("HWC calling vsync callback");
            ctx->procs->vsync(const_cast<hwc_procs_t *>(ctx->procs), 0, VsyncSystemTime());
        }
    } while(1);
    return NULL;
}

// Returns true if the HWC is ready to blank the screen or enter power
static bool hwc_standby_monitor(void *dev)
{
    bool standby;
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;

    standby = (ctx == NULL) || (ctx->powerMode == HWC_POWER_MODE_OFF);
    ALOGV("%s: standby=%d", __FUNCTION__, standby);
    return standby;
}

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = -EINVAL;
    if (!strcmp(name, HWC_HARDWARE_COMPOSER)) {
        pthread_attr_t attr;
        struct hwc_context_t *dev;
        dev = (hwc_context_t*)malloc(sizeof(*dev));

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = HWC_DEVICE_API_VERSION_1_0;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = hwc_device_close;

        dev->device.prepare = hwc_prepare;
        dev->device.set = hwc_set;
        dev->device.blank = hwc_device_blank;
        dev->device.eventControl = hwc_device_eventControl;
        dev->device.registerProcs = hwc_registerProcs;

        dev->powerMode = HWC_POWER_MODE_NORMAL;

        pthread_attr_init(&attr);
        if (pthread_create(&dev->vsync_callback_thread, &attr, hwc_vsync_task, (void *)dev) != 0) {
            return status;
        }

        dev->pIpcClient = NexusIPCClientFactory::getClient("hwc");
        if (dev->pIpcClient == NULL) {
            ALOGE("%s: Could not instantiate Nexus IPC Client!!!", __FUNCTION__);
        }
        else {
            b_refsw_client_client_configuration clientConfig;

            memset(&clientConfig, 0, sizeof(clientConfig));
            strncpy(clientConfig.name.string, "hwc", sizeof(clientConfig.name.string));
            clientConfig.standbyMonitorCallback = hwc_standby_monitor;
            clientConfig.standbyMonitorContext  = dev;

            dev->pNexusClientContext = dev->pIpcClient->createClientContext(&clientConfig);
            if (dev->pNexusClientContext == NULL) {
                ALOGE("%s: Could not create Nexus Client Context!!!", __FUNCTION__); 
            }
            else {
                *device = &dev->device.common;
                status = 0;
            }
        }
    }
    return status;
}
