#ifndef _RETURN_BUFFER_HEADER_
#define _RETURN_BUFFER_HEADER_

#include "AndroidSpecificIFace.h"

typedef bool (*DISPLAY_FUNCTION) (void *, void *);

typedef struct _DISPLAY_FRAME_
{
    DISPLAY_FUNCTION                        pDisplayFn;
    void *                                  DecodedFr;
    void *                                  DisplayCnxt;
    unsigned int                            OutFlags;
}DISPLAY_FRAME,
*PDISPLAY_FRAME;
 

typedef struct _DELIVER_AUDIO_FRAME_
{
    void *                  pAudioDataBuff;
    unsigned int            SzAudDataBuff;
    unsigned int            SzFilled;
    unsigned int            usTimeStamp;
    unsigned int            OutFlags;
}DELIVER_AUDIO_FRAME,
*PDELIVER_AUDIO_FRAME;

#ifdef BCM_OMX_SUPPORT_ENCODER
typedef struct _DELIVER_ENCODED_FRAME_
{
    void *                  pVideoDataBuff;
    unsigned int            SzVidDataBuff;
    unsigned int            SzFilled;
    unsigned int            usTimeStamp;
    unsigned int            OutFlags;
}DELIVER_ENCODED_FRAME,
*PDELIVER_ENCODED_FRAME;
#endif
#endif