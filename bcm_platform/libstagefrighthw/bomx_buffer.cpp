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
#define LOG_TAG "bomx_buffer"

#include "bomx_buffer.h"
#include <string.h>

BOMX_Buffer::BOMX_Buffer(
    OMX_PTR pAppPrivate,
    OMX_U32 nSizeBytes,
    OMX_U8* pBuffer)
{
    memset(&m_header, 0, sizeof(m_header));
    BOMX_STRUCT_INIT(&m_header);
    m_header.pBuffer = pBuffer;
    m_header.nAllocLen = nSizeBytes;
    m_header.pAppPrivate = pAppPrivate;
    m_header.pPlatformPrivate = static_cast <OMX_PTR> (this);
}

BOMX_Buffer::~BOMX_Buffer()
{
    // Nothing to do
}

BOMX_InputBuffer::BOMX_InputBuffer(
    OMX_PTR pAppPrivate,
    OMX_U32 nSizeBytes,
    OMX_U8* pBuffer,
    OMX_PTR pComponentPrivate) : BOMX_Buffer(pAppPrivate,nSizeBytes,pBuffer)
{
    // The spec requires input buffers to put app private in the output port private field as well.
    m_header.pOutputPortPrivate = pAppPrivate;
    m_header.pInputPortPrivate = pComponentPrivate;
}

BOMX_InputBuffer::~BOMX_InputBuffer()
{
    // Nothing to do
}

OMX_ERRORTYPE BOMX_InputBuffer::GetBuffer(void **pData, OMX_U32 *pLength)
{
    OMX_U8 *pBuffer=NULL;
    OMX_U32 length=0;

    ALOG_ASSERT(NULL != pData);
    ALOG_ASSERT(NULL != pLength);

    if ( (m_header.nOffset + m_header.nFilledLen) <= m_header.nAllocLen )
    {
        length = m_header.nFilledLen;
        pBuffer = m_header.pBuffer + m_header.nOffset;
    }

    *pData = static_cast <void *> (pBuffer);
    *pLength = length;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_InputBuffer::UpdateBuffer(OMX_U32 amountUsed)
{
    ALOG_ASSERT(amountUsed <= m_header.nFilledLen);
    m_header.nOffset += amountUsed;
    m_header.nFilledLen -= amountUsed;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_InputBuffer::Reset()
{
    return Reset(m_header.nOffset);
}

OMX_ERRORTYPE BOMX_InputBuffer::Reset(OMX_U32 nOffset)
{
    BSTD_UNUSED(nOffset);
    return OMX_ErrorNone;
}

BOMX_OutputBuffer::BOMX_OutputBuffer(
    OMX_PTR pAppPrivate,
    OMX_U32 nSizeBytes,
    OMX_U8* pBuffer,
    OMX_PTR pComponentPrivate) : BOMX_Buffer(pAppPrivate,nSizeBytes,pBuffer)
{
    // The spec requires output buffers to put app private in the input port private field as well.
    m_header.pInputPortPrivate = pAppPrivate;
    m_header.pOutputPortPrivate = pComponentPrivate;
}

BOMX_OutputBuffer::~BOMX_OutputBuffer()
{
    // Nothing to do
}

OMX_ERRORTYPE BOMX_OutputBuffer::GetBuffer(void **pData, OMX_U32 *pLength)
{
    OMX_U8 *pBuffer=NULL;
    OMX_U32 length=0;

    ALOG_ASSERT(NULL != pData);
    ALOG_ASSERT(NULL != pLength);

    if ( (m_header.nFilledLen + m_header.nOffset) < m_header.nAllocLen )
    {
        length = m_header.nAllocLen - (m_header.nFilledLen + m_header.nOffset);
        pBuffer = m_header.pBuffer + m_header.nFilledLen + m_header.nOffset;
    }

    *pData = static_cast <void *> (pBuffer);
    *pLength = length;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_OutputBuffer::UpdateBuffer(OMX_U32 amountUsed)
{
    ALOG_ASSERT((m_header.nFilledLen+amountUsed+m_header.nOffset) <= m_header.nAllocLen);
    m_header.nFilledLen += amountUsed;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_OutputBuffer::Reset()
{
    return Reset(0);
}

OMX_ERRORTYPE BOMX_OutputBuffer::Reset(OMX_U32 nOffset)
{
    ALOG_ASSERT(nOffset <= m_header.nAllocLen);
    m_header.nOffset = nOffset;
    m_header.nFilledLen = 0;
    return OMX_ErrorNone;
}
