/******************************************************************************
 *    (c)2011-2015 Broadcom Corporation
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
//#define LOG_NDEBUG 0

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>

#undef  LOG_TAG
#define LOG_TAG "PmLibService"
#include <utils/Log.h>

#include "legacy/PmLibService.h"

#include "nexus_platform_features.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "pmlib.h"

#ifdef __cplusplus
}
#endif

using namespace android;

Mutex PmLibService::mLock("PmLibService Lock");

// String constant definitions...
const String8 PmLibService::IP_CTRL_CMD(IP_CTRL_CMD_FOR_SDK);
const String8 PmLibService::IP_IF0("eth0");
const String8 PmLibService::IP_IF1("eth1");
const String8 PmLibService::IP_IF_ENABLE("up");
const String8 PmLibService::IP_IF_DISABLE("down");
const String8 PmLibService::MOCA_CTRL_CMD("mocactl");
const String8 PmLibService::MOCA_ENABLE("start");
const String8 PmLibService::MOCA_DISABLE("stop");

PmLibService::PmLibService() : mPmCtx(NULL)
{
    struct brcm_pm_state pmState;

    ALOGV("%s", __func__);
    mPmCtx = brcm_pm_init();
    mState.enet_en          = true;
    mState.moca_en          = true;
    mState.sata_en          = true;
    mState.tp1_en           = true;
    mState.tp2_en           = true;
    mState.tp3_en           = true;
    mState.cpufreq_scale_en = false;
    mState.ddr_pm_en        = false;

    if (brcm_pm_get_status(mPmCtx, &pmState)) {
        ALOGE("%s: Can't get PM status!!!", __func__);
    }
    else {
        mState.sata_en   =  pmState.sata_status;
        mState.tp1_en    =  pmState.tp1_status;
        mState.tp2_en    =  pmState.tp2_status;
        mState.tp3_en    =  pmState.tp3_status;
        mState.ddr_pm_en = (pmState.srpd_status > 0);
    }
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
    status_t ret = OK;
    struct brcm_pm_state pmState;    

    Mutex::Autolock autoLock(mLock);

    ALOGV("%s", __func__);

    if (brcm_pm_get_status(mPmCtx, &pmState)) {
        ALOGE("%s: Can't get PM status!!!", __func__);
        ret = UNKNOWN_ERROR;
    }
    else {

        if (pmState.sata_status != BRCM_PM_UNDEF) {
            pmState.sata_status = state->sata_en;
            ALOGV("%s: %s SATA", __FUNCTION__, state->sata_en ? "Enable" : "Disable");
        }
        if (pmState.tp1_status != BRCM_PM_UNDEF) {
            pmState.tp1_status = state->tp1_en;
            ALOGV("%s: %s CPU1", __FUNCTION__, state->tp1_en ? "Enable" : "Disable");
        }
        if (pmState.tp2_status != BRCM_PM_UNDEF) {
            pmState.tp2_status = state->tp2_en;
            ALOGV("%s: %s CPU2", __FUNCTION__, state->tp2_en ? "Enable" : "Disable");
        }
        if (pmState.tp3_status != BRCM_PM_UNDEF) {
            pmState.tp3_status = state->tp3_en;
            ALOGV("%s: %s CPU3", __FUNCTION__, state->tp3_en ? "Enable" : "Disable");
        }
        if (pmState.srpd_status != BRCM_PM_UNDEF) {
            pmState.srpd_status = state->ddr_pm_en ? 64 : 0;
            ALOGV("%s: %s DDR self-refresh", __FUNCTION__, state->ddr_pm_en ? "Enable" : "Disable");
        }

        // TODO add support for CPU frequency scaling

        if (brcm_pm_set_status(mPmCtx, &pmState)) {
            ALOGE("%s: Can't set PM status!!!", __func__);
            ret = UNKNOWN_ERROR;
        }
        else {
            if (state->enet_en) {
                if (!mState.enet_en) {
                    ALOGD("%s: Bringing up ethernet...", __FUNCTION__);
                    system(String8(IP_CTRL_CMD + " " + IP_IF0 + " " + IP_IF_ENABLE).string());
                }
            } else {
                if (mState.enet_en) {
                    ALOGD("%s: Shutting down ethernet...", __FUNCTION__);
                    system(String8(IP_CTRL_CMD + " " + IP_IF0 + " " + IP_IF_DISABLE).string());
                }
            }

#if MOCA_SUPPORT
            if (state->moca_en) {
                if (!mState.moca_en) {
                    ALOGD("%s: Bringing up MoCA...", __FUNCTION__);
                    system(String8(MOCA_CTRL_CMD + " " + MOCA_ENABLE).string());
                    system(String8(IP_CTRL_CMD + " " + IP_IF1 + " " + IP_IF_ENABLE).string());
                }
            } else {
                if (mState.moca_en) {
                    ALOGD("%s: Shutting down MoCA...", __FUNCTION__);
                    system(String8(IP_CTRL_CMD + " " + IP_IF1 + " " + IP_IF_DISABLE).string());
                    system(String8(MOCA_CTRL_CMD + " " + MOCA_DISABLE).string());
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
