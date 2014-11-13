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

using namespace android;

const String16 sDump("android.permission.DUMP");

static const char *registrant_name [] = {
   "NONE",
   "HWC",
   "OMX",
};

Hwc::Hwc() : BnHwc() {
   for (int i = 0; i < HWC_BINDER_VIDEO_SURFACE_SIZE; i++)
      mVideoSurfaceId[i] = -1;
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
            result.appendFormat("registrant: binder 0x%x, kind %s\n",
               client.listener, registrant_name[client.kind-1]);
        }

        result.appendFormat("maximum video-surface: %d\n", HWC_BINDER_VIDEO_SURFACE_SIZE);
        for ( int i = 0; i < HWC_BINDER_VIDEO_SURFACE_SIZE; i++) {
            if (mVideoSurfaceId[i] != -1) {
               result.appendFormat("\tvideo-surface: %d maps to %d\n",
                  i, mVideoSurfaceId[i]);
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
        if ( client.listener == (int) listener->asBinder().get() ) {
           found = true;
           break;
        }
    }

    if (!found) {
       sp<IBinder> binder = listener->asBinder();
       binder->linkToDeath(this);

       mNotificationListeners.add( hwc_listener_t((int)binder.get(), kind));

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
        if ( client.listener == (int) listener->asBinder().get() ) {
           mNotificationListeners.removeAt(i);
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
        if ((client.listener == (int) listener->asBinder().get()) &&
            (client.kind == HWC_BINDER_HWC)) {
           mVideoSurfaceId[index] = value;
           break;
        }
    }
}

void Hwc::getVideoSurfaceId(const sp<IHwcListener>& listener, int index, int &value)
{
    Mutex::Autolock _l(mLock);

    value = mVideoSurfaceId[index];

    ALOGD("%s: %p, index %d, value %d", __FUNCTION__,
          listener->asBinder().get(), index, value);
}

void Hwc::setDisplayFrameId(const sp<IHwcListener>& listener, int surface, int frame)
{
    ALOGD("%s: %p, index %d, value %d", __FUNCTION__,
          listener->asBinder().get(), surface, frame);

    Mutex::Autolock _l(mLock);

    size_t N = mNotificationListeners.size();
    for (size_t i = 0; i < N; i++) {
       // TODO
    }
}

void Hwc::binderDied(const wp<IBinder>& who) {

    Mutex::Autolock _l(mLock);

    IBinder *binder = who.unsafe_get();
    if (binder != NULL) {
        size_t N = mNotificationListeners.size();
        for (size_t i = 0; i < N; i++) {
            const hwc_listener_t& client = mNotificationListeners[i];
            if ( client.listener == (int) binder ) {
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
