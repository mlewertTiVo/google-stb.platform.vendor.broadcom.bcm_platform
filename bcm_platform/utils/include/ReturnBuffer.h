#ifndef _RETURN_BUFFER_HEADER_
#define _RETURN_BUFFER_HEADER_

#include "gralloc_priv.h"

static 
PDISPLAY_FRAME
GetDisplayFrameFromANB(ANativeWindowBuffer *anb)
{
    private_handle_t * hnd = const_cast<private_handle_t *>
        (reinterpret_cast<private_handle_t const*>(anb->handle));

    PSHARED_DATA pShData = 
        (PSHARED_DATA) NEXUS_OffsetToCachedAddr(hnd->sharedDataPhyAddr);

    return &pShData->DisplayFrame;
}

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