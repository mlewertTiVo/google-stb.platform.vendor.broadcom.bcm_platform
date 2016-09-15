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
    ALOG_ASSERT(NULL != pTicks);
    ticks = *pTicks;
    if ( ticks <= 0 )
    {
        return 0;
    }
    return (uint32_t)((ticks/1000LL)*45LL);
}

void BOMX_PtsToTick(
    uint32_t pts,
    OMX_TICKS *pTicks   /* [out] */
    )
{
    ALOG_ASSERT(NULL != pTicks);
    *pTicks = (((OMX_S64)pts)*1000LL)/45LL;
}

int32_t BOMX_TickDiffMs(
    const OMX_TICKS *pTime1,
    const OMX_TICKS *pTime2
    )
{
    int32_t ms1,ms2;

    ALOG_ASSERT(NULL != pTime1);
    ALOG_ASSERT(NULL != pTime2);

    ms1 = (int32_t)(*pTime1/1000LL);
    ms2 = (int32_t)(*pTime2/1000LL);

    return ms2-ms1;
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

/* Please keep in sync with the enum from refsw
   nexus/modules/core/include/nexus_video_types.h */
float BOMX_NexusFramerateValue(NEXUS_VideoFrameRate framerate)
{
    switch ( framerate )
    {
    case NEXUS_VideoFrameRate_eUnknown: break;
    case NEXUS_VideoFrameRate_e23_976: return 23.976;
    case NEXUS_VideoFrameRate_e24:     return 24.0;
    case NEXUS_VideoFrameRate_e25:     return 25.0;
    case NEXUS_VideoFrameRate_e29_97:  return 29.97;
    case NEXUS_VideoFrameRate_e30:     return 30.0;
    case NEXUS_VideoFrameRate_e50:     return 50.0;
    case NEXUS_VideoFrameRate_e59_94:  return 59.94;
    case NEXUS_VideoFrameRate_e60:     return 60.0;
    case NEXUS_VideoFrameRate_e14_985: return 14.985;
    case NEXUS_VideoFrameRate_e7_493:  return 7.493;
    case NEXUS_VideoFrameRate_e10:     return 10.0;
    case NEXUS_VideoFrameRate_e15:     return 15.0;
    case NEXUS_VideoFrameRate_e20:     return 20.0;
    case NEXUS_VideoFrameRate_e12_5:   return 12.5;
    case NEXUS_VideoFrameRate_e100:    return 100.0;
    case NEXUS_VideoFrameRate_e119_88: return 119.88;
    case NEXUS_VideoFrameRate_e120:    return 120.0;
    case NEXUS_VideoFrameRate_e19_98:  return 19.98;
    case NEXUS_VideoFrameRate_e7_5:    return 7.5;
    case NEXUS_VideoFrameRate_e12:     return 12;
    case NEXUS_VideoFrameRate_e11_988: return 11.988;
    case NEXUS_VideoFrameRate_e9_99:   return 9.99;
    default:
        BDBG_CASSERT(NEXUS_VideoFrameRate_eMax == 23);
        ALOGE("Unhandled NEXUS_VideoFrameRate (%d).", framerate);
        break;
    }
    return 0.0;
};
