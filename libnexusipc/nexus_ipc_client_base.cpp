/******************************************************************************
 *    (c)2010-2013 Broadcom Corporation
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
 * $brcm_Workfile: nexus_ipc_client_base.cpp $
 * $brcm_Revision: $
 * $brcm_Date: $
 * 
 * Module Description:
 * This file contains the abstract base class implementation for all classes
 * that require some form of IPC client<-->server communication.
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/libnexusipc/nexus_ipc_client_base.cpp $
 *
 *****************************************************************************/
  
#define LOG_TAG "NexusIPCClientBase"

#include "nexus_ipc_client_base.h"

/* Static definitions */
android::Mutex NexusIPCClientBase::mLock("NexusIPCClientBase Lock");
unsigned  NexusIPCClientBase::mJoinRefCount(0);

NexusIPCClientBase::NexusIPCClientBase(const char *clientName)
{
    clientPid = getpid();

    BKNI_Memset(this->clientName.string, 0, sizeof(this->clientName));

    if (clientName == NULL) {
        BKNI_Snprintf(this->clientName.string, CLIENT_MAX_NAME, "app_%d", clientPid);
    } else {
        BKNI_Snprintf(this->clientName.string, CLIENT_MAX_NAME, "%s", clientName);
    }
}

const char *NexusIPCClientBase::getClientName()
{
    return const_cast<const char *>(&clientName.string[0]);
}

unsigned NexusIPCClientBase::getClientPid()
{
    return clientPid;
}

const char *NexusIPCClientBase::getPowerString(b_powerState pmState)
{
    switch (pmState) {
        case ePowerState_S0:
            return "S0";
        case ePowerState_S05:
            return "S0.5";
        case ePowerState_S1:
            return "S1";
        case ePowerState_S2:
            return "S2";
        case ePowerState_S3:
            return "S3";
        case ePowerState_S4:
            return "S4";
        case ePowerState_S5:
            return "S5";
        default:
            ALOGE("%s: Invalid power state %d!!!", __FUNCTION__, pmState);
            return "";
    }
}

