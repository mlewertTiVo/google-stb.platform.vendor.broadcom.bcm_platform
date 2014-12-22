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

#define LOG_TAG "bcm-hwc-b"

#include <binder/IServiceManager.h>
#include <utils/Log.h>
#include <binder/Parcel.h>
#include <binder/IPCThreadState.h>
#include <utils/String16.h>
#include <utils/String8.h>
#include <utils/threads.h>
#include <binder/PermissionCache.h>

#include <cutils/properties.h>
#include <private/android_filesystem_config.h>

#include "Hwc.h"
#include "IHwc.h"

#define INVALID_HANDLE 0xDEADF00D


using namespace android;

const String16 sDump("android.permission.DUMP");

static const char *registrant_name [] = {
   "NONE",
   "HWC",
   "OMX",
   "SDB",
};

Hwc::Hwc() : BnHwc() {
   for (int i = 0; i < HWC_BINDER_VIDEO_SURFACE_SIZE; i++) {
      mVideoSurface[i].surface = (int)INVALID_HANDLE;
      mVideoSurface[i].listener = 0;
      memset(&mVideoSurface[i].frame, 0, sizeof(struct hwc_position));
      memset(&mVideoSurface[i].clipped, 0, sizeof(struct hwc_position));
      mVideoSurface[i].zorder = -1;
      mVideoSurface[i].visible = 0;
   }

   for (int i = 0; i < HWC_BINDER_SIDEBAND_SURFACE_SIZE; i++) {
      mSidebandSurface[i].surface = (int)INVALID_HANDLE;
      mSidebandSurface[i].listener = 0;
      memset(&mSidebandSurface[i].frame, 0, sizeof(struct hwc_position));
      memset(&mSidebandSurface[i].clipped, 0, sizeof(struct hwc_position));
      mSidebandSurface[i].zorder = -1;
      mSidebandSurface[i].visible = 0;
   }
}

Hwc::~Hwc() {
}

status_t Hwc::dump(int fd, const Vector<String16>& args)
{
    String8 result;

    (void)args;

    IPCThreadState* ipc = IPCThreadState::self();
    const int pid = ipc->getCallingPid();
    const int uid = ipc->getCallingUid();
    if ((uid != AID_SHELL) &&
            !PermissionCache::checkPermission(sDump, pid, uid)) {
        result.appendFormat("Permission Denial: "
                "can't dump Hwc from pid=%d, uid=%d\n", pid, uid);
    } else {
        // Try to get the main lock, but don't insist if we can't
        // (this would indicate Hwc is stuck, but we want to be able to
        // print something in dumpsys).
        int retry = 3;
        while (mLock.tryLock()<0 && --retry>=0) {
            usleep(1000000);
        }
        const bool locked(retry >= 0);
        if (!locked) {
            result.append(
                    "Hwc appears to be unresponsive, "
                    "dumping anyways (no locks held)\n");
        }

        for ( size_t i = 0; i < mNotificationListeners.size(); i++) {
            const hwc_listener_t& client = mNotificationListeners[i];
            result.appendFormat("registrant: binder 0x%p, kind %s\n",
               client.binder.get(), registrant_name[client.kind-1]);
        }

        result.appendFormat("maximum video-surface: %d\n", HWC_BINDER_VIDEO_SURFACE_SIZE);
        for ( int i = 0; i < HWC_BINDER_VIDEO_SURFACE_SIZE; i++) {
            if (mVideoSurface[i].surface != (int)INVALID_HANDLE) {
               result.appendFormat("\tvideo-surface: %d maps to 0x%x (used by 0x%x), frame: {%d,%d,%d,%d}, clipped: {%d,%d,%d,%d}, zorder:%d, visible: %s\n",
                  i, mVideoSurface[i].surface, mVideoSurface[i].listener,
                  mVideoSurface[i].frame.x, mVideoSurface[i].frame.y, mVideoSurface[i].frame.w, mVideoSurface[i].frame.h,
                  mVideoSurface[i].clipped.x, mVideoSurface[i].clipped.y, mVideoSurface[i].clipped.w, mVideoSurface[i].clipped.h,
                  mVideoSurface[i].zorder, mVideoSurface[i].visible?"oui":"non");
            }
        }

        result.appendFormat("maximum sideband-surface: %d\n", HWC_BINDER_SIDEBAND_SURFACE_SIZE);
        for ( int i = 0; i < HWC_BINDER_SIDEBAND_SURFACE_SIZE; i++) {
            if (mSidebandSurface[i].surface != (int)INVALID_HANDLE) {
               result.appendFormat("\tsideband-surface: %d maps to 0x%x (used by 0x%x), frame: {%d,%d,%d,%d}, clipped: {%d,%d,%d,%d}, zorder:%d, visible: %s\n",
                  i, mSidebandSurface[i].surface, mSidebandSurface[i].listener,
                  mSidebandSurface[i].frame.x, mSidebandSurface[i].frame.y, mSidebandSurface[i].frame.w, mSidebandSurface[i].frame.h,
                  mSidebandSurface[i].clipped.x, mSidebandSurface[i].clipped.y, mSidebandSurface[i].clipped.w, mSidebandSurface[i].clipped.h,
                  mSidebandSurface[i].zorder, mSidebandSurface[i].visible?"oui":"non");
            }
        }

        if (locked) {
            mLock.unlock();
        }
    }
    write(fd, result.string(), result.size());
    return NO_ERROR;
}

void Hwc::registerListener(const sp<IHwcListener>& listener, int kind)
{
    ALOGD("%s: %p, tid %d, from tid %d", __FUNCTION__,
          listener->asBinder().get(), gettid(),
          IPCThreadState::self()->getCallingPid());

    Mutex::Autolock _l(mLock);

    size_t N = mNotificationListeners.size();
    bool found = false;
    for ( size_t i = 0; i < N; i++) {
        const hwc_listener_t& client = mNotificationListeners[i];
        if (client.binder.get() == listener->asBinder().get()) {
           found = true;
           break;
        }
    }

    if (!found) {
       sp<IBinder> binder = listener->asBinder();
       binder->linkToDeath(this);

       mNotificationListeners.add(hwc_listener_t(binder, kind));

       if (kind == HWC_BINDER_HWC) {
          sp<IHwcListener> client = interface_cast<IHwcListener> (binder);
          client->notify(HWC_BINDER_NTFY_CONNECTED, 0, 0);
       }
    }
}

void Hwc::unregisterListener(const sp<IHwcListener>& listener)
{
    ALOGD("%s: %p, tid %d, from tid %d", __FUNCTION__,
          listener->asBinder().get(), gettid(),
          IPCThreadState::self()->getCallingPid());

    Mutex::Autolock _l(mLock);

    size_t N = mNotificationListeners.size();
    for (size_t i = 0; i < N; i++) {
        const hwc_listener_t& client = mNotificationListeners[i];
        if (client.binder.get() == listener->asBinder().get()) {
           mNotificationListeners.removeAt(i);

           for ( int j = 0; j < HWC_BINDER_VIDEO_SURFACE_SIZE; j++) {
               if (mVideoSurface[j].listener == (int)listener->asBinder().get()) {
                   mVideoSurface[j].listener = 0;
               }
           }

           for ( int j = 0; j < HWC_BINDER_SIDEBAND_SURFACE_SIZE; j++) {
               if (mSidebandSurface[j].listener == (int)listener->asBinder().get()) {
                   mSidebandSurface[j].listener = 0;
               }
           }

           break;
        }
    }
}

void Hwc::setVideoSurfaceId(const sp<IHwcListener>& listener, int index, int value)
{
    ALOGD("%s: %p, index %d, value %d", __FUNCTION__,
          listener->asBinder().get(), index, value);

    Mutex::Autolock _l(mLock);

    size_t N = mNotificationListeners.size();
    for (size_t i = 0; i < N; i++) {
        const hwc_listener_t& client = mNotificationListeners[i];
        if ((client.binder.get() == listener->asBinder().get()) &&
            (client.kind == HWC_BINDER_HWC)) {
           mVideoSurface[index].surface = value;
           mVideoSurface[index].listener = 0;
           break;
        }
    }
}

void Hwc::getVideoSurfaceId(const sp<IHwcListener>& listener, int index, int &value)
{
    Mutex::Autolock _l(mLock);

    if (index > HWC_BINDER_VIDEO_SURFACE_SIZE) {
       ALOGE("%s: %p, index %d - invalid, ignored", __FUNCTION__,
              listener->asBinder().get(), index);
       value = -1;
    } else {
       // remember who asked for it last, this is who we will
       // notify of changes.  we are not responsible for managing
       // contention, we are just a pass through service.
       mVideoSurface[index].listener = (int)listener->asBinder().get();
       value = mVideoSurface[index].surface;
       ALOGD("%s: %p, index %d, value %d", __FUNCTION__,
             listener->asBinder().get(), index, value);

       // notify back the hwc so it can reset the frame counter for this
       // session.  session equals surface scope owner claimed.
       size_t N = mNotificationListeners.size();
       for (size_t i = 0; i < N; i++) {
           const hwc_listener_t& client = mNotificationListeners[i];
           if (client.kind == HWC_BINDER_HWC) {
              sp<IBinder> binder = client.binder;
              sp<IHwcListener> client = interface_cast<IHwcListener> (binder);
              client->notify(HWC_BINDER_NTFY_VIDEO_SURFACE_ACQUIRED, value, 0);
           }
       }
   }
}

void Hwc::setVideoGeometry(const sp<IHwcListener>& listener, int index,
                           struct hwc_position &frame, struct hwc_position &clipped,
                           int zorder, int visible)
{
    ALOGV("%s: %p, index %d", __FUNCTION__,
          listener->asBinder().get(), index);

    Mutex::Autolock _l(mLock);

    size_t N = mNotificationListeners.size();
    for (size_t i = 0; i < N; i++) {
        const hwc_listener_t& client = mNotificationListeners[i];
        if ((client.binder.get() == listener->asBinder().get()) &&
            (client.kind == HWC_BINDER_HWC)) {
           if (memcmp(&frame, &mVideoSurface[index].frame, sizeof(struct hwc_position)) ||
               memcmp(&clipped, &mVideoSurface[index].clipped, sizeof(struct hwc_position))) {
              memcpy(&mVideoSurface[index].frame, &frame, sizeof(struct hwc_position));
              memcpy(&mVideoSurface[index].clipped, &clipped, sizeof(struct hwc_position));
              mVideoSurface[index].zorder = zorder;
              mVideoSurface[index].visible = visible;
              if (mVideoSurface[index].listener) {
                 for (size_t j = 0; j < N; j++) {
                     const hwc_listener_t& notify = mNotificationListeners[j];
                     if ((int)notify.binder.get() == mVideoSurface[index].listener) {
                        sp<IBinder> binder = notify.binder;
                        sp<IHwcListener> target = interface_cast<IHwcListener> (binder);
                        target->notify(HWC_BINDER_NTFY_VIDEO_SURFACE_GEOMETRY_UPDATE, index, 0);
                        break;
                     }
                 }
              }
           }
           break;
        }
    }
}

void Hwc::getVideoGeometry(const sp<IHwcListener>& listener, int index,
                           struct hwc_position &frame, struct hwc_position &clipped,
                           int &zorder, int &visible)
{
    if (index > HWC_BINDER_VIDEO_SURFACE_SIZE) {
       ALOGE("%s: %p, index %d - invalid, ignored", __FUNCTION__,
              listener->asBinder().get(), index);
       memset(&frame, 0, sizeof(struct hwc_position));
       memset(&clipped, 0, sizeof(struct hwc_position));
       zorder = -1;
       visible = 0;
    } else {
       memcpy(&frame, &mVideoSurface[index].frame, sizeof(struct hwc_position));
       memcpy(&clipped, &mVideoSurface[index].clipped, sizeof(struct hwc_position));
       zorder = mVideoSurface[index].zorder;
       visible = mVideoSurface[index].visible;
       ALOGD("%s: %p, index %d, {%d,%d,%d,%d} {%d,%d,%d,%d} z:%d, visible: %s", __FUNCTION__,
             listener->asBinder().get(), index, frame.x, frame.y, frame.w, frame.h,
             clipped.x, clipped.y, clipped.w, clipped.h, zorder, visible?"oui":"non");
   }
}

void Hwc::setDisplayFrameId(const sp<IHwcListener>& listener, int handle, int frame)
{
    ALOGV("%s: %p, index %d, value %d", __FUNCTION__,
          listener->asBinder().get(), handle, frame);

    int index = -1;
    Mutex::Autolock _l(mLock);

    for (int i = 0; i < HWC_BINDER_VIDEO_SURFACE_SIZE; i++) {
       if (mVideoSurface[i].surface == handle) {
          index = i;
          break;
       }
    }

    if ((index == -1) || !mVideoSurface[index].listener) {
       ALOGE("%s: %p, handle %x, frame %d - not registered, ignored", __FUNCTION__,
              listener->asBinder().get(), handle, frame);
    } else {
       size_t N = mNotificationListeners.size();
       for (size_t i = 0; i < N; i++) {
           const hwc_listener_t& client = mNotificationListeners[i];
           if ((int)client.binder.get() == mVideoSurface[index].listener) {
              sp<IBinder> binder = client.binder;
              sp<IHwcListener> client = interface_cast<IHwcListener> (binder);
              client->notify(HWC_BINDER_NTFY_DISPLAY, handle, frame);
              break;
           }
       }
    }
}

void Hwc::setSidebandSurfaceId(const sp<IHwcListener>& listener, int index, int value)
{
    ALOGD("%s: %p, index %d, value %d", __FUNCTION__,
          listener->asBinder().get(), index, value);

    Mutex::Autolock _l(mLock);

    size_t N = mNotificationListeners.size();
    for (size_t i = 0; i < N; i++) {
        const hwc_listener_t& client = mNotificationListeners[i];
        if ((client.binder.get() == listener->asBinder().get()) &&
            (client.kind == HWC_BINDER_HWC)) {
           mSidebandSurface[index].surface = value;
           mSidebandSurface[index].listener = 0;
           break;
        }
    }
}

void Hwc::getSidebandSurfaceId(const sp<IHwcListener>& listener, int index, int &value)
{
    Mutex::Autolock _l(mLock);

    if (index > HWC_BINDER_SIDEBAND_SURFACE_SIZE) {
       ALOGE("%s: %p, index %d - invalid, ignored", __FUNCTION__,
              listener->asBinder().get(), index);
       value = -1;
    } else {
       // remember who asked for it last, this is who we will
       // notify of changes.  we are not responsible for managing
       // contention, we are just a pass through service.
       mSidebandSurface[index].listener = (int)listener->asBinder().get();
       value = mSidebandSurface[index].surface;
       ALOGD("%s: %p, index %d, value %d", __FUNCTION__,
             listener->asBinder().get(), index, value);

       // notify back the hwc so it can reset the frame counter for this
       // session.  session equals surface scope owner claimed.
       size_t N = mNotificationListeners.size();
       for (size_t i = 0; i < N; i++) {
           const hwc_listener_t& client = mNotificationListeners[i];
           if (client.kind == HWC_BINDER_HWC) {
              sp<IBinder> binder = client.binder;
              sp<IHwcListener> client = interface_cast<IHwcListener> (binder);
              client->notify(HWC_BINDER_NTFY_SIDEBAND_SURFACE_ACQUIRED, value, 0);
           }
       }
   }
}

void Hwc::binderDied(const wp<IBinder>& who) {

    Mutex::Autolock _l(mLock);

    IBinder *binder = who.unsafe_get();
    if (binder != NULL) {
        size_t N = mNotificationListeners.size();
        for (size_t i = 0; i < N; i++) {
            const hwc_listener_t& client = mNotificationListeners[i];
            if ( client.binder == binder ) {
               mNotificationListeners.removeAt(i);
               break;
            }
        }
    }
}

status_t Hwc::onTransact( uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    return BnHwc::onTransact(code, data, reply, flags);
}

void Hwc::instantiate()
{
    defaultServiceManager()->addService(String16("broadcom.hwc"), new Hwc());
}
