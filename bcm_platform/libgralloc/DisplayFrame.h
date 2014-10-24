#ifndef __DISPLAY_FRAME_HEADER__
#define __DISPLAY_FRAME_HEADER__

#include "nexus_simple_video_decoder.h"

typedef bool (*DISPLAY_FUNCTION) (void *, void *);

typedef struct _DISPLAY_FRAME_
{
    DISPLAY_FUNCTION                        pDisplayFn;
    void *                                  DecodedFr;
    void *                                  DisplayCnxt;
    unsigned int                            OutFlags;
    NEXUS_VideoDecoderFrameStatus frameStatus;
    bool display;    
}DISPLAY_FRAME,
*PDISPLAY_FRAME;

#endif
