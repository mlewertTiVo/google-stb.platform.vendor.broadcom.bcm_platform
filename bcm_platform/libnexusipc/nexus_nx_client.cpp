/******************************************************************************
 *    (c)2010-2015 Broadcom Corporation
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
 * $brcm_Workfile: nexus_ipc_client.cpp $
 * $brcm_Revision: 10 $
 * $brcm_Date: 12/14/12 2:28p $
 * 
 * Module Description:
 * This file contains the implementation of the class that uses Nexus bipc
 * communication with the Nexus Server process (nxserver) and Binder IPC
 * communication with the Nexus Service.  A client process should NOT instantiate
 * an object of this type directly.  Instead, they should use the "getClient()"
 * method of the NexusIPCClientFactory abstract factory class.
 * On the client side, the definition of these API functions either encapsulate
 * the API into a command + param format and sends the command over binder to
 * the server side for actual execution or via Nexus bipc to nxserver.
 * 
 *****************************************************************************/

#define LOG_TAG "nxclient"
//#define LOG_NDEBUG 0

#include <utils/Log.h>
#include <string.h>
#include <cutils/atomic.h>
#include <utils/Errors.h>
#include "cutils/properties.h"

// required for binder api calls
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include "nexus_interface.h"
#include "nexusservice.h"
#include "nexus_ipc_priv.h"
#include "nexus_nx_client.h"
#include "blst_list.h"

/* Define the MAX timeout to wait after setting NxClient standby settings
   to allow standby monitor threads to complete. */
#define NXCLIENT_PM_TIMEOUT_IN_MS (5000)

/* Define the MAX number of times to monitor for the NxClient to have
   entered the desired standby mode. */
#define NXCLIENT_PM_TIMEOUT_COUNT (20)

#ifdef UINT32_C
#undef UINT32_C
#define UINT32_C(x)  (x ## U)
#endif

NexusNxClient::StandbyMonitorThread::~StandbyMonitorThread()
{
    ALOGD("%s: called", __PRETTY_FUNCTION__); 

    if (this->name != NULL) {
        free(name);
        this->name = NULL;
    }
}

android::status_t NexusNxClient::StandbyMonitorThread::run(const char* name, int32_t priority, size_t stack)
{
    android::status_t status;
    
    this->name = strdup(name);

    status = Thread::run(name, priority, stack);
    if (status == android::OK) {
        state = StandbyMonitorThread::STATE_RUNNING;
    }
    return status;
}

bool NexusNxClient::StandbyMonitorThread::threadLoop()
{
    NEXUS_Error rc;
    NxClient_StandbyStatus standbyStatus, prevStatus;

    LOGD("%s: Entering for client \"%s\"", __PRETTY_FUNCTION__, getName());

    NxClient_GetStandbyStatus(&standbyStatus);
    
    prevStatus = standbyStatus;

    while (isRunning()) {
        rc = NxClient_GetStandbyStatus(&standbyStatus);
    
        if (rc == NEXUS_SUCCESS && standbyStatus.settings.mode != prevStatus.settings.mode) {
            bool ack = true;

            if (standbyStatus.settings.mode != NEXUS_PlatformStandbyMode_eOn) {
                if (mCallback != NULL) {
                    ack = mCallback(mContext);
                }
            }
            if (ack) {
                LOGD("%s: Acknowledge state %d\n", getName(), standbyStatus.settings.mode);
                NxClient_AcknowledgeStandby(true);
                prevStatus = standbyStatus;
            }
        }
        BKNI_Sleep(NXCLIENT_STANDBY_MONITOR_TIMEOUT_IN_MS);
    }
    LOGD("%s: Exiting for client \"%s\"", __PRETTY_FUNCTION__, getName());
    return false;
}

#define NEXUS_TRUSTED_DATA_PATH "/data/misc/nexus"
NEXUS_Error NexusNxClient::clientJoin(const b_refsw_client_client_configuration *config)
{
    NEXUS_Error rc = NEXUS_SUCCESS;
    NxClient_JoinSettings joinSettings;
    NEXUS_PlatformStatus status;
    char value[PROPERTY_VALUE_MAX];
    FILE *key = NULL;

    android::Mutex::Autolock autoLock(mLock);

    NxClient_GetDefaultJoinSettings(&joinSettings);
    BKNI_Snprintf(&joinSettings.name[0], NXCLIENT_MAX_NAME, "%s", getClientName());

    sprintf(value, "%s/nx_key", NEXUS_TRUSTED_DATA_PATH);
    key = fopen(value, "r");
    joinSettings.mode = NEXUS_ClientMode_eUntrusted;
    if (key == NULL) {
       ALOGE("%s: failed to open key file \'%s\', err=%d (%s)\n", __FUNCTION__, value, errno, strerror(errno));
    } else {
       memset(value, 0, sizeof(value));
       fread(value, PROPERTY_VALUE_MAX, 1, key);
       if (strstr(value, "trusted:") == value) {
          const char *password = &value[8];
          joinSettings.mode = NEXUS_ClientMode_eVerified;
          joinSettings.certificate.length = strlen(password);
          memcpy(joinSettings.certificate.data, password, joinSettings.certificate.length);
       }
       fclose(key);
    }

    if (config == NULL || config->standbyMonitorCallback == NULL) {
        joinSettings.ignoreStandbyRequest = true;
    }

    do {
        rc = NxClient_Join(&joinSettings);
        if (rc != NEXUS_SUCCESS) {
            LOGW("%s: NxServer is not ready, waiting...", __FUNCTION__);
            usleep(NXCLIENT_SERVER_TIMEOUT_IN_MS * 1000);
        }
    } while (rc != NEXUS_SUCCESS);

    LOGI("%s: \"%s\" joins %s mode (%d)", __FUNCTION__, joinSettings.name,
         (joinSettings.mode == NEXUS_ClientMode_eVerified) ? "VERIFIED" : "UNTRUSTED",
         joinSettings.mode);

    return rc;
}

NEXUS_Error NexusNxClient::clientUninit()
{
    NEXUS_Error rc = NEXUS_SUCCESS;
    android::Mutex::Autolock autoLock(mLock);
    void *res;

    LOGI("---- %s: Calling NxClient_Uninit() for client \"%s\" ----", __PRETTY_FUNCTION__, getClientName());
    NxClient_Uninit();
    return rc;
}

NexusNxClient::NexusNxClient(const char *client_name) : NexusIPCClient(client_name)
{
    LOGV("++++++ %s: \"%s\" ++++++", __PRETTY_FUNCTION__, getClientName());
}

NexusNxClient::~NexusNxClient()
{
    LOGV("~~~~~~ %s: \"%s\" ~~~~~~", __PRETTY_FUNCTION__, getClientName());
}

NEXUS_Error NexusNxClient::standbyCheck(NEXUS_PlatformStandbyMode mode)
{
    NxClient_StandbyStatus standbyStatus;
    int count = 0;

    while (count < NXCLIENT_PM_TIMEOUT_COUNT) {
        NxClient_GetStandbyStatus(&standbyStatus);
        if (standbyStatus.settings.mode == mode && standbyStatus.standbyTransition) {
            LOGD("%s: Entered S%d", __PRETTY_FUNCTION__, mode);
            break;
        }
        else {
            usleep(NXCLIENT_PM_TIMEOUT_IN_MS * 1000 / NXCLIENT_PM_TIMEOUT_COUNT);
            count++;
        }
    }
    return (count < NXCLIENT_PM_TIMEOUT_COUNT) ? NEXUS_SUCCESS : NEXUS_TIMEOUT;
}

static const b_video_window_type videoWindowTypeConversion[] =
{
    eVideoWindowType_eMain, /* full screen capable */
    eVideoWindowType_ePip,  /* reduced size only. typically quarter screen. */
    eVideoWindowType_eNone,  /* app will do video as graphics */
    eVideoWindowType_Max
};

/* Client side implementation of the APIs that are transferred to the server process over binder */
NexusClientContext * NexusNxClient::createClientContext(const b_refsw_client_client_configuration *config)
{
    NexusClientContext *client;

    /* Call parent class to do the Binder IPC work... */
    client = NexusIPCClient::createClientContext(config);

    if (client != NULL && config != NULL && config->standbyMonitorCallback != NULL) {
        mStandbyMonitorThread = new NexusNxClient::StandbyMonitorThread(config->standbyMonitorCallback, config->standbyMonitorContext);
        mStandbyMonitorThread->run(getClientName(), ANDROID_PRIORITY_NORMAL);
    }
    return client;
}

void NexusNxClient::destroyClientContext(NexusClientContext * client)
{
    /* Cancel the standby monitor thread... */
    if (mStandbyMonitorThread != NULL && mStandbyMonitorThread->isRunning()) {
        mStandbyMonitorThread->stop();
        mStandbyMonitorThread->join();
        mStandbyMonitorThread = NULL;
    }
    /* Call parent class to do the Binder IPC work... */
    NexusIPCClient::destroyClientContext(client);
    BKNI_Memset(&this->info, 0, sizeof(this->info));
    return;
}

bool NexusNxClient::setPowerState(b_powerState pmState)
{
    NEXUS_Error rc = NEXUS_SUCCESS;
    NxClient_StandbySettings standbySettings;

    NxClient_GetDefaultStandbySettings(&standbySettings);
    standbySettings.settings.wakeupSettings.ir = 1;
    standbySettings.settings.wakeupSettings.uhf = 1;
    standbySettings.settings.wakeupSettings.transport = 1;
    standbySettings.settings.wakeupSettings.cec = isCecEnabled(0) && isCecAutoWakeupEnabled(0);
    standbySettings.settings.wakeupSettings.gpio = 1;
    standbySettings.settings.wakeupSettings.timeout = 0;

    switch (pmState)
    {
        case ePowerState_S0:
        {
            LOGD("%s: About to set power state S0...", __PRETTY_FUNCTION__);
            standbySettings.settings.mode = NEXUS_PlatformStandbyMode_eOn;
            break;
        }

        case ePowerState_S05:
        {
            LOGD("%s: About to set power state S0.5...", __PRETTY_FUNCTION__);
            standbySettings.settings.mode = NEXUS_PlatformStandbyMode_eOn;
            break;
        }

        case ePowerState_S1:
        {
            LOGD("%s: About to set power state S1...", __PRETTY_FUNCTION__);
            standbySettings.settings.mode = NEXUS_PlatformStandbyMode_eActive;
            break;
        }

        case ePowerState_S2:
        {
            LOGD("%s: About to set power state S2...", __PRETTY_FUNCTION__);
            standbySettings.settings.mode = NEXUS_PlatformStandbyMode_ePassive;
            break;
        }

        case ePowerState_S3:
        case ePowerState_S5:
        {
            LOGD("%s: About to set power state %s...", __PRETTY_FUNCTION__, NexusIPCClientBase::getPowerString(pmState));
            standbySettings.settings.mode = NEXUS_PlatformStandbyMode_eDeepSleep;
            break;
        }

        default:
        {
            LOGE("%s: invalid power state %d!", __PRETTY_FUNCTION__, pmState);
            rc = NEXUS_INVALID_PARAMETER;
            break;
        }
    }

    if (rc == NEXUS_SUCCESS) {
        NxClient_StandbyStatus standbyStatus;

        rc = NxClient_GetStandbyStatus(&standbyStatus);

        if (rc == NEXUS_SUCCESS) {
            if (standbyStatus.settings.mode != standbySettings.settings.mode) {
                rc = NxClient_SetStandbySettings(&standbySettings);
            }
            if (rc == NEXUS_SUCCESS) {
                if (pmState == ePowerState_S0) {
                    NxClient_DisplaySettings displaySettings;

                    NxClient_GetDisplaySettings(&displaySettings);
                    displaySettings.hdmiPreferences.enabled = true;
                    displaySettings.componentPreferences.enabled = true;
                    displaySettings.compositePreferences.enabled = true;
                    rc = NxClient_SetDisplaySettings(&displaySettings);
                }
                else if (pmState == ePowerState_S05) {
                    NxClient_DisplaySettings displaySettings;

                    NxClient_GetDisplaySettings(&displaySettings);
                    displaySettings.hdmiPreferences.enabled = false;
                    displaySettings.componentPreferences.enabled = false;
                    displaySettings.compositePreferences.enabled = false;
                    rc = NxClient_SetDisplaySettings(&displaySettings);
                }
            }
            else {
                LOGE("%s: NxClient_SetStandbySettings failed [rc=%d]!", __PRETTY_FUNCTION__, rc);
            }
        }
        else {
            LOGE("%s: NxClient_GetStandbyStatus failed [rc=%d]!", __PRETTY_FUNCTION__, rc);
        }
    }

    /* Now check whether Nexus Platform has entered the desired standby mode (excluding S0 and S0.5)... */
    if (rc == NEXUS_SUCCESS && (pmState != ePowerState_S0 && pmState != ePowerState_S05)) {
        rc = standbyCheck(standbySettings.settings.mode);
        if (rc != NEXUS_SUCCESS) {
            LOGE("%s: standbyCheck failed [rc=%d]!", __PRETTY_FUNCTION__, rc);
        }
    }

    return (rc == NEXUS_SUCCESS) ? true : false;
}

b_powerState NexusNxClient::getPowerState()
{
    NEXUS_Error rc;
    NxClient_StandbyStatus standbyStatus;
    b_powerState state = ePowerState_Max;

    rc = NxClient_GetStandbyStatus(&standbyStatus);

    if (rc == NEXUS_SUCCESS) {
        switch (standbyStatus.settings.mode)
        {
            case NEXUS_PlatformStandbyMode_eOn:
                state = ePowerState_S0;
                break;
            case NEXUS_PlatformStandbyMode_eActive:
                state = ePowerState_S1;
                break;
            case NEXUS_PlatformStandbyMode_ePassive:
                state = ePowerState_S2;
                break;
            case NEXUS_PlatformStandbyMode_eDeepSleep:
                state = ePowerState_S3;
                break;
            default:
                state = ePowerState_Max;
        }

        if (state != ePowerState_Max) {
            LOGD("%s: Standby Status : \n"
                 "State   : %s\n"
                 "IR      : %d\n"
                 "UHF     : %d\n"
                 "XPT     : %d\n"
                 "CEC     : %d\n"
                 "GPIO    : %d\n"
                 "Timeout : %d\n", __PRETTY_FUNCTION__,
                 NexusIPCClientBase::getPowerString(state),
                 standbyStatus.status.wakeupStatus.ir,
                 standbyStatus.status.wakeupStatus.uhf,
                 standbyStatus.status.wakeupStatus.transport,
                 standbyStatus.status.wakeupStatus.cec,
                 standbyStatus.status.wakeupStatus.gpio,
                 standbyStatus.status.wakeupStatus.timeout);
        }
    }

    return state;
}

bool NexusNxClient::getHdmiOutputStatus(uint32_t portId, b_hdmiOutputStatus *pHdmiOutputStatus)
{
    NEXUS_Error rc = NEXUS_NOT_SUPPORTED;
#if NEXUS_HAS_HDMI_OUTPUT
    if (portId < NEXUS_NUM_HDMI_OUTPUTS) {
        unsigned loops;
        NxClient_DisplayStatus status;

        memset(pHdmiOutputStatus, 0, sizeof(*pHdmiOutputStatus));

        for (loops = 0; loops < 4; loops++) {
            NEXUS_Error rc2;
            NxClient_StandbyStatus standbyStatus;

            rc2 = NxClient_GetStandbyStatus(&standbyStatus);
            if (rc2 == NEXUS_SUCCESS && standbyStatus.settings.mode == NEXUS_PlatformStandbyMode_eOn) {
                rc = NxClient_GetDisplayStatus(&status);
                if ((rc == NEXUS_SUCCESS) && status.hdmi.status.connected) {
                    break;
                }
            }
            LOGV("%s: Waiting for HDMI%d output to be connected...", __PRETTY_FUNCTION__, portId);
            usleep(250 * 1000);
        }

        if (rc == NEXUS_SUCCESS) {
            if (status.hdmi.status.connected) {
                pHdmiOutputStatus->connected            = status.hdmi.status.connected;
                pHdmiOutputStatus->rxPowered            = status.hdmi.status.rxPowered;
                pHdmiOutputStatus->hdmiDevice           = status.hdmi.status.hdmiDevice;
                pHdmiOutputStatus->videoFormat          = status.hdmi.status.videoFormat;
                pHdmiOutputStatus->preferredVideoFormat = status.hdmi.status.preferredVideoFormat;
                pHdmiOutputStatus->aspectRatio          = status.hdmi.status.aspectRatio;
                pHdmiOutputStatus->colorSpace           = status.hdmi.status.colorSpace;
                pHdmiOutputStatus->audioFormat          = status.hdmi.status.audioFormat;
                pHdmiOutputStatus->audioSamplingRate    = status.hdmi.status.audioSamplingRate;
                pHdmiOutputStatus->audioSamplingSize    = status.hdmi.status.audioSamplingSize;
                pHdmiOutputStatus->audioChannelCount    = status.hdmi.status.audioChannelCount;
                pHdmiOutputStatus->physicalAddress[0]   = status.hdmi.status.physicalAddressA << 4 | status.hdmi.status.physicalAddressB;
                pHdmiOutputStatus->physicalAddress[1]   = status.hdmi.status.physicalAddressC << 4 | status.hdmi.status.physicalAddressD;
            }
        }
        else {
            LOGE("%s: Could not get HDMI%d output status!!!", __PRETTY_FUNCTION__, portId);
        }
    }
    else
#endif
    {
        LOGE("%s: No HDMI%d output on this device!!!", __PRETTY_FUNCTION__, portId);
    }
    return (rc == NEXUS_SUCCESS);
}
