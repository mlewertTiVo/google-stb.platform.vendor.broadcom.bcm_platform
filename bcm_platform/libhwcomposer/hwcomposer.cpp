/*
 * Copyright (C) 2014, 2015 Broadcom Canada Ltd.
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
#include "nxclient_config.h"
#include "bfifo.h"

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

#include "hwcutils.h"

#include "sync/sync.h"
// deliberate use of sw_sync as we do not have other sync
// primitive at this point.  just be careful when using those.
#include "sw_sync.h"

using namespace android;

#define INVALID_HANDLE               0xBAADCAFE
#define LAST_PING_FRAME_ID_INVALID   0xBAADCAFE
#define INVALID_FENCE                -1

// cursor surface is behaving slightly differently than
// other gpx (or mm) ones and may lead to a lot of false
// alarm logs.
#define HWC_CURSOR_SURFACE_SUPPORTED 0

#define HWC_SB_NO_ALLOC_SURF_CLI     1
#define HWC_MM_NO_ALLOC_SURF_CLI     1

#define HWC_REFRESH_HACK             0

#define HWC_NUM_DISP_BUFFERS         3

#define NSC_GPX_CLIENTS_NUMBER       11 /* graphics client layers; typically no
                                         * more than 3 are needed at any time, though
                                         * it has been seen up to 7 in some scenario,
                                         * so maximum is 10+1 (1 for the floating
                                         * HWC_FRAMEBUFFER_TARGET). */
#define NSC_MM_CLIENTS_NUMBER        3  /* multimedia client layers; typically no
                                         * more than 2 are needed at any time. */
#define NSC_SB_CLIENTS_NUMBER        2  /* sideband client layers; typically no
                                         * more than 1 are needed at any time. */

#define NSC_CLIENTS_NUMBER           (NSC_GPX_CLIENTS_NUMBER+NSC_MM_CLIENTS_NUMBER+NSC_SB_CLIENTS_NUMBER)
#define HWC_VD_CLIENTS_NUMBER        NSC_GPX_CLIENTS_NUMBER

#define VSYNC_CLIENT_REFRESH         16666667
#define DISPLAY_CONFIG_DEFAULT       0

#define BOTTOM_CLIENT_ZORDER         0
#define MM_CLIENT_ZORDER             (BOTTOM_CLIENT_ZORDER+1)
#define SB_CLIENT_ZORDER             (MM_CLIENT_ZORDER)
#define SUBTITLE_CLIENT_ZORDER       (MM_CLIENT_ZORDER+1)
#define GPX_CLIENT_ZORDER            (SUBTITLE_CLIENT_ZORDER+1)
#define CURSOR_CLIENT_ZORDER         (GPX_CLIENT_ZORDER+1)

#define DUMP_BUFFER_SIZE             1024

#define DISPLAY_SUPPORTED            2
#define HWC_PRIMARY_IX               0
#define HWC_VIRTUAL_IX               1
#define NEXUS_DISPLAY_OBJECTS        4
#define HWC_SET_COMP_THRESHOLD       2

/* note: matching other parts of the integration, we
 *       want to default product resolution to 1080p.
 */
#define GRAPHICS_RES_WIDTH_DEFAULT  1920
#define GRAPHICS_RES_HEIGHT_DEFAULT 1080
#define GRAPHICS_RES_WIDTH_PROP     "ro.graphics_resolution.width"
#define GRAPHICS_RES_HEIGHT_PROP    "ro.graphics_resolution.height"

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

#define HWC_DEFAULT_DISABLED         "0"
#define HWC_DEFAULT_ENABLED          "1"

#define HWC_GLES_VIRTUAL_PROP        "ro.hwc.gles.virtual"
#define HWC_DUMP_LAYER_PROP          "ro.hwc.dump.layer"
#define HWC_DUMP_VSYNC_PROP          "ro.hwc.dump.vsync"
#define HWC_NSC_COPY_PROP            "ro.hwc.nsc.copy"
#define HWC_TRACK_BUFFER_PROP        "ro.hwc.track.buffer"
#define HWC_DUMP_VIRT_PROP           "ro.hwc.dump.virt"
#define HWC_DUMP_MMA_OPS_PROP        "ro.hwc.dump.mma"
#define HWC_DUMP_FENCE_PROP          "ro.hwc.dump.fence"
#define HWC_SKIP_DUP_PROP            "ro.hwc.skip.dup"
#define HWC_WITH_FENCE_PROP          "ro.v3d.fence.expose"
#define HWC_USES_MMA_PROP            "ro.nx.mma"
#define HWC_TRACK_COMP_TIME          "ro.hwc.track.comptime"
#define HWC_TRACK_COMP_CHATTY        "ro.hwc.track.chatty"

#define HWC_CHECKPOINT_TIMEOUT       (5000)

enum {
    NEXUS_SURFACE_COMPOSITOR = 0,
    NEXUS_CURSOR,
    NEXUS_VIDEO_WINDOW,
    NEXUS_VIDEO_SIDEBAND,
};

enum {
    NEXUS_CLIENT_GPX = 0,
    NEXUS_CLIENT_MM,
    NEXUS_CLIENT_SB,
    NEXUS_CLIENT_VSYNC,
    NEXUS_CLIENT_VD,
};

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

typedef struct {
    int type;
    void *parent;

} COMMON_CLIENT_INFO;

typedef struct {
    buffer_handle_t layerhdl;
    buffer_handle_t grhdl;
    unsigned long long comp_ix;

} GPX_CLIENT_SURFACE_INFO;

typedef struct {
    COMMON_CLIENT_INFO ncci;
    NEXUS_SurfaceComposition composition;
    GPX_CLIENT_SURFACE_INFO last;
    unsigned int blending_type;
    int layer_type;
    int layer_subtype;
    int layer_flags;
    int layer_id;
    int plane_alpha;
    bool skip_set;

} GPX_CLIENT_INFO;

typedef struct {
    GPX_CLIENT_INFO root;
    NEXUS_SurfaceClientHandle svchdl;
    NEXUS_SurfaceClientSettings settings;
    long long unsigned last_ping_frame_id;
    int id;
} MM_CLIENT_INFO;

typedef struct {
    GPX_CLIENT_INFO root;
    NEXUS_SurfaceClientSettings settings;
    int id;
} SB_CLIENT_INFO;

typedef struct {
    COMMON_CLIENT_INFO ncci;
    BKNI_EventHandle vsync_event;
    int layer_type;
    int layer_subtype;
    nsecs_t refresh;

} VSYNC_CLIENT_INFO;

typedef struct {
    COMMON_CLIENT_INFO ncci;
    buffer_handle_t grhdl;
    NEXUS_SurfaceComposition composition;

} VD_CLIENT_INFO;

typedef struct {
    NEXUS_SurfaceClientHandle schdl;
    NEXUS_SurfaceCompositorClientId sccid;
    BFIFO_HEAD(DisplayFifo, NEXUS_SurfaceHandle) display_fifo;
    NEXUS_SurfaceHandle display_buffers[HWC_NUM_DISP_BUFFERS];

} DISPLAY_CLIENT_INFO;

extern "C" bool check_frame_transparency(void *data, size_t width, size_t height, size_t stride);
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

struct hwc_display_stats {
   unsigned long long prepare_call;
   unsigned long long prepare_skipped;
   unsigned long long set_call;
   unsigned long long set_skipped;
   unsigned long long composed;
};

struct hwc_time_track_stats {
   unsigned long long tracked;
   unsigned long long ontime;
   unsigned long long slow;
   long double fastest;
   long double slowest;
   long double cumul;
};

struct hwc_display_cfg {
   int width;
   int height;
};

struct hwc_work_item {
   struct hwc_work_item *next;
   unsigned long long comp_ix;
   int64_t tick_queued;
   NEXUS_SurfaceComposition comp[NSC_GPX_CLIENTS_NUMBER];
   hwc_display_contents_1_t content;
};

struct hwc_context_t {
    hwc_composer_device_1_t device;

    struct hwc_display_stats stats[DISPLAY_SUPPORTED];
    struct hwc_display_cfg cfg[DISPLAY_SUPPORTED];
    struct hwc_time_track_stats time_track[DISPLAY_SUPPORTED];

    /* our private state goes below here */
    NexusIPCClientBase *pIpcClient;
    NexusClientContext *pNexusClientContext;
    NxClient_AllocResults nxAllocResults;
    BKNI_MutexHandle dev_mutex;

    hwc_procs_t const* procs;

    bool vsync_callback_enabled;
    bool vsync_callback_initialized;
    BKNI_MutexHandle vsync_callback_enabled_mutex;
    pthread_t vsync_callback_thread;
    int vsync_thread_run;
    NEXUS_DisplayHandle display_handle;

    GPX_CLIENT_INFO gpx_cli[NSC_GPX_CLIENTS_NUMBER];
    MM_CLIENT_INFO mm_cli[NSC_MM_CLIENTS_NUMBER];
    SB_CLIENT_INFO sb_cli[NSC_SB_CLIENTS_NUMBER];
    BKNI_MutexHandle mutex;
    VSYNC_CLIENT_INFO syn_cli;
    bool nsc_video_changed;
    bool nsc_sideband_changed;
    VD_CLIENT_INFO vd_cli[HWC_VD_CLIENTS_NUMBER];
    DISPLAY_CLIENT_INFO disp_cli[DISPLAY_SUPPORTED];
    BKNI_EventHandle recycle_event;

    struct hwc_position overscan_position;

    bool display_gles_fallback;
    bool display_gles_always;
    bool display_gles_virtual;
    bool needs_fb_target;

    BKNI_MutexHandle power_mutex;
    int power_mode;

    HwcBinder_wrap *hwc_binder;

    NEXUS_Graphics2DHandle hwc_g2d;
    BKNI_MutexHandle g2d_mutex;
    NEXUS_Graphics2DCapabilities gfxCaps;
    BKNI_EventHandle checkpoint_event;

    BKNI_EventHandle composer_event[DISPLAY_SUPPORTED];
    BKNI_EventHandle composer_event_sync[DISPLAY_SUPPORTED];
    BKNI_EventHandle composer_event_forced[DISPLAY_SUPPORTED];
    pthread_t composer_thread[DISPLAY_SUPPORTED];
    int composer_thread_run[DISPLAY_SUPPORTED];
    int composer_ret_timeline[DISPLAY_SUPPORTED];
    struct hwc_work_item *composer_work_list[DISPLAY_SUPPORTED];

    bool display_dump_layer;
    bool display_dump_vsync;
    bool nsc_copy;
    bool track_buffer;
    bool skip_dup;
    bool display_dump_virt;
    bool fence_support;

    int hwc_with_mma;
    bool dump_mma;
    bool dump_fence;

    bool track_comp_time;
    bool track_comp_chatty;

    bool alpha_hole_background;
};

static void *hwc_compose_task_primary(void *argv);
static void *hwc_compose_task_virtual(void *argv);
static int hwc_compose_primary(struct hwc_context_t *ctx, hwc_display_contents_1_t* list,
   int *overlay_seen, int *fb_target_seen);
static int hwc_compose_virtual(struct hwc_context_t *ctx, hwc_display_contents_1_t* list,
   int *overlay_seen, int *fb_target_seen);

typedef void * (*HWC_COMPOSE_FPTR)(void *argv);
static HWC_COMPOSE_FPTR hwc_compose_task_item[DISPLAY_SUPPORTED] = {
   &hwc_compose_task_primary,
   &hwc_compose_task_virtual,
};

static void hwc_device_cleanup(struct hwc_context_t* ctx);
static int hwc_device_open(const struct hw_module_t* module,
   const char* name, struct hw_device_t** device);

static struct hw_module_methods_t hwc_module_methods = {
   open: hwc_device_open
};

static void hwc_nsc_recycled_cb(void *context, int param);

static void hw_vsync_cb(void *context, int param);

static void hwc_hide_unused_gpx_layer(struct hwc_context_t* dev, int index);
static void hwc_hide_unused_mm_layers(struct hwc_context_t* dev);
static void hwc_hide_unused_sb_layers(struct hwc_context_t* dev);

static void hwc_nsc_prepare_layer(struct hwc_context_t* dev, hwc_layer_1_t *layer,
   int layer_id, bool geometry_changed,
   unsigned int *video_layer_id, unsigned int *sideband_layer_id, unsigned int *skip_set);

static void hwc_binder_advertise_video_surface(struct hwc_context_t* dev);

static void hwc_checkpoint_callback(void *pParam, int param2);
static int hwc_checkpoint(struct hwc_context_t *ctx);
static int hwc_checkpoint_locked(struct hwc_context_t *ctx);

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
   "VDC", // NEXUS_CLIENT_VD
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
};

static int64_t hwc_tick(void)
{
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)(t.tv_sec) * 1000000000LL + t.tv_nsec;
}

static unsigned int hwc_android_blend_to_nsc_blend(unsigned int android_blend)
{
   switch (android_blend) {
   case HWC_BLENDING_PREMULT: return BLENDIND_TYPE_SRC_OVER;
   case HWC_BLENDING_COVERAGE: return BLENDIND_TYPE_SRC_OVER_NON_PREMULTIPLIED;
   default: /* fall-through. */
   case HWC_BLENDING_NONE: return BLENDIND_TYPE_SRC;
   }
}

static void hwc_get_buffer_sizes(PSHARED_DATA pSharedData, int *cur_width, int *cur_height)
{
   if (cur_width == NULL || cur_height == NULL || pSharedData == NULL) {
      return;
   }

   switch (pSharedData->planes[DEFAULT_PLANE].format) {
      case HAL_PIXEL_FORMAT_YV12:
         if (pSharedData->planes[EXTRA_PLANE].physAddr) {
            *cur_width  = pSharedData->planes[EXTRA_PLANE].width;
            *cur_height = pSharedData->planes[EXTRA_PLANE].height;
         } else if (pSharedData->planes[GL_PLANE].physAddr) {
            *cur_width  = pSharedData->planes[GL_PLANE].width;
            *cur_height = pSharedData->planes[GL_PLANE].height;
         } else if (pSharedData->planes[DEFAULT_PLANE].physAddr) {
            *cur_width  = pSharedData->planes[DEFAULT_PLANE].width;
            *cur_height = pSharedData->planes[DEFAULT_PLANE].height;
         }
      break;
      default:
         *cur_width  = pSharedData->planes[DEFAULT_PLANE].width;
         *cur_height = pSharedData->planes[DEFAULT_PLANE].height;
      break;
   }
}

static NEXUS_PixelFormat gralloc_to_nexus_pixel_format(int format)
{
   switch (format) {
      case HAL_PIXEL_FORMAT_RGBA_8888: return NEXUS_PixelFormat_eA8_B8_G8_R8;
      case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED: /* fall through. */
      case HAL_PIXEL_FORMAT_RGBX_8888: return NEXUS_PixelFormat_eX8_B8_G8_R8;
      case HAL_PIXEL_FORMAT_RGB_888:   return NEXUS_PixelFormat_eX8_B8_G8_R8;
      case HAL_PIXEL_FORMAT_RGB_565:   return NEXUS_PixelFormat_eR5_G6_B5;
      default:                         break;
   }

   ALOGE("%s: unsupported format %d", __FUNCTION__, format);
   return NEXUS_PixelFormat_eUnknown;
}

static int hwc_mem_lock_phys(struct hwc_context_t *ctx, unsigned handle, NEXUS_Addr *paddr) {
   NEXUS_Error rc;
   if (handle && ctx->hwc_with_mma) {
      NEXUS_MemoryBlockHandle block_handle = (NEXUS_MemoryBlockHandle) handle;
      rc = NEXUS_MemoryBlock_LockOffset(block_handle, paddr);

      if (ctx->dump_mma) {
         ALOGI("mma-lock-phys: %p -> %p", handle, *paddr);
      } else if (rc) {
         ALOGW("mma-lock-phys: %p -> invalid!?!", handle);
      }
   } else if (handle) {
      *paddr = (NEXUS_Addr)handle;
   } else {
      *paddr = (NEXUS_Addr)NULL;
   }
   return 0;
}

static int hwc_mem_unlock_phys(struct hwc_context_t *ctx, unsigned handle) {
   if (handle && ctx->hwc_with_mma) {
      NEXUS_MemoryBlockHandle block_handle = (NEXUS_MemoryBlockHandle) handle;
      NEXUS_MemoryBlock_UnlockOffset(block_handle);
   }
   return 0;
}

static int hwc_mem_lock(struct hwc_context_t *ctx, unsigned handle, void **addr, bool lock) {
   NEXUS_Error rc;
   if (handle && ctx->hwc_with_mma) {
      if (lock) {
         NEXUS_MemoryBlockHandle block_handle = (NEXUS_MemoryBlockHandle) handle;
         rc = NEXUS_MemoryBlock_Lock(block_handle, addr);

         if (ctx->dump_mma) {
            ALOGI("mma-lock: %p -> %p", handle, *addr);
         } else if (rc) {
            ALOGW("mma-lock: %p -> invalid!?!", handle);
         }
      }
   } else if (handle) {
      *addr = NEXUS_OffsetToCachedAddr((NEXUS_Addr)handle);
   } else {
      *addr = NULL;
   }
   return 0;
}

static int hwc_mem_unlock(struct hwc_context_t *ctx, unsigned handle, bool lock) {
   if (handle && ctx->hwc_with_mma) {
      if (lock) {
         NEXUS_MemoryBlockHandle block_handle = (NEXUS_MemoryBlockHandle) handle;
         NEXUS_MemoryBlock_Unlock(block_handle);

         if (ctx->dump_mma) {
            ALOGI("mma-unlock: %p", handle);
         }
      }
   }
   return 0;
}

static int dump_vd_layer_data(char *start, int capacity, int index, VD_CLIENT_INFO *client)
{
    int write = -1;
    int total_write = 0;
    int local_capacity = capacity;
    int offset = 0;
    void *pAddr;

    write = snprintf(start, local_capacity,
        "\t[%s]:[%s]:[%d]::z:%d::cp:{%d,%d,%d,%d}::cl:{%d,%d,%d,%d}::cv:{%d,%d}\n",
        client->composition.visible ? "LIVE" : "HIDE",
        nsc_cli_type[client->ncci.type],
        index,
        client->composition.zorder,
        client->composition.position.x,
        client->composition.position.y,
        client->composition.position.width,
        client->composition.position.height,
        client->composition.clipRect.x,
        client->composition.clipRect.y,
        client->composition.clipRect.width,
        client->composition.clipRect.height,
        client->composition.virtualDisplay.width,
        client->composition.virtualDisplay.height);

    if (write > 0) {
        local_capacity = (local_capacity > write) ? (local_capacity - write) : 0;
        offset += write;
        total_write += write;
    }

    if (client->grhdl) {
       private_handle_t *gr_buffer = (private_handle_t *)client->grhdl;
       hwc_mem_lock((struct hwc_context_t*)client->ncci.parent, (unsigned)gr_buffer->sharedData, &pAddr, true);
       PSHARED_DATA pSharedData = (PSHARED_DATA) pAddr;

       write = snprintf(start + offset, local_capacity,
          "\t\t\t[%p]::flg:0x%x::pid:%d::ogl:{s:%d,f:%d,sz:%d}::use:0x%x\n",
          client->grhdl,
          gr_buffer->flags,
          gr_buffer->pid,
          gr_buffer->oglStride,
          gr_buffer->oglFormat,
          gr_buffer->oglSize,
          gr_buffer->usage);

       if (write > 0) {
           local_capacity = (local_capacity > write) ? (local_capacity - write) : 0;
           offset += write;
           total_write += write;
       }

       write = snprintf(start + offset, local_capacity,
          "\t\t\t[%p]::default:{f:0x%x,bpp:%d,{%d,%d},0x%x,sz:%d}\n",
          client->grhdl,
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
          client->grhdl,
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

       write = snprintf(start + offset, local_capacity,
          "\t\t\t[%p]::gl:{f:0x%x,bpp:%d,{%d,%d},0x%x,sz:%d}\n",
          client->grhdl,
          pSharedData->planes[GL_PLANE].format,
          pSharedData->planes[GL_PLANE].bpp,
          pSharedData->planes[GL_PLANE].width,
          pSharedData->planes[GL_PLANE].height,
          pSharedData->planes[GL_PLANE].physAddr,
          pSharedData->planes[GL_PLANE].size);

       if (write > 0) {
           local_capacity = (local_capacity > write) ? (local_capacity - write) : 0;
           offset += write;
           total_write += write;
       }

       hwc_mem_unlock((struct hwc_context_t*)client->ncci.parent, (unsigned)gr_buffer->sharedData, true);
    }

    return total_write;
}

static int dump_gpx_layer_data(char *start, int capacity, int index, GPX_CLIENT_INFO *client)
{
    int write = -1;
    int total_write = 0;
    int local_capacity = capacity;
    int offset = 0;
    void *pAddr;

    write = snprintf(start, local_capacity,
        "\t[%s]:[%s]:[%d:%d]:[%s]:[%s]::f:%x::z:%d::cp:{%d,%d,%d,%d}::cl:{%d,%d,%d,%d}::cv:{%d,%d}::b:%d::pa:%d\n",
        client->composition.visible ? "LIVE" : "HIDE",
        nsc_cli_type[client->ncci.type],
        client->layer_id,
        index,
        hwc_layer_type[client->layer_type],
        nsc_layer_type[client->layer_subtype],
        client->layer_flags,
        client->composition.zorder,
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
        client->plane_alpha);

    if (write > 0) {
        local_capacity = (local_capacity > write) ? (local_capacity - write) : 0;
        offset += write;
        total_write += write;
    }

    write = snprintf(start + offset, local_capacity,
        "\t\t[%d:%d]::hdl:%p::grhdl:%p::comp:%llu\n",
        client->layer_id,
        index,
        client->last.layerhdl,
        client->last.grhdl,
        client->last.comp_ix);

    if (write > 0) {
        local_capacity = (local_capacity > write) ? (local_capacity - write) : 0;
        offset += write;
        total_write += write;
    }

    if (client->last.grhdl) {
       private_handle_t *gr_buffer = (private_handle_t *)client->last.grhdl;
       hwc_mem_lock((struct hwc_context_t*)client->ncci.parent, (unsigned)gr_buffer->sharedData, &pAddr, true);
       PSHARED_DATA pSharedData = (PSHARED_DATA) pAddr;

       write = snprintf(start + offset, local_capacity,
           "\t\t\t[%p]::flg:0x%x::pid:%d::ogl:{s:%d,f:%d,sz:%d}::use:0x%x\n",
           client->last.grhdl,
           gr_buffer->flags,
           gr_buffer->pid,
           gr_buffer->oglStride,
           gr_buffer->oglFormat,
           gr_buffer->oglSize,
           gr_buffer->usage);

       if (write > 0) {
           local_capacity = (local_capacity > write) ? (local_capacity - write) : 0;
           offset += write;
           total_write += write;
       }

       write = snprintf(start + offset, local_capacity,
           "\t\t\t[%p]::default:{f:0x%x,bpp:%d,{%d,%d},0x%x,sz:%d}\n",
           client->last.grhdl,
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
           client->last.grhdl,
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

       write = snprintf(start + offset, local_capacity,
           "\t\t\t[%p]::gl:{f:0x%x,bpp:%d,{%d,%d},0x%x,sz:%d}\n",
           client->last.grhdl,
           pSharedData->planes[GL_PLANE].format,
           pSharedData->planes[GL_PLANE].bpp,
           pSharedData->planes[GL_PLANE].width,
           pSharedData->planes[GL_PLANE].height,
           pSharedData->planes[GL_PLANE].physAddr,
           pSharedData->planes[GL_PLANE].size);

       if (write > 0) {
           local_capacity = (local_capacity > write) ? (local_capacity - write) : 0;
           offset += write;
           total_write += write;
       }

       hwc_mem_unlock((struct hwc_context_t*)client->ncci.parent, (unsigned)gr_buffer->sharedData, true);
    }

    return total_write;
}

static int dump_mm_layer_data(char *start, int capacity, int index, MM_CLIENT_INFO *client)
{
    int write = -1;

    write = snprintf(start, capacity,
        "\t[%s]:[%s]:[%d:%d]:[%s]:[%s]::z:%d::pcp:{%d,%d,%d,%d}::pcv:{%d,%d}::cm:%d::cp:{%d,%d,%d,%d}::cl:{%d,%d,%d,%d}::cv:{%d,%d}::svc:%p\n",
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
        client->svchdl);

    return write;
}

static int dump_sb_layer_data(char *start, int capacity, int index, SB_CLIENT_INFO *client)
{
    int write = -1;

    write = snprintf(start, capacity,
        "\t[%s]:[%s]:[%d:%d]:[%s]:[%s]::z:%d::pcp:{%d,%d,%d,%d}::pcv:{%d,%d}::cm:%d::cp:{%d,%d,%d,%d}::cl:{%d,%d,%d,%d}::cv:{%d,%d}\n",
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
        client->settings.composition.virtualDisplay.height);

    return write;
}

static void hwc_device_dump(struct hwc_composer_device_1* dev, char *buff, int buff_len)
{
    /* surface flinger gives us a single page equivalent (4096 bytes). */

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
       write = snprintf(buff + index, capacity, "\nDISPLAY:\n");
       if (write > 0) {
          capacity = (capacity > write) ? (capacity - write) : 0;
          index += write;
       }
    }
    if (capacity) {
       write = snprintf(buff + index, capacity, "\tprepare:{%llu,%llu}::set:{%llu,%llu}::comp:%llu::sync-ret:%d\n",
           ctx->stats[HWC_PRIMARY_IX].prepare_call,
           ctx->stats[HWC_PRIMARY_IX].prepare_skipped,
           ctx->stats[HWC_PRIMARY_IX].set_call,
           ctx->stats[HWC_PRIMARY_IX].set_skipped,
           ctx->stats[HWC_PRIMARY_IX].composed,
           ctx->composer_ret_timeline[HWC_PRIMARY_IX]);
       if (write > 0) {
           capacity = (capacity > write) ? (capacity - write) : 0;
           index += write;
       }
       write = snprintf(buff + index, capacity, "\tipc:%p::ncc:%p::vscb:%s::d:{%d,%d}::pm:%s::nsc-copy:%s::oscan:{%d,%d:%d,%d}\n",
           ctx->pIpcClient,
           ctx->pNexusClientContext,
           ctx->vsync_callback_enabled ? "enabled" : "disabled",
           ctx->cfg[HWC_PRIMARY_IX].width,
           ctx->cfg[HWC_PRIMARY_IX].height,
           hwc_power_mode[ctx->power_mode],
           ctx->nsc_copy ? "oui" : "non",
           ctx->overscan_position.x,
           ctx->overscan_position.y,
           ctx->overscan_position.w,
           ctx->overscan_position.h);
       if (write > 0) {
           capacity = (capacity > write) ? (capacity - write) : 0;
           index += write;
       }
       write = snprintf(buff + index, capacity, "\tcomp:%llu:{%llu,%llu}::slow:%.3f::fast:%.3f::avg:%.3f\n",
           ctx->time_track[HWC_PRIMARY_IX].tracked,
           ctx->time_track[HWC_PRIMARY_IX].slow,
           ctx->time_track[HWC_PRIMARY_IX].ontime,
           ctx->time_track[HWC_PRIMARY_IX].slowest,
           ctx->time_track[HWC_PRIMARY_IX].fastest,
           ctx->time_track[HWC_PRIMARY_IX].cumul/ctx->time_track[HWC_PRIMARY_IX].tracked);
       if (write > 0) {
           capacity = (capacity > write) ? (capacity - write) : 0;
           index += write;
       }
    }
    if (capacity) {
       write = snprintf(buff + index, capacity, "\nVIRTUAL DISPLAY:\n");
       if (write > 0) {
          capacity = (capacity > write) ? (capacity - write) : 0;
          index += write;
       }
    }
    if (capacity) {
       write = snprintf(buff + index, capacity, "\tprepare:{%llu,%llu}::set:{%llu,%llu}::comp:%llu::d:{%d,%d}::sync-ret:%d\n",
           ctx->stats[HWC_VIRTUAL_IX].prepare_call,
           ctx->stats[HWC_VIRTUAL_IX].prepare_skipped,
           ctx->stats[HWC_VIRTUAL_IX].set_call,
           ctx->stats[HWC_VIRTUAL_IX].set_skipped,
           ctx->stats[HWC_VIRTUAL_IX].composed,
           ctx->cfg[HWC_VIRTUAL_IX].width,
           ctx->cfg[HWC_VIRTUAL_IX].height,
           ctx->composer_ret_timeline[HWC_VIRTUAL_IX]);
       if (write > 0) {
           capacity = (capacity > write) ? (capacity - write) : 0;
           index += write;
       }
       write = snprintf(buff + index, capacity, "\tcomp:%llu:{%llu,%llu}::slow:%.3f::fast:%.3f::avg:%.3f\n",
           ctx->time_track[HWC_VIRTUAL_IX].tracked,
           ctx->time_track[HWC_VIRTUAL_IX].slow,
           ctx->time_track[HWC_VIRTUAL_IX].ontime,
           ctx->time_track[HWC_VIRTUAL_IX].slowest,
           ctx->time_track[HWC_VIRTUAL_IX].fastest,
           ctx->time_track[HWC_VIRTUAL_IX].cumul/ctx->time_track[HWC_VIRTUAL_IX].tracked);
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

    for (int i = 0; i < NSC_GPX_CLIENTS_NUMBER; i++) {
        write = 0;
        if (ctx->gpx_cli[i].composition.visible && capacity) {
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

    for (int i = 0; i < NSC_MM_CLIENTS_NUMBER; i++) {
        write = 0;
        if (ctx->mm_cli[i].root.composition.visible && capacity) {
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

    for (int i = 0; i < NSC_SB_CLIENTS_NUMBER; i++) {
        write = 0;
        if (ctx->sb_cli[i].root.composition.visible && capacity) {
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

    if (capacity) {
       write = snprintf(buff + index, capacity, "\n\n");
    }

    for (int i = 0; i < HWC_VD_CLIENTS_NUMBER; i++) {
        write = 0;
        if (ctx->vd_cli[i].composition.visible && capacity) {
           write = dump_vd_layer_data(buff + index, capacity, i, &ctx->vd_cli[i]);
           if (write > 0) {
              capacity = (capacity > write) ? (capacity - write) : 0;
              index += write;
           }
        }

        if (!capacity) {
           break;
        }
    }

    if (capacity) {
       write = snprintf(buff + index, capacity, "\n\n");
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
               if (ctx->mm_cli[i].id == ntfy.surface_hdl) {
                  ALOGD("%s: rest drop-dup-count on sfid %x, id %p", __FUNCTION__, ntfy.surface_hdl, ctx->mm_cli[i].id);
                  ctx->mm_cli[i].last_ping_frame_id = LAST_PING_FRAME_ID_INVALID;
                  break;
               }
           }
       }
       break;
       case HWC_BINDER_NTFY_OVERSCAN:
            memcpy(&ctx->overscan_position, &ntfy.frame, sizeof(ctx->overscan_position));
            {
                NEXUS_Error rc;
                NEXUS_SurfaceComposition composition;
                NxClient_GetSurfaceClientComposition(ctx->disp_cli[HWC_PRIMARY_IX].sccid, &composition);
                composition.virtualDisplay.width = ctx->cfg[HWC_PRIMARY_IX].width;
                composition.virtualDisplay.height = ctx->cfg[HWC_PRIMARY_IX].height;
                composition.position.x = ctx->overscan_position.x;
                composition.position.y = ctx->overscan_position.y;
                composition.position.width = (int)ctx->cfg[HWC_PRIMARY_IX].width + ctx->overscan_position.w;
                composition.position.height = (int)ctx->cfg[HWC_PRIMARY_IX].height + ctx->overscan_position.h;
                composition.zorder = GPX_CLIENT_ZORDER;
                composition.visible = true;
                composition.colorBlend = colorBlendingEquation[BLENDIND_TYPE_SRC_OVER];
                composition.alphaBlend = alphaBlendingEquation[BLENDIND_TYPE_SRC_OVER];
                rc = NxClient_SetSurfaceClientComposition(ctx->disp_cli[HWC_PRIMARY_IX].sccid, &composition);
                if ( rc ) {
                    ALOGW("%s: Unable to set client composition for overscan adjustment %d", __FUNCTION__, rc);
                }
            }
       break;
       case HWC_BINDER_NTFY_SIDEBAND_SURFACE_ACQUIRED:
       default:
       break;
       }
   }
}

static bool is_video_layer(struct hwc_context_t *ctx, hwc_layer_1_t *layer, int layer_id, bool *is_sideband, bool *is_yuv)
{
    bool rc = false;
    NexusClientContext *client_context = NULL;
    void *pAddr;

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
            private_handle_t *gr_buffer = (private_handle_t *)layer->handle;
            hwc_mem_lock(ctx, (unsigned)gr_buffer->sharedData, &pAddr, true);
            PSHARED_DATA pSharedData = (PSHARED_DATA) pAddr;
            if (pSharedData == NULL) {
               hwc_mem_unlock(ctx, (unsigned)gr_buffer->sharedData, true);
               goto out;
            }
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
            hwc_mem_unlock(ctx, (unsigned)gr_buffer->sharedData, true);
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
out:
    return rc;
}

static bool split_layer_scaling(struct hwc_context_t *ctx, hwc_layer_1_t *layer)
{
    bool ret = true;
    void *pAddr;

    private_handle_t *gr_buffer = (private_handle_t *)layer->handle;
    hwc_mem_lock(ctx, (unsigned)gr_buffer->sharedData, &pAddr, true);
    PSHARED_DATA pSharedData = (PSHARED_DATA) pAddr;

    NEXUS_Rect clip_position = {(int16_t)(int)layer->sourceCropf.left,
                                (int16_t)(int)layer->sourceCropf.top,
                                (uint16_t)((int)layer->sourceCropf.right - (int)layer->sourceCropf.left),
                                (uint16_t)((int)layer->sourceCropf.bottom - (int)layer->sourceCropf.top)};

    if (!layer->handle) {
        goto out;
    }

    if (pSharedData == NULL) {
        goto out;
    }

    if (is_video_layer(ctx, layer, -1, NULL, NULL)) {
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
    hwc_mem_unlock(ctx, (unsigned)gr_buffer->sharedData, true);
    return ret;
}

static bool hwc_can_layer_init_display(struct hwc_context_t* ctx, int layer_id, NEXUS_SurfaceComposition *pComp,
                                       NEXUS_SurfaceHandle outputHdl, bool is_virtual)
{
    NEXUS_SurfaceCreateSettings settings;

    if (is_virtual) {
       return false;
    }

    if (ctx->gpx_cli[layer_id].skip_set) {
       return true;
    }

    if ((memcmp(&pComp->colorBlend, &colorBlendingEquation[BLENDIND_TYPE_SRC_OVER], sizeof(pComp->colorBlend)) != 0) &&
       (memcmp(&pComp->colorBlend, &colorBlendingEquation[BLENDIND_TYPE_SRC], sizeof(pComp->colorBlend)) != 0)) {
       return false;
    }
    if ((memcmp(&pComp->alphaBlend, &alphaBlendingEquation[BLENDIND_TYPE_SRC_OVER], sizeof(pComp->alphaBlend)) != 0) &&
       (memcmp(&pComp->alphaBlend, &alphaBlendingEquation[BLENDIND_TYPE_SRC], sizeof(pComp->alphaBlend)) != 0)) {
       return false;
    }

    NEXUS_Surface_GetCreateSettings(outputHdl, &settings);
    if ((pComp->clipRect.x == 0) && (pComp->clipRect.y == 0) &&
       (pComp->clipRect.width == settings.width) &&
       (pComp->clipRect.height == settings.height)) {
       return true;
    }

    return false;
}

static void hwc_set_layer_blending(struct hwc_context_t* ctx, int layer_id, int blending)
{
    NEXUS_SurfaceComposition *pComp;

    // Assumption is that this is a gpx layer
    pComp = &ctx->gpx_cli[layer_id].composition;
    pComp->colorBlend = colorBlendingEquation[blending];
    pComp->alphaBlend = alphaBlendingEquation[blending];
}


bool hwc_compose_gralloc_buffer(
   struct hwc_context_t* ctx, int layer_id, PSHARED_DATA pSharedData,
   private_handle_t *gr_buffer, NEXUS_SurfaceHandle outputHdl, bool is_virtual,
   bool check_transparency, bool *skip_set, NEXUS_SurfaceHandle *pActSurf,
   NEXUS_SurfaceComposition *pComp)
{
    bool composed = false;
    bool is_cursor_layer = false;
    NEXUS_Error rc;
    int plane_select = -1;
    unsigned int stride = 0;
    void *pAddr;
    void *slock;
    NEXUS_PixelFormat pixel_format = NEXUS_PixelFormat_eUnknown;
    NEXUS_SurfaceCreateSettings displaySurfaceSettings;

    if (!is_virtual) {
       if (ctx->gpx_cli[layer_id].skip_set) {
          if (skip_set) {
             *skip_set = true;
          }
          ALOGV("%s: willingly skipping layer: %d\n", __FUNCTION__, layer_id);
          goto out;
       }
       is_cursor_layer = (ctx->gpx_cli[layer_id].layer_flags & HWC_IS_CURSOR_LAYER) ? true : false;
    }

    if (pSharedData->planes[DEFAULT_PLANE].format == HAL_PIXEL_FORMAT_YV12) {
        if (pSharedData->planes[EXTRA_PLANE].physAddr) {
            plane_select = EXTRA_PLANE;
            pixel_format = (NEXUS_PixelFormat)pSharedData->planes[plane_select].format;
        } else if (pSharedData->planes[GL_PLANE].physAddr) {
            plane_select = GL_PLANE;
            pixel_format = (NEXUS_PixelFormat)pSharedData->planes[plane_select].format;
        }
        if (plane_select == -1) {
            ALOGE("%s: unable to select yv12 plane: %d\n", __FUNCTION__, layer_id);
            goto out;
        }
        stride = pSharedData->planes[plane_select].width*pSharedData->planes[plane_select].bpp;
    }
    else {
        plane_select = DEFAULT_PLANE;
        stride = gr_buffer->oglStride;
        pixel_format = gralloc_to_nexus_pixel_format(pSharedData->planes[DEFAULT_PLANE].format);
    }

    hwc_mem_lock(ctx, pSharedData->planes[plane_select].physAddr, &pAddr, false);
    if ((ctx->hwc_with_mma && !pSharedData->planes[plane_select].physAddr) ||
        (!ctx->hwc_with_mma && pAddr == NULL)) {
        ALOGE("%s: plane buffer address NULL: %d\n", __FUNCTION__, layer_id);
        goto out_unlock;
    }

    NEXUS_Surface_GetCreateSettings(outputHdl, &displaySurfaceSettings);
    if ((is_virtual && ctx->display_dump_virt) ||
        (!is_virtual && ctx->display_dump_layer)) {
       ALOGI("%s: layer %u vis %u pf %u %ux%u (%ux%u@%u,%u) to %ux%u@%u,%u",
             is_virtual ? "vcmp" : "comp", layer_id, (unsigned)pComp->visible,
             pixel_format, pSharedData->planes[plane_select].width, pSharedData->planes[plane_select].height,
             pComp->clipRect.width, pComp->clipRect.height, pComp->clipRect.x, pComp->clipRect.y,
             pComp->position.width, pComp->position.height, pComp->position.x, pComp->position.y);
    }

    *pActSurf = NULL;
    if (pComp->visible) {
        size_t width = pSharedData->planes[DEFAULT_PLANE].width;
        if (!is_cursor_layer && check_transparency &&
            (pixel_format == NEXUS_PixelFormat_eA8_B8_G8_R8) &&
            (width > 32) && (width % 8  == 0)) {
           void *pAddr;
           bool transparent = false;
           hwc_mem_lock(ctx, pSharedData->planes[DEFAULT_PLANE].physAddr, &pAddr, true);
           if (pAddr != NULL) {
              nsecs_t t1, t2;
              t1 = systemTime(CLOCK_MONOTONIC);
              transparent = check_frame_transparency(pAddr, pSharedData->planes[DEFAULT_PLANE].width,
                                                          pSharedData->planes[DEFAULT_PLANE].height, stride);
              t2 = systemTime(CLOCK_MONOTONIC);
              int msecDelay = toMillisecondTimeoutDelay(t1, t2);
              ALOGV("%s, trasparency:%d, delay:%d", __FUNCTION__, transparent, msecDelay);
           }
           hwc_mem_unlock(ctx, (unsigned)pSharedData->planes[DEFAULT_PLANE].physAddr, true);
           if (transparent)
              goto out_unlock;
        }

        *pActSurf = hwc_to_nsc_surface(pSharedData->planes[plane_select].width,
                                       pSharedData->planes[plane_select].height,
                                       stride,
                                       pixel_format,
                                       ctx->hwc_with_mma,
                                       pSharedData->planes[plane_select].physAddr,
                                       (uint8_t *)pAddr);

        if (*pActSurf != NULL) {
            NEXUS_Graphics2DBlitSettings blitSettings;
            int adj;
            NEXUS_Graphics2D_GetDefaultBlitSettings(&blitSettings);

            blitSettings.source.surface = *pActSurf;
            blitSettings.source.rect = pComp->clipRect;
            blitSettings.dest.surface = outputHdl;
            blitSettings.output.surface = outputHdl;
            blitSettings.output.rect = pComp->position;
            blitSettings.colorOp = NEXUS_BlitColorOp_eUseBlendEquation;
            blitSettings.alphaOp = NEXUS_BlitAlphaOp_eUseBlendEquation;
            blitSettings.colorBlend = pComp->colorBlend;
            blitSettings.alphaBlend = pComp->alphaBlend;

            if ( blitSettings.output.rect.x < 0 ) {
                adj = -blitSettings.output.rect.x;
                blitSettings.output.rect.x = 0;
                blitSettings.output.rect.width = (adj <= blitSettings.output.rect.width) ? blitSettings.output.rect.width - adj : 0;
                blitSettings.source.rect.x += adj;
                blitSettings.source.rect.width = (adj <= blitSettings.source.rect.width) ? blitSettings.source.rect.width - adj : 0;
            }
            if ( blitSettings.output.rect.y < 0 ) {
                adj = -blitSettings.output.rect.y;
                blitSettings.output.rect.y = 0;
                blitSettings.output.rect.height = (adj <= blitSettings.output.rect.height) ? blitSettings.output.rect.height - adj : 0;
                blitSettings.source.rect.y += adj;
                blitSettings.source.rect.height = (adj <= blitSettings.source.rect.height) ? blitSettings.source.rect.height - adj : 0;
            }
            if ( blitSettings.output.rect.x + (int)blitSettings.output.rect.width > (int)displaySurfaceSettings.width ) {
                adj = (blitSettings.output.rect.x + blitSettings.output.rect.width) - displaySurfaceSettings.width;
                blitSettings.output.rect.width = (adj <= blitSettings.output.rect.width) ? blitSettings.output.rect.width - adj : 0;
                blitSettings.source.rect.width = (adj <= blitSettings.source.rect.width) ? blitSettings.source.rect.width - adj : 0;
            }
            if ( blitSettings.output.rect.y + (int)blitSettings.output.rect.height > (int)displaySurfaceSettings.height ) {
                adj = (blitSettings.output.rect.y + blitSettings.output.rect.height) - displaySurfaceSettings.height;
                blitSettings.output.rect.height = (adj <= blitSettings.output.rect.height) ? blitSettings.output.rect.height - adj : 0;
                blitSettings.source.rect.height = (adj <= blitSettings.source.rect.height) ? blitSettings.source.rect.height - adj : 0;
            }

            if ( blitSettings.output.rect.width > 0 && blitSettings.output.rect.height > 0 &&
                 blitSettings.source.rect.width > 0 && blitSettings.source.rect.height  > 0 ) {

               blitSettings.dest.rect = blitSettings.output.rect;
               if (BKNI_AcquireMutex(ctx->g2d_mutex) != BERR_SUCCESS) {
                  ALOGE("%s: failed g2d_mutex!", __FUNCTION__);
               } else {
                  rc = NEXUS_Graphics2D_Blit(ctx->hwc_g2d, &blitSettings);
                  if (rc == NEXUS_GRAPHICS2D_QUEUE_FULL) {
                      rc = hwc_checkpoint_locked(ctx);
                      if (rc)  {
                         ALOGW("Checkpoint timeout composing layer %u", __FUNCTION__, layer_id);
                      }
                      rc = NEXUS_Graphics2D_Blit(ctx->hwc_g2d, &blitSettings);
                  }
                  if (rc) {
                     ALOGE("%s: Unable to blit layer %u", __FUNCTION__, layer_id);
                  } else {
                     composed = true;
                  }
                  BKNI_ReleaseMutex(ctx->g2d_mutex);
               }
            }
        }
    }

out_unlock:
   hwc_mem_unlock(ctx, (unsigned)pSharedData->planes[plane_select].physAddr, false);
out:
   return composed;
}

static bool primary_need_nsc_layer(struct hwc_context_t *ctx, hwc_layer_1_t *layer,
   size_t total_layers, size_t *overlay_layers)
{
    bool rc = false;
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

    if (layer->compositionType == HWC_OVERLAY) {
       *overlay_layers += 1;
    }
    rc = true;
out:

    if (!rc && (total_layers == 1) && ctx->display_dump_layer) {
       ALOGI("comp: %llu - skip-single - reason %d", ctx->stats[HWC_PRIMARY_IX].prepare_call, skip_layer);
    }
    return rc;
}

static void hwc_prepare_gpx_layer(
    struct hwc_context_t* ctx, hwc_layer_1_t *layer, int layer_id,
    bool geometry_changed, bool is_virtual, bool is_locked,
    unsigned int *skip_set)
{
    NEXUS_Error rc;
    private_handle_t *gr_buffer = NULL;
    void *pAddr;
    NEXUS_SurfaceComposition *pComp;
    NEXUS_Rect disp_position = {(int16_t)layer->displayFrame.left,
                                (int16_t)layer->displayFrame.top,
                                (uint16_t)(layer->displayFrame.right - layer->displayFrame.left),
                                (uint16_t)(layer->displayFrame.bottom - layer->displayFrame.top)};
    NEXUS_Rect clip_position = {(int16_t)(int)layer->sourceCropf.left,
                                (int16_t)(int)layer->sourceCropf.top,
                                (uint16_t)((int)layer->sourceCropf.right - (int)layer->sourceCropf.left),
                                (uint16_t)((int)layer->sourceCropf.bottom - (int)layer->sourceCropf.top)};
    int cur_width = 0, cur_height = 0;
    unsigned int cur_blending_type;
    bool layer_changed = false;

    // sideband layer is handled through the video window directly.
    if (layer->compositionType == HWC_SIDEBAND) {
        return;
    }

    // if we do not have a valid handle at this point, we have a problem.
    if (!layer->handle) {
        ALOGE("%s: invalid handle on layer_id %d", __FUNCTION__, layer_id);
        return;
    }

    gr_buffer = (private_handle_t *)layer->handle;
    hwc_mem_lock(ctx, (unsigned)gr_buffer->sharedData, &pAddr, true);
    PSHARED_DATA pSharedData = (PSHARED_DATA) pAddr;
    if (pSharedData == NULL) {
        ALOGE("%s: invalid buffer on layer_id %d", __FUNCTION__, layer_id);
        goto out;
    }
    hwc_get_buffer_sizes(pSharedData, &cur_width, &cur_height);
    cur_blending_type = hwc_android_blend_to_nsc_blend(layer->blending);

    if (!is_locked) {
       if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
           goto out;
       }
    }

    ctx->gpx_cli[layer_id].skip_set = false;
    if (ctx->skip_dup &&
        (layer->compositionType == HWC_OVERLAY) &&
        !(layer->flags & HWC_IS_CURSOR_LAYER)) {
       if (ctx->gpx_cli[layer_id].last.layerhdl &&
           (ctx->gpx_cli[layer_id].last.layerhdl == layer->handle) &&
           !geometry_changed) {
           ctx->gpx_cli[layer_id].skip_set = true;
           if (skip_set) {
              *skip_set += 1;
           }
           goto out_mutex;
       }
    }

    if (!is_virtual) {
       ctx->gpx_cli[layer_id].last.layerhdl = layer->handle;
       ctx->gpx_cli[layer_id].layer_type = layer->compositionType;
       ctx->gpx_cli[layer_id].layer_subtype = NEXUS_SURFACE_COMPOSITOR;
       if (ctx->gpx_cli[layer_id].layer_type == HWC_CURSOR_OVERLAY) {
          ctx->gpx_cli[layer_id].layer_subtype = NEXUS_CURSOR;
       }
       ctx->gpx_cli[layer_id].layer_flags = layer->flags;
       ctx->gpx_cli[layer_id].plane_alpha = layer->planeAlpha;

       if ((memcmp((void *)&disp_position, (void *)&ctx->gpx_cli[layer_id].composition.position, sizeof(NEXUS_Rect)) != 0) ||
           (memcmp((void *)&clip_position, (void *)&ctx->gpx_cli[layer_id].composition.clipRect, sizeof(NEXUS_Rect)) != 0) ||
           (cur_width != ctx->gpx_cli[layer_id].composition.clipBase.width) ||
           (cur_height != ctx->gpx_cli[layer_id].composition.clipBase.height) ||
           (cur_blending_type != ctx->gpx_cli[layer_id].blending_type)) {
          layer_changed = true;
       }
       pComp = &ctx->gpx_cli[layer_id].composition;
    } else {
       layer_changed = true;
       pComp = &ctx->vd_cli[layer_id].composition;
    }

    if (layer_changed || !pComp->visible || geometry_changed) {
        pComp->zorder                = (layer->flags & HWC_IS_CURSOR_LAYER) ? CURSOR_CLIENT_ZORDER : GPX_CLIENT_ZORDER;
        pComp->position.x            = disp_position.x;
        pComp->position.y            = disp_position.y;
        pComp->position.width        = disp_position.width;
        pComp->position.height       = disp_position.height;
        pComp->virtualDisplay.width  = ctx->cfg[is_virtual?1:0].width;
        pComp->virtualDisplay.height = ctx->cfg[is_virtual?1:0].height;
        pComp->colorBlend            = colorBlendingEquation[cur_blending_type];
        pComp->alphaBlend            = alphaBlendingEquation[cur_blending_type];
        pComp->visible               = true;
        pComp->clipRect.x            = clip_position.x;
        pComp->clipRect.y            = clip_position.y;
        pComp->clipRect.width        = clip_position.width;
        pComp->clipRect.height       = clip_position.height;
        pComp->clipBase.width        = cur_width;
        pComp->clipBase.height       = cur_height;

        if (!is_virtual) {
           ctx->gpx_cli[layer_id].blending_type = cur_blending_type;
        }
    }

out_mutex:
    if (!is_locked) {
       BKNI_ReleaseMutex(ctx->mutex);
    }
out:
    hwc_mem_unlock(ctx, (unsigned)gr_buffer->sharedData, true);
    return;
}

static void primary_composition_setup(struct hwc_context_t *ctx, hwc_display_contents_1_t* list)
{
    size_t i;
    hwc_layer_1_t *layer;
    int skip_layer_index = -1;
    bool has_video = false;

    if (ctx->display_dump_layer) {
       ALOGI("comp: %llu - classify: %d, geom: %d",
          ctx->stats[HWC_PRIMARY_IX].prepare_call, list->numHwLayers, (bool)(list->flags & HWC_GEOMETRY_CHANGED));
    }

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
       if (is_video_layer(ctx, layer, -1, NULL, NULL)) {
          has_video = true;
          break;
       }
    }

    if ((ctx->display_gles_fallback && skip_layer_index != -1) ||
        (ctx->display_gles_always && has_video == false)) {
       for (i = 0; i < list->numHwLayers; i++) {
          layer = &list->hwLayers[i];
          if (layer->compositionType == HWC_OVERLAY &&
              !is_video_layer(ctx, layer, -1, NULL, NULL)) {
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
    struct hwc_context_t *ctx = (struct hwc_context_t*)dev;
    hwc_layer_1_t *layer;
    size_t i;
    int nx_layer_count = 0;
    int skiped_layer = 0;
    unsigned int video_layer_id = 0;
    unsigned int sideband_layer_id = 0;
    unsigned int skip_set = 0;
    size_t overlay_layers = 0;

    ctx->stats[HWC_PRIMARY_IX].prepare_call += 1;

    if (BKNI_AcquireMutex(ctx->power_mutex) != BERR_SUCCESS) {
        ALOGE("%s: Could not acquire power_mutex!!!", __FUNCTION__);
        ctx->stats[HWC_PRIMARY_IX].prepare_skipped += 1;
        goto out;
    }

    if ((ctx->power_mode != HWC_POWER_MODE_OFF) && (ctx->power_mode != HWC_POWER_MODE_DOZE_SUSPEND)) {
        BKNI_ReleaseMutex(ctx->power_mutex);

        if (ctx->hwc_binder) {
           ctx->hwc_binder->connect();
           hwc_binder_advertise_video_surface(ctx);
        }

        // setup the layer composition classification.
        primary_composition_setup(ctx, list);

        // reset all video/sideband layers first.
        hwc_hide_unused_mm_layers(ctx);
        hwc_hide_unused_sb_layers(ctx);

        // allocate the NSC layer, if need be change the geometry, etc...
        for (i = 0; i < list->numHwLayers; i++) {
            layer = &list->hwLayers[i];
            if (primary_need_nsc_layer(ctx, layer, list->numHwLayers, &overlay_layers)) {
                if (ctx->display_dump_layer) {
                   unsigned handle_dump = 0;
                   if (layer->handle != NULL) {
                      private_handle_t *gr_buffer = (private_handle_t *)layer->handle;
                      NEXUS_Addr pAddr;
                      hwc_mem_lock_phys(ctx, (unsigned)gr_buffer->sharedData, &pAddr);
                      handle_dump = (unsigned)pAddr;
                      hwc_mem_unlock_phys(ctx, (unsigned)gr_buffer->sharedData);
                   }
                   ALOGI("comp: %llu - show - sf:%d (%d) - hdl:0x%x", ctx->stats[HWC_PRIMARY_IX].prepare_call, (int)i, (int)layer->compositionType, handle_dump);
                }
                hwc_nsc_prepare_layer(ctx, layer, (int)i,
                                      (bool)(list->flags & HWC_GEOMETRY_CHANGED),
                                      &video_layer_id,
                                      &sideband_layer_id,
                                      &skip_set);
            } else {
                if (ctx->display_dump_layer) {
                   ALOGI("comp: %llu - hiding - sf:%d (%d)", ctx->stats[HWC_PRIMARY_IX].prepare_call, (int)i, (int)layer->compositionType);
                }
                hwc_hide_unused_gpx_layer(ctx, i);
            }
        }

        if ((skip_set > 1) ||
            ((overlay_layers - (video_layer_id + sideband_layer_id)) > 1)) {
           if (BKNI_AcquireMutex(ctx->mutex) == BERR_SUCCESS) {
              for (i = 0; i < list->numHwLayers; i++) {
                 ctx->gpx_cli[i].skip_set = false;
              }
              BKNI_ReleaseMutex(ctx->mutex);
           } else {
              ALOGE("comp: %llu - failed hidding skip", ctx->stats[HWC_PRIMARY_IX].prepare_call);
           }
        }

        // remove all remaining gpx layers from the stack that are not needed.
        for ( i = list->numHwLayers; i < NSC_GPX_CLIENTS_NUMBER; i++) {
           hwc_hide_unused_gpx_layer(ctx, i);
        }
    }
    else {
        ctx->stats[HWC_PRIMARY_IX].prepare_skipped += 1;
        BKNI_ReleaseMutex(ctx->power_mutex);
    }

out:
    return 0;
}

static int hwc_prepare_virtual(hwc_composer_device_1_t *dev, hwc_display_contents_1_t* list)
{
    struct hwc_context_t *ctx = (struct hwc_context_t*)dev;
    int rc = 0;
    hwc_layer_1_t *layer;
    size_t i;
    NEXUS_SurfaceComposition surf_comp;
    unsigned int stride;
    void *pAddr;
    int layer_id = 0;
    int virt_w = -1, virt_h = -1;

    if (list) {
       ctx->stats[HWC_VIRTUAL_IX].prepare_call += 1;

       if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
           ctx->stats[HWC_VIRTUAL_IX].prepare_skipped += 1;
           goto out;
       }

       for (i = 0; i < HWC_VD_CLIENTS_NUMBER; i++) {
           ctx->vd_cli[i].composition.visible = false;
       }

       ctx->cfg[HWC_VIRTUAL_IX].width = -1;
       ctx->cfg[HWC_VIRTUAL_IX].height = -1;
       if (list->outbuf != NULL) {
          void *pAddr;
          private_handle_t *gr_buffer = (private_handle_t *)list->outbuf;
          hwc_mem_lock(ctx, (unsigned)gr_buffer->sharedData, &pAddr, true);
          PSHARED_DATA pSharedData = (PSHARED_DATA) pAddr;
          if (pSharedData != NULL) {
             hwc_get_buffer_sizes(pSharedData, &virt_w, &virt_h);
             ctx->cfg[HWC_VIRTUAL_IX].width = virt_w;
             ctx->cfg[HWC_VIRTUAL_IX].height = virt_h;
          }
          hwc_mem_unlock(ctx, (unsigned)gr_buffer->sharedData, true);
       }

       if (ctx->display_gles_always || ctx->display_gles_virtual ||
           (ctx->cfg[HWC_VIRTUAL_IX].width == -1 && ctx->cfg[HWC_VIRTUAL_IX].height == -1)) {
          goto out_unlock;
       }

       for (i = 0; i < list->numHwLayers; i++) {
          layer = &list->hwLayers[i];
          if (ctx->display_dump_virt) {
             ALOGI("vcmp: %llu - %dx%d, layer: %d, kind: %d",
                   ctx->stats[HWC_VIRTUAL_IX].prepare_call, virt_w, virt_h, i, layer->compositionType);
          }
          if ((layer->compositionType == HWC_FRAMEBUFFER) || (layer->compositionType == HWC_OVERLAY)) {
             hwc_prepare_gpx_layer(ctx, layer, layer_id, false /*don't care*/, true, true, NULL);
             layer_id++;
             layer->compositionType = HWC_OVERLAY;
          }
       }

       if (ctx->display_dump_virt) {
          ALOGI("vcmp: %llu - %dx%d, composing %d layers", ctx->stats[HWC_VIRTUAL_IX].prepare_call, virt_w, virt_h, layer_id);
       }
    }

out_unlock:
    BKNI_ReleaseMutex(ctx->mutex);
out:
    return rc;
}

static int hwc_prepare(hwc_composer_device_1_t *dev,
        size_t numDisplays, hwc_display_contents_1_t** displays)
{
    int rc = 0;

    if (!numDisplays) {
        rc = -EINVAL;
        goto out;
    }

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

out:
    if (rc) {
       ALOGE("%s: failure %d", __FUNCTION__, rc);
    }
    return rc;
}

static void hwc_put_disp_surface(struct hwc_context_t *ctx, int disp_ix, NEXUS_SurfaceHandle surface)
{
   if (disp_ix > (DISPLAY_SUPPORTED-1)) {
      return;
   }

   if (surface) {
      NEXUS_SurfaceHandle *pSurface = BFIFO_WRITE(&ctx->disp_cli[disp_ix].display_fifo);
      *pSurface = surface;
      BFIFO_WRITE_COMMIT(&ctx->disp_cli[disp_ix].display_fifo, 1);
   }
}

static void hwc_seed_disp_surface(struct hwc_context_t *ctx, NEXUS_SurfaceHandle surface)
{
   if (surface) {
      NEXUS_Graphics2DFillSettings fillSettings;
      NEXUS_Graphics2D_GetDefaultFillSettings(&fillSettings);
      fillSettings.surface = surface;
      fillSettings.color = 0;
      fillSettings.colorOp = NEXUS_FillOp_eCopy;
      fillSettings.alphaOp = NEXUS_FillOp_eCopy;
      if (BKNI_AcquireMutex(ctx->g2d_mutex) != BERR_SUCCESS) {
         ALOGE("%s: failed g2d_mutex!", __FUNCTION__);
         return;
      }
      NEXUS_Graphics2D_Fill(ctx->hwc_g2d, &fillSettings);
      BKNI_ReleaseMutex(ctx->g2d_mutex);
   }
}

#define HWC_SURFACE_WAIT_TIMEOUT 50
#define HWC_SURFACE_WAIT_ATTEMPT 4
static NEXUS_SurfaceHandle hwc_get_disp_surface(struct hwc_context_t *ctx, int disp_ix)
{
    int no_surface_count = 0;

    if (disp_ix > (DISPLAY_SUPPORTED-1)) {
       return NULL;
    }

    while (BFIFO_READ_PEEK(&ctx->disp_cli[disp_ix].display_fifo) == 0) {
       if (BKNI_WaitForEvent(ctx->recycle_event, HWC_SURFACE_WAIT_TIMEOUT)) {
          no_surface_count++;
          if (no_surface_count == HWC_SURFACE_WAIT_ATTEMPT) {
             ALOGW("%s: warning no surface received in %d ms", __FUNCTION__,
                   HWC_SURFACE_WAIT_TIMEOUT*HWC_SURFACE_WAIT_ATTEMPT);
             return NULL;
          }
       }
    }

    NEXUS_SurfaceHandle *pSurface = BFIFO_READ(&ctx->disp_cli[disp_ix].display_fifo);
    BFIFO_READ_COMMIT(&ctx->disp_cli[disp_ix].display_fifo, 1);
    return *pSurface;
}

static int hwc_retire_fence(struct hwc_context_t *ctx, int index)
{
   struct sync_fence_info_data info;
   int sw_timeline, fence;

   if (index != HWC_PRIMARY_IX && index != HWC_VIRTUAL_IX) {
      goto out_err;
   }

   sw_timeline = ctx->composer_ret_timeline[index];
   if (sw_timeline == INVALID_FENCE) {
      goto out_err;
   }

   snprintf(info.name, sizeof(info.name), "hwc_%d_RET_%llu", index, ctx->stats[index].set_call);
   fence = sw_sync_fence_create(sw_timeline, info.name, ctx->stats[index].set_call);
   if (fence < 0) {
      goto out_err;
   }

   ALOGV("%s: %s:%s -> %d/%d", __FUNCTION__, (index == HWC_PRIMARY_IX) ? "prim" : "virt",
         info.name, sw_timeline, fence);
   return fence;

out_err:
   ALOGW("%s: %s: failed RET fence %llu", __FUNCTION__, (index == HWC_PRIMARY_IX) ? "prim" : "virt",
         ctx->stats[index].set_call);
   return INVALID_FENCE;
}

static void hwc_inc_retire_timeline(struct hwc_context_t *ctx, int index, int inc, bool err)
{
   if (ctx->dump_fence) {
      ALOGI("%s: %llu - retire[%d]-inc: %d, on-err: %d\n",
            (index == HWC_PRIMARY_IX) ? "fence" : "vfence",
            ctx->stats[index].set_call, ctx->composer_ret_timeline[index], inc, err);
   }

   if (ctx->composer_ret_timeline[index] != INVALID_FENCE) {
      sw_sync_timeline_inc(ctx->composer_ret_timeline[index], (unsigned) inc);
   }
}

static int hwc_release_timeline_with_fence(struct hwc_context_t *ctx, int index, int layer, int *fence)
{
   struct sync_fence_info_data info;
   int sw_timeline;

   if (fence == NULL) {
      goto out_err;
   }

   if (index != HWC_PRIMARY_IX && index != HWC_VIRTUAL_IX) {
      goto out_err;
   }

   sw_timeline = sw_sync_timeline_create();
   if (sw_timeline < 0) {
      goto out_err;
   }

   snprintf(info.name, sizeof(info.name), "hwc_%d_REL_%llu_%u", index, ctx->stats[index].set_call, layer);
   *fence = sw_sync_fence_create(sw_timeline, info.name, 1);
   if (*fence < 0) {
      goto out_err;
   }

   ALOGV("%s: %s:%s -> %d/%d", __FUNCTION__, (index == HWC_PRIMARY_IX) ? "prim" : "virt",
         info.name, sw_timeline, *fence);
   return sw_timeline;

out_err:
   ALOGW("%s: %s: failed REL fence %llu/%d, timeline %d", __FUNCTION__, (index == HWC_PRIMARY_IX) ? "prim" : "virt",
         ctx->stats[index].set_call, layer, sw_timeline);
   *fence = INVALID_FENCE;
   if (sw_timeline >= 0) {
     close(sw_timeline);
   }
   return INVALID_FENCE;
}

static int hwc_set_primary(struct hwc_context_t *ctx, hwc_display_contents_1_t* list)
{
    size_t hwc_work_item_size = 0;
    struct hwc_work_item *this_frame = NULL;
    size_t i;

    if (!list) {
       return -EINVAL;
    }

    ctx->stats[HWC_PRIMARY_IX].set_call += 1;

    if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
        ctx->stats[HWC_PRIMARY_IX].set_skipped += 1;
        goto out_close_fence;
    }

    if (BKNI_AcquireMutex(ctx->power_mutex) != BERR_SUCCESS) {
        ALOGE("%s: Could not acquire power_mutex!!!", __FUNCTION__);
        ctx->stats[HWC_PRIMARY_IX].set_skipped += 1;
        goto out_mutex;
    }

    if ((ctx->power_mode != HWC_POWER_MODE_OFF) && (ctx->power_mode != HWC_POWER_MODE_DOZE_SUSPEND)) {
        BKNI_ReleaseMutex(ctx->power_mutex);

        hwc_work_item_size = sizeof(hwc_work_item) + (list->numHwLayers * sizeof(hwc_layer_1_t));
        this_frame = (struct hwc_work_item *) calloc(1, hwc_work_item_size);
        if (this_frame == NULL) {
           goto out_mutex;
        }
        memcpy(&this_frame->content, list,
               sizeof(hwc_display_contents_1_t) + (list->numHwLayers * sizeof(hwc_layer_1_t)));
        for (i = 0; i < list->numHwLayers; i++) {
           memcpy(&this_frame->comp[i], &ctx->gpx_cli[i].composition, sizeof(NEXUS_SurfaceComposition));
        }
        this_frame->comp_ix = ctx->stats[HWC_PRIMARY_IX].set_call;
        this_frame->next = NULL;
        if (ctx->composer_work_list[HWC_PRIMARY_IX] == NULL) {
           ctx->composer_work_list[HWC_PRIMARY_IX] = this_frame;
        } else {
           struct hwc_work_item *item, *last;
           last = ctx->composer_work_list[HWC_PRIMARY_IX];
           while (last != NULL) { item = last; last = last->next; }
           item->next = this_frame;
        }

        if (ctx->track_comp_time) {
          this_frame->tick_queued = hwc_tick();
        }

        if (!ctx->fence_support) {
           list->retireFenceFd = INVALID_FENCE;
           this_frame->content.retireFenceFd = INVALID_FENCE;
           for (i = 0; i < list->numHwLayers; i++) {
              list->hwLayers[i].releaseFenceFd = INVALID_FENCE;
              this_frame->content.hwLayers[i].releaseFenceFd = INVALID_FENCE;
           }
        } else {
           list->retireFenceFd = hwc_retire_fence(ctx, HWC_PRIMARY_IX);
           this_frame->content.retireFenceFd = list->retireFenceFd;
           if (ctx->dump_fence) {
              ALOGI("fence: %llu/%d - retire: %d\n",
                 ctx->stats[HWC_PRIMARY_IX].set_call, i, list->retireFenceFd);
           }
           for (i = 0; i < list->numHwLayers; i++) {
              if (((list->hwLayers[i].compositionType == HWC_OVERLAY) ||
                   (list->hwLayers[i].compositionType == HWC_FRAMEBUFFER_TARGET)) &&
                   ctx->gpx_cli[i].composition.visible) {
                 this_frame->content.hwLayers[i].releaseFenceFd =
                     hwc_release_timeline_with_fence(ctx, HWC_PRIMARY_IX, i, &list->hwLayers[i].releaseFenceFd);
                 if (ctx->dump_fence) {
                    ALOGI("fence: %llu/%d - timeline: %d -> fence: %d\n",
                       ctx->stats[HWC_PRIMARY_IX].set_call, i,
                       this_frame->content.hwLayers[i].releaseFenceFd, list->hwLayers[i].releaseFenceFd);
                 }
              }
           }
        }

        if (ctx->display_dump_layer) {
           ALOGI("comp: %llu (%llu) - queued\n",
                 ctx->stats[HWC_PRIMARY_IX].set_call, ctx->stats[HWC_PRIMARY_IX].prepare_call);
        }

        if (!ctx->fence_support) {
           BKNI_ReleaseMutex(ctx->mutex);
           BKNI_SetEvent(ctx->composer_event[HWC_PRIMARY_IX]);
           BKNI_WaitForEvent(ctx->composer_event_sync[HWC_PRIMARY_IX], BKNI_INFINITE);
           goto out_close_fence;
        } else {
           BKNI_ReleaseMutex(ctx->mutex);
           BKNI_SetEvent(ctx->composer_event[HWC_PRIMARY_IX]);
           if (ctx->stats[HWC_PRIMARY_IX].set_call >= ctx->stats[HWC_PRIMARY_IX].composed + HWC_SET_COMP_THRESHOLD) {
              BKNI_WaitForEvent(ctx->composer_event_forced[HWC_PRIMARY_IX], BKNI_INFINITE);
           }
           goto out;
        }
    }

out_mutex:
    BKNI_ReleaseMutex(ctx->mutex);
out_close_fence:
    for (i = 0; i < list->numHwLayers; i++) {
       if (list->hwLayers[i].acquireFenceFd >= 0) {
          close(list->hwLayers[i].acquireFenceFd);
          if (ctx->dump_fence) {
             ALOGI("fence: %llu/%d - acquire: %d -> err+close\n",
                   ctx->stats[HWC_PRIMARY_IX].set_call, i,
                   list->hwLayers[i].acquireFenceFd);
          }
       }
    }
out:
    return 0;
}

static int hwc_compose_primary(struct hwc_context_t *ctx, hwc_work_item *item, int *overlay_seen, int *fb_target_seen)
{
   int layer_composed = 0;
   NEXUS_SurfaceHandle outputHdl = NULL;
   size_t i;
   NEXUS_Error rc;
   bool is_sideband = false, is_yuv = false, has_video = false, oob_video = false;
   bool display_surface_seeded, skip_set;
   hwc_display_contents_1_t* list = &item->content;
   NEXUS_SurfaceHandle surface[NSC_GPX_CLIENTS_NUMBER];

   memset(surface, 0, sizeof(surface));

   if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
       if (list->retireFenceFd != INVALID_FENCE) {
          hwc_inc_retire_timeline(ctx, HWC_PRIMARY_IX, 1, true);
       }
       goto out;
   }
   outputHdl = hwc_get_disp_surface(ctx, HWC_PRIMARY_IX);
   if (outputHdl == NULL) {
      ALOGE("%s: no display surface available", __FUNCTION__);
      BKNI_ReleaseMutex(ctx->mutex);
      if (list->retireFenceFd != INVALID_FENCE) {
         hwc_inc_retire_timeline(ctx, HWC_PRIMARY_IX, 1, true);
      }
      goto out;
   }
   BKNI_ReleaseMutex(ctx->mutex);

   for (i = 0; i < list->numHwLayers; i++) {
      if (list->hwLayers[i].handle &&
          private_handle_t::validate(list->hwLayers[i].handle) != 0) {
         ALOGW("comp: %llu/%llu - layer: %d - invalid for comp (%d)\n",
               ctx->stats[HWC_PRIMARY_IX].set_call, ctx->stats[HWC_PRIMARY_IX].composed,
               i, list->hwLayers[i].acquireFenceFd);
         continue;
      }
      if (i > NSC_GPX_CLIENTS_NUMBER-1) {
         ALOGE("Exceedeed max number of accounted layers\n");
         break;
      }
      void *pAddr;
      private_handle_t *gr_buffer = (private_handle_t *)list->hwLayers[i].handle;
      if (gr_buffer == NULL) {
         continue;
      }
      hwc_mem_lock(ctx, (unsigned)gr_buffer->sharedData, &pAddr, true);
      PSHARED_DATA pSharedData = (PSHARED_DATA) pAddr;
      has_video = is_video_layer(ctx, &list->hwLayers[i], -1, &is_sideband, &is_yuv);
      if (has_video && !is_yuv) {
         oob_video = true;
         if (!is_sideband) {
            if (ctx->hwc_binder) {
               // TODO: currently only one video window exposed.
               if (ctx->mm_cli[0].last_ping_frame_id != pSharedData->videoFrame.status.serialNumber) {
                  ctx->mm_cli[0].last_ping_frame_id = pSharedData->videoFrame.status.serialNumber;
                  ctx->hwc_binder->setframe(ctx->mm_cli[0].id, ctx->mm_cli[0].last_ping_frame_id);
               }
            }
         }
      } else if (item->comp[i].visible) {
         if (list->hwLayers[i].compositionType == HWC_OVERLAY) {
            *overlay_seen += 1;
         } else if (list->hwLayers[i].compositionType == HWC_FRAMEBUFFER_TARGET) {
            *fb_target_seen += 1;
         }
         if (!layer_composed) {
            bool can_skip_fill = oob_video && hwc_can_layer_init_display(ctx, i, &item->comp[i], outputHdl, false);
            if (!can_skip_fill) {
               display_surface_seeded = true;
               hwc_seed_disp_surface(ctx, outputHdl);
            } else {
               hwc_set_layer_blending(ctx, i, BLENDIND_TYPE_SRC);
            }
         }
         if (list->hwLayers[i].acquireFenceFd >= 0) {
            if (!ctx->fence_support) {
               ALOGI("fence: %llu/%d - fd:%d -> wait+close in sync mode!?! check platform setup.\n",
                     ctx->stats[HWC_PRIMARY_IX].set_call, i,
                     list->hwLayers[i].acquireFenceFd);
            }
            sync_wait(list->hwLayers[i].acquireFenceFd, BKNI_INFINITE);
            close(list->hwLayers[i].acquireFenceFd);
            list->hwLayers[i].acquireFenceFd = INVALID_FENCE;
            if (ctx->dump_fence) {
               ALOGI("fence: %llu/%d - acquire: %d -> wait+close\n",
                     ctx->stats[HWC_PRIMARY_IX].set_call, i,
                     list->hwLayers[i].acquireFenceFd);
            }
         }
         if (pSharedData != NULL) {
            if (hwc_compose_gralloc_buffer(ctx, i, pSharedData, gr_buffer, outputHdl, false,
                                           oob_video, &skip_set, &surface[i], &item->comp[i])) {
               layer_composed++;
            }
            ctx->gpx_cli[i].last.grhdl = (buffer_handle_t)gr_buffer;
            ctx->gpx_cli[i].last.comp_ix = item->comp_ix;
         }
      }
      hwc_mem_unlock(ctx, (unsigned)gr_buffer->sharedData, true);
   }

   if (layer_composed == 0) {
      if (has_video && !ctx->alpha_hole_background) {
         if (skip_set) {
            BKNI_AcquireMutex(ctx->mutex);
            hwc_put_disp_surface(ctx, 0, outputHdl);
            BKNI_ReleaseMutex(ctx->mutex);
            if (list->retireFenceFd != INVALID_FENCE) {
               hwc_inc_retire_timeline(ctx, HWC_PRIMARY_IX, 1, false);
            }
         } else {
            if (!display_surface_seeded) {
               hwc_seed_disp_surface(ctx, outputHdl);
            }
            rc = hwc_checkpoint(ctx);
            if (rc) {
               ALOGW("%s: checkpoint timeout", __FUNCTION__);
            } else {
               rc = NEXUS_SurfaceClient_PushSurface(ctx->disp_cli[HWC_PRIMARY_IX].schdl, outputHdl, NULL, false);
               if (rc) {
                  ALOGW("%s: failed to push surface to client (%d)", __FUNCTION__, rc);
                  if (list->retireFenceFd != INVALID_FENCE) {
                     hwc_inc_retire_timeline(ctx, HWC_PRIMARY_IX, 1, true);
                  }
               } else {
                  ctx->alpha_hole_background = true;
               }
            }
         }
      } else {
         BKNI_AcquireMutex(ctx->mutex);
         hwc_put_disp_surface(ctx, 0, outputHdl);
         BKNI_ReleaseMutex(ctx->mutex);
         if (list->retireFenceFd != INVALID_FENCE) {
            hwc_inc_retire_timeline(ctx, HWC_PRIMARY_IX, 1, false);
         }
      }
   } else {
      ctx->alpha_hole_background = false;
      rc = hwc_checkpoint(ctx);
      if (rc) {
         ALOGW("%s: checkpoint timeout", __FUNCTION__);
      } else {
         rc = NEXUS_SurfaceClient_PushSurface(ctx->disp_cli[HWC_PRIMARY_IX].schdl, outputHdl, NULL, false);
         if (rc) {
            ALOGW("%s: failed to push surface to client (%d)", __FUNCTION__, rc);
            if (list->retireFenceFd != INVALID_FENCE) {
               hwc_inc_retire_timeline(ctx, HWC_PRIMARY_IX, 1, true);
            }
         }
      }
   }

   if (ctx->display_dump_layer) {
      ALOGI("comp: %llu (%llu,%llu): ov:%d, fb:%d - composed:%d (%c%c%c)\n",
            ctx->stats[HWC_PRIMARY_IX].composed, ctx->stats[HWC_PRIMARY_IX].prepare_call,
            ctx->stats[HWC_PRIMARY_IX].set_call, *overlay_seen, *fb_target_seen, layer_composed,
            is_yuv?'y':'-', oob_video?'o':'-', is_sideband?'s':'-');
   }

out:
   for (i = 0; i < list->numHwLayers; i++) {
      if (surface[i]) {
         NEXUS_Surface_Destroy(surface[i]);
      }
      if (list->hwLayers[i].acquireFenceFd != INVALID_FENCE) {
         close(list->hwLayers[i].acquireFenceFd);
         if (ctx->dump_fence) {
            ALOGI("fence: %llu/%d - acquire: %d -> comp+err+close\n",
                  ctx->stats[HWC_PRIMARY_IX].set_call, i,
                  list->hwLayers[i].acquireFenceFd);
         }
      }
      if (list->hwLayers[i].releaseFenceFd != INVALID_FENCE) {
         sw_sync_timeline_inc(list->hwLayers[i].releaseFenceFd, 1);
         close(list->hwLayers[i].releaseFenceFd);
         if (ctx->dump_fence) {
            ALOGI("fence: %llu/%d - timeline: %d -> inc+close\n",
                  ctx->stats[HWC_PRIMARY_IX].set_call, i,
                  list->hwLayers[i].releaseFenceFd);
         }
      }
   }
   return layer_composed;
}

static int hwc_set_virtual(struct hwc_context_t *ctx, hwc_display_contents_1_t* list)
{
    size_t hwc_work_item_size = 0;
    struct hwc_work_item *this_frame = NULL;
    size_t i;

    if (!list) {
       /* !!not an error, just nothing to do.*/
       return 0;
    }

    ctx->stats[HWC_VIRTUAL_IX].set_call += 1;

    if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
       ctx->stats[HWC_VIRTUAL_IX].set_skipped += 1;
       goto out_close_fence;
    }

    if (ctx->display_gles_always || ctx->display_gles_virtual ||
        (ctx->cfg[HWC_VIRTUAL_IX].width == -1 && ctx->cfg[HWC_VIRTUAL_IX].height == -1)) {
       list->retireFenceFd = list->outbufAcquireFenceFd;
       goto out_mutex;
    }

    if (list->outbuf == NULL) {
      ALOGE("vcmp: %llu - unexpected null output target", ctx->stats[HWC_VIRTUAL_IX].set_call);
      ctx->stats[HWC_VIRTUAL_IX].set_skipped += 1;
      goto out_mutex;
    }

    if (BKNI_AcquireMutex(ctx->power_mutex) != BERR_SUCCESS) {
       ALOGE("%s: could not acquire power_mutex!!!", __FUNCTION__);
       ctx->stats[HWC_VIRTUAL_IX].set_skipped += 1;
       goto out_mutex;
    }

    if ((ctx->power_mode != HWC_POWER_MODE_OFF) && (ctx->power_mode != HWC_POWER_MODE_DOZE_SUSPEND)) {
       BKNI_ReleaseMutex(ctx->power_mutex);

       hwc_work_item_size = sizeof(hwc_work_item) + (list->numHwLayers * sizeof(hwc_layer_1_t));
       this_frame = (struct hwc_work_item *) calloc(1, hwc_work_item_size);
       if (this_frame == NULL) {
          goto out_mutex;
       }
       memcpy(&this_frame->content, list,
              sizeof(hwc_display_contents_1_t) + (list->numHwLayers * sizeof(hwc_layer_1_t)));
       for (i = 0; i < list->numHwLayers; i++) {
          memcpy(&this_frame->comp[i], &ctx->vd_cli[i].composition, sizeof(NEXUS_SurfaceComposition));
       }
       this_frame->comp_ix = ctx->stats[HWC_VIRTUAL_IX].set_call;
       this_frame->next = NULL;
       if (ctx->composer_work_list[HWC_VIRTUAL_IX] == NULL) {
          ctx->composer_work_list[HWC_VIRTUAL_IX] = this_frame;
       } else {
          struct hwc_work_item *item, *last;
          last = ctx->composer_work_list[HWC_VIRTUAL_IX];
          while (last != NULL) { item = last; last = last->next; }
          item->next = this_frame;
       }

       if (ctx->track_comp_time) {
          this_frame->tick_queued = hwc_tick();
       }

       if (!ctx->fence_support) {
          list->retireFenceFd = INVALID_FENCE;
          this_frame->content.retireFenceFd = INVALID_FENCE;
          for (i = 0; i < list->numHwLayers; i++) {
             list->hwLayers[i].releaseFenceFd = INVALID_FENCE;
             this_frame->content.hwLayers[i].releaseFenceFd = INVALID_FENCE;
          }
       } else {
          list->retireFenceFd = hwc_retire_fence(ctx, HWC_VIRTUAL_IX);
          this_frame->content.retireFenceFd = list->retireFenceFd;
          if (ctx->dump_fence) {
             ALOGI("vfence: %llu/%d - retire: %d\n",
                ctx->stats[HWC_VIRTUAL_IX].set_call, i, list->retireFenceFd);
          }
          for (i = 0; i < list->numHwLayers; i++) {
             if (ctx->vd_cli[i].composition.visible &&
                 ((list->hwLayers[i].compositionType == HWC_OVERLAY) ||
                  (list->hwLayers[i].compositionType == HWC_FRAMEBUFFER_TARGET))) {
                this_frame->content.hwLayers[i].releaseFenceFd =
                    hwc_release_timeline_with_fence(ctx, HWC_VIRTUAL_IX, i, &list->hwLayers[i].releaseFenceFd);
                if (ctx->dump_fence) {
                   ALOGI("vfence: %llu/%d - timeline: %d -> fence: %d\n",
                      ctx->stats[HWC_VIRTUAL_IX].set_call, i,
                      this_frame->content.hwLayers[i].releaseFenceFd, list->hwLayers[i].releaseFenceFd);
                }
             }
          }
       }

       if (ctx->display_dump_virt) {
          ALOGI("vcmp: %llu (%llu) - queued\n",
                ctx->stats[HWC_VIRTUAL_IX].set_call, ctx->stats[HWC_VIRTUAL_IX].prepare_call);
       }

       if (!ctx->fence_support) {
          BKNI_ReleaseMutex(ctx->mutex);
          BKNI_SetEvent(ctx->composer_event[HWC_VIRTUAL_IX]);
          BKNI_WaitForEvent(ctx->composer_event_sync[HWC_VIRTUAL_IX], BKNI_INFINITE);
          goto out_close_fence;
       } else {
          BKNI_ReleaseMutex(ctx->mutex);
          BKNI_SetEvent(ctx->composer_event[HWC_VIRTUAL_IX]);
          goto out;
       }
    }

out_mutex:
    BKNI_ReleaseMutex(ctx->mutex);
out_close_fence:
    for (i = 0; i < list->numHwLayers; i++) {
       if (list->hwLayers[i].acquireFenceFd >= 0) {
          close(list->hwLayers[i].acquireFenceFd);
          if (ctx->dump_fence) {
             ALOGI("vfence: %llu/%d - acquire: %d -> err+close\n",
                   ctx->stats[HWC_VIRTUAL_IX].set_call, i,
                   list->hwLayers[i].acquireFenceFd);
          }
       }
    }
out:
    return 0;
}

static int hwc_compose_virtual(struct hwc_context_t *ctx, hwc_work_item *item, int *overlay_seen, int *fb_target_seen)
{
   int layer_composed = 0;
   NEXUS_SurfaceHandle outputHdl = NULL;
   NEXUS_Error rc;
   size_t i;
   void *pAddr;
   hwc_display_contents_1_t* list = &item->content;
   NEXUS_SurfaceHandle surface[NSC_GPX_CLIENTS_NUMBER];

   memset(surface, 0, sizeof(surface));

   private_handle_t *gr_out_buffer = (private_handle_t *)list->outbuf;
   hwc_mem_lock(ctx, (unsigned)gr_out_buffer->sharedData, &pAddr, true);
   PSHARED_DATA pOutSharedData = (PSHARED_DATA) pAddr;
   if (pOutSharedData == NULL) {
      ALOGE("vcmp: %llu (%p) - invalid output buffer?", ctx->stats[HWC_VIRTUAL_IX].set_call, gr_out_buffer->sharedData);
      ctx->stats[HWC_VIRTUAL_IX].set_skipped += 1;
      hwc_mem_unlock(ctx, (unsigned)gr_out_buffer->sharedData, true);
      goto out;
   }
   outputHdl = hwc_to_nsc_surface(pOutSharedData->planes[DEFAULT_PLANE].width,
                                  pOutSharedData->planes[DEFAULT_PLANE].height,
                                  pOutSharedData->planes[DEFAULT_PLANE].stride,
                                  gralloc_to_nexus_pixel_format(pOutSharedData->planes[DEFAULT_PLANE].format),
                                  ctx->hwc_with_mma,
                                  pOutSharedData->planes[DEFAULT_PLANE].physAddr,
                                  (uint8_t *)pAddr);
   if (outputHdl == NULL) {
      ALOGE("vcmp: %llu (%p) - no display surface available", ctx->stats[HWC_VIRTUAL_IX].set_call, gr_out_buffer->sharedData);
      ctx->stats[HWC_VIRTUAL_IX].set_skipped += 1;
      hwc_mem_unlock(ctx, (unsigned)gr_out_buffer->sharedData, true);
      goto out;
   }

   for (i = 0; i < list->numHwLayers; i++) {
      if (list->hwLayers[i].compositionType == HWC_OVERLAY) {
         *overlay_seen += 1;
      } else if (list->hwLayers[i].compositionType == HWC_FRAMEBUFFER_TARGET) {
         *fb_target_seen += 1;
      }
      if (item->comp[i].visible) {
         if (i > HWC_VD_CLIENTS_NUMBER) {
            break;
         }
         private_handle_t *gr_buffer = (private_handle_t *)list->hwLayers[i].handle;
         if (gr_buffer == NULL) {
            continue;
         }
         hwc_mem_lock(ctx, (unsigned)gr_buffer->sharedData, &pAddr, true);
         PSHARED_DATA pSharedData = (PSHARED_DATA) pAddr;
         if (list->hwLayers[i].acquireFenceFd >= 0) {
            if (!ctx->fence_support) {
               ALOGI("vfence: %llu/%d - fd:%d -> wait+close in sync mode!?! check platform setup.\n",
                     ctx->stats[HWC_VIRTUAL_IX].set_call, i,
                     list->hwLayers[i].acquireFenceFd);
            }
            sync_wait(list->hwLayers[i].acquireFenceFd, BKNI_INFINITE);
            close(list->hwLayers[i].acquireFenceFd);
            list->hwLayers[i].acquireFenceFd = INVALID_FENCE;
            if (ctx->dump_fence) {
               ALOGI("vfence: %llu/%d - acquire: %d -> wait+close\n",
                     ctx->stats[HWC_VIRTUAL_IX].set_call, i,
                     list->hwLayers[i].acquireFenceFd);
            }
         }
         if (pSharedData == NULL) {
            hwc_mem_unlock(ctx, (unsigned)gr_buffer->sharedData, true);
            continue;
         }
         if (hwc_compose_gralloc_buffer(ctx, i, pSharedData, gr_buffer, outputHdl,
                                        true, false, NULL, &surface[i], &item->comp[i])) {
            layer_composed++;
         }
         hwc_mem_unlock(ctx, (unsigned)gr_buffer->sharedData, true);
      }
   }

   if (layer_composed) {
      if (ctx->fence_support && (list->outbufAcquireFenceFd >= 0)) {
         sync_wait(list->outbufAcquireFenceFd, BKNI_INFINITE);
         close(list->outbufAcquireFenceFd);
         list->outbufAcquireFenceFd = INVALID_FENCE;
      }
      rc = hwc_checkpoint(ctx);
      if (rc)  {
         ALOGW("vcmp: checkpoint timeout");
      }
      for (i = 0; i < list->numHwLayers; i++) {
         if (surface[i]) {
            NEXUS_Surface_Destroy(surface[i]);
         }
      }
   }
   hwc_mem_unlock(ctx, (unsigned)gr_out_buffer->sharedData, true);
   if (outputHdl) {
      NEXUS_Surface_Destroy(outputHdl);
   }

   if (ctx->display_dump_virt) {
      ALOGI("vcmp: %llu (%llu,%llu): ov:%d, fb:%d - composed:%d\n",
            ctx->stats[HWC_VIRTUAL_IX].composed, ctx->stats[HWC_VIRTUAL_IX].prepare_call,
            ctx->stats[HWC_VIRTUAL_IX].set_call, *overlay_seen, *fb_target_seen, layer_composed);
   }

out:
   if (list->outbufAcquireFenceFd >= 0) {
      close(list->outbufAcquireFenceFd);
      list->outbufAcquireFenceFd = INVALID_FENCE;
   }
   for (i = 0; i < list->numHwLayers; i++) {
      if (list->hwLayers[i].acquireFenceFd != INVALID_FENCE) {
         close(list->hwLayers[i].acquireFenceFd);
         if (ctx->dump_fence) {
            ALOGI("vfence: %llu/%d - acquire: %d -> comp+err+close\n",
                  ctx->stats[HWC_VIRTUAL_IX].set_call, i,
                  list->hwLayers[i].acquireFenceFd);
         }
      }
      if (list->hwLayers[i].releaseFenceFd != INVALID_FENCE) {
         sw_sync_timeline_inc(list->hwLayers[i].releaseFenceFd, 1);
         close(list->hwLayers[i].releaseFenceFd);
         if (ctx->dump_fence) {
            ALOGI("vfence: %llu/%d - timeline: %d -> inc+close\n",
                  ctx->stats[HWC_VIRTUAL_IX].set_call, i,
                  list->hwLayers[i].releaseFenceFd);
         }
      }
   }
   if (list->retireFenceFd != INVALID_FENCE) {
      hwc_inc_retire_timeline(ctx, HWC_VIRTUAL_IX, 1, false);
   }

   return layer_composed;
}

static int hwc_set(hwc_composer_device_1_t *dev,
        size_t numDisplays, hwc_display_contents_1_t** displays)
{
    struct hwc_context_t *ctx = (struct hwc_context_t*)dev;
    uint32_t i;
    int rc = 0;

    if (!numDisplays) {
        rc = -EINVAL;
        goto out;
    }

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

out:
    if (rc) {
       ALOGE("%s: failure %d", __FUNCTION__, rc);
    }
    return rc;
}

static void hwc_device_cleanup(struct hwc_context_t* ctx)
{
    int i, j;

    if (ctx) {
        ctx->vsync_thread_run = 0;
        pthread_join(ctx->vsync_callback_thread, NULL);

        if (ctx->pIpcClient != NULL) {
            if (ctx->pNexusClientContext != NULL) {
                ctx->pIpcClient->destroyClientContext(ctx->pNexusClientContext);
            }
            delete ctx->pIpcClient;
        }

        for (i = 0; i < NSC_SB_CLIENTS_NUMBER; i++) {
           /* nothing to do since we do not acquire the surfaces explicitely. */
        }

        for (i = 0; i < NSC_MM_CLIENTS_NUMBER; i++) {
           /* nothing to do since we do not acquire the surfaces explicitely. */
        }

        for (i = 0; i < DISPLAY_SUPPORTED; i++) {
           ctx->composer_thread_run[i] = 0;
           if (ctx->disp_cli[i].schdl) {
              NEXUS_SurfaceClient_Release(ctx->disp_cli[i].schdl);
           }
           pthread_join(ctx->composer_thread[i], NULL);
           if (ctx->composer_event[i]) {
              BKNI_DestroyEvent(ctx->composer_event[i]);
           }
           if (ctx->composer_event_sync[i]) {
              BKNI_DestroyEvent(ctx->composer_event_sync[i]);
           }
           if (ctx->composer_event_forced[i]) {
              BKNI_DestroyEvent(ctx->composer_event_forced[i]);
           }
           if (ctx->composer_ret_timeline[i] != INVALID_FENCE) {
              close(ctx->composer_ret_timeline[i]);
              ctx->composer_ret_timeline[i] = INVALID_FENCE;
           }
        }
        if (ctx->recycle_event) {
           BKNI_DestroyEvent(ctx->recycle_event);
        }

        NxClient_Free(&ctx->nxAllocResults);

        BKNI_DestroyMutex(ctx->vsync_callback_enabled_mutex);
        BKNI_DestroyMutex(ctx->mutex);
        BKNI_DestroyMutex(ctx->power_mutex);
        BKNI_DestroyMutex(ctx->dev_mutex);
        BKNI_DestroyMutex(ctx->g2d_mutex);

        if (ctx->hwc_binder) {
           delete ctx->hwc_binder;
        }

        if (ctx->hwc_g2d) {
           NEXUS_Graphics2D_Close(ctx->hwc_g2d);
        }
        if (ctx->checkpoint_event) {
           BKNI_DestroyEvent(ctx->checkpoint_event);
        }
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
    NEXUS_CallbackDesc desc;

    if (disp == HWC_DISPLAY_PRIMARY) {
       ALOGI("%s: %s->%s", __FUNCTION__,
           hwc_power_mode[ctx->power_mode], hwc_power_mode[mode]);

       switch (mode) {
         case HWC_POWER_MODE_OFF:
         case HWC_POWER_MODE_DOZE:
         case HWC_POWER_MODE_DOZE_SUSPEND:
             break;
         case HWC_POWER_MODE_NORMAL:
             if (!ctx->vsync_callback_initialized)
             {
                 desc.callback = hw_vsync_cb;
                 desc.context = (void *)&ctx->syn_cli;
                 NEXUS_Display_SetVsyncCallback(ctx->display_handle, &desc);
                 ctx->vsync_callback_initialized = true;
             }
             break;
         default:
            goto out;
       }

       if (BKNI_AcquireMutex(ctx->power_mutex) != BERR_SUCCESS) {
           ALOGE("%s: Could not acquire power_mutex!!!", __FUNCTION__);
           goto out;
       }
       if (ctx->power_mode != mode) {
          ctx->power_mode = mode;
       }
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
           BKNI_ReleaseMutex(ctx->vsync_callback_enabled_mutex);
        }
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
                      values[i] = ctx->cfg[HWC_PRIMARY_IX].width;
                   break;
                   case HWC_DISPLAY_HEIGHT:
                      values[i] = ctx->cfg[HWC_PRIMARY_IX].height;
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
       NEXUS_SurfaceCursorHandle cursor = NULL;
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

#define VSYNC_HACK_REFRESH_COUNT 10
static void * hwc_vsync_task(void *argv)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)argv;
    const double nsec_to_msec = 1.0 / 1000000.0;
    int64_t vsync_system_time;
    unsigned int vsync_hack_count = 0;
    unsigned long long vsync_hack_count_last_tick = 0;
    unsigned long long vsync_hack_count_expected_tick = 0;

    do
    {
       if (BKNI_AcquireMutex(ctx->power_mutex) != BERR_SUCCESS) {
          ALOGE("%s: Could not acquire power_mutex!!!", __FUNCTION__);
          goto out;
       }
       if ((ctx->power_mode != HWC_POWER_MODE_OFF) && (ctx->power_mode != HWC_POWER_MODE_DOZE_SUSPEND)) {
          BKNI_ReleaseMutex(ctx->power_mutex);
          BKNI_WaitForEvent(ctx->syn_cli.vsync_event, BKNI_INFINITE);

          vsync_system_time = hwc_tick();

          if (HWC_REFRESH_HACK) {
             vsync_hack_count++;
             if (vsync_hack_count_expected_tick == 0) {
                vsync_hack_count_expected_tick = ctx->stats[HWC_PRIMARY_IX].composed + 1;
             }
             if (vsync_hack_count >= VSYNC_HACK_REFRESH_COUNT/2) {
                vsync_hack_count_last_tick = ctx->stats[HWC_PRIMARY_IX].composed;
                if (vsync_hack_count_expected_tick <= vsync_hack_count_last_tick) {
                   vsync_hack_count = 0; /* reset */
                   vsync_hack_count_expected_tick = ctx->stats[HWC_PRIMARY_IX].composed + 1;
                }
             } else if (vsync_hack_count >= VSYNC_HACK_REFRESH_COUNT) {
                 vsync_hack_count = 0;
                 if (vsync_hack_count_last_tick == ctx->stats[HWC_PRIMARY_IX].composed) {
                    ALOGV("hack-refresh: tick %llu", vsync_hack_count_expected_tick);
                    if (ctx->procs->invalidate != NULL) {
                       ctx->procs->invalidate(const_cast<hwc_procs_t *>(ctx->procs));
                    }
                    vsync_hack_count_expected_tick = 0;
                 }
             }
         }

         if (BKNI_AcquireMutex(ctx->vsync_callback_enabled_mutex) == BERR_SUCCESS) {
            if (ctx->vsync_callback_enabled && ctx->procs->vsync != NULL) {
               BKNI_ReleaseMutex(ctx->vsync_callback_enabled_mutex);
               ctx->procs->vsync(const_cast<hwc_procs_t *>(ctx->procs), 0, vsync_system_time);
               if (ctx->display_dump_vsync) {
                  ALOGI("vsync-pushed: @ %lld", vsync_system_time);
               }
            } else {
               BKNI_ReleaseMutex(ctx->vsync_callback_enabled_mutex);
               if (ctx->display_dump_vsync) {
                  ALOGI("vsync-ticked: @ %lld", vsync_system_time);
               }
            }
         }

      } else {
         BKNI_ReleaseMutex(ctx->power_mutex);
         BKNI_Sleep(16);
      }

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

static void hwc_collect_composer(struct hwc_context_t *ctx, struct hwc_work_item *this_frame,
   int index, int overlay_seen, int fb_target_seen, int layer_composed)
{
   const long double nsec_to_msec = 1.0 / 1000000.0;
   long double comp_time;

   if (this_frame->tick_queued) {
      int64_t tick_composed = hwc_tick();

      comp_time = nsec_to_msec * (tick_composed - this_frame->tick_queued);

      if (ctx->display_dump_layer ||
          (ctx->track_comp_chatty && ((tick_composed - this_frame->tick_queued) > VSYNC_CLIENT_REFRESH))) {
         ALOGI("%s: %llu: ov:%d, fb:%d, comp:%d, time:%.3f msecs => %s\n",
               (index == HWC_PRIMARY_IX) ? "comp" : "vcmp",
               this_frame->comp_ix, overlay_seen, fb_target_seen, layer_composed,
               comp_time, ((tick_composed - this_frame->tick_queued) > VSYNC_CLIENT_REFRESH) ? "slow" : "on-time");
      }

      if (layer_composed) {
         ctx->time_track[index].tracked++;
         if ((tick_composed - this_frame->tick_queued) > VSYNC_CLIENT_REFRESH) {
            ctx->time_track[index].slow++;
         } else {
            ctx->time_track[index].ontime++;
         }
         if ((comp_time > 0) && (comp_time > ctx->time_track[index].slowest)) {
            ctx->time_track[index].slowest = comp_time;
         }
         if (ctx->time_track[index].fastest == 0) {
            ctx->time_track[index].fastest = comp_time;
         }
         if ((comp_time > 0) && (comp_time < ctx->time_track[index].fastest)) {
            ctx->time_track[index].fastest = comp_time;
         }
         ctx->time_track[index].cumul += comp_time;
      }
   }
}

static void * hwc_compose_task_primary(void *argv)
{
   struct hwc_context_t* ctx = (struct hwc_context_t*)argv;
   struct hwc_work_item *this_frame = NULL;
   int layer_composed, overlay_seen, fb_target_seen;

   ALOGI("%s: starting primary composer loop.", __FUNCTION__);

   do
   {
      if (BKNI_AcquireMutex(ctx->power_mutex) != BERR_SUCCESS) {
         ALOGE("%s: could not acquire power_mutex!!!", __FUNCTION__);
         goto out;
      }
      if ((ctx->power_mode != HWC_POWER_MODE_OFF) && (ctx->power_mode != HWC_POWER_MODE_DOZE_SUSPEND)) {
         BKNI_ReleaseMutex(ctx->power_mutex);
         BKNI_WaitForEvent(ctx->composer_event[HWC_PRIMARY_IX], BKNI_INFINITE);
         bool try_compose = true, release_forced = false;

         do {
            this_frame = NULL;
            if (BKNI_AcquireMutex(ctx->mutex) == BERR_SUCCESS) {
               this_frame = ctx->composer_work_list[HWC_PRIMARY_IX];
               if (this_frame == NULL) {
                  try_compose = false;
               } else {
                  ctx->composer_work_list[HWC_PRIMARY_IX] = this_frame->next;
                  if (ctx->stats[HWC_PRIMARY_IX].set_call >= ctx->stats[HWC_PRIMARY_IX].composed + HWC_SET_COMP_THRESHOLD) {
                     release_forced = true;
                  }
                  ctx->stats[HWC_PRIMARY_IX].composed++;
               }
               BKNI_ReleaseMutex(ctx->mutex);
            } else {
               ALOGE("%s: could not acquire mutex!!!", __FUNCTION__);
               try_compose = false;
            }

            if (this_frame != NULL) {
               overlay_seen = 0; fb_target_seen = 0;
               layer_composed = hwc_compose_primary(ctx, this_frame, &overlay_seen, &fb_target_seen);
               hwc_collect_composer(ctx, this_frame, HWC_PRIMARY_IX, overlay_seen, fb_target_seen, layer_composed);
               free(this_frame);
               this_frame = NULL;
            }

            if (release_forced) {
               release_forced = false;
               BKNI_SetEvent(ctx->composer_event_forced[HWC_PRIMARY_IX]);
            }

         } while (try_compose);

         if (!ctx->fence_support) {
            BKNI_SetEvent(ctx->composer_event_sync[HWC_PRIMARY_IX]);
         }

      } else {
         BKNI_ReleaseMutex(ctx->power_mutex);
         BKNI_Sleep(16);
      }

   } while(ctx->composer_thread_run[HWC_PRIMARY_IX]);

out:
   ALOGE("%s: primary composer loop - EXIT!", __FUNCTION__);
   return NULL;
}

static void * hwc_compose_task_virtual(void *argv)
{
   struct hwc_context_t* ctx = (struct hwc_context_t*)argv;
   struct hwc_work_item *this_frame = NULL;
   int layer_composed, overlay_seen, fb_target_seen;

   ALOGI("%s: starting virtual composer loop.", __FUNCTION__);

   do
   {
      if (BKNI_AcquireMutex(ctx->power_mutex) != BERR_SUCCESS) {
         ALOGE("%s: could not acquire power_mutex!!!", __FUNCTION__);
         goto out;
      }
      if ((ctx->power_mode != HWC_POWER_MODE_OFF) && (ctx->power_mode != HWC_POWER_MODE_DOZE_SUSPEND)) {
         BKNI_ReleaseMutex(ctx->power_mutex);
         BKNI_WaitForEvent(ctx->composer_event[HWC_VIRTUAL_IX], BKNI_INFINITE);
         bool try_compose = true;

         do {
            this_frame = NULL;
            if (BKNI_AcquireMutex(ctx->mutex) == BERR_SUCCESS) {
               this_frame = ctx->composer_work_list[HWC_VIRTUAL_IX];
               if (this_frame == NULL) {
                  try_compose = false;
               } else {
                  ctx->composer_work_list[HWC_VIRTUAL_IX] = this_frame->next;
                  ctx->stats[HWC_VIRTUAL_IX].composed++;
               }
               BKNI_ReleaseMutex(ctx->mutex);
            } else {
               ALOGE("%s: could not acquire mutex!!!", __FUNCTION__);
               try_compose = false;
            }

            if (this_frame != NULL) {
               overlay_seen = 0; fb_target_seen = 0;
               layer_composed = hwc_compose_virtual(ctx, this_frame, &overlay_seen, &fb_target_seen);
               hwc_collect_composer(ctx, this_frame, HWC_VIRTUAL_IX, overlay_seen, fb_target_seen, layer_composed);
               free(this_frame);
               this_frame = NULL;
            }
         } while (try_compose);

         if (!ctx->fence_support) {
            BKNI_SetEvent(ctx->composer_event_sync[HWC_VIRTUAL_IX]);
         }

      } else {
         BKNI_ReleaseMutex(ctx->power_mutex);
         BKNI_Sleep(16);
      }

   } while(ctx->composer_thread_run[HWC_VIRTUAL_IX]);

out:
   ALOGE("%s: virtual composer loop - EXIT!", __FUNCTION__);
   return NULL;
}

static void hwc_read_dev_props(struct hwc_context_t* dev)
{
   char value[PROPERTY_VALUE_MAX];

   if (dev == NULL) {
      return;
   }

   dev->cfg[HWC_PRIMARY_IX].width = property_get_int32(GRAPHICS_RES_WIDTH_PROP, GRAPHICS_RES_WIDTH_DEFAULT);
   dev->cfg[HWC_PRIMARY_IX].height = property_get_int32(GRAPHICS_RES_HEIGHT_PROP, GRAPHICS_RES_HEIGHT_DEFAULT);

   dev->cfg[HWC_VIRTUAL_IX].width = -1;
   dev->cfg[HWC_VIRTUAL_IX].height = -1;

   dev->alpha_hole_background = false;

   if (property_get(HWC_GLES_MODE_PROP, value, HWC_DEFAULT_GLES_MODE)) {
      dev->display_gles_fallback =
         (strncmp(value, HWC_GLES_MODE_FALLBACK, strlen(HWC_GLES_MODE_FALLBACK)) == 0) ? true : false;
      dev->display_gles_always =
         (strncmp(value, HWC_GLES_MODE_ALWAYS, strlen(HWC_GLES_MODE_ALWAYS)) == 0) ? true : false;
   }

   if (property_get(HWC_GLES_VIRTUAL_PROP, value, HWC_DEFAULT_ENABLED)) {
      dev->display_gles_virtual = (strtoul(value, NULL, 10) == 0) ? false : true;
   }

   if (property_get(HWC_DUMP_LAYER_PROP, value, HWC_DEFAULT_DISABLED)) {
      dev->display_dump_layer = (strtoul(value, NULL, 10) == 0) ? false : true;
   }

   if (property_get(HWC_DUMP_VSYNC_PROP, value, HWC_DEFAULT_DISABLED)) {
      dev->display_dump_vsync = (strtoul(value, NULL, 10) == 0) ? false : true;
   }

   if (property_get(HWC_NSC_COPY_PROP, value, HWC_DEFAULT_DISABLED)) {
      dev->nsc_copy = (strtoul(value, NULL, 10) == 0) ? false : true;
   }

   if (property_get(HWC_TRACK_BUFFER_PROP, value, HWC_DEFAULT_DISABLED)) {
      dev->track_buffer = (strtoul(value, NULL, 10) == 0) ? false : true;
   }

   if (property_get(HWC_DUMP_VIRT_PROP, value, HWC_DEFAULT_DISABLED)) {
      dev->display_dump_virt = (strtoul(value, NULL, 10) == 0) ? false : true;
   }

   if (property_get(HWC_USES_MMA_PROP, value, HWC_DEFAULT_DISABLED)) {
      dev->hwc_with_mma = (strtoul(value, NULL, 10) > 0) ? 1 : 0;
   }

   if (property_get(HWC_DUMP_MMA_OPS_PROP, value, HWC_DEFAULT_DISABLED)) {
      dev->dump_mma = (strtoul(value, NULL, 10) > 0) ? true : false;
   }

   if (property_get(HWC_DUMP_FENCE_PROP, value, HWC_DEFAULT_DISABLED)) {
      dev->dump_fence = (strtoul(value, NULL, 10) > 0) ? true : false;
   }

   if (property_get(HWC_SKIP_DUP_PROP, value, HWC_DEFAULT_DISABLED)) {
      dev->skip_dup = (strtoul(value, NULL, 10) > 0) ? 1 : 0;
   }

   dev->fence_support = property_get_bool(HWC_WITH_FENCE_PROP, 0);

   if (property_get(HWC_TRACK_COMP_TIME, value, HWC_DEFAULT_ENABLED)) {
      dev->track_comp_time = (strtoul(value, NULL, 10) > 0) ? true : false;
   }

   if (property_get(HWC_TRACK_COMP_CHATTY, value, HWC_DEFAULT_DISABLED)) {
      dev->track_comp_chatty = (strtoul(value, NULL, 10) > 0) ? true : false;
   }
}

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = -EINVAL;
    struct hwc_context_t *dev = NULL;
    NxClient_AllocSettings nxAllocSettings;
    NEXUS_SurfaceClientSettings client_settings;
    NEXUS_SurfaceComposition composition;
    NEXUS_ClientConfiguration clientConfig;
    int i, j;

    if (!strcmp(name, HWC_HARDWARE_COMPOSER)) {
        NEXUS_Error rc = NEXUS_SUCCESS;
        NEXUS_Graphics2DSettings gfxSettings;
        pthread_attr_t attr;
        int thread_policy;
        struct sched_param thread_sched_param;
        dev = (struct hwc_context_t*)calloc(1, sizeof(*dev));

        if (dev == NULL) {
           ALOGE("Failed to create hwcomposer!");
           goto out;
        }

        hwc_read_dev_props(dev);

        if ( BKNI_CreateEvent(&dev->recycle_event) ) {
           goto clean_up;
        }
        if (BKNI_CreateMutex(&dev->vsync_callback_enabled_mutex) == BERR_OS_ERROR) {
           goto clean_up;
        }
        if (BKNI_CreateMutex(&dev->mutex) == BERR_OS_ERROR) {
           goto clean_up;
        }
        if (BKNI_CreateMutex(&dev->dev_mutex) == BERR_OS_ERROR) {
           goto clean_up;
        }
        if (BKNI_CreateMutex(&dev->power_mutex) == BERR_OS_ERROR) {
           goto clean_up;
        }
        if (BKNI_CreateMutex(&dev->g2d_mutex) == BERR_OS_ERROR) {
           goto clean_up;
        }

        NEXUS_Platform_GetClientConfiguration(&clientConfig);
        NxClient_GetDefaultAllocSettings(&nxAllocSettings);
        nxAllocSettings.surfaceClient = DISPLAY_SUPPORTED;
        rc = NxClient_Alloc(&nxAllocSettings, &dev->nxAllocResults);
        if (rc) {
           ALOGE("%s: failed NxClient_Alloc (rc=%d)!", __FUNCTION__, rc);
           goto clean_up;
        }

        for (i = 0; i < DISPLAY_SUPPORTED; i++) {
            dev->disp_cli[i].sccid = dev->nxAllocResults.surfaceClient[i].id;
            dev->composer_thread_run[i] = 1;
            dev->composer_work_list[i] = NULL;
            if (dev->fence_support) {
               dev->composer_ret_timeline[i] = sw_sync_timeline_create();
               if (dev->composer_ret_timeline[i] < 0) {
                  dev->composer_ret_timeline[i] = INVALID_FENCE;
               }
            }
            rc = BKNI_CreateEvent(&dev->composer_event[i]);
            if (rc) {
               ALOGE("%s: unable to create composer event", __FUNCTION__, i);
               goto clean_up;
            }
            rc = BKNI_CreateEvent(&dev->composer_event_sync[i]);
            if (rc) {
               ALOGE("%s: unable to create composer sync event", __FUNCTION__, i);
               goto clean_up;
            }
            rc = BKNI_CreateEvent(&dev->composer_event_forced[i]);
            if (rc) {
               ALOGE("%s: unable to create composer forced event", __FUNCTION__, i);
               goto clean_up;
            }
        }

        for (i = 0; i < (DISPLAY_SUPPORTED-1); i++) {
            dev->disp_cli[i].schdl = NEXUS_SurfaceClient_Acquire(dev->disp_cli[i].sccid);
            if (NULL == dev->disp_cli[i].schdl) {
               ALOGE("%s: unable to allocate surface client", __FUNCTION__);
               goto clean_up;
            }
            NEXUS_SurfaceClient_GetSettings(dev->disp_cli[i].schdl, &client_settings);
            client_settings.recycled.callback = hwc_nsc_recycled_cb;
            client_settings.recycled.context = dev;
            rc = NEXUS_SurfaceClient_SetSettings(dev->disp_cli[i].schdl, &client_settings);
            if (rc) {
               ALOGE("%s: unable to setup surface client", __FUNCTION__);
               goto clean_up;
            }
            NxClient_GetSurfaceClientComposition(dev->disp_cli[i].sccid, &composition);
            composition.virtualDisplay.width = dev->cfg[i].width;
            composition.virtualDisplay.height = dev->cfg[i].height;
            composition.position.x = 0;
            composition.position.y = 0;
            composition.position.width = dev->cfg[i].width;
            composition.position.height = dev->cfg[i].height;
            composition.zorder = GPX_CLIENT_ZORDER;
            composition.visible = (i == 0) ? true : false;
            composition.colorBlend = colorBlendingEquation[BLENDIND_TYPE_SRC_OVER];
            composition.alphaBlend = alphaBlendingEquation[BLENDIND_TYPE_SRC_OVER];
            rc = NxClient_SetSurfaceClientComposition(dev->disp_cli[i].sccid, &composition);
            if (rc) {
               ALOGE("%s: unable to set client composition %d", __FUNCTION__, rc);
               goto clean_up;
            }
            for (j = 0; j < HWC_NUM_DISP_BUFFERS; j++) {
               NEXUS_SurfaceCreateSettings surfaceCreateSettings;
               NEXUS_Surface_GetDefaultCreateSettings(&surfaceCreateSettings);
               surfaceCreateSettings.width = dev->cfg[i].width;
               surfaceCreateSettings.height = dev->cfg[i].height;
               surfaceCreateSettings.pixelFormat = NEXUS_PixelFormat_eA8_B8_G8_R8;
               if (dev->hwc_with_mma) {
                  surfaceCreateSettings.heap = clientConfig.heap[NXCLIENT_DYNAMIC_HEAP];
               }
               dev->disp_cli[i].display_buffers[j] = hwc_surface_create(&surfaceCreateSettings, dev->hwc_with_mma);
               if (dev->disp_cli[i].display_buffers[j] == NULL) {
                  goto clean_up;
               }
            }
            BFIFO_INIT(&dev->disp_cli[i].display_fifo, dev->disp_cli[i].display_buffers, HWC_NUM_DISP_BUFFERS);
            BFIFO_WRITE_COMMIT(&dev->disp_cli[i].display_fifo, HWC_NUM_DISP_BUFFERS);
        }

        BKNI_CreateEvent(&dev->syn_cli.vsync_event);
        dev->syn_cli.refresh       = VSYNC_CLIENT_REFRESH;
        dev->syn_cli.ncci.type     = NEXUS_CLIENT_VSYNC;
        dev->syn_cli.ncci.parent   = (void *)dev;

        for (i = 0; i < NSC_GPX_CLIENTS_NUMBER; i++) {
            dev->gpx_cli[i].ncci.parent = (void *)dev;
            dev->gpx_cli[i].layer_id = i;
            dev->gpx_cli[i].ncci.type = NEXUS_CLIENT_GPX;
            NxClient_GetSurfaceClientComposition(dev->disp_cli[HWC_PRIMARY_IX].sccid, &dev->gpx_cli[i].composition);
        }

        for (i = 0; i < NSC_MM_CLIENTS_NUMBER; i++) {
            dev->mm_cli[i].root.ncci.parent = (void *)dev;
            dev->mm_cli[i].root.layer_id = i;
            dev->mm_cli[i].root.ncci.type = NEXUS_CLIENT_MM;
            dev->mm_cli[i].id = INVALID_HANDLE + i;
            memset(&dev->mm_cli[i].root.composition, 0, sizeof(dev->mm_cli[i].root.composition));
            dev->mm_cli[i].last_ping_frame_id = LAST_PING_FRAME_ID_INVALID;

            // TODO: add support for more than one video at the time.
            if (i == 0) {
               dev->nsc_video_changed = true;
            }
        }

        for (i = 0; i < NSC_SB_CLIENTS_NUMBER; i++) {
            dev->sb_cli[i].root.ncci.parent = (void *)dev;
            dev->sb_cli[i].root.layer_id = i;
            dev->sb_cli[i].root.ncci.type = NEXUS_CLIENT_SB;
            dev->sb_cli[i].id = INVALID_HANDLE + i;
            memset(&dev->sb_cli[i].root.composition, 0, sizeof(dev->sb_cli[i].root.composition));

            // TODO: add support for more than one sideband at the time.
            if (i == 0) {
               dev->nsc_sideband_changed = true;
            }
        }

        for (i = 0; i < HWC_VD_CLIENTS_NUMBER; i++) {
            dev->vd_cli[i].ncci.parent = (void *)dev;
            dev->vd_cli[i].ncci.type = NEXUS_CLIENT_VD;
            NxClient_GetSurfaceClientComposition(dev->disp_cli[HWC_VIRTUAL_IX].sccid, &dev->vd_cli[i].composition);
            if (!i) {
               dev->vd_cli[i].composition.visible = false;
               NxClient_SetSurfaceClientComposition(dev->disp_cli[HWC_VIRTUAL_IX].sccid, &dev->vd_cli[i].composition);
            }
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

        dev->vsync_thread_run = 1;
        dev->display_handle = NULL;

        dev->pIpcClient = NexusIPCClientFactory::getClient("hwc");
        if (dev->pIpcClient == NULL) {
            ALOGE("%s: failed NexusIPCClientFactory::getClient", __FUNCTION__);
        } else {
            b_refsw_client_client_configuration clientConfig;

            memset(&clientConfig, 0, sizeof(clientConfig));
            clientConfig.standbyMonitorCallback = hwc_standby_monitor;
            clientConfig.standbyMonitorContext  = dev;

            dev->pNexusClientContext = dev->pIpcClient->createClientContext(&clientConfig);
            if (dev->pNexusClientContext == NULL) {
               ALOGE("%s: failed createClientContext", __FUNCTION__);
            } else {
               NEXUS_InterfaceName interfaceName;
               NEXUS_PlatformObjectInstance objects[NEXUS_DISPLAY_OBJECTS]; /* won't overflow. */
               size_t num = NEXUS_DISPLAY_OBJECTS;
               NEXUS_Error nrc;
               strcpy(interfaceName.name, "NEXUS_Display");
               nrc = NEXUS_Platform_GetObjects(&interfaceName, &objects[0], num, &num);
               if (nrc == NEXUS_SUCCESS) {
                  ALOGD("%s: display handle is %p", __FUNCTION__, objects[0].object);
                  dev->display_handle = (NEXUS_DisplayHandle)objects[0].object;
               } else {
                  ALOGE("%s: failed to get display handle, sync will not work.", __FUNCTION__);
               }
            }
        }

        pthread_attr_init(&attr);
        attr.sched_policy = SCHED_FIFO;
        attr.sched_priority = sched_get_priority_max(SCHED_FIFO);
        if (pthread_create(&dev->vsync_callback_thread, &attr, hwc_vsync_task, (void *)dev) != 0) {
            ALOGE("%s: unable to create vsync thread", __FUNCTION__, i);
            goto clean_up;
        }
        pthread_attr_destroy(&attr);
        pthread_setname_np(dev->vsync_callback_thread, "nx_sync_cb");
        status = pthread_getschedparam(dev->vsync_callback_thread, &thread_policy, &thread_sched_param);
        ALOGI("%s: pthread_getschedparam(%d,%d) on vsync thread: %d", __FUNCTION__,
              thread_policy, thread_sched_param.sched_priority, status);

        for (i = 0; i < DISPLAY_SUPPORTED; i++) {
            pthread_attr_init(&attr);
            attr.sched_policy = SCHED_FIFO;
            attr.sched_priority = sched_get_priority_max(SCHED_FIFO);
            if (pthread_create(&dev->composer_thread[i], &attr, hwc_compose_task_item[i], (void *)dev) != 0) {
                ALOGE("%s: unable to create composer thread", __FUNCTION__, i);
                goto clean_up;
            }
            pthread_attr_destroy(&attr);
            pthread_setname_np(dev->composer_thread[i], (i==0)?"hwc_comp_prim":"hwc_comp_virt");
            pthread_getschedparam(dev->composer_thread[i], &thread_policy, &thread_sched_param);
            status = pthread_getschedparam(dev->composer_thread[i], &thread_policy, &thread_sched_param);
            ALOGI("%s: pthread_getschedparam(%d,%d) on %s composer thread: %d", __FUNCTION__,
                  thread_policy, thread_sched_param.sched_priority, (i==0)?"primary":"virtual", status);
        }

        dev->hwc_binder = new HwcBinder_wrap;
        if (dev->hwc_binder == NULL) {
           ALOGE("%s: failed to create hwcbinder, some services will not run!", __FUNCTION__);
        } else {
           dev->hwc_binder->get()->register_notify(&hwc_binder_notify, (int)dev);
           ALOGE("%s: created hwcbinder (%p)", __FUNCTION__, dev->hwc_binder);
        }

        rc = BKNI_CreateEvent(&dev->checkpoint_event);
        if (rc) {
          ALOGE("%s: unable to create checkpoint event", __FUNCTION__);
          goto clean_up;
        }

        NEXUS_Graphics2DOpenSettings g2dOpenSettings;
        NEXUS_Graphics2D_GetDefaultOpenSettings(&g2dOpenSettings);
        g2dOpenSettings.compatibleWithSurfaceCompaction = false;
        dev->hwc_g2d = NEXUS_Graphics2D_Open(NEXUS_ANY_ID, &g2dOpenSettings);
        if (dev->hwc_g2d == NULL) {
           ALOGE("%s: failed to create hwc_g2d, composition will not work!", __FUNCTION__);
        } else {
           NEXUS_Graphics2D_GetSettings(dev->hwc_g2d, &gfxSettings);
           gfxSettings.pollingCheckpoint = false;
           gfxSettings.checkpointCallback.callback = hwc_checkpoint_callback;
           gfxSettings.checkpointCallback.context = (void *)dev;
           rc = NEXUS_Graphics2D_SetSettings(dev->hwc_g2d, &gfxSettings);
           if (rc) {
              ALOGE("%s: failed to setup hwc_g2d checkpoint, composition will not work!", __FUNCTION__);
           }
           NEXUS_Graphics2D_GetCapabilities(dev->hwc_g2d, &dev->gfxCaps);
           ALOGI("%s: gfx caps: h-down-scale: %d, v-down-scale: %d", __FUNCTION__,
                 dev->gfxCaps.maxHorizontalDownScale, dev->gfxCaps.maxVerticalDownScale);
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

static void hw_vsync_cb(void *context, int param)
{
    VSYNC_CLIENT_INFO *ci = (VSYNC_CLIENT_INFO *)context;
    struct hwc_context_t* ctx = (struct hwc_context_t*)ci->ncci.parent;
    BSTD_UNUSED(param);

    BKNI_SetEvent(ci->vsync_event);
}

static void hwc_nsc_recycled_cb(void *context, int param)
{
    struct hwc_context_t *ctx = (struct hwc_context_t *)context;
    NEXUS_SurfaceHandle surfaces[HWC_NUM_DISP_BUFFERS];
    NEXUS_Error rc;
    size_t numSurfaces, i;

    BSTD_UNUSED(param);
    numSurfaces = 0;
    rc = NEXUS_SurfaceClient_RecycleSurface(ctx->disp_cli[HWC_PRIMARY_IX].schdl, surfaces, HWC_NUM_DISP_BUFFERS, &numSurfaces);
    if (rc) {
       ALOGW("%s: recycle Surface Error %#x %d", __FUNCTION__, ctx->disp_cli[HWC_PRIMARY_IX].schdl, rc);
    } else if ( numSurfaces > 0 ) {
       for (i = 0; i < numSurfaces; i++) {
          NEXUS_SurfaceHandle *pHandle = BFIFO_WRITE(&ctx->disp_cli[HWC_PRIMARY_IX].display_fifo);
          *pHandle = surfaces[i];
          BFIFO_WRITE_COMMIT(&ctx->disp_cli[HWC_PRIMARY_IX].display_fifo, 1);
       }
    }

    hwc_inc_retire_timeline(ctx, HWC_PRIMARY_IX, (int)numSurfaces, false);
    BKNI_SetEvent(ctx->recycle_event);
}

static void hwc_prepare_mm_layer(
    struct hwc_context_t* ctx,
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

    // Factor in overscan adjustments
    disp_position.x += ctx->overscan_position.x;
    disp_position.y += ctx->overscan_position.y;
    disp_position.width = (int)disp_position.width + (((int)disp_position.width * (int)ctx->overscan_position.w)/ctx->cfg[HWC_PRIMARY_IX].width);
    disp_position.height = (int)disp_position.height + (((int)disp_position.height * (int)ctx->overscan_position.h)/ctx->cfg[HWC_PRIMARY_IX].height);
    clip_position.x += ctx->overscan_position.x;
    clip_position.y += ctx->overscan_position.y;
    clip_position.width = (int)clip_position.width + (((int)clip_position.width * (int)ctx->overscan_position.w)/ctx->cfg[HWC_PRIMARY_IX].width);
    clip_position.height = (int)clip_position.height + (((int)clip_position.height * (int)ctx->overscan_position.h)/ctx->cfg[HWC_PRIMARY_IX].height);

    // make the gpx corresponding layer non-visible in the layer stack.
    if (ctx->gpx_cli[gpx_layer_id].composition.visible) {
       ctx->gpx_cli[gpx_layer_id].composition.visible = false;
    }

    ctx->mm_cli[vid_layer_id].root.layer_type = layer->compositionType;
    ctx->mm_cli[vid_layer_id].root.layer_subtype = NEXUS_VIDEO_WINDOW;
    if (ctx->mm_cli[vid_layer_id].root.layer_type == HWC_SIDEBAND) {
       ALOGE("%s: miscategorized sideband layer as video layer %d!", __FUNCTION__, vid_layer_id);
       goto out_unlock;
    }
    ctx->mm_cli[vid_layer_id].root.layer_flags = layer->flags;

    // deal with any change in the geometry/visibility of the layer.
   if (!ctx->mm_cli[vid_layer_id].root.composition.visible) {
      ctx->mm_cli[vid_layer_id].root.composition.visible = true;
      layer_updated = true;
   }

   if ((memcmp((void *)&disp_position, (void *)&ctx->mm_cli[vid_layer_id].root.composition.position, sizeof(NEXUS_Rect)) != 0) ||
       (memcmp((void *)&clip_position, (void *)&ctx->mm_cli[vid_layer_id].root.composition.clipRect, sizeof(NEXUS_Rect)) != 0)) {
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
      ctx->mm_cli[vid_layer_id].root.composition.virtualDisplay.width  = ctx->cfg[HWC_PRIMARY_IX].width;
      ctx->mm_cli[vid_layer_id].root.composition.virtualDisplay.height = ctx->cfg[HWC_PRIMARY_IX].height;

      ctx->mm_cli[vid_layer_id].settings.composition.contentMode           = NEXUS_VideoWindowContentMode_eFull;
      ctx->mm_cli[vid_layer_id].settings.composition.virtualDisplay.width  = ctx->cfg[HWC_PRIMARY_IX].width;
      ctx->mm_cli[vid_layer_id].settings.composition.virtualDisplay.height = ctx->cfg[HWC_PRIMARY_IX].height;
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

out_unlock:
    BKNI_ReleaseMutex(ctx->mutex);
out:
    return;
}

static void hwc_prepare_sb_layer(
    struct hwc_context_t* ctx,
    hwc_layer_1_t *layer,
    unsigned int gpx_layer_id,
    unsigned int sb_layer_id)
{
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

    // Factor in overscan adjustments
    disp_position.x += ctx->overscan_position.x;
    disp_position.y += ctx->overscan_position.y;
    disp_position.width = (int)disp_position.width + (((int)disp_position.width * (int)ctx->overscan_position.w)/ctx->cfg[HWC_PRIMARY_IX].width);
    disp_position.height = (int)disp_position.height + (((int)disp_position.height * (int)ctx->overscan_position.h)/ctx->cfg[HWC_PRIMARY_IX].height);
    clip_position.x += ctx->overscan_position.x;
    clip_position.y += ctx->overscan_position.y;
    clip_position.width = (int)clip_position.width + (((int)clip_position.width * (int)ctx->overscan_position.w)/ctx->cfg[HWC_PRIMARY_IX].width);
    clip_position.height = (int)clip_position.height + (((int)clip_position.height * (int)ctx->overscan_position.h)/ctx->cfg[HWC_PRIMARY_IX].height);

    // make the gpx corresponding layer non-visible in the layer stack.
    if (ctx->gpx_cli[gpx_layer_id].composition.visible) {
       ctx->gpx_cli[gpx_layer_id].composition.visible = false;
    }

    ctx->sb_cli[sb_layer_id].root.layer_type = layer->compositionType;
    ctx->sb_cli[sb_layer_id].root.layer_subtype = NEXUS_VIDEO_SIDEBAND;
    if (ctx->sb_cli[sb_layer_id].root.layer_type == HWC_OVERLAY) {
       ALOGE("%s: miscategorized video layer as sideband layer %d!", __FUNCTION__, sb_layer_id);
       goto out_unlock;
    }
    ctx->sb_cli[sb_layer_id].root.layer_flags = layer->flags;

    // deal with any change in the geometry/visibility of the layer.
    if (!ctx->sb_cli[sb_layer_id].root.composition.visible) {
        ctx->sb_cli[sb_layer_id].root.composition.visible = true;
        layer_updated = true;
    }

    if ((memcmp((void *)&disp_position, (void *)&ctx->sb_cli[sb_layer_id].root.composition.position, sizeof(NEXUS_Rect)) != 0) ||
       (memcmp((void *)&clip_position, (void *)&ctx->sb_cli[sb_layer_id].root.composition.clipRect, sizeof(NEXUS_Rect)) != 0)) {
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
        ctx->sb_cli[sb_layer_id].root.composition.virtualDisplay.width  = ctx->cfg[HWC_PRIMARY_IX].width;
        ctx->sb_cli[sb_layer_id].root.composition.virtualDisplay.height = ctx->cfg[HWC_PRIMARY_IX].height;
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

out_unlock:
    BKNI_ReleaseMutex(ctx->mutex);
out:
    return;
}

static void hwc_nsc_prepare_layer(
    struct hwc_context_t* ctx, hwc_layer_1_t *layer, int layer_id,
    bool geometry_changed, unsigned int *video_layer_id,
    unsigned int *sideband_layer_id, unsigned int *skip_set)
{
    bool is_sideband = false;
    bool is_yuv = false;

    if (is_video_layer(ctx, layer, layer_id, &is_sideband, &is_yuv)) {
        if (!is_sideband) {
            if (is_yuv) {
               hwc_prepare_gpx_layer(ctx, layer, layer_id, geometry_changed, false, false, skip_set);
            } else {
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
        hwc_prepare_gpx_layer(ctx, layer, layer_id, geometry_changed, false, false, skip_set);
    }
}

static void hwc_hide_unused_gpx_layer(struct hwc_context_t* ctx, int index)
{
    NEXUS_Error rc;

    if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
        goto out;
    }

    ctx->gpx_cli[index].last.layerhdl = NULL;
    ctx->gpx_cli[index].skip_set = false;
    ctx->gpx_cli[index].composition.visible = false;

    BKNI_ReleaseMutex(ctx->mutex);

out:
    return;
}

static void nx_client_hide_unused_mm_layer(struct hwc_context_t* ctx, int index)
{
    NEXUS_SurfaceComposition composition;
    NEXUS_Error rc;

    if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
        goto out;
    }

    if (ctx->mm_cli[index].root.composition.visible) {
       ctx->mm_cli[index].root.composition.visible = false;
    }

    BKNI_ReleaseMutex(ctx->mutex);
out:
    return;
}

static void nx_client_hide_unused_sb_layer(struct hwc_context_t* ctx, int index)
{
    NEXUS_SurfaceComposition composition;
    NEXUS_Error rc;

    if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
        goto out;
    }

    if (ctx->sb_cli[index].root.composition.visible) {
       ctx->sb_cli[index].root.composition.visible = false;
    }

    BKNI_ReleaseMutex(ctx->mutex);

out:
    return;
}

static void hwc_hide_unused_mm_layers(struct hwc_context_t* ctx)
{
    for (int i = 0; i < NSC_MM_CLIENTS_NUMBER; i++)
    {
       nx_client_hide_unused_mm_layer(ctx, i);
    }
}

static void hwc_hide_unused_sb_layers(struct hwc_context_t* ctx)
{
    for (int i = 0; i < NSC_SB_CLIENTS_NUMBER; i++)
    {
       nx_client_hide_unused_sb_layer(ctx, i);
    }
}

static void hwc_binder_advertise_video_surface(struct hwc_context_t* ctx)
{
    if (BKNI_AcquireMutex(ctx->mutex) != BERR_SUCCESS) {
        goto out;
    }

    // TODO: for the time being, only advertize one.
    if (ctx->nsc_video_changed && ctx->hwc_binder) {
       ctx->hwc_binder->setvideo(0, ctx->mm_cli[0].id,
                                 ctx->cfg[HWC_PRIMARY_IX].width, ctx->cfg[HWC_PRIMARY_IX].height);
       ctx->nsc_video_changed = false;
    }

    if (ctx->nsc_sideband_changed && ctx->hwc_binder) {
       ctx->hwc_binder->setsideband(0, ctx->sb_cli[0].id,
                                    ctx->cfg[HWC_PRIMARY_IX].width, ctx->cfg[HWC_PRIMARY_IX].height);
       ctx->nsc_sideband_changed = false;
    }

    BKNI_ReleaseMutex(ctx->mutex);

out:
    return;
}

static void hwc_checkpoint_callback(void *pParam, int param2)
{
    struct hwc_context_t *ctx = (struct hwc_context_t *)pParam;
    (void)param2;
    BKNI_SetEvent(ctx->checkpoint_event);
}

static int hwc_checkpoint(struct hwc_context_t *ctx)
{
    int rc;

    if (BKNI_AcquireMutex(ctx->g2d_mutex) != BERR_SUCCESS) {
        ALOGE("%s: failed g2d_mutex!", __FUNCTION__);
        return -1;
    }
    rc = hwc_checkpoint_locked(ctx);
    BKNI_ReleaseMutex(ctx->g2d_mutex);
    return rc;
}

static int hwc_checkpoint_locked(struct hwc_context_t *ctx)
{
    NEXUS_Error rc;

    rc = NEXUS_Graphics2D_Checkpoint(ctx->hwc_g2d, NULL);
    switch (rc) {
    case NEXUS_SUCCESS:
      break;
    case NEXUS_GRAPHICS2D_QUEUED:
      rc = BKNI_WaitForEvent(ctx->checkpoint_event, HWC_CHECKPOINT_TIMEOUT);
      if (rc) {
          ALOGW("%s: checkpoint timeout", __FUNCTION__);
          return -1;
      }
      break;
    default:
      ALOGE("%s: checkpoint error (%d)", __FUNCTION__, rc);
      return -1;
    }

    return 0;
}
