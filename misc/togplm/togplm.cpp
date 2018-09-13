/******************************************************************************
 *    (c) 2017 Broadcom Limited
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
 *
 *****************************************************************************/
#define LOG_TAG "togplm"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <log/log.h>
#include <cutils/properties.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/mman.h>
#include <linux/kdev_t.h>
#include <sched.h>
#include <sys/resource.h>
#include <cutils/sched_policy.h>

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

#include "vendor_bcm_props.h"

#define PLM_TOG_DO   "dyn.nx.tog.plm"

using namespace android;
typedef void (* HWC_APP_BINDER_NTFY_CB)(int, int, struct hwc_notification_info &);

class HwcAppBinder : public HwcListener
{
public:

    HwcAppBinder() : cb(NULL), cb_data(0) {};
    virtual ~HwcAppBinder() {};

    virtual void notify(int msg, struct hwc_notification_info &ntfy);

    inline void listen() {
        if (get_hwc(false) != NULL)
            get_hwc(false)->registerListener(this, HWC_BINDER_COM);
        else
            ALOGE("%s: failed to associate %p with HwcAppBinder service.", __FUNCTION__, this);
    };

    inline void hangup() {
        if (get_hwc(false) != NULL)
            get_hwc(false)->unregisterListener(this);
        else
            ALOGE("%s: failed to dissociate %p from HwcAppBinder service.", __FUNCTION__, this);
    };

    inline void evalplm(void) {
        if (get_hwc(false) != NULL) {
            get_hwc(false)->evalPlm(this);
        }
    };

private:
    HWC_APP_BINDER_NTFY_CB cb;
    int cb_data;
};

class HwcAppBinder_wrap
{
private:

    sp<HwcAppBinder> ihwc;

public:
    HwcAppBinder_wrap(void) {
        ALOGV("%s: allocated %p", __FUNCTION__, this);
        ihwc = new HwcAppBinder;
        ihwc.get()->listen();
    };

    virtual ~HwcAppBinder_wrap(void) {
        ALOGV("%s: cleared %p", __FUNCTION__, this);
        ihwc.get()->hangup();
        ihwc.clear();
    };

    void evalplm(void) {
        ihwc.get()->evalplm();
    }

    HwcAppBinder *get(void) {
        return ihwc.get();
    }
};

void HwcAppBinder::notify(int msg, struct hwc_notification_info &ntfy)
{
    ALOGV( "%s: notify received: msg=%u", __FUNCTION__, msg);

    if (cb)
        cb(cb_data, msg, ntfy);
}

HwcAppBinder_wrap *m_appHwcBinder;

int main(int argc, char* argv[])
{
   bool plm;

   (void)argc;
   (void)argv;

   plm = property_get_bool(BCM_DYN_HWC2_TWEAK_PLMOFF, 0);
   if (!plm) {
      property_set(BCM_DYN_HWC2_TWEAK_PLMOFF, "1");
   } else {
      property_set(BCM_DYN_HWC2_TWEAK_PLMOFF, "0");
   }

   property_set(PLM_TOG_DO, "done");

   m_appHwcBinder = new HwcAppBinder_wrap;
   if ( NULL == m_appHwcBinder ) {
      ALOGE("Unable to connect to HwcBinder");
   } else {
      m_appHwcBinder->evalplm();
      sleep(5);
      delete m_appHwcBinder;
   }

   return 0;
}
