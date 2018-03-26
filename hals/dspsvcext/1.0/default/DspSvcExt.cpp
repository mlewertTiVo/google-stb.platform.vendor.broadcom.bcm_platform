#include "DspSvcExt.h"

#include "HwcCommon.h"
#include "Hwc.h"
#include "HwcListener.h"
#include "IHwc.h"
#include "HwcSvc.h"

#include <android-base/logging.h>

namespace bcm {
namespace hardware {
namespace dspsvcext {
namespace V1_0 {
namespace implementation {

typedef void (* HWC_APP_BINDER_NTFY_CB)(int, int, struct hwc_notification_info &);

class HwcAppBinder : public HwcListener
{
public:

    HwcAppBinder() : cb(NULL), cb_data(0) {};
    virtual ~HwcAppBinder() {};

    virtual void notify(int msg, struct hwc_notification_info &ntfy);

    inline void listen() {
        if (get_hwc(false) != NULL)
            get_hwc(false)->registerListener(this, HWC_BINDER_COM);
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
private:
    HWC_APP_BINDER_NTFY_CB cb;
    int cb_data;
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

    HwcAppBinder *get(void) {
        return ihwc.get();
    }
};

void HwcAppBinder::notify(int msg, struct hwc_notification_info &ntfy)
{
    ALOGV( "%s: notify received: msg=%u", __FUNCTION__, msg);

    if (cb)
        cb(cb_data, msg, ntfy);
}

HwcAppBinder_wrap *appHwcBinder = NULL;

Return<DspSvcExtStatus> DspSvcExt::start() {
   if (appHwcBinder != NULL)
      return DspSvcExtStatus::SUCCESS;
   if (appHwcBinder == NULL)
      appHwcBinder = new HwcAppBinder_wrap;
   if (appHwcBinder == NULL) {
      ALOGE("unable to connect to HwcBinder");
      return DspSvcExtStatus::BAD_VALUE;
   }
   ALOGI("connected to HwcBinder");
   return DspSvcExtStatus::SUCCESS;
}

Return<DspSvcExtStatus> DspSvcExt::setOvs(const DspSvcExtOvs& o) {
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

Return<void> DspSvcExt::getOvs(getOvs_cb _hidl_cb) {
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

}  // namespace implementation
}  // namespace V1_0
}  // namespace dspsvcext
}  // namespace hardware
}  // namespace bcm
