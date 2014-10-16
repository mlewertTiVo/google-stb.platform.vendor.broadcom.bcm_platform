/******************************************************************************
* (c) 2014 Broadcom Corporation
*
* This program is the proprietary software of Broadcom Corporation and/or its
* licensors, and may only be used, duplicated, modified or distributed pursuant
* to the terms and conditions of a separate, written license agreement executed
* between you and Broadcom (an "Authorized License").  Except as set forth in
* an Authorized License, Broadcom grants no license (express or implied), right
* to use, or waiver of any kind with respect to the Software, and Broadcom
* expressly reserves all rights in and to the Software and all intellectual
* property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
* HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
* NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
*
* Except as expressly set forth in the Authorized License,
*
* 1. This program, including its structure, sequence and organization,
*    constitutes the valuable trade secrets of Broadcom, and you shall use all
*    reasonable efforts to protect the confidentiality thereof, and to use
*    this information only in connection with your use of Broadcom integrated
*    circuit products.
*
* 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
*    AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
*    WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT
*    TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED
*    WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A
*    PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET
*    ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME
*    THE ENTIRE RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
*
* 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
*    LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT,
*    OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO
*    YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN
*    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS
*    OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER
*    IS GREATER. THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF
*    ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
******************************************************************************/

#ifndef _OMX_NEXUS_VIDEO_ENCODER_
#define _OMX_NEXUS_VIDEO_ENCODER_

#include "nexus_types.h"
#include "nexus_platform.h"
#include "nexus_ipc_client_factory.h"
#include "nexus_playback.h"
#include "nexus_base_mmap.h"
#include "bioatom.h"
#include "ReturnBuffer.h"
#include "LinkedList.h"
#include "MiscIFace.h"
#include "BcmDebug.h"
#include "ErrorStatus.h"
#include "OMX_IVCommon.h"
#include "OMX_Video.h"

#include <utils/Vector.h>

#define FRAME_TYPE_T NEXUS_VideoEncoderDescriptor

// Length of the buffer queues
#define VIDEO_ENCODE_DEPTH              16
#define EOS_DOWN_CNT_DEPTH_ENCODE       16

using namespace android;
using  android::Vector;

#define FAST_INIT_ENC_VID_FR(_ENC_FR_)     \
        _ENC_FR_->BaseAddr=0;              \
        _ENC_FR_->ClientFlags=0;           \
        _ENC_FR_->CombinedSz=0;            \
        _ENC_FR_->usTimeStampOriginal=0;   \
        _ENC_FR_->usTimeStampIntepolated=0; \
        _ENC_FR_->FrameData->clear();

typedef enum _Encode_Frame_Type_
{
    Encode_Frame_Type_SPS = 0,
    Encode_Frame_Type_PPS = 1,
    Encode_Frame_Type_Picture = 2
} Encode_Frame_Type;

typedef struct _NEXUS_ENCODED_VIDEO_FRAME_
{
    Vector < FRAME_TYPE_T *>            *FrameData;     // Vector of chunks of data
    unsigned int                        BaseAddr;       // Base address returned by nexus
    unsigned int                        CombinedSz;     // Total size of all the chunks in the frame vector
    unsigned long long                  usTimeStampOriginal;  // Timestamp in us
    unsigned long long                  usTimeStampIntepolated;// Timestamp in us
    unsigned int                        ClientFlags;  // OMX (Or any other) Flags associated with buffer (if Any)
    LIST_ENTRY                          ListEntry;    // Linked List
}NEXUS_ENCODED_VIDEO_FRAME,
*PNEXUS_ENCODED_VIDEO_FRAME;

typedef void (*ENCODER_BUFFER_DONE_CALLBACK) (unsigned int, unsigned int,unsigned int);

typedef struct ENCODER_DONE_CONTEXT_
{
    unsigned int                    Param1;
    unsigned int                    Param2;
    unsigned int                    Param3;
    ENCODER_BUFFER_DONE_CALLBACK    pFnDoneCallBack;
}ENCODER_DONE_CONTEXT;

typedef struct _NEXUS_SURFACE_
{
    NEXUS_SurfaceHandle             handle;
    LIST_ENTRY                      ListEntry;
} NEXUS_SURFACE, *PNEXUS_SURFACE;

typedef struct _NEXUS_VIDEO_ENCODER_INPUT_CONTEXT_
{
    unsigned char                   *bufPtr;   // Pointer to uncompressed input data buffer
    unsigned int                    bufSize;   // Size of uncompressed input data buffer
    OMX_COLOR_FORMATTYPE            colorFormat;
    OMX_U32                         flags;
    unsigned int                    width;
    unsigned int                    height;
    PNEXUS_SURFACE                  pNxSurface;
    unsigned long long              uSecTS;
    ENCODER_BUFFER_DONE_CALLBACK    pFnDoneCallBack;
    ENCODER_DONE_CONTEXT            DoneContext;
    LIST_ENTRY                      ListEntry;
}NEXUS_VIDEO_ENCODER_INPUT_CONTEXT,
*PNEXUS_VIDEO_ENCODER_INPUT_CONTEXT;

typedef struct _VIDEO_ENCODER_START_PARAMS_
{
    unsigned int                    width;
    unsigned int                    height;
    NEXUS_VideoFrameRate            frameRate;
    OMX_VIDEO_PARAM_AVCTYPE         avcParams;
} VIDEO_ENCODER_START_PARAMS,
*PVIDEO_ENCODER_START_PARAMS;

class OMXNexusVideoEncoder
{

public:
    
    OMXNexusVideoEncoder (const char *callerName, int numInBuf);
    
    ~OMXNexusVideoEncoder ();
    bool StartEncoder(PVIDEO_ENCODER_START_PARAMS startParams);
    bool StartInput(PVIDEO_ENCODER_START_PARAMS startParams);
    bool StartOutput(PVIDEO_ENCODER_START_PARAMS startParams);
    void StopEncoder();
    void StopInput();
    void StopOutput();
    bool FlushInput();
    bool FlushOutput();
    bool EncodeFrame(PNEXUS_VIDEO_ENCODER_INPUT_CONTEXT pNxInputContext);
    bool GetEncodedFrame(PDELIVER_ENCODED_FRAME);


    ErrorStatus GetLastError();

    void imageBufferCallback();

private:

    typedef struct _FRAME_REPEAT_PARAMS_
    {
        unsigned long long        TimeStamp;
        unsigned long long        TimeStampInterpolated;
    }FRAME_REPEAT_PARAMS,
        *PFRAME_REPEAT_PARAMS;

    NEXUS_SimpleEncoderHandle           EncoderHandle;
    NEXUS_SimpleVideoDecoderHandle      DecoderHandle;
    NEXUS_SimpleStcChannelHandle        StcChannel;
    NEXUS_VideoImageInputHandle         ImageInput;
    PNEXUS_SURFACE                      *pSurface;
    unsigned int                        NumNxSurfaces;

    //Pool Of Empty frames.
    LIST_ENTRY                          EmptyFrList;
    unsigned int                        EmptyFrListLen;

    //FIFO of encoded Frames.
    LIST_ENTRY                          EncodedFrList;
    unsigned int                        EncodedFrListLen;

    LIST_ENTRY                          SurfaceBusyList;
    LIST_ENTRY                          SurfaceAvailList;
    unsigned int                        SurfaceAvailListLen;

    LIST_ENTRY                          InputContextList;

    ErrorStatus                         LastErr;
    NexusIPCClientBase                  *NxIPCClient;
    NexusClientContext                  *NxClientCntxt;

    bool                                EncoderStarted;
    bool                                CodecConfigDone;
    //Usage
    //Mutex::Autolock lock(nexSurf->mFreeListLock);
    Mutex   mListLock;
    FRAME_REPEAT_PARAMS                 FrRepeatParams;

    // Our Encoder Keep spewing Out Frames with TS of Zero Even when
    // The input is not there. We need to keep track when to capture
    // and when Stop The Capture.
    bool                                CaptureFrames;
    DumpData                            *pdumpES;
    VIDEO_ENCODER_START_PARAMS          EncoderStartParams;
private:
    unsigned int    RetriveFrameFromHardware();
    void            StartCaptureFrames();
    void            StopCaptureFrames();
    bool            CaptureFrameEnabled();

    bool            ShouldDiscardFrame(PNEXUS_ENCODED_VIDEO_FRAME pCheckFr);
    bool            GetFrameStart(NEXUS_VideoEncoderDescriptor *,size_t, Encode_Frame_Type, unsigned int *);

    bool            ReturnEncodedFrameSynchronized(PNEXUS_ENCODED_VIDEO_FRAME);
    bool            IsEncoderStarted();
//    bool            DetectEOS();
//    bool            FlushDecoder();
//    void            PrintAudioMuxOutFrame(const NEXUS_AudioMuxOutputFrame *);
//    void            PrintFrame(const PNEXUS_DECODED_AUDIO_FRAME);
    void            PrintVideoEncoderStatus();

};

#endif
