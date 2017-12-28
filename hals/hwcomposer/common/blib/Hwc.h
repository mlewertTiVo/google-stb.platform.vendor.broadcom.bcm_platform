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

#ifndef __HWC__H__INCLUDED__
#define __HWC__H__INCLUDED__

#include <utils/Atomic.h>
#include <utils/Errors.h>
#include <utils/threads.h>
#include <binder/MemoryDealer.h>
#include <utils/KeyedVector.h>
#include <utils/SortedVector.h>
#include <utils/Vector.h>

#include "IHwc.h"
#include <binder/Binder.h>
#include "HwcSvc.h"

#define HWC_BINDER_VIDEO_SURFACE_SIZE    (2)
#define HWC_BINDER_SIDEBAND_SURFACE_SIZE (4)

using namespace android;

struct hwc_listener_t
{
   inline hwc_listener_t() : binder(NULL), kind(HWC_BINDER_UNDEF) {}
   inline hwc_listener_t(sp<IBinder> b, int k) : binder(b), kind(k) {}

   sp<IBinder> binder;
   int kind;
};

struct hwc_disp_notifier_t
{
   void *listener;
   int surface;
   struct hwc_position frame;
   struct hwc_position clipped;
   int zorder;
   int visible;
   int disp_w;
   int disp_h;
};

class Hwc : public BnHwc, public IBinder::DeathRecipient
{
public:
    static void instantiate();

    virtual status_t dump(int fd, const Vector<String16>& args);

    virtual void registerListener(const sp<IHwcListener>& listener, int kind);
    virtual void unregisterListener(const sp<IHwcListener>& listener);

    virtual void setVideoSurfaceId(const sp<IHwcListener>& listener, int index, int value, int disp_w, int disp_h);
    virtual void getVideoSurfaceId(const sp<IHwcListener>& listener, int index, int &value);

    virtual void setDisplayFrameId(const sp<IHwcListener>& listener, int handle, int frame);

    virtual void setSidebandSurfaceId(const sp<IHwcListener>& listener, int index, int value, int disp_w, int disp_h);
    virtual void getSidebandSurfaceId(const sp<IHwcListener>& listener, int index, int &value);

    virtual void setGeometry(const sp<IHwcListener>& listener, int type, int index,
                             struct hwc_position &frame, struct hwc_position &clipped,
                             int zorder, int visible);
    virtual void getGeometry(const sp<IHwcListener>& listener, int type, int index,
                             struct hwc_position &frame, struct hwc_position &clipped,
                             int &zorder, int &visible);

    virtual void setOverscanAdjust(const sp<IHwcListener>& listener,
                             struct hwc_position &position);
    virtual void getOverscanAdjust(const sp<IHwcListener>& listener,
                             struct hwc_position &position);

    virtual void evalPlm(const sp<IHwcListener>& listener);

    virtual status_t onTransact(uint32_t code,
                                const Parcel& data,
                                Parcel* reply,
                                uint32_t flags);

private:
    Hwc();
    virtual ~Hwc();

    void binderDied(const wp<IBinder>& who);

    mutable Mutex mLock;

    Vector< hwc_listener_t > mNotificationListeners;
    hwc_disp_notifier_t mVideoSurface[HWC_BINDER_VIDEO_SURFACE_SIZE];
    hwc_disp_notifier_t mSidebandSurface[HWC_BINDER_SIDEBAND_SURFACE_SIZE];
    struct hwc_position overscan_position;
};

#endif // __HWC__H__INCLUDED__
