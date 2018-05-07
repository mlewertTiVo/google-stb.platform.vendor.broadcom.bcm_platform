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
#ifndef HWC2_INCLUDED
#define HWC2_INCLUDED

#define HWC2_LOGRET_ALWAYS  0
#define HWC2_INBOUND_DBG    0
#define HWC2_DUMP_CFG       1
/* log usage: runtime enable via setting the property and causing a
 *            dumpsys SurfaceFlinger to trigger log mask evaluation.
 *            for each wanted category, issue a 'setprop <name> <value>'
 *            where value is 0 to reset and >0 otherwise as listed.
 *
 * log masks: generic to hwc2 (all displays).
 */
#define HWC2_LOG_GLB        "dyn.nx.hwc2.log.mask"
#define LOG_AR_DEBUG        (1<<0)  /* layer add|removal. */
#define LOG_Z_DEBUG         (1<<1)  /* z-based reordering of layer stack. */
#define LOG_SEED_DEBUG      (1<<2)  /* seed background (internal). */
#define LOG_DIM_DEBUG       (1<<3)  /* dim layer from surface-flinger. */
#define LOG_OOB_DEBUG       (1<<4)  /* out-of-bounds video layer. */
#define LOG_GLOB_COMP_DEBUG (1<<5)  /* global composition information. */
#define LOG_OFFLD_DEBUG     (1<<6)  /* offloading to gles information. */
#define LOG_CFGS_DEBUG      (1<<7)  /* display configuration information. */
/*
 * log masks: specific to 'external' display (i.e. main display for stb).
 */
#define HWC2_LOG_EXT        "dyn.nx.hwc2.log.ext.mask"
#define LOG_FENCE_DEBUG     (1<<0)  /* fence add|signal. */
#define LOG_RGBA_DEBUG      (1<<1)  /* rgba layer composition (i.e. device overlay). */
#define LOG_COMP_DEBUG      (1<<2)  /* composition stack for all layers. */
#define LOG_COMP_SUM_DEBUG  (1<<3)  /* summary only of composition stack passed to nsc. */
#define LOG_SDB_DEBUG       (1<<4)  /* sideband layer composition (location). */
#define LOG_PAH_DEBUG       (1<<5)  /* pip-alpha-hole punch-thru. */
#define LOG_ICB_DEBUG       (1<<6)  /* use of intermediate composition buffer for blit ops. */
/*
 * log masks: specific to 'virtual' display (same categories as external unless noted).
 */
#define HWC2_LOG_VD         "dyn.nx.hwc2.log.vd.mask"


typedef void (* HWC_BINDER_NTFY_CB)(void *, int, struct hwc_notification_info &);

#define HWC2_MAGIC      0xBC353C02
#define HWC2_VID_MAGIC  0x00E33200
#define HWC2_SDB_MAGIC  0x51DEBA00
#define HWC2_TLM_MAGIC  0xBAAD1DEA

#define HWC2_INVALID    -1
#define HWC2_MAX_REG_CB 3
#define HWC2_MAX_DUMP   4096
#define HWC2_CM_SIZE    16
#define HWC2_NX_DSP_OBJ 4
#define HWC2_CHKPT_TO   5000
#define HWC2_FBS_NUM    3
#define HWC2_FB_WAIT_TO 50
#define HWC2_FB_RETRY   4
#define HWC2_ASHIFT     24
#define HWC2_RLPF       0xCAFEBAAD
#define HWC2_COMP_RUN   1
#define HWC2_VOMP_RUN   0
#define HWC2_VID_WIN    (3+1)
#define HWC2_MEMC_ROT   0 /* m2mc supports flip, but no 90-rot. */
#define HWC2_OPQ        0xFF000000
#define HWC2_TRS        0x00000000
#define HWC2_MEMIF_DEV  "ro.nx.ashmem.devname"
#define HWC2_PAH        1
#define HWC2_PAH_DIV    2
#define HWC2_SYNC_TO    3500 /* slightly more than android timeout. */

#define HWC2_FB_MAX_W   1920
#define HWC2_FB_MAX_H   1080

/* timeline creation/destruction are expensive operations; we use
 * a pool which recycles yet keeps sufficient depth to allow layers
 * to come and go in sync between device and client.
 */
#define HWC2_MAX_TL     10
#define HWC2_MAX_VTL    10

#define HWC2_DSP_NAME   32
#define HWC2_DSP_EXT    2001
#define HWC2_DSP_VD     3001

#define HWC2_VD_MAX_NUM 0
#define HWC2_VD_MAX_SZ  2048
#define HWC2_VD_GLES    1 /* vd uses gles only for now. */

#define HWC2_EXT_GLES   "ro.nx.hwc2.ext.gles"
#define HWC2_EXT_AFB_W  "ro.nx.hwc2.afb.w"
#define HWC2_EXT_AFB_H  "ro.nx.hwc2.afb.h"
#define HWC2_EXT_NFB_W  "ro.nx.hwc2.nfb.w"
#define HWC2_EXT_NFB_H  "ro.nx.hwc2.nfb.h"
#define HWC2_GFB_MAX_W  "ro.nx.hwc2.gfb.w"
#define HWC2_GFB_MAX_H  "ro.nx.hwc2.gfb.h"

#define HWC2_EOTF_HDR10 1
#define HWC2_EOTF_HLG   2
#define HWC2_EOTF_SDR   3
#define HWC2_EOTF_NS    4

/* dump frames content for each composition, requires:
 *
 *   # mkdir -p /data/nxmedia/hwc2
 *   # chmod 777 /data/nxmedia/hwc2
 *   # setprop dyn.nx.hwc2.dump.data <dump>
 *   # dumpsys SurfaceFlinger
 *
 * <dump> is 'hwc2_record_dump_e'
 *
 * then to control dump on/off (for capturing on a test as
 * example):
 *
 *   # setprop dyn.nx.hwc2.dump.this <what>
 *   # <test>
 *   # setprop dyn.nx.hwc2.dump.this 0
 *   # ls -l /data/nxmedia/hwc2
 *
 * <what> is: 0 (nothing), 1 (header), 2 (content), 3 (filter).
 */
#define HWC2_DUMP_SET   "dyn.nx.hwc2.dump.data"
#define HWC2_DUMP_NOW   "dyn.nx.hwc2.dump.this"
#define HWC2_DUMP_LOC   "/data/nxmedia/hwc2"

#define HWC2_VIDOUT_FMT "dyn.nx.vidout.hwc"

/* callbacks into client registered on launch. */
struct hwc2_reg_cb_t {
   hwc2_callback_descriptor_t desc;
   hwc2_function_pointer_t    func;
   hwc2_callback_data_t       data;
};

/* display configuration unit. */
struct hwc2_dsp_cfg_t {
   struct hwc2_dsp_cfg_t *next;
   uint32_t              w;
   uint32_t              h;
   uint32_t              vsync;
   uint32_t              xdp;
   uint32_t              ydp;

   uint32_t              ew;
   uint32_t              eh;
   bool                  hdr10;
   bool                  hlg;
   bool                  plm;
};

/* layer release timeline unit. */
struct hwc2_lyr_tl_t {
   uint64_t          hdl;
   int               tl;
   uint64_t          ix;

   uint64_t          pt;
   uint64_t          si;

   pthread_mutex_t   mtx_lc;
   bool              que;
   bool              rem;
};

/* display context target. */
struct hwc2_dsp_ct_t {
   buffer_handle_t      tgt;
   int32_t              rdf;
   struct hwc2_lyr_tl_t rtl;
   android_dataspace_t  dsp;
   size_t               dmg_n;
   hwc_rect_t           dmg_r;
};

/* layer unit. */
struct hwc2_lyr_t {
   struct hwc2_lyr_t       *next;
   uint64_t                hdl;

   hwc2_composition_t      cCli; /* offered */
   hwc2_composition_t      cDev; /* wanted  */
   bool                    rp; /* request wanted (not composition type) */

   hwc2_blend_mode_t       bm; /* blend mode */
   buffer_handle_t         bh; /* buffer handle */
   int32_t                 af; /* acquire fence for buffer 'bh' */
   hwc_color_t             sc; /* solid color */
   int32_t                 dsp; /* dataspace */
   hwc_rect_t              fr; /* frame */
   hwc_rect_t              crp; /* source crop (we rounded float to int) */
   float                   al; /* alpha */
   native_handle_t         *sbh; /* sideband layer native handle */
   bool                    sfd; /* surface damage (false means no change) */
   hwc_transform_t         tr; /* transform - unused */
   hwc_rect_t              vis; /* visible region */
   uint32_t                z; /* z-order */
   int32_t                 cx; /* cursor x-position */
   int32_t                 cy; /* cursor y-position */
   int32_t                 rf; /* release fence for this layer current buffer */
   bool                    oob; /* is oob-video */
   uint32_t                lpf; /* last pinged frame (oob-video) */
   bool                    scmp; /* can skip composition */
   NEXUS_MemoryBlockHandle lblk; /* previous block buffer set */
   uint64_t                thdl; /* layer timeline handle. */
};

/* unit of work for composition. */
struct hwc2_frame_t {
   struct hwc2_frame_t *next;

   buffer_handle_t     tgt;  /* target client buffer for frame composition. */
   int32_t             ftgt;  /* target client buffer fence waiter. */

   buffer_handle_t     otgt;  /* target output buffer for frame composition. */
   int32_t             oftgt;  /* target output buffer fence waiter. */

   int32_t             cnt;
   int32_t             scnt;
   int32_t             vcnt;
   struct hwc2_lyr_t   lyr[0];
};

/* color matrix unit. */
struct hwc2_cm_t {
   android_color_transform_t type;
   float                     mtx[HWC2_CM_SIZE];
};

/* virtual display specific information. */
struct hwc2_vd_t {
   buffer_handle_t                     output;
   int32_t                             wrFence;
   hwc2_cm_t                           cm;
   hwc2_dsp_ct_t                       ct;
   struct hwc2_lyr_tl_t                rtl[HWC2_MAX_VTL];
};

/* framebuffer unit. */
struct hwc2_fb_t {
   int                 fd;
   NEXUS_SurfaceHandle s;
};

/* composition mode. */
enum hwc2_cbs_e {
   cbs_e_none = 0,   /* not yet setup (no output) */
   cbs_e_bypass,     /* bypass client - optimal mode. */
   cbs_e_nscfb,      /* compositor framebuffer middle man. */
};

/* video layer (oob|sideband) information. */
struct hwc2_lyr_vid_t {
   hwc_rect_t        fr;
   hwc_rect_t        crp;
};

/* external (ie hdmi) display specific information. */
struct hwc2_ext_t {
   hwc2_vsync_t                        vsync;
   hwc2_cm_t                           cm;
   hwc2_dsp_ct_t                       ct;
   NxClient_AllocResults               nxa;
   NEXUS_SurfaceClientHandle           sch;
   enum hwc2_cbs_e                     cb;
   bool                                cbs;
   NEXUS_DisplayHandle                 hdsp;
   BFIFO_HEAD(ExtFB, struct hwc2_fb_t) fb;
   struct hwc2_fb_t                    fbs_c[HWC2_FBS_NUM];
   struct hwc2_fb_t                    fbs[HWC2_FBS_NUM];
   pthread_mutex_t                     mtx_fbs;
   bool                                bfb;
   struct hwc2_lyr_tl_t                rtl[HWC2_MAX_TL];
   struct hwc2_lyr_vid_t               vid[HWC2_VID_WIN];
   bool                                rhpd;
   uint32_t                            bg;
   bool                                gles;
   struct hwc2_fb_t                    yvi;
   struct hwc2_fb_t                    icb;
};

enum hwc2_record_dump_e {
   hwc2_record_dump_none = 0,
   hwc2_record_dump_inter,
   hwc2_record_dump_final,
   hwc2_record_dump_both,
};

/* display unit. */
struct hwc2_dsp_t {
   hwc2_display_type_t     type;
   char                    name[HWC2_DSP_NAME];
   bool                    validated;
   hwc2_power_mode_t       pmode;
   union {
      struct hwc2_vd_t     vd;
      struct hwc2_ext_t    ext;
   } u;
   struct hwc2_lyr_t       *lyr;
   struct hwc2_dsp_cfg_t   *aCfg;
   struct hwc2_dsp_cfg_t   *cfgs;
   pthread_mutex_t         mtx_cfg;
   bool                    cfg_al;
   bool                    cfg_up;
   struct hwc_position     op;

   BKNI_EventHandle        cmp_evt;
   BKNI_EventHandle        cmp_syn;
   pthread_t               cmp;
   int                     cmp_active;
   int                     cmp_tl;
   struct hwc2_frame_t     *cmp_wl;
   pthread_mutex_t         mtx_cmp_wl;
   pthread_mutex_t         mtx_lyr;

   int32_t                 lm;
   bool                    sfb;
   uint64_t                tlm;
   enum hwc2_record_dump_e dmp;

   uint32_t                gfbw;
   uint32_t                gfbh;

   uint64_t                pres;
   uint64_t                post;
};

/* hwc binder. do not use directly, use the strong pointer wrap
 * instead.
 */
class HwcBinder : public HwcListener {
public:

   HwcBinder() : cb(NULL), cb_data(0) {};
   virtual ~HwcBinder() {};

   virtual void notify(int msg, struct hwc_notification_info &ntfy);

   inline void listen() {
      if (get_hwc(false) != NULL) {
         get_hwc(false)->registerListener(this, HWC_BINDER_HWC);
      }
   };
   inline void hangup() {
      if (get_hwc(false) != NULL) {
         get_hwc(false)->unregisterListener(this);
      }
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

/* strong pointer wrap of HwcBinder for use by hwc2. */
class HwcBinder_wrap {
private:
   sp<HwcBinder> ihwc;
   bool iconnected;

public:
   HwcBinder_wrap(void) {
      ihwc = new HwcBinder;
      iconnected = false;
   };
   virtual ~HwcBinder_wrap(void) {
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

enum hwc2_tweaks_e {
   /* boolean. */
   hwc2_tweak_fb_compressed = 0,
   hwc2_tweak_pip_alpha_hole,
   hwc2_tweak_bypass_disable,
   hwc2_tweak_plm_off,
   hwc2_tweak_dump_enabled,
   hwc2_tweak_scale_gles,
   hwc2_tweak_forced_eotf,
   hwc2_tweak_hdp0,
   /* settings. */
   hwc2_tweak_eotf,
   hwc2_tweak_dump_this,
};

enum hwc2_seeding_e {
   hwc2_seeding_none = 0,
   hwc2_seeding_gfx,
   hwc2_seeding_vid,
};

#endif
