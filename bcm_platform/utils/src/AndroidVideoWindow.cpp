#include "AndroidVideoWindow.h"

AndroidVideoWindow::AndroidVideoWindow() 
    : PlayerID (0xffffffff), pANW (NULL), pNxClientContext(NULL),
    IsVideoWinSet(false), pSharedData(NULL)
{
    LOG_CREATE_DESTROY("%s: ENTER",__FUNCTION__);
}

AndroidVideoWindow::~AndroidVideoWindow()
{
    LOG_CREATE_DESTROY("%s: ENTER",__FUNCTION__);
    RemoveVideoWindow();
}

bool
AndroidVideoWindow::SetPlatformSpecificContext(NexusClientContext *NxClintCnxt)
{
    if(pNxClientContext)
    {
        LOG_ERROR("%s: NxClientContext Already Set  == Returning Error ==",__FUNCTION__);
        return false;
    }

    pNxClientContext = NxClintCnxt;
    
    //if( (pANW) && (PlayerID!=0xffffffff) && (pNxClientContext))
    //{
    //    //All the values are set, we will set the video window now
    //    SetVideoWindow();
    //}

    return true;
}

bool
AndroidVideoWindow::SetPrivData(ANativeWindowBuffer *anb, 
                                unsigned int PlID)
{
    LOG_ERROR("%s:  Setting Priv Data Of ANB:%p",__FUNCTION__,anb);
    PlayerID = PlID;

    BCMOMX_DBG_ASSERT(anb);

    private_handle_t * hnd = const_cast<private_handle_t *>
        (reinterpret_cast<private_handle_t const*>(anb->handle));

    PSHARED_DATA pShData = 
        (PSHARED_DATA) NEXUS_OffsetToCachedAddr(hnd->sharedDataPhyAddr);

    pSharedData = pShData;
    pShData->videoWindow.nexusClientContext = pNxClientContext;
    pShData->videoWindow.windowVisible = 1;

    LOG_ERROR("%s: Accessing The Shared Data Now %p",__FUNCTION__,pShData);

    android_atomic_release_store(PlayerID + 1, &pShData->videoWindow.windowIdPlusOne);

    LOG_ERROR("%s: [%d] %dx%d, client_context_handle=%p, sharedDataPhyAddr=0X%x, visible=%d pShData->videoWindow.windowIdPlusOne:%d", __FUNCTION__,
        PlayerID, hnd->width, hnd->height, (void *)pSharedData->videoWindow.nexusClientContext, 
        hnd->sharedDataPhyAddr, 1, pShData->videoWindow.windowIdPlusOne);

    return true;
}

bool 
AndroidVideoWindow::RemoveVideoWindow()
{
    ALOGE("%s: Enter", __FUNCTION__);

    // Ensure the video window is made invisible by the HWC if it is a layer...
    if (pSharedData != NULL && android_atomic_acquire_load(&pSharedData->videoWindow.layerIdPlusOne) != 0) 
    {
        const unsigned timeout = 20;    // 20ms wait
        unsigned cnt = 1000 / timeout;  // 1s wait

		/* JIRA SWANDROID-718, disable waiting for acknowledgement from hwcomposer */
#if 0  
        while (android_atomic_acquire_load(&pSharedData->videoWindow.windowIdPlusOne) != 0 && cnt) 
        {
            LOGD("%s: [%d] Waiting for video window visible to be acknowledged by HWC...", __FUNCTION__, PlayerID);
            usleep(1000*timeout);
            cnt--;
        }

        if (cnt) 
        {
            LOG_ERROR("%s: [%d] Video window visible acknowledged by HWC", __FUNCTION__, PlayerID);
            ALOGE("%s: Exit ", __FUNCTION__);
            return true;
        }else {
            LOG_ERROR("%s: [%d] Video window visible was NOT acknowledged by HWC!", __FUNCTION__, PlayerID);
        }
#endif
    }

    ALOGE("%s: Exit ", __FUNCTION__);
    return false;
}





