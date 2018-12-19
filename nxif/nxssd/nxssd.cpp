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
#include "vendor_bcm_props.h"

static NxWrap *mNxWrap = NULL;
static bool mStandby = false;
static BKNI_EventHandle terminate;
static pthread_t wait_task;

static BERR_Code load() {
   BERR_Code rc = BERR_SUCCESS;
   ssdd_Settings ssdd;
   ALOGI("%s: lookup ssd settings...", __FUNCTION__);
   SSDTl_Get_Default_Settings(&ssdd);
   ALOGI("%s: init ssd...", __FUNCTION__);
   rc = SSDTl_Init(&ssdd);
   if (rc != BERR_SUCCESS) {
      ALOGE("%s: could not init ssd-tl!", __FUNCTION__);
   }
   property_set(BCM_DYN_SSD_STATE, "init");
   return rc;
}

static void *wait_ops(
   void *argv) {
   (void)argv;
   ALOGI("%s: wait for ssd ops...", __FUNCTION__);
   SSDTl_Wait_For_Operations();
   return NULL;
}

static void wait() {
   pthread_attr_t attr;
   pthread_attr_init(&attr);
   pthread_create(&wait_task, &attr, wait_ops, (void *)NULL);
   pthread_attr_destroy(&attr);
   pthread_setname_np(wait_task, "nxssd_worker");
}

static bool stdbMon(
   void *ctx) {
   (void) ctx;
   nxwrap_pwr_state pwr_s;
   bool pwr = nxwrap_get_pwr_info(&pwr_s, NULL);
   if (pwr && (pwr_s > ePowerState_S2)) {
      SSDTl_Shutdown();
      pthread_join(wait_task, NULL);
      mStandby = true;
   }
   return true;
}

static void stdbChg(
   void *context,
   int param) {
   (void)context;
   (void)param;
   nxwrap_pwr_state pwr_s;
   bool pwr = nxwrap_get_pwr_info(&pwr_s, NULL);
   if (pwr && (pwr_s <= ePowerState_S2)) {
      if (mStandby) {
         mStandby = false;
         if (load() == BERR_SUCCESS) wait();
      }
   }
}

int main(int argc, char** argv) {
   char value[PROPERTY_VALUE_MAX];
   NxClient_CallbackThreadSettings cts;
   BERR_Code rc = BERR_SUCCESS;

   (void) argc;
   (void) argv;

   property_set(BCM_DYN_SSD_STATE, "start");

   for (;;) {
      memset(value, 0, sizeof(value));
      property_get(BCM_DYN_NX_STATE, value, NULL);
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
      mNxWrap->join_v(stdbMon, mNxWrap);
   }

   BKNI_CreateEvent(&terminate);

   NxClient_GetDefaultCallbackThreadSettings(&cts);
   cts.standbyStateChanged.callback  = stdbChg;
   cts.standbyStateChanged.context   = (void *)NULL;
   rc = NxClient_StartCallbackThread(&cts);

   if (load() == BERR_SUCCESS) {
      wait();
      /* never signalling effectively. */
      BKNI_WaitForEvent(terminate, BKNI_INFINITE);
   }
   BKNI_DestroyEvent(terminate);

   ALOGI("%s: terminating", __FUNCTION__);
   SSDTl_Uninit();

exit:
   property_set(BCM_DYN_SSD_STATE, "ended");
   if (mNxWrap) {
      mNxWrap->leave();
      delete mNxWrap;
      mNxWrap = NULL;
   }
   return 0;
}

