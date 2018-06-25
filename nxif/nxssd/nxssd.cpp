/*
 * Copyright (C) 2017 Broadcom Canada Ltd.
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
#define LOG_NDEBUG 0
#define LOG_TAG "nxssd"

#include <log/log.h>
#include <cutils/properties.h>
#include "nxclient.h"
#include <nxwrap.h>
#include "ssd_tl.h"

static NxWrap *mNxWrap = NULL;

static bool stbMon(void *ctx)
{
    bool standby = false;

    (void) ctx;

    // TODO: is this really necessary?  if so, will need transition back to full too.
    // SSDTl_Uninit();

    standby = true;
    return standby;
}

int main(int argc, char** argv)
{
   char value[PROPERTY_VALUE_MAX];
   BERR_Code rc = BERR_SUCCESS;
   ssdd_Settings ssdd;

   (void) argc;
   (void) argv;

   property_set("dyn.nx.ssd.state", "start");

   for (;;) {
      memset(value, 0, sizeof(value));
      property_get("dyn.nx.state", value, NULL);
      if (strlen(value) && !strncmp(value, "loaded", strlen(value))) {
         break;
      }
      usleep(1000000/4);
   }

   mNxWrap = new NxWrap("nxssd");
   if (mNxWrap == NULL) {
      ALOGE("%s: could not create nexus client context!", __FUNCTION__);
      goto exit;
   } else {
      mNxWrap->join_v(stbMon, mNxWrap);
   }

   ALOGI("%s: lookup ssd settings...", __FUNCTION__);
   SSDTl_Get_Default_Settings(&ssdd);

   ALOGI("%s: init ssd...", __FUNCTION__);
   rc = SSDTl_Init(&ssdd);
   if (rc != BERR_SUCCESS) {
      ALOGE("%s: could not init ssd-tl!", __FUNCTION__);
      goto exit;
   }

   property_set("dyn.nx.ssd.state", "init");

   ALOGI("%s: wait for ssd ops...", __FUNCTION__);
   SSDTl_Wait_For_Operations();

   ALOGI("%s: terminating", __FUNCTION__);
   SSDTl_Uninit();

exit:
   property_set("dyn.nx.ssd.state", "ended");
   if (mNxWrap) {
      mNxWrap->leave();
      delete mNxWrap;
      mNxWrap = NULL;
   }
   return 0;
}

