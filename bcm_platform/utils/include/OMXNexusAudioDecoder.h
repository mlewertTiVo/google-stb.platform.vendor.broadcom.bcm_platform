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

#ifndef _OMX_NEXUS_AUDIO_DECODER_
#define _OMX_NEXUS_AUDIO_DECODER_

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

#include <utils/Vector.h>

// Length of the buffer queues
#define AUDIO_DECODE_DEPTH          16
#define EOS_DOWN_CNT_DEPTH_AUDIO    16

using namespace android;
using  android::Vector;

#define FAST_INIT_DEC_AUD_FR(__DEC_AUD_FR_)     \
        __DEC_AUD_FR_->BaseAddr=0;              \
        __DEC_AUD_FR_->ClientFlags=0;           \
        __DEC_AUD_FR_->CombinedSz=0;            \
        __DEC_AUD_FR_->usTimeStampOriginal=0;   \
        __DEC_AUD_FR_->usTimeStampIntepolated=0; \
        __DEC_AUD_FR_->FrameData->clear();


typedef struct _NEXUS_DECODED_AUDIO_FRAME_
{
    Vector <NEXUS_AudioMuxOutputFrame *> *FrameData;        // Vector of chunks of data
    unsigned int                        BaseAddr;           // Base address returned by nexus
    unsigned int                        CombinedSz;     // Total size of all the chunks in the frame vector
    unsigned long long                  usTimeStampOriginal;  // Timestamp in us
    unsigned long long                  usTimeStampIntepolated;// Timestamp in us
    unsigned int                        ClientFlags;  // OMX (Or any other) Flags associated with buffer (if Any)
    LIST_ENTRY                          ListEntry;    // Linked List
}NEXUS_DECODED_AUDIO_FRAME,
*PNEXUS_DECODED_AUDIO_FRAME;


class OMXNexusAudioDecoder : public StartDecoderIFace,
                             public FeederEventsListener
{

public:
    OMXNexusAudioDecoder(char *callerName, NEXUS_AudioCodec NxCodec);
    ~OMXNexusAudioDecoder();
    bool StartDecoder(unsigned int);

    bool GetDecodedFrame(PDELIVER_AUDIO_FRAME);


    ErrorStatus GetLastError();
    bool Flush();


    // FeederEventsListener Interface Function To Get the Notification
    // For EOS
    void InputEOSReceived(unsigned int, unsigned long long);

    bool RegisterDecoderEventListener(DecoderEventsListener *);

private:

    typedef struct _FRAME_REPEAT_PARAMS_
    {
        unsigned long long        TimeStamp;
        unsigned long long        TimeStampInterpolated;
    }FRAME_REPEAT_PARAMS,
        *PFRAME_REPEAT_PARAMS;

    NEXUS_SimpleAudioDecoderHandle      AudioDecoHandle;
    NEXUS_SimpleEncoderHandle           EncoderHandle;
    LIST_ENTRY                          EmptyFrList;
    unsigned int                        EmptyFrListLen;

    LIST_ENTRY                          DecodedFrList;
    unsigned int                        DecodedFrListLen;

    ErrorStatus                         LastErr;
    NexusIPCClientBase                  *NxIPCClient;
    NexusClientContext                  *NxClientCntxt;

    bool                                 DecoderStarted;


    //For EOS Detection
    bool                                 StartEOSDetection;
    unsigned int                         ClientFlags;
    unsigned int                         DownCnt;


    DecoderEventsListener                *DecoEvtLisnr;
    //Usage
    //Mutex::Autolock lock(nexSurf->mFreeListLock);
    Mutex   mListLock;

    unsigned int                        FlushCnt;
    FRAME_REPEAT_PARAMS                 FrRepeatParams;
    NEXUS_PidChannelHandle              NxPIDChannel;
	NEXUS_AudioCodec					NxAudioCodec;
    // Our Encoder Keep spewing Out Frames with TS of Zero Even when
    // The input is not there. We need to keep track when to capture
    // and when Stop The Capture.
    bool                                CaptureFrames;
private:
    unsigned int    RetriveFrameFromHardware();
    void            StartCaptureFrames();
    void            StopCaptureFrames();
    bool            CaptureFrameEnabled();

    unsigned int    DecEosDownCnt();
    bool            ShouldDiscardFrame(PNEXUS_DECODED_AUDIO_FRAME pCheckFr);
    bool            GetFrameStart(NEXUS_AudioMuxOutputFrame *,size_t, unsigned int *);

    bool            ReturnDecodedFrameSynchronized(PNEXUS_DECODED_AUDIO_FRAME);
    bool            StartDecoder(NEXUS_PidChannelHandle);
    void            StopDecoder();
    bool            PauseDecoder();
    bool            UnPauseDecoder();
    bool            IsDecoderStarted();
    bool            DetectEOS();
    bool            FlushDecoder();
    void            PrintAudioMuxOutFrame(const NEXUS_AudioMuxOutputFrame *);
    void            PrintFrame(const PNEXUS_DECODED_AUDIO_FRAME);
    void            PrintAudioDecoderStatus();
    void            PrintAudioEncoderStatus();


private:
    /*Hide the operations that you don't support*/
    OMXNexusAudioDecoder(OMXNexusAudioDecoder &);
    void operator=(OMXNexusAudioDecoder *);

};

#endif
