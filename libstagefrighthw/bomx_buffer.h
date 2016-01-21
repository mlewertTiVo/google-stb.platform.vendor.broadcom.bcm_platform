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
#ifndef BOMX_BUFFER_H__
#define BOMX_BUFFER_H__

#include "bomx_utils.h"
#include "OMX_Core.h"

#define BOMX_BUFFERHEADER_TO_BUFFER(pHeader) (static_cast <BOMX_Buffer *> ((pHeader)->pPlatformPrivate))

class BOMX_Buffer
{
public:
    BOMX_Buffer(
        OMX_PTR pAppPrivate,
        OMX_U32 nSizeBytes,
        OMX_U8* pBuffer);

    virtual ~BOMX_Buffer();

    OMX_BUFFERHEADERTYPE *GetHeader() { return &m_header; }
    OMX_TICKS *GetTimestamp() { return &m_header.nTimeStamp; }
    OMX_U32 GetFlags() { return m_header.nFlags; }

    virtual OMX_ERRORTYPE GetBuffer(void **pData, OMX_U32 *pLength) = 0;
    virtual OMX_ERRORTYPE UpdateBuffer(OMX_U32 amountUsed) = 0;

    virtual OMX_ERRORTYPE Reset()=0;
    virtual OMX_ERRORTYPE Reset(OMX_U32 nOffset)=0;

    virtual OMX_PTR GetComponentPrivate() = 0;
    virtual OMX_DIRTYPE GetDir() = 0;

protected:
    OMX_BUFFERHEADERTYPE m_header;

private:
};

class BOMX_InputBuffer : public BOMX_Buffer
{
public:
    BOMX_InputBuffer(
        OMX_PTR pAppPrivate,
        OMX_U32 nSizeBytes,
        OMX_U8* pBuffer,
        OMX_PTR pComponentPrivate);

    virtual ~BOMX_InputBuffer();

    virtual OMX_ERRORTYPE GetBuffer(void **pData, OMX_U32 *pLength);
    virtual OMX_ERRORTYPE UpdateBuffer(OMX_U32 amountUsed);

    virtual OMX_ERRORTYPE Reset();
    virtual OMX_ERRORTYPE Reset(OMX_U32 nOffset);

    OMX_PTR GetComponentPrivate() {return m_header.pInputPortPrivate;}
    OMX_DIRTYPE GetDir() {return OMX_DirInput;}
protected:

private:
};

class BOMX_OutputBuffer : public BOMX_Buffer
{
public:
    BOMX_OutputBuffer(
        OMX_PTR pAppPrivate,
        OMX_U32 nSizeBytes,
        OMX_U8* pBuffer,
        OMX_PTR pComponentPrivate);

    virtual ~BOMX_OutputBuffer();

    virtual OMX_ERRORTYPE GetBuffer(void **pData, OMX_U32 *pLength);
    virtual OMX_ERRORTYPE UpdateBuffer(OMX_U32 amountUsed);

    virtual OMX_ERRORTYPE Reset();
    virtual OMX_ERRORTYPE Reset(OMX_U32 nOffset);

    OMX_PTR GetComponentPrivate() {return m_header.pOutputPortPrivate;}
    OMX_DIRTYPE GetDir() {return OMX_DirOutput;}
protected:

private:
};

#endif //BOMX_BUFFER_H__
