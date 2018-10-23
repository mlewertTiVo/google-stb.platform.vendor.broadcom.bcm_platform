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
#include "bomx_video_decoder_stats.h"
#include "bomx_pes_formatter.h"
#include "bomx_avc_parser.h"
#include "bomx_hevc_parser.h"
#include "bomx_mpeg2_parser.h"
#include <stdio.h>
#include <cutils/native_handle.h>
#include <utils/List.h>
#include "vendor_bcm_props.h"

extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_Create(OMX_COMPONENTTYPE *, OMX_IN OMX_STRING, OMX_IN OMX_PTR, OMX_IN OMX_CALLBACKTYPE*);
extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_CreateTunnel(OMX_COMPONENTTYPE *, OMX_IN OMX_STRING, OMX_IN OMX_PTR, OMX_IN OMX_CALLBACKTYPE*);
extern "C" const char *BOMX_VideoDecoder_GetRole(unsigned roleIndex);

extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_CreateVp9(OMX_COMPONENTTYPE *, OMX_IN OMX_STRING, OMX_IN OMX_PTR, OMX_IN OMX_CALLBACKTYPE*);
extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_CreateVp9Tunnel(OMX_COMPONENTTYPE *, OMX_IN OMX_STRING, OMX_IN OMX_PTR, OMX_IN OMX_CALLBACKTYPE*);
extern "C" const char *BOMX_VideoDecoder_GetRoleVp9(unsigned roleIndex);

struct BOMX_VideoDecoderInputBufferInfo
{
    void *pHeader;              // Header buffer in NEXUS_Memory space
    size_t maxHeaderLen;        // Allocated header buffer size in bytes
    size_t headerLen;           // Filled header buffer size in bytes
    void *pPayload;             // Optional payload buffer in NEXUS_Memory space (set for useBuffer not allocateBuffer)
    unsigned numDescriptors;    // Number of descriptors in use for this buffer
    uint32_t pts;               // PTS for this input buffer
    bool complete;              // True if buffer has already been processed by the playpump.
};

enum BOMX_VideoDecoderFrameBufferState
{
    BOMX_VideoDecoderFrameBufferState_eReady,           // Frame decoded and ready to be delivered to client
    BOMX_VideoDecoderFrameBufferState_eDelivered,       // Frame delivered to client
    BOMX_VideoDecoderFrameBufferState_eReturned,        // Frame returned from client and ready to recycle
    BOMX_VideoDecoderFrameBufferState_eInvalid,         // Frame has been invalidated or is not yet decoded
    BOMX_VideoDecoderFrameBufferState_eMax
};

enum BOMX_VideoDecoderOutputBufferType
{
    BOMX_VideoDecoderOutputBufferType_eStandard,
    BOMX_VideoDecoderOutputBufferType_eNative,
    BOMX_VideoDecoderOutputBufferType_eMetadata,
    BOMX_VideoDecoderOutputBufferType_eNone,
    BOMX_VideoDecoderOutputBufferType_eMax
};

struct BOMX_VideoDecoderFrameBuffer;

#define BOMX_VIDEO_DECODER_DESTRIPE_PLANAR (1)

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
            int yMemBlkFd;
            NEXUS_SurfaceHandle hSurfaceCb;
            int cbMemBlkFd;
            NEXUS_SurfaceHandle hSurfaceCr;
            int crMemBlkFd;
#else
            NEXUS_SurfaceHandle hDestripeSurface;
            int destripeMemBlkFd;
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
    void *pPrivateHandle;
    BOMX_VideoDecoderOutputBufferInfo *pBufferInfo;
    bool display;
};

struct BOMX_VideoDecoderRole
{
    char name[OMX_MAX_STRINGNAME_SIZE];
    int omxCodec;
};

#define NXCLIENT_INVALID_ID (0xffffffff)

class BOMX_InputDataTracker
{
public:
    BOMX_InputDataTracker();
    ~BOMX_InputDataTracker() {};
    void AddEntry(unsigned pts);
    inline size_t GetNumEntries();
    void SetLastReturnedPts(unsigned pts);
    unsigned GetMaxDeltaPts();
    unsigned GetLastAddedPts();
    void PrintStats();
    void Reset();

private:
    List<unsigned> m_ptsTracker;
    size_t m_numInputBuffers;
    size_t m_numRetBuffers;
    size_t m_numNotRetBuffers;
};

struct BOMX_VideoSurfaceIndex
{
   bool used;
   void *user;
   int  hwc_cli;
};

struct BOMX_ColorAspects
{
    bool bValid;
    ColorAspects colorAspects;
};

struct BOMX_HdrInfo
{
    bool bValid;
    HDRStaticInfo hdrInfo;
};

class BOMX_VideoDecoder : public BOMX_Component
{
public:
    BOMX_VideoDecoder(
        OMX_COMPONENTTYPE *pComponentType,
        const OMX_STRING pName,
        const OMX_PTR pAppData,
        const OMX_CALLBACKTYPE *pCallbacks,
        NxWrap *pNxWrap,
        bool secure=false,
        bool tunnel=false,
        unsigned numRoles=0,
        const BOMX_VideoDecoderRole *pRoles=NULL,
        const char *(*pGetRole)(unsigned roleIndex)=NULL);

    virtual ~BOMX_VideoDecoder();

    // Begin OMX Standard functions.
    virtual OMX_ERRORTYPE GetParameter(
            OMX_IN  OMX_INDEXTYPE nParamIndex,
            OMX_INOUT OMX_PTR pComponentParameterStructure);

    virtual OMX_ERRORTYPE SetParameter(
            OMX_IN  OMX_INDEXTYPE nIndex,
            OMX_IN  OMX_PTR pComponentParameterStructure);

    virtual OMX_ERRORTYPE UseBuffer(
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes,
            OMX_IN OMX_U8* pBuffer);

    virtual OMX_ERRORTYPE AllocateBuffer(
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes);

    virtual OMX_ERRORTYPE FreeBuffer(
            OMX_IN  OMX_U32 nPortIndex,
            OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

    virtual OMX_ERRORTYPE EmptyThisBuffer(
            OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

    virtual OMX_ERRORTYPE FillThisBuffer(
            OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

    virtual OMX_ERRORTYPE GetExtensionIndex(
            OMX_IN  OMX_STRING cParameterName,
            OMX_OUT OMX_INDEXTYPE* pIndexType);

    virtual OMX_ERRORTYPE GetConfig(
            OMX_IN  OMX_INDEXTYPE nIndex,
            OMX_INOUT OMX_PTR pComponentConfigStructure);

    virtual OMX_ERRORTYPE SetConfig(
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
    void SourceChangedEvent();
    void StreamChangedEvent();
    void DecodeErrorEvent();
    void FifoEmptyEvent();
    void PlaypumpErrorCallbackEvent();
    void PlaypumpCcErrorEvent();

    void SetVideoGeometry(NEXUS_Rect *pPosition, NEXUS_Rect *pClipRect, unsigned serialNumber, unsigned gfxWidth, unsigned gfxHeight, unsigned zorder, bool visible);
    void DisplayFrameEvent();
    void RunDisplayThread(void);
    void BinderNotifyDisplay(struct hwc_notification_info &ntfy);

    inline OmxBinder_wrap *omxHwcBinder(void) { return m_omxHwcBinder; };
    void InputBufferTimeoutCallback();
    void FormatChangeTimeoutCallback();

protected:

    enum ConfigBufferState
    {
        ConfigBufferState_eAccumulating,        // Config buffer has not yet been submitted to playpump
        ConfigBufferState_eFlushed,             // Input port has been flushed with the existing config buffer
        ConfigBufferState_eSubmitted,           // Config buffer has been submited to playpump
        ConfigBufferState_eMax
    };

    enum InputReturnMode
    {
        InputReturnMode_eDefault,               // Default mode
        InputReturnMode_eTimeout,               // Return buffers as a result of an input-buffers timeout event
        InputReturnMode_eAll                    // Return all completed buffers
    };

    NEXUS_SimpleVideoDecoderHandle m_hSimpleVideoDecoder;
    NEXUS_PlaypumpHandle m_hPlaypump;
    NEXUS_PidChannelHandle m_hPidChannel;
    B_EventHandle m_hPlaypumpEvent;
    B_SchedulerEventId m_playpumpEventId;
    B_SchedulerTimerId m_playpumpTimerId;
    B_EventHandle m_hOutputFrameEvent;
    B_SchedulerEventId m_outputFrameEventId;
    B_EventHandle m_hSourceChangedEvent;
    B_SchedulerEventId m_sourceChangedEventId;
    B_EventHandle m_hStreamChangedEvent;
    B_SchedulerEventId m_streamChangedEventId;
    B_EventHandle m_hDecodeErrorEvent;
    B_SchedulerEventId m_decodeErrorEventId;
    B_EventHandle m_hFifoEmptyEvent;
    B_SchedulerEventId m_fifoEmptyEventId;
    B_EventHandle m_hPlaypumpErrorCallbackEvent;
    B_SchedulerEventId m_playpumpErrorCallbackEventId;
    B_EventHandle m_hPlaypumpCcErrorEvent;
    B_SchedulerEventId m_playpumpCcErrorEventId;
    BKNI_EventHandle m_hDisplayFrameEvent;
    B_SchedulerEventId m_displayFrameEventId;
    B_MutexHandle m_hDisplayMutex;
    B_SchedulerTimerId m_inputBuffersTimerId;
    B_EventHandle m_hCheckpointEvent;
    NEXUS_Graphics2DHandle m_hGraphics2d;
    unsigned m_submittedDescriptors;
    unsigned m_maxDescriptorsPerBuffer;
    NEXUS_PlaypumpScatterGatherDescriptor *m_pPlaypumpDescList;
    BOMX_InputDataTracker m_inputDataTracker;
    BOMX_BufferTracker *m_pBufferTracker;
    unsigned m_AvailInputBuffers;
    NEXUS_VideoFrameRate m_frameRate;

    bool m_frEstimated;
    unsigned m_frStableCount;
    OMX_TICKS m_deltaUs;
    OMX_TICKS m_lastTsUs;

    NxWrap                           *m_pNxWrap;
    uint64_t                         m_nexusClient;
    NxClient_AllocResults            m_allocResults;
    unsigned                         m_nxClientId;
    NEXUS_SurfaceClientHandle        m_hSurfaceClient;
    NEXUS_SurfaceClientHandle        m_hVideoClient;
    NEXUS_SurfaceHandle              m_hAlphaSurface;
    bool                             m_setSurface;
    FILE                            *m_pPesFile;
    FILE                            *m_pInputFile;
    FILE                            *m_pOutputFile;
    BOMX_PesFormatter               *m_pPes;

    char m_inputMimeType[OMX_MAX_STRINGNAME_SIZE];
    char m_outputMimeType[OMX_MAX_STRINGNAME_SIZE];

    #define BOMX_VIDEO_EOS_LEN (B_BPP_PACKET_LEN*3) /* BPP TPD+LAST+TPD */
    void *m_pEosBuffer;
    bool m_eosPending;
    bool m_eosDelivered;
    bool m_eosReceived;
    unsigned m_eosPts;
    enum FormatChangeState
    {
        FCState_eNone,
        FCState_eWaitForSerial,
        FCState_eProcessCallback,
        FCState_eWaitForPortReconfig
    };
    FormatChangeState m_formatChangeState;
    B_SchedulerTimerId m_formatChangeTimerId;
    unsigned m_formatChangeSerial;
    bool m_nativeGraphicsEnabled;
    bool m_metadataEnabled;
    bool m_adaptivePlaybackEnabled;
    bool m_secureDecoder;
    bool m_allocNativeHandle;
    bool m_tunnelMode;
    bool m_tunnelHfr;
    int m_enableHdrForNonVp9;
    native_handle_t *m_pTunnelNativeHandle;
    uint32_t m_tunnelCurrentPts;
    bool m_waitingForStc;
    unsigned m_stcSyncValue;
    bool m_stcResumePending;
    unsigned m_outputWidth;
    unsigned m_outputHeight;
    unsigned m_maxDecoderWidth;
    unsigned m_maxDecoderHeight;
    BOMX_VideoDecoderRole *m_pRoles;
    unsigned m_numRoles;

    #define BOMX_BCMV_HEADER_SIZE (10)
    uint8_t m_pBcmvHeader[BOMX_BCMV_HEADER_SIZE];

    #define BOMX_VIDEO_CODEC_CONFIG_BUFFER_SIZE (4096)
    void *m_pConfigBuffer;
    ConfigBufferState m_configBufferState;
    size_t m_configBufferSize;

    void *m_pEndOfChunkBuffer;  // End of chunk BPP buffer for VP9

    BOMX_VideoDecoderOutputBufferType m_outputMode;

    BLST_Q_HEAD(FrameBufferFreeList, BOMX_VideoDecoderFrameBuffer) m_frameBufferFreeList;
    BLST_Q_HEAD(FrameBufferAllocList, BOMX_VideoDecoderFrameBuffer) m_frameBufferAllocList;

    OmxBinder_wrap *m_omxHwcBinder;
    int m_memTracker;
    bool m_secureRuntimeHeaps;
    bool m_decInstanceCounted;

    // Needed to save the latest geometry and frame serial to display
    NEXUS_Rect m_framePosition, m_frameClip;
    unsigned m_frameSerial, m_frameWidth, m_frameHeight, m_framezOrder;
    bool m_displayFrameAvailable;
    bool m_displayThreadStop;
    pthread_t m_hDisplayThread;
    unsigned m_lastReturnedSerial;

    size_t m_droppedFrames;
    size_t m_consecDroppedFrames;
    size_t m_maxConsecDroppedFrames;
    size_t m_earlyDroppedFrames;
    int m_earlyDropThresholdMs;
    nsecs_t m_startTime;
    size_t m_texturedFrames;

    NEXUS_SimpleStcChannelHandle m_tunnelStcChannel;
    NEXUS_SimpleStcChannelHandle m_tunnelStcChannelSync;

    bool m_inputFlushing;
    bool m_outputFlushing;
    bool m_ptsReceived;

    NEXUS_VideoDecoderStreamInformation m_videoStreamInfo;
    BOMX_HdrInfo m_hdrInfoFwks;
    BOMX_HdrInfo m_hdrInfoStream;
    BOMX_HdrInfo m_hdrInfoFinal;
    bool m_describeHdrColorInfoDisabled;
    BOMX_ColorAspects m_colorAspectsFwks;
    BOMX_ColorAspects m_colorAspectsStream;
    BOMX_ColorAspects m_colorAspectsFinal;

    bool m_redux;
    int m_indexSurface;
    bool m_virtual;
    bool m_forcePortResetOnHwTex;
    sp<SdbGeomCb> m_sdbGeomCb;

    OMX_VIDEO_CODINGTYPE GetCodec() {return m_pVideoPorts[0]->GetDefinition()->format.video.eCompressionFormat;}
    NEXUS_VideoCodec GetNexusCodec();
    NEXUS_VideoCodec GetNexusCodec(OMX_VIDEO_CODINGTYPE omxType);
    void ColorAspectsFromNexusStreamInfo(ColorAspects *colorAspects);
    void HdrInfoFromNexusStreamInfo(HDRStaticInfo *hdrInfo);
    void SetStreamColorAspects(int32_t primaries, int32_t transfer, int32_t coeffs, bool fullRange);
    void GenerateFinalColorAspects();
    void GenerateFinalHdrInfo();

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
    void CopyAlignedSurfaceToClient(uint8_t *pClientMemory, const uint8_t *pSurfaceMemory, uint16_t height, unsigned int pitch, OMX_S32 stride);
    void CopySurfaceToClient(const BOMX_VideoDecoderOutputBufferInfo *pInfo);
    BOMX_VideoDecoderFrameBuffer *FindFrameBuffer(VideoDecoderOutputMetaData *pMetadata);
    BOMX_VideoDecoderFrameBuffer *FindFrameBuffer(unsigned serialNumber);
    OMX_ERRORTYPE DestripeToYV12(SHARED_DATA *pSharedData, NEXUS_StripedSurfaceHandle hStripedSurface);

    void DisplayFrame_locked(unsigned serialNumber);
    void SetVideoGeometry_locked(NEXUS_Rect *pPosition, NEXUS_Rect *pClipRect, unsigned serialNumber, unsigned gfxWidth, unsigned gfxHeight, unsigned zorder, bool visible);

    OMX_ERRORTYPE BuildInputFrame(
        OMX_BUFFERHEADERTYPE *pBufferHeader,
        bool first,
        unsigned chunkLength,
        uint32_t pts,
        void *pCodecHeader,
        size_t codecHeaderLength,
        NEXUS_PlaypumpScatterGatherDescriptor *pDescriptors,
        unsigned maxDescriptors,
        size_t *pNumDescriptors
        );

    void CancelTimerId(B_SchedulerTimerId& timerId);
    OMX_ERRORTYPE VerifyInputPortBuffers();
    OMX_ERRORTYPE UpdateInputDimensions();
    void CleanupPortBuffers(OMX_U32 nPortIndex);
    BOMX_Buffer *AssociateOutVdec2Omx(BOMX_VideoDecoderFrameBuffer *pVdecBuffer, bool &shouldResetPort);
    void RemoveAllVdecOmxAssociation();

    // These functions are used to pace the input buffers rate
    void ReturnInputBuffers(InputReturnMode mode = InputReturnMode_eDefault);
    void ReturnInputPortBuffer(BOMX_Buffer *pBuffer);
    void ProcessFifoData(bool *pendingData = NULL);

    // These functions are used for frame rate estimation
    void ResetEstimation();
    void EstimateFrameRate(OMX_TICKS tsUs);

    // The functions below allow derived classes to override them
    virtual NEXUS_Error AllocateInputBuffer(uint32_t nSize, void*& pBuffer);
    virtual void FreeInputBuffer(void*& pBuffer);
    virtual NEXUS_Error AllocateConfigBuffer(uint32_t nSize, void*& pBuffer);
    virtual void FreeConfigBuffer(void*& pBuffer);
    virtual OMX_ERRORTYPE ConfigBufferInit();
    virtual OMX_ERRORTYPE ConfigBufferAppend(const void *pBuffer, size_t length);
    virtual NEXUS_Error OpenPidChannel(uint32_t pid);
    virtual void ClosePidChannel();

    // Helper class to report rendered frame events to the framework when doing
    // video tunneling
    class BOMX_RenderedFrameHandler
    {
    public:
        const size_t MAX_FRAMES_REPORTED_AFTER_EOS = 48;
        BOMX_RenderedFrameHandler(BOMX_VideoDecoder *parent);
        ~BOMX_RenderedFrameHandler() {};
        void NewRenderedFrame(OMX_S64 timestamp, bool isEos);
        void Reset();

    private:
        BOMX_VideoDecoder *m_pParent;
        bool m_eosHandlingStarted;
        size_t m_numRenderedAfterEos;
        unsigned m_reportedFrameDelta;
    };

    BOMX_RenderedFrameHandler m_renderedFrameHandler;
private:
    BOMX_VIDEO_STATS_DEC;

    void DumpInputBuffer(OMX_BUFFERHEADERTYPE *pHeader);

};

#endif //BOMX_VIDEO_DECODER_H__
