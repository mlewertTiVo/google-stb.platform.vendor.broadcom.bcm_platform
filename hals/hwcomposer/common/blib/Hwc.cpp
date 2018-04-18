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
   "COM",
};

Hwc::Hwc() : BnHwc() {
   for (int i = 0; i < HWC_BINDER_VIDEO_SURFACE_SIZE; i++) {
      mVideoSurface[i].surface = (int)INVALID_HANDLE;
      mVideoSurface[i].listener = NULL;
      memset(&mVideoSurface[i].frame, 0, sizeof(struct hwc_position));
      memset(&mVideoSurface[i].clipped, 0, sizeof(struct hwc_position));
      mVideoSurface[i].zorder = -1;
      mVideoSurface[i].visible = 0;
      mVideoSurface[i].disp_w = 0;
      mVideoSurface[i].disp_h = 0;
   }

   for (int i = 0; i < HWC_BINDER_SIDEBAND_SURFACE_SIZE; i++) {
      mSidebandSurface[i].surface = (int)INVALID_HANDLE;
      mSidebandSurface[i].listener = NULL;
      memset(&mSidebandSurface[i].frame, 0, sizeof(struct hwc_position));
      memset(&mSidebandSurface[i].clipped, 0, sizeof(struct hwc_position));
      mSidebandSurface[i].zorder = -1;
      mSidebandSurface[i].visible = 0;
      mSidebandSurface[i].disp_w = 0;
      mSidebandSurface[i].disp_h = 0;
   }

   // TODO: get from backed up properties.
   overscan_position.x = 0;
   overscan_position.y = 0;
   overscan_position.w = 0;
   overscan_position.h = 0;
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
               result.appendFormat("\tvideo-surface: %d -> 0x%x, display: {%d,%d} (in-use %p), frame: {%d,%d,%d,%d}, clipped: {%d,%d,%d,%d}, zorder:%d, visible: %s\n",
                  i, mVideoSurface[i].surface, mVideoSurface[i].disp_w, mVideoSurface[i].disp_h, mVideoSurface[i].listener,
                  mVideoSurface[i].frame.x, mVideoSurface[i].frame.y, mVideoSurface[i].frame.w, mVideoSurface[i].frame.h,
                  mVideoSurface[i].clipped.x, mVideoSurface[i].clipped.y, mVideoSurface[i].clipped.w, mVideoSurface[i].clipped.h,
                  mVideoSurface[i].zorder, mVideoSurface[i].visible?"oui":"non");
            }
        }

        result.appendFormat("maximum sideband-surface: %d\n", HWC_BINDER_SIDEBAND_SURFACE_SIZE);
        for ( int i = 0; i < HWC_BINDER_SIDEBAND_SURFACE_SIZE; i++) {
            if (mSidebandSurface[i].surface != (int)INVALID_HANDLE) {
               result.appendFormat("\tsideband-surface: %d -> 0x%x, display: {%d,%d} (in-use %p), frame: {%d,%d,%d,%d}, clipped: {%d,%d,%d,%d}, zorder:%d, visible: %s\n",
                  i, mSidebandSurface[i].surface, mSidebandSurface[i].disp_w, mSidebandSurface[i].disp_h, mSidebandSurface[i].listener,
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
          listener->asBinder(listener).get(), gettid(),
          IPCThreadState::self()->getCallingPid());

    Mutex::Autolock _l(mLock);

    size_t N = mNotificationListeners.size();
    bool found = false;
    for ( size_t i = 0; i < N; i++) {
        const hwc_listener_t& client = mNotificationListeners[i];
        if (client.binder.get() == listener->asBinder(listener).get()) {
           found = true;
           break;
        }
    }

    if (!found) {
       sp<IBinder> binder = listener->asBinder(listener);
       binder->linkToDeath(this);

       mNotificationListeners.add(hwc_listener_t(binder, kind));

       if ((kind & HWC_BINDER_HWC) == HWC_BINDER_HWC) {
          sp<IHwcListener> client = interface_cast<IHwcListener> (binder);
          struct hwc_notification_info ntfy;
          memset(&ntfy, 0, sizeof(struct hwc_notification_info));
          client->notify(HWC_BINDER_NTFY_CONNECTED, ntfy);
       }
    }
}

void Hwc::unregisterListener(const sp<IHwcListener>& listener)
{
    ALOGD("%s: %p, tid %d, from tid %d", __FUNCTION__,
          listener->asBinder(listener).get(), gettid(),
          IPCThreadState::self()->getCallingPid());

    Mutex::Autolock _l(mLock);

    size_t N = mNotificationListeners.size();
    for (size_t i = 0; i < N; i++) {
        const hwc_listener_t& client = mNotificationListeners[i];
        if (client.binder.get() == listener->asBinder(listener).get()) {
           sp<IBinder> binder = listener->asBinder(listener);
           binder->unlinkToDeath(this);

           mNotificationListeners.removeAt(i);

           for ( int j = 0; j < HWC_BINDER_VIDEO_SURFACE_SIZE; j++) {
               if (mVideoSurface[j].listener == (void *)listener->asBinder(listener).get()) {
                   mVideoSurface[j].listener = 0;
               }
           }

           for ( int j = 0; j < HWC_BINDER_SIDEBAND_SURFACE_SIZE; j++) {
               if (mSidebandSurface[j].listener == (void *)listener->asBinder(listener).get()) {
                   mSidebandSurface[j].listener = 0;
               }
           }

           break;
        }
    }
}

void Hwc::setVideoSurfaceId(const sp<IHwcListener>& listener, int index, int value, int disp_w, int disp_h)
{
    ALOGD("%s: %p, index %d, value %d, display {%d,%d}", __FUNCTION__,
          listener->asBinder(listener).get(), index, value, disp_w, disp_h);

    Mutex::Autolock _l(mLock);

    size_t N = mNotificationListeners.size();
    for (size_t i = 0; i < N; i++) {
        const hwc_listener_t& client = mNotificationListeners[i];
        if ((client.binder.get() == listener->asBinder(listener).get()) &&
            ((client.kind & HWC_BINDER_HWC) == HWC_BINDER_HWC)) {
           mVideoSurface[index].surface = value;
           mVideoSurface[index].listener = 0;
           mVideoSurface[index].disp_w = disp_w;
           mVideoSurface[index].disp_h = disp_h;
           break;
        }
    }
}

void Hwc::getVideoSurfaceId(const sp<IHwcListener>& listener, int index, int &value)
{
    Mutex::Autolock _l(mLock);

    if (index > HWC_BINDER_VIDEO_SURFACE_SIZE-1) {
       ALOGE("%s: %p, index %d - invalid, ignored", __FUNCTION__,
              listener->asBinder(listener).get(), index);
       value = -1;
    } else {
       // remember who asked for it last, this is who we will
       // notify of changes.  we are not responsible for managing
       // contention, we are just a pass through service.
       mVideoSurface[index].listener = (void *)listener->asBinder(listener).get();
       value = mVideoSurface[index].surface;
       ALOGD("%s: %p, index %d, value %x", __FUNCTION__,
             listener->asBinder(listener).get(), index, value);

       // notify back the hwc so it can reset the frame counter for this
       // session.  session equals surface scope owner claimed.
       size_t N = mNotificationListeners.size();
       for (size_t i = 0; i < N; i++) {
           const hwc_listener_t& client = mNotificationListeners[i];
           if ((client.kind & HWC_BINDER_HWC) == HWC_BINDER_HWC) {
              sp<IBinder> binder = client.binder;
              sp<IHwcListener> client = interface_cast<IHwcListener> (binder);
              struct hwc_notification_info ntfy;
              memset(&ntfy, 0, sizeof(struct hwc_notification_info));
              ntfy.surface_hdl = value;
              client->notify(HWC_BINDER_NTFY_VIDEO_SURFACE_ACQUIRED, ntfy);
           }
       }
   }
}

void Hwc::freeVideoSurfaceId(const sp<IHwcListener>& listener, int index)
{
    Mutex::Autolock _l(mLock);

    if (index > HWC_BINDER_VIDEO_SURFACE_SIZE-1) {
       ALOGE("%s: %p, index %d - invalid, ignored", __FUNCTION__,
              listener->asBinder(listener).get(), index);
    } else {
       mVideoSurface[index].listener = NULL;
       ALOGD("%s: %p, index %d", __FUNCTION__,
             listener->asBinder(listener).get(), index);

       for ( int i = 0; i < HWC_BINDER_VIDEO_SURFACE_SIZE; i++) {
           if (mVideoSurface[i].listener != NULL) {
              return;
           }
       }
       for ( int i = 0; i < HWC_BINDER_SIDEBAND_SURFACE_SIZE; i++) {
           if (mSidebandSurface[i].listener != NULL) {
              return;
           }
       }
       // notify back the hwc that there is no longer any active video
       // client.
       size_t N = mNotificationListeners.size();
       for (size_t i = 0; i < N; i++) {
           const hwc_listener_t& client = mNotificationListeners[i];
           if ((client.kind & HWC_BINDER_HWC) == HWC_BINDER_HWC) {
              sp<IBinder> binder = client.binder;
              sp<IHwcListener> client = interface_cast<IHwcListener> (binder);
              struct hwc_notification_info ntfy;
              memset(&ntfy, 0, sizeof(struct hwc_notification_info));
              client->notify(HWC_BINDER_NTFY_NO_VIDEO_CLIENT, ntfy);
           }
       }
   }
}

void Hwc::setGeometry(const sp<IHwcListener>& listener, int type, int index,
                      struct hwc_position &frame, struct hwc_position &clipped,
                      int zorder, int visible)
{
    ALOGV("%s: %p, index %d", __FUNCTION__,
          listener->asBinder(listener).get(), index);
    bool updated = false;

    Mutex::Autolock _l(mLock);

    size_t N = mNotificationListeners.size();
    for (size_t i = 0; i < N; i++) {
        const hwc_listener_t& client = mNotificationListeners[i];
        if ((client.binder.get() == listener->asBinder(listener).get()) &&
            ((client.kind & HWC_BINDER_HWC) == HWC_BINDER_HWC)) {
           if (((type & HWC_BINDER_OMX) == HWC_BINDER_OMX) && (index < HWC_BINDER_VIDEO_SURFACE_SIZE)) {
              if (memcmp(&frame, &mVideoSurface[index].frame, sizeof(struct hwc_position)) ||
                  memcmp(&clipped, &mVideoSurface[index].clipped, sizeof(struct hwc_position)) ||
                  (mVideoSurface[index].zorder != zorder) ||
                  (mVideoSurface[index].visible != visible)) {
                 memcpy(&mVideoSurface[index].frame, &frame, sizeof(struct hwc_position));
                 memcpy(&mVideoSurface[index].clipped, &clipped, sizeof(struct hwc_position));
                 mVideoSurface[index].zorder = zorder;
                 mVideoSurface[index].visible = visible;
                 updated = true;
              }
           } else if (((type & HWC_BINDER_SDB) == HWC_BINDER_SDB) && (index < HWC_BINDER_SIDEBAND_SURFACE_SIZE)) {
              if (memcmp(&frame, &mSidebandSurface[index].frame, sizeof(struct hwc_position)) ||
                  memcmp(&clipped, &mSidebandSurface[index].clipped, sizeof(struct hwc_position)) ||
                  (mSidebandSurface[index].zorder != zorder) ||
                  (mSidebandSurface[index].visible != visible)) {
                 memcpy(&mSidebandSurface[index].frame, &frame, sizeof(struct hwc_position));
                 memcpy(&mSidebandSurface[index].clipped, &clipped, sizeof(struct hwc_position));
                 mSidebandSurface[index].zorder = zorder;
                 mSidebandSurface[index].visible = visible;
                 updated = true;
              }
           }
           break;
        }
    }

    // for sideband surfaces, inform of the geometry update.  for video surface, the
    // geometry information is collapsed within the frame display notification.
    //
    if (updated && ((type & HWC_BINDER_SDB) == HWC_BINDER_SDB) && (mSidebandSurface[index].listener)) {
       for (size_t j = 0; j < N; j++) {
          const hwc_listener_t& notify = mNotificationListeners[j];
          if ((void *)notify.binder.get() == mSidebandSurface[index].listener) {
             sp<IBinder> binder = notify.binder;
             sp<IHwcListener> target = interface_cast<IHwcListener> (binder);
             struct hwc_notification_info ntfy;
             memset(&ntfy, 0, sizeof(struct hwc_notification_info));
             ntfy.surface_hdl = mSidebandSurface[index].surface;
             ntfy.display_width = mSidebandSurface[index].disp_w;
             ntfy.display_height = mSidebandSurface[index].disp_h;
             memcpy(&ntfy.frame, &mSidebandSurface[index].frame, sizeof(struct hwc_position));
             memcpy(&ntfy.clipped, &mSidebandSurface[index].clipped, sizeof(struct hwc_position));
             ntfy.zorder = mSidebandSurface[index].zorder;
             target->notify(HWC_BINDER_NTFY_SIDEBAND_SURFACE_GEOMETRY_UPDATE, ntfy);
             break;
          }
       }
    }
}

void Hwc::getGeometry(const sp<IHwcListener>& listener, int type, int index,
                      struct hwc_position &frame, struct hwc_position &clipped,
                      int &zorder, int &visible)
{
    memset(&frame, 0, sizeof(struct hwc_position));
    memset(&clipped, 0, sizeof(struct hwc_position));
    zorder = -1;
    visible = 0;

    if ((((type & HWC_BINDER_OMX) == HWC_BINDER_OMX) && (index > HWC_BINDER_VIDEO_SURFACE_SIZE-1)) ||
        (((type & HWC_BINDER_SDB) == HWC_BINDER_SDB) && (index > HWC_BINDER_SIDEBAND_SURFACE_SIZE-1))) {
       ALOGE("%s: %p, type %d, index %d - invalid, ignored", __FUNCTION__,
              listener->asBinder(listener).get(), type, index);
    } else if ((type & HWC_BINDER_OMX) == HWC_BINDER_OMX) {
       memcpy(&frame, &mVideoSurface[index].frame, sizeof(struct hwc_position));
       memcpy(&clipped, &mVideoSurface[index].clipped, sizeof(struct hwc_position));
       zorder = mVideoSurface[index].zorder;
       visible = mVideoSurface[index].visible;
       ALOGD("%s:video: %p, index %d, {%d,%d,%d,%d} {%d,%d,%d,%d} z:%d, visible: %s", __FUNCTION__,
             listener->asBinder(listener).get(), index, frame.x, frame.y, frame.w, frame.h,
             clipped.x, clipped.y, clipped.w, clipped.h, zorder, visible?"oui":"non");
    } else if ((type & HWC_BINDER_SDB) == HWC_BINDER_SDB) {
       memcpy(&frame, &mSidebandSurface[index].frame, sizeof(struct hwc_position));
       memcpy(&clipped, &mSidebandSurface[index].clipped, sizeof(struct hwc_position));
       zorder = mSidebandSurface[index].zorder;
       visible = mSidebandSurface[index].visible;
       ALOGD("%s:sideband: %p, index %d, {%d,%d,%d,%d} {%d,%d,%d,%d} z:%d, visible: %s", __FUNCTION__,
             listener->asBinder(listener).get(), index, frame.x, frame.y, frame.w, frame.h,
             clipped.x, clipped.y, clipped.w, clipped.h, zorder, visible?"oui":"non");
    }
}

void Hwc::setDisplayFrameId(const sp<IHwcListener>& listener, int handle, int frame)
{
    ALOGV("%s: %p, index %d, value %d", __FUNCTION__,
          listener->asBinder(listener).get(), handle, frame);

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
              listener->asBinder(listener).get(), handle, frame);
    } else {
       size_t N = mNotificationListeners.size();
       for (size_t i = 0; i < N; i++) {
           const hwc_listener_t& client = mNotificationListeners[i];
           if ((void *)client.binder.get() == mVideoSurface[index].listener) {
              sp<IBinder> binder = client.binder;
              sp<IHwcListener> client = interface_cast<IHwcListener> (binder);
              struct hwc_notification_info ntfy;
              memset(&ntfy, 0, sizeof(struct hwc_notification_info));
              ntfy.surface_hdl = handle;
              ntfy.display_width = mVideoSurface[index].disp_w;
              ntfy.display_height = mVideoSurface[index].disp_h;
              ntfy.frame_id = frame;
              memcpy(&ntfy.frame, &mVideoSurface[index].frame, sizeof(struct hwc_position));
              memcpy(&ntfy.clipped, &mVideoSurface[index].clipped, sizeof(struct hwc_position));
              ntfy.zorder = mVideoSurface[index].zorder;
              client->notify(HWC_BINDER_NTFY_DISPLAY, ntfy);
              break;
           }
       }
    }
}

void Hwc::setSidebandSurfaceId(const sp<IHwcListener>& listener, int index, int value, int disp_w, int disp_h)
{
    ALOGV("%s: %p, index %d, value %x", __FUNCTION__,
          listener->asBinder(listener).get(), index, value);

    Mutex::Autolock _l(mLock);

    size_t N = mNotificationListeners.size();
    for (size_t i = 0; i < N; i++) {
        const hwc_listener_t& client = mNotificationListeners[i];
        if ((client.binder.get() == listener->asBinder(listener).get()) &&
            ((client.kind & HWC_BINDER_HWC) == HWC_BINDER_HWC)) {
           mSidebandSurface[index].surface = value;
           mSidebandSurface[index].disp_w = disp_w;
           mSidebandSurface[index].disp_h = disp_h;
           break;
        }
    }
}

void Hwc::getSidebandSurfaceId(const sp<IHwcListener>& listener, int index, int &value)
{
    Mutex::Autolock _l(mLock);

    if (index > HWC_BINDER_SIDEBAND_SURFACE_SIZE-1) {
       ALOGE("%s: %p, index %d - invalid, ignored", __FUNCTION__,
              listener->asBinder(listener).get(), index);
       value = -1;
    } else {
       // remember who asked for it last, this is who we will
       // notify of changes.  we are not responsible for managing
       // contention, we are just a pass through service.
       mSidebandSurface[index].listener = (void *)listener->asBinder(listener).get();
       // reset sideband surface information for new listener.
       memset(&mSidebandSurface[index].frame, 0, sizeof(struct hwc_position));
       memset(&mSidebandSurface[index].clipped, 0, sizeof(struct hwc_position));
       mSidebandSurface[index].zorder = 0;
       mSidebandSurface[index].visible = 0;

       value = mSidebandSurface[index].surface;
       ALOGD("%s: %p, index %d, value %x", __FUNCTION__,
             listener->asBinder(listener).get(), index, value);

       // notify back the hwc so it can reset the frame counter for this
       // session.  session equals surface scope owner claimed.
       size_t N = mNotificationListeners.size();
       for (size_t i = 0; i < N; i++) {
           const hwc_listener_t& client = mNotificationListeners[i];
           if ((client.kind & HWC_BINDER_HWC) == HWC_BINDER_HWC) {
              sp<IBinder> binder = client.binder;
              sp<IHwcListener> client = interface_cast<IHwcListener> (binder);
              struct hwc_notification_info ntfy;
              memset(&ntfy, 0, sizeof(struct hwc_notification_info));
              ntfy.surface_hdl = value;
              client->notify(HWC_BINDER_NTFY_SIDEBAND_SURFACE_ACQUIRED, ntfy);
           }
       }
   }
}

void Hwc::freeSidebandSurfaceId(const sp<IHwcListener>& listener, int index)
{
    Mutex::Autolock _l(mLock);

    if (index > HWC_BINDER_SIDEBAND_SURFACE_SIZE-1) {
       ALOGE("%s: %p, index %d - invalid, ignored", __FUNCTION__,
              listener->asBinder(listener).get(), index);
    } else {
       mSidebandSurface[index].listener = NULL;
       ALOGD("%s: %p, index %d", __FUNCTION__,
             listener->asBinder(listener).get(), index);

       for ( int i = 0; i < HWC_BINDER_VIDEO_SURFACE_SIZE; i++) {
           if (mVideoSurface[i].listener != NULL) {
              return;
           }
       }
       for ( int i = 0; i < HWC_BINDER_SIDEBAND_SURFACE_SIZE; i++) {
           if (mSidebandSurface[i].listener != NULL) {
              return;
           }
       }
       // notify back the hwc that there is no longer any active video
       // client.
       size_t N = mNotificationListeners.size();
       for (size_t i = 0; i < N; i++) {
           const hwc_listener_t& client = mNotificationListeners[i];
           if ((client.kind & HWC_BINDER_HWC) == HWC_BINDER_HWC) {
              sp<IBinder> binder = client.binder;
              sp<IHwcListener> client = interface_cast<IHwcListener> (binder);
              struct hwc_notification_info ntfy;
              memset(&ntfy, 0, sizeof(struct hwc_notification_info));
              client->notify(HWC_BINDER_NTFY_NO_VIDEO_CLIENT, ntfy);
           }
       }
   }
}

void Hwc::setOverscanAdjust(const sp<IHwcListener>& listener,
                      struct hwc_position &position)
{
    (void)listener;

    Mutex::Autolock _l(mLock);

    // note: it is not the purpose to protect against
    //       stupid settings.
    overscan_position.x = position.x;
    overscan_position.y = position.y;
    overscan_position.w = position.w;
    overscan_position.h = position.h;

    // notify back the hwc so it can reset the frame counter for this
    // session.  session equals surface scope owner claimed.
    size_t N = mNotificationListeners.size();
    for (size_t i = 0; i < N; i++) {
        const hwc_listener_t& client = mNotificationListeners[i];
        if ((client.kind & HWC_BINDER_HWC) == HWC_BINDER_HWC) {
           sp<IBinder> binder = client.binder;
           sp<IHwcListener> client = interface_cast<IHwcListener> (binder);
           struct hwc_notification_info ntfy;
           memset(&ntfy, 0, sizeof(struct hwc_notification_info));
           ntfy.frame.x = overscan_position.x;
           ntfy.frame.y = overscan_position.y;
           ntfy.frame.w = overscan_position.w;
           ntfy.frame.h = overscan_position.h;
           client->notify(HWC_BINDER_NTFY_OVERSCAN, ntfy);
        }
    }
}

void Hwc::getOverscanAdjust(const sp<IHwcListener>& listener,
                      struct hwc_position &position)
{
    (void)listener;

    Mutex::Autolock _l(mLock);

    position.x = overscan_position.x;
    position.y = overscan_position.y;
    position.w = overscan_position.w;
    position.h = overscan_position.h;
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
#if defined(BCM_FULL_TREBLE)
    ProcessState::initWithDriver("/dev/vndbinder");
#endif
    defaultServiceManager()->addService(String16("broadcom.hwc"), new Hwc());
}
