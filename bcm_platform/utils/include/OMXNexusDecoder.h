#ifndef _OMX_NEXUS_VIDEO_DECODER_
#define _OMX_NEXUS_VIDEO_DECODER_
 
#include <utils/Log.h>
#include <utils/threads.h> // Mutex Class operations
 
#include "nexus_types.h"
#include "nexus_platform.h"
#include "nexus_ipc_client_factory.h"
#include "nexus_playback.h"
#include "nexus_base_mmap.h"
#include "ReturnBuffer.h"
#include "LinkedList.h"
#include "MiscIFace.h"
#include "BcmDebug.h"
#include "ErrorStatus.h"
#include "PlatformSpecificIFace.h"

// Length of the buffer queues 
#define DECODE_DEPTH                    16
#define DOWN_CNT_DEPTH                  16


using namespace android;
//using android::Vector;


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
    OMXNexusDecoder(    char * CallerName, 
                        NEXUS_VideoCodec NxVideoCodec,
                        PaltformSpecificContext *);

    ~OMXNexusDecoder();

    void CheckAndClearBufferState(PDISPLAY_FRAME);
    bool DisplayFrame(PDISPLAY_FRAME);
    bool GetFramesFromHardware();
    bool GetDecodedFrame(PDISPLAY_FRAME);
    unsigned GetFrameTimeStampMs(PDISPLAY_FRAME);

    ErrorStatus GetLastError();
    bool RegisterDecoderEventListener(DecoderEventsListener *);    
    bool Flush();

// Implement DecoderEventsIFace
    bool StartDecoder(unsigned int);
    
//Implement InputEOSListener
    void InputEOSReceived(unsigned int);
    bool GetDecoSts(NEXUS_VideoDecoderStatus *);

private:

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
    // Else we will have to scan the queue everytime
    unsigned int                    NextExpectedFr;

//Data FOR Special Conditions Like EOS 
    unsigned int                    ClientFlags;        //Like EOS or something

    //EOS Occureed On Input Side, Look For Or Generate EOS On ouput side
    bool                            StartEOSDetection;  
    unsigned int                    DownCnt;

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
    void StopDecoder();
    bool StartDecoder(NEXUS_PidChannelHandle videoPIDChannel);
    bool PauseDecoder();
    bool UnPauseDecoder();
    bool DropFrame(PNEXUS_DECODED_FRAME);
    bool FrameAlreadyExist(PLIST_ENTRY,PNEXUS_DECODED_FRAME);
    bool IsDecoderStarted();
    bool ReturnFrameSynchronized(PNEXUS_DECODED_FRAME,bool FlagDispFrame=false);
    bool DetectEOS();
    void PrintDecoStats();
    

    /*Hide the operations that you don't support*/
    OMXNexusDecoder(OMXNexusDecoder &CpyFromMe);
    void operator=(OMXNexusDecoder *Rhs);
    
};


bool
DisplayBuffer (void *, void *);
#endif