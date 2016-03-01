/******************************************************************************
 *    (c)2015 Broadcom Corporation
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

#include "brcm_audio.h"
#include "nexus_ipc_client_factory.h"

StandbyMonitorThread::StandbyMonitorThread() : mMutex("BrcmAudio::StandbyMonitorThread::mMutex")
{
    int i;
    for (i = 0; i < MAX_STANDBY_MONITOR_CALLBACKS; i++) {
        mCallbacks[i] = NULL;
        mContexts[i] = NULL;
    }
    mNumCallbacks = 0;
}

StandbyMonitorThread::~StandbyMonitorThread()
{
}

int StandbyMonitorThread::RegisterCallback(b_standby_monitor_callback callback, void *context)
{
    android::status_t status;
    int i;
    android::Mutex::Autolock _l(mMutex);

    for (i = 0; i < MAX_STANDBY_MONITOR_CALLBACKS; i++) {
        if (mCallbacks[i] == NULL)
            break;
    }

    if (i >= MAX_STANDBY_MONITOR_CALLBACKS) {
        ALOGE("%s: Too many callbacks", __FUNCTION__);
        return -ENOMEM;
    }

    mCallbacks[i] = callback;
    mContexts[i] = context;
    mNumCallbacks++;
    if (mNumCallbacks == 1) {
        /* Start running monitor thread */
        status = this->run();
        if (status != android::OK){
            ALOGE("%s: error starting standby thread", __FUNCTION__);
            mCallbacks[i] = NULL;
            mContexts[i] = NULL;
            mNumCallbacks--;
            return -ENOSYS;
        }
    }
    ALOGV("%s: Callback %d/%d registered", __FUNCTION__, i, mNumCallbacks);
    return i;
}

void StandbyMonitorThread::UnregisterCallback(int id)
{
    android::Mutex::Autolock _l(mMutex);
    if ((id < 0) || (id >= MAX_STANDBY_MONITOR_CALLBACKS)) {
        ALOGE("%s: Invalid callback %d", __FUNCTION__, id);
        return;
    }

    if (mNumCallbacks == 0) {
        ALOGE("%s: No callbacks registered", __FUNCTION__);
        return;
    }

    if (mCallbacks[id] == NULL) {
        ALOGE("%s: Callback %d not registered", __FUNCTION__, id);
        return;
    }

    mCallbacks[id] = NULL;
    mContexts[id] = NULL;
    mNumCallbacks--;
    ALOGV("%s: Callback %d/%d unregistered", __FUNCTION__, id, mNumCallbacks);

    if (mNumCallbacks == 0) {
        android::status_t status = this->requestExitAndWait();
        if (status != android::OK){
            ALOGE("%s: Failed to stop standby thread!", __FUNCTION__);
        }
    }
}


/* Standby monitor thread */
bool StandbyMonitorThread::threadLoop()
{
    NEXUS_Error rc;
    unsigned mStandbyId = NxClient_RegisterAcknowledgeStandby();
    NxClient_StandbyStatus standbyStatus, prevStatus;
    int i;

    ALOGV("%s", __FUNCTION__);
    ALOGD("RegisterAcknowledgeStandby() = %d", mStandbyId);
    NxClient_GetStandbyStatus(&standbyStatus);
    prevStatus = standbyStatus;

    while (!exitPending()) {
        if ((mMutex.tryLock() == 0) && mNumCallbacks > 0) {
            rc = NxClient_GetStandbyStatus(&standbyStatus);

            if (standbyStatus.settings.mode != prevStatus.settings.mode) {
                bool ack = true;

                if (standbyStatus.settings.mode != NEXUS_PlatformStandbyMode_eOn) {
                    for (i = 0; i < MAX_STANDBY_MONITOR_CALLBACKS; i++) {
                        if (mCallbacks[i] != NULL) {
                            ack = ack && mCallbacks[i](mContexts[i]);
                            ALOGV("%s: Can suspend after %d callabck %s\n", __FUNCTION__, i, ack?"true":"false");
                        }
                    }
                }
                if (ack) {
                    ALOGD("%s: Acknowledge state %d\n", __FUNCTION__, standbyStatus.settings.mode);
                    NxClient_AcknowledgeStandby(mStandbyId);
                    prevStatus = standbyStatus;
                }
            }
            mMutex.unlock();
        }
        BKNI_Sleep(NXCLIENT_STANDBY_MONITOR_TIMEOUT_IN_MS);
    }
    NxClient_UnregisterAcknowledgeStandby(mStandbyId);
    ALOGD("UnregisterAcknoledgeStandby(%d)", mStandbyId);

    ALOGV("%s: Exiting", __FUNCTION__);
    return false;
}

