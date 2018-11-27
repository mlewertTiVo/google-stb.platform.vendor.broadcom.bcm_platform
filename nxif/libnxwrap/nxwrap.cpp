/******************************************************************************
 * (c) 2017 Broadcom
 *
 * This program is the proprietary software of Broadcom and/or its licensors,
 * and may only be used, duplicated, modified or distributed pursuant to the terms and
 * conditions of a separate, written license agreement executed between you and Broadcom
 * (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 * no license (express or implied), right to use, or waiver of any kind with respect to the
 * Software, and Broadcom expressly reserves all rights in and to the Software and all
 * intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 * HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 * NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 * secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 * and to use this information only in connection with your use of Broadcom integrated circuit products.
 *
 * 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES
 * OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE,
 * LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION
 * OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF
 * USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
 * LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR
 * EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR
 * USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT
 * ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
 * LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF
 * ANY LIMITED REMEDY.
 *
 *****************************************************************************/
#define LOG_TAG "nxwrap"
//#define LOG_NDEBUG 0

#include <utils/Log.h>
#include <string.h>
#include <cutils/atomic.h>
#include <utils/Errors.h>
#include <utils/Mutex.h>
#include "cutils/properties.h"
#include <nxwrap.h>
#include "nexus_platform.h"
#include <bcm/hardware/nexus/1.1/INexus.h>
#if defined(SRAI_PRESENT)
#include "sage_srai.h"
#endif

using namespace android;
using namespace android::hardware;
using namespace bcm::hardware::nexus::V1_0;
using namespace bcm::hardware::nexus::V1_1;

Mutex NxWrap::mLck("NxWrap-Lock");
static struct pmlib_state_t gPm;

class NxWrapHpd : public INexusHpdCb {
public:
   NxWrapHpd(NxWrap *p) { mParent = p; };
   NxWrap *mParent;
   Return<void> onHpd(bool connected) { if (mParent) mParent->actHp(connected); return Void(); };
};
static NxWrapHpd *gNxiHpd = NULL;

class NxWrapDsp : public INexusDspCb {
public:
   NxWrapDsp(NxWrap *p) { mParent = p; };
   NxWrap *mParent;
   Return<void> onDsp() { if (mParent) mParent->actDc(); return Void(); };
};
static NxWrapDsp *gNxiDsp = NULL;

static const sp<::bcm::hardware::nexus::V1_1::INexus> nxi(void) {
   sp<::bcm::hardware::nexus::V1_1::INexus> inx = NULL;
   Mutex::Autolock _l(NxWrap::mLck);

   inx = ::bcm::hardware::nexus::V1_1::INexus::getService();
   if (inx != NULL) {
      return inx;
   }

   return NULL;
}

NxWrap::NxWrap(bool w_nxi) {
   snprintf(mName, NXWRAP_NAME_MAX, "nxwrap-%d", getpid());
   if (w_nxi) {
      gNxiHpd = new NxWrapHpd(this);
      gNxiDsp = new NxWrapDsp(this);
   } else {
      gNxiHpd = NULL;
      gNxiDsp = NULL;
   }
}

NxWrap::NxWrap(const char *name, bool w_nxi) {
   snprintf(mName, NXWRAP_NAME_MAX, "%s", name);
   if (w_nxi) {
      gNxiHpd = new NxWrapHpd(this);
      gNxiDsp = new NxWrapDsp(this);
   } else {
      gNxiHpd = NULL;
      gNxiDsp = NULL;
   }
}

void NxWrap::actHp(bool connected) {
   if (mHpCb) {
      mHpCb(mHpCtx, connected);
   }
}

void NxWrap::actDc() {
   if (mDcCb) {
      mDcCb(mDcCtx);
   }
}

int NxWrap::join() {
   NEXUS_Error rc = NEXUS_SUCCESS;
   NxClient_JoinSettings joinSettings;
   NEXUS_PlatformStatus status;

   if (nxi() != NULL) {
      if (client()) {
         ALOGI("join(): client already registered, ignoring.");
      }
   }

   Mutex::Autolock autoLock(mLck);
   NxClient_GetDefaultJoinSettings(&joinSettings);
   joinSettings.ignoreStandbyRequest = true;
   sprintf(joinSettings.name, NX_NOGRAB_MAGIC);
   do {
      rc = NxClient_Join(&joinSettings);
      // try to join server forever.
      if (rc != NEXUS_SUCCESS) {
         usleep(NXCLIENT_SERVER_TIMEOUT_IN_MS * 1000);
       }
   } while (rc != NEXUS_SUCCESS);
   return rc;
}

#define NXWRAP_JOIN_V "/vendor/usr/jwl"
int NxWrap::join_v() {
   NEXUS_Error rc = NEXUS_SUCCESS;
   NxClient_JoinSettings joinSettings;
   NEXUS_PlatformStatus status;

   if (nxi() != NULL) {
      if (client()) {
         ALOGI("join_v(): client already registered, ignoring.");
      }
   }

   Mutex::Autolock autoLock(mLck);
   NxClient_GetDefaultJoinSettings(&joinSettings);
   joinSettings.ignoreStandbyRequest = true;
   if (!access(NXWRAP_JOIN_V, R_OK)) {
      joinSettings.mode = NEXUS_ClientMode_eVerified;
      ALOGW("join_v: wanting verified client.");
   }
   sprintf(joinSettings.name, NX_NOGRAB_MAGIC);
   do {
      rc = NxClient_Join(&joinSettings);
      // try to join server forever.
      if (rc != NEXUS_SUCCESS) {
         usleep(NXCLIENT_SERVER_TIMEOUT_IN_MS * 1000);
       }
   } while (rc != NEXUS_SUCCESS);
   return rc;
}

int NxWrap::join_once() {
   NEXUS_Error rc = NEXUS_SUCCESS;
   NxClient_JoinSettings joinSettings;
   NEXUS_PlatformStatus status;

   if (nxi() != NULL) {
      if (client()) {
         ALOGI("join_once(): client already registered, ignoring.");
      }
   }

   Mutex::Autolock autoLock(mLck);
   NxClient_GetDefaultJoinSettings(&joinSettings);
   joinSettings.ignoreStandbyRequest = true;
   sprintf(joinSettings.name, NX_NOGRAB_MAGIC);
   rc = NxClient_Join(&joinSettings);
   return rc;
}

int NxWrap::join(StdbyMonCb cb, void *ctx) {
   NEXUS_Error rc = NEXUS_SUCCESS;
   NxClient_JoinSettings joinSettings;
   NEXUS_PlatformStatus status;

   if (nxi() != NULL) {
      if (client()) {
         ALOGI("join(stbdy): client already registered, ignoring.");
      }
   }

   if (cb == NULL) {
      return join();
   }

   Mutex::Autolock autoLock(mLck);
   NxClient_GetDefaultJoinSettings(&joinSettings);
   sprintf(joinSettings.name, NX_NOGRAB_MAGIC);
   do {
      rc = NxClient_Join(&joinSettings);
      // try to join server forever.
      if (rc != NEXUS_SUCCESS) {
         usleep(NXCLIENT_SERVER_TIMEOUT_IN_MS * 1000);
       }
   } while (rc != NEXUS_SUCCESS);

   if (cb != NULL) {
      mStdbyMon = new NxWrap::StdbyMon(cb, ctx);
      if (mStdbyMon != NULL) {
         mStdbyMon->run(mName, ANDROID_PRIORITY_NORMAL);
      }
   }

   return rc;
}

int NxWrap::join_v(StdbyMonCb cb, void *ctx) {
   NEXUS_Error rc = NEXUS_SUCCESS;
   NxClient_JoinSettings joinSettings;
   NEXUS_PlatformStatus status;

   if (nxi() != NULL) {
      if (client()) {
         ALOGI("join_v(stdby): client already registered, ignoring.");
      }
   }

   if (cb == NULL) {
      return join();
   }

   Mutex::Autolock autoLock(mLck);
   NxClient_GetDefaultJoinSettings(&joinSettings);
   sprintf(joinSettings.name, NX_NOGRAB_MAGIC);
   if (!access(NXWRAP_JOIN_V, R_OK)) {
      joinSettings.mode = NEXUS_ClientMode_eVerified;
      ALOGW("join_v(stdby): wanting verified client.");
   }
   do {
      rc = NxClient_Join(&joinSettings);
      // try to join server forever.
      if (rc != NEXUS_SUCCESS) {
         usleep(NXCLIENT_SERVER_TIMEOUT_IN_MS * 1000);
       }
   } while (rc != NEXUS_SUCCESS);

   if (cb != NULL) {
      mStdbyMon = new NxWrap::StdbyMon(cb, ctx);
      if (mStdbyMon != NULL) {
         mStdbyMon->run(mName, ANDROID_PRIORITY_NORMAL);
      }
   }

   return rc;
}

void NxWrap::leave() {
   Mutex::Autolock autoLock(mLck);

   if (mStdbyMon != NULL &&
       mStdbyMon->isRunning()) {
      mStdbyMon->stop();
      mStdbyMon->join();
      mStdbyMon = NULL;
   }

   if (gNxiHpd != NULL) {
      delete gNxiHpd;
      gNxiHpd = NULL;
   }

   if (gNxiDsp != NULL) {
      delete gNxiDsp;
      gNxiDsp = NULL;
   }

   NxClient_Uninit();
}

uint64_t NxWrap::client() {
   uint64_t client = 0;
   if (nxi() != NULL) {
      client = nxi()->client(getpid());
   }
   return client;
}

void NxWrap::setPwr(struct pmlib_state_t *s) {
   NexusPowerState p;
   memcpy(&p, s, sizeof(struct pmlib_state_t));
   if (nxi() != NULL) {
      nxi()->setPwr(p);
   }
}

void getPwr_cb(const NexusPowerState p) {
   memcpy(&gPm, &p, sizeof(struct pmlib_state_t));
}
void NxWrap::getPwr(struct pmlib_state_t *s) {
   if (nxi() != NULL) {
      nxi()->getPwr(getPwr_cb);
   }
   memcpy(s, &gPm, sizeof(struct pmlib_state_t));
}

int NxWrap::regHp(uint64_t cid, HpNtfyCb cb, void *ctx) {
   if (nxi() != NULL) {
      nxi()->registerHpdCb(cid, gNxiHpd);
   }
   mHpCb  = cb;
   mHpCtx = ctx;
   return 0;
}

int NxWrap::regDc(uint64_t cid, DcNtfyCb cb, void *ctx) {
   if (nxi() != NULL) {
      nxi()->registerDspCb(cid, gNxiDsp);
   }
   mDcCb  = cb;
   mDcCtx = ctx;
   return 0;
}

NxWrap::StdbyMon::StdbyMon(StdbyMonCb cb, void *ctx) :
   mState(STATE_STOPPED),
   mCb(cb),
   mCtx(ctx) {
   mStdbyId = NxClient_RegisterAcknowledgeStandby();
}

NxWrap::StdbyMon::~StdbyMon() {
   NxClient_UnregisterAcknowledgeStandby(mStdbyId);
}

status_t NxWrap::StdbyMon::run(const char* name, int32_t priority, size_t stack)
{
   status_t status;
   Mutex::Autolock autoLock(mSmLck);
   mState = StdbyMon::STATE_RUNNING;
   status = Thread::run(name, priority, stack);
   if (status != OK) {
      mState = StdbyMon::STATE_STOPPED;
   }
   return status;
}

bool NxWrap::StdbyMon::threadLoop()
{
   NEXUS_Error rc;
   NxClient_StandbyStatus standbyStatus;

   while (isRunning()) {
      rc = NxClient_GetStandbyStatus(&standbyStatus);
      if (rc == NEXUS_SUCCESS &&
          standbyStatus.transition == NxClient_StandbyTransition_eAckNeeded) {
         if (standbyStatus.settings.mode != NEXUS_PlatformStandbyMode_eOn &&
             mCb(mCtx)) {
            NxClient_AcknowledgeStandby(mStdbyId);
         }
      }
      Mutex::Autolock autoLock(mSmLck);
      mSmCond.waitRelative(mSmLck,
         NXCLIENT_STANDBY_MONITOR_TIMEOUT_IN_MS * 1000000ll);
   }
   return false;
}

void NxWrap::rmlmk(uint64_t client) {
   if (nxi() != NULL) {
      nxi()->rmlmk(client);
   }
}

int NxWrap::setWoL(const char *ifc) {
   hidl_string hifc(ifc);
   if (nxi() != NULL) {
      NexusStatus status = nxi()->setWoL(hifc);
      return status == NexusStatus::SUCCESS ? 0 : -EAGAIN;
   }
   return -EAGAIN;
}

int NxWrap::acquireWL() {
   if (nxi() != NULL) {
      NexusStatus status = nxi()->acquireWL();
      return status == NexusStatus::SUCCESS ? 0 : -EINVAL;
   }
   return -EAGAIN;
}

int NxWrap::releaseWL() {
   if (nxi() != NULL) {
      NexusStatus status = nxi()->releaseWL();
      return status == NexusStatus::SUCCESS ? 0 : -EINVAL;
   }
   return -EAGAIN;
}

void NxWrap::sraiClient() {
#if defined(SRAI_PRESENT)
   SRAI_Settings ss;
   SRAI_GetSettings(&ss);
   ss.generalHeapIndex     = NXCLIENT_FULL_HEAP;
   ss.videoSecureHeapIndex = NXCLIENT_VIDEO_SECURE_HEAP;
   ss.exportHeapIndex      = NXCLIENT_EXPORT_HEAP;
   SRAI_SetSettings(&ss);
   return;
#else
   // no-op.
   return;
#endif
}

// helper functions for easy hook up.  creates the middleware client and returns a
// reference to it, no standby activation in place since simple client.
//
extern "C" void* nxwrap_create_client(void **nxwrap) {

   uint64_t client = 0;
   NxWrap *nx = new NxWrap(false);

   if (nx != NULL) {
      *nxwrap = nx;
      client = nx->client();
      if (!client) {
         nx->join();
         if (nxi() == NULL) {
            client = (uint64_t)(intptr_t)nx;
         } else {
            client = nx->client();
         }
      }
   }

   return (void *)(intptr_t)client;
}

extern "C" void* nxwrap_create_verified_client(void **nxwrap) {

   uint64_t client = 0;
   NxWrap *nx = new NxWrap(false);

   if (nx != NULL) {
      *nxwrap = nx;
      client = nx->client();
      if (!client) {
         nx->join_v();
         if (nxi() == NULL) {
            client = (uint64_t)(intptr_t)nx;
         } else {
            client = nx->client();
         }
      }
   }

   return (void *)(intptr_t)client;
}

extern "C" void nxwrap_destroy_client(void *nxwrap) {

   NxWrap *nx = (NxWrap *)nxwrap;

   if (nx != NULL) {
      nx->leave();
      delete nx;
      nx = NULL;
   }
}

extern "C" void nxwrap_rmlmk(void *nxwrap) {

   NxWrap *nx = (NxWrap *)nxwrap;

   if (nx != NULL) {
      nx->rmlmk(nx->client());
   }
}

// helpers function for accessing some common information that can be needed
// by various modules/hals.
//
extern "C" const char *nxwrap_get_power_string(nxwrap_pwr_state state) {
   switch (state) {
   case ePowerState_S0: return "S0";
   case ePowerState_S05: return "S0.5";
   case ePowerState_S1: return "S1";
   case ePowerState_S2: return "S2";
   case ePowerState_S3: return "S3";
   case ePowerState_S4: return "S4";
   case ePowerState_S5: return "S5";
   default: return "";
   }
}

extern "C" bool nxwrap_get_pwr_info(nxwrap_pwr_state *state,
                                    nxwrap_wake_status *wake) {
   NEXUS_Error rc;
   NxClient_StandbyStatus standbyStatus;
   rc = NxClient_GetStandbyStatus(&standbyStatus);

   if (rc == NEXUS_SUCCESS) {
      switch (standbyStatus.settings.mode) {
      case NEXUS_PlatformStandbyMode_eOn: *state = ePowerState_S0; break;
      case NEXUS_PlatformStandbyMode_eActive: *state = ePowerState_S1; break;
      case NEXUS_PlatformStandbyMode_ePassive: *state = ePowerState_S2; break;
      case NEXUS_PlatformStandbyMode_eDeepSleep: *state = ePowerState_S3; break;
      default: *state = ePowerState_Max;
      }

      if (wake && *state != ePowerState_Max) {
         wake->ir        = standbyStatus.status.wakeupStatus.ir;
         wake->uhf       = standbyStatus.status.wakeupStatus.uhf;
         wake->keypad    = standbyStatus.status.wakeupStatus.keypad;
         wake->gpio      = standbyStatus.status.wakeupStatus.gpio;
         wake->nmi       = standbyStatus.status.wakeupStatus.nmi;
         wake->cec       = standbyStatus.status.wakeupStatus.cec;
         wake->transport = standbyStatus.status.wakeupStatus.transport;
         wake->timeout   = standbyStatus.status.wakeupStatus.timeout;

         ALOGD("Standby Status : \n"
               "State   : %s\n"
               "IR      : %d\n"
               "UHF     : %d\n"
               "Keypad  : %d\n"
               "XPT     : %d\n"
               "CEC     : %d\n"
               "GPIO    : %d\n"
               "Timeout : %d\n",
               nxwrap_get_power_string(*state),
               wake->ir,
               wake->uhf,
               wake->keypad,
               wake->transport,
               wake->cec,
               wake->gpio,
               wake->timeout);
      }
   }

   return (rc == NEXUS_SUCCESS) ? true : false;
}

static NEXUS_Error nxwrap_standby_check(NEXUS_PlatformStandbyMode mode) {
   NxClient_StandbyStatus standbyStatus;
   NEXUS_Error rc;
   int count = 0;

   while (count < NXCLIENT_PM_TIMEOUT_COUNT) {
      rc = NxClient_GetStandbyStatus(&standbyStatus);
      if (rc == NEXUS_SUCCESS && standbyStatus.settings.mode == mode && standbyStatus.transition == NxClient_StandbyTransition_eDone) {
         break;
      } else {
         usleep(NXCLIENT_PM_TIMEOUT_IN_MS * 1000 / NXCLIENT_PM_TIMEOUT_COUNT);
         count++;
      }
   }
   return (count < NXCLIENT_PM_TIMEOUT_COUNT) ? NEXUS_SUCCESS : NEXUS_TIMEOUT;
}

extern "C" bool nxwrap_set_power_state(nxwrap_pwr_state state, bool cec_wake) {
   NEXUS_Error rc = NEXUS_SUCCESS;
   NxClient_StandbySettings standbySettings;
   NxClient_GetDefaultStandbySettings(&standbySettings);
   standbySettings.settings.wakeupSettings.ir = 1;
   standbySettings.settings.wakeupSettings.uhf = 1;
   standbySettings.settings.wakeupSettings.transport = 1;
   standbySettings.settings.wakeupSettings.cec = cec_wake?1:0;
   standbySettings.settings.wakeupSettings.gpio = 1;
   standbySettings.settings.wakeupSettings.timeout = 0;

   switch (state) {
   case ePowerState_S0: standbySettings.settings.mode = NEXUS_PlatformStandbyMode_eOn; break;
   case ePowerState_S05: standbySettings.settings.mode = NEXUS_PlatformStandbyMode_eOn; break;
   case ePowerState_S1: standbySettings.settings.mode = NEXUS_PlatformStandbyMode_eActive; break;
   case ePowerState_S2: standbySettings.settings.mode = NEXUS_PlatformStandbyMode_ePassive; break;
   case ePowerState_S3: case ePowerState_S5: standbySettings.settings.mode = NEXUS_PlatformStandbyMode_eDeepSleep; break;
   default: rc = NEXUS_INVALID_PARAMETER; break;
   }

   if (rc == NEXUS_SUCCESS) {
      NxClient_StandbyStatus standbyStatus;
      rc = NxClient_GetStandbyStatus(&standbyStatus);
      if (rc == NEXUS_SUCCESS) {
         if (memcmp(&standbyStatus.settings, &standbySettings.settings, sizeof(standbyStatus.settings))) {
            rc = NxClient_SetStandbySettings(&standbySettings);
            if (rc != NEXUS_SUCCESS) {
               ALOGE("%s: NxClient_SetStandbySettings failed [rc=%d]!", __PRETTY_FUNCTION__, rc);
            } else if (state != ePowerState_S0 && state != ePowerState_S05) {
               rc = nxwrap_standby_check(standbySettings.settings.mode);
               if (rc != NEXUS_SUCCESS) {
                  ALOGE("%s: standby check failed [rc=%d]!", __PRETTY_FUNCTION__, rc);
               }
            }
         }
      } else {
         ALOGE("%s: NxClient_GetStandbyStatus failed [rc=%d]!", __PRETTY_FUNCTION__, rc);
      }
   }
   return (rc == NEXUS_SUCCESS) ? true : false;
}

