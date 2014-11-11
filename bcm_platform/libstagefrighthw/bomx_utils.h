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
 *****************************************************************************/
#ifndef BOMX_UTILS_H__
#define BOMX_UTILS_H__

#include "bstd.h"
#include "bdbg.h"
#include "bkni.h"
#include "OMX_Core.h"
#include "OMX_Types.h"
#include "biobits.h"

#include <cutils/log.h>

extern "C" {

#define BOMX_STRUCT_INIT(pStructure) do { (pStructure)->nSize = sizeof(*(pStructure)); (pStructure)->nVersion.s.nVersionMajor =1; (pStructure)->nVersion.s.nVersionMinor =0; (pStructure)->nVersion.s.nRevision =0; (pStructure)->nVersion.s.nStep =0; } while (0)
#define BOMX_STRUCT_VALIDATE(pStructure) do { if ( NULL == (pStructure) || (pStructure)->nSize != sizeof(*(pStructure)) ) return BOMX_ERR_TRACE(OMX_ErrorBadParameter);  if ( (pStructure)->nVersion.s.nVersionMajor !=1 || (pStructure)->nVersion.s.nVersionMinor !=0 || (pStructure)->nVersion.s.nRevision != 0 || (pStructure)->nVersion.s.nStep != 0 ) return BOMX_ERR_TRACE(OMX_ErrorVersionMismatch); } while (0)

#define BOMX_BERR_TRACE(berr) (((berr) == BERR_SUCCESS)?BERR_SUCCESS:BOMX_PrintBError(LOG_TAG, __FILE__,__LINE__,(berr)))
#define BOMX_ERR_TRACE(omxerr) (((omxerr) == OMX_ErrorNone)?OMX_ErrorNone:BOMX_PrintError(LOG_TAG, __FILE__,__LINE__,(omxerr)))
#define BOMX_GET_NXCLIENT_SLOT(arr,slot) {for ( (slot)=0; (slot)<NXCLIENT_MAX_IDS && (arr)[(slot)].id != 0; (slot)++ );}

#define BOMX_ERR(x) ALOGE x
#define BOMX_WRN(x) ALOGW x
#define BOMX_MSG(x) ALOGV x
#define BOMX_ASSERT(cond) ALOG_ASSERT(cond, "%s:%u", __FILE__, __LINE__);

BERR_Code BOMX_PrintBError(const char *pLogTag, const char *pFile, unsigned lineno, BERR_Code berr);
OMX_ERRORTYPE BOMX_PrintError(const char *pLogTag, const char *pFile, unsigned lineno, OMX_ERRORTYPE omxerr);

uint32_t BOMX_TickToPts(
    const OMX_TICKS *pTicks
    );

void BOMX_PtsToTick(
    uint32_t pts,
    OMX_TICKS *pTicks   /* [out] */
    );

int32_t BOMX_TickDiffMs(
    const OMX_TICKS *pTime1,
    const OMX_TICKS *pTime2
    );

int BOMX_FormPesHeader(
    const OMX_BUFFERHEADERTYPE *pFrame,
    void *pBuffer,
    size_t bufferSize,
    unsigned streamId,
    const void *pCodecHeader,
    size_t codecHeaderLength,
    size_t *pHeaderLength
    );

const char * BOMX_StateName(OMX_STATETYPE state);

}


#endif /* #ifndef BOMX_UTILS_H__*/
