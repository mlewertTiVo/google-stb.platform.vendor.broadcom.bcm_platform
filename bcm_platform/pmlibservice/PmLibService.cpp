/******************************************************************************
 *    (c)2011-2013 Broadcom Corporation
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
#define LOG_NDEBUG 0

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>

#undef  LOG_TAG
#define LOG_TAG "PmLibService"
#include <utils/Log.h>

#include "PmLibService.h"

#include "nexus_platform_features.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "pmlib-263x.h"

#ifdef __cplusplus
}
#endif

using namespace android;

Mutex PmLibService::mLock("PmLibService Lock");

PmLibService::PmLibService() : mPmCtx(NULL)
{
    ALOGV("%s", __func__);
    mPmCtx = brcm_pm_init();
    mState.usb   = true;
    mState.enet  = true;
    mState.moca  = true;
    mState.sata  = true;
    mState.tp1   = true;
    mState.tp2   = true;
    mState.tp3   = true;
    mState.memc1 = true;
    mState.cpu   = true;
    mState.ddr   = true;
    mState.flags = true;
}

PmLibService::~PmLibService()
{
    ALOGV("%s", __func__);
    brcm_pm_close(mPmCtx);
    mPmCtx = NULL;
}

void PmLibService::instantiate()
{
    ALOGV("%s", __func__);
    defaultServiceManager()->addService(IPmLibService::descriptor, new PmLibService());
}

status_t PmLibService::setState(pmlib_state_t *state)
{
    int rc;
    status_t ret = OK;
    struct brcm_pm_state pmState;    

    Mutex::Autolock autoLock(mLock);

    ALOGV("%s", __func__);

    rc = brcm_pm_get_status(mPmCtx, &pmState);
    if (rc) {
        ALOGE("%s: Can't get PM status [rc=%d]!!!", __func__, rc);
        ret = UNKNOWN_ERROR;
    }
    else {
#if (PMLIB_SUPPORTS_BRCMSTB_SYSFS == 1)
        pmState.usb_status = state->usb;

#if (NEXUS_NUM_MEMC > 1)
        pmState.memc1_status = state->memc1;
#else
        pmState.memc1_status = BRCM_PM_UNDEF;
#endif
        pmState.sata_status = state->sata;
        pmState.tp1_status = state->tp1;
#if (BCHP_CHIP == 7435)
        pmState.tp2_status = state->tp2;
        pmState.tp3_status = state->tp3;
#endif
        /* pmState.cpu_divisor = state->cpu?1:8; */
        pmState.ddr_timeout = state->ddr?0:64;
        
        pmState.standby_flags = state->flags;

        /* 2.6.37-2.4 Kernel has some issue while resuming from S2 standby mode. So it requires
         * some delay to added while resuming. Changing the flag to 0x4 makes sure that this
         * delay is added. Needs to be removed once the Kernel fix is available.
         */
#if (BCHP_CHIP == 7358)     
        pmState.standby_flags |=  0x4;
#endif

#endif  // PMLIB_SUPPORTS_BRCMSTB_SYSFS

        rc = brcm_pm_set_status(mPmCtx, &pmState);
        if (rc) {
            ALOGE("%s: Can't set PM status [rc=%d]!!!", __func__, rc);
            ret = UNKNOWN_ERROR;
        }
        else {
            if (state->enet) {
                if (!mState.enet) {
                    system("netcfg eth0 up");
                }
            } else {
                if (mState.enet) {
                    system("netcfg eth0 down");
                }
            }

#if MOCA_SUPPORT
            if (state->moca) {
                if (!mState.moca) {
                    system("mocactl start");
                    system("netcfg eth1 up");
                }
            } else {
                if (mState.moca) {
                    system("netcfg eth1 down");
                    system("mocactl stop");
                }
            }
#endif
            mState = *state;
        }
    }
    return ret;
}

status_t PmLibService::getState(pmlib_state_t *state)
{
    Mutex::Autolock autoLock(mLock);

    ALOGV("%s", __func__);

    *state = mState;
    return OK;
}
