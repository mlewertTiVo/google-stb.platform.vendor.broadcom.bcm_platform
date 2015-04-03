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
//#define LOG_NDEBUG 0
#define LOG_TAG "bomx_utils"

#include "bomx_utils.h"
#include <stdarg.h>

#define MAKE_ERR_TABLE_ENTRY(err) {err,#err}

static struct {
    OMX_ERRORTYPE err;
    const char *pString;
} g_errorTable[] =
{
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorInsufficientResources),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorUndefined),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorInvalidComponentName),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorComponentNotFound),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorInvalidComponent),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorBadParameter),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorNotImplemented),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorUnderflow),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorOverflow),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorHardware),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorInvalidState),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorStreamCorrupt),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorPortsNotCompatible),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorResourcesLost),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorNoMore),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorVersionMismatch),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorNotReady),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorTimeout),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorSameState),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorResourcesPreempted),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorPortUnresponsiveDuringAllocation),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorPortUnresponsiveDuringDeallocation),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorPortUnresponsiveDuringStop),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorIncorrectStateTransition),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorIncorrectStateOperation),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorUnsupportedSetting),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorUnsupportedIndex),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorBadPortIndex),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorPortUnpopulated),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorComponentSuspended),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorDynamicResourcesUnavailable),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorMbErrorsInFrame),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorFormatNotDetected),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorContentPipeOpenFailed),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorContentPipeCreationFailed),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorSeperateTablesUsed),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorTunnelingUnsupported),
    MAKE_ERR_TABLE_ENTRY(OMX_ErrorNone) // Last
};

OMX_ERRORTYPE BOMX_PrintError(const char *pLogTag, const char *pFile, unsigned lineno, OMX_ERRORTYPE omxerr)
{
    const char *pErrString="unknown";
    int i;
    if ( omxerr == OMX_ErrorNone )
    {
        return OMX_ErrorNone;
    }
    else if ( omxerr == OMX_ErrorNoMore )
    {
        // Don't loudly report errorNoMore - it's a normal return code
        return OMX_ErrorNoMore;
    }
    for ( i = 0; g_errorTable[i].err != OMX_ErrorNone; i++ )
    {
        if ( g_errorTable[i].err == omxerr )
        {
            pErrString = g_errorTable[i].pString;
            break;
        }
    }
    ALOG(LOG_ERROR, pLogTag, "!!!Error %s(%#x) at %s:%d\n", pErrString, omxerr, pFile, lineno);
    return omxerr;
}

BERR_Code BOMX_PrintBError(const char *pLogTag, const char *pFile, unsigned lineno, BERR_Code berr)
{
    const char *pErrString="unknown";
    if ( berr == BERR_SUCCESS )
    {
        return BERR_SUCCESS;
    }
    ALOG(LOG_ERROR, pLogTag, "!!!Error %s(%#x) at %s:%d\n", pErrString, berr, pFile, lineno);
    return berr;
}

uint32_t BOMX_TickToPts(
    const OMX_TICKS *pTicks
    )
{
    OMX_S64 ticks;
    BOMX_ASSERT(NULL != pTicks);
    ticks = *pTicks;
    if ( ticks <= 0 )
    {
        return 0;
    }
    return (uint32_t)((ticks/1000LL)*45);
}

void BOMX_PtsToTick(
    uint32_t pts,
    OMX_TICKS *pTicks   /* [out] */
    )
{
    BOMX_ASSERT(NULL != pTicks);
    *pTicks = (((OMX_S64)pts)*1000LL)/45LL;
}

int32_t BOMX_TickDiffMs(
    const OMX_TICKS *pTime1,
    const OMX_TICKS *pTime2
    )
{
    int32_t ms1,ms2;

    BOMX_ASSERT(NULL != pTime1);
    BOMX_ASSERT(NULL != pTime2);

    ms1 = (int32_t)(*pTime1/1000LL);
    ms2 = (int32_t)(*pTime2/1000LL);

    return ms2-ms1;
}

int BOMX_FormPesHeader(
    const OMX_BUFFERHEADERTYPE *pFrame,
    const uint32_t *pPts,
    void *pBuffer,
    size_t bufferSize,
    unsigned streamId,
    const void *pCodecHeader,
    size_t codecHeaderLength,
    size_t *pHeaderLength
    )
{
    uint8_t *pHeaderBuf;
    unsigned payloadLength;
    unsigned length;
    uint32_t pts;
    bool unbounded=false;

    BOMX_ASSERT(NULL != pFrame);
    BOMX_ASSERT(NULL != pBuffer);
    BOMX_ASSERT(NULL != pHeaderLength);
    if ( codecHeaderLength > 0 )
    {
        BOMX_ASSERT(NULL != pCodecHeader);
    }

    pHeaderBuf = (uint8_t *)pBuffer;

    length=14; /* PES header size, 3 byte syncword + 1 byte stream ID + 2 bytes length + 3 byte optional header + 5 byte PTS */
    payloadLength = pFrame->nFilledLen + codecHeaderLength;
    payloadLength += length - 6; /* Payload length doesn't count the 6-byte header */
    if ( length > bufferSize )
    {
        BOMX_ERR(("Buffer not large enough for PES header"));
        return -1;
    }

    if ( payloadLength > 65535 )
    {
        unbounded=true;
    }

    if ( pPts )
    {
        pts = *pPts;
    }
    else
    {
        pts = BOMX_TickToPts(&pFrame->nTimeStamp);
    }

    /* Write PES Header */
    pHeaderBuf[0] = 0x00;
    pHeaderBuf[1] = 0x00;
    pHeaderBuf[2] = 0x01;
    pHeaderBuf[3] = (uint8_t)streamId;
    if ( unbounded )
    {
        pHeaderBuf[4] = 0;
        pHeaderBuf[5] = 0;
    }
    else
    {
        pHeaderBuf[4] = payloadLength >> 8;
        pHeaderBuf[5] = payloadLength & 0xff;
    }
    pHeaderBuf[6] = 0x81;                   /* Indicate header with 0x10 in the upper bits, original material */
    pHeaderBuf[7] = 0x80;
    pHeaderBuf[8] = 0x05;
    pHeaderBuf[9] =
        B_SET_BITS("0010", 0x02, 7, 4) |
        B_SET_BITS("PTS [32..30]", B_GET_BITS(pts,31,29), 3, 1) |
        B_SET_BIT(marker_bit, 1, 0);
    pHeaderBuf[10] = B_GET_BITS(pts,28,21); /* PTS [29..15] -> PTS [29..22] */
    pHeaderBuf[11] =
        B_SET_BITS("PTS [29..15] -> PTS [21..15]", B_GET_BITS(pts,20,14), 7, 1) |
        B_SET_BIT(marker_bit, 1, 0);
    pHeaderBuf[12] = B_GET_BITS(pts,13,6); /* PTS [14..0] -> PTS [14..7]  */
    pHeaderBuf[13] =
        B_SET_BITS("PTS [14..0] -> PTS [6..0]", B_GET_BITS(pts,5,0), 7, 2) |
        B_SET_BIT("PTS[0]", 0, 1) |
        B_SET_BIT(marker_bit, 1, 0);

    if ( codecHeaderLength > 0 )
    {
        BKNI_Memcpy(pHeaderBuf+14, pCodecHeader, codecHeaderLength);
    }

    *pHeaderLength = length + codecHeaderLength;
    return 0;
}

const char * BOMX_StateName(OMX_STATETYPE state)
{
    switch ( state )
    {
    case OMX_StateInvalid:
        return "Invalid";
    case OMX_StateLoaded:
        return "Loaded";
    case OMX_StateIdle:
        return "Idle";
    case OMX_StateExecuting:
        return "Executing";
    case OMX_StatePause:
        return "Pause";
    case OMX_StateWaitForResources:
        return "Wait For Resources";
    default:
        return "Unknown";
    }
}
