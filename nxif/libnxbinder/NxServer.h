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

#ifndef __NXSERVER__H__INCLUDED__
#define __NXSERVER__H__INCLUDED__

#include <utils/Atomic.h>
#include <utils/Errors.h>
#include <utils/threads.h>
#include <binder/MemoryDealer.h>
#include <utils/KeyedVector.h>
#include <utils/SortedVector.h>
#include <utils/Vector.h>

#include "INxServer.h"
#include <binder/Binder.h>
#include "NxServerSvc.h"
#include "nexusirhandler.h"
#include <linux/brcmstb/hdmi_hpd_switch.h>
#include <INxHpdEvtSrc.h>
#include <INxDspEvtSrc.h>
#include "nexus_hdmi_output.h"
#include "nexus_display.h"
#include <fcntl.h>

using namespace android;

typedef void (*rmlmk)(uint64_t client);

class NxServer: public BnNxServer, public IBinder::DeathRecipient
{
public:
   static NxServer *instantiate(::rmlmk cb);
   void terminate();

   virtual void getNxClient(int pid, uint64_t &client);

   virtual status_t regHpdEvt(const sp<INxHpdEvtSrc> &listener);
   virtual status_t unregHpdEvt(const sp<INxHpdEvtSrc> &listener);

   virtual status_t regDspEvt(const sp<INxDspEvtSrc> &listener);
   virtual status_t unregDspEvt(const sp<INxDspEvtSrc> &listener);

   virtual void rmlmk(uint64_t client);

   virtual status_t onTransact(uint32_t code,
                               const Parcel& data,
                               Parcel* reply,
                               uint32_t flags);

private:
   NxServer(::rmlmk cb);
   virtual ~NxServer() {};

   NxServer *mWhoami;
   NexusIrHandler mIrHandler;
   mutable Mutex mLock;
   hdmi_state mHpd;
   Vector<sp<INxHpdEvtSrc>> mHpdEvtListener;
   Vector<sp<INxDspEvtSrc>> mDspEvtListener;
   ::rmlmk mRmlmkCb;

   void binderDied(const wp<IBinder>& who);

   void start_middleware();
   void stop_middleware();

   void init_hdmi_out();
   void deinit_hdmi_out();
   void cbHpdAction(hdmi_state state);
   NEXUS_VideoFormat forcedOutputFmt();
   NEXUS_VideoFormat bestOutputFmt(NEXUS_HdmiOutputStatus *status, NEXUS_DisplayCapabilities *caps);
   void cbDisplayAction();
   bool getLimitedColorSettings(unsigned &limitedColorDepth,
                                NEXUS_ColorSpace &limitedColorSpace);
   bool getForcedHdcp(void);

   bool init_ir();
   void deinit_ir();

   // callbacks registered with nexus.
   static void cbHotplug(void *ctx, int param);
   static void cbHdcp(void *ctx, int param);
   static void cbDisplay(void *ctx, int param);
};

#endif
