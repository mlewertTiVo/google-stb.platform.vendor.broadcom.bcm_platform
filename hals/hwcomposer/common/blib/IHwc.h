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

#ifndef __IHWC__H__INCLUDED__
#define __IHWC__H__INCLUDED__

#include <utils/RefBase.h>
#include <utils/Errors.h>
#include <binder/IInterface.h>
#include <IHwcListener.h>
#include "HwcCommon.h"

using namespace android;

class IHwc : public IInterface
{
public:
    DECLARE_META_INTERFACE(Hwc);

    virtual void registerListener(const sp<IHwcListener>& listener, int kind) = 0;
    virtual void unregisterListener(const sp<IHwcListener>& listener) = 0;

    virtual void setVideoSurfaceId(const sp<IHwcListener>& listener, int index, int value, int display_w, int display_h) = 0;
    virtual void getVideoSurfaceId(const sp<IHwcListener>& listener, int index, int &value) = 0;
    virtual void freeVideoSurfaceId(const sp<IHwcListener>& listener, int index) = 0;

    virtual void setDisplayFrameId(const sp<IHwcListener>& listener, int surface, int frame) = 0;

    virtual void setSidebandSurfaceId(const sp<IHwcListener>& listener, int index, int value, int display_w, int display_h) = 0;
    virtual void getSidebandSurfaceId(const sp<IHwcListener>& listener, int index, int &value) = 0;
    virtual void freeSidebandSurfaceId(const sp<IHwcListener>& listener, int index) = 0;

    virtual void setGeometry(const sp<IHwcListener>& listener, int type, int index,
                             struct hwc_position &frame, struct hwc_position &clipped,
                             int zorder, int visible) = 0;
    virtual void getGeometry(const sp<IHwcListener>& listener, int type, int index,
                             struct hwc_position &frame, struct hwc_position &clipped,
                             int &zorder, int &visible) = 0;

    virtual void setOverscanAdjust(const sp<IHwcListener>& listener,
                             struct hwc_position &position) = 0;
    virtual void getOverscanAdjust(const sp<IHwcListener>& listener,
                             struct hwc_position &position) = 0;
};

class BnHwc : public BnInterface<IHwc>
{
public:
    virtual status_t onTransact( uint32_t code,
                                 const Parcel& data,
                                 Parcel* reply,
                                 uint32_t flags = 0);
};

#endif  // __IHWC__H__INCLUDED__
