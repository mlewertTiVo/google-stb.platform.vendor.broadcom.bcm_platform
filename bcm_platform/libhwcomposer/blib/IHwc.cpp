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
    SET_SIDEBAND_SURF_CLIENT_ID,
    GET_SIDEBAND_SURF_CLIENT_ID,
    SET_VIDEO_GEOMETRY_CLIENT_ID,
    GET_VIDEO_GEOMETRY_CLIENT_ID,
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

    void setVideoSurfaceId(const sp<IHwcListener>& listener, int index, int value, int disp_w, int disp_h) {
        Parcel data, reply;
        data.writeInterfaceToken(IHwc::getInterfaceDescriptor());
        data.writeStrongBinder(listener->asBinder());
        data.writeInt32(index);
        data.writeInt32(value);
        data.writeInt32(disp_w);
        data.writeInt32(disp_h);
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

    void setSidebandSurfaceId(const sp<IHwcListener>& listener, int index, int value, int disp_w, int disp_h) {
        Parcel data, reply;
        data.writeInterfaceToken(IHwc::getInterfaceDescriptor());
        data.writeStrongBinder(listener->asBinder());
        data.writeInt32(index);
        data.writeInt32(value);
        data.writeInt32(disp_w);
        data.writeInt32(disp_h);
        remote()->transact(SET_SIDEBAND_SURF_CLIENT_ID, data, &reply);
    }

    void getSidebandSurfaceId(const sp<IHwcListener>& listener, int index, int &value) {
        Parcel data, reply;
        data.writeInterfaceToken(IHwc::getInterfaceDescriptor());
        data.writeStrongBinder(listener->asBinder());
        data.writeInt32(index);
        remote()->transact(GET_SIDEBAND_SURF_CLIENT_ID, data, &reply);
        value = reply.readInt32();
    }

    void setGeometry(const sp<IHwcListener>& listener, int type, int index,
                     struct hwc_position &frame, struct hwc_position &clipped,
                     int zorder, int visible) {
        Parcel data, reply;
        data.writeInterfaceToken(IHwc::getInterfaceDescriptor());
        data.writeStrongBinder(listener->asBinder());
        data.writeInt32(type);
        data.writeInt32(index);
        data.writeInt32(frame.x);
        data.writeInt32(frame.y);
        data.writeInt32(frame.h);
        data.writeInt32(frame.w);
        data.writeInt32(clipped.x);
        data.writeInt32(clipped.y);
        data.writeInt32(clipped.h);
        data.writeInt32(clipped.w);
        data.writeInt32(zorder);
        data.writeInt32(visible);
        remote()->transact(SET_VIDEO_GEOMETRY_CLIENT_ID, data, &reply);
    }

    void getGeometry(const sp<IHwcListener>& listener, int type, int index,
                     struct hwc_position &frame, struct hwc_position &clipped,
                     int &zorder, int &visible) {
        Parcel data, reply;
        data.writeInterfaceToken(IHwc::getInterfaceDescriptor());
        data.writeStrongBinder(listener->asBinder());
        data.writeInt32(type);
        data.writeInt32(index);
        remote()->transact(GET_VIDEO_GEOMETRY_CLIENT_ID, data, &reply);
        frame.x = reply.readInt32();
        frame.y = reply.readInt32();
        frame.h = reply.readInt32();
        frame.w = reply.readInt32();
        clipped.x = reply.readInt32();
        clipped.y = reply.readInt32();
        clipped.h = reply.readInt32();
        clipped.w = reply.readInt32();
        zorder = reply.readInt32();
        visible = reply.readInt32();
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
         int index  = data.readInt32();
         int value  = data.readInt32();
         int disp_w = data.readInt32();
         int disp_h = data.readInt32();
         setVideoSurfaceId(listener, index, value, disp_w, disp_h);
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

      case SET_SIDEBAND_SURF_CLIENT_ID:
      {
         CHECK_INTERFACE(IHwc, data, reply);
         sp<IHwcListener> listener = interface_cast<IHwcListener>(data.readStrongBinder());
         int index  = data.readInt32();
         int value  = data.readInt32();
         int disp_w = data.readInt32();
         int disp_h = data.readInt32();
         setSidebandSurfaceId(listener, index, value, disp_w, disp_h);
         return NO_ERROR;
      }
      break;

      case GET_SIDEBAND_SURF_CLIENT_ID:
      {
         CHECK_INTERFACE(IHwc, data, reply);
         int value = -1;
         sp<IHwcListener> listener = interface_cast<IHwcListener>(data.readStrongBinder());
         int index = data.readInt32();
         getSidebandSurfaceId(listener, index, value);
         reply->writeInt32(value);
         return NO_ERROR;
      }
      break;

      case SET_VIDEO_GEOMETRY_CLIENT_ID:
      {
         CHECK_INTERFACE(IHwc, data, reply);
         sp<IHwcListener> listener = interface_cast<IHwcListener>(data.readStrongBinder());
         int type = data.readInt32();
         int index = data.readInt32();
         struct hwc_position frame, clipped;
         int zorder, visible;
         frame.x = data.readInt32();
         frame.y = data.readInt32();
         frame.h = data.readInt32();
         frame.w = data.readInt32();
         clipped.x = data.readInt32();
         clipped.y = data.readInt32();
         clipped.h = data.readInt32();
         clipped.w = data.readInt32();
         zorder = data.readInt32();
         visible = data.readInt32();
         setGeometry(listener, type, index, frame, clipped, zorder, visible);
         return NO_ERROR;
      }
      break;

      case GET_VIDEO_GEOMETRY_CLIENT_ID:
      {
         CHECK_INTERFACE(IHwc, data, reply);
         sp<IHwcListener> listener = interface_cast<IHwcListener>(data.readStrongBinder());
         int type = data.readInt32();
         int index = data.readInt32();
         struct hwc_position frame, clipped;
         int zorder, visible;
         getGeometry(listener, type, index, frame, clipped, zorder, visible);
         reply->writeInt32(frame.x);
         reply->writeInt32(frame.y);
         reply->writeInt32(frame.h);
         reply->writeInt32(frame.w);
         reply->writeInt32(clipped.x);
         reply->writeInt32(clipped.y);
         reply->writeInt32(clipped.h);
         reply->writeInt32(clipped.w);
         reply->writeInt32(zorder);
         reply->writeInt32(visible);
         return NO_ERROR;
      }
      break;

      default:
         return BBinder::onTransact(code, data, reply, flags);
      break;
   }

   return NO_ERROR;
}
