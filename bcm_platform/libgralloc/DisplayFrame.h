#ifndef __DISPLAY_FRAME_HEADER__
#define __DISPLAY_FRAME_HEADER__

typedef bool (*DISPLAY_FUNCTION) (void *, void *);

typedef struct _DISPLAY_FRAME_
{
    DISPLAY_FUNCTION                        pDisplayFn;
    void *                                  DecodedFr;
    void *                                  DisplayCnxt;
    unsigned int                            OutFlags;
}DISPLAY_FRAME,
*PDISPLAY_FRAME;

#endif
