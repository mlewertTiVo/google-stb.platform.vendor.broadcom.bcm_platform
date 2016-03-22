/***************************************************************************
*       (c)2007-2015 Broadcom Corporation
*
* This program is the proprietary software of Broadcom Corporation and/or its licensors,
* and may only be used, duplicated, modified or distributed pursuant to the terms and
* conditions of a separate, written license agreement executed between you and Broadcom
* (an "Authorized License").    Except as set forth in an Authorized License, Broadcom grants
* no license (express or implied), right to use, or waiver of any kind with respect to the
* Software, and Broadcom expressly reserves all rights in and to the Software and all
* intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
* HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
* NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
*
* Except as expressly set forth in the Authorized License,
*
* 1.       This program, including its structure, sequence and organization, constitutes the valuable trade
* secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
* and to use this information only in connection with your use of Broadcom integrated circuit products.
*
*    2.       TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
* AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
* WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
* THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES
* OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE,
* LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION
* OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF
* USE OR PERFORMANCE OF THE SOFTWARE.
*
* 3.       TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
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
*
* Revision History:
*
* $brcm_Log: $
*
***************************************************************************/

#include "nexus_types.h"
#include "nexus_platform.h"
#include <nexus_cec.h>


BDBG_MODULE(nexus_cec);

/**
Summary:
Open a CEC interface
**/
NEXUS_CecHandle NEXUS_Cec_Open( /* attr{destructor=NEXUS_Cec_Close} */
    unsigned index __unused,
    const NEXUS_CecSettings *pSettings __unused
)
{
    BDBG_ERR(("%s: Not implemented!", __FUNCTION__));
    return NULL;
}


/**
Summary:
Close the Cec interface
**/
void NEXUS_Cec_Close(NEXUS_CecHandle handle __unused)
{
    return;
}

void NEXUS_Cec_GetDefaultSettings(
    NEXUS_CecSettings *pSettings /* [out] */
)
{
    BKNI_Memset(pSettings, 0, sizeof(*pSettings));

    pSettings->enabled = false;
    pSettings->disableLogicalAddressPolling = false;
    pSettings->logicalAddress = 0xFF;
    pSettings->physicalAddress[0]=0xFF;
    pSettings->physicalAddress[1]=0xFF;
    pSettings->deviceType = NEXUS_CecDeviceType_eTuner;
    pSettings->cecController = NEXUS_CecController_eTx;

    return;
}


void NEXUS_Cec_GetSettings(
    NEXUS_CecHandle handle __unused,
    NEXUS_CecSettings *pSettings /* [out] */
)
{
    NEXUS_Cec_GetDefaultSettings(pSettings);
}


NEXUS_Error NEXUS_Cec_SetSettings(
    NEXUS_CecHandle handle __unused,
    const NEXUS_CecSettings *pSettings __unused
)
{
    return NEXUS_NOT_SUPPORTED;
}


NEXUS_Error NEXUS_Cec_GetStatus(
    NEXUS_CecHandle handle __unused,
    NEXUS_CecStatus *pStatus /* [out] */
)
{
    BKNI_Memset(pStatus, 0, sizeof(NEXUS_CecStatus)) ;
    return NEXUS_NOT_SUPPORTED;
}


NEXUS_Error NEXUS_Cec_ReceiveMessage(
    NEXUS_CecHandle handle __unused,
    NEXUS_CecReceivedMessage *pReceivedMessage __unused /* [out] */
)
{
    return NEXUS_NOT_SUPPORTED;
}


void NEXUS_Cec_GetDefaultMessageData(
    NEXUS_CecMessage *pMessage /* [out] */
)
{
    BKNI_Memset(pMessage, 0, sizeof(NEXUS_CecMessage)) ;
    return;
}


NEXUS_Error NEXUS_Cec_TransmitMessage(
    NEXUS_CecHandle handle __unused,
    const NEXUS_CecMessage *pMessage __unused
)
{
    return NEXUS_NOT_SUPPORTED;
}

