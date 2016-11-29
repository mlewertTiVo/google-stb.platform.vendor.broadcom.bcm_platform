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
#ifndef HWC2_INCLUDED
#define HWC2_INCLUDED

typedef void (* HWC2_HP_NTFY_CB)(void *, bool);
typedef void (* HWC2_DC_NTFY_CB)(void *);
typedef void (* HWC_BINDER_NTFY_CB)(void *, int, struct hwc_notification_info &);

#define HWC2_MAGIC      0xBC353C02

#define HWC2_INVALID    -1
#define HWC2_MAX_REG_CB 3
#define HWC2_MAX_DUMP   4096
#define HWC2_CM_SIZE    16
#define HWC2_NX_DSP_OBJ 4
#define HWC2_CHKPT_TO   5000
#define HWC2_FBS_NUM    3
#define HWC2_FB_WAIT_TO 50
#define HWC2_FB_RETRY   4

#define HWC2_DSP_NAME   32
#define HWC2_DSP_EXT    2001
#define HWC2_DSP_VD     3001

#define HWC2_VD_MAX_NUM 1
#define HWC2_VD_MAX_W   1920
#define HWC2_VD_MAX_H   1080

#define HWC2_MEMIF_DEV  "ro.nexus.ashmem.devname"

/* wrapper around nexus hotplug event listener binder. do
 * not use directly, use the strong pointer wrap instead.
 */
class Hwc2HP : public BnNexusHdmiHotplugEventListener
{
public:

    Hwc2HP() {};
    ~Hwc2HP() {};
    virtual status_t onHdmiHotplugEventReceived(int32_t portId, bool connected);

    void register_notify(HWC2_HP_NTFY_CB callback, void *data) {
       cb = callback;
       cb_data = data;
    }

private:
    HWC2_HP_NTFY_CB cb;
    void * cb_data;
};

/* strong pointer wrap of Hwc2HP for use by hwc2. */
class Hwc2HP_sp
{
private:

   sp<Hwc2HP> ihwc;

public:
   Hwc2HP_sp(void) {
      ihwc = new Hwc2HP;
   };

   ~Hwc2HP_sp(void) {
      ihwc.clear();
   };

   Hwc2HP *get(void) {
      return ihwc.get();
   }
};

/* wrapper around nexus display changed event listener binder. do
 * not use directly, use the strong pointer wrap instead.
 */
class Hwc2DC : public BnNexusDisplaySettingsChangedEventListener
{
public:

    Hwc2DC() {};
    ~Hwc2DC() {};
    virtual status_t onDisplaySettingsChangedEventReceived(int32_t portId);

    void register_notify(HWC2_DC_NTFY_CB callback, void *data) {
       cb = callback;
       cb_data = data;
    }

private:
    HWC2_DC_NTFY_CB cb;
    void * cb_data;
};

/* strong pointer wrap of Hwc2DC for use by hwc2. */
class Hwc2DC_sp
{
private:

   sp<Hwc2DC> ihwc;

public:
   Hwc2DC_sp(void) {
      ihwc = new Hwc2DC;
   };

   ~Hwc2DC_sp(void) {
      ihwc.clear();
   };

   Hwc2DC *get(void) {
      return ihwc.get();
   }
};

/* unit of work for composition. */
struct hwc2_frame_t {
   struct hwc2_frame_t *next;
   unsigned long long  comp_ix;

};

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
};

/* display context target. */
struct hwc2_dsp_ct_t {
   buffer_handle_t tgt;
   int32_t rdf;
   android_dataspace_t dsp;
   size_t dmg_n;
   hwc_rect_t dmg_r;
};

/* layer unit. */
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

/* color matrix unit. */
struct hwc2_cm_t {
   android_color_transform_t type;
   float mtx[HWC2_CM_SIZE];
};

/* virtual display specific information. */
struct hwc2_vd_t {
   uint32_t        w;
   uint32_t        h;
   buffer_handle_t output;
   int32_t         wrFence;
   hwc2_cm_t       cm;
   hwc2_dsp_ct_t   ct;
};

/* framebuffer unit. */
struct hwc2_fb_t {
   int                 fd;
   NEXUS_SurfaceHandle s;
};

enum hwc2_cbs_e {
   cbs_e_none = 0,
   cbs_e_bypass,
   cbs_e_nscfb,
};

/* external (ie hdmi) display specific information. */
struct hwc2_ext_t {
   hwc2_vsync_t  vsync;
   hwc2_cm_t     cm;
   hwc2_dsp_ct_t ct;

   NxClient_AllocResults     nxa;
   NEXUS_SurfaceClientHandle sch;

   enum hwc2_cbs_e                     cb;
   bool                                cbs;
   NEXUS_DisplayHandle                 hdsp;
   BFIFO_HEAD(ExtFB, struct hwc2_fb_t) fb;
   struct hwc2_fb_t                    fbs[HWC2_FBS_NUM];
   pthread_mutex_t                     mtx_fbs;
   bool                                bfb;
};

/* display unit. */
struct hwc2_dsp_t {
   hwc2_display_type_t   type;
   char                  name[HWC2_DSP_NAME];
   bool                  validated;
   hwc2_power_mode_t     pmode;
   union {
      struct hwc2_vd_t   vd;
      struct hwc2_ext_t  ext;
   } u;
   struct hwc2_lyr_t     *lyr;
   struct hwc2_dsp_cfg_t *aCfg;
   struct hwc2_dsp_cfg_t *cfgs;

   BKNI_EventHandle      cmp_evt;
   pthread_t             cmp;
   int                   cmp_active;
   int                   cmp_tl;
   struct hwc2_frame_t   *cmp_wl;
   pthread_mutex_t       mtx_cmp_wl;

   uint64_t              pres;
};

/* hwc binder. do not use directly, use the strong pointer wrap
 * instead.
 */
class HwcBinder : public HwcListener
{
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
class HwcBinder_wrap
{
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

#endif
