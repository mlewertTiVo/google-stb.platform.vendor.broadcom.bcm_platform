#ifndef _OMX_NEXUS_VIDEO_ENCODER_
#define _OMX_NEXUS_VIDEO_ENCODER_

#include <utils/Vector.h>

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


class OMXNexusVideoEncoder 
{

public:
    
    OMXNexusVideoEncoder (char *callerName);
    
    ~OMXNexusVideoEncoder ();
    bool StartEncoder();        
    bool GetEncodedFrame(PDELIVER_ENCODED_FRAME);


    ErrorStatus GetLastError();

    // FeederEventsListener Interface Function To Get the Notification 
    // For EOS 
    void InputEOSReceived(unsigned int );

private:

    typedef struct _FRAME_REPEAT_PARAMS_
    {
        unsigned long long        TimeStamp;
        unsigned long long        TimeStampInterpolated;
    }FRAME_REPEAT_PARAMS,
        *PFRAME_REPEAT_PARAMS;

    NEXUS_SimpleEncoderHandle           EncoderHandle;

    //Pool Of Empty frames.
    LIST_ENTRY                          EmptyFrList;
    unsigned int                        EmptyFrListLen;

    //FIFO of encoded Frames.
    LIST_ENTRY                          EncodedFrList;
    unsigned int                        EncodedFrListLen;

    ErrorStatus                         LastErr;
    NexusIPCClientBase                  *NxIPCClient;
    NexusClientContext                  *NxClientCntxt;

    bool                                 EncoderStarted;

    //Usage
    //Mutex::Autolock lock(nexSurf->mFreeListLock);
    Mutex   mListLock;
    FRAME_REPEAT_PARAMS                 FrRepeatParams;
    
    // Our Encoder Keep spewing Out Frames with TS of Zero Even when 
    // The input is not there. We need to keep track when to capture 
    // and when Stop The Capture.
    bool                                CaptureFrames;
private: 
    unsigned int    RetriveFrameFromHardware();
    void            StartCaptureFrames();
    void            StopCaptureFrames();
    bool            CaptureFrameEnabled();

    bool            ShouldDiscardFrame(PNEXUS_ENCODED_VIDEO_FRAME pCheckFr);
    bool            GetFrameStart(NEXUS_VideoEncoderDescriptor *,size_t, unsigned int *);
    
    bool            ReturnEncodedFrameSynchronized(PNEXUS_ENCODED_VIDEO_FRAME);
    void            StopEncoder();
    bool            IsEncoderStarted();
//    bool            DetectEOS();
//    bool            FlushDecoder();
//    void            PrintAudioMuxOutFrame(const NEXUS_AudioMuxOutputFrame *);
//    void            PrintFrame(const PNEXUS_DECODED_AUDIO_FRAME); 
    void            PrintVideoEncoderStatus();


private:
    /*Hide the operations that you don't support*/
    OMXNexusVideoEncoder(OMXNexusVideoEncoder &);
    void operator=(OMXNexusVideoEncoder *);

};

#endif
