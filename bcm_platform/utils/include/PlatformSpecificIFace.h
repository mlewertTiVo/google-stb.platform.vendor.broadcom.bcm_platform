#ifndef __PLATFORM_SPECIFIC_IFACE_HEADER__
#define __PLATFORM_SPECIFIC_IFACE_HEADER__
#include "nexus_ipc_client_factory.h"

//This is passed to OMX for saving the NxClientContext
class PaltformSpecificContext
{
public:
    virtual bool SetPlatformSpecificContext(NexusClientContext *)=0;
    virtual ~PaltformSpecificContext(){return;}
};


#endif