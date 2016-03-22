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

#ifndef __IHWCLISTENER__H__INCLUDED__
#define __IHWCLISTENER__H__INCLUDED__

#include <utils/RefBase.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include "HwcCommon.h"

using namespace android;

class IHwcListener: public IInterface
{
public:
    DECLARE_META_INTERFACE(HwcListener);

    virtual void notify(int msg, struct hwc_notification_info &ntfy) = 0;
};

class BnHwcListener: public BnInterface<IHwcListener>
{
public:
    virtual status_t onTransact( uint32_t code,
                                 const Parcel& data,
                                 Parcel* reply,
                                 uint32_t flags = 0);
};

#endif // __IHWCLISTENER__H__INCLUDED__

