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
#ifndef BOMX_HWCBINDER_H__
#define BOMX_HWCBINDER_H__

#include "bstd.h"
#include "bdbg.h"
#include "bkni.h"

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
#if defined(BCM_FULL_TREBLE)
#include <bcm/hardware/dspsvcext/1.0/IDspSvcExt.h>
#endif

using namespace android;
#if defined(BCM_FULL_TREBLE)
using namespace android::hardware;
using namespace bcm::hardware::dspsvcext::V1_0;
#endif

typedef void (* OMX_BINDER_NTFY_CB)(void *, int, struct hwc_notification_info &);

class OmxBinder : public HwcListener
{
public:

    OmxBinder() : cb(NULL), cb_data(0) {};
    virtual ~OmxBinder() {};

    virtual void notify(int msg, struct hwc_notification_info &ntfy);

    inline void listen() {
       if (get_hwc(false) != NULL)
           get_hwc(false)->registerListener(this, HWC_BINDER_OMX);
       else
           ALOGE("%s: failed to associate %p with OmxBinder service.", __FUNCTION__, this);
    };

    inline void hangup() {
       if (get_hwc(false) != NULL)
           get_hwc(false)->unregisterListener(this);
       else
           ALOGE("%s: failed to dissociate %p from OmxBinder service.", __FUNCTION__, this);
    };

    inline void getvideo(int index, int &value) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->getVideoSurfaceId(this, index, value);
       }
    };

    inline void freevideo(int index) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->freeVideoSurfaceId(this, index);
       }
    };

    inline void getsideband(int index, int &value) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->getSidebandSurfaceId(this, index, value);
       }
    };

    inline void freesideband(int index) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->freeSidebandSurfaceId(this, index);
       }
    };

    inline void getvideogeometry(int index, struct hwc_position &frame, struct hwc_position &clipped,
                                 int &zorder, int &visible) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->getGeometry(this, HWC_BINDER_OMX, index, frame, clipped, zorder, visible);
       }
    };

    void register_notify(OMX_BINDER_NTFY_CB callback, void *data) {
       cb = callback;
       cb_data = data;
    }

private:
    OMX_BINDER_NTFY_CB cb;
    void * cb_data;
};

class OmxBinder_wrap
{
private:

   sp<OmxBinder> ihwc;

public:
   OmxBinder_wrap(void) {
      ALOGV("%s: allocated %p", __FUNCTION__, this);
      ihwc = new OmxBinder;
      ihwc.get()->listen();
   };

   virtual ~OmxBinder_wrap(void) {
      ALOGV("%s: cleared %p", __FUNCTION__, this);
      ihwc.get()->hangup();
      ihwc.clear();
   };

   void getvideo(int index, int &value) {
      ihwc.get()->getvideo(index, value);
   }

   void freevideo(int index) {
      ihwc.get()->freevideo(index);
   }

   void getsideband(int index, int &value) {
      ihwc.get()->getsideband(index, value);
   }

   void freesideband(int index) {
      ihwc.get()->freesideband(index);
   }

   void getvideogeometry(int index, struct hwc_position &frame, struct hwc_position &clipped,
                         int &zorder, int &visible) {
      ihwc.get()->getvideogeometry(index, frame, clipped, zorder, visible);
   }

   OmxBinder *get(void) {
      return ihwc.get();
   }
};

#if defined(BCM_FULL_TREBLE)
typedef void (*SB_GEOM_CB) (void *context, unsigned int x, unsigned int y,
                            unsigned int width, unsigned int height);

class SdbGeomCb : public IDspSvcExtSdbGeomCb {
public:
   SdbGeomCb(SB_GEOM_CB cb, void *cb_data): geom_cb(cb), geom_ctx(cb_data) {};
   SB_GEOM_CB geom_cb;
   void       *geom_ctx;
   Return<void> onGeom(int32_t i, const DspSvcExtGeom& geom);
};
#endif

#endif /* #ifndef BOMX_HWCBINDER_H__*/
