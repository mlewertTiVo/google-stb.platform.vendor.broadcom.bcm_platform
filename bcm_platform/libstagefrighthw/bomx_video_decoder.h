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
#include "nexus_types.h"
#include "nxclient.h"
#include "nexus_surface_compositor.h"

extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_Create(OMX_COMPONENTTYPE *, OMX_IN OMX_STRING, OMX_IN OMX_PTR, OMX_IN OMX_CALLBACKTYPE*);
extern "C" const char *BOMX_VideoDecoder_GetRole(unsigned roleIndex);

struct BOMX_VideoDecoderInputBufferInfo
{
    void *pHeader;              // Header buffer in NEXUS_Memory space
    size_t maxHeaderLen;        // Allocated header buffer size in bytes
    size_t headerLen;           // Filled header buffer size in bytes
    void *pPayload;             // Optional payload buffer in NEXUS_Memory space (set for useBuffer not allocateBuffer)
    unsigned numDescriptors;    // Number of descriptors in use for this buffer
    uint32_t pts;               // PTS for this input buffer
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

enum BOMX_VideoDecoderOutputBufferType
{
    BOMX_VideoDecoderOutputBufferType_eStandard,
    BOMX_VideoDecoderOutputBufferType_eNative,
    BOMX_VideoDecoderOutputBufferType_eMetadata,
    BOMX_VideoDecoderOutputBufferType_eMax
};

struct BOMX_VideoDecoderFrameBuffer;

#define BOMX_VIDEO_DECODER_DESTRIPE_PLANAR (0)

struct BOMX_VideoDecoderOutputBufferInfo
{
    BOMX_VideoDecoderFrameBuffer *pFrameBuffer;
    BOMX_VideoDecoderOutputBufferType type;
    union
    {
        struct
        {
#if BOMX_VIDEO_DECODER_DESTRIPE_PLANAR
            NEXUS_SurfaceHandle hSurfaceY;
            NEXUS_SurfaceHandle hSurfaceCb;
            NEXUS_SurfaceHandle hSurfaceCr;
#else
            NEXUS_SurfaceHandle hDestripeSurface;
#endif
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

struct BOMX_VideoDecoderFrameBuffer
{
    BLST_Q_ENTRY(BOMX_VideoDecoderFrameBuffer) node;
    BOMX_VideoDecoderFrameBufferState state;
    NEXUS_VideoDecoderFrameStatus frameStatus;
    NEXUS_StripedSurfaceHandle hStripedSurface;
    private_handle_t *pPrivateHandle;
    BOMX_VideoDecoderOutputBufferInfo *pBufferInfo;
};

#define NXCLIENT_INVALID_ID (0xffffffff)

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
    void PlaypumpTimer();
    void OutputFrameEvent();
    void OutputFrameTimer();

    void DisplayFrame(unsigned serialNumber);
    void SetVideoGeometry(NEXUS_Rect *pPosition, NEXUS_Rect *pClipRect, unsigned serialNumber, unsigned gfxWidth, unsigned gfxHeight, unsigned zorder, bool visible);

    inline OmxBinder_wrap *omxHwcBinder(void) { return m_omxHwcBinder; };

protected:

    enum ConfigBufferState
    {
        ConfigBufferState_eAccumulating,        // Config buffer has not yet been submitted to playpump
        ConfigBufferState_eFlushed,             // Input port has been flushed with the existing config buffer
        ConfigBufferState_eSubmitted,           // Config buffer has been submited to playpump
        ConfigBufferState_eMax
    };

    NEXUS_SimpleVideoDecoderHandle m_hSimpleVideoDecoder;
    NEXUS_PlaypumpHandle m_hPlaypump;
    NEXUS_PidChannelHandle m_hPidChannel;
    B_EventHandle m_hPlaypumpEvent;
    B_SchedulerEventId m_playpumpEventId;
    B_SchedulerTimerId m_playpumpTimerId;
    B_EventHandle m_hOutputFrameEvent;
    B_SchedulerEventId m_outputFrameEventId;
    B_SchedulerTimerId m_outputFrameTimerId;
    B_EventHandle m_hCheckpointEvent;
    NEXUS_Graphics2DHandle m_hGraphics2d;
    unsigned m_submittedDescriptors;

    BOMX_BufferTracker *m_pBufferTracker;

    NexusIPCClientBase              *m_pIpcClient;
    NexusClientContext              *m_pNexusClient;
    NxClient_AllocResults            m_allocResults;
    unsigned                         m_nxClientId;
    NEXUS_SurfaceClientHandle        m_hSurfaceClient;
    NEXUS_SurfaceClientHandle        m_hVideoClient;
    NEXUS_SurfaceHandle              m_hAlphaSurface;
    bool                             m_setSurface;

    char m_inputMimeType[OMX_MAX_STRINGNAME_SIZE];
    char m_outputMimeType[OMX_MAX_STRINGNAME_SIZE];

    #define BOMX_VIDEO_EOS_LEN (184*3) /* BPP TPD+LAST+TPD */
    void *m_pEosBuffer;
    bool m_eosPending;
    bool m_eosDelivered;
    bool m_eosReceived;
    bool m_formatChangePending;
    bool m_nativeGraphicsEnabled;
    bool m_metadataEnabled;
    bool m_secureDecoder;
    unsigned m_outputWidth;
    unsigned m_outputHeight;

    #define BOMX_BCMV_HEADER_SIZE (10)
    uint8_t m_pBcmvHeader[BOMX_BCMV_HEADER_SIZE];

    #define BOMX_VIDEO_CODEC_CONFIG_BUFFER_SIZE (1024)
    void *m_pConfigBuffer;
    ConfigBufferState m_configBufferState;
    size_t m_configBufferSize;

    BOMX_VideoDecoderOutputBufferType m_outputMode;

    BLST_Q_HEAD(FrameBufferFreeList, BOMX_VideoDecoderFrameBuffer) m_frameBufferFreeList;
    BLST_Q_HEAD(FrameBufferAllocList, BOMX_VideoDecoderFrameBuffer) m_frameBufferAllocList;

    OmxBinder_wrap *m_omxHwcBinder;

    OMX_VIDEO_CODINGTYPE GetCodec() {return m_pVideoPorts[0]->GetDefinition()->format.video.eCompressionFormat;}
    NEXUS_VideoCodec GetNexusCodec();
    NEXUS_VideoCodec GetNexusCodec(OMX_VIDEO_CODINGTYPE omxType);

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

    OMX_ERRORTYPE AddOutputPortBuffer(
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            VideoDecoderOutputMetaData *pMetadata);

    void InvalidateDecodedFrames();
    void PollDecodedFrames();
    void ReturnDecodedFrames();
    bool GraphicsCheckpoint();
    void CopySurfaceToClient(const BOMX_VideoDecoderOutputBufferInfo *pInfo);
    BOMX_VideoDecoderFrameBuffer *FindFrameBuffer(private_handle_t *pPrivateHandle);
    BOMX_VideoDecoderFrameBuffer *FindFrameBuffer(unsigned serialNumber);
    OMX_ERRORTYPE DestripeToRGB(SHARED_DATA *pSharedData, NEXUS_StripedSurfaceHandle hStripedSurface);

    void DisplayFrame_locked(unsigned serialNumber);
    void SetVideoGeometry_locked(NEXUS_Rect *pPosition, NEXUS_Rect *pClipRect, unsigned serialNumber, unsigned gfxWidth, unsigned gfxHeight, unsigned zorder, bool visible);

    OMX_ERRORTYPE BuildInputFrame(
        OMX_BUFFERHEADERTYPE *pBufferHeader,
        uint32_t pts,
        void *pCodecHeader,
        size_t codecHeaderLength,
        NEXUS_PlaypumpScatterGatherDescriptor *pDescriptors,
        unsigned maxDescriptors,
        unsigned *pNumDescriptors
        );

    // The functions below allow derived classes to override them
    virtual NEXUS_Error AllocateInputBuffer(uint32_t nSize, void*& pBuffer);
    virtual void FreeInputBuffer(void*& pBuffer);
    virtual OMX_ERRORTYPE ConfigBufferInit();
    virtual OMX_ERRORTYPE ConfigBufferAppend(const void *pBuffer, size_t length);
    virtual NEXUS_Error ExtraTransportConfig() {return 0;};

private:
};

#endif //BOMX_VIDEO_DECODER_H__
