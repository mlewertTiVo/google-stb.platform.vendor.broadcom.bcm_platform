#include "DspSvcExt.h"

#include "HwcCommon.h"
#include "Hwc.h"
#include "HwcListener.h"
#include "IHwc.h"
#include "HwcSvc.h"

using ::hwc_notification_info;
using ::hwc_position;

#include <inttypes.h>
#include <android-base/logging.h>

namespace bcm {
namespace hardware {
namespace dspsvcext {
namespace V1_0 {
namespace implementation {

typedef void (* HWC_APP_BINDER_NTFY_CB)(void *, int, struct hwc_notification_info &);

class HwcAppBinder : public HwcListener
{
public:

    HwcAppBinder() : cb(NULL), cb_data(0) {};
    virtual ~HwcAppBinder() {};

    virtual void notify(int msg, struct hwc_notification_info &ntfy);

    inline void listen() {
        if (get_hwc(false) != NULL)
            get_hwc(false)->registerListener(this, HWC_BINDER_COM|HWC_BINDER_SDB);
        else
            ALOGE("%s: failed to associate %p with HwcAppBinder service.", __FUNCTION__, this);
    };

    inline void hangup() {
        if (get_hwc(false) != NULL)
            get_hwc(false)->unregisterListener(this);
        else
            ALOGE("%s: failed to dissociate %p from HwcAppBinder service.", __FUNCTION__, this);
    };

    inline void getoverscan(struct hwc_position &position) {
        if (get_hwc(false) != NULL) {
            get_hwc(false)->getOverscanAdjust(this, position);
        }
    };

    inline void setoverscan(struct hwc_position &position) {
        if (get_hwc(false) != NULL) {
            get_hwc(false)->setOverscanAdjust(this, position);
        }
    };

    inline void getsideband(int ix, int &val) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->getSidebandSurfaceId(this, ix, val);
       }
    };

    inline void freesideband(int ix) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->freeSidebandSurfaceId(this, ix);
       }
    };

    void notifier(HWC_APP_BINDER_NTFY_CB callback, void *data) {
       cb = callback;
       cb_data = data;
    };

private:
    HWC_APP_BINDER_NTFY_CB cb;
    void *cb_data;
};

class HwcAppBinder_wrap
{
private:

    sp<HwcAppBinder> ihwc;

public:
    HwcAppBinder_wrap(void) {
        ALOGV("%s: allocated %p", __FUNCTION__, this);
        ihwc = new HwcAppBinder;
        ihwc.get()->listen();
    };

    virtual ~HwcAppBinder_wrap(void) {
        ALOGV("%s: cleared %p", __FUNCTION__, this);
        ihwc.get()->hangup();
        ihwc.clear();
    };

    void getoverscan(struct hwc_position &position) {
        ihwc.get()->getoverscan(position);
    }

    void setoverscan(struct hwc_position &position) {
        ihwc.get()->setoverscan(position);
    }

    void getsideband(int ix, int &val) {
        ihwc.get()->getsideband(ix, val);
    }

    void freesideband(int ix) {
        ihwc.get()->freesideband(ix);
    }

    HwcAppBinder *get(void) {
        return ihwc.get();
    }
};

void HwcAppBinder::notify(int msg, struct hwc_notification_info &ntfy)
{
    ALOGI("%s: notify received: msg=%u", __FUNCTION__, msg);

    if (cb)
        cb(cb_data, msg, ntfy);
}

HwcAppBinder_wrap *appHwcBinder = NULL;

static void DspEvtSvcNtfy(void *cb_data,
   int msg, struct hwc_notification_info &ntfy) {

   DspSvcExt *obs = (DspSvcExt *)cb_data;
   if (obs == NULL) return;

   switch (msg) {
   case HWC_BINDER_NTFY_SIDEBAND_SURFACE_GEOMETRY_UPDATE: {
      Vector<struct SdbGeomCb>::const_iterator v;
      Mutex::Autolock _l(obs->l); // scoped for vector.
      for (v = obs->s.begin(); v != obs->s.end(); ++v) {
         if ((*v).cb != NULL) {
            if ((*v).v == ntfy.surface_hdl) {
               ALOGI("[sbd]:client: %" PRIu32 ":NTFY_SIDEBAND_SURFACE_GEOMETRY_UPDATE(%d,%d,%dx%d)",
                     (*v).i, ntfy.frame.x, ntfy.frame.y, ntfy.frame.w, ntfy.frame.h);
               struct DspSvcExtGeom fromHidl;
               fromHidl.geom_x = ntfy.frame.x;
               fromHidl.geom_y = ntfy.frame.y;
               fromHidl.geom_w = ntfy.frame.w;
               fromHidl.geom_h = ntfy.frame.h;
               (*v).cb->onGeom((*v).i, fromHidl);
            }
         }
      }
   }
   break;
   default:
   break;
   }
}

Return<DspSvcExtStatus> DspSvcExt::start() {
   if (appHwcBinder != NULL)
      return DspSvcExtStatus::SUCCESS;
   if (appHwcBinder == NULL)
      appHwcBinder = new HwcAppBinder_wrap;
   if (appHwcBinder == NULL) {
      ALOGE("unable to connect to HwcBinder");
      return DspSvcExtStatus::BAD_VALUE;
   }
   appHwcBinder->get()->notifier(&DspEvtSvcNtfy, this);
   ALOGI("connected to HwcBinder");
   return DspSvcExtStatus::SUCCESS;
}

Return<DspSvcExtStatus> DspSvcExt::setOvs(
   const DspSvcExtOvs& o) {

   struct DspSvcExtOvs fromHidl;
   struct hwc_position toHwc;

   if (appHwcBinder == NULL) {
      return DspSvcExtStatus::BAD_VALUE;
   } else {
      memcpy(&fromHidl, &o, sizeof(struct DspSvcExtOvs));
      toHwc.x = fromHidl.ovs_x;
      toHwc.y = fromHidl.ovs_y;
      toHwc.w = fromHidl.ovs_w;
      toHwc.h = fromHidl.ovs_h;
      appHwcBinder->setoverscan(toHwc);
      return DspSvcExtStatus::SUCCESS;
   }
}

Return<void> DspSvcExt::getOvs(
   getOvs_cb _hidl_cb) {

   struct DspSvcExtOvs toHidl;
   struct hwc_position fromHwc;

   if (appHwcBinder == NULL) {
      memset(&toHidl, 0, sizeof(struct DspSvcExtOvs));
      _hidl_cb(toHidl);
      return Void();
   }

   memset(&fromHwc, 0, sizeof(struct hwc_position));
   appHwcBinder->getoverscan(fromHwc);
   toHidl.ovs_x = fromHwc.x;
   toHidl.ovs_y = fromHwc.y;
   toHidl.ovs_w = fromHwc.w;
   toHidl.ovs_h = fromHwc.h;
   _hidl_cb(toHidl);
   return Void();
}

Return<int32_t> DspSvcExt::regSdbCb(
   int32_t i,
   const ::android::sp<IDspSvcExtSdbGeomCb>& cb) {

   Mutex::Autolock _l(l);
   Vector<struct SdbGeomCb>::iterator v;

   for (v = s.begin(); v != s.end(); ++v) {
      if ((*v).i == i) {
         if (cb == NULL) {
            appHwcBinder->freesideband(i);
            s.erase(v);
            ALOGI("[sdb]:client: %" PRIu32 ": unregistered", i);
            return i;
         } else {
            ALOGW("[sdb]:client: %" PRIu32 ": already registered", i);
            return -1;
         }
      }
   }

   if (cb == NULL) {
      ALOGW("[sdb]:client: %" PRIu32 ": invalid registration", i);
      return -1;
   } else {
      int32_t v = -1;
      appHwcBinder->getsideband(i, v);
      if (v != -1) {
         DspSvcExtGeom g = {0,0,0,0};
         struct SdbGeomCb n = {i, v, cb, g};
         s.push_back(n);
         ALOGI("[sdb]:client: %" PRIu32 ": registered", i);
      }
      return v;
   }
}

Return<DspSvcExtStatus> DspSvcExt::setSdbGeom(
   int32_t i,
   const DspSvcExtGeom& g) {

   Vector<struct SdbGeomCb>::iterator v;
   Mutex::Autolock _l(l);
   for (v = s.begin(); v != s.end(); ++v) {
      if (((*v).i == i) && ((*v).cb != NULL)) {
         // filter out non-change in sideband layer position.
         if (memcmp(&g, &((*v).g), sizeof(DspSvcExtGeom))) {
            struct SdbGeomCb n = {(*v).i, (*v).v, (*v).cb, g};
            ALOGI("[sbd]:client: %" PRIu32 ":setSdbGeom(%d,%d,%dx%d)",
              (*v).i, g.geom_x, g.geom_y, g.geom_w, g.geom_h);
            (*v).cb->onGeom((*v).i, g);
            s.erase(v);
            s.push_back(n);
            return DspSvcExtStatus::SUCCESS;
         } else {
            return DspSvcExtStatus::ALREADY_REGISTERED;
         }
      }
   }
   return DspSvcExtStatus::BAD_VALUE;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace dspsvcext
}  // namespace hardware
}  // namespace bcm
