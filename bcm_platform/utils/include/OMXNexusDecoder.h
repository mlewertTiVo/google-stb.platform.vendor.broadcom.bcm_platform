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

#ifndef _OMX_NEXUS_VIDEO_DECODER_
#define _OMX_NEXUS_VIDEO_DECODER_

#include <utils/List.h>
#include <utils/threads.h> // Mutex Class operations

#include "nexus_types.h"
#include "nexus_platform.h"
#include "nexus_ipc_client_factory.h"
#include "nexus_playback.h"
#include "nexus_base_mmap.h"
#include "AndroidSpecificIFace.h"
#include "ReturnBuffer.h"
#include "LinkedList.h"
#include "MiscIFace.h"
#include "BcmDebug.h"
#include "ErrorStatus.h"
#include "PlatformSpecificIFace.h"

//#define GENERATE_DUMMY_EOS

// Length of the buffer queues
#define DECODE_DEPTH                    16
#define DOWN_CNT_DEPTH                  16


#define MIN_BUFFER_TO_HOLD_IN_DECODE_QUEUE  1
#define MAX_FRAME_SEQ_NUMBER                0xffffffff
#define STARTUP_TIME_INVALID                0xffffffff

using namespace android;
//using android::Vector;

#define SET_BIT(_X_, BIT)       (_X_ |= (1<<BIT))
#define CLEAR_BIT (_X_, BIT)    (_X_ &= ~(1<<BIT))
#define TOGGLE_BIT(_X_, BIT)    (_X_ ^= (1<<BIT))
#define TEST_BIT(_X_, BIT)      (_X_ & (1<<BIT))

typedef enum __BufferState__
{
    BufferState_Init=0,         // When the frame is in free list
    BufferState_Decoded=0x01,   // When the frame is in decoded list
    BufferState_Delivered=0x02  // When the frame is delivered to client
}BufferState;


class OMXNexusDecoder : public StartDecoderIFace,
                        public FeederEventsListener
{
public:
    OMXNexusDecoder(    char const * CallerName,
                        NEXUS_VideoCodec NxVideoCodec,
                        PaltformSpecificContext *);

    ~OMXNexusDecoder();

    void CheckAndClearBufferState(PDISPLAY_FRAME);
    bool DisplayFrame(PDISPLAY_FRAME);
    bool HaveDecodedFrames();
    bool GetFramesFromHardware();
    bool GetDecodedFrame(PDISPLAY_FRAME);
    unsigned GetFrameTimeStampMs(PDISPLAY_FRAME);
    void SetStartupTime(unsigned int);
    ErrorStatus GetLastError();
    bool RegisterDecoderEventListener(DecoderEventsListener *);
    bool Flush();

// Implement StartDecoderIFace
    bool StartDecoder(unsigned int);
    void StopDecoder();

//Implement InputEOSListener
    void InputEOSReceived(unsigned int,unsigned long long);
    bool GetDecoSts(NEXUS_VideoDecoderStatus *);

//Source Change CallBack from Decoder
    void SourceChangedCallback();
    unsigned int GetDecoderID();

private:
    void AddDecoderToList();
    void RemoveDecoderFromList();
    static List<OMXNexusDecoder *> DecoderInstanceList;
    static Mutex        mDecoderIniLock;
    unsigned int        DecoderID;
    typedef enum _EOS_States_
    {
        EOS_Init =0,
        EOS_ReceivedOnInput=1,
        EOS_ReceivedFromHardware=2,
        EOS_DeliveredOnOutput=3,
    }EOS_States;

    //EOS Occureed On Input Side, Look For Or Generate EOS On Output side
    unsigned    EOSState;

    inline bool IsEOSComplete()
    {
        return  ( (TEST_BIT(EOSState, EOS_ReceivedOnInput) &&
                     TEST_BIT(EOSState, EOS_ReceivedFromHardware) &&
                     TEST_BIT(EOSState, EOS_DeliveredOnOutput)) ? true:false) ;
    }

    inline bool IsEOSReceivedOnInput()
    {
        return TEST_BIT(EOSState, EOS_ReceivedOnInput) ? true:false;
    }

    inline bool IsEOSReceivedFromHardware()
    {
        return TEST_BIT(EOSState,EOS_ReceivedFromHardware) ? true:false;
    }

    inline bool IsEOSDeliveredOnOutput()
    {
        return TEST_BIT(EOSState,EOS_DeliveredOnOutput) ? true:false;
    }

    inline void ReSetEOSState()
    {
        EOSState=EOS_Init;
    }

    inline void EOSReceivedOnInput()
    {
        SET_BIT(EOSState, EOS_ReceivedOnInput);
        return;
    }

    inline void EOSReceivedFromHardware()
    {
        SET_BIT(EOSState, EOS_ReceivedFromHardware);
        return;
    }

    inline void EOSDeliveredToOutput()
    {
        SET_BIT(EOSState, EOS_DeliveredOnOutput);
        return;
    }

    typedef struct _NEXUS_DECODED_FRAME_
    {
        BufferState                     BuffState;
        NEXUS_VideoDecoderFrameStatus   FrStatus;
        unsigned long long              MilliSecTS;
        unsigned int                    ClientFlags; //Used for Sending the Client Data with frames
        LIST_ENTRY                      ListEntry;
    }NEXUS_DECODED_FRAME,
    *PNEXUS_DECODED_FRAME;

    //Listeners
    DecoderEventsListener           *DecoEvtLisnr;

    NEXUS_SimpleVideoDecoderHandle  decoHandle;
    NexusIPCClientBase              *ipcclient;
    NexusClientContext              *nexus_client;

    // Just to make sure that you get >=
    // frame every time and not lower.
    // Else we will have to scan the queue every time
    unsigned int                    NextExpectedFr;

//Data FOR Special Conditions Like EOS
    unsigned int                    ClientFlags;        // Like EOS or something
    unsigned long long              EOSFrameKey;        // Something that identifies the EOS frame

//Startup Time for The Frames That We Want To Start Delivering with
    unsigned int                    StartupTime;        // Specifically 32-Bit becuase Our Hardware Expects 32-bit

#ifdef GENERATE_DUMMY_EOS
    unsigned int            DownCnt;
#endif

    //Usage
    //Mutex::Autolock lock(nexSurf->mFreeListLock);
    Mutex   mListLock;

    LIST_ENTRY          EmptyFrList;
    unsigned int        EmptyFrListLen;


    LIST_ENTRY          DecodedFrList;
    unsigned int        DecodedFrListLen;

    LIST_ENTRY          DeliveredFrList;
    ErrorStatus         LastErr;

    unsigned int        DebugCounter;

// Data For Statistic Parameters
   unsigned int        FlushCnt;

   unsigned int         VideoPid;
   bool                 FlushDecoder();
   NEXUS_VideoCodec     NxVideoCodec;

private:

#ifndef GENERATE_DUMMY_EOS
    void OnEOSMoveAllDecodedToDeliveredList();
#endif

    bool StartDecoder(NEXUS_PidChannelHandle videoPIDChannel);
    bool PauseDecoder();
    bool UnPauseDecoder();
    bool DropFrame(PNEXUS_DECODED_FRAME);
    bool FrameAlreadyExist(PLIST_ENTRY,PNEXUS_DECODED_FRAME);
    bool IsDecoderStarted();
    bool ReturnFrameSynchronized(PNEXUS_DECODED_FRAME,bool FlagDispFrame=false);

#ifdef GENERATE_DUMMY_EOS
    bool DetectEOS();
#else
    unsigned int DetectEOS(PNEXUS_DECODED_FRAME pNxDecFr);
#endif

    void PrintDecoStats();

    bool ReturnPacketsAfterEOS();

    /*Hide the operations that you don't support*/
    OMXNexusDecoder(OMXNexusDecoder &CpyFromMe);
    void operator=(OMXNexusDecoder *Rhs);

};


bool
DisplayBuffer (void *, void *);
#endif
