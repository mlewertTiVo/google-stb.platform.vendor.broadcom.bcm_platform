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
 * $brcm_Workfile: $
 * $brcm_Revision: $
 * $brcm_Date: $
 * 
 * Module Description:
 * This file contains functions to make Nexus IPC calls to the Nexus service
 * in order to set the power state and retrieve the power state.
 * 
 * Revision History:
 * 
 * $brcm_Log: $
 *****************************************************************************/
#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "nexus_power"

#include <utils/Log.h>
#include <string.h>
#include <utils/Errors.h>

#include "nexus_ipc_client_factory.h"
#include "nexus_power.h"

static b_powerState NexusPowerStateToPowerState(NexusPowerState pmState)
{
    b_powerState state;

    switch (pmState)
    {
        case eNexusPowerState_S0:
            state = ePowerState_S0;
            break;

        case eNexusPowerState_S1:
            state = ePowerState_S1;
            break;
            
        case eNexusPowerState_S2:
            state = ePowerState_S2;
            break;
    
        case eNexusPowerState_S3:
            state = ePowerState_S3;
            break;

        case eNexusPowerState_S5:
            state = ePowerState_S5;
            break;

        default:
            state = ePowerState_S0;
            LOGW("%s: invalid Nexus Power State %d!", __FUNCTION__, pmState);
            break;
    }
    return state;
}

static NexusPowerState PowerStateToNexusPowerState(b_powerState state)
{
    NexusPowerState pmState;

    switch (state)
    {
        case ePowerState_S0:
            pmState = eNexusPowerState_S0;
            break;

        case ePowerState_S1:
            pmState = eNexusPowerState_S1;
            break;
            
        case ePowerState_S2:
            pmState = eNexusPowerState_S2;
            break;
    
        case ePowerState_S3:
            pmState = eNexusPowerState_S3;
            break;

        case ePowerState_S5:
            pmState = eNexusPowerState_S5;
            break;

        default:
            pmState = eNexusPowerState_S0;
            LOGW("%s: invalid Power State %d!", __FUNCTION__, state);
            break;
    }
    return pmState;
}

/* Android-L sets up a property to define the device type and hence
   the logical address of the device.  */
int NexusPower_getDeviceType()
{
    char value[PROPERTY_VALUE_MAX];
    int type = -1;

    if (property_get("ro.hdmi.device_type", value, NULL)) {
        type = atoi(value);
    }
    return type;
}

NEXUS_Error
NexusPower_SetPowerState(NexusPowerState pmState)
{
    NEXUS_Error rc = NEXUS_SUCCESS;
    NexusIPCClientBase *pIpcClient = NexusIPCClientFactory::getClient("Android-Power");
    b_powerState   state;
    const uint32_t cecId = 0;   // Hardcoded CEC id to 0
    int deviceType = NexusPower_getDeviceType();

    state = NexusPowerStateToPowerState(pmState);

    LOGD("%s: About to set power state %d...", __FUNCTION__, state);

    /* If powering out of standby, then ensure platform is brought up first before CEC,
       otherwise if powering down, then we must send CEC commands first. */
    if (pmState == eNexusPowerState_S0) {
        if (pIpcClient->setPowerState(state) != true) {
            LOGE("%s: Could not set PowerState %d!", __FUNCTION__, state);
            rc = NEXUS_UNKNOWN;
        }
        else if (deviceType == -1 && pIpcClient->isCecEnabled(cecId) && pIpcClient->setCecPowerState(cecId, state) != true) {
            LOGE("%s: Could not set CEC%d PowerState %d!", __FUNCTION__, cecId, state);
        }
    }
    else {
        if (deviceType == -1 && pIpcClient->isCecEnabled(cecId) && pIpcClient->setCecPowerState(cecId, state) != true) {
            LOGE("%s: Could not set CEC%d PowerState %d!", __FUNCTION__, cecId, state);
        }

        if (pIpcClient->setPowerState(state) != true) {
            LOGE("%s: Could not set PowerState %d!", __FUNCTION__, state);
            rc = NEXUS_UNKNOWN;
        }
    }

    delete pIpcClient;

    return rc;
}

NEXUS_Error
NexusPower_GetPowerState(NexusPowerState *pPmState)
{
     b_powerState state;

     if (pPmState == NULL) {
         LOGE("%s: invalid parameter \"pPmState\"!", __FUNCTION__);
         return NEXUS_INVALID_PARAMETER;
     }

    NexusIPCClientBase *pIpcClient = NexusIPCClientFactory::getClient("Android-Power");
    LOGD("%s: About to get power state...", __FUNCTION__);
    state = pIpcClient->getPowerState();
    LOGD("%s: Power state = %d", __FUNCTION__, state);
    *pPmState = PowerStateToNexusPowerState(state);
    delete pIpcClient;

    return NEXUS_SUCCESS;
}
