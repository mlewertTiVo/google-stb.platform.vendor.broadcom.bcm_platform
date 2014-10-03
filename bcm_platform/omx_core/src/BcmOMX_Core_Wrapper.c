/******************************************************************************
 *    (c)2010-2012 Broadcom Corporation
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
 * ANY LIMITED REMEDY. */


/* #include "OMX_RegLib.h" */
#include "OMX_Component.h"
#include "OMX_Core.h"
#include "OMX_ComponentRegistry.h"

#include "OMX_Core_Wrapper.h"
#include <utils/Log.h>
#undef LOG_TAG
#define LOG_TAG "BCM_OMX_CORE_WRAPPER"
#define UNUSED __attribute__((unused))

OMX_ERRORTYPE BcmComponentTable_EventHandler(
    OMX_IN OMX_HANDLETYPE hComponent UNUSED,
    OMX_IN OMX_PTR pAppData UNUSED,
    OMX_IN OMX_EVENTTYPE eEvent UNUSED,
    OMX_IN OMX_U32 nData1 UNUSED,
    OMX_IN OMX_U32 nData2 UNUSED,
    OMX_IN OMX_PTR pEventData UNUSED)
{
    return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE BcmComponentTable_EmptyBufferDone(
    OMX_OUT OMX_HANDLETYPE hComponent UNUSED,
    OMX_OUT OMX_PTR pAppData UNUSED,
    OMX_OUT OMX_BUFFERHEADERTYPE * pBuffer UNUSED)
{
    return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE BcmComponentTable_FillBufferDone(
    OMX_OUT OMX_HANDLETYPE hComponent UNUSED,
    OMX_OUT OMX_PTR pAppData UNUSED,
    OMX_OUT OMX_BUFFERHEADERTYPE * pBuffer UNUSED)
{
    return OMX_ErrorNotImplemented;
}


OMX_API OMX_ERRORTYPE BcmOMX_Init(void)
{
    LOGV("BcmOMX_Init\n");

    return OMX_Init();
}

OMX_API OMX_ERRORTYPE BcmOMX_Deinit(void)
{
    LOGV("BcmOMX_Deinit\n");

    return OMX_Deinit();
}

OMX_API OMX_ERRORTYPE BcmOMX_ComponentNameEnum(OMX_OUT OMX_STRING
    cComponentName, OMX_IN OMX_U32 nNameLength, OMX_IN OMX_U32 nIndex)
{

    LOGE("BcmOMX_ComponentNameEnum\n");

    return OMX_ComponentNameEnum(cComponentName, nNameLength, nIndex);
}

OMX_API OMX_ERRORTYPE BcmOMX_GetHandle(OMX_OUT OMX_HANDLETYPE * pHandle,
    OMX_IN OMX_STRING cComponentName,
    OMX_IN OMX_PTR pAppData, OMX_IN OMX_CALLBACKTYPE * pCallBacks)
{

    LOGE("BcmOMX_GetHandle\n");

    return OMX_GetHandle(pHandle, cComponentName, pAppData, pCallBacks);
}

OMX_API OMX_ERRORTYPE BcmOMX_FreeHandle(OMX_IN OMX_HANDLETYPE hComponent)
{
    LOGV("BcmOMX_FreeHandle\n");

    return OMX_FreeHandle(hComponent);
}

OMX_API OMX_ERRORTYPE BcmOMX_GetComponentsOfRole(OMX_IN OMX_STRING role,
    OMX_INOUT OMX_U32 * pNumComps, OMX_INOUT OMX_U8 ** compNames)
{

    LOGV("BcmOMX_GetComponentsOfRole\n");

    return OMX_GetComponentsOfRole(role, pNumComps, compNames);
}

OMX_API OMX_ERRORTYPE BcmOMX_GetRolesOfComponent(OMX_IN OMX_STRING compName,
    OMX_INOUT OMX_U32 * pNumRoles, OMX_OUT OMX_U8 ** roles)
{

    LOGV("BcmOMX_GetRolesOfComponent\n");

    return OMX_GetRolesOfComponent(compName, pNumRoles, roles);
}

OMX_API OMX_ERRORTYPE BcmOMX_SetupTunnel(OMX_IN OMX_HANDLETYPE hOutput,
    OMX_IN OMX_U32 nPortOutput,
    OMX_IN OMX_HANDLETYPE hInput, OMX_IN OMX_U32 nPortInput)
{

    LOGV("BcmOMX_SetupTunnel\n");

    return OMX_SetupTunnel(hOutput, nPortOutput, hInput, nPortInput);
}

OMX_API OMX_ERRORTYPE BcmOMX_GetContentPipe(
    OMX_OUT OMX_HANDLETYPE * hPipe UNUSED,
    OMX_IN OMX_STRING szURI UNUSED)
{

    LOGV("BcmOMX_GetContentPipe\n");

    //return OMX_GetContentPipe(
    //      hPipe,
    //      szURI);
    return (OMX_ERRORTYPE)0;
}

