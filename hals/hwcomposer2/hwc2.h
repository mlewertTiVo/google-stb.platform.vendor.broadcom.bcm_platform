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

#define HWC2_MAGIC      0xBC353C02

#define HWC2_INVALID    -1
#define HWC2_MAX_REG_CB 3
#define HWC2_MAX_DUMP   4096
#define HWC2_CM_SIZE    16
#define HWC2_NX_DSP_OBJ 4
#define HWC2_CHKPT_TO   5000

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
   unsigned long long comp_ix;

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
   uint32_t w;
   uint32_t h;

   buffer_handle_t output;
   int32_t wrFence;
   hwc2_cm_t cm;
   hwc2_dsp_ct_t ct;
};

/* external (ie hdmi) display specific information. */
struct hwc2_ext_t {
   uint32_t w;
   uint32_t h;
   int      xdp;
   int      ydp;

   hwc2_vsync_t vsync;
   hwc2_cm_t cm;
   hwc2_dsp_ct_t ct;

   NxClient_AllocResults nx_alloc;
   bool cb; /* composition bypass state. */
   bool cbs; /* composition bypass change pending. */
   NEXUS_DisplayHandle hdsp;
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

#endif

