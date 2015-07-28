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
#ifndef BOMX_AUDIO_DECODER_H__
#define BOMX_AUDIO_DECODER_H__

#include "bomx_component.h"
#include "bomx_buffer_tracker.h"
#include "nexus_simple_audio_decoder.h"
#include "nexus_playpump.h"
#include "OMX_Audio.h"
#include "OMX_AudioExt.h"
#include "blst_queue.h"
#include "nexus_types.h"
#include "nxclient.h"
#include "bomx_pes_formatter.h"
#include <stdio.h>

extern "C" OMX_ERRORTYPE BOMX_AudioDecoder_Create(OMX_COMPONENTTYPE *, OMX_IN OMX_STRING, OMX_IN OMX_PTR, OMX_IN OMX_CALLBACKTYPE*);
extern "C" const char *BOMX_AudioDecoder_GetRole(unsigned roleIndex);

struct BOMX_AudioDecoderInputBufferInfo
{
    void *pHeader;              // Header buffer in NEXUS_Memory space
    size_t maxHeaderLen;        // Allocated header buffer size in bytes
    size_t headerLen;           // Filled header buffer size in bytes
    void *pPayload;             // Optional payload buffer in NEXUS_Memory space (set for useBuffer not allocateBuffer)
    unsigned numDescriptors;    // Number of descriptors in use for this buffer
    uint32_t pts;               // PTS for this input buffer
    bool complete;              // True if buffer has already been processed by the playpump.
};

struct BOMX_AudioDecoderOutputBufferInfo
{
    NEXUS_MemoryBlockHandle hMemoryBlock;
    void *pNexusMemory;     // Locked block virtual address
    void *pClientMemory;    // NULL if OMX_AllocBuffer was called
    bool nexusOwned;        // True if owned by nexus.  False if not (e.g. decoder is stopped because of input port disable)
};

struct BOMX_AudioDecoderRole
{
    char name[OMX_MAX_STRINGNAME_SIZE];
    int omxCodec;
};

enum BOMX_AudioDecoderState
{
    BOMX_AudioDecoderState_eStopped,
    BOMX_AudioDecoderState_eStarted,
    BOMX_AudioDecoderState_eSuspended,
    BOMX_AudioDecoderState_eMax
};

class BOMX_AudioDecoder : public BOMX_Component
{
public:
    BOMX_AudioDecoder(
        OMX_COMPONENTTYPE *pComponentType,
        const OMX_STRING pName,
        const OMX_PTR pAppData,
        const OMX_CALLBACKTYPE *pCallbacks,
        bool secure=false,
        unsigned numRoles=0,
        const BOMX_AudioDecoderRole *pRoles=NULL);

    virtual ~BOMX_AudioDecoder();

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

    void InputBufferTimeoutCallback();

protected:

    enum ConfigBufferState
    {
        ConfigBufferState_eAccumulating,        // Config buffer has not yet been submitted to playpump
        ConfigBufferState_eFlushed,             // Input port has been flushed with the existing config buffer
        ConfigBufferState_eSubmitted,           // Config buffer has been submited to playpump
        ConfigBufferState_eMax
    };

    NEXUS_AudioDecoderHandle m_hAudioDecoder;
    NEXUS_PlaypumpHandle m_hPlaypump;
    NEXUS_PidChannelHandle m_hPidChannel;
    B_EventHandle m_hPlaypumpEvent;
    B_SchedulerEventId m_playpumpEventId;
    B_SchedulerTimerId m_playpumpTimerId;
    B_EventHandle m_hOutputFrameEvent;
    B_SchedulerEventId m_outputFrameEventId;
    B_SchedulerTimerId m_inputBuffersTimerId;
    unsigned m_submittedDescriptors;
    BOMX_AudioDecoderState m_decoderState;

    BOMX_BufferTracker *m_pBufferTracker;
    unsigned m_AvailInputBuffers;

    NexusIPCClientBase              *m_pIpcClient;
    NexusClientContext              *m_pNexusClient;
    FILE                            *m_pPesFile;
    FILE                            *m_pInputFile;
    FILE                            *m_pOutputFile;
    BOMX_PesFormatter               *m_pPes;

    char m_inputMimeType[OMX_MAX_STRINGNAME_SIZE];
    char m_outputMimeType[OMX_MAX_STRINGNAME_SIZE];

    #define BOMX_AUDIO_EOS_LEN (B_BPP_PACKET_LEN) /* BPP LAST */
    void *m_pEosBuffer;
    bool m_eosPending;
    bool m_eosDelivered;
    bool m_eosReceived;
    bool m_eosStandalone;
    OMX_TICKS m_eosTimeStamp;
    bool m_formatChangePending;
    bool m_secureDecoder;
    bool m_firstFrame;
    BOMX_AudioDecoderRole *m_pRoles;
    unsigned m_numRoles;
    NEXUS_AudioDecoderFrameStatus *m_pFrameStatus;
    NEXUS_MemoryBlockHandle *m_pMemoryBlocks;
    unsigned m_sampleRate;

    #define BOMX_BCMA_HEADER_SIZE (26)
    uint8_t m_pBcmaHeader[BOMX_BCMA_HEADER_SIZE];

    #define BOMX_AUDIO_CODEC_CONFIG_BUFFER_SIZE (1024)
    void *m_pConfigBuffer;
    ConfigBufferState m_configBufferState;
    size_t m_configBufferSize;

    OMX_AUDIO_CODINGTYPE GetCodec() {return m_pAudioPorts[0]->GetDefinition()->format.audio.eEncoding;}
    NEXUS_AudioCodec GetNexusCodec();
    NEXUS_AudioCodec GetNexusCodec(OMX_AUDIO_CODINGTYPE omxType);
    int GetCodecDelay();

    NEXUS_Error SetInputPortState(OMX_STATETYPE newState);
    NEXUS_Error SetOutputPortState(OMX_STATETYPE newState);
    NEXUS_Error StartDecoder();
    NEXUS_Error StopDecoder();

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
            OMX_IN OMX_U8* pBuffer);

    void AddOutputBuffers();
    void RemoveOutputBuffers();
    void PollDecodedFrames();

    OMX_ERRORTYPE BuildInputFrame(
        OMX_BUFFERHEADERTYPE *pBufferHeader,
        bool first,
        unsigned chunkLength,
        uint32_t pts,
        void *pCodecHeader,
        size_t codecHeaderLength,
        NEXUS_PlaypumpScatterGatherDescriptor *pDescriptors,
        unsigned maxDescriptors,
        unsigned *pNumDescriptors
        );

    void CancelTimerId(B_SchedulerTimerId& timerId);

    // These functions are used to pace the input buffers rate
    void InputBufferNew();
    void InputBufferReturned();
    void InputBufferCounterReset();
    void ReturnInputBuffers(OMX_TICKS decodeTs, bool causedByTimeout);
    bool ReturnInputPortBuffer(BOMX_Buffer *pBuffer);

    // The functions below allow derived classes to override them
    virtual NEXUS_Error AllocateInputBuffer(uint32_t nSize, void*& pBuffer);
    virtual void FreeInputBuffer(void*& pBuffer);
    virtual OMX_ERRORTYPE ConfigBufferInit();
    virtual OMX_ERRORTYPE ConfigBufferAppend(const void *pBuffer, size_t length);
    virtual NEXUS_Error OpenPidChannel(uint32_t pid);
    virtual void ClosePidChannel();

private:
// TODO:    BOMX_AUDIO_STATS_DEC;

    void DumpInputBuffer(OMX_BUFFERHEADERTYPE *pHeader);
};

#endif //BOMX_AUDIO_DECODER_H__
