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
//  * 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
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
#undef LOG_TAG
#define LOG_TAG "bomx_video_decoder_secure"

#include <cutils/log.h>

#include "bomx_video_decoder_secure.h"
#include "nexus_platform.h"
#include "OMX_IndexExt.h"
#include "OMX_VideoExt.h"


extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_Secure_Create(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    BOMX_VideoDecoder_Secure *pVideoDecoder = new BOMX_VideoDecoder_Secure(pComponentTpe, pName, pAppData, pCallbacks);
    if ( NULL == pVideoDecoder )
    {
        return BOMX_ERR_TRACE(OMX_ErrorUndefined);
    }
    else
    {
        if ( pVideoDecoder->IsValid() )
        {
            return OMX_ErrorNone;
        }
        else
        {
            delete pVideoDecoder;
            return BOMX_ERR_TRACE(OMX_ErrorUndefined);
        }
    }
}

extern "C" const char *BOMX_VideoDecoder_Secure_GetRole(unsigned roleIndex)
{
    return BOMX_VideoDecoder_GetRole(roleIndex);
}

BOMX_VideoDecoder_Secure::BOMX_VideoDecoder_Secure(
    OMX_COMPONENTTYPE *pComponentType,
    const OMX_STRING pName,
    const OMX_PTR pAppData,
    const OMX_CALLBACKTYPE *pCallbacks)
    :BOMX_VideoDecoder(pComponentType, pName, pAppData, pCallbacks)
    ,m_Sage_PlatformHandle(NULL)
{
    m_secureDecoder = true;
    ALOGV("%s", __FUNCTION__);
}

BOMX_VideoDecoder_Secure::~BOMX_VideoDecoder_Secure()
{
    ALOGV("%s", __FUNCTION__);
    Sage_Platform_Close();
}

int BOMX_VideoDecoder_Secure::Sage_Platform_Init()
{
    BSAGElib_State sage_platform_status;
    BERR_Code sage_rc = BERR_SUCCESS;

    ALOGV("%s", __FUNCTION__);
    sage_rc = SRAI_Platform_Open(BSAGE_PLATFORM_ID_COMMONDRM, &sage_platform_status,
                                 &m_Sage_PlatformHandle);
    if (sage_rc != BERR_SUCCESS)
    {
        ALOGE("%s - Error calling platform_open, ret:%d", __FUNCTION__, sage_rc);
        m_Sage_PlatformHandle = 0;
        return -1;
    }

    if(sage_platform_status == BSAGElib_State_eUninit)
    {
        m_Sagelib_Container = SRAI_Container_Allocate();
        if(m_Sagelib_Container == NULL)
        {
            ALOGE("%s - Error fetching container", __FUNCTION__);
            return -1;
        }

        sage_rc = SRAI_Platform_Init(m_Sage_PlatformHandle, m_Sagelib_Container);
        if (sage_rc != BERR_SUCCESS)
        {
            ALOGE("%s - Error calling platform init, ret:%d", __FUNCTION__,
                   sage_rc);
            SRAI_Container_Free(m_Sagelib_Container);
            m_Sagelib_Container = NULL;
            m_Sage_PlatformHandle = 0;
            return -1;
        }
    }

    ALOGV("%s, successful", __FUNCTION__);
    return 0;
}

void BOMX_VideoDecoder_Secure::Sage_Platform_Close()
{
    ALOGV("%s", __FUNCTION__);
    if (m_Sage_PlatformHandle) {
        SRAI_Platform_Close(m_Sage_PlatformHandle);
        m_Sage_PlatformHandle = NULL;
    }

// TODO: This is causing a crash, check with the security team
#if 0
    if (m_Sagelib_Container != NULL) {
        SRAI_Container_Free(m_Sagelib_Container);
        m_Sagelib_Container = NULL;
    }
#endif
}

OMX_ERRORTYPE BOMX_VideoDecoder_Secure::ConfigBufferAppend(const void *pBuffer, size_t length)
{
    NEXUS_Error err;

    BOMX_ASSERT(NULL != pBuffer);
    ALOGV("%s, buffer:%p, length:%d", __FUNCTION__, pBuffer, length);
    if ( m_configBufferSize + length > BOMX_VIDEO_CODEC_CONFIG_BUFFER_SIZE )
        return BOMX_ERR_TRACE(OMX_ErrorOverflow);

    uint8_t *dest = (uint8_t *)m_pConfigBuffer + m_configBufferSize;
    err = SecureCopy(dest, pBuffer, length);
    if (err)
    {
        ALOGE("%s: Failed to do secure copy, err:%d", __FUNCTION__, err);
        return BOMX_ERR_TRACE(OMX_ErrorUndefined);
    }

    m_configBufferSize += length;
    return OMX_ErrorNone;
}

NEXUS_Error BOMX_VideoDecoder_Secure::ExtraTransportConfig()
{
    NEXUS_SetPidChannelBypassKeyslot(m_hPidChannel, NEXUS_BypassKeySlot_eGR2R);
    return NEXUS_SUCCESS;
}

NEXUS_Error BOMX_VideoDecoder_Secure::AllocateInputBuffer(uint32_t nSize, void*& pBuffer)
{
    // Init Sage Platform if it hasn't been done yet
    if ((m_Sage_PlatformHandle == NULL) && (Sage_Platform_Init() != 0))
    {
        ALOGE("%s: Error initializing sage platform", __FUNCTION__);
        m_Sage_PlatformHandle = NULL;
        return NEXUS_UNKNOWN;
    }

    // Allocate secure buffer
    uint8_t *sec_ptr = SRAI_Memory_Allocate(nSize, SRAI_MemoryType_SagePrivate);
    if (!sec_ptr)
    {
        ALOGE("%s: Failed to allocate secure buffer", __FUNCTION__);
        return NEXUS_UNKNOWN;
    }

    ALOGV("%s, buff:%p, size:%u", __FUNCTION__, sec_ptr, nSize);
    pBuffer = sec_ptr;
    return NEXUS_SUCCESS;
}

void BOMX_VideoDecoder_Secure::FreeInputBuffer(void*& pBuffer)
{
    SRAI_Memory_Free((uint8_t*)pBuffer);
    pBuffer = NULL;
}

NEXUS_Error BOMX_VideoDecoder_Secure::AllocateNexusMemory(size_t nSize, void *& pBuffer)
{
    NEXUS_ClientConfiguration               clientConfig;
    NEXUS_MemoryAllocationSettings          memorySettings;
    NEXUS_Error errCode;

    ALOGV("%s, size:%d", __FUNCTION__, nSize);
    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    NEXUS_Memory_GetDefaultAllocationSettings(&memorySettings);
    memorySettings.heap = clientConfig.heap[1];
    errCode = NEXUS_Memory_Allocate(nSize, &memorySettings, &pBuffer);
    if ( errCode )
    {
        BOMX_ERR(("Unable to allocate nexus memory"));
        return errCode;
    }

    return NEXUS_SUCCESS;
}

static void complete(void *context, int param)
{
    BSTD_UNUSED(param);
    BKNI_SetEvent((BKNI_EventHandle)context);
}


NEXUS_Error BOMX_VideoDecoder_Secure::SecureCopy(void *pDest, const void *pSrc, size_t nSize)
{
    NEXUS_DmaHandle dma;
    NEXUS_DmaJobHandle job;
    NEXUS_DmaJobSettings jobSettings;
    BKNI_EventHandle event;
    NEXUS_Error rc;

    ALOGV("%s_, dest:%p, src:%p, size:%d", __FUNCTION__, pDest, pSrc, nSize);
    BKNI_CreateEvent(&event);
    dma = NEXUS_Dma_Open(NEXUS_ANY_ID, NULL);
    BDBG_ASSERT(dma);

    NEXUS_DmaJob_GetDefaultSettings(&jobSettings);
    jobSettings.completionCallback.callback = complete;
    jobSettings.completionCallback.context = event;
    jobSettings.bypassKeySlot = NEXUS_BypassKeySlot_eGR2R;
    job = NEXUS_DmaJob_Create(dma, &jobSettings);
    BDBG_ASSERT(job);

    NEXUS_DmaJobBlockSettings blockSettings;
    NEXUS_DmaJob_GetDefaultBlockSettings(&blockSettings);
    blockSettings.pSrcAddr = pSrc;
    blockSettings.pDestAddr = pDest;
    blockSettings.blockSize = nSize;
    blockSettings.cached = false;
    rc = NEXUS_DmaJob_ProcessBlocks(job, &blockSettings, 1);
    if (rc == NEXUS_DMA_QUEUED) {
        NEXUS_DmaJobStatus status;
        rc = BKNI_WaitForEvent(event, BKNI_INFINITE);
        BDBG_ASSERT(!rc);
        rc = NEXUS_DmaJob_GetStatus(job, &status);
        BDBG_ASSERT(!rc && (status.currentState == NEXUS_DmaJobState_eComplete));
    }
    else {
        ALOGE("%s: error in dma transfer, err:%d", __FUNCTION__, rc);
        return rc;
    }

    NEXUS_DmaJob_Destroy(job);
    NEXUS_Dma_Close(dma);
    BKNI_DestroyEvent(event);
    return NEXUS_SUCCESS;
}
