/******************************************************************************
 *    (c)2010-2016 Broadcom Corporation
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
//#define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "bomx_audio_decoder_secure"

#include <cutils/log.h>
#include <cutils/native_handle.h>

#include "bomx_audio_decoder_secure.h"
#include "nexus_platform.h"
#include "nexus_dma.h"
#include "nexus_security_client.h"
#include "bomx_secure_buff.h"
#include "OMX_IndexExt.h"

extern "C" OMX_ERRORTYPE BOMX_AudioDecoder_Secure_CreateAac(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    NEXUS_AudioCapabilities audioCaps;
    NxWrap *pNxWrap = NULL;
    BOMX_AudioDecoder_Secure *pAudioDecoderSec = NULL;

    pNxWrap = new NxWrap(pName);
    if (pNxWrap == NULL)
    {
        ALOGW("Unable to determine presence of AAC hardware!");
    }
    else
    {
        pNxWrap->join();
        NEXUS_GetAudioCapabilities(&audioCaps);
        if ( !audioCaps.dsp.codecs[NEXUS_AudioCodec_eAacAdts].decode &&
             !audioCaps.dsp.codecs[NEXUS_AudioCodec_eAacPlusAdts].decode )
        {
            ALOGW("AAC hardware support is not available");
            goto error;
        }
    }

    pAudioDecoderSec = new BOMX_AudioDecoder_Secure(
                              pComponentTpe, pName, pAppData, pCallbacks,
                              pNxWrap,
                              BOMX_AUDIO_GET_ROLE_COUNT(g_aacRole),
                              g_aacRole, BOMX_AudioDecoder_GetRoleAac);

    if ( NULL == pAudioDecoderSec )
    {
        goto error;
    }
    else
    {
        OMX_ERRORTYPE constructorError = pAudioDecoderSec->IsValid();
        if ( constructorError == OMX_ErrorNone )
        {
            return OMX_ErrorNone;
        }
        else
        {
            pNxWrap->leave();
            delete pNxWrap;
            delete pAudioDecoderSec;
            return BOMX_ERR_TRACE(constructorError);
        }
    }

error:
    if (pNxWrap)
    {
        pNxWrap->leave();
        delete pNxWrap;
    }
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

extern "C" OMX_ERRORTYPE BOMX_AudioDecoder_Secure_CreateAc3(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    NEXUS_AudioCapabilities audioCaps;
    NxWrap *pNxWrap = NULL;
    BOMX_AudioDecoder_Secure *pAudioDecoderSec = NULL;

    pNxWrap = new NxWrap(pName);
    if (pNxWrap == NULL)
    {
        ALOGW("Unable to determine presence of AAC hardware!");
    }
    else
    {
        pNxWrap->join();
        NEXUS_GetAudioCapabilities(&audioCaps);
        if ( !audioCaps.dsp.codecs[NEXUS_AudioCodec_eAc3].decode &&
             !audioCaps.dsp.codecs[NEXUS_AudioCodec_eAc3Plus].decode )
        {
            ALOGW("AC3 hardware support is not available");
            goto error;
        }
    }

    pAudioDecoderSec = new BOMX_AudioDecoder_Secure(
                              pComponentTpe, pName, pAppData, pCallbacks,
                              pNxWrap,
                              BOMX_AUDIO_GET_ROLE_COUNT(g_ac3Role),
                              g_ac3Role, BOMX_AudioDecoder_GetRoleAc3);

    if ( NULL == pAudioDecoderSec )
    {
        goto error;
    }
    else
    {
        OMX_ERRORTYPE constructorError = pAudioDecoderSec->IsValid();
        if ( constructorError == OMX_ErrorNone )
        {
            return OMX_ErrorNone;
        }
        else
        {
            pNxWrap->leave();
            delete pNxWrap;
            delete pAudioDecoderSec;
            return BOMX_ERR_TRACE(constructorError);
        }
    }

error:
    if (pNxWrap)
    {
        pNxWrap->leave();
        delete pNxWrap;
    }
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

extern "C" OMX_ERRORTYPE BOMX_AudioDecoder_Secure_CreateEAc3(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    NEXUS_AudioCapabilities audioCaps;
    NxWrap *pNxWrap = NULL;
    BOMX_AudioDecoder_Secure *pAudioDecoderSec = NULL;

    pNxWrap = new NxWrap(pName);
    if (pNxWrap == NULL)
    {
        ALOGW("Unable to determine presence of AAC hardware!");
    }
    else
    {
        pNxWrap->join();
        NEXUS_GetAudioCapabilities(&audioCaps);
        if ( !audioCaps.dsp.codecs[NEXUS_AudioCodec_eAc3].decode &&
             !audioCaps.dsp.codecs[NEXUS_AudioCodec_eAc3Plus].decode )
        {
            ALOGW("EAC3 hardware support is not available");
            goto error;
        }
    }

    pAudioDecoderSec = new BOMX_AudioDecoder_Secure(
                              pComponentTpe, pName, pAppData, pCallbacks,
                              pNxWrap,
                              BOMX_AUDIO_GET_ROLE_COUNT(g_eac3Role),
                              g_eac3Role, BOMX_AudioDecoder_GetRoleEAc3);

    if ( NULL == pAudioDecoderSec )
    {
        goto error;
    }
    else
    {
        OMX_ERRORTYPE constructorError = pAudioDecoderSec->IsValid();
        if ( constructorError == OMX_ErrorNone )
        {
            return OMX_ErrorNone;
        }
        else
        {
            pNxWrap->leave();
            delete pNxWrap;
            delete pAudioDecoderSec;
            return BOMX_ERR_TRACE(constructorError);
        }
    }

error:
    if (pNxWrap)
    {
        pNxWrap->leave();
        delete pNxWrap;
    }
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

BOMX_AudioDecoder_Secure::BOMX_AudioDecoder_Secure(
    OMX_COMPONENTTYPE *pComponentType,
    const OMX_STRING pName,
    const OMX_PTR pAppData,
    const OMX_CALLBACKTYPE *pCallbacks,
    NxWrap *pNxWrap,
    unsigned numRoles,
    const BOMX_AudioDecoderRole *pRoles,
    const char *(*pGetRole)(unsigned roleIndex))
    :BOMX_AudioDecoder(pComponentType, pName, pAppData, pCallbacks, pNxWrap, true, numRoles, pRoles, pGetRole)
{
    ALOGV("%s", __FUNCTION__);
}

BOMX_AudioDecoder_Secure::~BOMX_AudioDecoder_Secure()
{
    ALOGV("%s", __FUNCTION__);
}

NEXUS_Error BOMX_AudioDecoder_Secure::AllocateInputBuffer(uint32_t nSize, void*& pBuffer)
{
    NEXUS_MemoryBlockHandle inputBuffHandle;
    NEXUS_Error err = BOMX_AllocSecureBuffer(nSize, true, &inputBuffHandle);
    if (err != NEXUS_SUCCESS) {
        ALOGE("%s: failed to allocate secure input buffer", __FUNCTION__);
        return err;
    }

    ALOGV("%s: allocated handle:%p", __FUNCTION__, inputBuffHandle);
    if ( m_allocNativeHandle ) {
        native_handle_t *nativeHandle = native_handle_create(0,1);
        nativeHandle->data[0] = (uint32_t)(intptr_t)inputBuffHandle;
        pBuffer = nativeHandle;
    } else {
        pBuffer = (void*)inputBuffHandle;
    }
    return NEXUS_SUCCESS;
}

void BOMX_AudioDecoder_Secure::FreeInputBuffer(void*& pBuffer)
{
    NEXUS_MemoryBlockHandle inputBuffHandle;
    if ( m_allocNativeHandle ) {
        native_handle_t *nativeBuffHandle = static_cast<native_handle_t *>(pBuffer);
        inputBuffHandle = (NEXUS_MemoryBlockHandle)(intptr_t)(nativeBuffHandle->data[0]);
        BOMX_FreeSecureBuffer(inputBuffHandle);
        native_handle_delete(nativeBuffHandle);
    } else {
        inputBuffHandle = (NEXUS_MemoryBlockHandle)pBuffer;
        BOMX_FreeSecureBuffer(inputBuffHandle);
    }

    pBuffer = NULL;
}

OMX_ERRORTYPE BOMX_AudioDecoder_Secure::EmptyThisBuffer(OMX_IN OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    BOMX_SecBufferSt *secInputBuff;
    NEXUS_MemoryBlockHandle inputBuffHandle;
    native_handle_t *nativeBuffHandle;
    OMX_ERRORTYPE omx_err;
    NEXUS_Error err;

    if (( NULL == pBufferHeader || pBufferHeader->pBuffer == NULL))
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);

    if ( m_allocNativeHandle ) {
        nativeBuffHandle = static_cast<native_handle_t *>((void*)pBufferHeader->pBuffer);
        inputBuffHandle = (NEXUS_MemoryBlockHandle)(intptr_t)(nativeBuffHandle->data[0]);
    } else {
        inputBuffHandle = (NEXUS_MemoryBlockHandle) pBufferHeader->pBuffer;
    }
    err = BOMX_LockSecureBuffer(inputBuffHandle, &secInputBuff);
    if (err != NEXUS_SUCCESS) {
        ALOGE("%s: bufferHandle:%p", __FUNCTION__, inputBuffHandle);
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    if ( pBufferHeader->nFlags & OMX_BUFFERFLAG_CODECCONFIG ) {
        // The audio decoder expects the codec config to be in the clear. Verify if the
        // input buffer handle contains a copy of it in the clear buffer.
        if (secInputBuff->clearBuffOffset == 0) {
            ALOGE("%s: expected data in clear buffer", __FUNCTION__);
            BOMX_UnlockSecureBuffer(inputBuffHandle);
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        OMX_U8 *clearBuffer = (OMX_U8*)secInputBuff + secInputBuff->clearBuffOffset;
        pBufferHeader->pBuffer = clearBuffer;
        ALOGV("%s: replaced codec config buffer, ptr:%p, size:%zu",
                __FUNCTION__, clearBuffer, secInputBuff->clearBuffSize);
    } else if ( pBufferHeader->nFilledLen > 0 ) {
        // Replace the input buffer with the secure buffer
        pBufferHeader->pBuffer = (OMX_U8*)secInputBuff->pSecureBuff;
        ALOGV("%s: replaced input buffer, size:%zu", __FUNCTION__, secInputBuff->clearBuffSize);
    }

    omx_err = BOMX_AudioDecoder::EmptyThisBuffer(pBufferHeader);
    if ( m_allocNativeHandle ) {
        pBufferHeader->pBuffer = (OMX_U8*) nativeBuffHandle;
    } else {
        pBufferHeader->pBuffer = (OMX_U8*) inputBuffHandle;
    }
    ALOGV("%s: restored input buffer:%p", __FUNCTION__, pBufferHeader->pBuffer);

    BOMX_UnlockSecureBuffer(inputBuffHandle);
    return omx_err;
}


NEXUS_Error BOMX_AudioDecoder_Secure::OpenPidChannel(uint32_t pid)
{
    NEXUS_Error rc = BOMX_AudioDecoder::OpenPidChannel(pid);
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

void BOMX_AudioDecoder_Secure::ClosePidChannel()
{
    if ( m_hPidChannel )
    {
        // Set back to default (workaround for SW)
        (void)NEXUS_SetPidChannelBypassKeyslot(m_hPidChannel, NEXUS_BypassKeySlot_eG2GR);
        BOMX_AudioDecoder::ClosePidChannel();
    }
}
