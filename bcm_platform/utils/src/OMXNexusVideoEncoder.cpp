#include "OMXNexusVideoEncoder.h"

#undef LOG_TAG
#define LOG_TAG "BCM_OMX_VIDEO_ENC"

//Path level Debug Messages
#define LOG_START_STOP_DBG      ALOGD
#define LOG_EOS_DBG             ALOGD         
#define LOG_FLUSH_MSGS          ALOGD
#define LOG_CREATE_DESTROY      ALOGD

//Path independent Debug Messages
#define LOG_WARNING             ALOGD
#define LOG_ERROR               ALOGD
#define LOG_INFO                

OMXNexusVideoEncoder::OMXNexusVideoEncoder(char *CallerName)
    : EncoderHandle(NULL),
    LastErr(ErrStsSuccess),EmptyFrListLen(0),EncodedFrListLen(0),
    EncoderStarted(false),
    CaptureFrames(true)
{
    LOG_CREATE_DESTROY("%s: ENTER ===",__FUNCTION__);
    InitializeListHead(&EmptyFrList);
    InitializeListHead(&EncodedFrList);

    b_refsw_client_client_configuration         config;
    b_refsw_client_client_info                  client_info;
    b_refsw_client_connect_resource_settings    connectSettings;
    NxIPCClient = NexusIPCClientFactory::getClient(CallerName);

    BKNI_Memset(&config, 0, sizeof(config));
    BKNI_Snprintf(config.name.string,sizeof(config.name.string),CallerName);
    
    config.resources.encoder = true;
    config.resources.audioDecoder = false;
    config.resources.audioPlayback = false;            
    config.resources.videoDecoder = false;
    config.resources.screen.required = false;   

    NxClientCntxt = NxIPCClient->createClientContext(&config);

    if (NULL == NxClientCntxt) 
    {
        LOG_ERROR("%s: Could not create client \"%s\" context!", 
                  __FUNCTION__, 
                  NxIPCClient->getClientName());

        LastErr = ErrStsClientCreateFail;
        return;
    }else {
        LOG_CREATE_DESTROY("%s: OMX client \"%s\" context created successfully: client=0x%p", 
                 __FUNCTION__, NxIPCClient->getClientName(), NxClientCntxt);
    }

    // Now connect the client resources
    NxIPCClient->getClientInfo(NxClientCntxt, &client_info);
    NxIPCClient->getDefaultConnectClientSettings(&connectSettings);

    connectSettings.simpleEncoder[0].id = client_info.encoderId;
    connectSettings.simpleEncoder[0].video.cpuAccessible = true;
	connectSettings.simpleEncoder[0].display = true;

    if (true != NxIPCClient->connectClientResources(NxClientCntxt, &connectSettings)) 
    {
        LOG_ERROR("%s: Could not connect client \"%s\" resources!",
            __FUNCTION__, NxIPCClient->getClientName());

        LastErr = ErrStsConnectResourcesFail;
        return;
    }else{
        LOG_CREATE_DESTROY("%s: Connect Client \"%s\" Resources Success!",
            __FUNCTION__, NxIPCClient->getClientName());
    }


    EncoderHandle = NxIPCClient->acquireSimpleEncoderHandle();

    if ( NULL == EncoderHandle )
    {
        LOG_ERROR("%s Unable to acquire Simple Audio Encoder \n",__FUNCTION__);
        LastErr = ErrStsDecoAcquireFail;
        return;
    }else{
        LOG_CREATE_DESTROY("%s: Acquired Encoder Handle", __FUNCTION__);
    }

    FrRepeatParams.TimeStamp=0;
    FrRepeatParams.TimeStampInterpolated=0;

    for (unsigned int i=0; i < VIDEO_ENCODE_DEPTH; i++) 
    {
        PNEXUS_ENCODED_VIDEO_FRAME pEncoFr = (PNEXUS_ENCODED_VIDEO_FRAME) 
                                                    malloc(sizeof(NEXUS_ENCODED_VIDEO_FRAME));
        if (!pEncoFr) 
        {
            LastErr = ErrStsOutOfResource;
            return;
        }
        
        pEncoFr->FrameData = new Vector <NEXUS_VideoEncoderDescriptor *>();
        FAST_INIT_ENC_VID_FR(pEncoFr);
        InitializeListHead(&pEncoFr->ListEntry);
        InsertHeadList(&EmptyFrList,&pEncoFr->ListEntry);
        EmptyFrListLen++;
        BCMOMX_DBG_ASSERT(EmptyFrListLen <= VIDEO_ENCODE_DEPTH);
    }

    LastErr = ErrStsSuccess;
    LOG_CREATE_DESTROY("%s: OMXNexusVideoEncoder Created!!",__FUNCTION__);

}

OMXNexusVideoEncoder::~OMXNexusVideoEncoder()
{
    LOG_CREATE_DESTROY("%s: ",__FUNCTION__);
    unsigned int NumEntriesFreed=0;
    if(IsEncoderStarted())
    {
        LOG_CREATE_DESTROY("%s: Stopping Encoder",__FUNCTION__);
        StopEncoder();
    }

    NxIPCClient->disconnectClientResources(NxClientCntxt);

    if (EncoderHandle) 
    {
        LOG_CREATE_DESTROY("%s: Releasing Video Encoder",__FUNCTION__);
        NEXUS_SimpleEncoder_Release(EncoderHandle); 
        EncoderHandle=NULL;
    }
    
    if (NxClientCntxt) 
    {
        LOG_CREATE_DESTROY("%s: Destroying Client Context",__FUNCTION__);
        NxIPCClient->destroyClientContext(NxClientCntxt); 
        NxClientCntxt=NULL;
    }

    //Free the entries from the the Free list
    LOG_CREATE_DESTROY("%s: Freeing Empty Frame List",__FUNCTION__);
    while (!IsListEmpty(&EmptyFrList)) 
    {
        PLIST_ENTRY ThisEntry =  RemoveTailList(&EmptyFrList);
        EmptyFrListLen--;
        PNEXUS_ENCODED_VIDEO_FRAME pEncoFr = CONTAINING_RECORD(ThisEntry,NEXUS_ENCODED_VIDEO_FRAME,ListEntry);
        if (pEncoFr->FrameData) delete pEncoFr->FrameData;
        NumEntriesFreed++;
        free(pEncoFr);
    }
    BCMOMX_DBG_ASSERT(0==EmptyFrListLen);

    LOG_CREATE_DESTROY("%s: Freeing Encoded Frame List",__FUNCTION__);
    //Remaining in  list goes away
    while (!IsListEmpty(&EncodedFrList))
    {
        PLIST_ENTRY ThisEntry =  RemoveTailList(&EncodedFrList);
        EncodedFrListLen--;
        PNEXUS_ENCODED_VIDEO_FRAME pEncoFr = CONTAINING_RECORD(ThisEntry,NEXUS_ENCODED_VIDEO_FRAME,ListEntry);
        if (pEncoFr->FrameData) delete pEncoFr->FrameData;
        NumEntriesFreed++;
        free(pEncoFr);
    }

    BCMOMX_DBG_ASSERT(NumEntriesFreed==VIDEO_ENCODE_DEPTH);
    LOG_CREATE_DESTROY("%s: Deleting The NxIPCClient",__FUNCTION__);
    delete NxIPCClient;
}


bool
OMXNexusVideoEncoder::StartEncoder()
{
    NEXUS_Error errCode;
    NEXUS_SimpleEncoderStartSettings EncoderStartSettings;

    if (EncoderStarted) 
    {
        LOG_START_STOP_DBG("%s[%d]: Encoder Already Started",
                           __FUNCTION__, __LINE__);

        LastErr = ErrStsStartEncoFailed;
        return false;
    }
    
    NEXUS_SimpleEncoder_GetDefaultStartSettings(&EncoderStartSettings);
    EncoderStartSettings.input.display = true;
    EncoderStartSettings.output.video.settings.codec = NEXUS_VideoCodec_eH264;
	EncoderStartSettings.output.transport.type = NEXUS_TransportType_eEs;

//TODO: Revisit
#if 0
    startSettings.output.video.settings.bounds.inputDimension.max.width  = width;
    startSettings.output.video.settings.bounds.inputDimension.max.height = height;
    /* higher frame rate has lower delay; TODO: may set encode input min if display refresh rate > encode framerate to reduce latency */
    startSettings.output.video.settings.bounds.inputFrameRate.min        = encoderSettings.videoEncoder.frameRate;
    startSettings.output.video.settings.bounds.outputFrameRate.min       = encoderSettings.videoEncoder.frameRate;
    startSettings.output.video.settings.bounds.streamStructure.max.framesB = 0;
#endif


    errCode = NEXUS_SimpleEncoder_Start(EncoderHandle, &EncoderStartSettings);

    if (NEXUS_SUCCESS != errCode) 
    {
        LOG_ERROR("%s[%d]: NEXUS_SimpleEncoder_Start Failed [Err:%d]!!",
            __FUNCTION__, __LINE__, errCode);

        LastErr = ErrStsStartEncoFailed;
        return false;
    }else{
        LOG_START_STOP_DBG("%s[%d]: SimpleEncoder Started Successfully [Err:%d]!!",
            __FUNCTION__, __LINE__, errCode);
    }

    LastErr = ErrStsSuccess;
    EncoderStarted = true;
    StartCaptureFrames();
    return true;
}

void 
OMXNexusVideoEncoder::StopEncoder()
{
    if (EncoderStarted) 
    {
        NEXUS_SimpleEncoder_Stop(EncoderHandle);

        LOG_START_STOP_DBG("%s[%d]: Video Encoder Stopped!!",
                           __FUNCTION__, __LINE__);

        EncoderStarted=false;
        LastErr = ErrStsSuccess;
    }else{

        LOG_START_STOP_DBG("%s[%d]: Stop Called When Video Encoder Already Stopped!!",
                           __FUNCTION__, __LINE__);

        LastErr = ErrStsDecoNotStarted;
    }
    
    return;
}


bool
OMXNexusVideoEncoder::IsEncoderStarted()
{
    return EncoderStarted;
}

void 
OMXNexusVideoEncoder::PrintVideoEncoderStatus() 
{
    NEXUS_SimpleEncoderStatus EncSts;
    NEXUS_SimpleEncoder_GetStatus(EncoderHandle,&EncSts);
       
    LOG_INFO("%s[%d]: pBaseAddr:%p pMetaDataBuff:%p picturesReceived:%d picturesEncoded:%d picturesDroppedFRC:%d picturesDroppedHRD:%d picturesDroppedErrors:%d pictureIdLastEncoded:%d",
             __FUNCTION__,__LINE__, 
             EncSts.video.pBufferBase,
             EncSts.video.pMetadataBufferBase,
             EncSts.video.picturesReceived,
             EncSts.video.picturesEncoded,
             EncSts.video.picturesDroppedFRC,
             EncSts.video.picturesDroppedHRD,
             EncSts.video.picturesDroppedErrors,
             EncSts.video.pictureIdLastEncoded
             );

}

bool
OMXNexusVideoEncoder::GetEncodedFrame(PDELIVER_ENCODED_FRAME pDeliverFr)
{
    Mutex::Autolock lock(mListLock);

    PrintVideoEncoderStatus();
    RetriveFrameFromHardware();

    if (!IsEncoderStarted()) 
    {
        LOG_ERROR("%s %d: Encoder Is Not Started",__FUNCTION__,__LINE__);
        LastErr= ErrStsEncoNotStarted;
        return false;
    }

    if ( (!pDeliverFr) ||  (!pDeliverFr->pVideoDataBuff) || (!pDeliverFr->SzVidDataBuff))
    {
        LOG_ERROR("%s: Invalid Parameters",__FUNCTION__);
        LastErr = ErrStsInvalidParam;
        return false;
    }

//TODO: Enable this EOS code on need basis.
#if 0
    //Check For the EOS First
    if (IsListEmpty(&EncodedFrList)) 
    {
        BCMOMX_DBG_ASSERT(0==DecodedFrListLen);
        if(DetectEOS())
        {
            LOG_EOS_DBG("%s: Delivering EOS Frame ClientFlags[%d]",__FUNCTION__,ClientFlags);

            pDeliverFr->OutFlags = ClientFlags;ClientFlags=0;
            pDeliverFr->SzFilled=0;
            pDeliverFr->usTimeStamp=0;
            if (DecoEvtLisnr) DecoEvtLisnr->EOSDelivered();
        }

        LastErr = ErrStsDecodeListEmpty;
        return false;
    }
#endif

    if (!IsListEmpty(&EncodedFrList)) 
    {
        batom_cursor BatomCursor;
        batom_vec BatomVector[VIDEO_ENCODE_DEPTH];

        PLIST_ENTRY DeliverEntry = RemoveTailList(&EncodedFrList);
        EncodedFrListLen--;

        BCMOMX_DBG_ASSERT(DeliverEntry);

        PNEXUS_ENCODED_VIDEO_FRAME pNxVidEncFr = 
                CONTAINING_RECORD(DeliverEntry,NEXUS_ENCODED_VIDEO_FRAME,ListEntry);

        BCMOMX_DBG_ASSERT(pNxVidEncFr);
        BCMOMX_DBG_ASSERT(pNxVidEncFr->BaseAddr);

        if (pNxVidEncFr->CombinedSz > pDeliverFr->SzVidDataBuff) 
        {
            LOG_ERROR("%s: Provided Buffer Too Small To Hold The Encoded Video Data",__FUNCTION__);
            BCMOMX_DBG_ASSERT( pDeliverFr->SzVidDataBuff <= pNxVidEncFr->CombinedSz );
            ReturnEncodedFrameSynchronized(pNxVidEncFr);
            LastErr = ErrStsOutOfResource;
            return false;
        }
        
        unsigned int NumVectors = pNxVidEncFr->FrameData->size();

        BCMOMX_DBG_ASSERT(NumVectors <= VIDEO_ENCODE_DEPTH);

        for (unsigned int i = 0; i < NumVectors; i++) 
        {
            NEXUS_VideoEncoderDescriptor * pVidEncOut = pNxVidEncFr->FrameData->editItemAt(i);
            BCMOMX_DBG_ASSERT(pVidEncOut);


            batom_vec_init(&BatomVector[i], 
                           (void *)(pNxVidEncFr->BaseAddr + pVidEncOut->offset), 
                           pVidEncOut->length);
        }

		batom_cursor_from_vec(&BatomCursor, BatomVector, NumVectors);
		unsigned int SizeToCopy = batom_cursor_size(&BatomCursor); 


		if (SizeToCopy > pDeliverFr->SzVidDataBuff) 
		{
			LOG_ERROR("%s: Input Buffer Sz [%d] Not Enough To Hold Encoded Video Data [Sz:%d]",
					  __FUNCTION__,pDeliverFr->SzVidDataBuff,SizeToCopy);

			BCMOMX_DBG_ASSERT(SizeToCopy <= pDeliverFr->SzVidDataBuff); 
			ReturnEncodedFrameSynchronized(pNxVidEncFr);
			LastErr = ErrStsOutOfResource;
			return false;
		}

		unsigned int CopiedSz=0;

		CopiedSz =  batom_cursor_copy(&BatomCursor,	pDeliverFr->pVideoDataBuff, SizeToCopy);
		
		if (CopiedSz != SizeToCopy) 
		{
			LOG_ERROR("%s: Cursor Copy Error Requested Size %d != Copied Sz:%d",
					  __FUNCTION__,SizeToCopy,CopiedSz);

			BCMOMX_DBG_ASSERT(CopiedSz == SizeToCopy); 
			ReturnEncodedFrameSynchronized(pNxVidEncFr);
			LastErr = ErrStsNexusReturnedErr;
			return false;
		}

		pDeliverFr->OutFlags = pNxVidEncFr->ClientFlags;
		pDeliverFr->usTimeStamp = pNxVidEncFr->usTimeStampOriginal;
		pDeliverFr->SzFilled = SizeToCopy;
		ReturnEncodedFrameSynchronized(pNxVidEncFr);
		LastErr = ErrStsSuccess;
		return true; 
    }

    return false;
}

bool
OMXNexusVideoEncoder::ReturnEncodedFrameSynchronized(PNEXUS_ENCODED_VIDEO_FRAME pReturnFr)
{
    if (!pReturnFr) 
    {
        LOG_ERROR("%s %d:  Invlaid Parameters",__FUNCTION__,__LINE__);
        return false;
    }

    if (!IsEncoderStarted()) 
    {
        LOG_ERROR("%s %d: Encoder Is Not Started",__FUNCTION__,__LINE__);
        LastErr= ErrStsEncoNotStarted;
        return false;
    }

    unsigned int CntReturnFrame = pReturnFr->FrameData->size();
    FAST_INIT_ENC_VID_FR(pReturnFr);    
    
    //The Vector should be empty now
    BCMOMX_DBG_ASSERT(0==pReturnFr->FrameData->size());
    
    InsertHeadList(&EmptyFrList,&pReturnFr->ListEntry);
    EmptyFrListLen++;

    if (CntReturnFrame) 
    {
        NEXUS_SimpleEncoder_VideoReadComplete(EncoderHandle,CntReturnFrame);

    }else{
        LOG_WARNING("%s :Returning A Frame With Zero Count [%d]",
                    __FUNCTION__,CntReturnFrame);

        BCMOMX_DBG_ASSERT(CntReturnFrame);
    }

    return true;
}

bool 
OMXNexusVideoEncoder::GetFrameStart( NEXUS_VideoEncoderDescriptor *pVidEncDescr,
                                        size_t    CntElements,
                                        unsigned int *OutIndex)
{
    if ( (!CntElements)  || (!pVidEncDescr))
    {
        LOG_ERROR("%s [%d]:Invalid Parameters",__FUNCTION__,__LINE__);
        BCMOMX_DBG_ASSERT(false);
        return false;
    }

    for (unsigned int Index=0; Index < CntElements; Index++) 
    {
        if (pVidEncDescr->flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_FRAME_START) 
        {
            *OutIndex=Index;
            return true;
        }
        pVidEncDescr++;
    }

    return false;
}
/***************************************************************************************************  
 *              THERE IS NO GUARANTEED WAY TO DETERMINE REPEATED AUDIO FRAME
 *              FROM THE HARDWARE.
 * We will not Pull anything from hardware till we have something in the decoded list.
 * There is no guarantted way of determining duplicate frames. So we pull N frames, use them
 * and then return them back to hardware before we pull new ones. This cycle assures that we
 * never get duplicate frames.
****************************************************************************************************/
    
// Lock is already held when this function is called 
// The EOS Logic Looks for NULL FRAME FROM encoder after the 
// EOS is received on the input side.
//
bool 
OMXNexusVideoEncoder::ShouldDiscardFrame(PNEXUS_ENCODED_VIDEO_FRAME pCheckFr)
{
//  if( (!pCheckFr) || !pCheckFr->FrameData->size() )
//  {
//      //There are no frames to check against.
//      LOG_ERROR("%s [%d]:Invalid Parameters",__FUNCTION__,__LINE__);
//      return true; //So that the caller drops this frame
//  }
//
//  if(pCheckFr->usTimeStampOriginal < FrRepeatParams.TimeStamp)
//  {
//      //This is a Repeated Frame.
//      return true;
//  }

#if 0   // Enable this on need basis.
    if (StartEOSDetection) 
    {
        // We Stop the capture when we get first frame Invalid After The EOS is 
        // Received on the input side.
        if ( (!pCheckFr->usTimeStampOriginal) || 
             (pCheckFr->usTimeStampOriginal < FrRepeatParams.TimeStamp)  )
        {
            LOG_EOS_DBG("%s:%d EOS SET -- Discarding Frame TS:%lld Disabling Capture Now !!----",
                     __FUNCTION__,__LINE__,
                     pCheckFr->usTimeStampOriginal);

            StopCaptureFrames();
            return true;
        }
    }
#endif

    if (!pCheckFr->usTimeStampOriginal) 
    {
        LOG_WARNING("%s:%d NULL TIME STAMP Discarding Frame TS:%lld !!----",
                 __FUNCTION__,__LINE__,
                 pCheckFr->usTimeStampOriginal);
        return true;
    }

    FrRepeatParams.TimeStamp = pCheckFr->usTimeStampOriginal; 
    FrRepeatParams.TimeStampInterpolated = pCheckFr->usTimeStampIntepolated;
    return false;
}

#if 0

void
OMXNexusAudioDecoder::PrintAudioMuxOutFrame(const NEXUS_AudioMuxOutputFrame *pAudMuxOutFr)
{
    if (pAudMuxOutFr) 
    {
        LOG_INFO("%s:%d pAudMuxOutFr->flags:%d pAudMuxOutFr->length:%d pAudMuxOutFr->offset:%d pAudMuxOutFr->originalPts:%d pAudMuxOutFr->Pts:%lld",
                 __FUNCTION__,__LINE__, 
                 pAudMuxOutFr->flags,
                 pAudMuxOutFr->length,
                 pAudMuxOutFr->offset,
                 pAudMuxOutFr->originalPts,
                 pAudMuxOutFr->pts);
    }
}

void
OMXNexusAudioDecoder::PrintFrame(const PNEXUS_DECODED_AUDIO_FRAME pPrintFr)
{
    if (!pPrintFr) 
    {
        LOG_ERROR("%s[%d]: Invalid Parameters",__FUNCTION__,__LINE__);
        BCMOMX_DBG_ASSERT(pPrintFr);
        return;
    }
    
    LOG_INFO("%s:%d: Frame Parameters: BaseAddr:%p ClientFlags:%d CombinedSz:%d TimeStamp[Original]:%lld TimeStamp[Interpolated]:%lld",
             __FUNCTION__,__LINE__,
             pPrintFr->BaseAddr, pPrintFr->ClientFlags, 
             pPrintFr->CombinedSz,
             pPrintFr->usTimeStampOriginal,
             pPrintFr->usTimeStampIntepolated);

    LOG_INFO("%s:%d ==== Printing Chunks Of Frame",__FUNCTION__,__LINE__);
    for (unsigned int i=0;i < pPrintFr->FrameData->size();i++) 
    {
        NEXUS_AudioMuxOutputFrame *pAudMuxOutFr=NULL;
        pAudMuxOutFr = pPrintFr->FrameData->editItemAt(i);
        PrintAudioMuxOutFrame(pAudMuxOutFr);
    }
}
#endif

unsigned int 
OMXNexusVideoEncoder::RetriveFrameFromHardware()
{
    NEXUS_SimpleEncoderStatus EncSts;
    const NEXUS_VideoEncoderDescriptor *pFrames1=NULL, *pFrames2=NULL;
    size_t  SzFr1=0,SzFr2=0;
    unsigned int FramesRetrived=0;
    NEXUS_Error NxErrCode = NEXUS_SUCCESS;

    //Mutex::Autolock lock(mListLock); 
    if (!IsEncoderStarted()) 
    {
        LOG_ERROR("%s[%d]: Encoder Is Not Started",__FUNCTION__,__LINE__);
        LastErr= ErrStsEncoNotStarted;
        return 0;
    }

    if (false == CaptureFrameEnabled()) 
    {
        LOG_ERROR("%s[%d]: Capture Frame Not Enabled",__FUNCTION__,__LINE__);
        LastErr= ErrStsRetFramesNoFrProcessed;
        return 0;
    }

    // We will not Pull anything from hardware till we have something in the encoded list.
    // There is no guarantted way of determining duplicate frames. So we pull N frames, use them
    // and then return them back to hardware before we pull new ones. This cycle assures that we 
    // never get duplicate frames.
    if (!IsListEmpty(&EncodedFrList)) 
    {
        LOG_INFO("%s[%d]: We Still Have Frame To Deliver, Not Pulling New Frames",__FUNCTION__,__LINE__);
        LastErr= ErrStsEncodeListFull; // Check this errortype
        return 0;
    }

    // Get the Empty Frame from Free List
    // Get a full frame from hardware
    // Update the Structure with the frame.
    // Return the frame to the decoded list.
    if (IsListEmpty(&EmptyFrList)) 
    {
        LOG_ERROR("%s[%d]: No More Space To Hold A New Frame",__FUNCTION__,__LINE__);
        LastErr= ErrStsOutOfResource;
        return 0;
    }                 

    NEXUS_SimpleEncoder_GetStatus(EncoderHandle, &EncSts);

    NxErrCode = NEXUS_SimpleEncoder_GetVideoBuffer(EncoderHandle, 
                                                   &pFrames1, 
                                                   &SzFr1, 
                                                   &pFrames2, 
                                                   &SzFr2);

    if ( (NxErrCode != NEXUS_SUCCESS) || (0 == SzFr1 + SzFr2) || (0 == SzFr1)) 
    {
        LOG_ERROR("%s[%d]: No Encoded Frames From Encoder Sts:%d Sz1:%d Sz2:%d",
                    __FUNCTION__,__LINE__,
                  NxErrCode,SzFr1,SzFr2);

        LastErr= ErrStsRetFramesNoFrProcessed;
        return 0;
    }

    // SzFr1 should have something && the pointer should not be null. 
    // Assert if any one of the above condition is not true.
    BCMOMX_DBG_ASSERT( (SzFr1 > 0) && (NULL != pFrames1));

    unsigned int FrStartIndex=0xffffffff;

    if( false == GetFrameStart((NEXUS_VideoEncoderDescriptor *) pFrames1, 
                                 SzFr1, &FrStartIndex) )
    {
        //There is no Frame Start At All In Data That Was Returned
        LOG_ERROR("%s [%d]: Frame Start Not Found, Dropping %d Frames",
                  __FUNCTION__,__LINE__,SzFr1);

        //No Frame Start Found
        NEXUS_SimpleEncoder_VideoReadComplete(EncoderHandle, SzFr1);
        LastErr= ErrStsRetFramesNoFrProcessed;
        return 0;

    }else{
        //There is a Frame Start Somewhere. Return The Initial Non Aligned Frames, If Any
        if ( (FrStartIndex > 0) && (FrStartIndex != 0xffffffff) ) 
        {
            LOG_ERROR("%s [%d]: Dropping Misaligned Frames",__FUNCTION__,__LINE__);
            NEXUS_SimpleEncoder_VideoReadComplete(EncoderHandle, FrStartIndex);
            LastErr= ErrStsMisalignedFramesDropped;
            return 0;
        }
    }

    //Now We should always be at 0 index where the frame starts
    BCMOMX_DBG_ASSERT(0==FrStartIndex);

    const NEXUS_VideoEncoderDescriptor *pCurrVidEncDescr = pFrames1;
    unsigned int NumToProcess=SzFr1;
    unsigned int DiscardedCnt=0;

    PNEXUS_ENCODED_VIDEO_FRAME  pEmptyFr=NULL;
    
    
    while (NumToProcess) 
    {
        if ( pCurrVidEncDescr[0].flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_FRAME_START )
        {
            //
            // You should never come here with a valid pEmptyFr
            //
            BCMOMX_DBG_ASSERT(NULL == pEmptyFr);

            if (IsListEmpty(&EmptyFrList)) 
            {
//              LOG_WARNING("%s [%d]: RAN OUT OF Buffers FLL:%d DLL:%d==",
//                          __FUNCTION__,__LINE__,EmptyFrListLen,DecodedFrListLen);

                //Validate Your List Sizes
                BCMOMX_DBG_ASSERT(VIDEO_ENCODE_DEPTH == (EmptyFrListLen+EncodedFrListLen));
                return FramesRetrived;
            }

            PLIST_ENTRY pThisEntry = RemoveTailList(&EmptyFrList); 
            EmptyFrListLen--;
            BCMOMX_DBG_ASSERT(pThisEntry);
            pEmptyFr = CONTAINING_RECORD(pThisEntry,NEXUS_ENCODED_VIDEO_FRAME,ListEntry);    
            FAST_INIT_ENC_VID_FR(pEmptyFr);
        }

        if (pEmptyFr) 
        {
            if ( pCurrVidEncDescr[0].flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_ORIGINALPTS_VALID )
            {
                pEmptyFr->usTimeStampOriginal = (pCurrVidEncDescr[0].originalPts/45) * 1000;
            }

            if ( pCurrVidEncDescr[0].flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_PTS_VALID )
            {
                pEmptyFr->usTimeStampIntepolated = (pCurrVidEncDescr[0].pts/90) * 1000;
            }
            
            pEmptyFr->CombinedSz += pCurrVidEncDescr[0].length;
            pEmptyFr->BaseAddr = (unsigned int) EncSts.video.pBufferBase;

            pEmptyFr->FrameData->add((NEXUS_VideoEncoderDescriptor *) pCurrVidEncDescr);
            
            if ( pCurrVidEncDescr[0].flags & NEXUS_AUDIOMUXOUTPUTFRAME_FLAG_FRAME_END ) // TODO: Won't compile.
            {
                if(false == ShouldDiscardFrame(pEmptyFr))
                {
                    InsertHeadList(&EncodedFrList, &pEmptyFr->ListEntry);
                    EncodedFrListLen++;
                    //PrintFrame(pEmptyFr);
                    LOG_INFO("%s[%d]: Frame Queued Encoded List Length:%d Empty List Length:%d",
                             __FUNCTION__,__LINE__,
                             EncodedFrListLen,EmptyFrListLen);
                    
                    FramesRetrived++;
                    pEmptyFr=NULL;

                }else{

                    FAST_INIT_ENC_VID_FR(pEmptyFr);    
                    InsertHeadList(&EmptyFrList, &pEmptyFr->ListEntry);
                    EmptyFrListLen++;
                    DiscardedCnt++;

                    LOG_INFO("%s[%d]: Frame Not Accepted Encoded List Length:%d Empty List Length:%d",
                             __FUNCTION__,__LINE__,EncodedFrListLen,EmptyFrListLen);

                    pEmptyFr=NULL;

                    // We have discarded a frame, Is It Becuase Of EOS?? If that is the case 
                    // WE dont want to capture anymore. 

                    // TODO: revisit
                    //if (false == CaptureFrameEnabled()) break; 
                }
            }
        }

        pCurrVidEncDescr++;
        NumToProcess--;

        if(!NumToProcess)
        {
            pCurrVidEncDescr = pFrames2;
            NumToProcess = SzFr2;
        }
    }

    if(pEmptyFr)
    {
        LOG_INFO("%s [%d]: One Frame Partial Without End Flag Set==",
                 __FUNCTION__,__LINE__);

        FAST_INIT_ENC_VID_FR(pEmptyFr);
        InsertHeadList(&EmptyFrList, &pEmptyFr->ListEntry);
        EmptyFrListLen++;
    }

    LOG_INFO("%s [%d]: Frames Retrieved :%d ==",
         __FUNCTION__,__LINE__,FramesRetrived);

    return FramesRetrived;
}


void 
OMXNexusVideoEncoder::InputEOSReceived(unsigned int InputFlags)
{
    LOG_EOS_DBG("%s %d: EOS Received On The Input Side",__FUNCTION__,__LINE__);
    BCMOMX_DBG_ASSERT(false);
    return;
}

ErrorStatus
OMXNexusVideoEncoder::GetLastError()
{
    return LastErr;
}

void 
OMXNexusVideoEncoder::StartCaptureFrames()
{
    CaptureFrames=true;
}

void 
OMXNexusVideoEncoder::StopCaptureFrames()
{
    CaptureFrames=false;
}

bool 
OMXNexusVideoEncoder::CaptureFrameEnabled()
{
    return CaptureFrames;
}

