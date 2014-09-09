/******************************************************************************
 *    (c)2010-2014 Broadcom Corporation
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
  
#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "NexusNxCecService"
//#define LOG_NDEBUG 0

#include "nexusnxcecservice.h"
#include "nxclient.h"

bool NexusNxService::CecServiceManager::getCecPhysicalAddress(b_cecPhysicalAddress *pCecPhyAddr)
{
    bool success = false;
#if NEXUS_HAS_HDMI_OUTPUT

#ifdef ANDROID_SUPPORTS_NXCLIENT_HDMI_STATUS
    NEXUS_Error rc = NEXUS_NOT_SUPPORTED;
    NxClient_DisplayStatus status;
    unsigned loops;

    if (cecId < NEXUS_NUM_HDMI_OUTPUTS) {
        for (loops = 0; loops < 4; loops++) {
            ALOGV("%s: Waiting for HDMI output %d to be connected...", __FUNCTION__, cecId);
            rc = NxClient_GetDisplayStatus(&status);
            if ((rc == NEXUS_SUCCESS) && status.hdmi.status.connected) {
                break;
            }
            usleep(250 * 1000);
        }

        if (rc == NEXUS_SUCCESS && status.hdmi.status.connected) {
            ALOGV("%s: HDMI output %d is connected.", __FUNCTION__, cecId);
            success = true;
            pCecPhyAddr->addressA = status.hdmi.status.physicalAddressA;
            pCecPhyAddr->addressB = status.hdmi.status.physicalAddressB;
            pCecPhyAddr->addressC = status.hdmi.status.physicalAddressC;
            pCecPhyAddr->addressD = status.hdmi.status.physicalAddressD;
        }
        else {
            ALOGW("%s: HDMI output %d not connected.", __FUNCTION__, cecId);
        }
    }
    else {
        LOGE("%s: HDMI output %d does not exist on this platform!!!", __FUNCTION__, cecId);
    }
#else
#warning Reference software does not support obtaining HDMI output status in NxClient mode
    success = true;
#endif
#endif
    return success;
}

NexusNxService::CecServiceManager::~CecServiceManager()
{
    ALOGV("%s: for CEC%d called", __PRETTY_FUNCTION__, cecId); 
}

