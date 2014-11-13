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
#ifndef BOMX_VIDEO_DECODER_H__
#define BOMX_VIDEO_DECODER_H__

#include "bomx_component.h"
#include "bomx_buffer_tracker.h"
#include "nexus_simple_video_decoder.h"
#include "nexus_playpump.h"
#include "OMX_Video.h"
#include "OMX_VideoExt.h"
#include <media/hardware/HardwareAPI.h>
#include <ui/GraphicBuffer.h>
#include "gralloc_priv.h"
#include "blst_queue.h"
#include "bomx_hwcbinder.h"

extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_Create(OMX_COMPONENTTYPE *, OMX_IN OMX_STRING, OMX_IN OMX_PTR, OMX_IN OMX_CALLBACKTYPE*);
extern "C" const char *BOMX_VideoDecoder_GetRole(unsigned roleIndex);

struct BOMX_VideoDecoderInputBufferInfo
{
    void *pHeader;              // Header buffer in NEXUS_Memory space
    void *pPayload;             // Optional payload buffer in NEXUS_Memory space (set for useBuffer not allocateBuffer)
    unsigned numDescriptors;    // Number of descriptors in use for this buffer
};

enum BOMX_VideoDecoderFrameBufferState
{
    BOMX_VideoDecoderFrameBufferState_eReady,           // Frame decoded and ready to be delivered to client
    BOMX_VideoDecoderFrameBufferState_eDelivered,       // Frame delivered to client
    BOMX_VideoDecoderFrameBufferState_eDisplayReady,    // Frame returned from client and ready to display
    BOMX_VideoDecoderFrameBufferState_eDropReady,       // Frame returned from client and ready to drop
    BOMX_VideoDecoderFrameBufferState_eInvalid,         // Frame has been invalidated or is not yet decoded
    BOMX_VideoDecoderFrameBufferState_eMax
};

struct BOMX_VideoDecoderFrameBuffer
{
    BLST_Q_ENTRY(BOMX_VideoDecoderFrameBuffer) node;
    BOMX_VideoDecoderFrameBufferState state;
    NEXUS_VideoDecoderFrameStatus frameStatus;
};

enum BOMX_VideoDecoderOutputBufferType
{
    BOMX_VideoDecoderOutputBufferType_eStandard,
    BOMX_VideoDecoderOutputBufferType_eNative,
    BOMX_VideoDecoderOutputBufferType_eMetadata,
    BOMX_VideoDecoderOutputBufferType_eMax
};

struct BOMX_VideoDecoderOutputBufferInfo
{
    BOMX_VideoDecoderFrameBuffer *pFrameBuffer;
    BOMX_VideoDecoderOutputBufferType type;
    union
    {
        struct
        {
            NEXUS_SurfaceHandle hSurface;
            void *pSurfaceMemory;   // Returned from NEXUS_Surface_GetMemory
            void *pClientMemory;    // Client memory provided in OMX_UseBuffer (NULL for OMX_AllocateBuffer)
        } standard;
        struct
        {
            private_handle_t *pPrivateHandle;
            SHARED_DATA *pSharedData;
        } native;
        struct
        {
            VideoDecoderOutputMetaData *pMetadata;
        } metadata;
    } typeInfo;
};

class BOMX_VideoDecoder : public BOMX_Component
{
public:
    BOMX_VideoDecoder(
        OMX_COMPONENTTYPE *pComponentType,
        const OMX_STRING pName,
        const OMX_PTR pAppData,
        const OMX_CALLBACKTYPE *pCallbacks);

    virtual ~BOMX_VideoDecoder();

    // Begin OMX Standard functions.

    OMX_ERRORTYPE SendCommand(
        OMX_IN  OMX_COMMANDTYPE Cmd,
        OMX_IN  OMX_U32 nParam1,
        OMX_IN  OMX_PTR pCmdData);

    OMX_ERRORTYPE GetParameter(
            OMX_IN  OMX_INDEXTYPE nParamIndex,
            OMX_INOUT OMX_PTR pComponentParameterStructure);

    OMX_ERRORTYPE SetParameter(
            OMX_IN  OMX_INDEXTYPE nIndex,
            OMX_IN  OMX_PTR pComponentParameterStructure);

    OMX_ERRORTYPE UseBuffer(
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes,
            OMX_IN OMX_U8* pBuffer);

    OMX_ERRORTYPE AllocateBuffer(
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes);

    OMX_ERRORTYPE FreeBuffer(
            OMX_IN  OMX_U32 nPortIndex,
            OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

    OMX_ERRORTYPE EmptyThisBuffer(
            OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

    OMX_ERRORTYPE FillThisBuffer(
            OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

    OMX_ERRORTYPE GetExtensionIndex(
            OMX_IN  OMX_STRING cParameterName,
            OMX_OUT OMX_INDEXTYPE* pIndexType);

    OMX_ERRORTYPE GetConfig(
            OMX_IN  OMX_INDEXTYPE nIndex,
            OMX_INOUT OMX_PTR pComponentConfigStructure);

    OMX_ERRORTYPE SetConfig(
            OMX_IN  OMX_INDEXTYPE nIndex,
            OMX_IN  OMX_PTR pComponentConfigStructure);

    // End OMX Standard functions.

    // Begin OMX Command Handlers - These functions should block until completion.  The return code
    // will take care of sending the correct callback to the client.
    // All commands must be implemented except for MarkBuffer, which is optional.
    OMX_ERRORTYPE CommandStateSet(
        OMX_STATETYPE newState,
        OMX_STATETYPE oldState);

    OMX_ERRORTYPE CommandFlush(
        OMX_U32 portIndex);

    OMX_ERRORTYPE CommandPortEnable(
        OMX_U32 portIndex);
    OMX_ERRORTYPE CommandPortDisable(
        OMX_U32 portIndex);
    // End OMX Command Handlers

    // Local Event Handlers
    void PlaypumpEvent();
    void OutputFrameEvent();
    void OutputFrameTimer();

protected:
    NEXUS_SimpleVideoDecoderHandle m_hSimpleVideoDecoder;
    NEXUS_PlaypumpHandle m_hPlaypump;
    NEXUS_PidChannelHandle m_hPidChannel;
    B_EventHandle m_hPlaypumpEvent;
    B_SchedulerEventId m_playpumpEventId;
    B_EventHandle m_hOutputFrameEvent;
    B_SchedulerEventId m_outputFrameEventId;
    B_SchedulerTimerId m_outputFrameTimerId;
    B_EventHandle m_hCheckpointEvent;
    NEXUS_Graphics2DHandle m_hGraphics2d;
    unsigned m_submittedDescriptors;

    BOMX_BufferTracker *m_pBufferTracker;

    NexusIPCClientBase              *m_pIpcClient;
    NexusClientContext              *m_pNexusClient;

    char m_inputMimeType[OMX_MAX_STRINGNAME_SIZE];
    char m_outputMimeType[OMX_MAX_STRINGNAME_SIZE];

    #define BOMX_VIDEO_EOS_LEN (184*3) /* BPP TPD+LAST+TPD */
    void *m_pEosBuffer;
    bool m_eosPending;
    bool m_eosDelivered;
    bool m_formatChangePending;

    #define BOMX_BCMV_HEADER_SIZE (10)
    uint8_t m_pBcmvHeader[BOMX_BCMV_HEADER_SIZE];

    #define BOMX_VIDEO_CODEC_CONFIG_HEADER_SIZE (9)
    #define BOMX_VIDEO_CODEC_CONFIG_BUFFER_SIZE (1024+BOMX_VIDEO_CODEC_CONFIG_HEADER_SIZE)
    void *m_pConfigBuffer;
    bool m_configRequired;
    size_t m_configBufferSize;

    BOMX_VideoDecoderOutputBufferType m_outputMode;

    BLST_Q_HEAD(FrameBufferFreeList, BOMX_VideoDecoderFrameBuffer) m_frameBufferFreeList;
    BLST_Q_HEAD(FrameBufferAllocList, BOMX_VideoDecoderFrameBuffer) m_frameBufferAllocList;

    OmxBinder_wrap *m_omxHwcBinder;
    int m_surfaceClientId;

    OMX_VIDEO_CODINGTYPE GetCodec() {return m_pVideoPorts[0]->GetDefinition()->format.video.eCompressionFormat;}
    NEXUS_VideoCodec GetNexusCodec();

    NEXUS_Error SetInputPortState(OMX_STATETYPE newState);
    NEXUS_Error SetOutputPortState(OMX_STATETYPE newState);

    OMX_ERRORTYPE AddInputPortBuffer(
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes,
            OMX_IN OMX_U8* pBuffer,
            bool componentAllocated);

    OMX_ERRORTYPE AddOutputPortBuffer(
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes,
            OMX_IN OMX_U8* pBuffer,
            bool componentAllocated);

    OMX_ERRORTYPE AddOutputPortBuffer(
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            private_handle_t *pPrivateHandle);

    void InvalidateDecodedFrames();
    void PollDecodedFrames();
    void ReturnDecodedFrames();
    void GraphicsCheckpoint();
    void CopySurfaceToClient(const BOMX_VideoDecoderOutputBufferInfo *pInfo);
    void ConfigBufferInit();
    OMX_ERRORTYPE ConfigBufferAppend(const void *pBuffer, size_t length);
private:
};

#endif //BOMX_VIDEO_DECODER_H__
