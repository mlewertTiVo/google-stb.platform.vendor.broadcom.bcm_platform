#ifndef  __ANDROID_SPECIFIC_IFACE_HEADER__
#define __ANDROID_SPECIFIC_IFACE_HEADER__
#include <ui/GraphicBufferMapper.h>
#include <ui/GraphicBuffer.h>
#include <media/stagefright/MediaBuffer.h>

//This will be passed to AwesomePlayer to do its Job.
class SavePlayerSpecificContext
{
public:
    virtual bool SetPrivData(ANativeWindowBuffer *, unsigned int)=0;
    virtual ~SavePlayerSpecificContext(){return;}
};


#endif