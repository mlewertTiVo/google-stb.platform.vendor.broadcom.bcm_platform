/*
 * Copyright (C) 2014 Broadcom Canada Ltd.
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

#include <hardware/hardware.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <utils/List.h>
#include <utils/Timers.h>
#include <cutils/properties.h>

#include <hardware/hwcomposer.h>

#include <EGL/egl.h>
#include "nexus_ipc_client_factory.h"
#include "gralloc_priv.h"

#include "nexus_base_mmap.h"
#include "nexus_surface_client.h"
#include "nexus_surface_cursor.h"
#include "nxclient.h"

#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include "HwcCommon.h"
#include "Hwc.h"
#include "HwcListener.h"
#include "IHwc.h"
#include "HwcSvc.h"

#include "hwcconv.h"
#include "hwcutils.h"

#include "sync/sync.h"
// deliberate use of sw_sync as we do not have other sync
// primitive at this point.  just be careful when using those.
#include "sw_sync.h"

using namespace android;

#define INVALID_HANDLE               0xBAADCAFE
#define INVALID_FENCE                -1

// cursor surface is behaving slightly differently than
// other gpx (or mm) ones and may lead to a lot of false
// alarm logs.
#define HWC_CURSOR_SURFACE_VERBOSE   0
#define HWC_CURSOR_SURFACE_SUPPORTED 0

#define HWC_DISPLAY_FENCE_VERBOSE    0

#define HWC_SURFACE_LIFE_CYCLE_ERROR 0
#define HWC_DUMP_LAYER_ON_ERROR      0

#define HWC_DUMP_LAYER_CLASS         0

#define HWC_SB_NO_ALLOC_SURF_CLI     1
#define HWC_MM_NO_ALLOC_SURF_CLI     1

#define NSC_GPX_CLIENTS_NUMBER       11 /* graphics client layers; typically no
                                         * more than 3 are needed at any time, though
                                         * it has been seen up to 7 in some scenario,
                                         * so maximum is 10+1 (1 for the floating
                                         * HWC_FRAMEBUFFER_TARGET). */
#define NSC_MM_CLIENTS_NUMBER        3  /* multimedia client layers; typically no
                                         * more than 2 are needed at any time. */
#define NSC_SB_CLIENTS_NUMBER        2  /* sideband client layers; typically no
                                         * more than 1 are needed at any time. */

#define VSYNC_USES_NSC_SURF          0

#define NSC_CLIENTS_NUMBER           (NSC_GPX_CLIENTS_NUMBER+NSC_MM_CLIENTS_NUMBER+NSC_SB_CLIENTS_NUMBER+VSYNC_USES_NSC_SURF)

#define VSYNC_CLIENT_LAYER_ID        0 /* first layer. */
#define VSYNC_CLIENT_WIDTH           16
#define VSYNC_CLIENT_HEIGHT          16
#define VSYNC_CLIENT_STRIDE          VSYNC_CLIENT_WIDTH*4
#define VSYNC_CLIENT_REFRESH         16666667
#define DISPLAY_CONFIG_DEFAULT       0
#define SURFACE_ALIGNMENT            16

#define VSYNC_CLIENT_ZORDER          0
#define MM_CLIENT_ZORDER             (VSYNC_CLIENT_ZORDER+1)
#define SB_CLIENT_ZORDER             (VSYNC_CLIENT_ZORDER+1)
#define GPX_CLIENT_ZORDER            (MM_CLIENT_ZORDER+1)
#define CURSOR_CLIENT_ZORDER         (GPX_CLIENT_ZORDER+1)

#define GPX_SURFACE_STACK            3
#define DUMP_BUFFER_SIZE             1024
#define LAST_PING_FRAME_ID_INVALID   0xBAADCAFE
#define HWC_DO_YV12_CONV             0

/* note: matching other parts of the integration, we
 *       want to default product resolution to 1080p.
 */
#define DISPLAY_DEFAULT_HD_RES       "1080p"
#define DISPLAY_HD_RES_PROP          "ro.hd_output_format"

/* gles mode: 'always' is to always force gles composition over
 *            nsc one.
 *            'fallback' is to allow gles fallback when special layers
 *            stack cannot be handled, such as when a dim-layer is there.
 *            'never' (or any other value for that matter) just disables
 *            any gles path.  visual artifact will be seen.
 */
#define HWC_GLES_MODE_FALLBACK       "fallback"
#define HWC_GLES_MODE_ALWAYS         "always"
#define HWC_GLES_MODE_NEVER          "never"
#define HWC_DEFAULT_GLES_MODE        HWC_GLES_MODE_FALLBACK
#define HWC_GLES_MODE_PROP           "ro.hwc.gles.mode"

/* sw-sync: 1 to enable, 0 otherwise.  note: if using 'always' gles mode,
 *          ensure the sw-sync are DISABLED (0).
 */
#define HWC_DEFAULT_SW_SYNC          "1"
#define HWC_SW_SYNC_PROP             "ro.hwc.sw_sync"

#define HWC_CHECKPOINT_TIMEOUT       (100)

enum {
    NEXUS_SURFACE_COMPOSITOR = 0,
    NEXUS_CURSOR,
    NEXUS_VIDEO_WINDOW,
    NEXUS_VIDEO_SIDEBAND,
    NEXUS_SYNC,
};

enum {
    NEXUS_CLIENT_GPX = 0,
    NEXUS_CLIENT_MM,
    NEXUS_CLIENT_SB,
    NEXUS_CLIENT_VSYNC,
};

typedef enum {
    SURF_OWNER_NO_OWNER = 0,
    SURF_OWNER_HWC,
    SURF_OWNER_HWC_PUSH,
    SURF_OWNER_NSC,

} GPX_CLIENT_SURFACE_OWNER;

typedef struct {
    NEXUS_SurfaceClientHandle schdl;
    NEXUS_SurfaceCompositorClientId sccid;
    int type;
    void *parent;

} COMMON_CLIENT_INFO;

typedef struct {
    GPX_CLIENT_SURFACE_OWNER owner;
    NEXUS_SurfaceHandle shdl;
    NEXUS_SurfaceCursorHandle schdl;
    buffer_handle_t grhdl;
    unsigned int use_order;
    int comp_ix;

} GPX_CLIENT_SURFACE_INFO;

typedef struct {
    COMMON_CLIENT_INFO ncci;
    NEXUS_SurfaceComposition composition;
    GPX_CLIENT_SURFACE_INFO slist[GPX_SURFACE_STACK];
    int width;
    int height;
    unsigned int blending_type;
    int layer_type;
    int layer_subtype;
    int layer_flags;
    int layer_id;
    bool skip_set;
    unsigned int use_order;
    int plane_alpha;

} GPX_CLIENT_INFO;

typedef struct {
    GPX_CLIENT_INFO root;
    NEXUS_SurfaceClientHandle svchdl;
    NEXUS_SurfaceClientSettings settings;
    long long unsigned last_ping_frame_id;

} MM_CLIENT_INFO;

typedef struct {
    GPX_CLIENT_INFO root;
    NEXUS_SurfaceClientSettings settings;

} SB_CLIENT_INFO;

typedef struct {
    COMMON_CLIENT_INFO ncci;
    NEXUS_SurfaceHandle shdl;
    BKNI_EventHandle vsync_event;
    int layer_type;
    int layer_subtype;
    nsecs_t refresh;

} VSYNC_CLIENT_INFO;

typedef void (* HWC_BINDER_NTFY_CB)(int, int, struct hwc_notification_info &);

class HwcBinder : public HwcListener
{
public:

    HwcBinder() : cb(NULL), cb_data(0) {};
    virtual ~HwcBinder() {};

    virtual void notify(int msg, struct hwc_notification_info &ntfy);

    inline void listen() {
       if (get_hwc(false) != NULL)
           get_hwc(false)->registerListener(this, HWC_BINDER_HWC);
       else
           ALOGE("%s: failed to associate %p with HwcBinder service.", __FUNCTION__, this);
    };

    inline void hangup() {
       if (get_hwc(false) != NULL)
           get_hwc(false)->unregisterListener(this);
       else
           ALOGE("%s: failed to dissociate %p from HwcBinder service.", __FUNCTION__, this);
    };

    inline void setvideo(int index, int value, int display_w, int display_h) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->setVideoSurfaceId(this, index, value, display_w, display_h);
       }
    };

    inline void setgeometry(int type, int index,
                            struct hwc_position &frame, struct hwc_position &clipped,
                            int zorder, int visible) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->setGeometry(this, type, index, frame, clipped, zorder, visible);
       }
    }

    inline void setsideband(int index, int value, int display_w, int display_h) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->setSidebandSurfaceId(this, index, value, display_w, display_h);
       }
    };

    inline void setframe(int surface, int frame) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->setDisplayFrameId(this, surface, frame);
       }
    };

    void register_notify(HWC_BINDER_NTFY_CB callback, int data) {
       cb = callback;
       cb_data = data;
    }

private:
    HWC_BINDER_NTFY_CB cb;
    int cb_data;
};

class HwcBinder_wrap
{
private:

   sp<HwcBinder> ihwc;
   bool iconnected;

public:
   HwcBinder_wrap(void) {
      ALOGD("%s: allocated %p", __FUNCTION__, this);
      ihwc = new HwcBinder;
      iconnected = false;
   };

   virtual ~HwcBinder_wrap(void) {
      ALOGD("%s: cleared %p", __FUNCTION__, this);
      ihwc.get()->hangup();
      ihwc.clear();
   };

   void connected(bool status) {
      iconnected = status;
   }

   void connect(void) {
      if (!iconnected) {
         ihwc.get()->listen();
      }
   }

   void setvideo(int index, int value, int display_w, int display_h) {
      if (iconnected) {
         ihwc.get()->setvideo(index, value, display_w, display_h);
      }
   }

   void setgeometry(int type, int index,
                    struct hwc_position &frame, struct hwc_position &clipped,
                    int zorder, int visible) {
      if (iconnected) {
         ihwc.get()->setgeometry(type, index, frame, clipped, zorder, visible);
      }
   }

   void setsideband(int index, int value, int display_w, int display_h) {
      if (iconnected) {
         ihwc.get()->setsideband(index, value, display_w, display_h);
      }
   }

   void setframe(int surface, int frame) {
      if (iconnected) {
         ihwc.get()->setframe(surface, frame);
      }
   }

   HwcBinder *get(void) {
      return ihwc.get();
   }
};

void HwcBinder::notify(int msg, struct hwc_notification_info &ntfy)
{
   ALOGD( "%s: notify received: msg=%u", __FUNCTION__, msg);

   if (cb)
      cb(cb_data, msg, ntfy);
}

struct hwc_context_t {
    hwc_composer_device_1_t device;

    unsigned int prepare_call;
    unsigned int prepare_skipped;
    unsigned int set_call;
    unsigned int set_skipped;

    /* our private state goes below here */
    NexusIPCClientBase *pIpcClient;
    NexusClientContext *pNexusClientContext;
    NxClient_AllocResults nxAllocResults;
    BKNI_MutexHandle dev_mutex;

    hwc_procs_t const* procs;

    bool vsync_callback_enabled;
    BKNI_MutexHandle vsync_callback_enabled_mutex;
    pthread_t vsync_callback_thread;
    int vsync_thread_run;

    BKNI_EventHandle prepare_event;

    GPX_CLIENT_INFO gpx_cli[NSC_GPX_CLIENTS_NUMBER];
    MM_CLIENT_INFO mm_cli[NSC_MM_CLIENTS_NUMBER];
    SB_CLIENT_INFO sb_cli[NSC_SB_CLIENTS_NUMBER];
    BKNI_MutexHandle mutex;
    VSYNC_CLIENT_INFO syn_cli;
    bool nsc_video_changed;
    bool nsc_sideband_changed;

    int display_width;
    int display_height;

    bool display_gles_fallback;
    bool display_gles_always;
    bool needs_fb_target;

    int cursor_layer_id;
    GPX_CLIENT_SURFACE_INFO cursor_cache;

    BKNI_MutexHandle power_mutex;
    int power_mode;

    HwcBinder_wrap *hwc_binder;

    NEXUS_Graphics2DHandle hwc_2dg;
    NEXUS_Graphics2DCapabilities gfxCaps;
    BKNI_EventHandle checkpoint_event;

    int sync_timeline;
};

static void hwc_device_cleanup(hwc_context_t* ctx);
static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);

static struct hw_module_methods_t hwc_module_methods = {
    open: hwc_device_open
};

static void hwc_nsc_recycled_cb(void *context, int param);
static void hwc_sync_recycled_cb(void *context, int param);

static void hw_vsync_cb(void *context, int param);

static void hwc_hide_unused_gpx_layer(hwc_context_t* dev, int index);
static void hwc_hide_unused_mm_layers(hwc_context_t* dev);
static void hwc_hide_unused_sb_layers(hwc_context_t* dev);

static void hwc_nsc_prepare_layer(hwc_context_t* dev, hwc_layer_1_t *layer,
   int layer_id, bool geometry_changed,
   unsigned int *video_layer_id, unsigned int *sideband_layer_id);

static void hwc_binder_advertise_video_surface(hwc_context_t* dev);

static void hwc_checkpoint_callback(void *pParam, int param2);
static int hwc_checkpoint(hwc_context_t *dev);

hwc_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 0,
        id: HWC_HARDWARE_MODULE_ID,
        name: "HWC module BCM STB platforms",
        author: "Broadcom Canada Ltd.",
        methods: &hwc_module_methods,
        dso: 0,
        reserved: {0}
    }
};

/*****************************************************************************/

static const char *nsc_cli_type[] =
{
   "GPX", // NEXUS_CLIENT_GPX
   "MUL", // NEXUS_CLIENT_MM
   "SDB", // NEXUS_CLIENT_SB
   "SNC", // NEXUS_CLIENT_VSYNC
};

static const char *hwc_power_mode[] =
{
   "OFF",  // HWC_POWER_MODE_OFF
   "DOZE", // HWC_POWER_MODE_DOZE
   "ON",   // HWC_POWER_MODE_NORMAL
   "DZS",  // HWC_POWER_MODE_DOZE_SUSPEND
};

// this matches the overlay specified by HWC API.
static const char *hwc_layer_type[] =
{
   "FB", // HWC_FRAMEBUFFER
   "OV", // HWC_OVERLAY
   "BG", // HWC_BACKGROUND
   "FT", // HWC_FRAMEBUFFER_TARGET
   "SB", // HWC_SIDEBAND
   "CO", // HWC_CURSOR_OVERLAY
};

// this matches the corresponding nexus integration.
static const char *nsc_layer_type[] =
{
   "SC", // NEXUS_SURFACE_COMPOSITOR
   "CU", // NEXUS_CURSOR
   "VW", // NEXUS_VIDEO_WINDOW
   "VS", // NEXUS_VIDEO_SIDEBAND
   "SY", // NEXUS_SYNC
};

static const char *nsc_surface_owner[] =
{
   "FRE", // SURF_OWNER_NO_OWNER
   "HWC", // SURF_OWNER_HWC
   "PUS", // SURF_OWNER_HWC_PUSH
   "NSC", // SURF_OWNER_NSC
};

static void hwc_unlock_surface(private_handle_t *pPrivateHandle)
{
  SHARED_DATA *pSharedData;

  if ( NULL == pPrivateHandle )
  {
    ALOGE("%s: Invalid Gralloc Buffer", __FUNCTION__);
    return;
  }
  pSharedData = (SHARED_DATA *)NEXUS_OffsetToCachedAddr(pPrivateHandle->sharedData);
  if ( NULL == pSharedData )
  {
    ALOGE("%s: Invalid Gralloc Buffer Memory", __FUNCTION__);
    return;
  }
  if ( 0 == android_atomic_acquire_load(&pSharedData->hwc.active) )
  {
    ALOGE("Unlocking surface %#x that is not marked active!  Layer says %d surface %#x", pPrivateHandle->sharedData, pSharedData->hwc.layer, pSharedData->hwc.surface);
  }
  else
  {
    pSharedData->hwc.layer = -1;
    pSharedData->hwc.surface = NULL;
    android_atomic_acquire_store(0, &pSharedData->hwc.active);
  }
}

static void hwc_lock_surface(private_handle_t *pPrivateHandle, int layer, NEXUS_SurfaceHandle surface)
{
  SHARED_DATA *pSharedData;

  if ( NULL == pPrivateHandle )
  {
    ALOGE("%s: Invalid Gralloc Buffer", __FUNCTION__);
    return;
  }
  pSharedData = (SHARED_DATA *)NEXUS_OffsetToCachedAddr(pPrivateHandle->sharedData);
  if ( NULL == pSharedData )
  {
    ALOGE("%s: Invalid Gralloc Buffer Memory", __FUNCTION__);
    return;
  }
  if ( android_atomic_acquire_load(&pSharedData->hwc.active) )
  {
    ALOGE("Locking surface %#x that is marked active! Marked Layer %d Surface %#x New Layer %d Surface %#x", pPrivateHandle->sharedData, pSharedData->hwc.layer, pSharedData->hwc.surface, layer, surface);
  }
  else
  {
    pSharedData->hwc.layer = layer;
    pSharedData->hwc.surface = surface;
    android_atomic_acquire_store(1, &pSharedData->hwc.active);
  }
}

static NEXUS_PixelFormat gralloc_to_nexus_pixel_format(int format)
{
   switch (format) {
      case HAL_PIXEL_FORMAT_RGBA_8888: return NEXUS_PixelFormat_eA8_B8_G8_R8;
      case HAL_PIXEL_FORMAT_RGBX_8888: return NEXUS_PixelFormat_eX8_B8_G8_R8;
      case HAL_PIXEL_FORMAT_RGB_888:   return NEXUS_PixelFormat_eX8_B8_G8_R8;
      case HAL_PIXEL_FORMAT_RGB_565:   return NEXUS_PixelFormat_eR5_G6_B5;
      default:                         break;
   }

   ALOGE("%s: unsupported format %d", __FUNCTION__, format);
   return NEXUS_PixelFormat_eUnknown;
}

static int dump_gpx_layer_data(char *start, int capacity, int index, GPX_CLIENT_INFO *client)
{
    int write = -1;
    int total_write = 0;
    int local_capacity = capacity;
    int offset = 0;

    write = snprintf(start, local_capacity,
        "\t[%s]:[%s]:[%d:%d]:[%s]:[%s]::f:%x::z:%d::sz:{%d,%d}::cp:{%d,%d,%d,%d}::cl:{%d,%d,%d,%d}::cv:{%d,%d}::b:%d::pa:%d::sfc:%p::scc:%d::order:%x\n",
        client->composition.visible ? "LIVE" : "HIDE",
        nsc_cli_type[client->ncci.type],
        client->layer_id,
        index,
        hwc_layer_type[client->layer_type],
        nsc_layer_type[client->layer_subtype],
        client->layer_flags,
        client->composition.zorder,
        client->width,
        client->height,
        client->composition.position.x,
        client->composition.position.y,
        client->composition.position.width,
        client->composition.position.height,
        client->composition.clipRect.x,
        client->composition.clipRect.y,
        client->composition.clipRect.width,
        client->composition.clipRect.height,
        client->composition.virtualDisplay.width,
        client->composition.virtualDisplay.height,
        client->blending_type,
        client->plane_alpha,
        client->ncci.schdl,
        client->ncci.sccid,
        client->use_order);

    if (write > 0) {
        local_capacity = (local_capacity > write) ? (local_capacity - write) : 0;
        offset += write;
        total_write += write;
    }

    for (int j = 0; j < GPX_SURFACE_STACK; j++) {
        write = snprintf(start + offset, local_capacity,
            "\t\t[%d:%d]:[%d]:[%s]::shdl:%p::schdl:%p::grhdl:%p::order:%x::comp:%d\n",
            client->layer_id,
            index,
            j,
            nsc_surface_owner[client->slist[j].owner],
            client->slist[j].shdl,
            client->slist[j].schdl,
            client->slist[j].grhdl,
            client->slist[j].use_order,
            client->slist[j].comp_ix);

        if (write > 0) {
            local_capacity = (local_capacity > write) ? (local_capacity - write) : 0;
            offset += write;
            total_write += write;
        }

        if (client->slist[j].grhdl) {
           private_handle_t *bcmBuffer = (private_handle_t *)client->slist[j].grhdl;
           PSHARED_DATA pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(bcmBuffer->sharedData);

           write = snprintf(start + offset, local_capacity,
               "\t\t\t[%p]::flg:0x%x::pid:%d::ogl:{s:%d,f:%d,sz:%d}::use:0x%x\n",
               client->slist[j].grhdl,
               bcmBuffer->flags,
               bcmBuffer->pid,
               bcmBuffer->oglStride,
               bcmBuffer->oglFormat,
               bcmBuffer->oglSize,
               bcmBuffer->usage);

           if (write > 0) {
               local_capacity = (local_capacity > write) ? (local_capacity - write) : 0;
               offset += write;
               total_write += write;
           }

           write = snprintf(start + offset, local_capacity,
               "\t\t\t[%p]::default:{f:0x%x,bpp:%d,{%d,%d},0x%x,sz:%d}\n",
               client->slist[j].grhdl,
               pSharedData->planes[DEFAULT_PLANE].format,
               pSharedData->planes[DEFAULT_PLANE].bpp,
               pSharedData->planes[DEFAULT_PLANE].width,
               pSharedData->planes[DEFAULT_PLANE].height,
               pSharedData->planes[DEFAULT_PLANE].physAddr,
               pSharedData->planes[DEFAULT_PLANE].size);

           if (write > 0) {
               local_capacity = (local_capacity > write) ? (local_capacity - write) : 0;
               offset += write;
               total_write += write;
           }

           write = snprintf(start + offset, local_capacity,
               "\t\t\t[%p]::extra:{f:0x%x,bpp:%d,{%d,%d},0x%x,sz:%d}\n",
               client->slist[j].grhdl,
               pSharedData->planes[EXTRA_PLANE].format,
               pSharedData->planes[EXTRA_PLANE].bpp,
               pSharedData->planes[EXTRA_PLANE].width,
               pSharedData->planes[EXTRA_PLANE].height,
               pSharedData->planes[EXTRA_PLANE].physAddr,
               pSharedData->planes[EXTRA_PLANE].size);

           if (write > 0) {
               local_capacity = (local_capacity > write) ? (local_capacity - write) : 0;
               offset += write;
               total_write += write;
           }
        }
    }

    return total_write;
}

static int dump_mm_layer_data(char *start, int capacity, int index, MM_CLIENT_INFO *client)
{
    int write = -1;

    write = snprintf(start, capacity,
        "\t[%s]:[%s]:[%d:%d]:[%s]:[%s]::z:%d::pcp:{%d,%d,%d,%d}::pcv:{%d,%d}::cm:%d::cp:{%d,%d,%d,%d}::cl:{%d,%d,%d,%d}::cv:{%d,%d}::svc:%p::sfc:%p::scc:%x\n",
        client->root.composition.visible ? "LIVE" : "HIDE",
        nsc_cli_type[client->root.ncci.type],
        client->root.layer_id,
        index,
        hwc_layer_type[client->root.layer_type],
        nsc_layer_type[client->root.layer_subtype],
        client->root.composition.zorder,
        client->root.composition.position.x,
        client->root.composition.position.y,
        client->root.composition.position.width,
        client->root.composition.position.height,
        client->root.composition.virtualDisplay.width,
        client->root.composition.virtualDisplay.height,
        client->settings.composition.contentMode,
        client->settings.composition.position.x,
        client->settings.composition.position.y,
        client->settings.composition.position.width,
        client->settings.composition.position.height,
        client->settings.composition.clipRect.x,
        client->settings.composition.clipRect.y,
        client->settings.composition.clipRect.width,
        client->settings.composition.clipRect.height,
        client->settings.composition.virtualDisplay.width,
        client->settings.composition.virtualDisplay.height,
        client->svchdl,
        client->root.ncci.schdl,
        client->root.ncci.sccid);

    return write;
}

static int dump_sb_layer_data(char *start, int capacity, int index, SB_CLIENT_INFO *client)
{
    int write = -1;

    write = snprintf(start, capacity,
        "\t[%s]:[%s]:[%d:%d]:[%s]:[%s]::z:%d::pcp:{%d,%d,%d,%d}::pcv:{%d,%d}::cm:%d::cp:{%d,%d,%d,%d}::cl:{%d,%d,%d,%d}::cv:{%d,%d}::scc:%d\n",
        client->root.composition.visible ? "LIVE" : "HIDE",
        nsc_cli_type[client->root.ncci.type],
        client->root.layer_id,
        index,
        hwc_layer_type[client->root.layer_type],
        nsc_layer_type[client->root.layer_subtype],
        client->root.composition.zorder,
        client->root.composition.position.x,
        client->root.composition.position.y,
        client->root.composition.position.width,
        client->root.composition.position.height,
        client->root.composition.virtualDisplay.width,
        client->root.composition.virtualDisplay.height,
        client->settings.composition.contentMode,
        client->settings.composition.position.x,
        client->settings.composition.position.y,
        client->settings.composition.position.width,
        client->settings.composition.position.height,
        client->settings.composition.clipRect.x,
        client->settings.composition.clipRect.y,
        client->settings.composition.clipRect.width,
        client->settings.composition.clipRect.height,
        client->settings.composition.virtualDisplay.width,
        client->settings.composition.virtualDisplay.height,
        client->root.ncci.sccid);

    return write;
}

static unsigned int hwc_inc_or_wrap_use_order(GPX_CLIENT_INFO *client)
{
    if ((client->use_order + 1) > INVALID_HANDLE) {
       client->use_order = 0;
    } else {
       ++client->use_order;
    }

    return client->use_order;
}

static void dump_layer_on_error(GPX_CLIENT_INFO *client)
{
    if (HWC_DUMP_LAYER_ON_ERROR) {
       char dump_buffer[DUMP_BUFFER_SIZE];
       dump_gpx_layer_data(dump_buffer, DUMP_BUFFER_SIZE, client->layer_id, client);
       ALOGE("%s", dump_buffer);
    }
}

static int hwc_gpx_get_next_surface_locked(GPX_CLIENT_INFO *client)
{
    for (int i = 0; i < GPX_SURFACE_STACK; i++) {
       if (client->slist[i].owner == SURF_OWNER_NO_OWNER) {
          client->slist[i].owner = SURF_OWNER_HWC;
          client->slist[i].use_order = hwc_inc_or_wrap_use_order(client);
          return i;
       }
    }

    if ((client->layer_subtype != NEXUS_CURSOR) || HWC_CURSOR_SURFACE_VERBOSE) {
       if (HWC_SURFACE_LIFE_CYCLE_ERROR) ALOGE("%s: failed client %d", __FUNCTION__, client->layer_id);
       dump_layer_on_error(client);
    }

    return -1;
}

static int hwc_gpx_push_surface_locked(GPX_CLIENT_INFO *client)
{
    for (int i = 0; i < GPX_SURFACE_STACK; i++) {
       if (client->slist[i].owner == SURF_OWNER_HWC_PUSH) {
          client->slist[i].owner = SURF_OWNER_NSC;
          return i;
       }
    }

    if ((client->layer_subtype != NEXUS_CURSOR) || HWC_CURSOR_SURFACE_VERBOSE) {
       if (HWC_SURFACE_LIFE_CYCLE_ERROR) ALOGE("%s: failed client %d", __FUNCTION__, client->layer_id);
       dump_layer_on_error(client);
    }

    return -1;
}

static int hwc_gpx_get_recycle_surface_locked(GPX_CLIENT_INFO *client, NEXUS_SurfaceHandle shdl)
{
    for (int i = 0; i < GPX_SURFACE_STACK; i++) {
       if ((client->slist[i].owner == SURF_OWNER_NSC) &&
           (client->slist[i].shdl == shdl)) {
          return i;
       }
    }

    if ((client->layer_subtype != NEXUS_CURSOR) || HWC_CURSOR_SURFACE_VERBOSE) {
       if (HWC_SURFACE_LIFE_CYCLE_ERROR) ALOGE("%s: failed client %d", __FUNCTION__, client->layer_id);
       dump_layer_on_error(client);
    }

    return -1;
}

static int hwc_gpx_get_current_surface_locked(GPX_CLIENT_INFO *client)
{
    int selected = -1;
    int selected_use_order = -1;

    for (int i = 0; i < GPX_SURFACE_STACK; i++) {
       if ((client->slist[i].owner == SURF_OWNER_NSC) ||
           (client->slist[i].owner == SURF_OWNER_HWC_PUSH)) {
          if ((selected != -1) &&
              (client->slist[i].use_order > client->slist[selected].use_order)) {
             selected = i;
          } else if (selected == -1) {
             selected = i;
          }
       }
    }

    return selected;
}


static NEXUS_SurfaceCursorHandle hwc_gpx_update_cursor_surface_locked(struct hwc_composer_device_1* dev)
{
    struct hwc_context_t *ctx = (hwc_context_t*)dev;

    for (int i = 0; i < NSC_GPX_CLIENTS_NUMBER; i++) {
       if (ctx->gpx_cli[i].layer_subtype == NEXUS_CURSOR) {
          for (int j = 0; j < GPX_SURFACE_STACK; j++) {
             // valid handle and surface has been returned.
             if (ctx->gpx_cli[i].slist[j].schdl &&
                 (ctx->gpx_cli[i].slist[j].owner == SURF_OWNER_HWC)) {
                ctx->gpx_cli[i].slist[j].owner = SURF_OWNER_HWC_PUSH;
                return ctx->gpx_cli[i].slist[j].schdl;
             }
          }
       }
    }

    return NULL;
}

static void hwc_device_dump(struct hwc_composer_device_1* dev, char *buff, int buff_len)
{
    /* surface flinger gives us a single page equivalent (4096 bytes). */

    NEXUS_SurfaceComposition composition;
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    int capacity, write, index;
    int i;

    if (buff == NULL || !buff_len)
        return;

    memset(buff, 0, buff_len);

    capacity = buff_len;
    index = 0;

    if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
        goto out;
    }

    // global information on the context go here.
    if (capacity) {
       write = snprintf(buff + index, capacity, "\nINFO:\n");
       if (write > 0) {
          capacity = (capacity > write) ? (capacity - write) : 0;
          index += write;
       }
    }
    if (capacity) {
       write = snprintf(buff + index, capacity, "\tprepare:{%u,%u}::set:{%u,%u}::gles-fbk:%s::gles-alw:%s::sync:%d\n",
           ctx->prepare_call,
           ctx->prepare_skipped,
           ctx->set_call,
           ctx->set_skipped,
           ctx->display_gles_fallback ? "enabled" : "disabled",
           ctx->display_gles_always ? "enabled" : "disabled",
           ctx->sync_timeline);
       if (write > 0) {
           capacity = (capacity > write) ? (capacity - write) : 0;
           index += write;
       }
       write = snprintf(buff + index, capacity, "\tipc:%p::ncc:%p::vscb:%s::d:{%d,%d}::pm:%s\n",
           ctx->pIpcClient,
           ctx->pNexusClientContext,
           ctx->vsync_callback_enabled ? "enabled" : "disabled",
           ctx->display_width,
           ctx->display_height,
           hwc_power_mode[ctx->power_mode]);
       if (write > 0) {
           capacity = (capacity > write) ? (capacity - write) : 0;
           index += write;
       }
    }

    // per layer information go here.
    if (capacity) {
       write = snprintf(buff + index, capacity, "\nNEXUS SURFACE COMPOSITOR LAYER STACK:\n");
       if (write > 0) {
          capacity = (capacity > write) ? (capacity - write) : 0;
          index += write;
       }
    }

    for (int i = 0; i < NSC_GPX_CLIENTS_NUMBER; i++)
    {
        write = 0;
        NxClient_GetSurfaceClientComposition(ctx->gpx_cli[i].ncci.sccid, &composition);

        if (composition.visible && capacity) {
           write = dump_gpx_layer_data(buff + index, capacity, i, &ctx->gpx_cli[i]);
           if (write > 0) {
              capacity = (capacity > write) ? (capacity - write) : 0;
              index += write;
           }
        }

        if (!capacity) {
           break;
        }
    }

    for (int i = 0; i < NSC_MM_CLIENTS_NUMBER; i++)
    {
        write = 0;
        if (ctx->mm_cli[i].root.ncci.sccid < INVALID_HANDLE) {
           NxClient_GetSurfaceClientComposition(ctx->mm_cli[i].root.ncci.sccid, &composition);
        } else {
           memcpy(&composition, &ctx->mm_cli[i].root.composition, sizeof(composition));
        }
        if (composition.visible && capacity) {
           write = dump_mm_layer_data(buff + index, capacity, i, &ctx->mm_cli[i]);
           if (write > 0) {
              capacity = (capacity > write) ? (capacity - write) : 0;
              index += write;
           }
        }

        if (!capacity) {
           break;
        }
    }

    for (int i = 0; i < NSC_SB_CLIENTS_NUMBER; i++)
    {
        write = 0;
        if (ctx->sb_cli[i].root.ncci.sccid < INVALID_HANDLE) {
           NxClient_GetSurfaceClientComposition(ctx->sb_cli[i].root.ncci.sccid, &composition);
        } else {
           memcpy(&composition, &ctx->sb_cli[i].root.composition, sizeof(composition));
        }
        if (composition.visible && capacity) {
           write = dump_sb_layer_data(buff + index, capacity, i, &ctx->sb_cli[i]);
           if (write > 0) {
              capacity = (capacity > write) ? (capacity - write) : 0;
              index += write;
           }
        }

        if (!capacity) {
           break;
        }
    }

    // extra layer for vsync.
    if (VSYNC_USES_NSC_SURF && capacity) {
       /** layer-id:type:subtype:surf-handle:surf:surf-client **/
       write = snprintf(buff + index, capacity, "\t[%s]:[%d:0]:[%s]:[%s]::sfc:%p::sf:%p::scc:%d\n",
           nsc_cli_type[ctx->syn_cli.ncci.type],
           VSYNC_CLIENT_LAYER_ID,
           hwc_layer_type[ctx->syn_cli.layer_type],
           nsc_layer_type[ctx->syn_cli.layer_subtype],
           ctx->syn_cli.ncci.schdl,
           ctx->syn_cli.shdl,
           ctx->syn_cli.ncci.sccid);
       if (write > 0) {
           capacity = (capacity > write) ? (capacity - write) : 0;
           index += write;
       }
    }

    if (capacity) {
       write = snprintf(buff + index, capacity, "\n");
    }

    BKNI_ReleaseMutex(ctx->mutex);

out:
    return;
}

static void hwc_binder_notify(int dev, int msg, struct hwc_notification_info &ntfy)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;

    if (ctx) {
       switch (msg) {
       case HWC_BINDER_NTFY_CONNECTED:
           ctx->hwc_binder->connected(true);
       break;
       case HWC_BINDER_NTFY_DISCONNECTED:
           ctx->hwc_binder->connected(false);
       break;
       case HWC_BINDER_NTFY_VIDEO_SURFACE_ACQUIRED:
       {
           for (int i = 0; i < NSC_MM_CLIENTS_NUMBER; i++) {
               // reset the 'drop duplicate frame notifier' count.
               if (ctx->mm_cli[i].root.ncci.sccid == (NEXUS_SurfaceCompositorClientId)ntfy.surface_hdl) {
                  ALOGD("%s: rest drop-dup-count on sfid %x, sccid %p", __FUNCTION__, ntfy.surface_hdl, ctx->mm_cli[i].root.ncci.sccid);
                  ctx->mm_cli[i].last_ping_frame_id = LAST_PING_FRAME_ID_INVALID;
                  break;
               }
           }
       }
       break;
       case HWC_BINDER_NTFY_SIDEBAND_SURFACE_ACQUIRED:
       default:
       break;
       }
   }
}

static bool is_video_layer(hwc_layer_1_t *layer, int layer_id, bool *is_sideband, bool *is_yuv)
{
    bool rc = false;
    NexusClientContext *client_context = NULL;

    if (is_sideband) {
        *is_sideband = false;
    }
    if (is_yuv) {
        *is_yuv = false;
    }

    if (((layer->compositionType == HWC_OVERLAY && layer->handle) ||
         (layer->compositionType == HWC_SIDEBAND && layer->sidebandStream))
        && !(layer->flags & HWC_SKIP_LAYER)) {

        client_context = NULL;

        if (layer->compositionType == HWC_OVERLAY) {
            int index = -1;
            private_handle_t *bcmBuffer = (private_handle_t *)layer->handle;
            PSHARED_DATA pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(bcmBuffer->sharedData);
            index = android_atomic_acquire_load(&pSharedData->videoWindow.windowIdPlusOne);
            if (index > 0) {
                client_context = reinterpret_cast<NexusClientContext *>(pSharedData->videoWindow.nexusClientContext);
                if (client_context != NULL) {
                   rc = true;
                }
            } else if ((pSharedData->planes[DEFAULT_PLANE].format == HAL_PIXEL_FORMAT_YV12) &&
                       (pSharedData->planes[EXTRA_PLANE].format == NEXUS_PixelFormat_eY08_Cb8_Y18_Cr8)) {
                if (is_yuv) {
                   *is_yuv = true;
                }
                rc = true;
            }
        } else if (layer->compositionType == HWC_SIDEBAND) {
            client_context = (NexusClientContext*)layer->sidebandStream->data[1];
            if (client_context != NULL) {
               rc = true;
            }
            if (is_sideband) {
               *is_sideband = true;
            }
        }
    }

    if (rc) {
       layer->hints |= HWC_HINT_CLEAR_FB;
       ALOGV("%s: found on layer %d.", __FUNCTION__, layer_id);
    }
    return rc;
}

static bool split_layer_scaling(
    struct hwc_context_t *ctx,
    hwc_layer_1_t *layer)
{
    bool ret = true;

    private_handle_t *bcmBuffer = (private_handle_t *)layer->handle;
    PSHARED_DATA pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(bcmBuffer->sharedData);

    NEXUS_Rect clip_position = {(int16_t)(int)layer->sourceCropf.left,
                                (int16_t)(int)layer->sourceCropf.top,
                                (uint16_t)((int)layer->sourceCropf.right - (int)layer->sourceCropf.left),
                                (uint16_t)((int)layer->sourceCropf.bottom - (int)layer->sourceCropf.top)};

    if (!layer->handle) {
        goto out;
    }

    if (is_video_layer(layer, -1, NULL, NULL)) {
        goto out;
    }

    if (clip_position.width && ((pSharedData->planes[DEFAULT_PLANE].width / clip_position.width) >= ctx->gfxCaps.maxHorizontalDownScale)) {
        ALOGV("%s: width: %d -> %d (>%d)", __FUNCTION__, pSharedData->planes[DEFAULT_PLANE].width, clip_position.width,
              ctx->gfxCaps.maxHorizontalDownScale);
        ret = false;
        goto out;
    }

    if (clip_position.height && ((pSharedData->planes[DEFAULT_PLANE].height / clip_position.height) >= ctx->gfxCaps.maxVerticalDownScale)) {
        ALOGV("%s: height: %d -> %d (>%d)", __FUNCTION__, pSharedData->planes[DEFAULT_PLANE].height, clip_position.height,
              ctx->gfxCaps.maxVerticalDownScale);
        ret = false;
        goto out;
    }

out:
    return ret;
}

static bool primary_need_nsc_layer(hwc_composer_device_1_t *dev, hwc_layer_1_t *layer)
{
    bool rc = false;
    struct hwc_context_t *ctx = (hwc_context_t*)dev;
    int skip_layer = -1;

    ++skip_layer; /* 0 */
    if ((layer->handle == NULL) &&
        (layer->compositionType != HWC_SIDEBAND)) {
        goto out;
    }

    ++skip_layer; /* 1 */
    if ((layer->compositionType == HWC_SIDEBAND) &&
        (layer->sidebandStream == NULL)) {
        goto out;
    }

    ++skip_layer; /* 2 */
    if (layer->flags & HWC_SKIP_LAYER) {
        goto out;
    }

    ++skip_layer; /* 3 */
    if (layer->compositionType == HWC_FRAMEBUFFER) {
        goto out;
    }

    ++skip_layer; /* 4 */
    if (layer->compositionType == HWC_FRAMEBUFFER_TARGET) {
        if (!ctx->display_gles_always && !ctx->display_gles_fallback) {
           goto out;
        }
        if (ctx->display_gles_fallback && !ctx->needs_fb_target) {
           goto out;
        }
        if (ctx->display_gles_always && !ctx->needs_fb_target) {
           goto out;
        }
    }

    rc = true;
out:
    return rc;
}

static void primary_composition_setup(hwc_composer_device_1_t *dev, hwc_display_contents_1_t* list)
{
    size_t i;
    hwc_layer_1_t *layer;
    struct hwc_context_t *ctx = (hwc_context_t*)dev;
    int skip_layer_index = -1;
    bool has_video = false;

    ctx->needs_fb_target = false;
    for (i = 0; i < list->numHwLayers; i++) {
        layer = &list->hwLayers[i];
        // we do not handle background layer at this time, we report such to SF.
        if (layer->compositionType == HWC_BACKGROUND)
           layer->compositionType = HWC_FRAMEBUFFER;
        // sideband layer stays a sideband layer.
        if (layer->compositionType == HWC_SIDEBAND)
           continue;
        // framebuffer target layer stays such (SF composing via GL into it, eg: animation/transition).
        if (layer->compositionType == HWC_FRAMEBUFFER_TARGET) {
           // note: to enable triple buffer on the FB target, set 'NUM_FRAMEBUFFER_SURFACE_BUFFERS := 3'
           //       in the BoardConfig.mk.
           //
           // layer->hints |= HWC_HINT_TRIPLE_BUFFER;
           continue;
        }
        // everything else should be an overlay unless we cannot handle it or not allowed to
        // handle it.
        if ((layer->compositionType == HWC_FRAMEBUFFER) ||
            (layer->compositionType == HWC_OVERLAY)) {
           layer->compositionType = HWC_FRAMEBUFFER;
           if (layer->flags & HWC_SKIP_LAYER) {
              if (ctx->display_gles_fallback) {
                 skip_layer_index = i;
              }
           } else {
              if (layer->handle) {
                 if ((layer->flags & HWC_IS_CURSOR_LAYER) && HWC_CURSOR_SURFACE_SUPPORTED) {
                    layer->compositionType = HWC_CURSOR_OVERLAY;
                 } else {
                    layer->compositionType = HWC_OVERLAY;
                    layer->hints |= HWC_HINT_TRIPLE_BUFFER;
                 }
                 if (ctx->display_gles_fallback && !split_layer_scaling(ctx, layer)) {
                    layer->compositionType = HWC_FRAMEBUFFER;
                 }
              }
           }
        }
    }

    for (i = 0; i < list->numHwLayers; i++) {
       layer = &list->hwLayers[i];
       if (is_video_layer(layer, -1, NULL, NULL)) {
          has_video = true;
          break;
       }
    }

    if ((ctx->display_gles_fallback && skip_layer_index != -1) ||
        (ctx->display_gles_always && has_video == false)) {
       for (i = 0; i < list->numHwLayers; i++) {
          layer = &list->hwLayers[i];
          if (layer->compositionType == HWC_OVERLAY &&
              !is_video_layer(layer, -1, NULL, NULL)) {
             layer->compositionType = HWC_FRAMEBUFFER;
             layer->hints = 0;
          }
       }
    }

    if (ctx->display_gles_fallback ||
        (ctx->display_gles_always && has_video == false)) {
       for (i = 0; i < list->numHwLayers; i++) {
           layer = &list->hwLayers[i];
           if (layer->compositionType == HWC_FRAMEBUFFER) {
               ctx->needs_fb_target = true;
               break;
           }
       }
    }
}

static int hwc_prepare_primary(hwc_composer_device_1_t *dev, hwc_display_contents_1_t* list)
{
    struct hwc_context_t *ctx = (hwc_context_t*)dev;
    hwc_layer_1_t *layer;
    size_t i;
    int nx_layer_count = 0;
    int skiped_layer = 0;
    unsigned int video_layer_id = 0;
    unsigned int sideband_layer_id = 0;

    // blocking wait here...
    BKNI_WaitForEvent(ctx->prepare_event, 1000);

    ctx->prepare_call++;

    // Don't prepare the primary if we are powering or powered off...
    if (BKNI_AcquireMutex(ctx->power_mutex) != BERR_SUCCESS) {
        ALOGE("%s: Could not acquire power_mutex!!!", __FUNCTION__);
        ctx->prepare_skipped++;
        goto out;
    }
    if ((ctx->power_mode != HWC_POWER_MODE_OFF) && (ctx->power_mode != HWC_POWER_MODE_DOZE_SUSPEND)) {
        BKNI_ReleaseMutex(ctx->power_mutex);

        if (ctx->hwc_binder) {
           ctx->hwc_binder->connect();
           hwc_binder_advertise_video_surface(ctx);
        }

        // setup the layer composition classification.
        primary_composition_setup(dev, list);

        // reset all video/sideband layers first.
        hwc_hide_unused_mm_layers(ctx);
        hwc_hide_unused_sb_layers(ctx);

        // allocate the NSC layer, if need be change the geometry, etc...
        for (i = 0; i < list->numHwLayers; i++) {
            layer = &list->hwLayers[i];
            if (primary_need_nsc_layer(dev, layer)) {
                if (HWC_DUMP_LAYER_CLASS) {
                   ALOGI("COMP: %d - SHOW - SF:%d (%d) -> NSC:%d", ctx->prepare_call, i, layer->compositionType, i);
                }
                hwc_nsc_prepare_layer(ctx, layer, (int)i,
                                      (bool)(list->flags & HWC_GEOMETRY_CHANGED),
                                      &video_layer_id,
                                      &sideband_layer_id);
            } else {
                if (HWC_DUMP_LAYER_CLASS) {
                   ALOGI("COMP: %d - HIDE - SF:%d (%d) -> NSC:%d", ctx->prepare_call, i, layer->compositionType, i);
                }
                hwc_hide_unused_gpx_layer(ctx, i);
            }
        }

        // remove all remaining gpx layers from the stack that are not needed.
        for ( i = list->numHwLayers; i < NSC_GPX_CLIENTS_NUMBER; i++) {
           hwc_hide_unused_gpx_layer(ctx, i);
        }
    }
    else {
        BKNI_ReleaseMutex(ctx->power_mutex);
    }

out:
    return 0;
}

static int hwc_prepare_virtual(hwc_composer_device_1_t *dev, hwc_display_contents_1_t* list)
{
    struct hwc_context_t *ctx = (hwc_context_t*)dev;
    int rc = 0;

    if (list) {
       // leave the first layer for the FB target, and let SF do the GL composition.
       list->numHwLayers = 1;
    }

    return rc;
}

static int hwc_prepare(hwc_composer_device_1_t *dev,
        size_t numDisplays, hwc_display_contents_1_t** displays)
{
    int rc = 0;

    if (!numDisplays)
        return -EINVAL;

    for (int32_t i = numDisplays - 1; i >= 0; i--) {
        hwc_display_contents_1_t *list = displays[i];
        switch (i) {
           case HWC_DISPLAY_PRIMARY:
               rc = hwc_prepare_primary(dev, list);
           break;
           case HWC_DISPLAY_VIRTUAL:
              rc = hwc_prepare_virtual(dev, list);
           break;
           case HWC_DISPLAY_EXTERNAL:
           default:
              rc = -EINVAL;
           break;
        }
    }

    return rc;
}

static int hwc_set_primary(struct hwc_context_t *ctx, hwc_display_contents_1_t* list)
{
    size_t i;
    NEXUS_Error rc;
    bool is_sideband = false;
    bool is_yuv = false;
    bool is_video = false;
    int overlay_seen = 0;
    int fb_target_seen = 0;
    int fence_id = INVALID_FENCE;
    struct sync_fence_info_data fence;
    int layer_composed = 0;

    // should never happen as primary display should always be there.
    if (!list)
       return -EINVAL;

    ctx->set_call++;

    if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
        ctx->set_skipped++;
        goto out;
    }

    // Don't set the primary display if we are powering or powered off...
    if (BKNI_AcquireMutex(ctx->power_mutex) != BERR_SUCCESS) {
        ALOGE("%s: Could not acquire power_mutex!!!", __FUNCTION__);
        ctx->set_skipped++;
        goto out_mutex;
    }
    if ((ctx->power_mode != HWC_POWER_MODE_OFF) && (ctx->power_mode != HWC_POWER_MODE_DOZE_SUSPEND)) {
        BKNI_ReleaseMutex(ctx->power_mutex);
        NEXUS_SurfaceCompositor_SetPause(NULL, true);

        if (ctx->sync_timeline != INVALID_FENCE) {
           snprintf(fence.name, sizeof(fence.name), "hwc_prim_%d", ctx->set_call);
           fence_id = sw_sync_fence_create(ctx->sync_timeline, fence.name, ctx->set_call);
           if (fence_id < 0) {
              ALOGW("%s: failed composition sync fence: %s", __FUNCTION__, fence.name);
           } else if (HWC_DISPLAY_FENCE_VERBOSE) {
              ALOGI("%s: composition sync fence: %s, fence: %d", __FUNCTION__, fence.name, fence_id);
           }
        }
        list->retireFenceFd = fence_id;

        for (i = 0; i < list->numHwLayers; i++) {
            list->hwLayers[i].releaseFenceFd = INVALID_FENCE;
            //
            // video layers have a no-op 'hidden' gpx layer association, so this check is valid
            // for all since gpx layers > mm layers.
            //
            if (i > NSC_GPX_CLIENTS_NUMBER) {
                ALOGE("Exceedeed max number of accounted layers\n");
                break;
            }
            //
            // video layer: signal the buffer to be displayed, drop duplicates.
            //
            is_video = is_video_layer(&list->hwLayers[i], -1, &is_sideband, &is_yuv);
            if (is_video && !is_yuv) {
                if (!is_sideband) {
                    private_handle_t *bcmBuffer = (private_handle_t *)list->hwLayers[i].handle;
                    PSHARED_DATA pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(bcmBuffer->sharedData);
                    if (ctx->hwc_binder) {
                        // TODO: currently only one video window exposed.
                        if (ctx->mm_cli[0].last_ping_frame_id != pSharedData->videoFrame.status.serialNumber) {
                            ctx->mm_cli[0].last_ping_frame_id = pSharedData->videoFrame.status.serialNumber;
                            ctx->hwc_binder->setframe(ctx->mm_cli[0].root.ncci.sccid, ctx->mm_cli[0].last_ping_frame_id);
                        }
                    }
                }
            }
            //
            // gpx layer: use the 'cached' visibility that was assigned during 'prepare'.
            //
            else if (ctx->gpx_cli[i].composition.visible) {
                if (!ctx->gpx_cli[i].skip_set) {
                   if (list->hwLayers[i].compositionType == HWC_OVERLAY) {
                      overlay_seen++;
                   } else if (list->hwLayers[i].compositionType == HWC_FRAMEBUFFER_TARGET) {
                      fb_target_seen++;
                   }
                   // assign a fence for each layer we have marked as 'overlay'/'framebuffer_target' that are
                   // purely gpx composition through nsc.
                   if ((list->hwLayers[i].compositionType == HWC_OVERLAY) ||
                       (list->hwLayers[i].compositionType == HWC_FRAMEBUFFER_TARGET)) {
                      fence_id = INVALID_FENCE;
                      if (ctx->sync_timeline != INVALID_FENCE) {
                         snprintf(fence.name, sizeof(fence.name), "hwc_prim_%d_%d", ctx->set_call, i);
                         fence_id = sw_sync_fence_create(ctx->sync_timeline, fence.name, ctx->set_call);
                         if (fence_id < 0) {
                            ALOGW("%s: failed layer sync fence: %s", __FUNCTION__, fence.name);
                         } else if (HWC_DISPLAY_FENCE_VERBOSE) {
                            ALOGI("%s: layer sync fence: %s, fence: %d", __FUNCTION__, fence.name, fence_id);
                         }
                      }
                      list->hwLayers[i].releaseFenceFd = fence_id;
                   }
                   int six = hwc_gpx_push_surface_locked(&ctx->gpx_cli[i]);
                   if (six != -1) {
                      ctx->gpx_cli[i].slist[six].comp_ix = ctx->set_call;
                      rc = NEXUS_SurfaceClient_PushSurface(ctx->gpx_cli[i].ncci.schdl, ctx->gpx_cli[i].slist[six].shdl, NULL, false);
                      if (rc) {
                          if (list->hwLayers[i].releaseFenceFd != INVALID_FENCE) {
                             close(list->hwLayers[i].releaseFenceFd);
                             list->hwLayers[i].releaseFenceFd = INVALID_FENCE;
                          }
                          hwc_unlock_surface((private_handle_t *)ctx->gpx_cli[i].slist[six].grhdl);
                          ctx->gpx_cli[i].slist[six].comp_ix = -1;
                          ctx->gpx_cli[i].slist[six].grhdl = NULL;
                          ctx->gpx_cli[i].slist[six].owner = SURF_OWNER_NO_OWNER;
                          if ((ctx->gpx_cli[i].layer_subtype == NEXUS_CURSOR) &&
                              ctx->gpx_cli[i].slist[six].schdl) {
                              NEXUS_SurfaceCursor_Destroy(ctx->gpx_cli[i].slist[six].schdl);
                              ctx->gpx_cli[i].slist[six].schdl = NULL;
                          }
                          if (ctx->gpx_cli[i].slist[six].shdl) {
                             NEXUS_Surface_Destroy(ctx->gpx_cli[i].slist[six].shdl);
                             ctx->gpx_cli[i].slist[six].shdl = NULL;
                          }
                          ALOGE("%s: push surface %p failed on layer %d, rc=%d\n", __FUNCTION__, ctx->gpx_cli[i].slist[six].shdl, i, rc);
                      } else {
                          layer_composed++;
                      }
                   }
                } else {
                   ctx->gpx_cli[i].skip_set = false;
                }
            }
        }

        NEXUS_SurfaceCompositor_SetPause(NULL, false);

        if (layer_composed == 0) {
           if (list->retireFenceFd != INVALID_FENCE) {
              sw_sync_timeline_inc(ctx->sync_timeline, 1);
              close(list->retireFenceFd);
              list->retireFenceFd = INVALID_FENCE;
           }
        }
    }
    else {
        BKNI_ReleaseMutex(ctx->power_mutex);
    }

    ALOGV("%s: composition %d: overlay %d, fb_target:%d, composed: %d\n", __FUNCTION__, ctx->set_call, overlay_seen, fb_target_seen, layer_composed);

out_mutex:
    BKNI_ReleaseMutex(ctx->mutex);

out:
    return 0;
}

static int hwc_set_virtual(struct hwc_context_t *ctx, hwc_display_contents_1_t* list)
{
    // nothing to do, surface-flinger would trigger the composition.
    (void)ctx;
    (void)list;

    return 0;
}

static int hwc_set(hwc_composer_device_1_t *dev,
        size_t numDisplays, hwc_display_contents_1_t** displays)
{
    struct hwc_context_t *ctx = (hwc_context_t*)dev;
    uint32_t i;
    int rc = 0;

    if (!numDisplays)
        return -EINVAL;

    for (i = 0; i < numDisplays; i++) {
        hwc_display_contents_1_t* list = displays[i];
        switch (i) {
           case HWC_DISPLAY_PRIMARY:
              rc = hwc_set_primary(ctx, list);
           break;
           case HWC_DISPLAY_VIRTUAL:
              rc = hwc_set_virtual(ctx, list);
           break;
           case HWC_DISPLAY_EXTERNAL:
           default:
              rc = -EINVAL;
           break;
        }
    }

    return rc;
}

static void hwc_device_cleanup(struct hwc_context_t* ctx)
{
    if (ctx) {
        ctx->vsync_thread_run = 0;
        pthread_join(ctx->vsync_callback_thread, NULL);

        if (ctx->pIpcClient != NULL) {
            if (ctx->pNexusClientContext != NULL) {
                ctx->pIpcClient->destroyClientContext(ctx->pNexusClientContext);
            }
            delete ctx->pIpcClient;
        }

        for (int i = 0; i < NSC_SB_CLIENTS_NUMBER; i++)
        {
           /* nothing to do since we do not acquire the surfaces explicitely. */
        }

        for (int i = 0; i < NSC_MM_CLIENTS_NUMBER; i++)
        {
           NEXUS_SurfaceClient_ReleaseVideoWindow(ctx->mm_cli[i].svchdl);
           NEXUS_SurfaceClient_Release(ctx->mm_cli[i].root.ncci.schdl);
        }

        for (int i = 0; i < NSC_GPX_CLIENTS_NUMBER; i++)
        {
           for (int j = 0; j < GPX_SURFACE_STACK; j++) {
              if (ctx->gpx_cli[i].slist[j].schdl)
                 NEXUS_SurfaceCursor_Destroy(ctx->gpx_cli[i].slist[j].schdl);
              if (ctx->gpx_cli[i].slist[j].shdl)
                 NEXUS_Surface_Destroy(ctx->gpx_cli[i].slist[j].shdl);
           }
           NEXUS_SurfaceClient_Release(ctx->gpx_cli[i].ncci.schdl);
        }

        if (ctx->syn_cli.shdl)
           NEXUS_Surface_Destroy(ctx->syn_cli.shdl);
        NEXUS_SurfaceClient_Release(ctx->syn_cli.ncci.schdl);

        NxClient_Free(&ctx->nxAllocResults);

        BKNI_DestroyMutex(ctx->vsync_callback_enabled_mutex);
        BKNI_DestroyMutex(ctx->mutex);
        BKNI_DestroyMutex(ctx->power_mutex);
        BKNI_DestroyMutex(ctx->dev_mutex);

        if (ctx->hwc_binder)
           delete ctx->hwc_binder;

        if (ctx->hwc_2dg)
           NEXUS_Graphics2D_Close(ctx->hwc_2dg);
        if (ctx->checkpoint_event)
           BKNI_DestroyEvent(ctx->checkpoint_event);

        if (ctx->sync_timeline != INVALID_FENCE)
           close(ctx->sync_timeline);
    }
}

static int hwc_device_close(struct hw_device_t *dev)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;

    hwc_device_cleanup(ctx);

    if (ctx) {
        free(ctx);
    }
    return 0;
}

static int hwc_device_setPowerMode(struct hwc_composer_device_1* dev, int disp, int mode)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    int rc = -EINVAL;

    if (disp == HWC_DISPLAY_PRIMARY) {
       ALOGI("%s: %s->%s", __FUNCTION__,
           hwc_power_mode[ctx->power_mode], hwc_power_mode[mode]);

       switch (mode) {
         case HWC_POWER_MODE_OFF:
         case HWC_POWER_MODE_DOZE:
         case HWC_POWER_MODE_DOZE_SUSPEND:
             break;
         case HWC_POWER_MODE_NORMAL:
             if (VSYNC_USES_NSC_SURF) {
                NEXUS_SurfaceClient_PushSurface(ctx->syn_cli.ncci.schdl, ctx->syn_cli.shdl, NULL, false);
             } else {
                /* note: needs restore after standby. */
                NEXUS_CallbackDesc desc;
                desc.callback = hw_vsync_cb;
                desc.context = (void *)&ctx->syn_cli;
                NEXUS_Display_SetVsyncCallback(1, &desc);
             }
             break;
         default:
            goto out;
       }

       if (BKNI_AcquireMutex(ctx->power_mutex) != BERR_SUCCESS) {
           ALOGE("%s: Could not acquire power_mutex!!!", __FUNCTION__);
           goto out;
       }
       ctx->power_mode = mode;
       BKNI_ReleaseMutex(ctx->power_mutex);
       rc = 0;
    }

out:
    return rc;
}

static int hwc_device_query(struct hwc_composer_device_1* dev, int what, int* value)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;

    (void) ctx;

    switch (what) {
       case HWC_BACKGROUND_LAYER_SUPPORTED:
           //TODO: add background layer support.
           *value = 0;
       break;

       case HWC_VSYNC_PERIOD:
           if (BKNI_AcquireMutex(ctx->dev_mutex) != BERR_SUCCESS) {
               return -EINVAL;
           }
           *value = (int) ctx->syn_cli.refresh;
           BKNI_ReleaseMutex(ctx->dev_mutex);
       break;

       case HWC_DISPLAY_TYPES_SUPPORTED:
           *value = HWC_DISPLAY_PRIMARY_BIT | HWC_DISPLAY_VIRTUAL_BIT;
       break;
    }

    return 0;
}

static void hwc_registerProcs(struct hwc_composer_device_1* dev, hwc_procs_t const* procs)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    ALOGI("HWC hwc_register_procs (%p)", procs);
    ctx->procs = (hwc_procs_t *)procs;
}

static int hwc_device_eventControl(struct hwc_composer_device_1* dev, int disp, int event, int enabled)
{
    int status = -EINVAL;

    (void)dev;
    (void)disp;

    if (event == HWC_EVENT_VSYNC) {
        struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
        if (BKNI_AcquireMutex(ctx->vsync_callback_enabled_mutex) == BERR_SUCCESS) {
           ctx->vsync_callback_enabled = enabled;
           status = 0;
        }
        BKNI_ReleaseMutex(ctx->vsync_callback_enabled_mutex);
        if (!status) ALOGV("HWC HWC_EVENT_VSYNC (%s)", enabled ? "enabled":"disabled");
    }
    else {
        ALOGE("HWC unknown event control requested %d:%d", event, enabled);
    }

    return status;
}

static int hwc_device_getDisplayConfigs(struct hwc_composer_device_1* dev, int disp,
    uint32_t* configs, size_t* numConfigs)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    int ret = 0;

    if (*numConfigs == 0 || configs == NULL)
       return ret;

    if (BKNI_AcquireMutex(ctx->dev_mutex) != BERR_SUCCESS) {
       return -EINVAL;
    }

    if (disp == HWC_DISPLAY_PRIMARY) {
        //TODO: support more configurations?
        *numConfigs = 1;
        configs[0] = DISPLAY_CONFIG_DEFAULT;
    } else {
        ret = -1;
    }

    BKNI_ReleaseMutex(ctx->dev_mutex);

    return ret;
}

static int hwc_device_getDisplayAttributes(struct hwc_composer_device_1* dev, int disp,
    uint32_t config, const uint32_t* attributes, int32_t* values)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    int i = 0;

    if (BKNI_AcquireMutex(ctx->dev_mutex) != BERR_SUCCESS) {
       return -EINVAL;
    }

    if (disp == HWC_DISPLAY_PRIMARY) {
        //TODO: add more configurations, hook up with actual edid.
        if (config == DISPLAY_CONFIG_DEFAULT) {
            while (attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE) {
               switch (attributes[i]) {
                   case HWC_DISPLAY_VSYNC_PERIOD:
                      values[i] = (int32_t)ctx->syn_cli.refresh;
                   break;
                   case HWC_DISPLAY_WIDTH:
                      values[i] = ctx->display_width;
                   break;
                   case HWC_DISPLAY_HEIGHT:
                      values[i] = ctx->display_height;
                   break;
                   case HWC_DISPLAY_DPI_X:
                   case HWC_DISPLAY_DPI_Y:
                      values[i] = 0;
                   break;
                   default:
                   break;
               }

               i++;
            }
            BKNI_ReleaseMutex(ctx->dev_mutex);
            return 0;
        }
    }

    BKNI_ReleaseMutex(ctx->dev_mutex);
    return -EINVAL;
}

static int hwc_device_getActiveConfig(struct hwc_composer_device_1* dev, int disp)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;

    if (BKNI_AcquireMutex(ctx->dev_mutex) != BERR_SUCCESS) {
       return -EINVAL;
    }

    if (disp == HWC_DISPLAY_PRIMARY) {
        // always first one, index 0.
        BKNI_ReleaseMutex(ctx->dev_mutex);
        return 0;
    } else {
        BKNI_ReleaseMutex(ctx->dev_mutex);
        return -1;
    }
}

static int hwc_device_setActiveConfig(struct hwc_composer_device_1* dev, int disp, int index)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;

    if (BKNI_AcquireMutex(ctx->dev_mutex) != BERR_SUCCESS) {
       return -EINVAL;
    }

    if (disp == HWC_DISPLAY_PRIMARY && index == 0) {
       BKNI_ReleaseMutex(ctx->dev_mutex);
       return 0;
    }

    BKNI_ReleaseMutex(ctx->dev_mutex);
    return -EINVAL;
}

static int hwc_device_setCursorPositionAsync(struct hwc_composer_device_1 *dev, int disp, int x_pos, int y_pos)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;

    if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
       return -EINVAL;
    }

    if (disp == HWC_DISPLAY_PRIMARY) {
       NEXUS_SurfaceCursorHandle cursor = hwc_gpx_update_cursor_surface_locked(dev);
       if (cursor != NULL) {
          NEXUS_SurfaceCursorSettings config;
          NEXUS_SurfaceCursor_GetSettings(cursor, &config);
          config.composition.position.x = x_pos;
          config.composition.position.y = y_pos;
          NEXUS_SurfaceCursor_SetSettings(cursor, &config);
          // TODO: push right away?  or wait for next hwc_set?
       } else {
          //ALOGE("%s: failed cursor update (display %d, pos {%d,%d})", __FUNCTION__, disp, x_pos, y_pos);
       }
    }

    BKNI_ReleaseMutex(ctx->mutex);
    return 0;
}


/*****************************************************************************/

static int64_t VsyncSystemTime(void)
{
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)(t.tv_sec) * 1000000000LL + t.tv_nsec;
}

static void * hwc_vsync_task(void *argv)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)argv;

    ALOGI("HWC Starting VSYNC thread");
    do
    {
        if ((VSYNC_USES_NSC_SURF && ctx->syn_cli.shdl) || !VSYNC_USES_NSC_SURF) {
            if (BKNI_AcquireMutex(ctx->power_mutex) != BERR_SUCCESS) {
                ALOGE("%s: Could not acquire power_mutex!!!", __FUNCTION__);
                goto out;
            }
            if ((ctx->power_mode != HWC_POWER_MODE_OFF) && (ctx->power_mode != HWC_POWER_MODE_DOZE_SUSPEND)) {
                BKNI_ReleaseMutex(ctx->power_mutex);
                if (VSYNC_USES_NSC_SURF) {
                   NEXUS_SurfaceClient_PushSurface(ctx->syn_cli.ncci.schdl, ctx->syn_cli.shdl, NULL, false);
                }
                BKNI_WaitForEvent(ctx->syn_cli.vsync_event, BKNI_INFINITE);
            }
            else {
                BKNI_ReleaseMutex(ctx->power_mutex);
                BKNI_Sleep(16);
            }
            BKNI_SetEvent(ctx->prepare_event);
        }

        if (BKNI_AcquireMutex(ctx->vsync_callback_enabled_mutex) == BERR_SUCCESS) {
            if (ctx->vsync_callback_enabled && ctx->procs->vsync !=NULL) {
                ctx->procs->vsync(const_cast<hwc_procs_t *>(ctx->procs), 0, VsyncSystemTime());
            }
        }
        BKNI_ReleaseMutex(ctx->vsync_callback_enabled_mutex);

    } while(ctx->vsync_thread_run);

out:
    return NULL;
}

static bool hwc_standby_monitor(void *dev)
{
    bool standby = false;
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;

    if (BKNI_AcquireMutex(ctx->power_mutex) != BERR_SUCCESS) {
       ALOGE("%s: Could not acquire power_mutex!!!", __FUNCTION__);
       goto out;
    }
    standby = (ctx == NULL) || (ctx->power_mode == HWC_POWER_MODE_OFF);
    ALOGV("%s: standby=%d", __FUNCTION__, standby);
    BKNI_ReleaseMutex(ctx->power_mutex);

out:
    return standby;
}

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = -EINVAL;
    char value[PROPERTY_VALUE_MAX];
    struct hwc_context_t *dev = NULL;

    if (!strcmp(name, HWC_HARDWARE_COMPOSER)) {
        NEXUS_Error rc = NEXUS_SUCCESS;
        NEXUS_Graphics2DSettings gfxSettings;
        pthread_attr_t attr;
        dev = (hwc_context_t*)calloc(1, sizeof(*dev));

        if (dev == NULL) {
           ALOGE("Failed to create hwcomposer!");
           goto out;
        }

        dev->display_width = 1280;
        dev->display_height = 720;
        if (property_get(DISPLAY_HD_RES_PROP, value, DISPLAY_DEFAULT_HD_RES)) {
           if (!strncmp((char *) value, DISPLAY_DEFAULT_HD_RES, 5)) {
              dev->display_width = 1920;
              dev->display_height = 1080;
           }
        }
        dev->cursor_layer_id = -1;

        if (property_get(HWC_GLES_MODE_PROP, value, HWC_DEFAULT_GLES_MODE)) {
           dev->display_gles_fallback =
              (strncmp(value, HWC_GLES_MODE_FALLBACK, strlen(HWC_GLES_MODE_FALLBACK)) == 0) ? true : false;
           dev->display_gles_always =
              (strncmp(value, HWC_GLES_MODE_ALWAYS, strlen(HWC_GLES_MODE_ALWAYS)) == 0) ? true : false;
        }

        {
            NxClient_AllocSettings nxAllocSettings;
            NEXUS_SurfaceClientSettings client_settings;
            NEXUS_SurfaceCreateSettings surface_create_settings;
            NEXUS_SurfaceMemory vsync_surface_memory;
            NEXUS_Rect vsync_disp_position = {0,0,VSYNC_CLIENT_WIDTH,VSYNC_CLIENT_HEIGHT};
            NEXUS_SurfaceComposition vsync_composition;

            NxClient_GetDefaultAllocSettings(&nxAllocSettings);
            nxAllocSettings.surfaceClient = NSC_GPX_CLIENTS_NUMBER;
            if (VSYNC_USES_NSC_SURF)
               nxAllocSettings.surfaceClient += VSYNC_USES_NSC_SURF;
            if (!HWC_MM_NO_ALLOC_SURF_CLI)
               nxAllocSettings.surfaceClient += NSC_MM_CLIENTS_NUMBER;
            if (!HWC_SB_NO_ALLOC_SURF_CLI)
               nxAllocSettings.surfaceClient += NSC_SB_CLIENTS_NUMBER;
            rc = NxClient_Alloc(&nxAllocSettings, &dev->nxAllocResults);
            if (rc)
            {
                ALOGE("%s: Cannot allocate NxClient graphic window (rc=%d)!", __FUNCTION__, rc);
                goto out;
            }

            BKNI_CreateEvent(&dev->syn_cli.vsync_event);
            dev->syn_cli.layer_type    = HWC_OVERLAY;
            dev->syn_cli.layer_subtype = NEXUS_SYNC;
            dev->syn_cli.refresh       = VSYNC_CLIENT_REFRESH;
            dev->syn_cli.ncci.type     = NEXUS_CLIENT_VSYNC;
            dev->syn_cli.ncci.parent   = (void *)dev;

            /* create vsync nx client */
            if (VSYNC_USES_NSC_SURF) {
               dev->syn_cli.ncci.sccid    = dev->nxAllocResults.surfaceClient[VSYNC_CLIENT_LAYER_ID].id;
               dev->syn_cli.ncci.schdl    = NEXUS_SurfaceClient_Acquire(dev->syn_cli.ncci.sccid);
               if (!dev->syn_cli.ncci.schdl) {
                   ALOGE("%s: NEXUS_SurfaceClient_Acquire vsync client failed", __FUNCTION__);
                   goto clean_up;
               }
               NEXUS_SurfaceClient_GetSettings(dev->syn_cli.ncci.schdl, &client_settings);
               client_settings.recycled.callback = hwc_sync_recycled_cb;
               client_settings.recycled.context = (void *)&dev->syn_cli;
               rc = NEXUS_SurfaceClient_SetSettings(dev->syn_cli.ncci.schdl, &client_settings);
               if (rc) {
                   ALOGE("%s: NEXUS_SurfaceClient_SetSettings vsync client failed", __FUNCTION__);
                   goto clean_up;
               }
               dev->syn_cli.shdl = hwc_to_nsc_surface(VSYNC_CLIENT_WIDTH,
                                                      VSYNC_CLIENT_HEIGHT,
                                                      VSYNC_CLIENT_STRIDE,
                                                      NEXUS_PixelFormat_eA8_B8_G8_R8,
                                                      NULL);
               if (dev->syn_cli.shdl == NULL) {
                   ALOGE("%s: hwc_to_nsc_surface vsync client failed", __FUNCTION__);
                   goto clean_up;
               }

               NEXUS_Surface_GetMemory(dev->syn_cli.shdl, &vsync_surface_memory);
               memset(vsync_surface_memory.buffer, 0, vsync_surface_memory.bufferSize);

               NxClient_GetSurfaceClientComposition(dev->syn_cli.ncci.sccid, &vsync_composition);
               vsync_composition.position              = vsync_disp_position;
               vsync_composition.virtualDisplay.width  = dev->display_width;
               vsync_composition.virtualDisplay.height = dev->display_height;
               vsync_composition.visible               = true;
               vsync_composition.zorder                = VSYNC_CLIENT_ZORDER;
               NxClient_SetSurfaceClientComposition(dev->syn_cli.ncci.sccid, &vsync_composition);
            }

            /* create layers nx gpx clients */
            for (int i = 0; i < NSC_GPX_CLIENTS_NUMBER; i++)
            {
                dev->gpx_cli[i].ncci.parent = (void *)dev;
                dev->gpx_cli[i].layer_id = i;
                dev->gpx_cli[i].ncci.type = NEXUS_CLIENT_GPX;
                dev->gpx_cli[i].ncci.sccid = dev->nxAllocResults.surfaceClient[VSYNC_USES_NSC_SURF+i].id;
                dev->gpx_cli[i].ncci.schdl = NEXUS_SurfaceClient_Acquire(dev->gpx_cli[i].ncci.sccid);
                if (!dev->gpx_cli[i].ncci.schdl)
                {
                    ALOGE("%s:gpx: NEXUS_SurfaceClient_Acquire lid=%d cid=%x sc=%x failed", __FUNCTION__,
                        i, dev->gpx_cli[i].ncci.sccid, dev->gpx_cli[i].ncci.schdl);
                    goto clean_up;
                }

                NEXUS_SurfaceClient_GetSettings(dev->gpx_cli[i].ncci.schdl, &client_settings);
                client_settings.recycled.callback = hwc_nsc_recycled_cb;
                client_settings.recycled.context = (void *)&dev->gpx_cli[i];
                rc = NEXUS_SurfaceClient_SetSettings(dev->gpx_cli[i].ncci.schdl, &client_settings);
                if (rc) {
                    ALOGE("%s:gpx: NEXUS_SurfaceClient_SetSettings %d failed", __FUNCTION__, i);
                    goto clean_up;
                }

                memset(&dev->gpx_cli[i].composition, 0, sizeof(NEXUS_SurfaceComposition));
            }

            /* create layers nx mm clients */
            for (int i = 0; i < NSC_MM_CLIENTS_NUMBER; i++)
            {
                dev->mm_cli[i].root.ncci.parent = (void *)dev;
                dev->mm_cli[i].root.layer_id = i;
                dev->mm_cli[i].root.ncci.type = NEXUS_CLIENT_MM;
                if (!HWC_MM_NO_ALLOC_SURF_CLI) {
                    dev->mm_cli[i].root.ncci.sccid = dev->nxAllocResults.surfaceClient[VSYNC_USES_NSC_SURF+NSC_GPX_CLIENTS_NUMBER+i].id;
                    dev->mm_cli[i].root.ncci.schdl = NEXUS_SurfaceClient_Acquire(dev->mm_cli[i].root.ncci.sccid);
                    if (!dev->mm_cli[i].root.ncci.schdl)
                    {
                        ALOGE("%s:mm: NEXUS_SurfaceClient_Acquire lid=%d cid=%x sc=%x failed", __FUNCTION__,
                             i, dev->mm_cli[i].root.ncci.sccid, dev->mm_cli[i].root.ncci.schdl);
                        goto clean_up;
                    }

                    NEXUS_SurfaceClient_GetSettings(dev->mm_cli[i].root.ncci.schdl, &client_settings);
                    client_settings.recycled.callback = hwc_nsc_recycled_cb;
                    client_settings.recycled.context = (void *)&dev->mm_cli[i];
                    rc = NEXUS_SurfaceClient_SetSettings(dev->mm_cli[i].root.ncci.schdl, &client_settings);
                    if (rc) {
                        ALOGE("%s:mm: NEXUS_SurfaceClient_SetSettings %d failed", __FUNCTION__, i);
                        goto clean_up;
                    }
                    dev->mm_cli[i].svchdl = NEXUS_SurfaceClient_AcquireVideoWindow(dev->mm_cli[i].root.ncci.schdl, 0 /* always 0 */);
                    if (dev->mm_cli[i].svchdl == NULL) {
                        ALOGE("%s:mm: NEXUS_SurfaceClient_AcquireVideoWindow %d failed", __FUNCTION__, i);
                        goto clean_up;
                    }
                } else {
                    dev->mm_cli[i].root.ncci.sccid = INVALID_HANDLE + i;
                }

                memset(&dev->mm_cli[i].root.composition, 0, sizeof(NEXUS_SurfaceComposition));
                dev->mm_cli[i].last_ping_frame_id = LAST_PING_FRAME_ID_INVALID;

                // TODO: add support for more than one video at the time.
                if (i == 0) {
                   dev->nsc_video_changed = true;
                }
            }

            /* create layers nx sb clients */
            for (int i = 0; i < NSC_SB_CLIENTS_NUMBER; i++)
            {
                dev->sb_cli[i].root.ncci.parent = (void *)dev;
                dev->sb_cli[i].root.layer_id = i;
                dev->sb_cli[i].root.ncci.type = NEXUS_CLIENT_SB;
                if (!HWC_SB_NO_ALLOC_SURF_CLI) {
                    dev->sb_cli[i].root.ncci.sccid = dev->nxAllocResults.surfaceClient[VSYNC_USES_NSC_SURF+NSC_GPX_CLIENTS_NUMBER+NSC_MM_CLIENTS_NUMBER+i].id;
                    /* do not acquire surface client nor video window to allow the user of this to own it instead. */
                } else {
                    dev->sb_cli[i].root.ncci.sccid = INVALID_HANDLE + i;
                }

                memset(&dev->mm_cli[i].root.composition, 0, sizeof(NEXUS_SurfaceComposition));

                // TODO: add support for more than one sideband at the time.
                if (i == 0) {
                   dev->nsc_sideband_changed = true;
                }
            }

            BKNI_CreateEvent(&dev->prepare_event);
        }

        dev->device.common.tag                     = HARDWARE_DEVICE_TAG;
        dev->device.common.version                 = HWC_DEVICE_API_VERSION_1_4;
        dev->device.common.module                  = const_cast<hw_module_t*>(module);
        dev->device.common.close                   = hwc_device_close;

        dev->device.prepare                        = hwc_prepare;
        dev->device.set                            = hwc_set;
        dev->device.setPowerMode                   = hwc_device_setPowerMode;
        dev->device.eventControl                   = hwc_device_eventControl;
        dev->device.query                          = hwc_device_query;
        dev->device.registerProcs                  = hwc_registerProcs;
        dev->device.dump                           = hwc_device_dump;
        dev->device.getDisplayConfigs              = hwc_device_getDisplayConfigs;
        dev->device.getDisplayAttributes           = hwc_device_getDisplayAttributes;
        dev->device.getActiveConfig                = hwc_device_getActiveConfig;
        dev->device.setActiveConfig                = hwc_device_setActiveConfig;
        dev->device.setCursorPositionAsync         = hwc_device_setCursorPositionAsync;

        if (BKNI_CreateMutex(&dev->vsync_callback_enabled_mutex) == BERR_OS_ERROR)
           goto clean_up;
        if (BKNI_CreateMutex(&dev->mutex) == BERR_OS_ERROR)
           goto clean_up;
        if (BKNI_CreateMutex(&dev->power_mutex) == BERR_OS_ERROR)
           goto clean_up;
        if (BKNI_CreateMutex(&dev->dev_mutex) == BERR_OS_ERROR)
           goto clean_up;

        dev->vsync_thread_run = 1;

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
        }

        pthread_attr_init(&attr);
        /* TODO: set schedpolicy to RR? */
        if (pthread_create(&dev->vsync_callback_thread, &attr, hwc_vsync_task, (void *)dev) != 0) {
            goto clean_up;
        }
        pthread_attr_destroy(&attr);

        dev->hwc_binder = new HwcBinder_wrap;
        if (dev->hwc_binder == NULL) {
           ALOGE("%s: failed to create hwcbinder, some services will not run!", __FUNCTION__);
        } else {
           dev->hwc_binder->get()->register_notify(&hwc_binder_notify, (int)dev);
           ALOGE("%s: created hwcbinder (%p)", __FUNCTION__, dev->hwc_binder);
        }

        rc = BKNI_CreateEvent(&dev->checkpoint_event);
        if ( rc ) {
          ALOGE(("Unable to create checkpoint event"));
          goto clean_up;
        }

        dev->hwc_2dg = NEXUS_Graphics2D_Open(NEXUS_ANY_ID, NULL);
        if (dev->hwc_2dg == NULL) {
           ALOGE("%s: failed to create hwc_2dg, conversion services will not work!", __FUNCTION__);
        } else {
           NEXUS_Graphics2D_GetSettings(dev->hwc_2dg, &gfxSettings);
           gfxSettings.pollingCheckpoint = false;
           gfxSettings.checkpointCallback.callback = hwc_checkpoint_callback;
           gfxSettings.checkpointCallback.context = (void *)dev;
           rc = NEXUS_Graphics2D_SetSettings(dev->hwc_2dg, &gfxSettings);
           if (rc) {
              ALOGE("%s: failed to setup hwc_2dg checkpoint, conversion services will not work!", __FUNCTION__);
           }
           NEXUS_Graphics2D_GetCapabilities(dev->hwc_2dg, &dev->gfxCaps);
           ALOGI("%s: gfx caps: h-down-scale: %d, v-down-scale: %d", __FUNCTION__,
                 dev->gfxCaps.maxHorizontalDownScale, dev->gfxCaps.maxVerticalDownScale);
        }
        if ((dev->hwc_2dg != NULL) && !HWC_DO_YV12_CONV) {
           NEXUS_Graphics2D_Close(dev->hwc_2dg);
           dev->hwc_2dg = NULL;
        }

        if (property_get(HWC_SW_SYNC_PROP, value, HWC_DEFAULT_SW_SYNC)) {
           if (strtoul(value, NULL, 10) == 0) {
              ALOGI("%s: sync timeline disabled", __FUNCTION__);
              dev->sync_timeline = INVALID_FENCE;
           } else {
              dev->sync_timeline = sw_sync_timeline_create();
              if (dev->sync_timeline < 0) {
                 ALOGW("%s: failed to create sync timeline, not using fences", __FUNCTION__);
                 dev->sync_timeline = INVALID_FENCE;
              }
           }
        }

        *device = &dev->device.common;
        status = 0;
        goto out;
    }

clean_up:
    hwc_device_cleanup(dev);
out:
    return status;
}

static void hwc_sync_recycled_cb(void *context, int param)
{
    NEXUS_Error rc;
    VSYNC_CLIENT_INFO *ci = (VSYNC_CLIENT_INFO *)context;
    NEXUS_SurfaceHandle recycledSurface = NULL;
    size_t n = 0;
    BSTD_UNUSED(param);

    if (!VSYNC_USES_NSC_SURF) {
       ALOGE("%s: invalid vsync path, check client configuration!", __FUNCTION__);
    }

    do {
          rc = NEXUS_SurfaceClient_RecycleSurface(ci->ncci.schdl, &recycledSurface, 1, &n);
          if (rc) {
              break;
          }
    }
    while (n > 0);

    BKNI_SetEvent(ci->vsync_event);
}

static void hw_vsync_cb(void *context, int param)
{
    VSYNC_CLIENT_INFO *ci = (VSYNC_CLIENT_INFO *)context;
    struct hwc_context_t* ctx = (struct hwc_context_t*)ci->ncci.parent;
    BSTD_UNUSED(param);

    if (VSYNC_USES_NSC_SURF) {
       ALOGE("%s: invalid vsync path, check client configuration!", __FUNCTION__);
    }

    BKNI_SetEvent(ci->vsync_event);
}

static void hwc_nsc_recycled_cb(void *context, int param)
{
    NEXUS_Error rc;
    GPX_CLIENT_INFO *ci = (GPX_CLIENT_INFO *)context;
    struct hwc_context_t* ctx = (struct hwc_context_t*)ci->ncci.parent;
    NEXUS_SurfaceHandle recycledSurface;
    size_t n = 0;
    int six = -1;
    int composition_done;
    BSTD_UNUSED(param);

    if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
        goto out;
    }
    composition_done = -1;
    do {
          recycledSurface = NULL;
          rc = NEXUS_SurfaceClient_RecycleSurface(ci->ncci.schdl, &recycledSurface, 1, &n);
          if (rc) {
              break;
          }
          if ((n > 0) && (recycledSurface != NULL)) {
              six = hwc_gpx_get_recycle_surface_locked(ci, recycledSurface);
              if (six != -1) {
                 if (composition_done == -1) {
                    composition_done = ci->slist[six].comp_ix;
                 }
                 if ((ci->slist[six].comp_ix != INVALID_FENCE) &&
                     (composition_done > ci->slist[six].comp_ix)) {
                    composition_done = ci->slist[six].comp_ix;
                 }
                 // do not recycle the cursor surface, it is being re-used.
                 if (ci->layer_subtype == NEXUS_CURSOR) {
                    ci->slist[six].owner = SURF_OWNER_HWC;
                    recycledSurface = NULL;
                    ci->slist[six].comp_ix = -1;
                 } else {
                    hwc_unlock_surface((private_handle_t *)ci->slist[six].grhdl);
                    ci->slist[six].owner = SURF_OWNER_NO_OWNER;
                    ci->slist[six].shdl = NULL;
                    ci->slist[six].grhdl = NULL;
                    ci->slist[six].comp_ix = -1;
                 }
              }
              if (recycledSurface)
                 NEXUS_Surface_Destroy(recycledSurface);
          }

          if (ctx && (ctx->sync_timeline != INVALID_FENCE)) {
             int sync_increment = ((composition_done == -1) ? 1 : (ctx->set_call - composition_done));
             for (six = 0 ; six < sync_increment ; six++) {
                sw_sync_timeline_inc(ctx->sync_timeline, 1);
             }
             if (HWC_DISPLAY_FENCE_VERBOSE) {
                ALOGI("%s: increment sync timeline: %d, current: %d, comp: %d", __FUNCTION__,
                   sync_increment, ctx->set_call, composition_done);
             }
         }
    }
    while (n > 0);

    BKNI_ReleaseMutex(ctx->mutex);

out:
    return;
}

enum BLENDIND_TYPE {
    BLENDIND_TYPE_SRC,
    BLENDIND_TYPE_SRC_OVER,
    BLENDIND_TYPE_SRC_OVER_NON_PREMULTIPLIED,
    BLENDIND_TYPE_LAST
} BLENDIND_TYPE;

const NEXUS_BlendEquation colorBlendingEquation[BLENDIND_TYPE_LAST] = {
    { /* BLENDIND_TYPE_SRC */
        NEXUS_BlendFactor_eSourceColor,
        NEXUS_BlendFactor_eOne,
        false,
        NEXUS_BlendFactor_eZero,
        NEXUS_BlendFactor_eZero,
        false,
        NEXUS_BlendFactor_eZero
    },
    { /* BLENDIND_TYPE_SRC_OVER */
        NEXUS_BlendFactor_eSourceColor,
        NEXUS_BlendFactor_eOne,
        false,
        NEXUS_BlendFactor_eDestinationColor,
        NEXUS_BlendFactor_eInverseSourceAlpha,
        false,
        NEXUS_BlendFactor_eZero
    },
    { /* BLENDIND_TYPE_SRC_OVER_NON_PREMULTIPLIED */
        NEXUS_BlendFactor_eSourceColor,
        NEXUS_BlendFactor_eSourceAlpha,
        false,
        NEXUS_BlendFactor_eDestinationColor,
        NEXUS_BlendFactor_eInverseSourceAlpha,
        false,
        NEXUS_BlendFactor_eZero
    }
};

const NEXUS_BlendEquation alphaBlendingEquation[BLENDIND_TYPE_LAST] = {
    { /* BLENDIND_TYPE_SRC */
        NEXUS_BlendFactor_eSourceAlpha,
        NEXUS_BlendFactor_eOne,
        false,
        NEXUS_BlendFactor_eZero,
        NEXUS_BlendFactor_eZero,
        false,
        NEXUS_BlendFactor_eZero
    },
    { /* BLENDIND_TYPE_SRC_OVER */
        NEXUS_BlendFactor_eSourceAlpha,
        NEXUS_BlendFactor_eOne,
        false,
        NEXUS_BlendFactor_eDestinationAlpha,
        NEXUS_BlendFactor_eInverseSourceAlpha,
        false,
        NEXUS_BlendFactor_eZero
    },
    { /* BLENDIND_TYPE_SRC_OVER_NON_PREMULTIPLIED */
        NEXUS_BlendFactor_eSourceAlpha,
        NEXUS_BlendFactor_eOne,
        false,
        NEXUS_BlendFactor_eDestinationAlpha,
        NEXUS_BlendFactor_eInverseSourceAlpha,
        false,
        NEXUS_BlendFactor_eZero
    },
};

static void hwc_prepare_gpx_layer(
    hwc_context_t* ctx,
    hwc_layer_1_t *layer,
    int layer_id,
    bool geometry_changed)
{
    NEXUS_Error rc;
    private_handle_t *bcmBuffer = NULL;
    NEXUS_SurfaceCreateSettings createSettings;
    NEXUS_Rect disp_position = {(int16_t)layer->displayFrame.left,
                                (int16_t)layer->displayFrame.top,
                                (uint16_t)(layer->displayFrame.right - layer->displayFrame.left),
                                (uint16_t)(layer->displayFrame.bottom - layer->displayFrame.top)};
    NEXUS_Rect clip_position = {(int16_t)(int)layer->sourceCropf.left,
                                (int16_t)(int)layer->sourceCropf.top,
                                (uint16_t)((int)layer->sourceCropf.right - (int)layer->sourceCropf.left),
                                (uint16_t)((int)layer->sourceCropf.bottom - (int)layer->sourceCropf.top)};
    int cur_width;
    int cur_height;
    int format;
    unsigned int stride;
    unsigned int cur_blending_type;

    // sideband layer is handled through the video window directly.
    if (layer->compositionType == HWC_SIDEBAND)
        return;

    // if we do not have a valid handle at this point, we have a problem.
    if (!layer->handle) {
        ALOGE("%s: invalid handle on layer_id %d", __FUNCTION__, layer_id);
        return;
    }

    bcmBuffer = (private_handle_t *)layer->handle;
    PSHARED_DATA pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(bcmBuffer->sharedData);
    cur_width = pSharedData->planes[DEFAULT_PLANE].width;
    cur_height = pSharedData->planes[DEFAULT_PLANE].height;
    format = pSharedData->planes[DEFAULT_PLANE].format;
    stride = (pSharedData->planes[DEFAULT_PLANE].width*pSharedData->planes[DEFAULT_PLANE].bpp + (SURFACE_ALIGNMENT - 1)) & ~(SURFACE_ALIGNMENT - 1);

    switch (layer->blending) {
        case HWC_BLENDING_PREMULT:
            cur_blending_type = BLENDIND_TYPE_SRC_OVER;
        break;
        case HWC_BLENDING_COVERAGE:
            cur_blending_type = BLENDIND_TYPE_SRC_OVER_NON_PREMULTIPLIED;
        break;
        default:
            ALOGI("%s: unsupported blending type, using default", __FUNCTION__);
        case HWC_BLENDING_NONE:
            cur_blending_type = BLENDIND_TYPE_SRC;
        break;
    }

    if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
        goto out;
    }

    ctx->gpx_cli[layer_id].layer_type = layer->compositionType;
    ctx->gpx_cli[layer_id].layer_subtype = NEXUS_SURFACE_COMPOSITOR;
    if (ctx->gpx_cli[layer_id].layer_type == HWC_CURSOR_OVERLAY)
       ctx->gpx_cli[layer_id].layer_subtype = NEXUS_CURSOR;
    ctx->gpx_cli[layer_id].layer_flags = layer->flags;
    ctx->gpx_cli[layer_id].plane_alpha = layer->planeAlpha;

    if (ctx->cursor_layer_id != -1) {
       if (ctx->cursor_layer_id == layer_id) {
          // we are done here, the cursor position is updated via the async callback instead and the
          // surface it uses is reused after recycling from nsc.
          goto out_unlock;
       } else {
          // clean out the previous cursor surface.
          ctx->cursor_layer_id = layer_id;
          if (ctx->cursor_cache.schdl) {
             NEXUS_SurfaceCursor_Destroy(ctx->cursor_cache.schdl);
             ctx->cursor_cache.schdl = NULL;
          }
          if (ctx->cursor_cache.shdl) {
             NEXUS_Surface_Destroy(ctx->cursor_cache.shdl);
             ctx->cursor_cache.shdl = NULL;
          }
       }
    }

    // deal with any change in the geometry/visibility of the layer.
    if ((memcmp((void *)&disp_position, (void *)&ctx->gpx_cli[layer_id].composition.position, sizeof(NEXUS_Rect)) != 0) ||
        (cur_width != ctx->gpx_cli[layer_id].width) ||
        (cur_height != ctx->gpx_cli[layer_id].height) ||
        (!ctx->gpx_cli[layer_id].composition.visible) ||
        (cur_blending_type != ctx->gpx_cli[layer_id].blending_type) ||
        geometry_changed) {

        NxClient_GetSurfaceClientComposition(ctx->gpx_cli[layer_id].ncci.sccid, &ctx->gpx_cli[layer_id].composition);
        ctx->gpx_cli[layer_id].composition.zorder                = (ctx->gpx_cli[layer_id].layer_flags & HWC_IS_CURSOR_LAYER) ? CURSOR_CLIENT_ZORDER : GPX_CLIENT_ZORDER;
        ctx->gpx_cli[layer_id].composition.position.x            = disp_position.x;
        ctx->gpx_cli[layer_id].composition.position.y            = disp_position.y;
        ctx->gpx_cli[layer_id].composition.position.width        = disp_position.width;
        ctx->gpx_cli[layer_id].composition.position.height       = disp_position.height;
        ctx->gpx_cli[layer_id].composition.virtualDisplay.width  = ctx->display_width;
        ctx->gpx_cli[layer_id].composition.virtualDisplay.height = ctx->display_height;
        ctx->gpx_cli[layer_id].blending_type                     = cur_blending_type;
        ctx->gpx_cli[layer_id].composition.colorBlend            = colorBlendingEquation[cur_blending_type];
        ctx->gpx_cli[layer_id].composition.alphaBlend            = alphaBlendingEquation[cur_blending_type];
        ctx->gpx_cli[layer_id].width                             = cur_width;
        ctx->gpx_cli[layer_id].height                            = cur_height;
        ctx->gpx_cli[layer_id].composition.visible               = true;

        if (cur_height && cur_width) {
           int16_t x = ctx->display_width * (cur_width - clip_position.width) / cur_width;
           int16_t y = ctx->display_height * (cur_height - clip_position.height) / cur_height;

           ctx->gpx_cli[layer_id].composition.clipRect.x         = x;
           ctx->gpx_cli[layer_id].composition.clipRect.y         = y;
           ctx->gpx_cli[layer_id].composition.clipRect.width     = ctx->display_width - x;
           ctx->gpx_cli[layer_id].composition.clipRect.height    = ctx->display_height - y;
        }

        rc = NxClient_SetSurfaceClientComposition(ctx->gpx_cli[layer_id].ncci.sccid, &ctx->gpx_cli[layer_id].composition);
        if (rc != NEXUS_SUCCESS) {
            ALOGE("%s: geometry failed on layer %d, err=%d", __FUNCTION__, layer_id, rc);
        }
    }

    // (re)create surface to render this layer.
    if (ctx->gpx_cli[layer_id].composition.visible) {
        int six = hwc_gpx_get_current_surface_locked(&ctx->gpx_cli[layer_id]);
        ctx->gpx_cli[layer_id].skip_set = false;
        if (six != -1 &&
            !geometry_changed &&
            (ctx->gpx_cli[layer_id].slist[six].grhdl == layer->handle)) {
            if (HWC_SURFACE_LIFE_CYCLE_ERROR) ALOGV("%s: skip no change on layer: %d\n", __FUNCTION__, layer_id);
            ctx->gpx_cli[layer_id].skip_set = true;
            goto out_unlock;
        }
        six = hwc_gpx_get_next_surface_locked(&ctx->gpx_cli[layer_id]);
        if (six == -1) {
            goto out_unlock;
        }
        uint8_t *addr;
        void *slock;
        switch (pSharedData->planes[DEFAULT_PLANE].format) {
            case HAL_PIXEL_FORMAT_YV12:
                addr  = (uint8_t *)NEXUS_OffsetToCachedAddr(pSharedData->planes[EXTRA_PLANE].physAddr);
                if (addr == NULL) {
                    ALOGE("%s: extra buffer address NULL: %d\n", __FUNCTION__, layer_id);
                    goto out_unlock;
                }
                stride = pSharedData->planes[EXTRA_PLANE].width * pSharedData->planes[EXTRA_PLANE].bpp;
                ctx->gpx_cli[layer_id].slist[six].shdl = hwc_to_nsc_surface(pSharedData->planes[EXTRA_PLANE].width,
                                                                            pSharedData->planes[EXTRA_PLANE].height,
                                                                            stride,
                                                                            (NEXUS_PixelFormat)pSharedData->planes[EXTRA_PLANE].format,
                                                                            addr);

                if (ctx->gpx_cli[layer_id].slist[six].shdl == NULL) {
                    ctx->gpx_cli[layer_id].slist[six].owner = SURF_OWNER_NO_OWNER;
                    ALOGE("%s: standard surface creation failed: %d, %p, %dx%d, %d\n", __FUNCTION__, layer_id,
                          addr, disp_position.width, disp_position.height, stride);
                    goto out_unlock;
                }

                if (HWC_DO_YV12_CONV) {
                   NEXUS_Surface_Lock(ctx->gpx_cli[layer_id].slist[six].shdl, &slock);
                   NEXUS_Surface_Flush(ctx->gpx_cli[layer_id].slist[six].shdl);
                   rc = yv12_to_422planar(bcmBuffer,
                                          ctx->gpx_cli[layer_id].slist[six].shdl,
                                          ctx->hwc_2dg);
                   if ((rc != NEXUS_SUCCESS) || (0 != hwc_checkpoint(ctx))) {
                       ALOGE("%s: conversion failed: %d\n", __FUNCTION__, layer_id);
                       NEXUS_Surface_Destroy(ctx->gpx_cli[layer_id].slist[six].shdl);
                       ctx->gpx_cli[layer_id].slist[six].owner = SURF_OWNER_NO_OWNER;
                       ctx->gpx_cli[layer_id].slist[six].shdl = NULL;
                       goto out_unlock;
                   }
                }
            break;

            default:
                addr  = (uint8_t *)NEXUS_OffsetToCachedAddr(pSharedData->planes[DEFAULT_PLANE].physAddr);
                if (addr == NULL) {
                    ALOGE("%s: buffer address NULL: %d\n", __FUNCTION__, layer_id);
                    goto out_unlock;
                }
                ctx->gpx_cli[layer_id].slist[six].shdl = hwc_to_nsc_surface(pSharedData->planes[DEFAULT_PLANE].width,
                                                                            pSharedData->planes[DEFAULT_PLANE].height,
                                                                            stride,
                                                                            gralloc_to_nexus_pixel_format(format),
                                                                            addr);
                if (ctx->gpx_cli[layer_id].slist[six].shdl == NULL) {
                    ctx->gpx_cli[layer_id].slist[six].owner = SURF_OWNER_NO_OWNER;
                    ALOGE("%s: standard surface creation failed: %d, %p, %dx%d, %d\n", __FUNCTION__, layer_id,
                          addr, disp_position.width, disp_position.height, stride);
                    goto out_unlock;
                }
            break;
        }

        // surface creation succeeded if we are here.
        ctx->gpx_cli[layer_id].slist[six].owner = SURF_OWNER_HWC_PUSH;
        ctx->gpx_cli[layer_id].slist[six].grhdl = layer->handle;
        hwc_lock_surface((private_handle_t *)layer->handle, layer_id, ctx->gpx_cli[layer_id].slist[six].shdl);
        if (ctx->gpx_cli[layer_id].layer_subtype == NEXUS_CURSOR) {
            NEXUS_SurfaceCursorCreateSettings cursorSettings;
            NEXUS_SurfaceCursorSettings config;

            NEXUS_SurfaceCursor_GetDefaultCreateSettings(&cursorSettings);
            cursorSettings.surface = ctx->gpx_cli[layer_id].slist[six].shdl;
            //ctx->gpx_cli[layer_id].slist[six].schdl = NEXUS_SurfaceCursor_Create(??, &cursorSettings);
            ctx->gpx_cli[layer_id].slist[six].schdl = NULL;
            if (ctx->gpx_cli[layer_id].slist[six].schdl) {
                NEXUS_SurfaceCursor_GetSettings(ctx->gpx_cli[layer_id].slist[six].schdl, &config);
                config.composition.visible               = true;
                config.composition.virtualDisplay.width  = ctx->display_width;
                config.composition.virtualDisplay.height = ctx->display_height;
                config.composition.position.x            = disp_position.x;
                config.composition.position.y            = disp_position.y;
                config.composition.position.width        = disp_position.width;
                config.composition.position.height       = disp_position.height;
                NEXUS_SurfaceCursor_SetSettings(ctx->gpx_cli[layer_id].slist[six].schdl, &config);
            }

            ctx->cursor_cache.shdl = ctx->gpx_cli[layer_id].slist[six].shdl;
            ctx->cursor_cache.schdl = ctx->gpx_cli[layer_id].slist[six].schdl;
        }
    }

out_unlock:
    BKNI_ReleaseMutex(ctx->mutex);
out:
    return;
}

static void hwc_prepare_mm_layer(
    hwc_context_t* ctx,
    hwc_layer_1_t *layer,
    unsigned int gpx_layer_id,
    unsigned int vid_layer_id)
{
    NEXUS_Error rc;
    NEXUS_Rect disp_position = {(int16_t)layer->displayFrame.left,
                                (int16_t)layer->displayFrame.top,
                                (uint16_t)(layer->displayFrame.right - layer->displayFrame.left),
                                (uint16_t)(layer->displayFrame.bottom - layer->displayFrame.top)};
    NEXUS_Rect clip_position = {(int16_t)(int)layer->sourceCropf.left,
                                (int16_t)(int)layer->sourceCropf.top,
                                (uint16_t)((int)layer->sourceCropf.right - (int)layer->sourceCropf.left),
                                (uint16_t)((int)layer->sourceCropf.bottom - (int)layer->sourceCropf.top)};
    int cur_width;
    int cur_height;
    unsigned int stride;
    unsigned int cur_blending_type;
    bool layer_updated = true;

    if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
        goto out;
    }

    // make the gpx corresponding layer non-visible in the layer stack.
    NxClient_GetSurfaceClientComposition(ctx->gpx_cli[gpx_layer_id].ncci.sccid, &ctx->gpx_cli[gpx_layer_id].composition);
    if (ctx->gpx_cli[gpx_layer_id].composition.visible) {
       ctx->gpx_cli[gpx_layer_id].composition.visible = false;
       rc = NxClient_SetSurfaceClientComposition(ctx->gpx_cli[gpx_layer_id].ncci.sccid, &ctx->gpx_cli[gpx_layer_id].composition);
       if (rc != NEXUS_SUCCESS) {
           ALOGE("%s: failed hidding gpx layer %d for video layer %d, err=%d", __FUNCTION__, gpx_layer_id, vid_layer_id, rc);
       }
    }

    ctx->mm_cli[vid_layer_id].root.layer_type = layer->compositionType;
    ctx->mm_cli[vid_layer_id].root.layer_subtype = NEXUS_VIDEO_WINDOW;
    if (ctx->mm_cli[vid_layer_id].root.layer_type == HWC_SIDEBAND) {
       ALOGE("%s: miscategorized sideband layer as video layer %d!", __FUNCTION__, vid_layer_id);
       goto out_unlock;
    }
    ctx->mm_cli[vid_layer_id].root.layer_flags = layer->flags;

    // deal with any change in the geometry/visibility of the layer.
    if (ctx->mm_cli[vid_layer_id].root.ncci.sccid < INVALID_HANDLE) {
       NxClient_GetSurfaceClientComposition(ctx->mm_cli[vid_layer_id].root.ncci.sccid, &ctx->mm_cli[vid_layer_id].root.composition);
       if (!ctx->mm_cli[vid_layer_id].root.composition.visible) {
          ctx->mm_cli[vid_layer_id].root.composition.visible = true;
          layer_updated = true;
       }
       if (memcmp((void *)&disp_position, (void *)&ctx->mm_cli[vid_layer_id].root.composition.position, sizeof(NEXUS_Rect)) != 0) {

           layer_updated = true;
           ctx->mm_cli[vid_layer_id].root.composition.zorder                = MM_CLIENT_ZORDER;
           ctx->mm_cli[vid_layer_id].root.composition.position.x            = disp_position.x;
           ctx->mm_cli[vid_layer_id].root.composition.position.y            = disp_position.y;
           ctx->mm_cli[vid_layer_id].root.composition.position.width        = disp_position.width;
           ctx->mm_cli[vid_layer_id].root.composition.position.height       = disp_position.height;
           ctx->mm_cli[vid_layer_id].root.composition.clipRect.x            = clip_position.x;
           ctx->mm_cli[vid_layer_id].root.composition.clipRect.y            = clip_position.y;
           ctx->mm_cli[vid_layer_id].root.composition.clipRect.width        = clip_position.width;
           ctx->mm_cli[vid_layer_id].root.composition.clipRect.height       = clip_position.height;
           ctx->mm_cli[vid_layer_id].root.composition.virtualDisplay.width  = ctx->display_width;
           ctx->mm_cli[vid_layer_id].root.composition.virtualDisplay.height = ctx->display_height;

           NEXUS_SurfaceClient_GetSettings(ctx->mm_cli[vid_layer_id].svchdl, &ctx->mm_cli[vid_layer_id].settings);
           ctx->mm_cli[vid_layer_id].settings.composition.contentMode           = NEXUS_VideoWindowContentMode_eFull;
           ctx->mm_cli[vid_layer_id].settings.composition.virtualDisplay.width  = ctx->display_width;
           ctx->mm_cli[vid_layer_id].settings.composition.virtualDisplay.height = ctx->display_height;
           ctx->mm_cli[vid_layer_id].settings.composition.position.x            = disp_position.x;
           ctx->mm_cli[vid_layer_id].settings.composition.position.y            = disp_position.y;
           ctx->mm_cli[vid_layer_id].settings.composition.position.width        = disp_position.width;
           ctx->mm_cli[vid_layer_id].settings.composition.position.height       = disp_position.height;
           ctx->mm_cli[vid_layer_id].settings.composition.clipRect.x            = clip_position.x;
           ctx->mm_cli[vid_layer_id].settings.composition.clipRect.y            = clip_position.y;
           ctx->mm_cli[vid_layer_id].settings.composition.clipRect.width        = clip_position.width;
           ctx->mm_cli[vid_layer_id].settings.composition.clipRect.height       = clip_position.height;
           rc = NEXUS_SurfaceClient_SetSettings(ctx->mm_cli[vid_layer_id].svchdl, &ctx->mm_cli[vid_layer_id].settings);
           if (rc != NEXUS_SUCCESS) {
               ALOGE("%s: geometry failed on video layer %d, err=%d", __FUNCTION__, vid_layer_id, rc);
           }
       }
       if (layer_updated) {
          rc = NxClient_SetSurfaceClientComposition(ctx->mm_cli[vid_layer_id].root.ncci.sccid, &ctx->mm_cli[vid_layer_id].root.composition);
          if (rc != NEXUS_SUCCESS) {
              ALOGE("%s: geometry failed on layer %d, err=%d", __FUNCTION__, vid_layer_id, rc);
          }
       }
    } else {
       if (!ctx->mm_cli[vid_layer_id].root.composition.visible) {
          ctx->mm_cli[vid_layer_id].root.composition.visible = true;
          layer_updated = true;
       }

       if (memcmp((void *)&disp_position, (void *)&ctx->mm_cli[vid_layer_id].root.composition.position, sizeof(NEXUS_Rect)) != 0) {

          layer_updated = true;
          ctx->mm_cli[vid_layer_id].root.composition.zorder                = MM_CLIENT_ZORDER;
          ctx->mm_cli[vid_layer_id].root.composition.position.x            = disp_position.x;
          ctx->mm_cli[vid_layer_id].root.composition.position.y            = disp_position.y;
          ctx->mm_cli[vid_layer_id].root.composition.position.width        = disp_position.width;
          ctx->mm_cli[vid_layer_id].root.composition.position.height       = disp_position.height;
          ctx->mm_cli[vid_layer_id].root.composition.clipRect.x            = clip_position.x;
          ctx->mm_cli[vid_layer_id].root.composition.clipRect.y            = clip_position.y;
          ctx->mm_cli[vid_layer_id].root.composition.clipRect.width        = clip_position.width;
          ctx->mm_cli[vid_layer_id].root.composition.clipRect.height       = clip_position.height;
          ctx->mm_cli[vid_layer_id].root.composition.virtualDisplay.width  = ctx->display_width;
          ctx->mm_cli[vid_layer_id].root.composition.virtualDisplay.height = ctx->display_height;

          ctx->mm_cli[vid_layer_id].settings.composition.contentMode           = NEXUS_VideoWindowContentMode_eFull;
          ctx->mm_cli[vid_layer_id].settings.composition.virtualDisplay.width  = ctx->display_width;
          ctx->mm_cli[vid_layer_id].settings.composition.virtualDisplay.height = ctx->display_height;
          ctx->mm_cli[vid_layer_id].settings.composition.position.x            = disp_position.x;
          ctx->mm_cli[vid_layer_id].settings.composition.position.y            = disp_position.y;
          ctx->mm_cli[vid_layer_id].settings.composition.position.width        = disp_position.width;
          ctx->mm_cli[vid_layer_id].settings.composition.position.height       = disp_position.height;
          ctx->mm_cli[vid_layer_id].settings.composition.clipRect.x            = clip_position.x;
          ctx->mm_cli[vid_layer_id].settings.composition.clipRect.y            = clip_position.y;
          ctx->mm_cli[vid_layer_id].settings.composition.clipRect.width        = clip_position.width;
          ctx->mm_cli[vid_layer_id].settings.composition.clipRect.height       = clip_position.height;
       }

       if (layer_updated && ctx->hwc_binder) {
          struct hwc_position frame, clipped;

          frame.x = disp_position.x;
          frame.y = disp_position.y;
          frame.w = disp_position.width;
          frame.h = disp_position.height;
          clipped.x = clip_position.x;
          clipped.y = clip_position.y;
          clipped.w = clip_position.width;
          clipped.h = clip_position.height;

          ctx->hwc_binder->setgeometry(HWC_BINDER_OMX, 0, frame, clipped, MM_CLIENT_ZORDER, 1);
       }
    }

out_unlock:
    BKNI_ReleaseMutex(ctx->mutex);
out:
    return;
}

static void hwc_prepare_sb_layer(
    hwc_context_t* ctx,
    hwc_layer_1_t *layer,
    unsigned int gpx_layer_id,
    unsigned int sb_layer_id)
{
    NEXUS_Error rc;
    NEXUS_Rect disp_position = {(int16_t)layer->displayFrame.left,
                                (int16_t)layer->displayFrame.top,
                                (uint16_t)(layer->displayFrame.right - layer->displayFrame.left),
                                (uint16_t)(layer->displayFrame.bottom - layer->displayFrame.top)};
    NEXUS_Rect clip_position = {(int16_t)(int)layer->sourceCropf.left,
                                (int16_t)(int)layer->sourceCropf.top,
                                (uint16_t)((int)layer->sourceCropf.right - (int)layer->sourceCropf.left),
                                (uint16_t)((int)layer->sourceCropf.bottom - (int)layer->sourceCropf.top)};
    int cur_width;
    int cur_height;
    unsigned int stride;
    unsigned int cur_blending_type;
    bool layer_updated = true;

    if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
        goto out;
    }

    // make the gpx corresponding layer non-visible in the layer stack.
    NxClient_GetSurfaceClientComposition(ctx->gpx_cli[gpx_layer_id].ncci.sccid, &ctx->gpx_cli[gpx_layer_id].composition);
    if (ctx->gpx_cli[gpx_layer_id].composition.visible) {
       ctx->gpx_cli[gpx_layer_id].composition.visible = false;
       rc = NxClient_SetSurfaceClientComposition(ctx->gpx_cli[gpx_layer_id].ncci.sccid, &ctx->gpx_cli[gpx_layer_id].composition);
       if (rc != NEXUS_SUCCESS) {
           ALOGE("%s: failed hidding gpx layer %d for video layer %d, err=%d", __FUNCTION__, gpx_layer_id, sb_layer_id, rc);
       }
    }

    ctx->sb_cli[sb_layer_id].root.layer_type = layer->compositionType;
    ctx->sb_cli[sb_layer_id].root.layer_subtype = NEXUS_VIDEO_SIDEBAND;
    if (ctx->sb_cli[sb_layer_id].root.layer_type == HWC_OVERLAY) {
       ALOGE("%s: miscategorized video layer as sideband layer %d!", __FUNCTION__, sb_layer_id);
       goto out_unlock;
    }
    ctx->sb_cli[sb_layer_id].root.layer_flags = layer->flags;

    // deal with any change in the geometry/visibility of the layer.
    if (ctx->sb_cli[sb_layer_id].root.ncci.sccid < INVALID_HANDLE) {
       NxClient_GetSurfaceClientComposition(ctx->sb_cli[sb_layer_id].root.ncci.sccid, &ctx->sb_cli[sb_layer_id].root.composition);
       if (!ctx->sb_cli[sb_layer_id].root.composition.visible) {
          ctx->sb_cli[sb_layer_id].root.composition.visible = true;
          layer_updated = true;
       }
       if (memcmp((void *)&disp_position, (void *)&ctx->sb_cli[sb_layer_id].root.composition.position, sizeof(NEXUS_Rect)) != 0) {

           layer_updated = true;
           ctx->sb_cli[sb_layer_id].root.composition.zorder                = SB_CLIENT_ZORDER;
           ctx->sb_cli[sb_layer_id].root.composition.position.x            = disp_position.x;
           ctx->sb_cli[sb_layer_id].root.composition.position.y            = disp_position.y;
           ctx->sb_cli[sb_layer_id].root.composition.position.width        = disp_position.width;
           ctx->sb_cli[sb_layer_id].root.composition.position.height       = disp_position.height;
           ctx->sb_cli[sb_layer_id].root.composition.clipRect.x            = clip_position.x;
           ctx->sb_cli[sb_layer_id].root.composition.clipRect.y            = clip_position.y;
           ctx->sb_cli[sb_layer_id].root.composition.clipRect.width        = clip_position.width;
           ctx->sb_cli[sb_layer_id].root.composition.clipRect.height       = clip_position.height;
           ctx->sb_cli[sb_layer_id].root.composition.virtualDisplay.width  = ctx->display_width;
           ctx->sb_cli[sb_layer_id].root.composition.virtualDisplay.height = ctx->display_height;
       }
       if (layer_updated) {
          rc = NxClient_SetSurfaceClientComposition(ctx->sb_cli[sb_layer_id].root.ncci.sccid, &ctx->sb_cli[sb_layer_id].root.composition);
          if (rc != NEXUS_SUCCESS) {
              ALOGE("%s: geometry failed on layer %d, err=%d", __FUNCTION__, sb_layer_id, rc);
          }
       }
    } else {

       if (!ctx->sb_cli[sb_layer_id].root.composition.visible) {
          ctx->sb_cli[sb_layer_id].root.composition.visible = true;
          layer_updated = true;
       }

       if (memcmp((void *)&disp_position, (void *)&ctx->sb_cli[sb_layer_id].root.composition.position, sizeof(NEXUS_Rect)) != 0) {

          layer_updated = true;
          ctx->sb_cli[sb_layer_id].root.composition.visible               = true;
          ctx->sb_cli[sb_layer_id].root.composition.zorder                = SB_CLIENT_ZORDER;
          ctx->sb_cli[sb_layer_id].root.composition.position.x            = disp_position.x;
          ctx->sb_cli[sb_layer_id].root.composition.position.y            = disp_position.y;
          ctx->sb_cli[sb_layer_id].root.composition.position.width        = disp_position.width;
          ctx->sb_cli[sb_layer_id].root.composition.position.height       = disp_position.height;
          ctx->sb_cli[sb_layer_id].root.composition.clipRect.x            = clip_position.x;
          ctx->sb_cli[sb_layer_id].root.composition.clipRect.y            = clip_position.y;
          ctx->sb_cli[sb_layer_id].root.composition.clipRect.width        = clip_position.width;
          ctx->sb_cli[sb_layer_id].root.composition.clipRect.height       = clip_position.height;
          ctx->sb_cli[sb_layer_id].root.composition.virtualDisplay.width  = ctx->display_width;
          ctx->sb_cli[sb_layer_id].root.composition.virtualDisplay.height = ctx->display_height;
       }

       if (layer_updated && ctx->hwc_binder) {
          struct hwc_position frame, clipped;

          frame.x = disp_position.x;
          frame.y = disp_position.y;
          frame.w = disp_position.width;
          frame.h = disp_position.height;
          clipped.x = clip_position.x;
          clipped.y = clip_position.y;
          clipped.w = clip_position.width;
          clipped.h = clip_position.height;

          ctx->hwc_binder->setgeometry(HWC_BINDER_SDB, 0, frame, clipped, SB_CLIENT_ZORDER, 1);
       }
    }

out_unlock:
    BKNI_ReleaseMutex(ctx->mutex);
out:
    return;
}

static void hwc_nsc_prepare_layer(
    hwc_context_t* ctx,
    hwc_layer_1_t *layer,
    int layer_id,
    bool geometry_changed,
    unsigned int *video_layer_id,
    unsigned int *sideband_layer_id)
{
    bool is_sideband = false;
    bool is_yuv = false;

    if (is_video_layer(layer, layer_id, &is_sideband, &is_yuv)) {
        if (!is_sideband) {
            if (is_yuv) {
               hwc_prepare_gpx_layer(ctx, layer, layer_id, geometry_changed);
            }
            else {
               if (*video_layer_id < NSC_MM_CLIENTS_NUMBER) {
                  hwc_prepare_mm_layer(ctx, layer, layer_id, *video_layer_id);
                  (*video_layer_id)++;
               } else {
                  // huh? shouldn't happen really, unless the system is not tuned
                  // properly for the use case, if such, do tune it.
                  ALOGE("%s: droping video layer %d - out of resources (max %d)!\n", __FUNCTION__,
                     *video_layer_id, NSC_MM_CLIENTS_NUMBER);
               }
            }
        } else if (is_sideband) {
            if (*sideband_layer_id < NSC_SB_CLIENTS_NUMBER) {
                hwc_prepare_sb_layer(ctx, layer, layer_id, *sideband_layer_id);
                (*sideband_layer_id)++;
            } else {
                // huh? shouldn't happen really, unless the system is not tuned
                // properly for the use case, if such, do tune it.
                ALOGE("%s: droping sideband layer %d - out of resources (max %d)!\n", __FUNCTION__,
                   *sideband_layer_id, NSC_SB_CLIENTS_NUMBER);
            }
        }
    } else {
        hwc_prepare_gpx_layer(ctx, layer, layer_id, geometry_changed);
    }
}

static void hwc_hide_unused_gpx_layer(hwc_context_t* ctx, int index)
{
    NEXUS_SurfaceComposition composition;
    NEXUS_Error rc;

    if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
        goto out;
    }

    NxClient_GetSurfaceClientComposition(ctx->gpx_cli[index].ncci.sccid, &composition);
    if (composition.visible) {
       composition.visible = false;
       rc = NxClient_SetSurfaceClientComposition(ctx->gpx_cli[index].ncci.sccid, &composition);
       ctx->gpx_cli[index].composition.visible = false;
       if (rc != NEXUS_SUCCESS) {
           ALOGE("%s: failed hiding layer %d, err=%d", __FUNCTION__, index, rc);
       }
    }

    BKNI_ReleaseMutex(ctx->mutex);

out:
    return;
}

static void nx_client_hide_unused_mm_layer(hwc_context_t* ctx, int index)
{
    NEXUS_SurfaceComposition composition;
    NEXUS_Error rc;

    if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
        goto out;
    }

    if (ctx->mm_cli[index].root.ncci.sccid < INVALID_HANDLE) {
       NxClient_GetSurfaceClientComposition(ctx->mm_cli[index].root.ncci.sccid, &composition);
       if (composition.visible) {
          composition.visible = false;
          rc = NxClient_SetSurfaceClientComposition(ctx->mm_cli[index].root.ncci.sccid, &composition);
          if (rc != NEXUS_SUCCESS) {
              ALOGE("%s: failed hiding layer %d, err=%d", __FUNCTION__, index, rc);
          }
       }
    }

    BKNI_ReleaseMutex(ctx->mutex);
out:
    return;
}

static void nx_client_hide_unused_sb_layer(hwc_context_t* ctx, int index)
{
    NEXUS_SurfaceComposition composition;
    NEXUS_Error rc;

    if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
        goto out;
    }

    if (ctx->sb_cli[index].root.ncci.sccid < INVALID_HANDLE) {
       NxClient_GetSurfaceClientComposition(ctx->sb_cli[index].root.ncci.sccid, &composition);
       if (composition.visible) {
          composition.visible = false;
          rc = NxClient_SetSurfaceClientComposition(ctx->sb_cli[index].root.ncci.sccid, &composition);
          if (rc != NEXUS_SUCCESS) {
              ALOGE("%s: failed hiding layer %d, err=%d", __FUNCTION__, index, rc);
          }
       }
    } else {
       ctx->sb_cli[index].root.composition.visible = false;
    }

    BKNI_ReleaseMutex(ctx->mutex);

out:
    return;
}

static void hwc_hide_unused_mm_layers(hwc_context_t* ctx)
{
    for (int i = 0; i < NSC_MM_CLIENTS_NUMBER; i++)
    {
       nx_client_hide_unused_mm_layer(ctx, i);
    }
}

static void hwc_hide_unused_sb_layers(hwc_context_t* ctx)
{
    for (int i = 0; i < NSC_SB_CLIENTS_NUMBER; i++)
    {
       nx_client_hide_unused_sb_layer(ctx, i);
    }
}

static void hwc_binder_advertise_video_surface(hwc_context_t* ctx)
{
    if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
        goto out;
    }

    // TODO: for the time being, only advertize one.
    if (ctx->nsc_video_changed && ctx->hwc_binder) {
       ctx->hwc_binder->setvideo(0, ctx->mm_cli[0].root.ncci.sccid,
                                 ctx->display_width, ctx->display_height);
       ctx->nsc_video_changed = false;
    }

    if (ctx->nsc_sideband_changed && ctx->hwc_binder) {
       ctx->hwc_binder->setsideband(0, ctx->sb_cli[0].root.ncci.sccid,
                                    ctx->display_width, ctx->display_height);
       ctx->nsc_sideband_changed = false;
    }

    BKNI_ReleaseMutex(ctx->mutex);

out:
    return;
}

static void hwc_checkpoint_callback(void *pParam, int param2)
{
    hwc_context_t *dev = (hwc_context_t *)pParam;
    (void)param2;
    BKNI_SetEvent(dev->checkpoint_event);
}

static int hwc_checkpoint(hwc_context_t *dev)
{
    NEXUS_Error rc;

    rc = NEXUS_Graphics2D_Checkpoint(dev->hwc_2dg, NULL);
    switch ( rc )
    {
    case NEXUS_SUCCESS:
      break;
    case NEXUS_GRAPHICS2D_QUEUED:
      rc = BKNI_WaitForEvent(dev->checkpoint_event, HWC_CHECKPOINT_TIMEOUT);
      if ( rc )
      {
          ALOGW("Checkpoint Timeout");
          return -1;
      }
      break;
    default:
      ALOGE("Checkpoint Error");
      return -1;
    }

    return 0;
}
