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
#ifndef __NXINEXUS__H__
#define __NXINEXUS__H__

#include <utils/Mutex.h>
#include <utils/ThreadDefs.h>
#include <bcm/hardware/nexus/1.0/INexus.h>
#include "nxwrap_common.h"
#include "nexusirhandler.h"
#include <linux/brcmstb/hdmi_hpd_switch.h>
#include "nexus_hdmi_output.h"
#include "nexus_display.h"
#include <fcntl.h>
#include <cutils/properties.h>
#include <utils/Vector.h>
#include "treble/PmLibService.h"

using namespace android;
using namespace android::hardware;
using namespace bcm::hardware::nexus::V1_0;

typedef void (*rmlmk)(uint64_t client);

struct HpdCb {
   uint64_t cId;
   sp<INexusHpdCb> cb;
};

struct DspCb {
   uint64_t cId;
   sp<INexusDspCb> cb;
};

class NexusImpl : public INexus, public hidl_death_recipient {
public:

   // INexus server interface.
   Return<uint64_t> client(int32_t pid);
   Return<NexusStatus> registerHpdCb(uint64_t cId, const ::android::sp<INexusHpdCb>& cb);
   Return<NexusStatus> registerDspCb(uint64_t cId, const ::android::sp<INexusDspCb>& cb);
   Return<NexusStatus> setPwr(const NexusPowerState& p);
   Return<void> getPwr(getPwr_cb _hidl_cb);
   Return<NexusStatus> rmlmk(uint64_t cId);
   Return<NexusStatus> setWoL(const hidl_string& ifc);
   Return<NexusStatus> acquireWL();
   Return<NexusStatus> releaseWL();

   // hidl_death_recipient
   virtual void serviceDied(uint64_t cookie, const wp<::android::hidl::base::V1_0::IBase>& who);

   void start_middleware();
   void stop_middleware();

   void rmlmk_callback(::rmlmk cb);

   virtual ~NexusImpl() {};

private:
   mutable Mutex mLock;
   NexusIrHandler mIrHandler;
   hdmi_state mHpd;
   Vector<struct HpdCb> mHpdCb;
   Vector<struct DspCb> mDspCb;
   PmLibService mPmLib;
   ::rmlmk mRmlmkCb;
   bool wl;

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

