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
#ifndef BOMX_PORT_H__
#define BOMX_PORT_H__

#include "bomx_utils.h"
#include "OMX_Component.h"
#include "bomx_buffer.h"
#include "blst_queue.h"
#include <string.h>

struct BOMX_BufferNode
{
    BLST_Q_ENTRY(BOMX_BufferNode) allocNode;
    BLST_Q_ENTRY(BOMX_BufferNode) queueNode;
    BOMX_Buffer *pBuffer;
};

class BOMX_Component;

// Prototype for a buffer comparison function.  Returns true
// if the comparison matches and false otherwise.
typedef bool (*BOMX_BufferCompareFunction)(BOMX_Buffer *pBuffer, void *pData);

class BOMX_Port
{
public:
    BOMX_Port(
        unsigned index,
        OMX_DIRTYPE dir,
        unsigned bufferCountMin,
        unsigned bufferSize,
        bool contiguous,
        unsigned alignment);

    virtual ~BOMX_Port();

    OMX_PORTDOMAINTYPE GetDomain() { return m_definition.eDomain; }

    const OMX_PARAM_PORTDEFINITIONTYPE *GetDefinition() { return &m_definition; }
    void GetDefinition(OMX_PARAM_PORTDEFINITIONTYPE *pDef) { *pDef = m_definition; }
    OMX_ERRORTYPE SetDefinition(const OMX_PARAM_PORTDEFINITIONTYPE *pConfig);

    OMX_ERRORTYPE Enable();
    OMX_ERRORTYPE Disable();

    bool IsEmpty() { return m_numBuffers == 0 ? true : false; }
    bool IsPopulated() { return m_definition.bPopulated == OMX_TRUE ? true : false; }
    bool IsEnabled() { return m_definition.bEnabled == OMX_TRUE ? true : false; }
    bool IsDisabled() { return m_definition.bEnabled == OMX_TRUE ? false : true; }

    OMX_ERRORTYPE AddBuffer(
        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
        OMX_IN OMX_PTR pAppPrivate,
        OMX_IN OMX_U32 nSizeBytes,
        OMX_IN OMX_U8* pBuffer,
        OMX_IN OMX_PTR pComponentPrivate,
        bool componentAllocated);

    OMX_ERRORTYPE RemoveBuffer(
        OMX_BUFFERHEADERTYPE *pBuffer
        );

    OMX_ERRORTYPE QueueBuffer(OMX_BUFFERHEADERTYPE* pBufferHdr);
    BOMX_Buffer *GetBuffer();
    BOMX_Buffer *GetNextBuffer(BOMX_Buffer *pBuffer);
    void BufferComplete(BOMX_Buffer *pBuffer);
    // Search through queued buffers for a match using the provided function
    BOMX_Buffer *FindBuffer(BOMX_BufferCompareFunction pCompareFunc, void *pData);
    BOMX_Buffer *GetPortBuffer();

    unsigned QueueDepth() { return m_queueDepth; }

    OMX_BUFFERSUPPLIERTYPE GetSupplier() { return m_supplier; }
    void SetSupplier(OMX_BUFFERSUPPLIERTYPE type) { m_supplier = type; }

    OMX_ERRORTYPE SetTunnel(BOMX_Component *pComp, BOMX_Port *pPort);
    BOMX_Component *GetTunnelComponent() { return m_pTunnelComponent; }

    bool IsTunneled() { return m_pTunnelPort != NULL ? true : false; }

    OMX_DIRTYPE GetDir() { return m_definition.eDir; }
    OMX_U32 GetIndex() { return m_definition.nPortIndex; }

protected:
    OMX_PARAM_PORTDEFINITIONTYPE m_definition;
    OMX_BUFFERSUPPLIERTYPE m_supplier;

    BLST_Q_HEAD(BOMX_BufferQueue, BOMX_BufferNode) m_bufferQueue;
    BLST_Q_HEAD(BOMX_BufferList, BOMX_BufferNode) m_bufferList;

    bool m_componentAllocated;
    unsigned m_numBuffers;
    unsigned m_queueDepth;

    unsigned m_numPortFormats;

    BOMX_BufferNode *FindBufferNode(BOMX_Buffer *pBuffer);
    bool IsBufferQueued(BOMX_Buffer *pBuffer);

    BOMX_Component *m_pTunnelComponent;
    BOMX_Port *m_pTunnelPort;

private:
};

class BOMX_AudioPort : public BOMX_Port
{
public:
    BOMX_AudioPort(
        unsigned index,
        OMX_DIRTYPE dir,
        unsigned bufferCountMin,
        unsigned bufferSize,
        bool contiguous,
        unsigned alignment,
        OMX_AUDIO_PORTDEFINITIONTYPE *pDefs,
        const OMX_AUDIO_PARAM_PORTFORMATTYPE *pPortFormats,
        unsigned numPortFormats);

    virtual ~BOMX_AudioPort();

    const OMX_AUDIO_PARAM_PORTFORMATTYPE *GetPortFormat(unsigned index) { return (index >= m_numPortFormats) ? NULL : &m_pPortFormats[index]; }
    OMX_ERRORTYPE GetPortFormat(unsigned index, OMX_AUDIO_PARAM_PORTFORMATTYPE *pFormat) { if (index >= m_numPortFormats) { return OMX_ErrorNoMore; } else { memcpy(pFormat, &m_pPortFormats[index], sizeof(*pFormat)); return OMX_ErrorNone; } }
    OMX_ERRORTYPE SetPortFormat(
        const OMX_AUDIO_PARAM_PORTFORMATTYPE *pFormat,
        const OMX_AUDIO_PORTDEFINITIONTYPE *pDefaults);

protected:
    const OMX_AUDIO_PARAM_PORTFORMATTYPE *m_pPortFormats;

private:
};

class BOMX_VideoPort : public BOMX_Port
{
public:
    BOMX_VideoPort(
        unsigned index,
        OMX_DIRTYPE dir,
        unsigned bufferCountMin,
        unsigned bufferSize,
        bool contiguous,
        unsigned alignment,
        OMX_VIDEO_PORTDEFINITIONTYPE *pDefs,
        const OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormats,
        unsigned numPortFormats);

    virtual ~BOMX_VideoPort();

    const OMX_VIDEO_PARAM_PORTFORMATTYPE *GetPortFormat(unsigned index);
    OMX_ERRORTYPE GetPortFormat(unsigned index, OMX_VIDEO_PARAM_PORTFORMATTYPE *pFormat) { if (index >= m_numPortFormats) { return OMX_ErrorNoMore; } else { memcpy(pFormat, &m_pPortFormats[index], sizeof(*pFormat)); return OMX_ErrorNone; } }
    OMX_ERRORTYPE SetPortFormat(
        const OMX_VIDEO_PARAM_PORTFORMATTYPE *pFormat,
        const OMX_VIDEO_PORTDEFINITIONTYPE *pDefaults);
    bool SetPortFormat(unsigned index, OMX_COLOR_FORMATTYPE colorFormat);

protected:
    OMX_VIDEO_PARAM_PORTFORMATTYPE *m_pPortFormats;

private:
};

class BOMX_ImagePort : public BOMX_Port
{
public:
    BOMX_ImagePort(
        unsigned index,
        OMX_DIRTYPE dir,
        unsigned bufferCountMin,
        unsigned bufferSize,
        bool contiguous,
        unsigned alignment,
        OMX_IMAGE_PORTDEFINITIONTYPE *pDefs,
        const OMX_IMAGE_PARAM_PORTFORMATTYPE *pPortFormats,
        unsigned numPortFormats);

    virtual ~BOMX_ImagePort();

    const OMX_IMAGE_PARAM_PORTFORMATTYPE *GetPortFormat(unsigned index);
    OMX_ERRORTYPE GetPortFormat(unsigned index, OMX_IMAGE_PARAM_PORTFORMATTYPE *pFormat) { if (index >= m_numPortFormats) { return OMX_ErrorNoMore; } else { memcpy(pFormat, &m_pPortFormats[index], sizeof(*pFormat)); return OMX_ErrorNone; } }
    OMX_ERRORTYPE SetPortFormat(
        const OMX_IMAGE_PARAM_PORTFORMATTYPE *pFormat,
        const OMX_IMAGE_PORTDEFINITIONTYPE *pDefaults);

protected:
    const OMX_IMAGE_PARAM_PORTFORMATTYPE *m_pPortFormats;

private:
};

class BOMX_OtherPort : public BOMX_Port
{
public:
    BOMX_OtherPort(
        unsigned index,
        OMX_DIRTYPE dir,
        unsigned bufferCountMin,
        unsigned bufferSize,
        bool contiguous,
        unsigned alignment,
        OMX_OTHER_PORTDEFINITIONTYPE *pDefs,
        const OMX_OTHER_PARAM_PORTFORMATTYPE *pPortFormats,
        unsigned numPortFormats);

    virtual ~BOMX_OtherPort();

    const OMX_OTHER_PARAM_PORTFORMATTYPE *GetPortFormat(unsigned index);
    OMX_ERRORTYPE GetPortFormat(unsigned index, OMX_OTHER_PARAM_PORTFORMATTYPE *pFormat) { if (index >= m_numPortFormats) { return OMX_ErrorNoMore; } else { memcpy(pFormat, &m_pPortFormats[index], sizeof(*pFormat)); return OMX_ErrorNone; } }
    OMX_ERRORTYPE SetPortFormat(
        const OMX_OTHER_PARAM_PORTFORMATTYPE *pFormat,
        const OMX_OTHER_PORTDEFINITIONTYPE *pDefaults);

protected:
    const OMX_OTHER_PARAM_PORTFORMATTYPE *m_pPortFormats;

private:
};

#endif //BOMX_PORT_H__
