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
#define LOG_TAG "bomx_port"

#include "bomx_port.h"
#include <stdlib.h>
#include <string.h>

BOMX_AudioPort::BOMX_AudioPort(
        unsigned index,
        OMX_DIRTYPE dir,
        unsigned bufferCountMin,
        unsigned bufferSize,
        bool contiguous,
        unsigned alignment,
        OMX_AUDIO_PORTDEFINITIONTYPE *pAudioDefs,
        const OMX_AUDIO_PARAM_PORTFORMATTYPE *pPortFormats,
        unsigned numPortFormats) : BOMX_Port(index, dir, bufferCountMin, bufferSize, contiguous, alignment)
{
    m_definition.eDomain = OMX_PortDomainAudio;
    m_definition.format.audio = *pAudioDefs;
    m_numPortFormats = numPortFormats;
    if ( numPortFormats > 0 )
    {
        OMX_AUDIO_PARAM_PORTFORMATTYPE *pPortFormatCopy;
        pPortFormatCopy = new OMX_AUDIO_PARAM_PORTFORMATTYPE[numPortFormats];
        ALOG_ASSERT(NULL != pPortFormatCopy);
        memcpy(pPortFormatCopy, pPortFormats, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE)*numPortFormats);
        m_pPortFormats = pPortFormatCopy;
    }
    else
    {
        m_pPortFormats = NULL;  // I think this is really invalid...
    }
}

BOMX_AudioPort::~BOMX_AudioPort()
{
    if ( m_pPortFormats )
    {
        delete m_pPortFormats;
    }
}

OMX_ERRORTYPE BOMX_AudioPort::SetPortFormat(
    const OMX_AUDIO_PARAM_PORTFORMATTYPE *pFormat,
    const OMX_AUDIO_PORTDEFINITIONTYPE *pDefaults)
{
    unsigned i;

    // Make sure this is a supported port format

    for ( i = 0; i < m_numPortFormats; i++ )
    {
        if ( m_pPortFormats[i].eEncoding == pFormat->eEncoding )
        {
            break;
        }
    }
    if ( i >= m_numPortFormats )
    {
        ALOGW("Invalid port format");
        return BOMX_ERR_TRACE(OMX_ErrorUnsupportedSetting);
    }

    m_definition.format.audio = *pDefaults;
    return OMX_ErrorNone;
}

BOMX_VideoPort::BOMX_VideoPort(
        unsigned index,
        OMX_DIRTYPE dir,
        unsigned bufferCountMin,
        unsigned bufferSize,
        bool contiguous,
        unsigned alignment,
        OMX_VIDEO_PORTDEFINITIONTYPE *pVideoDefs,
        const OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormats,
        unsigned numPortFormats) : BOMX_Port(index, dir, bufferCountMin, bufferSize, contiguous, alignment)
{
    m_definition.eDomain = OMX_PortDomainVideo;
    m_definition.format.video = *pVideoDefs;
    m_numPortFormats = numPortFormats;
    if ( numPortFormats > 0 )
    {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormatCopy;
        pPortFormatCopy = new OMX_VIDEO_PARAM_PORTFORMATTYPE[numPortFormats];
        ALOG_ASSERT(NULL != pPortFormatCopy);
        memcpy(pPortFormatCopy, pPortFormats, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE)*numPortFormats);
        m_pPortFormats = pPortFormatCopy;
    }
    else
    {
        m_pPortFormats = NULL;  // I think this is really invalid...
    }
}

BOMX_VideoPort::~BOMX_VideoPort()
{
    if ( m_pPortFormats )
    {
        delete m_pPortFormats;
    }
}

OMX_ERRORTYPE BOMX_VideoPort::SetPortFormat(
    const OMX_VIDEO_PARAM_PORTFORMATTYPE *pFormat,
    const OMX_VIDEO_PORTDEFINITIONTYPE *pDefaults)
{
    unsigned i;

    // Make sure this is a supported port format

    for ( i = 0; i < m_numPortFormats; i++ )
    {
        if ( m_pPortFormats[i].eCompressionFormat == pFormat->eCompressionFormat &&
             (m_pPortFormats[i].xFramerate == 0 || m_pPortFormats[i].xFramerate == pFormat->xFramerate) )
        {
            if ( pFormat->eCompressionFormat == OMX_VIDEO_CodingUnused &&
                 m_pPortFormats[i].eColorFormat != pFormat->eColorFormat )
            {
                // Color format mismatch for uncompressed video
                continue;
            }
            break;
        }
    }
    if ( i >= m_numPortFormats )
    {
        ALOGW("Invalid port format");
        return BOMX_ERR_TRACE(OMX_ErrorUnsupportedSetting);
    }

    m_definition.format.video = *pDefaults;
    return OMX_ErrorNone;
}

bool BOMX_VideoPort::SetPortFormat(unsigned index, OMX_COLOR_FORMATTYPE colorFormat)
{
    if ( index >= m_numPortFormats )
    {
        ALOGW("Invalid port format index %u", index);
        return false;
    }

    m_pPortFormats[index].eColorFormat = colorFormat;
    return true;
}

BOMX_ImagePort::BOMX_ImagePort(
        unsigned index,
        OMX_DIRTYPE dir,
        unsigned bufferCountMin,
        unsigned bufferSize,
        bool contiguous,
        unsigned alignment,
        OMX_IMAGE_PORTDEFINITIONTYPE *pImageDefs,
        const OMX_IMAGE_PARAM_PORTFORMATTYPE *pPortFormats,
        unsigned numPortFormats) : BOMX_Port(index, dir, bufferCountMin, bufferSize, contiguous, alignment)
{
    m_definition.eDomain = OMX_PortDomainImage;
    m_definition.format.image = *pImageDefs;
    m_numPortFormats = numPortFormats;
    if ( numPortFormats > 0 )
    {
        OMX_IMAGE_PARAM_PORTFORMATTYPE *pPortFormatCopy;
        pPortFormatCopy = new OMX_IMAGE_PARAM_PORTFORMATTYPE[numPortFormats];
        ALOG_ASSERT(NULL != pPortFormatCopy);
        memcpy(pPortFormatCopy, pPortFormats, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE)*numPortFormats);
        m_pPortFormats = pPortFormatCopy;
    }
    else
    {
        m_pPortFormats = NULL;  // I think this is really invalid...
    }
}

BOMX_ImagePort::~BOMX_ImagePort()
{
    if ( m_pPortFormats )
    {
        delete m_pPortFormats;
    }
}

OMX_ERRORTYPE BOMX_ImagePort::SetPortFormat(
    const OMX_IMAGE_PARAM_PORTFORMATTYPE *pFormat,
    const OMX_IMAGE_PORTDEFINITIONTYPE *pDefaults)
{
    unsigned i;

    // Make sure this is a supported port format

    for ( i = 0; i < m_numPortFormats; i++ )
    {
        if ( m_pPortFormats[i].eCompressionFormat == pFormat->eCompressionFormat )
        {
            if ( pFormat->eCompressionFormat == OMX_IMAGE_CodingUnused &&
                 m_pPortFormats[i].eColorFormat != pFormat->eColorFormat )
            {
                // Color format mismatch for uncompressed image
                continue;
            }
            break;
        }
    }
    if ( i >= m_numPortFormats )
    {
        ALOGW("Invalid port format");
        return BOMX_ERR_TRACE(OMX_ErrorUnsupportedSetting);
    }

    m_definition.format.image = *pDefaults;
    return OMX_ErrorNone;
}

BOMX_OtherPort::BOMX_OtherPort(
        unsigned index,
        OMX_DIRTYPE dir,
        unsigned bufferCountMin,
        unsigned bufferSize,
        bool contiguous,
        unsigned alignment,
        OMX_OTHER_PORTDEFINITIONTYPE *pOtherDefs,
        const OMX_OTHER_PARAM_PORTFORMATTYPE *pPortFormats,
        unsigned numPortFormats) : BOMX_Port(index, dir, bufferCountMin, bufferSize, contiguous, alignment)
{
    m_definition.eDomain = OMX_PortDomainOther;
    m_definition.format.other = *pOtherDefs;
    m_numPortFormats = numPortFormats;
    if ( numPortFormats > 0 )
    {
        OMX_OTHER_PARAM_PORTFORMATTYPE *pPortFormatCopy;
        pPortFormatCopy = new OMX_OTHER_PARAM_PORTFORMATTYPE[numPortFormats];
        ALOG_ASSERT(NULL != pPortFormatCopy);
        memcpy(pPortFormatCopy, pPortFormats, sizeof(OMX_OTHER_PARAM_PORTFORMATTYPE)*numPortFormats);
        m_pPortFormats = pPortFormatCopy;
    }
    else
    {
        m_pPortFormats = NULL;  // I think this is really invalid...
    }
}

BOMX_OtherPort::~BOMX_OtherPort()
{
    if ( m_pPortFormats )
    {
        delete m_pPortFormats;
    }
}

OMX_ERRORTYPE BOMX_OtherPort::SetPortFormat(
    const OMX_OTHER_PARAM_PORTFORMATTYPE *pFormat,
    const OMX_OTHER_PORTDEFINITIONTYPE *pDefaults)
{
    unsigned i;

    // Make sure this is a supported port format

    for ( i = 0; i < m_numPortFormats; i++ )
    {
        if ( m_pPortFormats[i].eFormat == pFormat->eFormat )
        {
            break;
        }
    }
    if ( i >= m_numPortFormats )
    {
        ALOGW("Invalid port format");
        return BOMX_ERR_TRACE(OMX_ErrorUnsupportedSetting);
    }

    m_definition.format.other = *pDefaults;
    return OMX_ErrorNone;
}

BOMX_Port::BOMX_Port(
    unsigned index,
    OMX_DIRTYPE dir,
    unsigned bufferCountMin,
    unsigned bufferSize,
    bool contiguous,
    unsigned alignment)
{
    m_queueDepth = 0;
    m_numBuffers = 0;
    m_pTunnelComponent = NULL;
    m_pTunnelPort = NULL;
    BLST_Q_INIT(&m_bufferQueue);
    BLST_Q_INIT(&m_bufferList);
    BOMX_STRUCT_INIT(&m_definition);
    m_definition.nPortIndex = index;
    m_definition.eDir = dir;
    m_definition.nBufferCountActual = bufferCountMin;
    m_definition.nBufferCountMin = bufferCountMin;
    m_definition.nBufferSize = bufferSize;
    m_definition.bEnabled = OMX_TRUE;
    m_definition.bPopulated = OMX_FALSE;
    m_definition.bBuffersContiguous = contiguous ? OMX_TRUE : OMX_FALSE;
    m_definition.nBufferAlignment = alignment;
    m_supplier = OMX_BufferSupplyUnspecified;
    m_hwTex = BOMX_PortBufferHwTexUsage_eUnknown;
}

BOMX_Port::~BOMX_Port()
{
    // Nothing to do
}

OMX_ERRORTYPE BOMX_Port::SetDefinition(const OMX_PARAM_PORTDEFINITIONTYPE *pConfig)
{
    ALOG_ASSERT(NULL != pConfig);
    BOMX_STRUCT_VALIDATE(pConfig);

    OMX_PARAM_PORTDEFINITIONTYPE newConfig = *pConfig;

    /**************************************************************************************
     * Don't allow changes to read-only parameters
     * Because of the threaded component model, calls to GetParameter()/SetParameter have
     * an inherent race condition with the component thread during a port enable/disable
     * sequence.  Values such as populated and enabled may change during that transition
     * and cause issues.  Just silently drop the changes and carry on.
     *************************************************************************************/

    newConfig.nPortIndex = m_definition.nPortIndex;
    newConfig.eDir = m_definition.eDir;
    newConfig.bEnabled = m_definition.bEnabled;
    newConfig.bPopulated = m_definition.bPopulated;
    newConfig.eDomain = m_definition.eDomain;
    newConfig.bBuffersContiguous = m_definition.bBuffersContiguous;
    newConfig.nBufferAlignment = m_definition.nBufferAlignment;

    m_definition = newConfig;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_Port::Enable()
{
    if ( m_definition.bEnabled == OMX_FALSE )
    {
        m_definition.bEnabled = OMX_TRUE;
        return OMX_ErrorNone;
    }
    else
    {
        ALOGW("Port already enabled");
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
    }
}

OMX_ERRORTYPE BOMX_Port::Disable()
{
    if ( m_definition.bEnabled == OMX_TRUE )
    {
        m_definition.bEnabled = OMX_FALSE;
        m_definition.bPopulated = OMX_FALSE;    // Per the spec a disabled port should never return populated
        return OMX_ErrorNone;
    }
    else
    {
        ALOGW("Port already disabled");
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
    }
}

OMX_ERRORTYPE BOMX_Port::AddBuffer(
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes,
    OMX_IN OMX_U8* pBuffer,
    OMX_IN OMX_PTR pComponentPrivate,
    bool componentAllocated)
{
    BOMX_Buffer *pOmxBuffer;
    BOMX_BufferNode *pNode;
    OMX_BUFFERHEADERTYPE *pHeader;

    ALOG_ASSERT(NULL != ppBufferHdr);
    ALOG_ASSERT(NULL != pBuffer);

    *ppBufferHdr = (OMX_BUFFERHEADERTYPE *)NULL;

    if ( componentAllocated != m_componentAllocated &&
         m_numBuffers > 0 )
    {
        ALOGW("You must exclusively use OMX_AllocateBuffer OR OMX_UseBuffer on any port");
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    if ( IsPopulated() )
    {
        ALOGW("Port is already populated");
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
    }

    pNode = new BOMX_BufferNode;
    if ( NULL == pNode )
    {
        return BOMX_ERR_TRACE(OMX_ErrorUndefined);
    }
    if ( m_definition.eDir == OMX_DirInput )
    {
        pOmxBuffer = new BOMX_InputBuffer(pAppPrivate, nSizeBytes, pBuffer, pComponentPrivate);
    }
    else
    {
        pOmxBuffer = new BOMX_OutputBuffer(pAppPrivate, nSizeBytes, pBuffer, pComponentPrivate);
    }
    if ( NULL == pOmxBuffer )
    {
        delete pNode;
        return BOMX_ERR_TRACE(OMX_ErrorUndefined);
    }
    pHeader = pOmxBuffer->GetHeader();
    if ( m_definition.eDir == OMX_DirInput )
    {
        pHeader->nInputPortIndex = m_definition.nPortIndex;
    }
    else
    {
        pHeader->nOutputPortIndex = m_definition.nPortIndex;
    }
    pNode->pBuffer = pOmxBuffer;
    BLST_Q_INSERT_TAIL(&m_bufferList, pNode, allocNode);
    m_numBuffers++;
    m_componentAllocated = componentAllocated;

    *ppBufferHdr = pHeader;

    if ( m_numBuffers == m_definition.nBufferCountActual )
    {
        m_definition.bPopulated = OMX_TRUE;
    }

    return OMX_ErrorNone;
}

BOMX_BufferNode *BOMX_Port::FindBufferNode(BOMX_Buffer *pBuffer)
{
    BOMX_BufferNode *pNode;

    for ( pNode = BLST_Q_FIRST(&m_bufferList);
          NULL != pNode;
          pNode = BLST_Q_NEXT(pNode, allocNode) )
    {
        if ( pNode->pBuffer == pBuffer )
        {
            break;
        }
    }

    return pNode;
}

bool BOMX_Port::IsBufferQueued(BOMX_Buffer *pBuffer)
{
    BOMX_BufferNode *pNode;

    for ( pNode = BLST_Q_FIRST(&m_bufferQueue);
          NULL != pNode;
          pNode = BLST_Q_NEXT(pNode, queueNode) )
    {
        if ( pNode->pBuffer == pBuffer )
        {
            break;
        }
    }

    return (pNode == NULL)?false:true;
}


OMX_ERRORTYPE BOMX_Port::RemoveBuffer(
    OMX_BUFFERHEADERTYPE *pBufferHeader
    )
{
    BOMX_Buffer *pBuffer;
    BOMX_BufferNode *pNode;

    ALOG_ASSERT(NULL != pBufferHeader);

    pBuffer = BOMX_BUFFERHEADER_TO_BUFFER(pBufferHeader);
    ALOG_ASSERT(NULL != pBuffer);

    /* Make sure we allocated this buffer */
    pNode = FindBufferNode(pBuffer);
    if ( NULL == pNode )
    {
        ALOGW("Buffer Header %p is not associated with this port", pBufferHeader);
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    if ( IsBufferQueued(pBuffer) )
    {
        ALOGW("Buffer is still queued with this port");
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    /* Remove node */
    BLST_Q_REMOVE(&m_bufferList, pNode, allocNode);
    /* Destroy buffer and node  */
    delete pNode;
    delete pBuffer;

    m_definition.bPopulated = OMX_FALSE;
    ALOG_ASSERT(m_numBuffers > 0);
    m_numBuffers--;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_Port::QueueBuffer(OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    BOMX_Buffer *pBuffer;
    BOMX_BufferNode *pNode;

    ALOG_ASSERT(NULL != pBufferHeader);

    pBuffer = BOMX_BUFFERHEADER_TO_BUFFER(pBufferHeader);
    ALOG_ASSERT(NULL != pBuffer);

    pNode = FindBufferNode(pBuffer);
    if ( NULL == pNode )
    {
        ALOGW("Buffer Header %p is not associated with this port", pBufferHeader);
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    // Make sure buffer isn't already queued
    if ( IsBufferQueued(pBuffer) )
    {
        ALOGW("Buffer Header %p is already queued", pBuffer);
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    BLST_Q_INSERT_TAIL(&m_bufferQueue, pNode, queueNode);
    m_queueDepth++;

    return OMX_ErrorNone;
}

BOMX_Buffer *BOMX_Port::GetBuffer()
{
    BOMX_BufferNode *pNode;

    pNode = BLST_Q_FIRST(&m_bufferQueue);

    return (pNode == NULL)?NULL:pNode->pBuffer;
}

BOMX_Buffer *BOMX_Port::GetNextBuffer(BOMX_Buffer *pBuffer)
{
    BOMX_BufferNode *pNode;
    for ( pNode = BLST_Q_FIRST(&m_bufferQueue);
          NULL != pNode;
          pNode = BLST_Q_NEXT(pNode, queueNode) )
    {
        if (pNode->pBuffer == pBuffer)
        {
            pNode = BLST_Q_NEXT(pNode, queueNode);
            break;
        }
    }

    return (pNode != NULL) ? pNode->pBuffer : NULL;
}

void BOMX_Port::BufferComplete(BOMX_Buffer *pBuffer)
{
    BOMX_BufferNode *pNode;

    for ( pNode = BLST_Q_FIRST(&m_bufferQueue);
          NULL != pNode;
          pNode = BLST_Q_NEXT(pNode, queueNode) )
    {
        if (pNode->pBuffer == pBuffer)
        {
            BLST_Q_REMOVE(&m_bufferQueue, pNode, queueNode);
            ALOG_ASSERT(m_queueDepth > 0);
            m_queueDepth--;
            return;
        }
    }

    ALOG_ASSERT(NULL != pNode);
}

BOMX_Buffer *BOMX_Port::FindBuffer(BOMX_BufferCompareFunction pCompareFunc, void *pData)
{
    if ( pCompareFunc )
    {
        BOMX_BufferNode *pNode;

        for ( pNode = BLST_Q_FIRST(&m_bufferList);
              NULL != pNode;
              pNode = BLST_Q_NEXT(pNode, allocNode) )
        {
            if ( pCompareFunc(pNode->pBuffer, pData) )
            {
                return pNode->pBuffer;
            }
        }
    }

    return NULL;
}

BOMX_Buffer *BOMX_Port::GetPortBuffer()
{
    BOMX_BufferNode *pNode;

    pNode = BLST_Q_FIRST(&m_bufferList);

    return (pNode == NULL)?NULL:pNode->pBuffer;
}

OMX_ERRORTYPE BOMX_Port::SetTunnel(BOMX_Component *pComp, BOMX_Port *pPort)
{
    if ( m_numBuffers > 0 )
    {
        ALOGW("Cannot change tunneling when buffers are assigned to a port");
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
    }

    if ( NULL == pComp && NULL == pPort )
    {
        m_pTunnelComponent = NULL;
        m_pTunnelPort = NULL;
        m_definition.bPopulated = OMX_FALSE;
        return OMX_ErrorNone;
    }

    ALOG_ASSERT(NULL != pComp);
    ALOG_ASSERT(NULL != pPort);

    if ( pPort->GetDomain() != this->GetDomain() )
    {
        ALOGW("Cannot tunnel components of different domains");
        return BOMX_ERR_TRACE(OMX_ErrorPortsNotCompatible);
    }

    m_pTunnelComponent = pComp;
    m_pTunnelPort = pPort;

    if ( IsEnabled() )
    {
        // For us, tunneling always implies proprietary communication.  No need for traditional port buffers.
        m_definition.bPopulated = OMX_TRUE;
    }

    return OMX_ErrorNone;
}
