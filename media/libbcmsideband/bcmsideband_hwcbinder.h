/******************************************************************************
 *    (c)2014 Broadcom Corporation
 *
 * This program is the proprietary software of Broadcom Corporation and/or its licensors,
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
 *****************************************************************************/
#ifndef BCMSIDEBAND_HWCBINDER_H__
#define BCMSIDEBAND_HWCBINDER_H__

#include <cutils/log.h>

#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include "HwcCommon.h"
#include "Hwc.h"
#include "HwcListener.h"
#include "IHwc.h"
#include "HwcSvc.h"

using namespace android;

typedef void (* BCMSIDEBAND_BINDER_NTFY_CB)(int, int, struct hwc_notification_info &);

class BcmSidebandBinder : public HwcListener
{
public:

    BcmSidebandBinder() : cb(NULL), cb_data(0) {};
    virtual ~BcmSidebandBinder() {};

    virtual void notify(int msg, struct hwc_notification_info &ntfy);

    inline void listen() {
       if (get_hwc(false) != NULL)
           get_hwc(false)->registerListener(this, HWC_BINDER_SDB);
       else
           ALOGE("%s: failed to associate %p with BcmSidebandBinder service.", __FUNCTION__, this);
    };

    inline void hangup() {
       if (get_hwc(false) != NULL)
           get_hwc(false)->unregisterListener(this);
       else
           ALOGE("%s: failed to dissociate %p from BcmSidebandBinder service.", __FUNCTION__, this);
    };

    inline void getvideo(int index, int &value) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->getVideoSurfaceId(this, index, value);
       }
    };

    inline void getsideband(int index, int &value) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->getSidebandSurfaceId(this, index, value);
       }
    };

    inline void getgeometry(int type, int index,
                            struct hwc_position &frame, struct hwc_position &clipped,
                            int &zorder, int &visible) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->getGeometry(this, type, index, frame, clipped, zorder, visible);
       }
    };

    void register_notify(BCMSIDEBAND_BINDER_NTFY_CB callback, int data) {
       cb = callback;
       cb_data = data;
    }

private:
    BCMSIDEBAND_BINDER_NTFY_CB cb;
    int cb_data;
};

class BcmSidebandBinder_wrap
{
private:

   sp<BcmSidebandBinder> ihwc;

public:
   BcmSidebandBinder_wrap(void) {
      ALOGV("%s: allocated %p", __FUNCTION__, this);
      ihwc = new BcmSidebandBinder;
      ihwc.get()->listen();
   };

   virtual ~BcmSidebandBinder_wrap(void) {
      ALOGV("%s: cleared %p", __FUNCTION__, this);
      ihwc.get()->hangup();
      ihwc.clear();
   };

   void getvideo(int index, int &value) {
      ihwc.get()->getvideo(index, value);
   }

   void getsideband(int index, int &value) {
      ihwc.get()->getsideband(index, value);
   }

   void getgeometry(int type, int index,
                    struct hwc_position &frame, struct hwc_position &clipped,
                    int &zorder, int &visible) {
      ihwc.get()->getgeometry(type, index, frame, clipped, zorder, visible);
   }

   BcmSidebandBinder *get(void) {
      return ihwc.get();
   }
};

#endif /* #ifndef BCMSIDEBAND_HWCBINDER_H__*/
