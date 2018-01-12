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
#ifndef _NXWRAP__H_
#define _NXWRAP__H_

#include <utils/threads.h>
#include <utils/Errors.h>
#include "nxclient.h"
#include "nxwrap_common.h"
#include "treble/PmLibService.h"

using namespace android;

typedef bool (*StdbyMonCb)(void *ctx);
typedef void (*HpNtfyCb)(void *ctx, bool connected);
typedef void (*DcNtfyCb)(void *ctx);

// basic wrap around nexus client support, allowing any android client
// to extend into being a nexus client for middleware support.
//
// provides a nexus standby monitor integration for clients who require such,
// usually limited to clients which may have async access to nexus hardware.
//
class NxWrap {
public:
   NxWrap();
   NxWrap(const char *name);
   virtual ~NxWrap() {};

   // connect to middleware, optionally instantiating a standby monitor process.
   int join();
   int join(StdbyMonCb cb, void *ctx);
   // disconnect from middleware.
   void leave();
   // get the identification of the middleware client from server, can be used to
   // pass around android clients if needed.
   uint64_t client();
   // register a hp (hotplug) event callback through middleware interface (null to
   // unregister).
   int regHp(uint64_t cid, HpNtfyCb cb, void *ctx);
   // register a dc (display changed) event callback through middleware interface (null to
   // unregister).
   int regDc(uint64_t cid, DcNtfyCb cb, void *ctx);
   // set power state to middleware.
   void setPwr(struct pmlib_state_t *s);
   // get power state from middleware.
   void getPwr(struct pmlib_state_t *s);
   // invoke rmlmk.
   void rmlmk(uint64_t cid);
   // setup wake-on-lan
   int setWoL(const char *ifc);

   static Mutex mLck;

   void actHp(bool connected);
   void actDc();

protected:
   struct StdbyMon: public Thread {
      enum State {
         STATE_UNKNOWN,
         STATE_STOPPED,
         STATE_RUNNING
      };
      StdbyMon(StdbyMonCb cb, void *ctx);
      virtual ~StdbyMon();

      virtual status_t run(const char* name = 0,
                           int32_t priority = PRIORITY_DEFAULT,
                           size_t stack = 0);

      virtual void stop() {mState = STATE_STOPPED;}
      bool isRunning() {return (mState == STATE_RUNNING);}
      private:
         State mState;
         StdbyMonCb mCb;
         void *mCtx;
         unsigned mStdbyId;
         bool threadLoop();

         StdbyMon(const StdbyMon &);
         StdbyMon &operator=(const StdbyMon &);
   };

private:
   sp<NxWrap::StdbyMon> mStdbyMon;
   char mName[NXWRAP_NAME_MAX];
   HpNtfyCb mHpCb;
   void *mHpCtx;
   DcNtfyCb mDcCb;
   void *mDcCtx;
};

#endif
