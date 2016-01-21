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

#define LOG_TAG "bcm-hwc-svc"

#include <utils/Log.h>
#include <binder/IServiceManager.h>
#include "IHwc.h"
#include "Hwc.h"

using namespace android;

// set a maximum attempt count to reach the service, if we go
// beyond that the service is likely not working or not started,
// we should ignore this and move on, accouting for such error
// in the calling application.
//
#define ATTEMPT_PAUSE_USEC 500000
#define MAX_ATTEMPT_COUNT  4
#define HWC_SERVICE_INTERFACE "broadcom.hwc"

// client singleton for Hwc binder interface
Mutex gLock;
sp<IHwc> gHwc;
uint32_t attempt_count = 0;

const sp<IHwc>& get_hwc(bool retry)
{
   Mutex::Autolock _l(gLock);

   if (gHwc.get() == 0) {
      attempt_count = 0;
      sp<IServiceManager> sm = defaultServiceManager();
      sp<IBinder> binder;
      do {
         binder = sm->getService(String16(HWC_SERVICE_INTERFACE));
         if (binder != 0) {
            break;
         }
         usleep( ATTEMPT_PAUSE_USEC );
         attempt_count++;
      }
      while( retry && (attempt_count < MAX_ATTEMPT_COUNT) );

      if ( binder != 0 ) {
         gHwc = interface_cast<IHwc>(binder);
      }
      else {
         if ( attempt_count >= MAX_ATTEMPT_COUNT ) {
            ALOGE( "%s: giving up waiting for \'%s\'",
                   __FUNCTION__, HWC_SERVICE_INTERFACE );
         } else if (retry == false) {
            ALOGW( "%s: service not ready \'%s\'",
                   __FUNCTION__, HWC_SERVICE_INTERFACE );
         }
         gHwc = NULL;
      }
   }
   // ** user must check return value before exercizing the service interface!
   return gHwc;
}

