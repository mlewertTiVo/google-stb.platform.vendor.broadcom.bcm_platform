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

#define LOG_TAG "bcm-hwc-ib"

#include <binder/Parcel.h>
#include "IHwc.h"
#include "IHwcListener.h"

using namespace android;

enum {
    NO_OP = IBinder::FIRST_CALL_TRANSACTION,
    REGISTER_LISTENER,
    UNREGISTER_LISTENER,
    SET_VIDEO_SURF_CLIENT_ID,
    GET_VIDEO_SURF_CLIENT_ID,
    SET_DISPLAY_FRAME_ID,
};

class BpHwc : public BpInterface<IHwc>
{
public:
    BpHwc(const sp<IBinder>& impl)
        : BpInterface<IHwc>(impl) {
    }

    void registerListener(const sp<IHwcListener>& listener, int kind) {
        Parcel data, reply;
        data.writeInterfaceToken(IHwc::getInterfaceDescriptor());
        data.writeStrongBinder(listener->asBinder());
        data.writeInt32(kind);
        remote()->transact(REGISTER_LISTENER, data, &reply);
    }

    void unregisterListener(const sp<IHwcListener>& listener) {
        Parcel data, reply;
        data.writeInterfaceToken(IHwc::getInterfaceDescriptor());
        data.writeStrongBinder(listener->asBinder());
        remote()->transact(UNREGISTER_LISTENER, data, &reply);
    }

    void setVideoSurfaceId(const sp<IHwcListener>& listener, int index, int value) {
        Parcel data, reply;
        data.writeInterfaceToken(IHwc::getInterfaceDescriptor());
        data.writeStrongBinder(listener->asBinder());
        data.writeInt32(index);
        data.writeInt32(value);
        remote()->transact(SET_VIDEO_SURF_CLIENT_ID, data, &reply);
    }

    void getVideoSurfaceId(const sp<IHwcListener>& listener, int index, int &value) {
        Parcel data, reply;
        data.writeInterfaceToken(IHwc::getInterfaceDescriptor());
        data.writeStrongBinder(listener->asBinder());
        data.writeInt32(index);
        remote()->transact(GET_VIDEO_SURF_CLIENT_ID, data, &reply);
        value = reply.readInt32();
    }

    void setDisplayFrameId(const sp<IHwcListener>& listener, int surface, int frame) {
        Parcel data, reply;
        data.writeInterfaceToken(IHwc::getInterfaceDescriptor());
        data.writeStrongBinder(listener->asBinder());
        data.writeInt32(surface);
        data.writeInt32(frame);
        remote()->transact(SET_DISPLAY_FRAME_ID, data, &reply);
    }
};

IMPLEMENT_META_INTERFACE(Hwc, "broadcom.hwc");

status_t BnHwc::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
   switch (code) {
      case REGISTER_LISTENER:
      {
         CHECK_INTERFACE(IHwc, data, reply);
         sp<IHwcListener> listener = interface_cast<IHwcListener>(data.readStrongBinder());
         int kind = data.readInt32();
         registerListener(listener, kind);
         return NO_ERROR;
      }
      break;

      case UNREGISTER_LISTENER:
      {
         CHECK_INTERFACE(IHwc, data, reply);
         sp<IHwcListener> listener = interface_cast<IHwcListener>(data.readStrongBinder());
         unregisterListener(listener);
         return NO_ERROR;
      }
      break;

      case SET_VIDEO_SURF_CLIENT_ID:
      {
         CHECK_INTERFACE(IHwc, data, reply);
         sp<IHwcListener> listener = interface_cast<IHwcListener>(data.readStrongBinder());
         int index = data.readInt32();
         int value = data.readInt32();
         setVideoSurfaceId(listener, index, value);
         return NO_ERROR;
      }
      break;

      case GET_VIDEO_SURF_CLIENT_ID:
      {
         CHECK_INTERFACE(IHwc, data, reply);
         int value = -1;
         sp<IHwcListener> listener = interface_cast<IHwcListener>(data.readStrongBinder());
         int index = data.readInt32();
         getVideoSurfaceId(listener, index, value);
         reply->writeInt32(value);
         return NO_ERROR;
      }
      break;

      case SET_DISPLAY_FRAME_ID:
      {
         CHECK_INTERFACE(IHwc, data, reply);
         sp<IHwcListener> listener = interface_cast<IHwcListener>(data.readStrongBinder());
         int surface = data.readInt32();
         int frame = data.readInt32();
         setDisplayFrameId(listener, surface, frame);
         return NO_ERROR;
      }
      break;

      default:
         return BBinder::onTransact(code, data, reply, flags);
      break;
   }

   return NO_ERROR;
}
