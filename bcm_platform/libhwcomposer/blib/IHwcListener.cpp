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

    void notify(int msg, int param1, int param2) {
        Parcel data, reply;
        data.writeInterfaceToken(IHwcListener::getInterfaceDescriptor());
        data.writeInt32(msg);
        data.writeInt32(param1);
        data.writeInt32(param2);
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
            int msg = data.readInt32();
            int param1 = data.readInt32();
            int param2 = data.readInt32();
            notify( msg, param1, param2 );
            return NO_ERROR;
        }
        break;

        default:
           return BBinder::onTransact(code, data, reply, flags);
        break;
    }

    return NO_ERROR;
}
