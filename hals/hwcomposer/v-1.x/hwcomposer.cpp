/*
 * Copyright (C) 2014-2016 Broadcom Canada Ltd.
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
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <utils/threads.h>
#include <cutils/sched_policy.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <utils/List.h>
#include <utils/Timers.h>
#include <utils/Thread.h>
#include <cutils/properties.h>

#include <hardware/hwcomposer.h>

#include <EGL/egl.h>
#include "nexus_ipc_client_factory.h"
#include "gralloc_priv.h"

#include "nexus_base_mmap.h"
#include "nexus_surface_client.h"
#include "nexus_surface_cursor.h"
#include "nexus_core_utils.h"
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
#include "sw_sync.h"
#include "nx_ashmem.h"

#include <inttypes.h>

using namespace android;

#define INVALID_HANDLE               0xBAADCAFE
#define LAST_PING_FRAME_ID_INVALID   0xBAADCAFE
#define LAST_PING_FRAME_ID_RESET     0xCAFEBAAD
#define INVALID_FENCE                -1

#define HWC_CURSOR_SURFACE_SUPPORTED 0

#define HWC_SB_NO_ALLOC_SURF_CLI     1
#define HWC_MM_NO_ALLOC_SURF_CLI     1

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

#define VSYNC_REFRESH                16666667
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
#define HWC_SET_COMP_THRESHOLD       1 /* legacy support (no fencing) */
#define HWC_SET_COMP_INLINE_DELAY    2
#define HWC_TICKER_W                 64
#define HWC_TICKER_H                 64
#define ALPHA_SHIFT                  24

#define HWC_OPAQUE                   0xFF000000
#define HWC_TRANSPARENT              0x00000000

#define HWC_SCOPE_PREP               0x0000C036
#define HWC_SCOPE_COMP               0x00006436

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
#define HWC_CAPABLE_COMP_BYPASS      "ro.nx.capable.cb"
#define HWC_CAPABLE_BACKGROUND       "ro.nx.capable.bg"
#define HWC_CAPABLE_TOGGLE_MODE      "ro.nx.capable.tm"
#define HWC_CAPABLE_SIGNAL_OOB_IL    "ro.nx.capable.si"
#define HWC_CAPABLE_SIDEBAND_HOLE    "ro.nx.capable.sh"
#define HWC_COMP_VIDEO               "ro.nx.cvbs"

#define HWC_DUMP_LAYER_PROP          "dyn.nx.hwc.dump.layer"
#define HWC_DUMP_VIRT_PROP           "dyn.nx.hwc.dump.virt"
#define HWC_DUMP_MMA_OPS_PROP        "dyn.nx.hwc.dump.mma"
#define HWC_DUMP_FENCE_PROP          "dyn.nx.hwc.dump.fence"
#define HWC_TRACK_COMP_CHATTY        "dyn.nx.hwc.track.chatty"
#define HWC_TICKER                   "dyn.nx.hwc.ticker"
#define HWC_FB_MODE                  "dyn.nx.hwc.fbmode"
#define HWC_IGNORE_CURSOR            "dyn.nx.hwc.nocursor"
#define HWC_BLANK_VIDEO              "dyn.nx.hwc.vid.blank"
#define HWC_UPSIDE_DOWN              "dyn.nx.hwc.comp.updwn"

#define HWC_GLES_VIRTUAL_PROP        "ro.hwc.gles.virtual"
#define HWC_WITH_V3D_FENCE_PROP      "ro.v3d.fence.expose"
#define HWC_WITH_DSP_FENCE_PROP      "ro.hwc.fence.retire"
#define HWC_TRACK_COMP_TIME          "ro.hwc.track.comptime"
#define HWC_G2D_SIM_OPS_PROP         "ro.hwc.g2d.sim_ops"
#define HWC_HACK_SYNC_REFRESH        "ro.hwc.hack.sync.refresh"

#define HWC_EXT_REFCNT_DEV           "ro.nexus.ashmem.devname"

#define HWC_CHECKPOINT_TIMEOUT       (5000)

#define HWC_DUMP_LEVEL_PREPARE       (1<<0)
#define HWC_DUMP_LEVEL_SET           (1<<1)
#define HWC_DUMP_LEVEL_COMPOSE       (1<<2)
#define HWC_DUMP_LEVEL_CLASSIFY      (1<<3)
#define HWC_DUMP_LEVEL_LATENCY       (1<<4)
#define HWC_DUMP_LEVEL_FINAL         (1<<5)
#define HWC_DUMP_LEVEL_HACKS         (1<<6)
#define HWC_DUMP_LEVEL_SCREEN_TIME   (1<<7)
#define HWC_DUMP_LEVEL_QOPS          (1<<8)

#define HWC_DUMP_FENCE_PRIM          (1<<0)
#define HWC_DUMP_FENCE_VIRT          (1<<1)
#define HWC_DUMP_FENCE_SUMMARY       (1<<2)
#define HWC_DUMP_FENCE_TICK          (1<<3)

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

typedef enum DISPLAY_CLIENT_MODE {
    CLIENT_MODE_NONE = 0,
    CLIENT_MODE_COMP_BYPASS,
    CLIENT_MODE_NSC_FRAMEBUFFER
} DISPLAY_CLIENT_MODE;

const NEXUS_BlendEquation nexusColorBlendingEquation[BLENDIND_TYPE_LAST] = {
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

const NEXUS_BlendEquation nexusColorSrcOverConstAlpha = {
        NEXUS_BlendFactor_eSourceColor,
        NEXUS_BlendFactor_eConstantAlpha,
        false,
        NEXUS_BlendFactor_eDestinationColor,
        NEXUS_BlendFactor_eInverseSourceAlpha,
        false,
        NEXUS_BlendFactor_eZero
};

const NEXUS_BlendEquation nexusAlphaBlendingEquation[BLENDIND_TYPE_LAST] = {
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

const NEXUS_BlendEquation nexusAlphaSrcOverConstAlpha = {
        NEXUS_BlendFactor_eSourceAlpha,
        NEXUS_BlendFactor_eConstantAlpha,
        false,
        NEXUS_BlendFactor_eDestinationAlpha,
        NEXUS_BlendFactor_eInverseSourceAlpha,
        false,
        NEXUS_BlendFactor_eZero
};

#define SHIFT_FACTOR 10
static NEXUS_Graphics2DColorMatrix g_hwc_ai32_Matrix_YCbCrtoRGB =
{
   SHIFT_FACTOR,
   {
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
    bool skip_set;
    int rel_tl;
    int rel_tl_ix;
    uint64_t rel_tl_tick;
} GPX_CLIENT_INFO;

typedef struct {
    GPX_CLIENT_INFO root;
    NEXUS_SurfaceClientSettings settings;
    long long unsigned last_ping_frame_id;
    int id;
} MM_CLIENT_INFO;

typedef struct {
    GPX_CLIENT_INFO root;
    NEXUS_SurfaceClientSettings settings;
    int id;
    bool geometry_updated;
} SB_CLIENT_INFO;

typedef struct {
    COMMON_CLIENT_INFO ncci;
    buffer_handle_t grhdl;
    NEXUS_SurfaceComposition composition;
    int rel_tl;
    int rel_tl_ix;
    uint64_t rel_tl_tick;
} VD_CLIENT_INFO;

typedef struct {
    NEXUS_SurfaceHandle fifo_surf;
    int fifo_fd;
} DISPLAY_FIFO_ELEMENT;

typedef struct {
    NEXUS_SurfaceClientHandle schdl;
    NEXUS_SurfaceCompositorClientId sccid;
    BFIFO_HEAD(DisplayFifo, NEXUS_SurfaceHandle) display_fifo;
    NEXUS_SurfaceHandle display_elements[HWC_NUM_DISP_BUFFERS];
    DISPLAY_FIFO_ELEMENT display_buffers[HWC_NUM_DISP_BUFFERS];
    pthread_mutex_t fifo_mutex;
    DISPLAY_CLIENT_MODE mode;
    bool mode_toggle;
} DISPLAY_CLIENT_INFO;

typedef struct {
    /* inputs. */
    int scope;
    hwc_layer_1_t *layer;
    PSHARED_DATA sharedData;
    int gr_usage;
    int sideband_client;
    /* outputs. */
    bool is_sideband;
    bool is_yuv;
} VIDEO_LAYER_VALIDATION;

typedef struct {
    int fill;
    int blit;
    int yv12;
} OPS_COUNT;

typedef void (* HWC_BINDER_NTFY_CB)(void *, int, struct hwc_notification_info &);
typedef void (* HWC_HOTPLUG_NTFY_CB)(void *);
typedef void (* HWC_DISPLAY_CHANGED_NTFY_CB)(void *);

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

    void register_notify(HWC_BINDER_NTFY_CB callback, void *data) {
       cb = callback;
       cb_data = data;
    }

private:
    HWC_BINDER_NTFY_CB cb;
    void * cb_data;
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

class HwcHotPlug : public BnNexusHdmiHotplugEventListener
{
public:

    HwcHotPlug() {};
    ~HwcHotPlug() {};
    virtual status_t onHdmiHotplugEventReceived(int32_t portId, bool connected);

    void register_notify(HWC_HOTPLUG_NTFY_CB callback, void *data) {
       cb = callback;
       cb_data = data;
    }

private:
    HWC_HOTPLUG_NTFY_CB cb;
    void * cb_data;
};

class HwcHotPlug_wrap
{
private:

   sp<HwcHotPlug> ihwc;

public:
   HwcHotPlug_wrap(void) {
      ALOGD("%s: allocated %p", __FUNCTION__, this);
      ihwc = new HwcHotPlug;
   };

   ~HwcHotPlug_wrap(void) {
      ALOGD("%s: cleared %p", __FUNCTION__, this);
      ihwc.clear();
   };

   HwcHotPlug *get(void) {
      return ihwc.get();
   }
};

status_t HwcHotPlug::onHdmiHotplugEventReceived(int32_t portId, bool connected)
{
   (void) portId;

   ALOGD( "%s: hotplug-%d -> %s", __FUNCTION__, portId, connected ? "connected" : "disconnected");

   if (connected && cb)
      cb(cb_data);

   return NO_ERROR;
}

class HwcDisplayChanged : public BnNexusDisplaySettingsChangedEventListener
{
public:

    HwcDisplayChanged() {};
    ~HwcDisplayChanged() {};
    virtual status_t onDisplaySettingsChangedEventReceived(int32_t portId);

    void register_notify(HWC_DISPLAY_CHANGED_NTFY_CB callback, void *data) {
       cb = callback;
       cb_data = data;
    }

private:
    HWC_DISPLAY_CHANGED_NTFY_CB cb;
    void * cb_data;
};

class HwcDisplayChanged_wrap
{
private:

   sp<HwcDisplayChanged> ihwc;

public:
   HwcDisplayChanged_wrap(void) {
      ALOGD("%s: allocated %p", __FUNCTION__, this);
      ihwc = new HwcDisplayChanged;
   };

   ~HwcDisplayChanged_wrap(void) {
      ALOGD("%s: cleared %p", __FUNCTION__, this);
      ihwc.clear();
   };

   HwcDisplayChanged *get(void) {
      return ihwc.get();
   }
};

status_t HwcDisplayChanged::onDisplaySettingsChangedEventReceived(int32_t portId)
{
   (void) portId;

   ALOGD( "%s: display-%d -> changed", __FUNCTION__, portId);

   if (cb)
      cb(cb_data);

   return NO_ERROR;
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
   unsigned long long skipped;
   unsigned long long oob;
   unsigned long long ontime;
   unsigned long long slow;
   long double fastest;
   long double slowest;
   long double cumul;
};

struct hwc_time_on_display_stats {
   int64_t last_push;
   int64_t last_vsync;
   int64_t vsync;
   unsigned long long tracked;
   long double fastest;
   long double slowest;
   long double cumul;
};

struct hwc_display_cfg {
   int width;
   int height;
   int x_dpi;
   int y_dpi;
};

struct hwc_work_item {
   struct hwc_work_item *next;
   unsigned long long comp_ix;
   int64_t tick_queued;
   int64_t comp_wait;
   int64_t fence_wait;
   int64_t surf_wait;
   bool skip_set[NSC_GPX_CLIENTS_NUMBER];
   NEXUS_SurfaceComposition comp[NSC_GPX_CLIENTS_NUMBER];
   int sb_cli[NSC_GPX_CLIENTS_NUMBER];
   int video_layers;
   int sideband_layers;
   hwc_display_contents_1_t content;
};

struct hwc_context_t {
    hwc_composer_device_1_t device;

    struct hwc_display_stats stats[DISPLAY_SUPPORTED];
    struct hwc_display_cfg cfg[DISPLAY_SUPPORTED];
    struct hwc_time_track_stats time_track[DISPLAY_SUPPORTED];
    struct hwc_time_on_display_stats on_display;

    /* our private state goes below here */
    NexusIPCClientBase *pIpcClient;
    NexusClientContext *pNexusClientContext;
    NxClient_AllocResults nxAllocResults;
    pthread_mutex_t dev_mutex;

    hwc_procs_t const* procs;

    bool vsync_callback_enabled;
    bool vsync_callback_initialized;
    pthread_mutex_t vsync_callback_enabled_mutex;
    pthread_t vsync_callback_thread;
    pthread_t recycle_callback_thread;
    int vsync_thread_run;
    int recycle_thread_run;
    NEXUS_DisplayHandle display_handle;
    char mem_if_name[PROPERTY_VALUE_MAX];
    int mem_if;

    GPX_CLIENT_INFO gpx_cli[NSC_GPX_CLIENTS_NUMBER];
    MM_CLIENT_INFO mm_cli[NSC_MM_CLIENTS_NUMBER];
    SB_CLIENT_INFO sb_cli[NSC_SB_CLIENTS_NUMBER];
    pthread_mutex_t mutex;
    bool nsc_video_changed;
    bool nsc_sideband_changed;
    VD_CLIENT_INFO vd_cli[HWC_VD_CLIENTS_NUMBER];
    DISPLAY_CLIENT_INFO disp_cli[DISPLAY_SUPPORTED];
    BKNI_EventHandle recycle_event;
    BKNI_EventHandle recycle_callback;
    BKNI_EventHandle vsync_callback;

    struct hwc_position overscan_position;
    uint32_t last_seed;

    bool display_gles_fallback;
    bool display_gles_always;
    bool display_gles_virtual;
    bool needs_fb_target;
    bool comp_bypass;

    pthread_mutex_t power_mutex;
    int power_mode;

    HwcBinder_wrap *hwc_binder;
    HwcHotPlug_wrap *hwc_hp;
    HwcDisplayChanged_wrap *hwc_dc;

    NEXUS_Graphics2DHandle hwc_g2d;
    pthread_mutex_t g2d_mutex;
    NEXUS_Graphics2DCapabilities gfxCaps;
    BKNI_EventHandle checkpoint_event;

    BKNI_EventHandle composer_event[DISPLAY_SUPPORTED];
    BKNI_EventHandle composer_event_sync[DISPLAY_SUPPORTED];
    BKNI_EventHandle composer_event_forced[DISPLAY_SUPPORTED];
    pthread_t composer_thread[DISPLAY_SUPPORTED];
    int composer_thread_run[DISPLAY_SUPPORTED];
    int ret_tl[DISPLAY_SUPPORTED];
    struct hwc_work_item *composer_work_list[DISPLAY_SUPPORTED];
    pthread_mutex_t comp_work_list_mutex[DISPLAY_SUPPORTED];;

    int display_dump_layer;
    int display_dump_virt;
    int dump_fence;

    bool g2d_allow_simult;
    bool fence_support;
    bool fence_retire;
    bool dump_mma;
    bool track_comp_time;
    bool track_comp_chatty;
    bool ticker;
    int  blank_video;
    bool upside_down;

    bool alpha_hole_background;
    bool flush_background;
    bool smart_background;
    bool toggle_fb_mode;
    bool ignore_cursor;
    bool hack_sync_refresh;
    bool signal_oob_inline;
    int prepare_video;
    int prepare_sideband;
    int sb_hole_threshold;
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
   .open = hwc_device_open
};

static void hwc_recycled_cb(void *context, int param);
static void hwc_vsync_cb(void *context, int param);

static void hwc_hide_unused_gpx_layer(struct hwc_context_t* dev, int index);
static void hwc_hide_unused_mm_layers(struct hwc_context_t* dev);
static void hwc_hide_unused_sb_layers(struct hwc_context_t* dev);

static void hwc_nsc_prepare_layer(struct hwc_context_t* dev, hwc_layer_1_t *layer,
   int layer_id, bool geometry_changed, unsigned int *video_layer_id,
   unsigned int *sideband_layer_id, unsigned int *skip_set, unsigned int *skip_cand);

static void hwc_binder_advertise_video_surface(struct hwc_context_t* dev);

static void hwc_checkpoint_callback(void *pParam, int param2);
static int hwc_checkpoint(struct hwc_context_t *ctx);
static int hwc_checkpoint_locked(struct hwc_context_t *ctx);

hwc_module_t HAL_MODULE_INFO_SYM = {
   .common = {
      .tag                = HARDWARE_MODULE_TAG,
      .module_api_version = HWC_MODULE_API_VERSION_0_1,
      .hal_api_version    = HARDWARE_HAL_API_VERSION,
      .id                 = HWC_HARDWARE_MODULE_ID,
      .name               = "hwc for set-top-box platforms",
      .author             = "Broadcom",
      .methods            = &hwc_module_methods,
      .dso                = 0,
      .reserved           = {0}
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

static const char *hwc_fb_mode[] =
{
   "INVALID",     // CLIENT_MODE_NONE
   "COMP-BYPASS", // CLIENT_MODE_COMP_BYPASS
   "NSC-FB",      // CLIENT_MODE_NSC_FRAMEBUFFER
};

static void hwc_assign_memory_interface_name(struct hwc_context_t *ctx)
{
   char device[PROPERTY_VALUE_MAX];

   if (strlen(ctx->mem_if_name) == 0) {
      memset(device, 0, sizeof(device));
      memset(ctx->mem_if_name, 0, sizeof(ctx->mem_if_name));

      property_get(HWC_EXT_REFCNT_DEV, device, NULL);
      if (strlen(device)) {
         strcpy(ctx->mem_if_name, "/dev/");
         strcat(ctx->mem_if_name, device);
      }
   }
}

static int hwc_open_memory_interface(struct hwc_context_t *ctx)
{
   int mem_fd = INVALID_FENCE;

   if (ctx->mem_if >= 0) {
      goto out;
   }
   hwc_assign_memory_interface_name(ctx);
   ctx->mem_if = open(ctx->mem_if_name, O_RDWR, 0);

out:
   return ctx->mem_if;
}

static void hwc_close_memory_interface(struct hwc_context_t *ctx)
{
   if (ctx->mem_if != INVALID_FENCE) {
      close(ctx->mem_if);
      ctx->mem_if = INVALID_FENCE;
   }
}

static NEXUS_Error hwc_ext_refcnt(struct hwc_context_t *ctx, NEXUS_MemoryBlockHandle block_handle, int cnt)
{
   int mem_if = hwc_open_memory_interface(ctx);
   if (mem_if != INVALID_FENCE) {
      struct nx_ashmem_ext_refcnt ashmem_ext_refcnt;
      memset(&ashmem_ext_refcnt, 0, sizeof(struct nx_ashmem_ext_refcnt));
      ashmem_ext_refcnt.hdl = (__u64)block_handle;
      ashmem_ext_refcnt.cnt = cnt;
      int ret = ioctl(mem_if, NX_ASHMEM_EXT_REFCNT, &ashmem_ext_refcnt);
      if (ret < 0) {
         ALOGE("%s: failed (%d) for %p, count:0x%x", __FUNCTION__, ret, block_handle, cnt);
         hwc_close_memory_interface(ctx);
         return NEXUS_OS_ERROR;
      }
   } else {
      return NEXUS_INVALID_PARAMETER;
   }
   return NEXUS_SUCCESS;
}

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

static NEXUS_PixelFormat gralloc_to_nexus_pixel_format(int format)
{
   switch (format) {
      case HAL_PIXEL_FORMAT_RGBA_8888: return NEXUS_PixelFormat_eA8_B8_G8_R8;
      case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED: /* fall through. */
      case HAL_PIXEL_FORMAT_RGBX_8888: return NEXUS_PixelFormat_eX8_B8_G8_R8;
      case HAL_PIXEL_FORMAT_RGB_888:   return NEXUS_PixelFormat_eX8_B8_G8_R8;
      case HAL_PIXEL_FORMAT_RGB_565:   return NEXUS_PixelFormat_eR5_G6_B5;
      case HAL_PIXEL_FORMAT_YV12: /* fall-thru */
      case HAL_PIXEL_FORMAT_YCbCr_420_888: return NEXUS_PixelFormat_eY08_Cb8_Y18_Cr8;
      default:                         break;
   }

   ALOGE("%s: unsupported format %d", __FUNCTION__, format);
   return NEXUS_PixelFormat_eUnknown;
}

static void hwc_setup_props_locked(struct hwc_context_t* ctx)
{
   ctx->display_dump_layer   = property_get_int32(HWC_DUMP_LAYER_PROP, 0);
   ctx->display_dump_virt    = property_get_int32(HWC_DUMP_VIRT_PROP, 0);
   ctx->dump_mma             = property_get_bool(HWC_DUMP_MMA_OPS_PROP, 0);
   ctx->dump_fence           = property_get_int32(HWC_DUMP_FENCE_PROP, 0);
   ctx->track_comp_chatty    = property_get_bool(HWC_TRACK_COMP_CHATTY, 0);
   ctx->ticker               = property_get_bool(HWC_TICKER, 0);
   ctx->blank_video          = property_get_int32(HWC_BLANK_VIDEO, 0);
   ctx->upside_down          = property_get_bool(HWC_UPSIDE_DOWN, 0);
}

static int hwc_setup_framebuffer_mode(struct hwc_context_t* dev, int disp_ix, DISPLAY_CLIENT_MODE mode)
{
   int j;
   NEXUS_Error rc;
   NEXUS_ClientConfiguration clientConfig;
   NEXUS_SurfaceClientSettings surfaceClientSettings;

   NEXUS_Platform_GetClientConfiguration(&clientConfig);

   if (dev->disp_cli[disp_ix].mode != CLIENT_MODE_NONE) {
      for (j = 0; j < HWC_NUM_DISP_BUFFERS; j++) {
         if (dev->disp_cli[disp_ix].display_buffers[j].fifo_fd != INVALID_FENCE) {
            close(dev->disp_cli[disp_ix].display_buffers[j].fifo_fd);
            dev->disp_cli[disp_ix].display_buffers[j].fifo_fd = INVALID_FENCE;
         }
         if (dev->disp_cli[disp_ix].display_buffers[j].fifo_surf) {
            NEXUS_Surface_Destroy(dev->disp_cli[disp_ix].display_buffers[j].fifo_surf);
         }
      }
   }

   if (dev->disp_cli[disp_ix].mode != mode) {
      NEXUS_SurfaceClient_GetSettings(dev->disp_cli[disp_ix].schdl, &surfaceClientSettings);
      surfaceClientSettings.recycled.callback = hwc_recycled_cb;
      surfaceClientSettings.recycled.context = (void *)dev;
      surfaceClientSettings.vsync.callback = hwc_vsync_cb;
      surfaceClientSettings.vsync.context = (void *)dev;
      surfaceClientSettings.allowCompositionBypass = (mode == CLIENT_MODE_COMP_BYPASS) ? true : false;
      rc = NEXUS_SurfaceClient_SetSettings(dev->disp_cli[disp_ix].schdl, &surfaceClientSettings);
      if (rc) {
         ALOGE("%s: unable to set surface client settings", __FUNCTION__);
         return -EINVAL;
      }
   }

   for (j = 0; j < HWC_NUM_DISP_BUFFERS; j++) {
      NEXUS_SurfaceCreateSettings surfaceCreateSettings;
      NEXUS_MemoryBlockHandle block_handle = NULL;
      bool dynamic_heap = false;
      NEXUS_Surface_GetDefaultCreateSettings(&surfaceCreateSettings);
      surfaceCreateSettings.width = dev->cfg[disp_ix].width;
      surfaceCreateSettings.height = dev->cfg[disp_ix].height;
      surfaceCreateSettings.pitch = surfaceCreateSettings.width * 4;
      surfaceCreateSettings.pixelFormat = NEXUS_PixelFormat_eA8_B8_G8_R8;
      if (mode == CLIENT_MODE_COMP_BYPASS) {
         surfaceCreateSettings.heap = NEXUS_Platform_GetFramebufferHeap(0);
      } else {
         surfaceCreateSettings.heap = clientConfig.heap[NXCLIENT_DYNAMIC_HEAP];
         dynamic_heap = true;
      }
      dev->disp_cli[disp_ix].display_buffers[j].fifo_surf = NULL;
      block_handle = hwc_block_create(&surfaceCreateSettings, dev->mem_if_name, dynamic_heap, &dev->disp_cli[disp_ix].display_buffers[j].fifo_fd);
      if (block_handle != NULL) {
         surfaceCreateSettings.pixelMemory = block_handle;
         surfaceCreateSettings.heap = NULL;
         dev->disp_cli[disp_ix].display_buffers[j].fifo_surf = hwc_surface_create(&surfaceCreateSettings, dynamic_heap);
      }
      if (dev->disp_cli[disp_ix].display_buffers[j].fifo_surf == NULL) {
         return -ENOMEM;
      }
      ALOGI("%s: fb:%d::%dx%d::%d::heap:%s -> %p (%d::%p)", __FUNCTION__, j, surfaceCreateSettings.width, surfaceCreateSettings.height,
            surfaceCreateSettings.pixelFormat, dynamic_heap?"d-cma":"gfx",
            dev->disp_cli[disp_ix].display_buffers[j].fifo_surf,
            dev->disp_cli[disp_ix].display_buffers[j].fifo_fd, block_handle);
   }
   if (!pthread_mutex_lock(&dev->disp_cli[disp_ix].fifo_mutex)) {
      for (j = 0; j < HWC_NUM_DISP_BUFFERS; j++) {
         dev->disp_cli[disp_ix].display_elements[j] = dev->disp_cli[disp_ix].display_buffers[j].fifo_surf;
      }
      BFIFO_INIT(&dev->disp_cli[disp_ix].display_fifo, dev->disp_cli[disp_ix].display_elements, HWC_NUM_DISP_BUFFERS);
      BFIFO_WRITE_COMMIT(&dev->disp_cli[disp_ix].display_fifo, HWC_NUM_DISP_BUFFERS);
      pthread_mutex_unlock(&dev->disp_cli[disp_ix].fifo_mutex);
   } else {
      return -EINVAL;
   }
   dev->disp_cli[disp_ix].mode = mode;
   return 0;
}

static void hwc_execute_dyn_change_locked(struct hwc_context_t* ctx)
{
   DISPLAY_CLIENT_MODE fbmode = (DISPLAY_CLIENT_MODE) property_get_int32(HWC_FB_MODE, 0);
   if (fbmode != ctx->disp_cli[HWC_PRIMARY_IX].mode) {
      if (fbmode == CLIENT_MODE_COMP_BYPASS || fbmode == CLIENT_MODE_NSC_FRAMEBUFFER) {
         ALOGW("toggling display %d fb-mode: %s->%s", HWC_PRIMARY_IX,
            hwc_fb_mode[ctx->disp_cli[HWC_PRIMARY_IX].mode], hwc_fb_mode[fbmode]);
         hwc_setup_framebuffer_mode(ctx, HWC_PRIMARY_IX, fbmode);
      }
   }
}

static int hwc_mem_lock_phys(struct hwc_context_t *ctx, NEXUS_MemoryBlockHandle block_handle, NEXUS_Addr *paddr) {
   NEXUS_Error rc = NEXUS_SUCCESS;
   NEXUS_Error ext = NEXUS_SUCCESS;

   if (block_handle) {
      ext = hwc_ext_refcnt(ctx, block_handle, NX_ASHMEM_REFCNT_ADD);
      if (ext == NEXUS_SUCCESS) {
         rc = NEXUS_MemoryBlock_LockOffset(block_handle, paddr);
         if (rc != NEXUS_SUCCESS) {
            hwc_ext_refcnt(ctx, block_handle, NX_ASHMEM_REFCNT_REM);
         }
      } else {
         rc = NEXUS_OS_ERROR;
      }

      if (rc != NEXUS_SUCCESS) {
         if (ctx->dump_mma) {
            ALOGW("mma-lock-phys: %p -> invalid!?!", block_handle);
         }
         *paddr = (NEXUS_Addr)NULL;
         return (int)rc;
      } else if (ctx->dump_mma) {
         ALOGI("mma-lock-phys: %p -> 0x%" PRIu64 "", block_handle, *paddr);
      }
   } else {
      *paddr = (NEXUS_Addr)NULL;
   }
   return 0;
}

static int hwc_mem_unlock_phys(struct hwc_context_t *ctx, NEXUS_MemoryBlockHandle block_handle) {
   if (block_handle) {
      NEXUS_MemoryBlock_UnlockOffset(block_handle);
      hwc_ext_refcnt(ctx, block_handle, NX_ASHMEM_REFCNT_REM);

      if (ctx->dump_mma) {
         ALOGI("mma-unlock-phys: %p", block_handle);
      }
   }
   return 0;
}

static int hwc_mem_lock(struct hwc_context_t *ctx, NEXUS_MemoryBlockHandle block_handle, void **addr, bool lock) {
   NEXUS_Error rc = NEXUS_SUCCESS;
   NEXUS_Error ext = NEXUS_SUCCESS;

   if (block_handle) {
      if (lock) {
         ext = hwc_ext_refcnt(ctx, block_handle, NX_ASHMEM_REFCNT_ADD);
         if (ext == NEXUS_SUCCESS) {
            rc = NEXUS_MemoryBlock_Lock(block_handle, addr);
            if (rc != NEXUS_SUCCESS) {
               hwc_ext_refcnt(ctx, block_handle, NX_ASHMEM_REFCNT_REM);
               if (rc == BERR_NOT_SUPPORTED) {
                  NEXUS_MemoryBlock_Unlock(block_handle);
               }
            }
         } else {
            rc = NEXUS_OS_ERROR;
         }

         if (rc) {
            if (ctx->dump_mma) {
               ALOGW("mma-lock: %p -> invalid!?!", block_handle);
            }
            *addr = NULL;
            return (int)rc;
         } else if (ctx->dump_mma) {
            ALOGI("mma-lock: %p -> %p", block_handle, *addr);
         }
      } else {
        return NEXUS_SUCCESS;
      }
   } else {
      *addr = NULL;
   }
   return 0;
}

static int hwc_mem_unlock(struct hwc_context_t *ctx, NEXUS_MemoryBlockHandle block_handle, bool lock) {
   if (block_handle) {
      if (lock) {
         NEXUS_MemoryBlock_Unlock(block_handle);
         hwc_ext_refcnt(ctx, block_handle, NX_ASHMEM_REFCNT_REM);

         if (ctx->dump_mma) {
            ALOGI("mma-unlock: %p", block_handle);
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
       NEXUS_MemoryBlockHandle block_handle = NULL;
       private_handle_t *gr_buffer = (private_handle_t *)client->grhdl;
       private_handle_t::get_block_handles(gr_buffer, &block_handle, NULL);
       hwc_mem_lock((struct hwc_context_t*)client->ncci.parent, block_handle, &pAddr, true);
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
          "\t\t\t[%p]::{f:0x%x,bpp:%d,{%d,%d},%p,sz:%d}\n",
          client->grhdl,
          pSharedData->container.format,
          pSharedData->container.bpp,
          pSharedData->container.width,
          pSharedData->container.height,
          pSharedData->container.block,
          pSharedData->container.size);

       if (write > 0) {
          local_capacity = (local_capacity > write) ? (local_capacity - write) : 0;
          offset += write;
          total_write += write;
       }

       hwc_mem_unlock((struct hwc_context_t*)client->ncci.parent, block_handle, true);
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
        "\t[%s]:[%s]:[%d:%d]:[%s]:[%s]::f:%x::z:%d::cp:{%d,%d,%d,%d}::cl:{%d,%d,%d,%d}::cv:{%d,%d}::b:%d\n",
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
        client->blending_type);
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
       NEXUS_MemoryBlockHandle block_handle = NULL;
       private_handle_t *gr_buffer = (private_handle_t *)client->last.grhdl;
       private_handle_t::get_block_handles(gr_buffer, &block_handle, NULL);
       hwc_mem_lock((struct hwc_context_t*)client->ncci.parent, block_handle, &pAddr, true);
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
           "\t\t\t[%p]::{f:0x%x,bpp:%d,{%d,%d},%p,s:%d,sz:%d}\n",
           client->last.grhdl,
           pSharedData->container.format,
           pSharedData->container.bpp,
           pSharedData->container.width,
           pSharedData->container.height,
           pSharedData->container.block,
           pSharedData->container.stride,
           pSharedData->container.size);

       if (write > 0) {
           local_capacity = (local_capacity > write) ? (local_capacity - write) : 0;
           offset += write;
           total_write += write;
       }

       hwc_mem_unlock((struct hwc_context_t*)client->ncci.parent, block_handle, true);
    }

    return total_write;
}

static int dump_mm_layer_data(char *start, int capacity, int index, MM_CLIENT_INFO *client)
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

    if (pthread_mutex_lock(&ctx->mutex)) {
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
           ctx->ret_tl[HWC_PRIMARY_IX]);
       if (write > 0) {
           capacity = (capacity > write) ? (capacity - write) : 0;
           index += write;
       }
       write = snprintf(buff + index, capacity, "\tipc:%p::ncc:%p::d:{%d,%d}::pm:%s::fm:%s::oscan:{%d,%d:%d,%d}\n",
           ctx->pIpcClient,
           ctx->pNexusClientContext,
           ctx->cfg[HWC_PRIMARY_IX].width,
           ctx->cfg[HWC_PRIMARY_IX].height,
           hwc_power_mode[ctx->power_mode],
           hwc_fb_mode[ctx->disp_cli[HWC_PRIMARY_IX].mode],
           ctx->overscan_position.x,
           ctx->overscan_position.y,
           ctx->overscan_position.w,
           ctx->overscan_position.h);
       if (write > 0) {
           capacity = (capacity > write) ? (capacity - write) : 0;
           index += write;
       }
       write = snprintf(buff + index, capacity, "\tcomp:%llu,%llu,%llu:{%llu,%llu}::slow:%.3Lf::fast:%.3Lf::avg:%.3Lf\n",
           ctx->time_track[HWC_PRIMARY_IX].tracked,
           ctx->time_track[HWC_PRIMARY_IX].oob,
           ctx->time_track[HWC_PRIMARY_IX].skipped,
           ctx->time_track[HWC_PRIMARY_IX].ontime,
           ctx->time_track[HWC_PRIMARY_IX].slow,
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
           ctx->ret_tl[HWC_VIRTUAL_IX]);
       if (write > 0) {
           capacity = (capacity > write) ? (capacity - write) : 0;
           index += write;
       }
       write = snprintf(buff + index, capacity, "\tcomp:%llu,%llu,%llu:{%llu,%llu}::slow:%.3Lf::fast:%.3Lf::avg:%.3Lf\n",
           ctx->time_track[HWC_VIRTUAL_IX].tracked,
           ctx->time_track[HWC_VIRTUAL_IX].oob,
           ctx->time_track[HWC_VIRTUAL_IX].skipped,
           ctx->time_track[HWC_VIRTUAL_IX].ontime,
           ctx->time_track[HWC_VIRTUAL_IX].slow,
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

    hwc_setup_props_locked(ctx);
    hwc_execute_dyn_change_locked(ctx);
    pthread_mutex_unlock(&ctx->mutex);

out:
    return;
}

static void hwc_binder_notify(void *dev, int msg, struct hwc_notification_info &ntfy)
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
                  ALOGD("%s: reset drop-dup-count on vid 0x%x, id 0x%x", __FUNCTION__, ntfy.surface_hdl, ctx->mm_cli[i].id);
                  ctx->mm_cli[i].last_ping_frame_id = LAST_PING_FRAME_ID_RESET;
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
                composition.colorBlend = nexusColorBlendingEquation[BLENDIND_TYPE_SRC_OVER];
                composition.alphaBlend = nexusAlphaBlendingEquation[BLENDIND_TYPE_SRC_OVER];
                rc = NxClient_SetSurfaceClientComposition(ctx->disp_cli[HWC_PRIMARY_IX].sccid, &composition);
                if (rc) {
                    ALOGW("%s: Unable to set client composition for overscan adjustment %d", __FUNCTION__, rc);
                }
            }
       break;
       case HWC_BINDER_NTFY_SIDEBAND_SURFACE_ACQUIRED:
       {
           for (int i = 0; i < NSC_SB_CLIENTS_NUMBER; i++) {
               if (ctx->sb_cli[i].id == ntfy.surface_hdl) {
                  ALOGD("%s: flush-background on sfid 0x%x, id 0x%x", __FUNCTION__, ntfy.surface_hdl, ctx->sb_cli[i].id);
                  ctx->flush_background = true;
                  ctx->sb_cli[i].geometry_updated = true;
                  break;
               }
           }
       }
       break;
       default:
       break;
       }
   }
}

static DISPLAY_CLIENT_MODE hwc_fb_mode_for_format(struct hwc_context_t* ctx, NxClient_DisplaySettings *settings)
{
    DISPLAY_CLIENT_MODE mode = CLIENT_MODE_NONE;
    NxClient_GetDisplaySettings(settings);

    if (ctx == NULL) {
       goto out;
    }

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
       mode = CLIENT_MODE_NSC_FRAMEBUFFER;
    break;
    default:
       mode = ctx->comp_bypass ? CLIENT_MODE_COMP_BYPASS : CLIENT_MODE_NSC_FRAMEBUFFER;
    break;
    }

out:
    return mode;
}

static void hwc_display_changed_notify(void *dev)
{
    NxClient_DisplaySettings settings;
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    DISPLAY_CLIENT_MODE newmode = hwc_fb_mode_for_format(ctx, &settings);

    if (ctx) {
       if (ctx->toggle_fb_mode) {
          if (ctx->disp_cli[HWC_PRIMARY_IX].mode != newmode) {
             ALOGI("%s: framebuffer mode: %s -> %s (on display-changed)", __FUNCTION__,
                hwc_fb_mode[ctx->disp_cli[HWC_PRIMARY_IX].mode], hwc_fb_mode[newmode]);
             ctx->disp_cli[HWC_PRIMARY_IX].mode_toggle = true;
          } else if (ctx->disp_cli[HWC_PRIMARY_IX].mode_toggle) {
             ALOGI("%s: framebuffer mode: %s preserved (CANCELLED on display-changed)", __FUNCTION__,
                hwc_fb_mode[ctx->disp_cli[HWC_PRIMARY_IX].mode]);
             ctx->disp_cli[HWC_PRIMARY_IX].mode_toggle = false;
          }
       }
    }
}

static void hwc_hotplug_notify(void *dev)
{
    NxClient_DisplaySettings settings;
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    DISPLAY_CLIENT_MODE newmode = hwc_fb_mode_for_format(ctx, &settings);
    int i;

    if (ctx) {
       if (ctx->toggle_fb_mode) {
          if (ctx->disp_cli[HWC_PRIMARY_IX].mode != newmode) {
             ALOGI("%s: framebuffer mode: %s -> %s (on hotplug)", __FUNCTION__,
                hwc_fb_mode[ctx->disp_cli[HWC_PRIMARY_IX].mode], hwc_fb_mode[newmode]);
             ctx->disp_cli[HWC_PRIMARY_IX].mode_toggle = true;
          } else if (ctx->disp_cli[HWC_PRIMARY_IX].mode_toggle) {
             ALOGI("%s: framebuffer mode: %s preserved (CANCELLED on hotplug)", __FUNCTION__,
                hwc_fb_mode[ctx->disp_cli[HWC_PRIMARY_IX].mode]);
             ctx->disp_cli[HWC_PRIMARY_IX].mode_toggle = false;
          }
       }

       if (ctx->procs && ctx->procs->invalidate != NULL) {
          if (!pthread_mutex_lock(&ctx->mutex)) {
             for (i = 0; i < NSC_GPX_CLIENTS_NUMBER; i++) {
                ctx->gpx_cli[i].last.layerhdl = NULL;
                ctx->gpx_cli[i].skip_set = false;
             }
             pthread_mutex_unlock(&ctx->mutex);
          }
          ALOGI("%s: refresh disp-%d on connected", __FUNCTION__, HWC_PRIMARY_IX);
          ctx->procs->invalidate(const_cast<hwc_procs_t *>(ctx->procs));
       }

       {
          NEXUS_HdmiOutputHandle handle;
          NEXUS_HdmiOutputBasicEdidData edid;
          NEXUS_VideoFormatInfo info;
          NEXUS_Error errCode;
          NEXUS_PlatformConfiguration *pConfig = (NEXUS_PlatformConfiguration *)BKNI_Malloc(sizeof(*pConfig));
          if (pConfig) {
             NEXUS_Platform_GetConfiguration(pConfig);
             handle = pConfig->outputs.hdmi[0]; /* always first output. */
             BKNI_Free(pConfig);
             errCode = NEXUS_HdmiOutput_GetBasicEdidData(handle, &edid);
             if (!errCode) {
                NEXUS_VideoFormat_GetInfo(settings.format, &info);

                int width = ctx->cfg[HWC_PRIMARY_IX].width;
                int height = ctx->cfg[HWC_PRIMARY_IX].height;
                if (width < 0 || height < 0)
                {
                   width = (int)info.width;
                   height = (int)info.height;
                }

                float x_dpi = edid.maxHorizSize ? (width * 1000.0 / ((float)edid.maxHorizSize * 0.39370)) : 0.0;
                float y_dpi = edid.maxVertSize  ? (height * 1000.0 / ((float)edid.maxVertSize * 0.39370)) : 0.0;

                ALOGI("%s: %d x % d (-> %d x %d) (pixel), %d x %d (cm) -> %.3f x %.3f (dpi)", __FUNCTION__,
                      info.width, info.height, width, height, edid.maxHorizSize, edid.maxVertSize, x_dpi / 1000.0, y_dpi / 1000.0);

                ctx->cfg[HWC_PRIMARY_IX].x_dpi = (int)x_dpi;
                ctx->cfg[HWC_PRIMARY_IX].y_dpi = (int)y_dpi;
             }
          }
       }
    }
}

static bool is_video_layer_locked(VIDEO_LAYER_VALIDATION *data)
{
   bool rc = false;
   NexusClientContext *client_context = NULL;

   if (data->layer->compositionType == HWC_OVERLAY) {
      int index = -1;
      if (data->sharedData == NULL) {
         ALOGE("%s: unexpected NULL overlay (%p) - verify caller.", __FUNCTION__, data->layer);
         goto out;
      }
      index = android_atomic_acquire_load(&(data->sharedData->videoWindow.windowIdPlusOne));
      if (index > 0) {
         client_context = reinterpret_cast<NexusClientContext *>(data->sharedData->videoWindow.nexusClientContext);
         if (client_context != NULL) {
            rc = true;
        }
      } else if (((data->sharedData->container.format == HAL_PIXEL_FORMAT_YV12) ||
                  (data->sharedData->container.format == HAL_PIXEL_FORMAT_YCbCr_420_888)) &&
                 (data->gr_usage & GRALLOC_USAGE_HW_COMPOSER)) {
         data->is_yuv = true;
         rc = true;
      }
   } else if (data->layer->compositionType == HWC_SIDEBAND) {
      if (data->scope == HWC_SCOPE_PREP) {
         client_context = (NexusClientContext*)(intptr_t)data->layer->sidebandStream->data[1];
         if (client_context != NULL) {
            rc = true;
         }
      } else if (data->scope == HWC_SCOPE_COMP) {
         if (data->sideband_client != 0) {
            rc = true;
         }
      }
      data->is_sideband = true;
   }

   if (rc) {
      data->layer->hints |= HWC_HINT_CLEAR_FB;
   }

out:
   return rc;
}

static int hwc_validate_layer_handle(VIDEO_LAYER_VALIDATION *data)
{
   int rc = 0;

   switch (data->layer->compositionType) {
   case HWC_OVERLAY:
      if (data->layer->handle && (private_handle_t::validate(data->layer->handle) != 0)) {
         rc = -1;
      }
   break;
   case HWC_SIDEBAND:
      if (data->scope == HWC_SCOPE_COMP) {
         if (data->sideband_client == 0) {
            rc = -2;
         }
      } else if (data->scope == HWC_SCOPE_PREP) {
         if (data->layer->sidebandStream && (data->layer->sidebandStream->data[1] == 0)) {
            rc = -3;
         }
      }
   break;
   default:
   break;
   }

   return rc;
}

static bool is_video_layer(struct hwc_context_t *ctx, VIDEO_LAYER_VALIDATION *data)
{
   bool rc = false;
   private_handle_t *gr_buffer = NULL;
   NEXUS_MemoryBlockHandle block_handle = NULL;
   NEXUS_Error lrc = NEXUS_SUCCESS;
   PSHARED_DATA pSharedData = NULL;
   void *pAddr;

   data->is_sideband = false;
   data->is_yuv = false;

   if (hwc_validate_layer_handle(data) != 0) {
      goto out;
   }

   if (((data->layer->compositionType == HWC_OVERLAY && data->layer->handle) ||
        (data->layer->compositionType == HWC_SIDEBAND && data->layer->sidebandStream))
       && !(data->layer->flags & HWC_SKIP_LAYER)) {
      if (data->layer->compositionType != HWC_SIDEBAND) {
         gr_buffer = (private_handle_t *)data->layer->handle;
         data->gr_usage = gr_buffer->usage;
         private_handle_t::get_block_handles(gr_buffer, &block_handle, NULL);
         lrc = hwc_mem_lock(ctx, block_handle, &pAddr, true);
         pSharedData = (PSHARED_DATA) pAddr;
         if ((lrc != NEXUS_SUCCESS) || pSharedData == NULL) {
            goto out;
         }
         data->sharedData = pSharedData;
      } else {
         lrc = NEXUS_NOT_INITIALIZED;
      }
      rc = is_video_layer_locked(data);
      if (lrc == NEXUS_SUCCESS) {
         hwc_mem_unlock(ctx, block_handle, true);
      }
   }

out:
   return rc;
}

static bool can_handle_downscale(struct hwc_context_t *ctx, hwc_layer_1_t *layer)
{
    bool ret = true;
    void *pAddr;
    int lrc = 0;
    PSHARED_DATA pSharedData = NULL;
    private_handle_t *gr_buffer = NULL;
    NEXUS_Rect source;
    NEXUS_Rect destination;
    NEXUS_MemoryBlockHandle block_handle = NULL;
    VIDEO_LAYER_VALIDATION video;

    if (!layer->handle) {
        goto out;
    }

    gr_buffer = (private_handle_t *)layer->handle;
    private_handle_t::get_block_handles(gr_buffer, &block_handle, NULL);
    lrc = hwc_mem_lock(ctx, block_handle, &pAddr, true);
    pSharedData = (PSHARED_DATA) pAddr;

    if (lrc || pSharedData == NULL) {
        goto out;
    }

    source.x = (int16_t)(int)layer->sourceCropf.left;
    source.y = (int16_t)(int)layer->sourceCropf.top;
    source.width = (uint16_t)((int)layer->sourceCropf.right - (int)layer->sourceCropf.left);
    source.height = (uint16_t)((int)layer->sourceCropf.bottom - (int)layer->sourceCropf.top);

    destination.x = (int16_t)(int)layer->displayFrame.left;
    destination.y = (int16_t)(int)layer->displayFrame.top;
    destination.width = (uint16_t)((int)layer->displayFrame.right - (int)layer->displayFrame.left);
    destination.height = (uint16_t)((int)layer->displayFrame.bottom - (int)layer->displayFrame.top);

    if ((source.x == destination.x) &&
        (source.y == destination.y) &&
        (source.width == destination.width) &&
        (source.height == destination.height)) {
        goto out_unlock;
    }

    video.scope = HWC_SCOPE_PREP;
    video.layer = layer;
    video.sharedData = pSharedData;
    video.gr_usage = gr_buffer->usage;
    if (is_video_layer_locked(&video)) {
       goto out_unlock;
    }

    if (source.width && destination.width &&
        ((source.width / destination.width) >= ctx->gfxCaps.maxHorizontalDownScale)) {
        ALOGV("%s: width: %d (%d) -> %d (>%d)", __FUNCTION__,
              source.width,
              pSharedData->container.width,
              destination.width,
              ctx->gfxCaps.maxHorizontalDownScale);
        ret = false;
        goto out_unlock;
    }

    if (source.height && destination.height &&
        ((source.height / destination.height) >= ctx->gfxCaps.maxVerticalDownScale)) {
        ALOGV("%s: height: %d (%d) -> %d (>%d)", __FUNCTION__,
              source.height,
              pSharedData->container.height,
              destination.height,
              ctx->gfxCaps.maxVerticalDownScale);
        ret = false;
        goto out_unlock;
    }

out_unlock:
    if (!lrc) hwc_mem_unlock(ctx, block_handle, true);
out:
    return ret;
}

static bool hwc_layer_seeds_output(bool skip_comp, NEXUS_SurfaceComposition *pComp,
    NEXUS_SurfaceHandle outputHdl)
{
    NEXUS_SurfaceCreateSettings settings;

    if (skip_comp) {
       return true;
    }

    if ((memcmp(&pComp->colorBlend, &nexusColorBlendingEquation[BLENDIND_TYPE_SRC_OVER], sizeof(pComp->colorBlend)) != 0) &&
        (memcmp(&pComp->colorBlend, &nexusColorBlendingEquation[BLENDIND_TYPE_SRC], sizeof(pComp->colorBlend)) != 0) &&
        (memcmp(&pComp->colorBlend, &nexusColorSrcOverConstAlpha, sizeof(pComp->colorBlend)) != 0)) {
       return false;
    }
    if ((memcmp(&pComp->alphaBlend, &nexusAlphaBlendingEquation[BLENDIND_TYPE_SRC_OVER], sizeof(pComp->alphaBlend)) != 0) &&
        (memcmp(&pComp->alphaBlend, &nexusAlphaBlendingEquation[BLENDIND_TYPE_SRC], sizeof(pComp->alphaBlend)) != 0) &&
        (memcmp(&pComp->alphaBlend, &nexusAlphaSrcOverConstAlpha, sizeof(pComp->colorBlend)) != 0)) {
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

    pComp = &ctx->gpx_cli[layer_id].composition;
    pComp->colorBlend = nexusColorBlendingEquation[blending];
    pComp->alphaBlend = nexusAlphaBlendingEquation[blending];
}

static void hwc_seed_disp_surface(struct hwc_context_t *ctx, NEXUS_SurfaceHandle surface,
   bool *q_ops, OPS_COUNT *ops_count, uint32_t background)
{
   NEXUS_Error rc;
   if (surface) {
      NEXUS_Graphics2DFillSettings fillSettings;
      NEXUS_Graphics2D_GetDefaultFillSettings(&fillSettings);
      fillSettings.surface = surface;
      fillSettings.color   = background;
      fillSettings.colorOp = NEXUS_FillOp_eCopy;
      fillSettings.alphaOp = NEXUS_FillOp_eCopy;
      if (pthread_mutex_lock(&ctx->g2d_mutex)) {
         ALOGE("%s: failed g2d_mutex!", __FUNCTION__);
         return;
      }
      rc = NEXUS_Graphics2D_Fill(ctx->hwc_g2d, &fillSettings);
      pthread_mutex_unlock(&ctx->g2d_mutex);
      if (rc == NEXUS_SUCCESS) {
         if (q_ops) {
            *q_ops = true;
         }
         if (ops_count) {
            ops_count->fill += 1;
         }
         if (ctx->last_seed == HWC_OPAQUE) {
            ctx->alpha_hole_background = false;
         }
         ctx->last_seed = background;
      }
   }
}

static uint32_t tick_color[2] = {0xFF00FF00, 0xFFFF00FF};
static void hwc_tick_disp_surface(struct hwc_context_t *ctx, NEXUS_SurfaceHandle surface)
{
   static int tick_ix = 0;
   NEXUS_Error rc;
   NEXUS_Graphics2DFillSettings fillSettings;
   NEXUS_Graphics2D_GetDefaultFillSettings(&fillSettings);
   fillSettings.surface     = surface;
   fillSettings.color       = tick_color[tick_ix];
   fillSettings.colorOp     = NEXUS_FillOp_eCopy;
   fillSettings.alphaOp     = NEXUS_FillOp_eCopy;
   fillSettings.rect.x      = ctx->cfg[HWC_PRIMARY_IX].width - (2*HWC_TICKER_W);
   fillSettings.rect.y      = ctx->cfg[HWC_PRIMARY_IX].height - (2*HWC_TICKER_H);
   fillSettings.rect.width  = HWC_TICKER_W;
   fillSettings.rect.height = HWC_TICKER_H;
   ++tick_ix %= 2;
   if (pthread_mutex_lock(&ctx->g2d_mutex)) {
      ALOGE("%s: failed g2d_mutex!", __FUNCTION__);
      return;
   }
   rc = NEXUS_Graphics2D_Fill(ctx->hwc_g2d, &fillSettings);
   if (rc == NEXUS_SUCCESS) {
      hwc_checkpoint_locked(ctx);
   }
   pthread_mutex_unlock(&ctx->g2d_mutex);
}

static uint32_t blank_color[2] = {0xFF00FF00, 0x80FF00FF};
static void hwc_blank_video_surface(struct hwc_context_t *ctx, NEXUS_SurfaceHandle surface)
{
   int i;
   NEXUS_Error rc;
   NEXUS_Graphics2DFillSettings fillSettings;
   NEXUS_Graphics2D_GetDefaultFillSettings(&fillSettings);
   fillSettings.surface     = surface;
   fillSettings.colorOp     = NEXUS_FillOp_eCopy;
   fillSettings.alphaOp     = NEXUS_FillOp_eCopy;
   int blanking = (ctx->blank_video > 1) ? 1 : 0;

   for (i = 0; i < NSC_MM_CLIENTS_NUMBER; i++) {
      if (ctx->mm_cli[i].root.composition.visible) {
         fillSettings.rect.x      = ctx->mm_cli[i].root.composition.position.x;
         fillSettings.rect.y      = ctx->mm_cli[i].root.composition.position.y;
         fillSettings.rect.width  = ctx->mm_cli[i].root.composition.position.width;
         fillSettings.rect.height = ctx->mm_cli[i].root.composition.position.height;
         fillSettings.color       = blank_color[blanking];
         if (pthread_mutex_lock(&ctx->g2d_mutex)) {
            ALOGE("%s: failed g2d_mutex!", __FUNCTION__);
            return;
         }
         rc = NEXUS_Graphics2D_Fill(ctx->hwc_g2d, &fillSettings);
         if (rc == NEXUS_SUCCESS) {
            hwc_checkpoint_locked(ctx);
         }
         pthread_mutex_unlock(&ctx->g2d_mutex);
      }
   }
   for (i = 0; i < NSC_SB_CLIENTS_NUMBER; i++) {
      if (ctx->sb_cli[i].root.composition.visible) {
         fillSettings.rect.x      = ctx->sb_cli[i].root.composition.position.x;
         fillSettings.rect.y      = ctx->sb_cli[i].root.composition.position.y;
         fillSettings.rect.width  = ctx->sb_cli[i].root.composition.position.width;
         fillSettings.rect.height = ctx->sb_cli[i].root.composition.position.height;
         fillSettings.color       = blank_color[blanking];
         if (pthread_mutex_lock(&ctx->g2d_mutex)) {
            ALOGE("%s: failed g2d_mutex!", __FUNCTION__);
            return;
         }
         rc = NEXUS_Graphics2D_Fill(ctx->hwc_g2d, &fillSettings);
         if (rc == NEXUS_SUCCESS) {
            hwc_checkpoint_locked(ctx);
         }
         pthread_mutex_unlock(&ctx->g2d_mutex);
      }
   }
}

static void hwc_sideband_alpha_hole(struct hwc_context_t *ctx, NEXUS_SurfaceHandle surface, OPS_COUNT *ops_count)
{
   int i;
   NEXUS_Error rc;
   NEXUS_Graphics2DFillSettings fillSettings;
   NEXUS_Graphics2D_GetDefaultFillSettings(&fillSettings);
   fillSettings.surface     = surface;
   fillSettings.colorOp     = NEXUS_FillOp_eCopy;
   fillSettings.alphaOp     = NEXUS_FillOp_eCopy;

   for (i = 0; i < NSC_SB_CLIENTS_NUMBER; i++) {
      if (ctx->sb_cli[i].root.composition.visible) {
         fillSettings.rect.x      = ctx->sb_cli[i].root.composition.position.x;
         fillSettings.rect.y      = ctx->sb_cli[i].root.composition.position.y;
         fillSettings.rect.width  = ctx->sb_cli[i].root.composition.position.width;
         fillSettings.rect.height = ctx->sb_cli[i].root.composition.position.height;
         fillSettings.color       = 0x00000000;
         if (pthread_mutex_lock(&ctx->g2d_mutex)) {
            ALOGE("%s: failed g2d_mutex!", __FUNCTION__);
            return;
         }
         rc = NEXUS_Graphics2D_Fill(ctx->hwc_g2d, &fillSettings);
         if (ops_count) {
            ops_count->fill += 1;
         }
         if (rc == NEXUS_SUCCESS) {
            hwc_checkpoint_locked(ctx);
         }
         pthread_mutex_unlock(&ctx->g2d_mutex);
      }
   }
}

static void hwc_pip_alpha_hole(struct hwc_context_t *ctx, NEXUS_SurfaceHandle surface, OPS_COUNT *ops_count)
{
   int i;
   NEXUS_Error rc;
   NEXUS_Graphics2DFillSettings fillSettings;
   NEXUS_Graphics2D_GetDefaultFillSettings(&fillSettings);
   fillSettings.surface     = surface;
   fillSettings.colorOp     = NEXUS_FillOp_eBlend;
   fillSettings.alphaOp     = NEXUS_FillOp_eCopy;

   for (i = 0; i < NSC_MM_CLIENTS_NUMBER; i++) {
      if (ctx->mm_cli[i].root.composition.visible) {
         fillSettings.rect.x      = ctx->mm_cli[i].root.composition.position.x;
         fillSettings.rect.y      = ctx->mm_cli[i].root.composition.position.y;
         fillSettings.rect.width  = ctx->mm_cli[i].root.composition.position.width;
         fillSettings.rect.height = ctx->mm_cli[i].root.composition.position.height;
         fillSettings.color       = 0x00000000;
         if (pthread_mutex_lock(&ctx->g2d_mutex)) {
            ALOGE("%s: failed g2d_mutex!", __FUNCTION__);
            return;
         }
         rc = NEXUS_Graphics2D_Fill(ctx->hwc_g2d, &fillSettings);
         if (ops_count) {
            ops_count->fill += 1;
         }
         if (rc == NEXUS_SUCCESS) {
            hwc_checkpoint_locked(ctx);
         }
         pthread_mutex_unlock(&ctx->g2d_mutex);
      }
   }
}

static void hwc_rel_tl_inc(struct hwc_context_t *ctx, int index, int layer)
{
   int sw_timeline = INVALID_FENCE;
   uint64_t start = 0, end = 0, created = 0;
   const long double nsec_to_msec = 1.0 / 1000000.0;

   sw_timeline = (index == HWC_PRIMARY_IX) ? ctx->gpx_cli[layer].rel_tl : ctx->vd_cli[layer].rel_tl;

   if (ctx->dump_fence & HWC_DUMP_FENCE_TICK) {
      start = hwc_tick();
      created = (index == HWC_PRIMARY_IX) ? ctx->gpx_cli[layer].rel_tl_tick : ctx->vd_cli[layer].rel_tl_tick;
   }

   if (sw_timeline != INVALID_FENCE) {
      sw_sync_timeline_inc(sw_timeline, 1);
   }

   if (ctx->dump_fence & HWC_DUMP_FENCE_TICK) {
      end = hwc_tick();
      ALOGI("%s: %llu - tl[%d]: %d, tick: %.7Lf, total: %.7Lf\n",
            (index == HWC_PRIMARY_IX) ? "fence" : "vfence",
            ctx->stats[index].set_call, layer, sw_timeline,
            nsec_to_msec * (end-start),
            created ? nsec_to_msec * (end-created) : 0);
   }
}

static void hwc_clear_acquire_release_fences(struct hwc_context_t* ctx, hwc_display_contents_1_t *list, int index)
{
   size_t i;

   for (i = 0; i < (size_t)index; i++) {
      if (list->hwLayers[i].acquireFenceFd != INVALID_FENCE) {
         close(list->hwLayers[i].acquireFenceFd);
         if (ctx->dump_fence & HWC_DUMP_FENCE_PRIM) {
            ALOGI("fence: %llu/%zu - acquire-fence: %d -> early-comp+close\n",
                  ctx->stats[HWC_PRIMARY_IX].set_call, i,
                  list->hwLayers[i].acquireFenceFd);
         }
         list->hwLayers[i].acquireFenceFd = INVALID_FENCE;
      }
      if (list->hwLayers[i].releaseFenceFd != INVALID_FENCE) {
         hwc_rel_tl_inc(ctx, HWC_PRIMARY_IX, i);
         list->hwLayers[i].releaseFenceFd = INVALID_FENCE;
         if (ctx->dump_fence & HWC_DUMP_FENCE_PRIM) {
            ALOGI("fence: %llu/%zu - timeline-release: %d -> early-inc\n",
                  ctx->stats[HWC_PRIMARY_IX].set_call, i, ctx->gpx_cli[i].rel_tl);
         }
      }
   }
}

bool hwc_compose_gralloc_buffer(
   struct hwc_context_t* ctx, hwc_display_contents_1_t *list,  int layer_id,
   PSHARED_DATA pSharedData, private_handle_t *gr_buffer, NEXUS_SurfaceHandle outputHdl,
   bool is_virtual, bool skip_comp, NEXUS_SurfaceHandle *pActSurf,
   NEXUS_SurfaceComposition *pComp, bool layer_seeds_output, int already_comp,
   bool *q_ops, OPS_COUNT *ops_count, int video_layer, int prior_bl)
{
    bool composed = false, blit_yv12, is_cursor_layer = false;
    NEXUS_Error rc;
    int adj;
    unsigned int cstride = 0, cr_offset = 0, cb_offset = 0;
    void *pAddr, *slock;
    NEXUS_SurfaceCreateSettings displaySurfaceSettings;
    NEXUS_Rect srcAdj, outAdj;
    NEXUS_Graphics2DBlitSettings blitSettings;
    int stats_ix = is_virtual ? HWC_VIRTUAL_IX : HWC_PRIMARY_IX;

    blit_yv12 = ((pSharedData->container.format == HAL_PIXEL_FORMAT_YV12) ||
                 (pSharedData->container.format == HAL_PIXEL_FORMAT_YCbCr_420_888)) ? true : false;

    if (!is_virtual) {
       if (skip_comp) {
          ALOGV("comp: %llu/%d - willingly skipping layer\n",
                ctx->stats[stats_ix].set_call, layer_id);
          goto out;
       }
       is_cursor_layer = (ctx->gpx_cli[layer_id].layer_flags & HWC_IS_CURSOR_LAYER) ? true : false;
    }

    NEXUS_Surface_GetCreateSettings(outputHdl, &displaySurfaceSettings);
    if ((is_virtual && (ctx->display_dump_virt & HWC_DUMP_LEVEL_COMPOSE)) ||
        (!is_virtual && (ctx->display_dump_layer & (HWC_DUMP_LEVEL_COMPOSE|HWC_DUMP_LEVEL_QOPS)))) {
       ALOGI("%s: %llu/%d - v:%u::f:%u::so:%s::al:0x%08x::bl:%x %ux%u (%ux%u@%u,%u) -> %ux%u@%u,%u",
             is_virtual ? "vcmp" : "comp",
             ctx->stats[stats_ix].set_call, layer_id,
             (unsigned)pComp->visible,
             gralloc_to_nexus_pixel_format(pSharedData->container.format),
             layer_seeds_output ? "yes" : "no",
             pComp->constantColor, ctx->gpx_cli[layer_id].blending_type,
             pSharedData->container.width, pSharedData->container.height,
             pComp->clipRect.width, pComp->clipRect.height, pComp->clipRect.x, pComp->clipRect.y,
             pComp->position.width, pComp->position.height, pComp->position.x, pComp->position.y);
    }

    *pActSurf = NULL;
    if (pComp->visible) {

        srcAdj = pComp->clipRect;
        outAdj = pComp->position;
        if (outAdj.x < 0) {
           adj = -outAdj.x;
           outAdj.x = 0;
           outAdj.width = (adj <= outAdj.width) ? outAdj.width - adj : 0;
           srcAdj.x += adj;
           srcAdj.width = (adj <= srcAdj.width) ? srcAdj.width - adj : 0;
        }
        if (outAdj.y < 0) {
           adj = -outAdj.y;
           outAdj.y = 0;
           outAdj.height = (adj <= outAdj.height) ? outAdj.height - adj : 0;
           srcAdj.y += adj;
           srcAdj.height = (adj <= srcAdj.height) ? srcAdj.height - adj : 0;
        }
        if (outAdj.x + (int)outAdj.width > (int)displaySurfaceSettings.width) {
           adj = (outAdj.x + outAdj.width) - displaySurfaceSettings.width;
           outAdj.width = (adj <= outAdj.width) ? outAdj.width - adj : 0;
           srcAdj.width = (adj <= srcAdj.width) ? srcAdj.width - adj : 0;
        }
        if (outAdj.y + (int)outAdj.height > (int)displaySurfaceSettings.height) {
           adj = (outAdj.y + outAdj.height) - displaySurfaceSettings.height;
           outAdj.height = (adj <= outAdj.height) ? outAdj.height - adj : 0;
           srcAdj.height = (adj <= srcAdj.height) ? srcAdj.height - adj : 0;
        }

        if (!(outAdj.width > 0 && outAdj.height > 0 &&
              srcAdj.width > 0 && srcAdj.height > 0)) {
           ALOGV("%s: %llu/%d - invalid, skipping src{%d,%d} -> dst{%d,%d}\n",
                 is_virtual ? "vcmp" : "comp",
                 ctx->stats[stats_ix].set_call, layer_id,
                 srcAdj.width, srcAdj.height, outAdj.width, outAdj.height);
           goto out;
        }

        if (blit_yv12) {
           NEXUS_SurfaceHandle srcCb, srcCr, srcY, dstYUV;
           void *buffer, *next;
           BM2MC_PACKET_Plane planeY, planeCb, planeCr, planeYCbCr, planeRgba;
           size_t size;

           BM2MC_PACKET_Blend combColor = {BM2MC_PACKET_BlendFactor_eSourceColor,
                                           BM2MC_PACKET_BlendFactor_eOne,
                                           false,
                                           BM2MC_PACKET_BlendFactor_eDestinationColor,
                                           BM2MC_PACKET_BlendFactor_eOne,
                                           false,
                                           BM2MC_PACKET_BlendFactor_eZero};
           BM2MC_PACKET_Blend copyColor = {BM2MC_PACKET_BlendFactor_eSourceColor,
                                           BM2MC_PACKET_BlendFactor_eOne,
                                           false,
                                           BM2MC_PACKET_BlendFactor_eZero,
                                           BM2MC_PACKET_BlendFactor_eZero,
                                           false,
                                           BM2MC_PACKET_BlendFactor_eZero};
           BM2MC_PACKET_Blend copyAlpha = {BM2MC_PACKET_BlendFactor_eSourceAlpha,
                                           BM2MC_PACKET_BlendFactor_eOne,
                                           false,
                                           BM2MC_PACKET_BlendFactor_eZero,
                                           BM2MC_PACKET_BlendFactor_eZero,
                                           false,
                                           BM2MC_PACKET_BlendFactor_eZero};

           srcY = hwc_to_nsc_surface(
                             pSharedData->container.width,
                             pSharedData->container.height,
                             pSharedData->container.stride,
                             NEXUS_PixelFormat_eY8,
                             pSharedData->container.block,
                             0);
           NEXUS_Surface_Lock(srcY, &slock);
           NEXUS_Surface_Flush(srcY);

           cstride = (pSharedData->container.stride/2 + (gr_buffer->alignment-1)) & ~(gr_buffer->alignment-1);
           cr_offset = pSharedData->container.stride * pSharedData->container.height;
           cb_offset = cr_offset + ((pSharedData->container.height/2) * ((pSharedData->container.stride/2 + (gr_buffer->alignment-1)) & ~(gr_buffer->alignment-1))),

           srcCr = hwc_to_nsc_surface(
                              pSharedData->container.width/2,
                              pSharedData->container.height/2,
                              cstride,
                              NEXUS_PixelFormat_eCr8,
                              pSharedData->container.block,
                              cr_offset);
           NEXUS_Surface_Lock(srcCr, &slock);
           NEXUS_Surface_Flush(srcCr);

           srcCb = hwc_to_nsc_surface(
                              pSharedData->container.width/2,
                              pSharedData->container.height/2,
                              cstride,
                              NEXUS_PixelFormat_eCb8,
                              pSharedData->container.block,
                              cb_offset);
           NEXUS_Surface_Lock(srcCb, &slock);
           NEXUS_Surface_Flush(srcCb);

           dstYUV = hwc_to_nsc_surface(
                              pSharedData->container.width,
                              pSharedData->container.height,
                              pSharedData->container.width * pSharedData->container.bpp,
                              NEXUS_PixelFormat_eY08_Cb8_Y18_Cr8,
                              0,
                              0);
           NEXUS_Surface_Lock(dstYUV, &slock);
           NEXUS_Surface_Flush(dstYUV);

           if (srcY != NULL && srcCr != NULL && srcCb != NULL && dstYUV != NULL) {
              if ((!layer_seeds_output || !ctx->smart_background) && !already_comp) {
                 hwc_seed_disp_surface(ctx, outputHdl, q_ops, ops_count, HWC_TRANSPARENT);
              }
              if (pthread_mutex_lock(&ctx->g2d_mutex)) {
                 ALOGE("%s: failed g2d_mutex!", __FUNCTION__);
              } else {
                 rc = NEXUS_Graphics2D_GetPacketBuffer(ctx->hwc_g2d, &buffer, &size, 1024);
                 if ((rc != NEXUS_SUCCESS) || !size) {
                    ALOGE("%s: failed getting packet buffer from g2d: (num:%d, id:0x%x)\n", __FUNCTION__,
                          NEXUS_GET_ERR_NUM(rc), NEXUS_GET_ERR_ID(rc));
                    pthread_mutex_unlock(&ctx->g2d_mutex);
                    goto out;
                 }
                 if ((q_ops != NULL) && !ctx->g2d_allow_simult) {
                    if (*q_ops == true) {
                       rc = hwc_checkpoint_locked(ctx);
                       if (rc)  {
                          ALOGW("%s: checkpoint timeout flushing blits...", __FUNCTION__);
                       } else {
                          *q_ops = false;
                          hwc_clear_acquire_release_fences(ctx, list, (layer_id > 0 ? layer_id - 1 : 0));
                       }
                    }
                 }
                 pthread_mutex_unlock(&ctx->g2d_mutex);

                 NEXUS_Surface_LockPlaneAndPalette(srcY, &planeY, NULL);
                 NEXUS_Surface_LockPlaneAndPalette(srcCb, &planeCb, NULL);
                 NEXUS_Surface_LockPlaneAndPalette(srcCr, &planeCr, NULL);
                 NEXUS_Surface_LockPlaneAndPalette(dstYUV, &planeYCbCr, NULL);
                 NEXUS_Surface_LockPlaneAndPalette(outputHdl, &planeRgba, NULL);

                 next = buffer;
                 {
                 BM2MC_PACKET_PacketFilterEnable *pPacket = (BM2MC_PACKET_PacketFilterEnable *)next;
                 BM2MC_PACKET_INIT(pPacket, FilterEnable, false );
                 pPacket->enable          = 0;
                 next = ++pPacket;
                 }
                 {
                 BM2MC_PACKET_PacketSourceFeeders *pPacket = (BM2MC_PACKET_PacketSourceFeeders *)next;
                 BM2MC_PACKET_INIT(pPacket, SourceFeeders, false );
                 pPacket->plane0          = planeCb;
                 pPacket->plane1          = planeCr;
                 pPacket->color           = 0;
                 next = ++pPacket;
                 }
                 {
                 BM2MC_PACKET_PacketDestinationFeeder *pPacket = (BM2MC_PACKET_PacketDestinationFeeder *)next;
                 BM2MC_PACKET_INIT(pPacket, DestinationFeeder, false );
                 pPacket->plane           = planeY;
                 pPacket->color           = 0;
                 next = ++pPacket;
                 }
                 {
                 BM2MC_PACKET_PacketOutputFeeder *pPacket = (BM2MC_PACKET_PacketOutputFeeder *)next;
                 BM2MC_PACKET_INIT(pPacket, OutputFeeder, false);
                 pPacket->plane           = planeYCbCr;
                 next = ++pPacket;
                 }
                 {
                 BM2MC_PACKET_PacketBlend *pPacket = (BM2MC_PACKET_PacketBlend *)next;
                 BM2MC_PACKET_INIT( pPacket, Blend, false );
                 pPacket->color_blend     = combColor;
                 pPacket->alpha_blend     = copyAlpha;
                 pPacket->color           = 0;
                 next = ++pPacket;
                 }
                 {
                 BM2MC_PACKET_PacketScaleBlendBlit *pPacket = (BM2MC_PACKET_PacketScaleBlendBlit *)next;
                 BM2MC_PACKET_INIT(pPacket, ScaleBlendBlit, true);
                 pPacket->src_rect.x      = 0;
                 pPacket->src_rect.y      = 0;
                 pPacket->src_rect.width  = planeCb.width;
                 pPacket->src_rect.height = planeCb.height;
                 pPacket->out_rect.x      = 0;
                 pPacket->out_rect.y      = 0;
                 pPacket->out_rect.width  = planeYCbCr.width;
                 pPacket->out_rect.height = planeYCbCr.height;
                 pPacket->dst_point.x     = 0;
                 pPacket->dst_point.y     = 0;
                 next = ++pPacket;
                 }
                 {
                 BM2MC_PACKET_PacketSourceFeeder *pPacket = (BM2MC_PACKET_PacketSourceFeeder *)next;
                 BM2MC_PACKET_INIT(pPacket, SourceFeeder, false );
                 pPacket->plane           = planeYCbCr;
                 pPacket->color           = 0xFF000000; /* add alpha, opaque. */
                 next = ++pPacket;
                 }
                 {
                 BM2MC_PACKET_PacketSourceControl *pPacket = (BM2MC_PACKET_PacketSourceControl *)next;
                 BM2MC_PACKET_INIT(pPacket, SourceControl, false);
                 pPacket->chroma_filter   = true;
                 next = ++pPacket;
                 }
                 {
                 BM2MC_PACKET_PacketDestinationNone *pPacket = (BM2MC_PACKET_PacketDestinationNone *)next;
                 BM2MC_PACKET_INIT(pPacket, DestinationNone, false);
                 next = ++pPacket;
                 }
                 {
                 BM2MC_PACKET_PacketOutputFeeder *pPacket = (BM2MC_PACKET_PacketOutputFeeder *)next;
                 BM2MC_PACKET_INIT(pPacket, OutputFeeder, false);
                 pPacket->plane           = planeRgba;
                 next = ++pPacket;
                 }
                 {
                 BM2MC_PACKET_PacketOutputControl *pPacket = (BM2MC_PACKET_PacketOutputControl *)next;
                 BM2MC_PACKET_INIT(pPacket, OutputControl, false);
                 pPacket->chroma_filter    = true;
                 next = ++pPacket;
                 }
                 {
                 BM2MC_PACKET_PacketBlend *pPacket = (BM2MC_PACKET_PacketBlend *)next;
                 BM2MC_PACKET_INIT( pPacket, Blend, false );
                 pPacket->color_blend     = copyColor;
                 pPacket->alpha_blend     = copyAlpha;
                 pPacket->color           = 0;
                 next = ++pPacket;
                 }
                 {
                 BM2MC_PACKET_PacketFilterEnable *pPacket = (BM2MC_PACKET_PacketFilterEnable *)next;
                 BM2MC_PACKET_INIT(pPacket, FilterEnable, false);
                 pPacket->enable          = 1;
                 next = ++pPacket;
                 }
                 {
                 BM2MC_PACKET_PacketSourceColorMatrixEnable *pPacket = (BM2MC_PACKET_PacketSourceColorMatrixEnable *)next;
                 BM2MC_PACKET_INIT(pPacket, SourceColorMatrixEnable, false);
                 pPacket->enable          = 1;
                 next = ++pPacket;
                 }
                 {
                 BM2MC_PACKET_PacketSourceColorMatrix *pPacket = (BM2MC_PACKET_PacketSourceColorMatrix *)next;
                 BM2MC_PACKET_INIT(pPacket, SourceColorMatrix, false);
                 NEXUS_Graphics2D_ConvertColorMatrix(&g_hwc_ai32_Matrix_YCbCrtoRGB, &pPacket->matrix);
                 next = ++pPacket;
                 }
                 {
                 BM2MC_PACKET_PacketScaleBlit *pPacket = (BM2MC_PACKET_PacketScaleBlit *)next;
                 BM2MC_PACKET_INIT(pPacket, ScaleBlit, true);
                 pPacket->src_rect.x       = 0;
                 pPacket->src_rect.y       = 0;
                 pPacket->src_rect.width   = planeYCbCr.width;
                 pPacket->src_rect.height  = planeYCbCr.height;
                 pPacket->out_rect.x       = outAdj.x;
                 pPacket->out_rect.y       = outAdj.y;
                 pPacket->out_rect.width   = outAdj.width;
                 pPacket->out_rect.height  = outAdj.height;
                 next = ++pPacket;
                 }
                 {
                 BM2MC_PACKET_PacketSourceColorMatrixEnable *pPacket = (BM2MC_PACKET_PacketSourceColorMatrixEnable *)next;
                 BM2MC_PACKET_INIT(pPacket, SourceColorMatrixEnable, false);
                 pPacket->enable          = 0;
                 next = ++pPacket;
                 }

                 if (ctx->display_dump_layer & HWC_DUMP_LEVEL_COMPOSE) {
                    ALOGI("%s: %llu/%d - y(%dx%d,s:%d,f:%d):cr(%dx%d,s:%d,f:%d,o:%d):cb(%dx%d,s:%d,f:%d,o:%d) => yuv(%dx%d,s:%d,f:%d) => rgba(%dx%d,s:%d,f:%d) @ src(0,0,%dx%d):dst(%d,%d,%dx%d)",
                       is_virtual ? "vcmp" : "comp",
                       ctx->stats[stats_ix].set_call, layer_id,
                       planeY.width, planeY.height, planeY.pitch, planeY.format,
                       planeCr.width, planeCr.height, planeCr.pitch, planeCr.format, cr_offset,
                       planeCb.width, planeCb.height, planeCb.pitch, planeCb.format, cb_offset,
                       planeYCbCr.width, planeYCbCr.height, planeYCbCr.pitch, planeYCbCr.format,
                       planeRgba.width, planeRgba.height, planeRgba.pitch, planeRgba.format,
                       planeYCbCr.width, planeYCbCr.height,
                       outAdj.x, outAdj.y, outAdj.width, outAdj.height);
                 }

                 if (pthread_mutex_lock(&ctx->g2d_mutex)) {
                    ALOGE("%s: failed g2d_mutex!", __FUNCTION__);
                 } else {
                    rc = NEXUS_Graphics2D_PacketWriteComplete(ctx->hwc_g2d, (uint8_t*)next - (uint8_t*)buffer);
                    if (rc != NEXUS_SUCCESS) {
                       ALOGE("%s: failed writting packet buffer: (num:%d, id:0x%x)\n", __FUNCTION__,
                             NEXUS_GET_ERR_NUM(rc), NEXUS_GET_ERR_ID(rc));
                    } else {
                       if (!ctx->g2d_allow_simult || ctx->fence_support) {
                          rc = hwc_checkpoint_locked(ctx);
                          if (rc)  {
                             ALOGW("%s: checkpoint timeout composing layer %u", __FUNCTION__, layer_id);
                          } else if (q_ops) {
                             *q_ops = false;
                          }
                       } else if (q_ops) {
                          *q_ops = true;
                       }
                    }
                    if (rc) {
                       ALOGE("%s: %llu/%d - failed layer completion",
                             is_virtual ? "vcmp" : "comp",
                             ctx->stats[stats_ix].set_call, layer_id);
                    } else {
                       composed = true;
                       if (ops_count) {
                          ops_count->yv12 += 1;
                       }
                       if (!ctx->g2d_allow_simult || ctx->fence_support) {
                          hwc_clear_acquire_release_fences(ctx, list, layer_id);
                       }
                    }
                    pthread_mutex_unlock(&ctx->g2d_mutex);
                 }

                 NEXUS_Surface_UnlockPlaneAndPalette(srcCb);
                 NEXUS_Surface_UnlockPlaneAndPalette(srcCr);
                 NEXUS_Surface_UnlockPlaneAndPalette(srcY);
                 NEXUS_Surface_UnlockPlaneAndPalette(dstYUV);
                 NEXUS_Surface_UnlockPlaneAndPalette(outputHdl);
              }
           }

           if (srcCb != NULL) {
              NEXUS_Surface_Unlock(srcCb);
              NEXUS_Surface_Destroy(srcCb);
           }
           if (srcCr != NULL) {
              NEXUS_Surface_Unlock(srcCr);
              NEXUS_Surface_Destroy(srcCr);
           }
           if (srcY != NULL) {
              NEXUS_Surface_Unlock(srcY);
              NEXUS_Surface_Destroy(srcY);
           }
           if (dstYUV != NULL) {
              NEXUS_Surface_Unlock(dstYUV);
              NEXUS_Surface_Destroy(dstYUV);
           }
        } else {
           *pActSurf = hwc_to_nsc_surface(pSharedData->container.width,
                                       pSharedData->container.height,
                                       gr_buffer->oglStride,
                                       gralloc_to_nexus_pixel_format(pSharedData->container.format),
                                       pSharedData->container.block,
                                       0);

           if (*pActSurf != NULL) {
              if ((ctx->gpx_cli[layer_id].blending_type == BLENDIND_TYPE_SRC_OVER) &&
                  (prior_bl == BLENDIND_TYPE_SRC_OVER || prior_bl == BLENDIND_TYPE_LAST) &&
                  (pComp->constantColor != HWC_OPAQUE)) {
                 pComp->colorBlend.d = NEXUS_BlendFactor_eInverseConstantAlpha;
                 pComp->alphaBlend.d = NEXUS_BlendFactor_eInverseConstantAlpha;
              }
              NEXUS_Graphics2D_GetDefaultBlitSettings(&blitSettings);
              blitSettings.source.surface = *pActSurf;
              blitSettings.source.rect    = srcAdj;
              blitSettings.dest.surface   = outputHdl;
              blitSettings.output.surface = outputHdl;
              blitSettings.output.rect    = outAdj;
              blitSettings.colorOp        = NEXUS_BlitColorOp_eUseBlendEquation;
              blitSettings.alphaOp        = NEXUS_BlitAlphaOp_eUseBlendEquation;
              blitSettings.colorBlend     = pComp->colorBlend;
              blitSettings.alphaBlend     = pComp->alphaBlend;
              blitSettings.dest.rect      = blitSettings.output.rect;
              blitSettings.constantColor  = pComp->constantColor;

              if (!already_comp) {
                 if (!ctx->smart_background) {
                    hwc_seed_disp_surface(ctx, outputHdl, q_ops, ops_count, HWC_TRANSPARENT);
                 } else {
                    if (!layer_seeds_output || (ctx->flush_background && video_layer)) {
                       hwc_seed_disp_surface(ctx, outputHdl, q_ops, ops_count,
                                             video_layer?HWC_TRANSPARENT:HWC_OPAQUE);
                       if (ctx->flush_background) {
                          ctx->flush_background = false;
                       }
                    }
                 }
              }

              if (pthread_mutex_lock(&ctx->g2d_mutex)) {
                 ALOGE("%s: failed g2d_mutex!", __FUNCTION__);
              } else {
                 rc = NEXUS_Graphics2D_Blit(ctx->hwc_g2d, &blitSettings);
                 if (rc == NEXUS_GRAPHICS2D_QUEUE_FULL) {
                    rc = hwc_checkpoint_locked(ctx);
                    if (rc) {
                       ALOGW("%s: checkpoint timeout composing layer %u", __FUNCTION__, layer_id);
                    } else {
                       hwc_clear_acquire_release_fences(ctx, list, (layer_id > 0 ? layer_id - 1 : 0));
                    }
                    rc = NEXUS_Graphics2D_Blit(ctx->hwc_g2d, &blitSettings);
                 }
                 if (rc) {
                    ALOGE("%s: failed to blit layer %u", __FUNCTION__, layer_id);
                 } else {
                    composed = true;
                    if (ops_count) {
                       ops_count->blit += 1;
                    }
                    if (q_ops) {
                       *q_ops = true;
                       if (ctx->fence_support) {
                          rc = hwc_checkpoint_locked(ctx);
                          if (rc)  {
                             ALOGW("%s: checkpoint timeout forcing layer %d composition", __FUNCTION__, layer_id);
                          } else {
                             *q_ops = false;
                             hwc_clear_acquire_release_fences(ctx, list, layer_id);
                          }
                       }
                    }
                 }
                 pthread_mutex_unlock(&ctx->g2d_mutex);
              }
           }
        }
    }

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

    if (!rc && (ctx->display_dump_layer & HWC_DUMP_LEVEL_CLASSIFY)) {
       if (total_layers == 1) {
          ALOGI("comp: %llu - skip-single - reason %d", ctx->stats[HWC_PRIMARY_IX].prepare_call, skip_layer);
       } else {
          ALOGI("comp: %llu - skip-layer (%d:%d:%d:%d) - reason %d", ctx->stats[HWC_PRIMARY_IX].prepare_call, layer->compositionType,
                ctx->display_gles_always, ctx->display_gles_fallback, ctx->needs_fb_target, skip_layer);
       }
    }
    return rc;
}

static void hwc_prepare_gpx_layer(
    struct hwc_context_t* ctx, hwc_layer_1_t *layer, int layer_id,
    bool geometry_changed, bool is_virtual, bool is_locked,
    unsigned int *skip_set, unsigned int *skip_cand)
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
    int cur_width = 0, cur_height = 0, lrc = 0;
    unsigned int cur_blending_type;
    bool layer_changed = false;
    NEXUS_MemoryBlockHandle block_handle = NULL;

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
    private_handle_t::get_block_handles(gr_buffer, &block_handle, NULL);
    lrc = hwc_mem_lock(ctx, block_handle, &pAddr, true);
    PSHARED_DATA pSharedData = (PSHARED_DATA) pAddr;
    if (lrc || pSharedData == NULL) {
       ALOGE("%s: invalid buffer on layer_id %d", __FUNCTION__, layer_id);
       goto out;
    }
    cur_width  = pSharedData->container.width;
    cur_height = pSharedData->container.height;
    cur_blending_type = hwc_android_blend_to_nsc_blend(layer->blending);

    if (!is_locked) {
       if (pthread_mutex_lock(&ctx->mutex)) {
           goto out_unlock;
       }
    }

    ctx->gpx_cli[layer_id].skip_set = false;
    if ((layer->compositionType == HWC_OVERLAY) &&
        !(layer->flags & HWC_IS_CURSOR_LAYER)) {
       if (skip_cand) {
          *skip_cand += 1;
       }
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

       if (ctx->disp_cli[HWC_PRIMARY_IX].mode == CLIENT_MODE_COMP_BYPASS) {
          disp_position.x += ctx->overscan_position.x;
          disp_position.y += ctx->overscan_position.y;
          disp_position.width = (int)disp_position.width + (((int)disp_position.width * (int)ctx->overscan_position.w)/ctx->cfg[HWC_PRIMARY_IX].width);
          disp_position.height = (int)disp_position.height + (((int)disp_position.height * (int)ctx->overscan_position.h)/ctx->cfg[HWC_PRIMARY_IX].height);
       }

       if ((memcmp((void *)&disp_position, (void *)&ctx->gpx_cli[layer_id].composition.position, sizeof(NEXUS_Rect)) != 0) ||
           (memcmp((void *)&clip_position, (void *)&ctx->gpx_cli[layer_id].composition.clipRect, sizeof(NEXUS_Rect)) != 0) ||
           (cur_width != ctx->gpx_cli[layer_id].composition.clipBase.width) ||
           (cur_height != ctx->gpx_cli[layer_id].composition.clipBase.height) ||
           (cur_blending_type != ctx->gpx_cli[layer_id].blending_type) ||
           ((NEXUS_Pixel)(layer->planeAlpha << ALPHA_SHIFT) != ctx->gpx_cli[layer_id].composition.constantColor)) {
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
        pComp->virtualDisplay.width  = ctx->cfg[is_virtual?HWC_VIRTUAL_IX:HWC_PRIMARY_IX].width;
        pComp->virtualDisplay.height = ctx->cfg[is_virtual?HWC_VIRTUAL_IX:HWC_PRIMARY_IX].height;
        pComp->colorBlend            = ((cur_blending_type == BLENDIND_TYPE_SRC_OVER) && layer->planeAlpha) ? nexusColorSrcOverConstAlpha : nexusColorBlendingEquation[cur_blending_type];
        pComp->alphaBlend            = ((cur_blending_type == BLENDIND_TYPE_SRC_OVER) && layer->planeAlpha) ? nexusAlphaSrcOverConstAlpha : nexusAlphaBlendingEquation[cur_blending_type];
        pComp->visible               = true;
        pComp->clipRect.x            = clip_position.x;
        pComp->clipRect.y            = clip_position.y;
        pComp->clipRect.width        = clip_position.width;
        pComp->clipRect.height       = clip_position.height;
        pComp->clipBase.width        = cur_width;
        pComp->clipBase.height       = cur_height;
        pComp->constantColor         = (NEXUS_Pixel)layer->planeAlpha << ALPHA_SHIFT;

        if (!is_virtual) {
           ctx->gpx_cli[layer_id].blending_type = cur_blending_type;
        }
    }

out_mutex:
    if (!is_locked) {
       pthread_mutex_unlock(&ctx->mutex);
    }
out_unlock:
    if (!lrc) hwc_mem_unlock(ctx, block_handle, true);
out:
    return;
}

static void primary_composition_setup(struct hwc_context_t *ctx, hwc_display_contents_1_t* list)
{
    size_t i;
    hwc_layer_1_t *layer;
    int skip_layer_index = -1;
    bool has_video = false;
    VIDEO_LAYER_VALIDATION video;

    if (ctx->display_dump_layer & HWC_DUMP_LEVEL_CLASSIFY) {
       ALOGI("comp: %llu - classify: %zu, geom: %d",
          ctx->stats[HWC_PRIMARY_IX].prepare_call, list->numHwLayers, (bool)(list->flags & HWC_GEOMETRY_CHANGED));
    }

    ctx->needs_fb_target = false;
    for (i = 0; i < list->numHwLayers; i++) {
        layer = &list->hwLayers[i];
        if (ctx->display_dump_layer & HWC_DUMP_LEVEL_CLASSIFY) {
           ALOGI("comp: %llu - in-layer: %zu, comp: %d",
              ctx->stats[HWC_PRIMARY_IX].prepare_call, i, layer->compositionType);
        }
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
                 if (ctx->display_gles_always ||
                     (ctx->display_gles_fallback && !can_handle_downscale(ctx, layer))) {
                    layer->compositionType = HWC_FRAMEBUFFER;
                 }
              }
           }
        }
        if (ctx->display_dump_layer & HWC_DUMP_LEVEL_CLASSIFY) {
           ALOGI("comp: %llu - out-layer: %zu, comp: %d",
              ctx->stats[HWC_PRIMARY_IX].prepare_call, i, layer->compositionType);
        }
    }

    ctx->prepare_video = 0;
    ctx->prepare_sideband = 0;
    video.scope = HWC_SCOPE_PREP;
    for (i = 0; i < list->numHwLayers; i++) {
       layer = &list->hwLayers[i];
       video.layer = layer;
       if (is_video_layer(ctx, &video)) {
          has_video = true;
          if (video.is_sideband) {
             ctx->prepare_sideband++;
          } else {
             ctx->prepare_video++;
          }
       }
    }

    if (ctx->display_gles_fallback ||
        (ctx->display_gles_always && !has_video)) {
       for (i = 0; i < list->numHwLayers; i++) {
          layer = &list->hwLayers[i];
          if (layer->compositionType == HWC_FRAMEBUFFER) {
              ctx->needs_fb_target = true;
              break;
          }
       }
    }

    if ((ctx->display_gles_fallback && (skip_layer_index != -1)) ||
        (ctx->display_gles_always && !has_video) ||
        ctx->needs_fb_target) {
       for (i = 0; i < list->numHwLayers; i++) {
          layer = &list->hwLayers[i];
          video.layer = layer;
          if (layer->compositionType == HWC_OVERLAY &&
              !is_video_layer(ctx, &video)) {
             layer->compositionType = HWC_FRAMEBUFFER;
             layer->hints = 0;
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
    unsigned int skip_cand = 0;
    size_t overlay_layers = 0;

    ctx->stats[HWC_PRIMARY_IX].prepare_call += 1;

    if (pthread_mutex_lock(&ctx->power_mutex)) {
        ALOGE("%s: Could not acquire power_mutex!!!", __FUNCTION__);
        ctx->stats[HWC_PRIMARY_IX].prepare_skipped += 1;
        goto out;
    }

    if ((ctx->power_mode != HWC_POWER_MODE_OFF) && (ctx->power_mode != HWC_POWER_MODE_DOZE_SUSPEND)) {
        pthread_mutex_unlock(&ctx->power_mutex);

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
                if (ctx->display_dump_layer & HWC_DUMP_LEVEL_PREPARE) {
                   unsigned handle_dump = 0;
                   int lrc = 0;
                   if (layer->handle != NULL) {
                      NEXUS_MemoryBlockHandle block_handle = NULL;
                      NEXUS_Addr pAddr;
                      private_handle_t *gr_buffer = (private_handle_t *)layer->handle;
                      private_handle_t::get_block_handles(gr_buffer, &block_handle, NULL);
                      lrc = hwc_mem_lock_phys(ctx, block_handle, &pAddr);
                      handle_dump = (unsigned)pAddr;
                      if (lrc == NEXUS_SUCCESS) {
                         hwc_mem_unlock_phys(ctx, block_handle);
                      }
                   }
                   ALOGI("comp: %llu - show - sf:%d (%d) - hdl:0x%x", ctx->stats[HWC_PRIMARY_IX].prepare_call, (int)i, (int)layer->compositionType, handle_dump);
                }
                hwc_nsc_prepare_layer(ctx, layer, (int)i,
                                      (bool)(list->flags & HWC_GEOMETRY_CHANGED),
                                      &video_layer_id,
                                      &sideband_layer_id,
                                      &skip_set,
                                      &skip_cand);
            } else {
                if (ctx->display_dump_layer & HWC_DUMP_LEVEL_PREPARE) {
                   ALOGI("comp: %llu - hiding - sf:%d (%d)", ctx->stats[HWC_PRIMARY_IX].prepare_call, (int)i, (int)layer->compositionType);
                }
                hwc_hide_unused_gpx_layer(ctx, i);
            }
        }

        if (skip_set < skip_cand) {
           if (!pthread_mutex_lock(&ctx->mutex)) {
              for (i = 0; i < list->numHwLayers; i++) {
                 ctx->gpx_cli[i].skip_set = false;
              }
              pthread_mutex_unlock(&ctx->mutex);
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
        pthread_mutex_unlock(&ctx->power_mutex);
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
    NEXUS_Error lrc;

    if (!list) {
       /* !!not an error, just nothing to do.*/
       return 0;
    }

    ctx->stats[HWC_VIRTUAL_IX].prepare_call += 1;

    if (pthread_mutex_lock(&ctx->mutex)) {
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
       NEXUS_MemoryBlockHandle block_handle = NULL;
       private_handle_t *gr_buffer = (private_handle_t *)list->outbuf;
       private_handle_t::get_block_handles(gr_buffer, &block_handle, NULL);
       lrc = hwc_mem_lock(ctx, block_handle, &pAddr, true);
       PSHARED_DATA pSharedData = (PSHARED_DATA) pAddr;
       if (pSharedData != NULL) {
          ctx->cfg[HWC_VIRTUAL_IX].width = pSharedData->container.width;
          ctx->cfg[HWC_VIRTUAL_IX].height = pSharedData->container.height;
       }
       if (!lrc) hwc_mem_unlock(ctx, block_handle, true);
    }

    if (ctx->display_gles_always || ctx->display_gles_virtual ||
        (ctx->cfg[HWC_VIRTUAL_IX].width == -1 && ctx->cfg[HWC_VIRTUAL_IX].height == -1)) {
       for (i = 0; i < list->numHwLayers; i++) {
          layer = &list->hwLayers[i];
          if ((layer->compositionType == HWC_FRAMEBUFFER) || (layer->compositionType == HWC_OVERLAY)) {
             layer->compositionType = HWC_FRAMEBUFFER;
          }
       }
       goto out_unlock;
    }

    for (i = 0; i < list->numHwLayers; i++) {
       layer = &list->hwLayers[i];
       if (ctx->display_dump_virt & HWC_DUMP_LEVEL_PREPARE) {
          ALOGI("vcmp: %llu - %dx%d, layer: %zu, kind: %d",
                ctx->stats[HWC_VIRTUAL_IX].prepare_call,
                ctx->cfg[HWC_VIRTUAL_IX].width, ctx->cfg[HWC_VIRTUAL_IX].height,
                i, layer->compositionType);
       }
       if ((layer->compositionType == HWC_FRAMEBUFFER) || (layer->compositionType == HWC_OVERLAY)) {
          hwc_prepare_gpx_layer(ctx, layer, layer_id, false /*don't care*/, true, true, NULL, NULL);
          layer_id++;
          layer->compositionType = HWC_OVERLAY;
       }
    }

    if (ctx->display_dump_virt & HWC_DUMP_LEVEL_PREPARE) {
       ALOGI("vcmp: %llu - %dx%d, composing %d layers",
             ctx->stats[HWC_VIRTUAL_IX].prepare_call,
             ctx->cfg[HWC_VIRTUAL_IX].width, ctx->cfg[HWC_VIRTUAL_IX].height,
             layer_id);
    }

out_unlock:
    pthread_mutex_unlock(&ctx->mutex);
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
      if (!pthread_mutex_lock(&ctx->disp_cli[disp_ix].fifo_mutex)) {
         NEXUS_SurfaceHandle *pElement = BFIFO_WRITE(&ctx->disp_cli[disp_ix].display_fifo);
         *pElement = surface;
         BFIFO_WRITE_COMMIT(&ctx->disp_cli[disp_ix].display_fifo, 1);
         pthread_mutex_unlock(&ctx->disp_cli[disp_ix].fifo_mutex);
      } else {
         ALOGW("%s: leaked surface %p (disp %d)!", __FUNCTION__, surface, disp_ix);
      }
   }
}

static void hwc_ret_tl_inc(struct hwc_context_t *ctx, int index, int inc, bool err)
{
   uint64_t tick = 0, action;
   const long double nsec_to_msec = 1.0 / 1000000.0;

   if (!ctx->fence_retire) {
      return;
   }

   if (ctx->dump_fence & HWC_DUMP_FENCE_PRIM) {
      ALOGI("%s: %llu - tl: %d, inc: %d, on-err: %d\n",
            (index == HWC_PRIMARY_IX) ? "fence" : "vfence",
            ctx->stats[index].set_call, ctx->ret_tl[index], inc, err);
   }

   if (ctx->dump_fence & HWC_DUMP_FENCE_TICK) {
      tick = hwc_tick();
   }

   if (inc && ctx->ret_tl[index] != INVALID_FENCE) {
      sw_sync_timeline_inc(ctx->ret_tl[index], (unsigned) inc);
   }

   if (ctx->dump_fence & HWC_DUMP_FENCE_TICK) {
      action = (hwc_tick() - tick);
      ALOGI("%s: %llu - tl: %d, tick: %.7Lf\n",
            (index == HWC_PRIMARY_IX) ? "fence" : "vfence",
            ctx->stats[index].set_call, ctx->ret_tl[index], nsec_to_msec * action);
   }
}

static bool hwc_valid_surface_now(struct hwc_context_t* ctx, int disp_ix, NEXUS_SurfaceHandle surface)
{
   int j;

   for (j = 0; j < HWC_NUM_DISP_BUFFERS; j++) {
      if (ctx->disp_cli[disp_ix].display_buffers[j].fifo_surf == surface) {
         return true;
      }
   }

   ALOGI("disp: %d - stale surface (%p) recycling...", disp_ix, surface);
   return false;
}

static void hwc_recycle_now(struct hwc_context_t* ctx)
{
   NEXUS_SurfaceHandle surfaces[HWC_NUM_DISP_BUFFERS];
   NEXUS_Error rc;
   size_t numSurfaces, i;

   numSurfaces = 0;
   rc = NEXUS_SurfaceClient_RecycleSurface(ctx->disp_cli[HWC_PRIMARY_IX].schdl, surfaces, HWC_NUM_DISP_BUFFERS, &numSurfaces);
   if (rc) {
      ALOGW("%s: error recycling %p %d", __FUNCTION__, ctx->disp_cli[HWC_PRIMARY_IX].schdl, rc);
   } else if ( numSurfaces > 0 ) {
      if (!pthread_mutex_lock(&ctx->disp_cli[HWC_PRIMARY_IX].fifo_mutex)) {
         for (i = 0; i < numSurfaces; i++) {
            if (hwc_valid_surface_now(ctx, HWC_PRIMARY_IX, surfaces[i])) {
               NEXUS_SurfaceHandle *pElement = BFIFO_WRITE(&ctx->disp_cli[HWC_PRIMARY_IX].display_fifo);
               *pElement = surfaces[i];
               BFIFO_WRITE_COMMIT(&ctx->disp_cli[HWC_PRIMARY_IX].display_fifo, 1);
            }
         }
         pthread_mutex_unlock(&ctx->disp_cli[HWC_PRIMARY_IX].fifo_mutex);
      } else {
         ALOGW("%s: lost surfaces!", __FUNCTION__);
      }
   }
   hwc_ret_tl_inc(ctx, HWC_PRIMARY_IX, (int)numSurfaces, false);
}

#define HWC_SURFACE_WAIT_TIMEOUT 50
#define HWC_SURFACE_WAIT_ATTEMPT 4
static NEXUS_SurfaceHandle hwc_get_disp_surface(struct hwc_context_t *ctx, int disp_ix)
{
    int no_surface_count = 0;
    NEXUS_SurfaceHandle *pElement = NULL;

    if (disp_ix > (DISPLAY_SUPPORTED-1)) {
       return NULL;
    }

    while (true) {
       if (!pthread_mutex_lock(&ctx->disp_cli[disp_ix].fifo_mutex)) {
          if (BFIFO_READ_PEEK(&ctx->disp_cli[disp_ix].display_fifo) != 0) {
             pthread_mutex_unlock(&ctx->disp_cli[disp_ix].fifo_mutex);
             goto out_read;
          } else {
             pthread_mutex_unlock(&ctx->disp_cli[disp_ix].fifo_mutex);
             hwc_recycle_now(ctx);
             if (BFIFO_READ_PEEK(&ctx->disp_cli[disp_ix].display_fifo) != 0) {
                goto out_read;
             }
          }
       } else {
          return NULL;
       }
       if (BKNI_WaitForEvent(ctx->recycle_event, HWC_SURFACE_WAIT_TIMEOUT)) {
          no_surface_count++;
          if (no_surface_count == HWC_SURFACE_WAIT_ATTEMPT) {
             ALOGW("%s: no surface received in %d ms", __FUNCTION__,
                   HWC_SURFACE_WAIT_TIMEOUT*HWC_SURFACE_WAIT_ATTEMPT);
             return NULL;
          }
       }
    }

out_read:
    if (!pthread_mutex_lock(&ctx->disp_cli[disp_ix].fifo_mutex)) {
       pElement = BFIFO_READ(&ctx->disp_cli[disp_ix].display_fifo);
       BFIFO_READ_COMMIT(&ctx->disp_cli[disp_ix].display_fifo, 1);
       pthread_mutex_unlock(&ctx->disp_cli[disp_ix].fifo_mutex);
    }
out:
    return *pElement;
}

static int hwc_ret_tl_fence(struct hwc_context_t *ctx, int index)
{
   struct sync_fence_info_data info;
   int sw_timeline = INVALID_FENCE, fence = INVALID_FENCE;

   if (index != HWC_PRIMARY_IX && index != HWC_VIRTUAL_IX) {
      goto out_err;
   }

   sw_timeline = ctx->ret_tl[index];
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

static int hwc_rel_tl_fence(struct hwc_context_t *ctx, int index, int layer)
{
   struct sync_fence_info_data info;
   int sw_timeline = INVALID_FENCE;
   int cur_ix, fence = INVALID_FENCE;

   if (index != HWC_PRIMARY_IX && index != HWC_VIRTUAL_IX) {
      goto out_err;
   }

   sw_timeline = (index == HWC_PRIMARY_IX) ? ctx->gpx_cli[layer].rel_tl : ctx->vd_cli[layer].rel_tl;
   if (sw_timeline == INVALID_FENCE) {
      goto out_err;
   }

   cur_ix = (index == HWC_PRIMARY_IX) ? ++ctx->gpx_cli[layer].rel_tl_ix : ++ctx->vd_cli[layer].rel_tl_ix;

   snprintf(info.name, sizeof(info.name), "hwc_%d_REL_%llu_%u_%u", index, ctx->stats[index].set_call, layer, cur_ix);
   fence = sw_sync_fence_create(sw_timeline, info.name, cur_ix);
   if (fence < 0) {
      goto out_err;
   }

   if (ctx->dump_fence & HWC_DUMP_FENCE_TICK) {
      if (index == HWC_PRIMARY_IX) {
         ctx->gpx_cli[layer].rel_tl_tick = hwc_tick();
      } else {
         ctx->vd_cli[layer].rel_tl_tick = hwc_tick();
      }
   } else {
      if (index == HWC_PRIMARY_IX) {
         ctx->gpx_cli[layer].rel_tl_tick = 0;
      } else {
         ctx->vd_cli[layer].rel_tl_tick = 0;
      }
   }

   if (ctx->dump_fence & HWC_DUMP_FENCE_SUMMARY) {
      ALOGI("%s: %s:%s -> %d/%d", __FUNCTION__, (index == HWC_PRIMARY_IX) ? "prim" : "virt",
            info.name, sw_timeline, fence);
   }
   return fence;

out_err:
   ALOGW("%s: %s: failed REL fence %llu/%d, timeline %d", __FUNCTION__, (index == HWC_PRIMARY_IX) ? "prim" : "virt",
         ctx->stats[index].set_call, layer, sw_timeline);
   return INVALID_FENCE;
}

static bool hwc_set_signal_oob_il_locked(struct hwc_context_t *ctx, hwc_display_contents_1_t* list)
{
   size_t i;
   private_handle_t *gr_buffer = NULL;
   NEXUS_MemoryBlockHandle block_handle = NULL;
   NEXUS_Error lrc = NEXUS_SUCCESS;
   PSHARED_DATA pSharedData = NULL;
   void *pAddr;
   VIDEO_LAYER_VALIDATION video;
   bool oob_signaled = false;

   for (i = 0; i < list->numHwLayers; i++) {
      memset(&video, 0, sizeof(VIDEO_LAYER_VALIDATION));
      video.layer = &list->hwLayers[i];
      if (is_video_layer(ctx, &video)) {
         if (!video.is_sideband) {
            if (ctx->hwc_binder) {
               gr_buffer = (private_handle_t *)list->hwLayers[i].handle;
               private_handle_t::get_block_handles(gr_buffer, &block_handle, NULL);
               lrc = hwc_mem_lock(ctx, block_handle, &pAddr, true);
               pSharedData = (PSHARED_DATA) pAddr;
               if ((lrc != NEXUS_SUCCESS) || pSharedData == NULL) {
                  continue;
               }
               if (ctx->mm_cli[0].last_ping_frame_id != pSharedData->videoFrame.status.serialNumber) {
                  ctx->mm_cli[0].last_ping_frame_id = pSharedData->videoFrame.status.serialNumber;
                  ctx->hwc_binder->setframe(ctx->mm_cli[0].id, ctx->mm_cli[0].last_ping_frame_id);
                  oob_signaled = true;
               }
               hwc_mem_unlock(ctx, block_handle, true);
               if (ctx->display_dump_layer & HWC_DUMP_LEVEL_COMPOSE) {
                  ALOGI("comp: %llu (%llu,%llu): kicked %zu::frame %llu\n",
                        ctx->stats[HWC_PRIMARY_IX].composed, ctx->stats[HWC_PRIMARY_IX].prepare_call,
                        ctx->stats[HWC_PRIMARY_IX].set_call, i, ctx->mm_cli[0].last_ping_frame_id);
               }
            }
            /* single oob video support. */
            break;
         }
      }
   }

   return oob_signaled;
}

static int hwc_set_primary(struct hwc_context_t *ctx, hwc_display_contents_1_t* list)
{
    size_t hwc_work_item_size = 0;
    struct hwc_work_item *this_frame = NULL;
    size_t i;
    bool oob_signaled = false;

    if (!list) {
       return -EINVAL;
    }

    ctx->stats[HWC_PRIMARY_IX].set_call += 1;

    if (pthread_mutex_lock(&ctx->mutex)) {
        ctx->stats[HWC_PRIMARY_IX].set_skipped += 1;
        goto out_close_fence;
    }

    if (pthread_mutex_lock(&ctx->power_mutex)) {
        ALOGE("%s: Could not acquire power_mutex!!!", __FUNCTION__);
        ctx->stats[HWC_PRIMARY_IX].set_skipped += 1;
        goto out_mutex;
    }

    if ((ctx->power_mode != HWC_POWER_MODE_OFF) && (ctx->power_mode != HWC_POWER_MODE_DOZE_SUSPEND)) {
        pthread_mutex_unlock(&ctx->power_mutex);

        hwc_work_item_size = sizeof(hwc_work_item) + (list->numHwLayers * sizeof(hwc_layer_1_t));
        this_frame = (struct hwc_work_item *) calloc(1, hwc_work_item_size);
        if (this_frame == NULL) {
           goto out_mutex;
        }
        memcpy(&this_frame->content, list,
               sizeof(hwc_display_contents_1_t) + (list->numHwLayers * sizeof(hwc_layer_1_t)));
        for (i = 0; i < list->numHwLayers; i++) {
           hwc_layer_1_t *layer = &list->hwLayers[i];
           memcpy(&this_frame->comp[i], &ctx->gpx_cli[i].composition, sizeof(NEXUS_SurfaceComposition));
           this_frame->skip_set[i] = ctx->gpx_cli[i].skip_set;
           if (layer->compositionType == HWC_SIDEBAND && layer->sidebandStream) {
              this_frame->sb_cli[i] = layer->sidebandStream->data[1];
           }
        }
        this_frame->comp_ix = ctx->stats[HWC_PRIMARY_IX].set_call;
        this_frame->video_layers = ctx->prepare_video;
        this_frame->sideband_layers = ctx->prepare_sideband;
        this_frame->next = NULL;
        if (!pthread_mutex_lock(&ctx->comp_work_list_mutex[HWC_PRIMARY_IX])) {
           if (ctx->composer_work_list[HWC_PRIMARY_IX] == NULL) {
              ctx->composer_work_list[HWC_PRIMARY_IX] = this_frame;
           } else {
              struct hwc_work_item *item, *last;
              last = ctx->composer_work_list[HWC_PRIMARY_IX];
              while (last != NULL) { item = last; last = last->next; }
              item->next = this_frame;
           }
        } else {
           ALOGE("comp: %llu: unable to post (%llu)", ctx->stats[HWC_PRIMARY_IX].set_call, ctx->stats[HWC_PRIMARY_IX].composed);
           ctx->stats[HWC_PRIMARY_IX].composed++;
           free(this_frame);
           goto out_mutex;
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
           pthread_mutex_unlock(&ctx->comp_work_list_mutex[HWC_PRIMARY_IX]);
        } else {
           int installed = 0;
           for (i = 0; i < list->numHwLayers; i++) {
              if (((list->hwLayers[i].compositionType == HWC_OVERLAY) ||
                   (list->hwLayers[i].compositionType == HWC_FRAMEBUFFER_TARGET)) &&
                   ctx->gpx_cli[i].composition.visible) {
                 this_frame->content.hwLayers[i].releaseFenceFd = hwc_rel_tl_fence(ctx, HWC_PRIMARY_IX, i);
                 list->hwLayers[i].releaseFenceFd = this_frame->content.hwLayers[i].releaseFenceFd;
                 installed++;
                 if (ctx->dump_fence & HWC_DUMP_FENCE_PRIM) {
                    ALOGI("fence: %llu/%zu - timeline-release: %d -> fence: %d\n",
                       ctx->stats[HWC_PRIMARY_IX].set_call, i,
                       ctx->gpx_cli[i].rel_tl, list->hwLayers[i].releaseFenceFd);
                 }
              }
           }
           if (ctx->fence_retire) {
              list->retireFenceFd = hwc_ret_tl_fence(ctx, HWC_PRIMARY_IX);
              this_frame->content.retireFenceFd = list->retireFenceFd;
              if (ctx->dump_fence & HWC_DUMP_FENCE_PRIM) {
                 ALOGI("fence: %llu/%zu - retire-fence: %d\n",
                    ctx->stats[HWC_PRIMARY_IX].set_call, i, list->retireFenceFd);
              }
           } else {
              list->retireFenceFd = INVALID_FENCE;
              this_frame->content.retireFenceFd = INVALID_FENCE;
           }
           pthread_mutex_unlock(&ctx->comp_work_list_mutex[HWC_PRIMARY_IX]);

           if (ctx->dump_fence & HWC_DUMP_FENCE_SUMMARY) {
              ALOGI("comp: %llu - installed %d fences (retire: %d) for %zu layers\n",
                    ctx->stats[HWC_PRIMARY_IX].set_call,
                    installed, list->retireFenceFd, list->numHwLayers);
           }
        }

        if (ctx->display_dump_layer & HWC_DUMP_LEVEL_SET) {
           ALOGI("comp: %llu (%llu) - queued\n",
                 ctx->stats[HWC_PRIMARY_IX].set_call, ctx->stats[HWC_PRIMARY_IX].prepare_call);
        }

        if (!ctx->fence_support) {
           pthread_mutex_unlock(&ctx->mutex);
           BKNI_SetEvent(ctx->composer_event[HWC_PRIMARY_IX]);
           BKNI_WaitForEvent(ctx->composer_event_sync[HWC_PRIMARY_IX], BKNI_INFINITE);
           goto out_close_fence;
        } else {
           if (ctx->signal_oob_inline) {
              oob_signaled = hwc_set_signal_oob_il_locked(ctx, list);
           }
           pthread_mutex_unlock(&ctx->mutex);
           BKNI_SetEvent(ctx->composer_event[HWC_PRIMARY_IX]);
           if (ctx->stats[HWC_PRIMARY_IX].set_call >=
                  ctx->stats[HWC_PRIMARY_IX].composed +
                  HWC_SET_COMP_THRESHOLD +
                  oob_signaled ? HWC_SET_COMP_INLINE_DELAY : 0) {
              BKNI_WaitForEvent(ctx->composer_event_forced[HWC_PRIMARY_IX], BKNI_INFINITE);
           }
           goto out;
        }
    } else {
         pthread_mutex_unlock(&ctx->power_mutex);
    }

out_mutex:
    pthread_mutex_unlock(&ctx->mutex);
out_close_fence:
    for (i = 0; i < list->numHwLayers; i++) {
       if (list->hwLayers[i].acquireFenceFd >= 0) {
          close(list->hwLayers[i].acquireFenceFd);
          if (ctx->dump_fence & HWC_DUMP_FENCE_PRIM) {
             ALOGI("fence: %llu/%zu - acquire: %d -> err+close\n",
                   ctx->stats[HWC_PRIMARY_IX].set_call, i,
                   list->hwLayers[i].acquireFenceFd);
          }
       }
    }
out:
    return 0;
}

static int hwc_compose_primary(struct hwc_context_t *ctx, hwc_work_item *item, int *overlay_seen, int *fb_target_seen, bool *oob_video)
{
   int layer_composed = 0;
   NEXUS_Error lrc = NEXUS_SUCCESS, lrcp = NEXUS_SUCCESS;
   NEXUS_SurfaceHandle outputHdl = NULL;
   size_t i;
   NEXUS_Error rc;
   bool is_sideband = false, is_yuv = false, has_video = false, q_ops = false;
   bool skip_comp = false, layer_seeds_output = false, forced_background = false;
   hwc_display_contents_1_t* list = &item->content;
   NEXUS_SurfaceHandle surface[NSC_GPX_CLIENTS_NUMBER];
   int video_seen = 0, chk;
   uint64_t pinged_frame = 0;
   int64_t tick_now = 0;
   VIDEO_LAYER_VALIDATION video;
   OPS_COUNT ops_count;
   bool sideband_alpha_hole = false;
   unsigned int prior_bl = BLENDIND_TYPE_LAST;

   memset(surface, 0, sizeof(surface));
   memset(&ops_count, 0, sizeof(ops_count));

   if (ctx->track_comp_time) {
      item->surf_wait = hwc_tick();
      item->fence_wait = 0;
   }
   if (ctx->disp_cli[HWC_PRIMARY_IX].mode_toggle) {
      hwc_setup_framebuffer_mode(ctx, HWC_PRIMARY_IX,
         (ctx->disp_cli[HWC_PRIMARY_IX].mode == CLIENT_MODE_COMP_BYPASS) ? CLIENT_MODE_NSC_FRAMEBUFFER : CLIENT_MODE_COMP_BYPASS);
      ctx->disp_cli[HWC_PRIMARY_IX].mode_toggle = false;
   }
   outputHdl = hwc_get_disp_surface(ctx, HWC_PRIMARY_IX);
   if (outputHdl == NULL) {
      ALOGE("%s: no display surface available", __FUNCTION__);
      if (list->retireFenceFd != INVALID_FENCE) {
         hwc_ret_tl_inc(ctx, HWC_PRIMARY_IX, 1, true);
      }
      goto out;
   }
   if (ctx->track_comp_time) {
      item->surf_wait = hwc_tick() - item->surf_wait;
   }

   video_seen = item->video_layers + item->sideband_layers;
   for (i = 0; i < list->numHwLayers; i++) {
      if (item->comp[i].visible) {
         if (list->hwLayers[i].compositionType == HWC_OVERLAY) {
            *overlay_seen += 1;
         } else if (list->hwLayers[i].compositionType == HWC_FRAMEBUFFER_TARGET) {
            *fb_target_seen += 1;
         }
      }
   }
   if (video_seen) {
      for (i = 0; i < NSC_MM_CLIENTS_NUMBER; i++) {
         if (ctx->mm_cli[i].last_ping_frame_id == LAST_PING_FRAME_ID_RESET) {
            ctx->alpha_hole_background = false;
            for (i = 0; i < list->numHwLayers; i++) {
               item->skip_set[i] = false;
            }
         }
      }
   }

   for (i = ctx->upside_down?list->numHwLayers-1:0; i < list->numHwLayers; ctx->upside_down?i--:i++) {
      lrc = NEXUS_OS_ERROR;
      memset(&video, 0, sizeof(VIDEO_LAYER_VALIDATION));
      video.scope = HWC_SCOPE_COMP;
      video.layer = &list->hwLayers[i];
      video.sideband_client = item->sb_cli[i];
      chk = hwc_validate_layer_handle(&video);
      if (chk != 0) {
         ALOGW("comp: %llu/%llu - layer: %zu (%d) - invalid for comp (%d)\n",
               ctx->stats[HWC_PRIMARY_IX].set_call, ctx->stats[HWC_PRIMARY_IX].composed,
               i, chk, list->hwLayers[i].acquireFenceFd);
         continue;
      }
      if (i > NSC_GPX_CLIENTS_NUMBER-1) {
         ALOGE("exceedeed max number of accounted layers\n");
         break;
      }
      void *pAddr;
      NEXUS_MemoryBlockHandle block_handle = NULL, phys_block_handle = NULL;
      private_handle_t *gr_buffer = NULL;
      PSHARED_DATA pSharedData = NULL;
      lrc = NEXUS_NOT_INITIALIZED;
      if (list->hwLayers[i].compositionType == HWC_CURSOR_OVERLAY && ctx->ignore_cursor) {
         continue;
      }
      if (list->hwLayers[i].compositionType != HWC_SIDEBAND) {
         gr_buffer = (private_handle_t *)list->hwLayers[i].handle;
         if (gr_buffer == NULL) {
            ALOGV("comp: %llu/%llu - layer: %zu - invalid buffer\n",
                  ctx->stats[HWC_PRIMARY_IX].set_call, ctx->stats[HWC_PRIMARY_IX].composed, i);
            continue;
         }
         private_handle_t::get_block_handles(gr_buffer, &block_handle, NULL);
         lrc = hwc_mem_lock(ctx, block_handle, &pAddr, true);
         pSharedData = (PSHARED_DATA) pAddr;
         if (lrc || pSharedData == NULL) {
            ALOGE("comp: %llu/%llu - layer: %zu - invalid shared data\n",
                  ctx->stats[HWC_PRIMARY_IX].set_call, ctx->stats[HWC_PRIMARY_IX].composed, i);
            continue;
         }
         if (gr_buffer->fmt_set != GR_NONE) {
            phys_block_handle = (NEXUS_MemoryBlockHandle)pSharedData->container.block;
            lrcp = hwc_mem_lock(ctx, phys_block_handle, &pAddr, true);
            if (lrcp || pAddr == NULL) {
               ALOGE("comp: %llu/%llu - layer: %zu - invalid physical data\n",
                     ctx->stats[HWC_PRIMARY_IX].set_call, ctx->stats[HWC_PRIMARY_IX].composed, i);
               continue;
            }
         } else {
            lrcp = NEXUS_NOT_INITIALIZED;
         }
      }
      video.sharedData = pSharedData;
      video.gr_usage = (gr_buffer != NULL) ? gr_buffer->usage : 0;
      has_video = is_video_layer_locked(&video);
      if (has_video && !video.is_yuv) {
         *oob_video = true;
         if (!video.is_sideband) {
            if (ctx->hwc_binder) {
               if (!ctx->signal_oob_inline) {
                  if (!pthread_mutex_lock(&ctx->mutex)) {
                     if (ctx->mm_cli[0].last_ping_frame_id != pSharedData->videoFrame.status.serialNumber) {
                        ctx->mm_cli[0].last_ping_frame_id = pSharedData->videoFrame.status.serialNumber;
                        ctx->hwc_binder->setframe(ctx->mm_cli[0].id, ctx->mm_cli[0].last_ping_frame_id);
                     }
                     pthread_mutex_unlock(&ctx->mutex);
                  }
               }
               pinged_frame = ctx->mm_cli[0].last_ping_frame_id;
            }
            if (*overlay_seen > 2) {
               struct hwc_position frame;
               frame.w = ctx->mm_cli[0].root.composition.position.width;
               frame.h = ctx->mm_cli[0].root.composition.position.height;
               if (frame.w <= ctx->cfg[HWC_PRIMARY_IX].width/4 &&
                   frame.h <= ctx->cfg[HWC_PRIMARY_IX].height/4) {
                  hwc_pip_alpha_hole(ctx, outputHdl, &ops_count);
               }
            }
         } else {
            is_sideband = true;
            if (ctx->hwc_binder) {
               if (!pthread_mutex_lock(&ctx->mutex)) {
                  if (ctx->sb_cli[0].geometry_updated) {
                     struct hwc_position frame, clipped;
                     frame.x = ctx->sb_cli[0].root.composition.position.x;;
                     frame.y = ctx->sb_cli[0].root.composition.position.y;
                     frame.w = ctx->sb_cli[0].root.composition.position.width;
                     frame.h = ctx->sb_cli[0].root.composition.position.height;
                     clipped.x = ctx->sb_cli[0].root.composition.clipRect.x;
                     clipped.y = ctx->sb_cli[0].root.composition.clipRect.y;
                     clipped.w = (ctx->sb_cli[0].root.composition.clipRect.width == 0xFFFF) ? 0 : ctx->sb_cli[0].root.composition.clipRect.width;
                     clipped.h = (ctx->sb_cli[0].root.composition.clipRect.height == 0xFFFF) ? 0 : ctx->sb_cli[0].root.composition.clipRect.height;
                     ctx->hwc_binder->setgeometry(HWC_BINDER_SDB, 0, frame, clipped, SB_CLIENT_ZORDER, 1);
                     ALOGV("comp: sdb-0: {%d,%d,%dx%d} -> {%d,%d,%dx%d}", frame.x, frame.y, frame.w, frame.h, clipped.x, clipped.y, clipped.w, clipped.h);
                     ctx->sb_cli[0].geometry_updated = false;
                     if (ctx->sb_hole_threshold < *overlay_seen) {
                        if (!clipped.x && !clipped.y && !clipped.w && !clipped.h &&
                            !frame.x && !frame.y && frame.w == ctx->cfg[HWC_PRIMARY_IX].width
                            && frame.h == ctx->cfg[HWC_PRIMARY_IX].height) {
                           ALOGV("comp: sdb-0: skip sdb alpha hole.");
                        } else {
                           sideband_alpha_hole = true;
                        }
                     }
                  }
                  pthread_mutex_unlock(&ctx->mutex);
               }
            }
         }
      } else if (item->comp[i].visible) {
         if (video.is_yuv) {
            is_yuv = true;
         }
         if (item->skip_set[i]) {
            if (video_seen && !ctx->alpha_hole_background) {
               item->skip_set[i] = false;
               forced_background = true;
            } else {
               skip_comp = true;
            }
         }
         if (!layer_composed) {
            layer_seeds_output = hwc_layer_seeds_output(item->skip_set[i], &item->comp[i], outputHdl);
            if (layer_seeds_output && video_seen && (*fb_target_seen) && !(*overlay_seen)) {
               layer_seeds_output = false;
            }
            if (layer_seeds_output) {
               if (ctx->smart_background) {
                  if (item->video_layers && !is_yuv && (*overlay_seen > 1)) {
                     ctx->flush_background = true;
                  } else {
                     hwc_set_layer_blending(ctx, i, BLENDIND_TYPE_SRC);
                  }
               }
            }
         } else {
            layer_seeds_output = false;
         }
         if (list->hwLayers[i].acquireFenceFd >= 0) {
            if (!ctx->fence_support) {
               ALOGI("fence: %llu/%zu - fd:%d -> wait+close in sync mode!?! check platform setup.\n",
                     ctx->stats[HWC_PRIMARY_IX].set_call, i,
                     list->hwLayers[i].acquireFenceFd);
            }
            if (ctx->track_comp_time) {
               tick_now = hwc_tick();
            }
            sync_wait(list->hwLayers[i].acquireFenceFd, BKNI_INFINITE);
            if (ctx->track_comp_time) {
               item->fence_wait += (hwc_tick() - tick_now);
            }
            if (ctx->dump_fence & HWC_DUMP_FENCE_PRIM) {
               ALOGI("fence: %llu/%zu - acquire-fence: %d -> wait+close\n",
                     ctx->stats[HWC_PRIMARY_IX].set_call, i,
                     list->hwLayers[i].acquireFenceFd);
            }
            close(list->hwLayers[i].acquireFenceFd);
            list->hwLayers[i].acquireFenceFd = INVALID_FENCE;
         }
         if (ctx->track_comp_time) {
            tick_now = hwc_tick();
         }
         if (hwc_compose_gralloc_buffer(ctx, list, i, pSharedData, gr_buffer, outputHdl,
                                        false, item->skip_set[i], &surface[i], &item->comp[i],
                                        layer_seeds_output, layer_composed, &q_ops, &ops_count,
                                        video_seen, prior_bl)) {
            layer_composed++;
            ctx->gpx_cli[i].last.grhdl = (buffer_handle_t)gr_buffer;
            ctx->gpx_cli[i].last.comp_ix = item->comp_ix;
            prior_bl = ctx->gpx_cli[i].blending_type;
         }
         if (ctx->track_comp_time) {
            item->comp_wait += (hwc_tick() - tick_now);
         }
      }
      if (lrcp == NEXUS_SUCCESS) {
         hwc_mem_unlock(ctx, phys_block_handle, true);
      }
      if (lrc == NEXUS_SUCCESS) {
         hwc_mem_unlock(ctx, block_handle, true);
      }
   }

   if (layer_composed == 0) {
      if (video_seen && !ctx->alpha_hole_background) {
         if (skip_comp) {
            hwc_put_disp_surface(ctx, 0, outputHdl);
            if (list->retireFenceFd != INVALID_FENCE) {
               hwc_ret_tl_inc(ctx, HWC_PRIMARY_IX, 1, false);
            }
            ALOGW("%s: dropping no-background on live-video! (comp:%llu)", __FUNCTION__, ctx->stats[HWC_PRIMARY_IX].composed);
         } else {
            hwc_seed_disp_surface(ctx, outputHdl, &q_ops, &ops_count, HWC_TRANSPARENT);
            rc = hwc_checkpoint(ctx);
            if (rc) {
               ALOGW("%s: checkpoint timeout", __FUNCTION__);
            } else {
               if (ctx->ticker) {
                  hwc_tick_disp_surface(ctx, outputHdl);
               }
               if (sideband_alpha_hole) {
                  hwc_sideband_alpha_hole(ctx, outputHdl, &ops_count);
               }
               if (ctx->blank_video) {
                  hwc_blank_video_surface(ctx, outputHdl);
               }
               rc = NEXUS_SurfaceClient_PushSurface(ctx->disp_cli[HWC_PRIMARY_IX].schdl, outputHdl, NULL, false);
               if (rc) {
                  ALOGW("%s: failed to push surface to client (%d)", __FUNCTION__, rc);
                  if (list->retireFenceFd != INVALID_FENCE) {
                     hwc_ret_tl_inc(ctx, HWC_PRIMARY_IX, 1, true);
                  }
               } else {
                  ctx->alpha_hole_background = true;
                  ctx->on_display.last_push = hwc_tick();
                  ctx->on_display.last_vsync = ctx->on_display.vsync;
               }
            }
         }
      } else {
         hwc_put_disp_surface(ctx, 0, outputHdl);
         if (list->retireFenceFd != INVALID_FENCE) {
            hwc_ret_tl_inc(ctx, HWC_PRIMARY_IX, 1, false);
         }
      }
   } else {
      ctx->alpha_hole_background = forced_background;
      if (q_ops) {
         rc = hwc_checkpoint(ctx);
      } else {
         rc = 0;
      }
      if (rc) {
         ALOGW("%s: checkpoint timeout", __FUNCTION__);
      } else {
         if (ctx->ticker) {
            hwc_tick_disp_surface(ctx, outputHdl);
         }
         if (sideband_alpha_hole) {
            hwc_sideband_alpha_hole(ctx, outputHdl, &ops_count);
         }
         if (ctx->blank_video) {
            hwc_blank_video_surface(ctx, outputHdl);
         }
         rc = NEXUS_SurfaceClient_PushSurface(ctx->disp_cli[HWC_PRIMARY_IX].schdl, outputHdl, NULL, false);
         if (rc) {
            ALOGW("%s: failed to push surface to client (%d)", __FUNCTION__, rc);
            if (list->retireFenceFd != INVALID_FENCE) {
               hwc_ret_tl_inc(ctx, HWC_PRIMARY_IX, 1, true);
            }
         } else {
            ctx->on_display.last_push = hwc_tick();
            ctx->on_display.last_vsync = ctx->on_display.vsync;
         }
      }
   }

   if (ctx->display_dump_layer & HWC_DUMP_LEVEL_FINAL) {
      ALOGI("comp: %llu (%llu,%llu): ls:%s::ov:%d::fb:%d::vs:%d (%" PRIu64 ") - final:%d (%c%c%c) - ops(f:%d:b:%d:v:%d)\n",
            ctx->stats[HWC_PRIMARY_IX].composed, ctx->stats[HWC_PRIMARY_IX].prepare_call,
            ctx->stats[HWC_PRIMARY_IX].set_call, ctx->last_seed == HWC_OPAQUE ? "opq" : "trs",
            *overlay_seen, *fb_target_seen, video_seen,
            pinged_frame, layer_composed, is_yuv?'y':'-', *oob_video?'o':'-', is_sideband?'s':'-',
            ops_count.fill, ops_count.blit, ops_count.yv12);
   }

out:
   for (i = 0; i < list->numHwLayers; i++) {
      if (surface[i]) {
         NEXUS_Surface_Destroy(surface[i]);
      }
      if (list->hwLayers[i].acquireFenceFd != INVALID_FENCE) {
         close(list->hwLayers[i].acquireFenceFd);
         if (ctx->dump_fence & HWC_DUMP_FENCE_PRIM) {
            ALOGI("fence: %llu/%zu - acquire-fence: %d -> comp+err+close\n",
                  ctx->stats[HWC_PRIMARY_IX].set_call, i,
                  list->hwLayers[i].acquireFenceFd);
         }
      }
      if (list->hwLayers[i].releaseFenceFd != INVALID_FENCE) {
         hwc_rel_tl_inc(ctx, HWC_PRIMARY_IX, i);
         list->hwLayers[i].releaseFenceFd = INVALID_FENCE;
         if (ctx->dump_fence & HWC_DUMP_FENCE_PRIM) {
            ALOGI("fence: %llu/%zu - timeline-release: %d -> inc\n",
                  ctx->stats[HWC_PRIMARY_IX].set_call, i, ctx->gpx_cli[i].rel_tl);
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

    if (pthread_mutex_lock(&ctx->mutex)) {
       ctx->stats[HWC_VIRTUAL_IX].set_skipped += 1;
       goto out_close_fence;
    }

    if (ctx->display_gles_always || ctx->display_gles_virtual ||
        (ctx->cfg[HWC_VIRTUAL_IX].width == -1 && ctx->cfg[HWC_VIRTUAL_IX].height == -1)) {
       list->retireFenceFd = dup(list->outbufAcquireFenceFd);
       close(list->outbufAcquireFenceFd);
       list->outbufAcquireFenceFd = INVALID_FENCE;
       goto out_mutex;
    }

    if (list->outbuf == NULL) {
      ALOGE("vcmp: %llu - unexpected null output target", ctx->stats[HWC_VIRTUAL_IX].set_call);
      ctx->stats[HWC_VIRTUAL_IX].set_skipped += 1;
      goto out_mutex;
    }

    if (pthread_mutex_lock(&ctx->power_mutex)) {
       ALOGE("%s: could not acquire power_mutex!!!", __FUNCTION__);
       ctx->stats[HWC_VIRTUAL_IX].set_skipped += 1;
       goto out_mutex;
    }

    if ((ctx->power_mode != HWC_POWER_MODE_OFF) && (ctx->power_mode != HWC_POWER_MODE_DOZE_SUSPEND)) {
       pthread_mutex_unlock(&ctx->power_mutex);

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
       if (!pthread_mutex_lock(&ctx->comp_work_list_mutex[HWC_VIRTUAL_IX])) {
          if (ctx->composer_work_list[HWC_VIRTUAL_IX] == NULL) {
             ctx->composer_work_list[HWC_VIRTUAL_IX] = this_frame;
          } else {
             struct hwc_work_item *item, *last;
             last = ctx->composer_work_list[HWC_VIRTUAL_IX];
             while (last != NULL) { item = last; last = last->next; }
             item->next = this_frame;
          }
       } else {
          ALOGE("vcmp: %llu: unable to post (%llu)", ctx->stats[HWC_VIRTUAL_IX].set_call, ctx->stats[HWC_VIRTUAL_IX].composed);
          ctx->stats[HWC_VIRTUAL_IX].composed++;
          free(this_frame);
          goto out_mutex;
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
          pthread_mutex_unlock(&ctx->comp_work_list_mutex[HWC_VIRTUAL_IX]);
       } else {
          for (i = 0; i < list->numHwLayers; i++) {
             if (ctx->vd_cli[i].composition.visible &&
                 ((list->hwLayers[i].compositionType == HWC_OVERLAY) ||
                  (list->hwLayers[i].compositionType == HWC_FRAMEBUFFER_TARGET))) {
                this_frame->content.hwLayers[i].releaseFenceFd = hwc_rel_tl_fence(ctx, HWC_VIRTUAL_IX, i);
                list->hwLayers[i].releaseFenceFd = this_frame->content.hwLayers[i].releaseFenceFd;
                if (ctx->dump_fence & HWC_DUMP_FENCE_VIRT) {
                   ALOGI("vfence: %llu/%zu - timeline-release: %d -> fence: %d\n",
                      ctx->stats[HWC_VIRTUAL_IX].set_call, i,
                      ctx->vd_cli[i].rel_tl, list->hwLayers[i].releaseFenceFd);
                }
             }
          }

          if (ctx->fence_retire) {
             list->retireFenceFd = hwc_ret_tl_fence(ctx, HWC_VIRTUAL_IX);
             this_frame->content.retireFenceFd = list->retireFenceFd;
             if (ctx->dump_fence & HWC_DUMP_FENCE_VIRT) {
                ALOGI("vfence: %llu/%zu - retire: %d\n",
                   ctx->stats[HWC_VIRTUAL_IX].set_call, i, list->retireFenceFd);
             }
          } else {
             list->retireFenceFd = INVALID_FENCE;
             this_frame->content.retireFenceFd = INVALID_FENCE;
          }
          pthread_mutex_unlock(&ctx->comp_work_list_mutex[HWC_VIRTUAL_IX]);
       }

       if (ctx->display_dump_virt & HWC_DUMP_LEVEL_SET) {
          ALOGI("vcmp: %llu (%llu) - queued\n",
                ctx->stats[HWC_VIRTUAL_IX].set_call, ctx->stats[HWC_VIRTUAL_IX].prepare_call);
       }

       if (!ctx->fence_support) {
          pthread_mutex_unlock(&ctx->mutex);
          BKNI_SetEvent(ctx->composer_event[HWC_VIRTUAL_IX]);
          BKNI_WaitForEvent(ctx->composer_event_sync[HWC_VIRTUAL_IX], BKNI_INFINITE);
          goto out_close_fence;
       } else {
          pthread_mutex_unlock(&ctx->mutex);
          BKNI_SetEvent(ctx->composer_event[HWC_VIRTUAL_IX]);
          goto out;
       }
    } else {
         pthread_mutex_unlock(&ctx->power_mutex);
    }

out_mutex:
    pthread_mutex_unlock(&ctx->mutex);
out_close_fence:
    for (i = 0; i < list->numHwLayers; i++) {
       if (list->hwLayers[i].acquireFenceFd >= 0) {
          close(list->hwLayers[i].acquireFenceFd);
          list->hwLayers[i].acquireFenceFd = INVALID_FENCE;
          if (ctx->dump_fence & HWC_DUMP_FENCE_VIRT) {
             ALOGI("vfence: %llu/%zu - acquire: %d -> err+close\n",
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
   NEXUS_Error lrc, lrcs;
   NEXUS_MemoryBlockHandle block_handle = NULL, out_block_handle = NULL;
   OPS_COUNT ops_count;
   bool q_ops = false;

   memset(surface, 0, sizeof(surface));

   private_handle_t *gr_out_buffer = (private_handle_t *)list->outbuf;
   private_handle_t::get_block_handles(gr_out_buffer, &out_block_handle, NULL);
   lrc = hwc_mem_lock(ctx, out_block_handle, &pAddr, true);
   PSHARED_DATA pOutSharedData = (PSHARED_DATA) pAddr;
   if (pOutSharedData == NULL) {
      ALOGE("vcmp: %llu (%p) - invalid output buffer?", ctx->stats[HWC_VIRTUAL_IX].set_call, out_block_handle);
      ctx->stats[HWC_VIRTUAL_IX].set_skipped += 1;
      goto out;
   }
   outputHdl = hwc_to_nsc_surface(pOutSharedData->container.width,
                                  pOutSharedData->container.height,
                                  pOutSharedData->container.stride,
                                  gralloc_to_nexus_pixel_format(pOutSharedData->container.format),
                                  pOutSharedData->container.block,
                                  0);
   if (outputHdl == NULL) {
      ALOGE("vcmp: %llu (%p) - no display surface available", ctx->stats[HWC_VIRTUAL_IX].set_call, out_block_handle);
      ctx->stats[HWC_VIRTUAL_IX].set_skipped += 1;
      if (!lrc) hwc_mem_unlock(ctx, out_block_handle, true);
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
         private_handle_t::get_block_handles(gr_buffer, &block_handle, NULL);
         lrcs = hwc_mem_lock(ctx, block_handle, &pAddr, true);
         PSHARED_DATA pSharedData = (PSHARED_DATA) pAddr;
         if (list->hwLayers[i].acquireFenceFd >= 0) {
            if (!ctx->fence_support) {
               ALOGI("vfence: %llu/%zu - fd:%d -> wait+close in sync mode!?! check platform setup.\n",
                     ctx->stats[HWC_VIRTUAL_IX].set_call, i,
                     list->hwLayers[i].acquireFenceFd);
            }
            sync_wait(list->hwLayers[i].acquireFenceFd, BKNI_INFINITE);
            close(list->hwLayers[i].acquireFenceFd);
            list->hwLayers[i].acquireFenceFd = INVALID_FENCE;
            if (ctx->dump_fence & HWC_DUMP_FENCE_VIRT) {
               ALOGI("vfence: %llu/%zu - acquire: %d -> wait+close\n",
                     ctx->stats[HWC_VIRTUAL_IX].set_call, i,
                     list->hwLayers[i].acquireFenceFd);
            }
         }
         if (pSharedData == NULL) {
            continue;
         }
         if (list->outbufAcquireFenceFd >= 0) {
            sync_wait(list->outbufAcquireFenceFd, BKNI_INFINITE);
            close(list->outbufAcquireFenceFd);
            list->outbufAcquireFenceFd = INVALID_FENCE;
         }
         if (hwc_compose_gralloc_buffer(ctx, list, i, pSharedData, gr_buffer, outputHdl,
                                        true, false, &surface[i], &item->comp[i],
                                        false, layer_composed, &q_ops, &ops_count,
                                        0, BLENDIND_TYPE_LAST)) {
            layer_composed++;
         }
         if (!lrcs) hwc_mem_unlock(ctx, block_handle, true);
      }
   }

   if (layer_composed) {
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
   if (!lrc) hwc_mem_unlock(ctx, out_block_handle, true);
   if (outputHdl) {
      NEXUS_Surface_Destroy(outputHdl);
   }

   if (ctx->display_dump_virt & HWC_DUMP_LEVEL_COMPOSE) {
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
         if (ctx->dump_fence & HWC_DUMP_FENCE_VIRT) {
            ALOGI("vfence: %llu/%zu - acquire-fence: %d -> comp+err+close\n",
                  ctx->stats[HWC_VIRTUAL_IX].set_call, i,
                  list->hwLayers[i].acquireFenceFd);
         }
      }
      if (list->hwLayers[i].releaseFenceFd != INVALID_FENCE) {
         hwc_rel_tl_inc(ctx, HWC_VIRTUAL_IX, i);
         list->hwLayers[i].releaseFenceFd = INVALID_FENCE;
         if (ctx->dump_fence & HWC_DUMP_FENCE_VIRT) {
            ALOGI("vfence: %llu/%zu - timeline-release: %d -> inc\n",
                  ctx->stats[HWC_VIRTUAL_IX].set_call, i, ctx->vd_cli[i].rel_tl);
         }
      }
   }
   if (list->retireFenceFd != INVALID_FENCE) {
      hwc_ret_tl_inc(ctx, HWC_VIRTUAL_IX, 1, false);
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

    ALOGI("%s: context %p", __FUNCTION__, ctx);

    if (ctx) {
        ctx->vsync_thread_run = 0;
        pthread_join(ctx->vsync_callback_thread, NULL);

        ctx->recycle_thread_run = 0;
        pthread_join(ctx->recycle_callback_thread, NULL);

        if (ctx->pIpcClient != NULL) {
            if (ctx->pNexusClientContext != NULL) {
                ctx->pIpcClient->destroyClientContext(ctx->pNexusClientContext);
            }
            if (ctx->hwc_hp) {
                ctx->pIpcClient->removeHdmiHotplugEventListener(0, ctx->hwc_hp->get());
                delete ctx->hwc_hp;
            }
            if (ctx->hwc_dc) {
                ctx->pIpcClient->removeDisplaySettingsChangedEventListener(0, ctx->hwc_dc->get());
                delete ctx->hwc_dc;
            }
            delete ctx->pIpcClient;
        }

        for (i = 0; i < NSC_GPX_CLIENTS_NUMBER; i++) {
           if (ctx->gpx_cli[i].rel_tl != INVALID_FENCE) {
              close(ctx->gpx_cli[i].rel_tl);
              ctx->gpx_cli[i].rel_tl = INVALID_FENCE;
           }
        }

        for (i = 0; i < HWC_VD_CLIENTS_NUMBER; i++) {
           if (ctx->vd_cli[i].rel_tl != INVALID_FENCE) {
              close(ctx->vd_cli[i].rel_tl);
              ctx->vd_cli[i].rel_tl = INVALID_FENCE;
           }
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
           if (ctx->ret_tl[i] != INVALID_FENCE) {
              close(ctx->ret_tl[i]);
              ctx->ret_tl[i] = INVALID_FENCE;
           }
           for (j = 0; j < HWC_NUM_DISP_BUFFERS; j++) {
              if (ctx->disp_cli[i].display_buffers[j].fifo_fd != INVALID_FENCE) {
                 close(ctx->disp_cli[i].display_buffers[j].fifo_fd);
                 ctx->disp_cli[i].display_buffers[j].fifo_fd = INVALID_FENCE;
              }
              if (ctx->disp_cli[i].display_buffers[j].fifo_surf) {
                 NEXUS_Surface_Destroy(ctx->disp_cli[i].display_buffers[j].fifo_surf);
              }
           }
           pthread_mutex_destroy(&ctx->disp_cli[i].fifo_mutex);
           pthread_mutex_destroy(&ctx->comp_work_list_mutex[i]);
        }
        if (ctx->recycle_event) {
           BKNI_DestroyEvent(ctx->recycle_event);
        }
        if (ctx->recycle_callback) {
           BKNI_DestroyEvent(ctx->recycle_callback);
        }
        if (ctx->vsync_callback) {
           BKNI_DestroyEvent(ctx->vsync_callback);
        }

        NxClient_Free(&ctx->nxAllocResults);

        if (ctx->hwc_binder) {
           delete ctx->hwc_binder;
        }

        if (ctx->hwc_g2d) {
           NEXUS_Graphics2D_Close(ctx->hwc_g2d);
        }
        if (ctx->checkpoint_event) {
           BKNI_DestroyEvent(ctx->checkpoint_event);
        }

        pthread_mutex_destroy(&ctx->vsync_callback_enabled_mutex);
        pthread_mutex_destroy(&ctx->mutex);
        pthread_mutex_destroy(&ctx->dev_mutex);
        pthread_mutex_destroy(&ctx->power_mutex);
        pthread_mutex_destroy(&ctx->g2d_mutex);

        hwc_close_memory_interface(ctx);
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
         case HWC_POWER_MODE_NORMAL:
            break;
         default:
            goto out;
       }

       if (pthread_mutex_lock(&ctx->power_mutex)) {
           ALOGE("%s: Could not acquire power_mutex!!!", __FUNCTION__);
           goto out;
       }
       if (ctx->power_mode != mode) {
          ctx->power_mode = mode;
       }
       pthread_mutex_unlock(&ctx->power_mutex);
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
           if (pthread_mutex_lock(&ctx->dev_mutex)) {
               return -EINVAL;
           }
           *value = (int)VSYNC_REFRESH;
           pthread_mutex_unlock(&ctx->dev_mutex);
       break;

       case HWC_DISPLAY_TYPES_SUPPORTED:
           *value = HWC_DISPLAY_PRIMARY_BIT | HWC_DISPLAY_VIRTUAL_BIT;
       break;
    }

    return 0;
}

static void hwc_device_registerProcs(struct hwc_composer_device_1* dev, hwc_procs_t const* procs)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    ALOGI("hwc_register_procs (%p) (%p, %p, %p)", procs, procs->vsync, procs->invalidate, procs->hotplug);
    ctx->procs = (hwc_procs_t *)procs;
}

static int hwc_device_eventControl(struct hwc_composer_device_1* dev, int disp, int event, int enabled)
{
    int status = -EINVAL;

    (void)dev;
    (void)disp;

    if (event == HWC_EVENT_VSYNC) {
        struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
        if (!pthread_mutex_lock(&ctx->vsync_callback_enabled_mutex)) {
           ctx->vsync_callback_enabled = enabled;
           status = 0;
           pthread_mutex_unlock(&ctx->vsync_callback_enabled_mutex);
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

    if (pthread_mutex_lock(&ctx->dev_mutex)) {
       return -EINVAL;
    }

    if (disp == HWC_DISPLAY_PRIMARY) {
        //TODO: support more configurations?
        *numConfigs = 1;
        configs[0] = DISPLAY_CONFIG_DEFAULT;
    } else {
        ret = -1;
    }

    pthread_mutex_unlock(&ctx->dev_mutex);

    return ret;
}

static int hwc_device_getDisplayAttributes(struct hwc_composer_device_1* dev, int disp,
    uint32_t config, const uint32_t* attributes, int32_t* values)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    int i = 0;

    if (pthread_mutex_lock(&ctx->dev_mutex)) {
       return -EINVAL;
    }

    if (disp == HWC_DISPLAY_PRIMARY) {
        //TODO: add more configurations, hook up with actual edid.
        if (config == DISPLAY_CONFIG_DEFAULT) {
            while (attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE) {
               switch (attributes[i]) {
                   case HWC_DISPLAY_VSYNC_PERIOD:
                      values[i] = (int32_t)VSYNC_REFRESH;
                   break;
                   case HWC_DISPLAY_WIDTH:
                      values[i] = ctx->cfg[HWC_PRIMARY_IX].width;
                   break;
                   case HWC_DISPLAY_HEIGHT:
                      values[i] = ctx->cfg[HWC_PRIMARY_IX].height;
                   break;
                   case HWC_DISPLAY_DPI_X:
                      values[i] = ctx->cfg[HWC_PRIMARY_IX].x_dpi;
                   break;
                   case HWC_DISPLAY_DPI_Y:
                      values[i] = ctx->cfg[HWC_PRIMARY_IX].y_dpi;
                   break;
                   default:
                   break;
               }

               i++;
            }
            pthread_mutex_unlock(&ctx->dev_mutex);
            return 0;
        }
    }

    pthread_mutex_unlock(&ctx->dev_mutex);
    return -EINVAL;
}

static int hwc_device_getActiveConfig(struct hwc_composer_device_1* dev, int disp)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;

    if (pthread_mutex_lock(&ctx->dev_mutex)) {
       return -EINVAL;
    }

    if (disp == HWC_DISPLAY_PRIMARY) {
        // always first one, index 0.
        pthread_mutex_unlock(&ctx->dev_mutex);
        return 0;
    } else {
        pthread_mutex_unlock(&ctx->dev_mutex);
        return -1;
    }
}

static int hwc_device_setActiveConfig(struct hwc_composer_device_1* dev, int disp, int index)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;

    if (pthread_mutex_lock(&ctx->dev_mutex)) {
       return -EINVAL;
    }

    if (disp == HWC_DISPLAY_PRIMARY && index == 0) {
       pthread_mutex_unlock(&ctx->dev_mutex);
       return 0;
    }

    pthread_mutex_unlock(&ctx->dev_mutex);
    return -EINVAL;
}

static int hwc_device_setCursorPositionAsync(struct hwc_composer_device_1 *dev, int disp, int x_pos, int y_pos)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;

    if (pthread_mutex_lock(&ctx->mutex)) {
       return -EINVAL;
    }

    if (disp == HWC_DISPLAY_PRIMARY) {
       if (!ctx->ignore_cursor) {
          NEXUS_SurfaceCursorHandle cursor = NULL;
          if (cursor != NULL) {
             NEXUS_SurfaceCursorSettings config;
             NEXUS_SurfaceCursor_GetSettings(cursor, &config);
             config.composition.position.x = x_pos;
             config.composition.position.y = y_pos;
             NEXUS_SurfaceCursor_SetSettings(cursor, &config);
             // TODO: push right away?  or wait for next hwc_set?
          }
       }
    }

    pthread_mutex_unlock(&ctx->mutex);
    return 0;
}

#define HACK_REFRESH_COUNT 10
static void * hwc_vsync_task(void *argv)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)argv;
    const double nsec_to_msec = 1.0 / 1000000.0;

    unsigned int hack_count = 0;
    unsigned long long hack_reference_composition = 0;
    unsigned long long hack_current_composition = 0;
    unsigned long long hack_triggered_composition = 0;
    bool no_lock = false;

    do
    {
       if (pthread_mutex_lock(&ctx->power_mutex)) {
          ALOGE("%s: Could not acquire power_mutex!!!", __FUNCTION__);
          goto out;
       }
       if ((ctx->power_mode != HWC_POWER_MODE_OFF) && (ctx->power_mode != HWC_POWER_MODE_DOZE_SUSPEND)) {
          pthread_mutex_unlock(&ctx->power_mutex);
          BKNI_WaitForEvent(ctx->vsync_callback, BKNI_INFINITE);

          ctx->on_display.vsync = hwc_tick();

          if (ctx->hack_sync_refresh) {
             if (!pthread_mutex_lock(&ctx->mutex)) {
                if (hack_reference_composition == 0) {
                   hack_reference_composition = ctx->stats[HWC_PRIMARY_IX].composed;
                }
                hack_count++;
                if (hack_count >= HACK_REFRESH_COUNT) {
                   hack_count = 0;
                   hack_current_composition = ctx->stats[HWC_PRIMARY_IX].composed;
                   if (hack_reference_composition &&
                      (hack_reference_composition == hack_current_composition) &&
                      (hack_triggered_composition != hack_current_composition) &&
                      ctx->procs->invalidate != NULL) {
                      no_lock = true;
                      if (ctx->display_dump_layer & HWC_DUMP_LEVEL_HACKS) {
                         ALOGI("*** FORCED REFRESH TICK @ %llu (last:%llu)", hack_reference_composition, hack_triggered_composition);
                      }
                      hack_reference_composition = 0;
                      hack_triggered_composition = hack_current_composition + 1;
                      pthread_mutex_unlock(&ctx->mutex);
                      ctx->procs->invalidate(const_cast<hwc_procs_t *>(ctx->procs));
                   } else if (hack_current_composition > hack_reference_composition) {
                      hack_reference_composition = 0;
                   }
                }
                if (!no_lock) {
                   pthread_mutex_unlock(&ctx->mutex);
                }
                no_lock = false;
             }
         }

         if (ctx->vsync_callback_enabled && ctx->procs->vsync != NULL) {
            ctx->procs->vsync(const_cast<hwc_procs_t *>(ctx->procs), 0, ctx->on_display.vsync);
         }

      } else {
         pthread_mutex_unlock(&ctx->power_mutex);
         BKNI_Sleep(16);
      }

   } while(ctx->vsync_thread_run);

out:
    return NULL;
}

static void hwc_collect_on_screen(struct hwc_context_t *ctx)
{
   const long double nsec_to_msec = 1.0 / 1000000.0;
   long double on_display, vsync_delta;
   int64_t tick_off_screen = hwc_tick();

   if (ctx->on_display.last_push == 0) {
      ALOGV("%s: errand recycle (@ %" PRId64 ")?\n",
            __FUNCTION__, tick_off_screen);
      return;
   }

   on_display = nsec_to_msec * (tick_off_screen - ctx->on_display.last_push);
   vsync_delta = nsec_to_msec * (ctx->on_display.last_push - ctx->on_display.last_vsync);
   ctx->on_display.last_push = 0;

   if ((on_display > 0) && (on_display > ctx->on_display.slowest)) {
      ctx->on_display.slowest = on_display;
   }
   if (ctx->on_display.fastest == 0) {
      ctx->on_display.fastest = on_display;
   }
   if ((on_display > 0) && (on_display < ctx->on_display.fastest)) {
      ctx->on_display.fastest = on_display;
   }
   ctx->on_display.cumul += on_display;
   ctx->on_display.tracked++;

   if (ctx->display_dump_layer & HWC_DUMP_LEVEL_SCREEN_TIME) {
      ALOGI("%s: tick:%.5Lf ms (vd: %.5Lf) (s:%.5Lf::f:%.5Lf::a:%.5Lf)\n",
            __FUNCTION__,
            on_display, vsync_delta,
            ctx->on_display.slowest,
            ctx->on_display.fastest,
            ctx->on_display.cumul/ctx->on_display.tracked);
   }
}

static void * hwc_recycle_task(void *argv)
{
   struct hwc_context_t* ctx = (struct hwc_context_t*)argv;

   do
   {
      BKNI_WaitForEvent(ctx->recycle_callback, BKNI_INFINITE);
      hwc_collect_on_screen(ctx);
      hwc_recycle_now(ctx);
      BKNI_SetEvent(ctx->recycle_event);

   } while(ctx->recycle_thread_run);

out:
    return NULL;
}

static bool hwc_standby_monitor(void *dev)
{
    bool standby = false;
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    int powerMode;

    if (pthread_mutex_lock(&ctx->power_mutex)) {
       ALOGE("%s: Could not acquire power_mutex!!!", __FUNCTION__);
       goto out;
    }
    powerMode = ctx->power_mode;
    pthread_mutex_unlock(&ctx->power_mutex);

    // If Nexus has been told to enter standby and the primary display is turned on,
    // then force the display to power off...
    if (powerMode == HWC_POWER_MODE_NORMAL) {
        hwc_device_setPowerMode(&ctx->device, HWC_DISPLAY_PRIMARY, HWC_POWER_MODE_OFF);
    }
    else {
        standby = (powerMode == HWC_POWER_MODE_OFF);
    }
    ALOGV("%s: standby=%d", __FUNCTION__, standby);

out:
    return standby;
}

static void hwc_collect_composer(struct hwc_context_t *ctx, struct hwc_work_item *this_frame,
   int index, int overlay_seen, int fb_target_seen, int layer_composed, bool oob_video)
{
   const long double nsec_to_msec = 1.0 / 1000000.0;
   long double comp_time;

   if (this_frame->tick_queued) {
      int64_t tick_composed = hwc_tick();

      comp_time = nsec_to_msec * (tick_composed - this_frame->tick_queued);

      if ((ctx->display_dump_layer & HWC_DUMP_LEVEL_LATENCY) ||
          (ctx->track_comp_chatty && ((tick_composed - this_frame->tick_queued) > VSYNC_REFRESH))) {
         ALOGI("%s: %llu: ov:%d, fb:%d, comp:%d, time:%.3Lf msecs (sw:%.5Lf::fw:%.5Lf::cw:%.5Lf) => %s\n",
               (index == HWC_PRIMARY_IX) ? "comp" : "vcmp",
               this_frame->comp_ix, overlay_seen, fb_target_seen, layer_composed,
               comp_time, nsec_to_msec * this_frame->surf_wait,
               nsec_to_msec * this_frame->fence_wait, nsec_to_msec * this_frame->comp_wait,
               ((tick_composed - this_frame->tick_queued) > VSYNC_REFRESH) ? "slow" : "on-time");
      }

      if (layer_composed) {
         ctx->time_track[index].tracked++;
         if ((tick_composed - this_frame->tick_queued) > VSYNC_REFRESH) {
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
      } else {
         if (oob_video) {
            ctx->time_track[index].oob++;
         } else {
            ctx->time_track[index].skipped++;
         }
      }
   }
}

static void * hwc_compose_task_primary(void *argv)
{
   struct hwc_context_t* ctx = (struct hwc_context_t*)argv;
   struct hwc_work_item *this_frame = NULL;
   int layer_composed, overlay_seen, fb_target_seen;
   bool oob_video;

   ALOGI("%s: starting primary composer loop.", __FUNCTION__);

   setpriority(PRIO_PROCESS, 0, PRIORITY_URGENT_DISPLAY + 2*PRIORITY_MORE_FAVORABLE);
   set_sched_policy(0, SP_FOREGROUND);

   do
   {
      if (pthread_mutex_lock(&ctx->power_mutex)) {
         ALOGE("%s: could not acquire power_mutex!!!", __FUNCTION__);
         goto out;
      }
      if ((ctx->power_mode != HWC_POWER_MODE_OFF) && (ctx->power_mode != HWC_POWER_MODE_DOZE_SUSPEND)) {
         pthread_mutex_unlock(&ctx->power_mutex);
         BKNI_WaitForEvent(ctx->composer_event[HWC_PRIMARY_IX], BKNI_INFINITE);
         bool try_compose = true, release_forced = false;

         do {
            this_frame = NULL;
            if (!pthread_mutex_lock(&ctx->comp_work_list_mutex[HWC_PRIMARY_IX])) {
               this_frame = ctx->composer_work_list[HWC_PRIMARY_IX];
               if (this_frame == NULL) {
                  try_compose = false;
               } else {
                  ctx->composer_work_list[HWC_PRIMARY_IX] = this_frame->next;
                  if (ctx->stats[HWC_PRIMARY_IX].set_call >=
                         ctx->stats[HWC_PRIMARY_IX].composed +
                         HWC_SET_COMP_THRESHOLD) {
                     release_forced = true;
                  }
                  ctx->stats[HWC_PRIMARY_IX].composed++;
               }
               pthread_mutex_unlock(&ctx->comp_work_list_mutex[HWC_PRIMARY_IX]);
            } else {
               ALOGE("%s: could not acquire work-list mutex!!!", __FUNCTION__);
               try_compose = false;
            }

            if (this_frame != NULL) {
               overlay_seen = 0; fb_target_seen = 0; oob_video = false;
               layer_composed = hwc_compose_primary(ctx, this_frame, &overlay_seen, &fb_target_seen, &oob_video);
               hwc_collect_composer(ctx, this_frame, HWC_PRIMARY_IX, overlay_seen, fb_target_seen, layer_composed, oob_video);
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
         pthread_mutex_unlock(&ctx->power_mutex);
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
      if (pthread_mutex_lock(&ctx->power_mutex)) {
         ALOGE("%s: could not acquire power_mutex!!!", __FUNCTION__);
         goto out;
      }
      if ((ctx->power_mode != HWC_POWER_MODE_OFF) && (ctx->power_mode != HWC_POWER_MODE_DOZE_SUSPEND)) {
         pthread_mutex_unlock(&ctx->power_mutex);
         BKNI_WaitForEvent(ctx->composer_event[HWC_VIRTUAL_IX], BKNI_INFINITE);
         bool try_compose = true;

         do {
            this_frame = NULL;
            if (!pthread_mutex_lock(&ctx->comp_work_list_mutex[HWC_VIRTUAL_IX])) {
               this_frame = ctx->composer_work_list[HWC_VIRTUAL_IX];
               if (this_frame == NULL) {
                  try_compose = false;
               } else {
                  ctx->composer_work_list[HWC_VIRTUAL_IX] = this_frame->next;
                  ctx->stats[HWC_VIRTUAL_IX].composed++;
               }
               pthread_mutex_unlock(&ctx->comp_work_list_mutex[HWC_VIRTUAL_IX]);
            } else {
               ALOGE("%s: could not acquire work-list mutex!!!", __FUNCTION__);
               try_compose = false;
            }

            if (this_frame != NULL) {
               overlay_seen = 0; fb_target_seen = 0;
               layer_composed = hwc_compose_virtual(ctx, this_frame, &overlay_seen, &fb_target_seen);
               hwc_collect_composer(ctx, this_frame, HWC_VIRTUAL_IX, overlay_seen, fb_target_seen, layer_composed, false);
               free(this_frame);
               this_frame = NULL;
            }
         } while (try_compose);

         if (!ctx->fence_support) {
            BKNI_SetEvent(ctx->composer_event_sync[HWC_VIRTUAL_IX]);
         }

      } else {
         pthread_mutex_unlock(&ctx->power_mutex);
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

   hwc_setup_props_locked(dev);

   dev->display_gles_virtual = property_get_bool(HWC_GLES_VIRTUAL_PROP,      1);
   dev->fence_support        = property_get_bool(HWC_WITH_V3D_FENCE_PROP,    0);
   dev->fence_retire         = property_get_bool(HWC_WITH_DSP_FENCE_PROP,    0);
   dev->track_comp_time      = property_get_bool(HWC_TRACK_COMP_TIME,        1);
   dev->g2d_allow_simult     = property_get_bool(HWC_G2D_SIM_OPS_PROP,       0);
   dev->smart_background     = property_get_bool(HWC_CAPABLE_BACKGROUND,     1);
   dev->toggle_fb_mode       = property_get_bool(HWC_CAPABLE_TOGGLE_MODE,    1);
   dev->ignore_cursor        = property_get_bool(HWC_IGNORE_CURSOR,          HWC_CURSOR_SURFACE_SUPPORTED ? 1 : 0);
   dev->hack_sync_refresh    = property_get_bool(HWC_HACK_SYNC_REFRESH,      0);
   dev->comp_bypass          = property_get_bool(HWC_CAPABLE_COMP_BYPASS,    0);
   dev->signal_oob_inline    = property_get_bool(HWC_CAPABLE_SIGNAL_OOB_IL,  0);
   dev->sb_hole_threshold    = property_get_int32(HWC_CAPABLE_SIDEBAND_HOLE, 1);

   if (property_get_bool(HWC_COMP_VIDEO, 0)) {
      dev->comp_bypass = false;
   }
}

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = -EINVAL;
    struct hwc_context_t *dev = NULL;
    NxClient_AllocSettings nxAllocSettings;
    NEXUS_SurfaceComposition composition;
    int i, j;
    status_t ret;

    if (!strcmp(name, HWC_HARDWARE_COMPOSER)) {
        NEXUS_Error rc = NEXUS_SUCCESS;
        NEXUS_Graphics2DSettings gfxSettings;
        pthread_attr_t attr;
        pthread_mutexattr_t mattr;
        dev = (struct hwc_context_t*)calloc(1, sizeof(*dev));

        if (dev == NULL) {
           ALOGE("FAILED to create hwcomposer!");
           goto out;
        }

        hwc_read_dev_props(dev);
        hwc_assign_memory_interface_name(dev);

        if (BKNI_CreateEvent(&dev->recycle_event)) {
           goto clean_up;
        }
        if (BKNI_CreateEvent(&dev->recycle_callback)) {
           goto clean_up;
        }
        if (BKNI_CreateEvent(&dev->vsync_callback)) {
           goto clean_up;
        }
        pthread_mutexattr_init(&mattr);
        pthread_mutex_init(&dev->vsync_callback_enabled_mutex, &mattr);
        pthread_mutex_init(&dev->mutex, &mattr);
        pthread_mutex_init(&dev->dev_mutex, &mattr);
        pthread_mutex_init(&dev->power_mutex, &mattr);
        pthread_mutex_init(&dev->g2d_mutex, &mattr);
        pthread_mutexattr_destroy(&mattr);

        {
           NEXUS_MemoryStatus status;
           NEXUS_ClientConfiguration clientConfig;
           char buf[128];
           NEXUS_Platform_GetClientConfiguration(&clientConfig);
           NEXUS_Heap_GetStatus(NEXUS_Platform_GetFramebufferHeap(0), &status);
           NEXUS_Heap_ToString(&status, buf, sizeof(buf));
           ALOGI("%s: fb0 heap: %s", __FUNCTION__, buf);
           NEXUS_Heap_GetStatus(clientConfig.heap[NXCLIENT_DYNAMIC_HEAP], &status);
           NEXUS_Heap_ToString(&status, buf, sizeof(buf));
           ALOGI("%s: d-cma heap: %s", __FUNCTION__, buf);
        }

        NxClient_GetDefaultAllocSettings(&nxAllocSettings);
        nxAllocSettings.surfaceClient = DISPLAY_SUPPORTED - 1;
        rc = NxClient_Alloc(&nxAllocSettings, &dev->nxAllocResults);
        if (rc) {
           ALOGE("%s: failed NxClient_Alloc (rc=%d)!", __FUNCTION__, rc);
           goto clean_up;
        }

        for (i = 0; i < DISPLAY_SUPPORTED; i++) {
            if (i == 0) {
               dev->disp_cli[i].sccid = dev->nxAllocResults.surfaceClient[i].id;
            }
            dev->composer_thread_run[i] = 1;
            dev->composer_work_list[i] = NULL;
            if (dev->fence_support) {
               dev->ret_tl[i] = sw_sync_timeline_create();
               if (dev->ret_tl[i] < 0) {
                  dev->ret_tl[i] = INVALID_FENCE;
               }
            }
            rc = BKNI_CreateEvent(&dev->composer_event[i]);
            if (rc) {
               ALOGE("%s: unable to create composer event %d", __FUNCTION__, i);
               goto clean_up;
            }
            rc = BKNI_CreateEvent(&dev->composer_event_sync[i]);
            if (rc) {
               ALOGE("%s: unable to create composer sync event %d", __FUNCTION__, i);
               goto clean_up;
            }
            rc = BKNI_CreateEvent(&dev->composer_event_forced[i]);
            if (rc) {
               ALOGE("%s: unable to create composer forced event %d", __FUNCTION__, i);
               goto clean_up;
            }
        }

        for (i = 0; i < (DISPLAY_SUPPORTED-1); i++) {
            dev->disp_cli[i].schdl = NEXUS_SurfaceClient_Acquire(dev->disp_cli[i].sccid);
            if (NULL == dev->disp_cli[i].schdl) {
               ALOGE("%s: unable to allocate surface client", __FUNCTION__);
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
            composition.colorBlend = nexusColorBlendingEquation[BLENDIND_TYPE_SRC_OVER];
            composition.alphaBlend = nexusAlphaBlendingEquation[BLENDIND_TYPE_SRC_OVER];
            rc = NxClient_SetSurfaceClientComposition(dev->disp_cli[i].sccid, &composition);
            if (rc) {
               ALOGE("%s: unable to set client composition %d", __FUNCTION__, rc);
               goto clean_up;
            }
            pthread_mutexattr_init(&mattr);
            pthread_mutex_init(&dev->disp_cli[i].fifo_mutex, &mattr);
            pthread_mutex_init(&dev->comp_work_list_mutex[i], &mattr);
            pthread_mutexattr_destroy(&mattr);
            rc = hwc_setup_framebuffer_mode(dev, i,
               dev->comp_bypass ? CLIENT_MODE_COMP_BYPASS : CLIENT_MODE_NSC_FRAMEBUFFER);
            if (rc) {
               ALOGE("%s: unable to setup framebuffer mode %d", __FUNCTION__, rc);
               goto clean_up;
            }
            hwc_hotplug_notify(dev);
        }

        for (i = 0; i < NSC_GPX_CLIENTS_NUMBER; i++) {
            dev->gpx_cli[i].ncci.parent = (void *)dev;
            dev->gpx_cli[i].layer_id = i;
            dev->gpx_cli[i].ncci.type = NEXUS_CLIENT_GPX;
            if (dev->fence_support) {
               dev->gpx_cli[i].rel_tl = sw_sync_timeline_create();
               if (dev->gpx_cli[i].rel_tl < 0) {
                  dev->gpx_cli[i].rel_tl = INVALID_FENCE;
               }
            } else {
               dev->gpx_cli[i].rel_tl = INVALID_FENCE;
            }
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
            memset(&dev->vd_cli[i].composition, 0, sizeof(dev->vd_cli[i].composition));
            if (dev->fence_support) {
               dev->vd_cli[i].rel_tl = sw_sync_timeline_create();
               if (dev->vd_cli[i].rel_tl < 0) {
                  dev->vd_cli[i].rel_tl = INVALID_FENCE;
               }
            } else {
               dev->vd_cli[i].rel_tl = INVALID_FENCE;
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
        dev->device.registerProcs                  = hwc_device_registerProcs;
        dev->device.dump                           = hwc_device_dump;
        dev->device.getDisplayConfigs              = hwc_device_getDisplayConfigs;
        dev->device.getDisplayAttributes           = hwc_device_getDisplayAttributes;
        dev->device.getActiveConfig                = hwc_device_getActiveConfig;
        dev->device.setActiveConfig                = hwc_device_setActiveConfig;
        dev->device.setCursorPositionAsync         = hwc_device_setCursorPositionAsync;

        dev->vsync_thread_run = 1;
        dev->recycle_thread_run = 1;
        dev->display_handle = NULL;

        dev->hwc_binder = new HwcBinder_wrap;
        if (dev->hwc_binder == NULL) {
           ALOGE("%s: failed to create hwcbinder, some services will not run!", __FUNCTION__);
        } else {
           dev->hwc_binder->get()->register_notify(&hwc_binder_notify, (void *)dev);
           ALOGI("%s: created hwcbinder (%p)", __FUNCTION__, dev->hwc_binder);
        }

        dev->hwc_hp = new HwcHotPlug_wrap;
        if (dev->hwc_hp == NULL) {
           ALOGE("%s: failed to create hwc-hotplug, some services will not run!", __FUNCTION__);
        } else {
           dev->hwc_hp->get()->register_notify(&hwc_hotplug_notify, (void *)dev);
           ALOGI("%s: created hwc-hotplug (%p)", __FUNCTION__, dev->hwc_hp);
        }

        dev->hwc_dc = new HwcDisplayChanged_wrap;
        if (dev->hwc_dc == NULL) {
           ALOGE("%s: failed to create hwc-display-changed, some services will not run!", __FUNCTION__);
        } else {
           dev->hwc_dc->get()->register_notify(&hwc_display_changed_notify, (void *)dev);
           ALOGI("%s: created hwc-display-changed (%p)", __FUNCTION__, dev->hwc_dc);
        }

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

            ret = dev->pIpcClient->addHdmiHotplugEventListener(0, dev->hwc_hp->get());
            if (ret != NO_ERROR) {
               ALOGE("%s: failed to get register hotplug listener: %d.", __FUNCTION__, ret);
            }
            ret = dev->pIpcClient->addDisplaySettingsChangedEventListener(0, dev->hwc_dc->get());
            if (ret != NO_ERROR) {
               ALOGE("%s: failed to get register display-changed listener: %d.", __FUNCTION__, ret);
            }
        }

        pthread_attr_init(&attr);
        if (pthread_create(&dev->vsync_callback_thread, &attr, hwc_vsync_task, (void *)dev) != 0) {
            ALOGE("%s: unable to create vsync thread", __FUNCTION__);
            goto clean_up;
        }
        pthread_attr_destroy(&attr);
        pthread_setname_np(dev->vsync_callback_thread, "hwc_sync_cb");

        pthread_attr_init(&attr);
        if (pthread_create(&dev->recycle_callback_thread, &attr, hwc_recycle_task, (void *)dev) != 0) {
            ALOGE("%s: unable to create recycle thread", __FUNCTION__);
            goto clean_up;
        }
        pthread_attr_destroy(&attr);
        pthread_setname_np(dev->recycle_callback_thread, "hwc_rec_cb");

        for (i = 0; i < DISPLAY_SUPPORTED; i++) {
            pthread_attr_init(&attr);
            if (pthread_create(&dev->composer_thread[i], &attr, hwc_compose_task_item[i], (void *)dev) != 0) {
                ALOGE("%s: unable to create composer thread %d", __FUNCTION__, i);
                goto clean_up;
            }
            pthread_attr_destroy(&attr);
            pthread_setname_np(dev->composer_thread[i], (i==0)?"hwc_comp_prim":"hwc_comp_virt");
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

static void hwc_vsync_cb(void *context, int param)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)context;
    BSTD_UNUSED(param);

    BKNI_SetEvent(ctx->vsync_callback);
}

static void hwc_recycled_cb(void *context, int param)
{
    struct hwc_context_t *ctx = (struct hwc_context_t *)context;
    BSTD_UNUSED(param);

    BKNI_SetEvent(ctx->recycle_callback);
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

   if (pthread_mutex_lock(&ctx->mutex)) {
      goto out;
   }

   disp_position.x += ctx->overscan_position.x;
   disp_position.y += ctx->overscan_position.y;
   disp_position.width = (int)disp_position.width + (((int)disp_position.width * (int)ctx->overscan_position.w)/ctx->cfg[HWC_PRIMARY_IX].width);
   disp_position.height = (int)disp_position.height + (((int)disp_position.height * (int)ctx->overscan_position.h)/ctx->cfg[HWC_PRIMARY_IX].height);
   clip_position.x += ctx->overscan_position.x;
   clip_position.y += ctx->overscan_position.y;
   clip_position.width = (int)clip_position.width + (((int)clip_position.width * (int)ctx->overscan_position.w)/ctx->cfg[HWC_PRIMARY_IX].width);
   clip_position.height = (int)clip_position.height + (((int)clip_position.height * (int)ctx->overscan_position.h)/ctx->cfg[HWC_PRIMARY_IX].height);

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
    pthread_mutex_unlock(&ctx->mutex);
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

   if (pthread_mutex_lock(&ctx->mutex)) {
       goto out;
   }

   disp_position.x += ctx->overscan_position.x;
   disp_position.y += ctx->overscan_position.y;
   disp_position.width = (int)disp_position.width + (((int)disp_position.width * (int)ctx->overscan_position.w)/ctx->cfg[HWC_PRIMARY_IX].width);
   disp_position.height = (int)disp_position.height + (((int)disp_position.height * (int)ctx->overscan_position.h)/ctx->cfg[HWC_PRIMARY_IX].height);
   clip_position.x += ctx->overscan_position.x;
   clip_position.y += ctx->overscan_position.y;
   clip_position.width = (int)clip_position.width + (((int)clip_position.width * (int)ctx->overscan_position.w)/ctx->cfg[HWC_PRIMARY_IX].width);
   clip_position.height = (int)clip_position.height + (((int)clip_position.height * (int)ctx->overscan_position.h)/ctx->cfg[HWC_PRIMARY_IX].height);

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

   if (layer_updated) {
      ctx->sb_cli[sb_layer_id].geometry_updated = true;
   }

out_unlock:
    pthread_mutex_unlock(&ctx->mutex);
out:
    return;
}

static void hwc_nsc_prepare_layer(
    struct hwc_context_t* ctx, hwc_layer_1_t *layer, int layer_id,
    bool geometry_changed, unsigned int *video_layer_id,
    unsigned int *sideband_layer_id, unsigned int *skip_set, unsigned int *skip_cand)
{
    VIDEO_LAYER_VALIDATION video;

    video.scope = HWC_SCOPE_PREP;
    video.layer = layer;
    if (is_video_layer(ctx, &video)) {
        if (!video.is_sideband) {
            if (video.is_yuv) {
               hwc_prepare_gpx_layer(ctx, layer, layer_id, geometry_changed, false, false, skip_set, skip_cand);
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
        } else if (video.is_sideband) {
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
        hwc_prepare_gpx_layer(ctx, layer, layer_id, geometry_changed, false, false, skip_set, skip_cand);
    }
}

static void hwc_hide_unused_gpx_layer(struct hwc_context_t* ctx, int index)
{
    NEXUS_Error rc;

    if (pthread_mutex_lock(&ctx->mutex)) {
        goto out;
    }

    ctx->gpx_cli[index].last.layerhdl = NULL;
    ctx->gpx_cli[index].skip_set = false;
    ctx->gpx_cli[index].composition.visible = false;

    pthread_mutex_unlock(&ctx->mutex);

out:
    return;
}

static void nx_client_hide_unused_mm_layer(struct hwc_context_t* ctx, int index)
{
    NEXUS_SurfaceComposition composition;
    NEXUS_Error rc;

    if (pthread_mutex_lock(&ctx->mutex)) {
        goto out;
    }

    if (ctx->mm_cli[index].root.composition.visible) {
       ctx->mm_cli[index].root.composition.visible = false;
    }

    pthread_mutex_unlock(&ctx->mutex);
out:
    return;
}

static void nx_client_hide_unused_sb_layer(struct hwc_context_t* ctx, int index)
{
    NEXUS_SurfaceComposition composition;
    NEXUS_Error rc;

    if (pthread_mutex_lock(&ctx->mutex)) {
        goto out;
    }

    if (ctx->sb_cli[index].root.composition.visible) {
       ctx->sb_cli[index].root.composition.visible = false;
    }

    pthread_mutex_unlock(&ctx->mutex);

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
    if (pthread_mutex_lock(&ctx->mutex)) {
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

    pthread_mutex_unlock(&ctx->mutex);

out:
    return;
}

static void hwc_checkpoint_callback(void *context, int param)
{
    struct hwc_context_t *ctx = (struct hwc_context_t *)context;
    (void)param;

    BKNI_SetEvent(ctx->checkpoint_event);
}

static int hwc_checkpoint(struct hwc_context_t *ctx)
{
    int rc;

    if (pthread_mutex_lock(&ctx->g2d_mutex)) {
        ALOGE("%s: failed g2d_mutex!", __FUNCTION__);
        return -1;
    }
    rc = hwc_checkpoint_locked(ctx);
    pthread_mutex_unlock(&ctx->g2d_mutex);
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
