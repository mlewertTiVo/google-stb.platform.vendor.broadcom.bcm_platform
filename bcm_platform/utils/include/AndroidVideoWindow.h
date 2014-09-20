#ifndef  __ANDROID_VIDEO_WINDOW__HEADER__
#define __ANDROID_VIDEO_WINDOW__HEADER__


#include <cutils/atomic.h>
#include "gralloc_priv.h"
#include "PlatformSpecificIFace.h"
#include "AndroidSpecificIFace.h"
#include <ui/GraphicBufferMapper.h>
#include <ui/GraphicBuffer.h>
#include <media/stagefright/MediaBuffer.h>

#include "BcmDebug.h"

#define LOG_ERROR               ALOGE
#define LOG_CREATE_DESTROY      ALOGE


class AndroidVideoWindow : public PaltformSpecificContext,
                           public SavePlayerSpecificContext 
{
public :
    AndroidVideoWindow ();
    ~AndroidVideoWindow ();
    bool SetPlatformSpecificContext(NexusClientContext *);
    bool SetPrivData(ANativeWindowBuffer *, unsigned int);
    void *GetWindow();

private:
   unsigned int         PlayerID;
   ANativeWindow        *pANW;
   NexusClientContext   *pNxClientContext; 
   bool                 IsVideoWinSet;
   PSHARED_DATA         pSharedData;

   //bool SetVideoWindow(MediaBuffer *buffer);
   bool RemoveVideoWindow();

   /*Hide the operations that you don't support*/
   AndroidVideoWindow(AndroidVideoWindow &CpyFromMe);
   void operator=(AndroidVideoWindow *Rhs);
};


#endif
