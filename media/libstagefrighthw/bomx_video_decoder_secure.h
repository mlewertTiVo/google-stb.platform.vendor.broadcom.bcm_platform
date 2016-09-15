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
 *****************************************************************************/
#ifndef BOMX_VIDEO_DECODER_SECURE_H__
#define BOMX_VIDEO_DECODER_SECURE_H__

#include "bomx_video_decoder.h"

extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_Secure_Create(OMX_COMPONENTTYPE *, OMX_IN OMX_STRING,
                                                         OMX_IN OMX_PTR, OMX_IN OMX_CALLBACKTYPE*);
extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_Secure_CreateTunnel(OMX_COMPONENTTYPE *, OMX_IN OMX_STRING,
                                                         OMX_IN OMX_PTR, OMX_IN OMX_CALLBACKTYPE*);
extern "C" const char *BOMX_VideoDecoder_Secure_GetRole(unsigned roleIndex);

extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_Secure_CreateVp9(OMX_COMPONENTTYPE *, OMX_IN OMX_STRING,
                                                         OMX_IN OMX_PTR, OMX_IN OMX_CALLBACKTYPE*);
extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_Secure_CreateVp9Tunnel(OMX_COMPONENTTYPE *, OMX_IN OMX_STRING,
                                                         OMX_IN OMX_PTR, OMX_IN OMX_CALLBACKTYPE*);

class BOMX_VideoDecoder_Secure : public BOMX_VideoDecoder
{
public:
    BOMX_VideoDecoder_Secure(
        OMX_COMPONENTTYPE *pComponentType,
        const OMX_STRING pName,
        const OMX_PTR pAppData,
        const OMX_CALLBACKTYPE *pCallbacks,
        NexusIPCClientBase *pIpcClient=NULL,
        NexusClientContext *pNexusClient=NULL,
        bool tunnel=false,
        unsigned numRoles=0,
        const BOMX_VideoDecoderRole *pRoles=NULL,
        const char *(*pGetRole)(unsigned roleIndex)=NULL);


    virtual ~BOMX_VideoDecoder_Secure();

protected:
    virtual OMX_ERRORTYPE EmptyThisBuffer( OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);
    virtual NEXUS_Error AllocateInputBuffer(uint32_t nSize, void*& pBuffer);
    virtual void FreeInputBuffer(void*& pBuffer);
    virtual NEXUS_Error AllocateConfigBuffer(uint32_t nSize, void*& pBuffer);
    virtual void FreeConfigBuffer(void*& pBuffer);
    virtual OMX_ERRORTYPE ConfigBufferAppend(const void *pBuffer, size_t length);
    virtual NEXUS_Error OpenPidChannel(uint32_t pid);
    virtual void ClosePidChannel();

private:
    NEXUS_Error SecureCopy(void *pDest, const void *pSrc, size_t nSize);
};

#endif //BOMX_VIDEO_DECODER_SECURE_H__
