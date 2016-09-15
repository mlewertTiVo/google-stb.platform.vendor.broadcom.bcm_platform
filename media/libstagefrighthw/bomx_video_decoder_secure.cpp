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
 *****************************************************************************/
//#define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "bomx_video_decoder_secure"

#include <cutils/log.h>

#include "bomx_video_decoder_secure.h"
#include "nexus_platform.h"
#include "nexus_dma.h"
#include "nexus_security_client.h"
#include "bomx_secure_buff.h"
#include "OMX_IndexExt.h"
#include "OMX_VideoExt.h"
#include "sage_srai.h"

OMX_ERRORTYPE BOMX_VideoDecoder_Secure_CreateCommon(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks,
    bool tunnelMode)
{
    BOMX_VideoDecoder_Secure *pVideoDecoder = new BOMX_VideoDecoder_Secure(pComponentTpe, pName, pAppData, pCallbacks, NULL, NULL, tunnelMode);
    if ( NULL == pVideoDecoder )
    {
        return BOMX_ERR_TRACE(OMX_ErrorUndefined);
    }
    else
    {
        OMX_ERRORTYPE constructorError = pVideoDecoder->IsValid();
        if ( constructorError == OMX_ErrorNone )
        {
            return OMX_ErrorNone;
        }
        else
        {
            delete pVideoDecoder;
            return BOMX_ERR_TRACE(constructorError);
        }
    }
}

extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_Secure_Create(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    return BOMX_VideoDecoder_Secure_CreateCommon(pComponentTpe, pName, pAppData, pCallbacks, false);
}

extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_Secure_CreateTunnel(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    return BOMX_VideoDecoder_Secure_CreateCommon(pComponentTpe, pName, pAppData, pCallbacks, true);
}

extern "C" const char *BOMX_VideoDecoder_Secure_GetRole(unsigned roleIndex)
{
    return BOMX_VideoDecoder_GetRole(roleIndex);
}

OMX_ERRORTYPE BOMX_VideoDecoder_Secure_CreateVp9Common(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks,
    bool tunnelMode)
{
    static const BOMX_VideoDecoderRole vp9Role = {"video_decoder.vp9", OMX_VIDEO_CodingVP9};
    BOMX_VideoDecoder *pVideoDecoder;
    unsigned i;
    bool vp9Supported = false;
    NEXUS_VideoDecoderCapabilities caps;
    NexusIPCClientBase *pIpcClient = NULL;
    NexusClientContext *pNexusClient = NULL;

    pIpcClient = NexusIPCClientFactory::getClient(pName);
    if (pIpcClient)
    {
        pNexusClient = pIpcClient->createClientContext();
    }
    if (pNexusClient == NULL)
    {
        ALOGW("Unable to determine presence of VP9 hardware!");
    }
    else
    {
        // Check if the platform supports VP9
        NEXUS_GetVideoDecoderCapabilities(&caps);
        for ( i = 0; i < caps.numVideoDecoders; i++ )
        {
            if ( caps.memory[i].supportedCodecs[NEXUS_VideoCodec_eVp9] )
            {
                vp9Supported = true;
                break;
            }
        }
    }

    if ( !vp9Supported )
    {
        ALOGW("VP9 hardware support is not available");
        goto error;
    }

    // VP9 can be disabled by this property
    if ( property_get_int32(B_PROPERTY_TRIM_VP9, 0) )
    {
        ALOGW("VP9 hardware support is available but disabled (ro.nx.trim.vp9=1)");
        goto error;
    }

    pVideoDecoder = new BOMX_VideoDecoder_Secure(pComponentTpe, pName, pAppData, pCallbacks,
                                                 pIpcClient, pNexusClient,
                                                 tunnelMode, 1, &vp9Role, BOMX_VideoDecoder_GetRoleVp9);
    if ( NULL == pVideoDecoder )
    {
        goto error;
    }
    else
    {
        OMX_ERRORTYPE constructorError = pVideoDecoder->IsValid();
        if ( constructorError == OMX_ErrorNone )
        {
            return OMX_ErrorNone;
        }
        else
        {
            delete pVideoDecoder;
            return BOMX_ERR_TRACE(constructorError);
        }
    }

error:
    if (pIpcClient)
    {
        if (pNexusClient)
        {
            pIpcClient->destroyClientContext(pNexusClient);
        }
        delete pIpcClient;
    }
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_Secure_CreateVp9Tunnel(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    return BOMX_VideoDecoder_Secure_CreateVp9Common(pComponentTpe, pName, pAppData, pCallbacks, true);
}

extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_Secure_CreateVp9(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    return BOMX_VideoDecoder_Secure_CreateVp9Common(pComponentTpe, pName, pAppData, pCallbacks, false);
}

BOMX_VideoDecoder_Secure::BOMX_VideoDecoder_Secure(
    OMX_COMPONENTTYPE *pComponentType,
    const OMX_STRING pName,
    const OMX_PTR pAppData,
    const OMX_CALLBACKTYPE *pCallbacks,
    NexusIPCClientBase *pIpcClient,
    NexusClientContext *pNexusClient,
    bool tunnel,
    unsigned numRoles,
    const BOMX_VideoDecoderRole *pRoles,
    const char *(*pGetRole)(unsigned roleIndex))
    :BOMX_VideoDecoder(pComponentType, pName, pAppData, pCallbacks, pIpcClient, pNexusClient, true, tunnel, numRoles, pRoles, pGetRole)
{
    ALOGV("%s", __FUNCTION__);
}

BOMX_VideoDecoder_Secure::~BOMX_VideoDecoder_Secure()
{
    ALOGV("%s", __FUNCTION__);
}

OMX_ERRORTYPE BOMX_VideoDecoder_Secure::ConfigBufferAppend(const void *pBuffer, size_t length)
{
    NEXUS_Error err;

    ALOG_ASSERT(NULL != pBuffer);
    ALOGV("%s, buffer:%p, length:%zu", __FUNCTION__, pBuffer, length);
    if ( m_configBufferSize + length > BOMX_VIDEO_CODEC_CONFIG_BUFFER_SIZE )
    {
        ALOGE("Config buffer not big enough! Size: %zu", m_configBufferSize + length);
        return BOMX_ERR_TRACE(OMX_ErrorOverflow);
    }

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

NEXUS_Error BOMX_VideoDecoder_Secure::AllocateInputBuffer(uint32_t nSize, void*& pBuffer)
{
    NEXUS_MemoryBlockHandle inputBuffHandle;
    NEXUS_Error err = BOMX_AllocSecureBuffer(nSize, false, &inputBuffHandle);
    if (err != NEXUS_SUCCESS) {
        ALOGE("%s: failed to allocate secure input buffer", __FUNCTION__);
        return err;
    }

    ALOGV("%s: allocated handle:%p", __FUNCTION__, inputBuffHandle);
    pBuffer = (void*)inputBuffHandle;
    return NEXUS_SUCCESS;
}

void BOMX_VideoDecoder_Secure::FreeInputBuffer(void*& pBuffer)
{
    NEXUS_MemoryBlockHandle inputBuffHandle;
    inputBuffHandle = (NEXUS_MemoryBlockHandle)pBuffer;
    BOMX_FreeSecureBuffer(inputBuffHandle);
    pBuffer = NULL;
}

NEXUS_Error BOMX_VideoDecoder_Secure::AllocateConfigBuffer(uint32_t nSize, void*& pBuffer)
{
    uint8_t *configBuffer;

    configBuffer = SRAI_Memory_Allocate(nSize, SRAI_MemoryType_SagePrivate);
    if (configBuffer == NULL) {
        ALOGE("%s: failed to allocate codec config buffer", __FUNCTION__);
        return NEXUS_OUT_OF_DEVICE_MEMORY;
    }
    pBuffer = configBuffer;
    return NEXUS_SUCCESS;
}

void BOMX_VideoDecoder_Secure::FreeConfigBuffer(void*& pBuffer)
{
    SRAI_Memory_Free((uint8_t*)pBuffer);
    pBuffer = NULL;
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

    ALOGV("%s_, dest:%p, src:%p, size:%zu", __FUNCTION__, pDest, pSrc, nSize);
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

NEXUS_Error BOMX_VideoDecoder_Secure::OpenPidChannel(uint32_t pid)
{
    NEXUS_Error rc = BOMX_VideoDecoder::OpenPidChannel(pid);
    if ( NEXUS_SUCCESS == rc )
    {
        // Setup to read from restricted memory to save codec config data w/dma
        rc = NEXUS_SetPidChannelBypassKeyslot(m_hPidChannel, NEXUS_BypassKeySlot_eGR2R);
        if ( rc )
        {
            ALOGE("Unable to set bypass key slot");
            ClosePidChannel();
            return BOMX_BERR_TRACE(rc);
        }
    }
    return rc;
}

void BOMX_VideoDecoder_Secure::ClosePidChannel()
{
    if ( m_hPidChannel )
    {
        // Set back to default (workaround for SW)
        (void)NEXUS_SetPidChannelBypassKeyslot(m_hPidChannel, NEXUS_BypassKeySlot_eG2GR);
        BOMX_VideoDecoder::ClosePidChannel();
    }
}


OMX_ERRORTYPE BOMX_VideoDecoder_Secure::EmptyThisBuffer(OMX_IN OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    BOMX_SecBufferSt *secInputBuff;
    NEXUS_MemoryBlockHandle inputBuffHandle;
    OMX_ERRORTYPE omx_err;
    NEXUS_Error err;

    if (( NULL == pBufferHeader || pBufferHeader->pBuffer == NULL))
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);

    inputBuffHandle = (NEXUS_MemoryBlockHandle) pBufferHeader->pBuffer;
    err = BOMX_LockSecureBuffer(inputBuffHandle, &secInputBuff);
    if (err != NEXUS_SUCCESS) {
        ALOGE("%s: bufferHandle:%p", __FUNCTION__, inputBuffHandle);
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    pBufferHeader->pBuffer = (OMX_U8*)secInputBuff->pSecureBuff;
    omx_err = BOMX_VideoDecoder::EmptyThisBuffer(pBufferHeader);
    BOMX_UnlockSecureBuffer(inputBuffHandle);
    pBufferHeader->pBuffer = (OMX_U8*) inputBuffHandle;
    return omx_err;
}
