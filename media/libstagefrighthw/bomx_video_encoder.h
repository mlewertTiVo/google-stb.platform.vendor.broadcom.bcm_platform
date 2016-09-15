/******************************************************************************
 *    (c)2010-2015 Broadcom Corporation
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
#ifndef BOMX_VIDEO_ENCODER_H__
#define BOMX_VIDEO_ENCODER_H__

#include "bomx_component.h"
#include "bomx_buffer_tracker.h"
#include "nexus_simple_video_decoder.h"
#include "nexus_simple_encoder.h"
#include "OMX_Video.h"
#include "OMX_VideoExt.h"
#include <media/hardware/HardwareAPI.h>
#include <ui/GraphicBuffer.h>
#include "gralloc_priv.h"
#include "blst_queue.h"
#include "nexus_types.h"
#include "nxclient.h"

#include <utils/Vector.h>

extern "C" OMX_ERRORTYPE BOMX_VideoEncoder_Create(OMX_COMPONENTTYPE *, OMX_IN OMX_STRING, OMX_IN OMX_PTR, OMX_IN OMX_CALLBACKTYPE*);
extern "C" const char *BOMX_VideoEncoder_GetRole(unsigned roleIndex);

using namespace android;
using  android::Vector;

#define  BOMX_VIDEO_ENCODER_INIT_ENC_FR(_ENC_FR_) \
        _ENC_FR_->baseAddr=0; \
        _ENC_FR_->clientFlags=0; \
        _ENC_FR_->combinedSz=0; \
        _ENC_FR_->usTimeStampOriginal=0; \
        _ENC_FR_->usTimeStampIntepolated=0; \
        _ENC_FR_->frameData->clear();

typedef enum _Encode_Frame_Type_
{
    Encode_Frame_Type_SPS = 0,
    Encode_Frame_Type_PPS = 1,
    Encode_Frame_Type_Picture = 2
} Encode_Frame_Type;

typedef struct BOMX_NexusEncodedVideoFrame
{
    Vector < NEXUS_VideoEncoderDescriptor *>  *frameData; // Vector of chunks of data
    unsigned int baseAddr; // Base address returned by nexus
    unsigned int combinedSz; // Total size of all the chunks in the frame vector
    unsigned long long usTimeStampOriginal; // Timestamp in us
    unsigned long long usTimeStampIntepolated; // Timestamp in us
    unsigned int clientFlags; // OMX (Or any other) Flags associated with buffer (if Any)
    BLST_Q_ENTRY(BOMX_NexusEncodedVideoFrame) link; // Linked List
} BOMX_NexusEncodedVideoFrame;

struct BOMX_ImageSurfaceNode
{
    BLST_Q_ENTRY(BOMX_ImageSurfaceNode) node;
    NEXUS_SurfaceHandle hSurface;
    int memBlkFd;
};

enum BOMX_VideoEncoderInputBufferType
{
    BOMX_VideoEncoderInputBufferType_eStandard = 0,
    BOMX_VideoEncoderInputBufferType_eNative,
    BOMX_VideoEncoderInputBufferType_eMetadata,
    BOMX_VideoEncoderInputBufferType_eMax
};

struct BOMX_VideoEncoderInputBufferInfo
{
    uint32_t pts; // PTS for this input buffer
    BOMX_VideoEncoderInputBufferType type; // buffer type
    union
    {
        struct
        {
            void *pAllocatedBuffer; // OMX_AllocateBuffer
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

enum BOMX_VideoEncoderOutputBufferType
{
    BOMX_VideoEncoderOutputBufferType_eUseBuffer,
    BOMX_VideoEncoderOutputBufferType_eAllocatedBuffer,
    BOMX_VideoEncoderOutputBufferType_eMax
};

struct BOMX_VideoEncoderOutputBufferInfo
{
    BOMX_VideoEncoderOutputBufferType type;
};

class BOMX_VideoEncoder : public BOMX_Component
{
public:
    BOMX_VideoEncoder(
        OMX_COMPONENTTYPE *pComponentType,
        const OMX_STRING pName,
        const OMX_PTR pAppData,
        const OMX_CALLBACKTYPE *pCallbacks,
        NexusIPCClientBase *pIpcClient=NULL,
        NexusClientContext *pNexusClient=NULL);

    virtual ~BOMX_VideoEncoder();

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
    void OutputFrameEvent();
    void OutputBufferProcess();
    void ImageBufferProcess();
    void InputBufferProcess();

protected:


    NexusIPCClientBase              *m_pIpcClient;
    NexusClientContext              *m_pNexusClient;
    NxClient_AllocResults            m_allocResults;
    unsigned                         m_nxClientId;

    bool                            m_bInputResourceAllocated;
    NEXUS_SimpleVideoDecoderHandle  m_hSimpleVideoDecoder;
    NEXUS_SimpleEncoderHandle       m_hSimpleEncoder;
    NEXUS_SimpleStcChannelHandle    m_hStcChannel;
    NEXUS_VideoImageInputHandle     m_hImageInput;
    BLST_Q_HEAD(BOMX_ImageSurfaceFreeList,  BOMX_ImageSurfaceNode) m_ImageSurfaceFreeList;
    BLST_Q_HEAD(BOMX_ImageSurfacePushedList,  BOMX_ImageSurfaceNode) m_ImageSurfacePushedList;
    unsigned m_nImageSurfaceFreeListLen;
    unsigned m_nImageSurfacePushedListLen;

    bool m_bSimpleEncoderStarted;
    bool m_bCodecConfigDone;
    bool m_bOutputEosReceived;
    bool m_bInputEosPushed;
    bool m_bRetryPushInputEos;
    bool m_bPushDummyFrame;

    BLST_Q_HEAD(BOMX_EncodedFrList, BOMX_NexusEncodedVideoFrame) m_EncodedFrList;
    BLST_Q_HEAD(BOMX_EmptyFrList, BOMX_NexusEncodedVideoFrame) m_EmptyFrList;
    unsigned int m_EmptyFrListLen;
    unsigned int m_EncodedFrListLen;

    B_EventHandle m_hImageInputEvent;
    B_SchedulerEventId m_ImageInputEventId;

    B_EventHandle m_hOutputBufferProcessEvent;
    B_SchedulerEventId m_outputBufferProcessEventId;

    B_EventHandle m_hInputBufferProcessEvent;
    B_SchedulerEventId m_inputBufferProcessEventId;

    B_EventHandle m_hCheckpointEvent;
    NEXUS_Graphics2DHandle m_hGraphics2d;

    OMX_VIDEO_PARAM_AVCTYPE m_sAvcVideoParams;
    OMX_VIDEO_PARAM_BITRATETYPE m_sVideoBitrateParams;

    BOMX_BufferTracker *m_pBufferTracker;
    unsigned m_AvailInputBuffers;

    char m_inputMimeType[OMX_MAX_STRINGNAME_SIZE];
    char m_outputMimeType[OMX_MAX_STRINGNAME_SIZE];

    unsigned m_maxFrameWidth;
    unsigned m_maxFrameHeight;
    bool m_metadataEnabled;
    bool m_nativeGraphicsEnabled;
    int m_duplicateFrameCount;
    unsigned long long m_lastOutputFramePTS;
    unsigned long long m_lastOutputFrameTicks;

    BOMX_VideoEncoderInputBufferType m_inputMode;

    NEXUS_SimpleEncoderStatus m_videoEncStatus;

    FILE *m_pITBDescDumpFile;
    int m_memTracker;

    OMX_VIDEO_CODINGTYPE GetCodec() {
        return m_pVideoPorts[1]->GetDefinition()->format.video.eCompressionFormat;
    }
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

    OMX_ERRORTYPE AddInputPortBuffer(
        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
        OMX_IN OMX_U32 nPortIndex,
        OMX_IN OMX_PTR pAppPrivate,
        private_handle_t *pPrivateHandle);

    OMX_ERRORTYPE AddInputPortBuffer(
        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
        OMX_IN OMX_U32 nPortIndex,
        OMX_IN OMX_PTR pAppPrivate,
        VideoDecoderOutputMetaData *pMetadata);

    OMX_ERRORTYPE AddOutputPortBuffer(
        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
        OMX_IN OMX_U32 nPortIndex,
        OMX_IN OMX_PTR pAppPrivate,
        OMX_IN OMX_U32 nSizeBytes,
        OMX_IN OMX_U8* pBuffer,
        bool componentAllocated);


    // misc functions
    void StopEncoder();
    NEXUS_Error StartOutput();
    void StopOutput();
    NEXUS_Error StartInput();
    NEXUS_Error AllocateEncoderResource();
    void ReleaseEncoderResource();
    NEXUS_Error StartEncoder();
    void DestroyImageSurfaces();
    NEXUS_VideoFrameRate MapOMXFrameRateToNexus(OMX_U32);
    unsigned int RetrieveFrameFromHardware(void *pBufferBase);
    bool ReturnEncodedFrameSynchronized(BOMX_NexusEncodedVideoFrame *);
    void PrintVideoEncoderStatus(void *pBufferBase);
    bool IsEncoderStarted();
    NEXUS_Error PushInputEosFrame();
    OMX_ERRORTYPE BuildInputFrame(OMX_BUFFERHEADERTYPE *);
    OMX_ERRORTYPE BuildDummyFrame();
    NEXUS_VideoCodecLevel ConvertOMXLevelTypetoNexus(OMX_VIDEO_AVCLEVELTYPE );
    NEXUS_VideoCodecProfile ConvertOMXProfileTypetoNexus(OMX_VIDEO_AVCPROFILETYPE );
    void NotifyOutputPortSettingsChanged();

    void ReturnInputBuffers(bool returnAll = false);
    void ResetEncodedFrameList();

    void VideoEncoderBufferBlock_Lock(void **pBufferBase);
    void VideoEncoderBufferBlock_Unlock();

    NEXUS_Error AllocateInputBuffer(uint32_t nSize, void*& pBuffer);
    void FreeInputBuffer(void*& pBuffer);
    bool GraphicsCheckpoint();
    NEXUS_Error ConvertYv12To422p(NEXUS_SurfaceHandle, NEXUS_SurfaceHandle, NEXUS_SurfaceHandle, NEXUS_SurfaceHandle, bool);
    NEXUS_Error ExtractGrallocBuffer(private_handle_t *, NEXUS_SurfaceHandle);
    NEXUS_Error ExtractNexusBuffer(uint8_t *, unsigned int, unsigned, NEXUS_SurfaceHandle);



    bool ConvertOMXPixelFormatToCrYCbY(OMX_BUFFERHEADERTYPE *, NEXUS_SurfaceHandle);
    bool GetCodecConfig(void *pBufferBase, const NEXUS_VideoEncoderDescriptor *, size_t , const NEXUS_VideoEncoderDescriptor *, size_t );
    bool HaveCompleteFrame( const NEXUS_VideoEncoderDescriptor *, size_t, const NEXUS_VideoEncoderDescriptor *, size_t, size_t *);

    NEXUS_Error UpdateEncoderSettings();
    OMX_VIDEO_AVCLEVELTYPE GetMaxLevelAvc(OMX_VIDEO_AVCPROFILETYPE profile);

private:

    NEXUS_SurfaceHandle CreateSurface(int width, int height, int stride, NEXUS_PixelFormat format,
        NEXUS_MemoryBlockHandle handle, unsigned offset, void *pAddr, int *pMemBlkFd);
};

#endif //BOMX_VIDEO_ENCODER_H__
