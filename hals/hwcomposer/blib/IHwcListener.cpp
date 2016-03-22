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

#define LOG_TAG "bcm-hwc-ibl"
#include <utils/Log.h>

#include <stdint.h>
#include <sys/types.h>
#include <binder/Parcel.h>
#include "IHwcListener.h"
#include "HwcCommon.h"

using namespace android;

enum {
    NOTIFY_EVENT = IBinder::FIRST_CALL_TRANSACTION
};

class BpHwcListener : public BpInterface<IHwcListener>
{
public:
    BpHwcListener(const sp<IBinder>& impl)
        : BpInterface<IHwcListener>(impl) {
    }

    void notify(int msg, struct hwc_notification_info &ntfy) {
        Parcel data, reply;
        data.writeInterfaceToken(IHwcListener::getInterfaceDescriptor());
        data.writeInt32(msg);
        data.writeInt32(ntfy.surface_hdl);
        data.writeInt32(ntfy.display_width);
        data.writeInt32(ntfy.display_height);
        data.writeInt32(ntfy.frame_id);
        data.writeInt32(ntfy.frame.x);
        data.writeInt32(ntfy.frame.y);
        data.writeInt32(ntfy.frame.w);
        data.writeInt32(ntfy.frame.h);
        data.writeInt32(ntfy.clipped.x);
        data.writeInt32(ntfy.clipped.y);
        data.writeInt32(ntfy.clipped.w);
        data.writeInt32(ntfy.clipped.h);
        data.writeInt32(ntfy.zorder);
        remote()->transact(NOTIFY_EVENT, data, &reply);
    }
};

IMPLEMENT_META_INTERFACE(HwcListener, "broadcom.hwclistener");

status_t BnHwcListener::onTransact( uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch (code) {
        case NOTIFY_EVENT:
        {
            CHECK_INTERFACE(IHwcListener, data, reply);
            struct hwc_notification_info ntfy;
            int msg = data.readInt32();
            ntfy.surface_hdl = data.readInt32();
            ntfy.display_width = data.readInt32();
            ntfy.display_height = data.readInt32();
            ntfy.frame_id = data.readInt32();
            ntfy.frame.x = data.readInt32();
            ntfy.frame.y = data.readInt32();
            ntfy.frame.w = data.readInt32();
            ntfy.frame.h = data.readInt32();
            ntfy.clipped.x = data.readInt32();
            ntfy.clipped.y = data.readInt32();
            ntfy.clipped.w = data.readInt32();
            ntfy.clipped.h = data.readInt32();
            ntfy.zorder = data.readInt32();
            notify( msg, ntfy );
            return NO_ERROR;
        }
        break;

        default:
           return BBinder::onTransact(code, data, reply, flags);
        break;
    }

    return NO_ERROR;
}
