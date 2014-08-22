#include "OMXNexusAudioDecoder.h"

#undef LOG_TAG
#define LOG_TAG "BCM_OMX_AUDIO_DEC"

//Path level Debug Messages
#define LOG_START_STOP_DBG      ALOGD
#define LOG_EOS_DBG             ALOGD         
#define LOG_FLUSH_MSGS          ALOGD
#define LOG_CREATE_DESTROY      ALOGD

//Path independent Debug Messages
#define LOG_WARNING             ALOGD
#define LOG_ERROR               ALOGD
#define LOG_INFO                


OMXNexusAudioDecoder::OMXNexusAudioDecoder(char *CallerName, NEXUS_AudioCodec NxCodec)
    : AudioDecoHandle(NULL),EncoderHandle(NULL),
    LastErr(ErrStsSuccess),EmptyFrListLen(0),DecodedFrListLen(0),
    StartEOSDetection(false), DecoderStarted(false),
    DecoEvtLisnr(NULL),FlushCnt(0), CaptureFrames(true),NxPIDChannel(0),
    NxAudioCodec(NxCodec)
{
    LOG_CREATE_DESTROY("%s: ENTER ===",__FUNCTION__);
    InitializeListHead(&EmptyFrList);
    InitializeListHead(&DecodedFrList);

    b_refsw_client_client_configuration         config;
    b_refsw_client_client_info                  client_info;
    b_refsw_client_connect_resource_settings    connectSettings;
    NxIPCClient = NexusIPCClientFactory::getClient(CallerName);

    BKNI_Memset(&config, 0, sizeof(config));
    BKNI_Snprintf(config.name.string,sizeof(config.name.string),CallerName);
    
    config.resources.encoder = true;
    config.resources.audioDecoder = true;
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

    connectSettings.simpleAudioDecoder.id = client_info.audioDecoderId;
    connectSettings.simpleAudioDecoder.decoderCaps.encoder = true;

    connectSettings.simpleEncoder[0].id = client_info.encoderId;
    connectSettings.simpleEncoder[0].audio.cpuAccessible = true;
    //connectSettings.simpleEncoder[0].video.cpuAccessible = true;

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


    AudioDecoHandle = NxIPCClient->acquireAudioDecoderHandle();
    if ( NULL == AudioDecoHandle )    
    {        
        LOG_ERROR("%s Unable to acquire Simple Audio Decoder \n",__FUNCTION__); 
        LastErr = ErrStsDecoAcquireFail;
        return;
    }else{
        LOG_CREATE_DESTROY("%s: Acquired Audio Decoder Handle", __FUNCTION__);
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

    for (unsigned int i=0; i < AUDIO_DECODE_DEPTH; i++) 
    {
        PNEXUS_DECODED_AUDIO_FRAME pDecoFr = (PNEXUS_DECODED_AUDIO_FRAME) 
                                                    malloc(sizeof(NEXUS_DECODED_AUDIO_FRAME));
        if (!pDecoFr) 
        {
            LastErr = ErrStsOutOfResource;
            return;
        }
        
        pDecoFr->FrameData = new Vector <NEXUS_AudioMuxOutputFrame *>();
        FAST_INIT_DEC_AUD_FR(pDecoFr);
        InitializeListHead(&pDecoFr->ListEntry);
        InsertHeadList(&EmptyFrList,&pDecoFr->ListEntry);
        EmptyFrListLen++;
        BCMOMX_DBG_ASSERT(EmptyFrListLen <= AUDIO_DECODE_DEPTH);
    }

    LastErr = ErrStsSuccess;
    LOG_CREATE_DESTROY("%s: OMXNexusAudioDecoder Created!!",__FUNCTION__);

}

OMXNexusAudioDecoder::~OMXNexusAudioDecoder()
{
    LOG_CREATE_DESTROY("%s: ",__FUNCTION__);
    unsigned int NumEntriesFreed=0;
    DecoEvtLisnr=NULL;
    if(IsDecoderStarted())
    {
        LOG_CREATE_DESTROY("%s: Stopping Decoder",__FUNCTION__);
        StopDecoder();
    }

    NxIPCClient->disconnectClientResources(NxClientCntxt);

    if (AudioDecoHandle) 
    {
        LOG_CREATE_DESTROY("%s: Releasing Decoder",__FUNCTION__);
        NEXUS_SimpleAudioDecoder_Release(AudioDecoHandle); 
        AudioDecoHandle=NULL;
    }

    if (EncoderHandle) 
    {
        LOG_CREATE_DESTROY("%s: Releasing Encoder",__FUNCTION__);
        NEXUS_SimpleEncoder_Release(EncoderHandle); 
        AudioDecoHandle=NULL;
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
        PNEXUS_DECODED_AUDIO_FRAME pDecoFr = CONTAINING_RECORD(ThisEntry,NEXUS_DECODED_AUDIO_FRAME,ListEntry);
        if (pDecoFr->FrameData) delete pDecoFr->FrameData;
        NumEntriesFreed++;
        free(pDecoFr);
    }
    BCMOMX_DBG_ASSERT(0==EmptyFrListLen);

    LOG_CREATE_DESTROY("%s: Freeing Decoded Frame List",__FUNCTION__);
    //Remaining in  list goes away
    while (!IsListEmpty(&DecodedFrList))
    {
        PLIST_ENTRY ThisEntry =  RemoveTailList(&DecodedFrList);
        DecodedFrListLen--;
        PNEXUS_DECODED_AUDIO_FRAME pDecoFr = CONTAINING_RECORD(ThisEntry,NEXUS_DECODED_AUDIO_FRAME,ListEntry);
        if (pDecoFr->FrameData) delete pDecoFr->FrameData;
        NumEntriesFreed++;
        free(pDecoFr);
    }

    BCMOMX_DBG_ASSERT(NumEntriesFreed==AUDIO_DECODE_DEPTH);
    LOG_CREATE_DESTROY("%s: Deleting The NxIPCClient",__FUNCTION__);
    delete NxIPCClient;
}


bool
OMXNexusAudioDecoder::StartDecoder(unsigned int ClientParam)
{
    return StartDecoder((NEXUS_PidChannelHandle)ClientParam);
}


bool
OMXNexusAudioDecoder::StartDecoder(NEXUS_PidChannelHandle NexusPIDChannel)
{
    NEXUS_Error errCode;
    NEXUS_SimpleAudioDecoderStartSettings AudioDecStartSettings;
    NEXUS_SimpleEncoderStartSettings EncoderStartSettings;
    NxPIDChannel = NexusPIDChannel;

    if (DecoderStarted) 
    {
        LOG_START_STOP_DBG("%s[%d]: Decoder Already Started",
                           __FUNCTION__, __LINE__);

        LastErr = ErrStsStartDecoFailed;
        return false;
    }
    
    NEXUS_SimpleEncoder_GetDefaultStartSettings(&EncoderStartSettings);
    EncoderStartSettings.input.audio = AudioDecoHandle;
    EncoderStartSettings.output.audio.codec = NEXUS_AudioCodec_eLpcm1394;
    EncoderStartSettings.output.audio.pid = 1;
    EncoderStartSettings.output.transport.type = NEXUS_TransportType_eEs;
    errCode = NEXUS_SimpleEncoder_Start(EncoderHandle, &EncoderStartSettings);

    if (NEXUS_SUCCESS != errCode) 
    {
        LOG_ERROR("%s[%d]: NEXUS_SimpleEncoder_Start Failed [Err:%d]!!",
            __FUNCTION__, __LINE__, errCode);

        LastErr = ErrStsStartDecoFailed;
        return false;
    }else{
        LOG_START_STOP_DBG("%s[%d]: SimpleEncoder Started Successfully [Err:%d]!!",
            __FUNCTION__, __LINE__, errCode);
    }

    NEXUS_SimpleAudioDecoder_GetDefaultStartSettings(&AudioDecStartSettings);
    AudioDecStartSettings.primary.codec = NxAudioCodec;
    AudioDecStartSettings.primary.pidChannel = NxPIDChannel;
    errCode = NEXUS_SimpleAudioDecoder_Start(AudioDecoHandle, &AudioDecStartSettings);

    if (NEXUS_SUCCESS != errCode) 
    {
        LOG_ERROR("%s[%d]: SimpleAudioDecoder Start Failed [Err:%d]!!",
            __FUNCTION__, __LINE__, errCode);

        NEXUS_SimpleEncoder_Stop(EncoderHandle);
        LastErr = ErrStsStartDecoFailed;
        return false;
    }else{
        LOG_START_STOP_DBG("%s[%d]: SimpleDecoder Started Successfully [Err:%d]!!",
            __FUNCTION__, __LINE__, errCode);
    }

    
    LastErr = ErrStsSuccess;
    DecoderStarted = true;
    StartCaptureFrames();
    return true;
}

void 
OMXNexusAudioDecoder::StopDecoder()
{
    if (DecoderStarted) 
    {
        NEXUS_SimpleAudioDecoder_Stop(AudioDecoHandle); 
        NEXUS_SimpleEncoder_Stop(EncoderHandle);

        LOG_START_STOP_DBG("%s[%d]: Audio Decoder Stopped!!",
                           __FUNCTION__, __LINE__);

        DecoderStarted=false;
        LastErr = ErrStsSuccess;
    }else{

        LOG_START_STOP_DBG("%s[%d]: Stop Called When Audio Dec Already Stopped!!",
                           __FUNCTION__, __LINE__);

        LastErr = ErrStsDecoNotStarted;
    }
    
    return;
}


bool
OMXNexusAudioDecoder::IsDecoderStarted()
{
    return DecoderStarted;
}

void 
OMXNexusAudioDecoder::PrintAudioEncoderStatus() 
{
    NEXUS_SimpleEncoderStatus EncSts;
    NEXUS_SimpleEncoder_GetStatus(EncoderHandle,&EncSts);
       
    LOG_INFO("%s[%d]: pBaseAddr:%p pMetaDataBuff:%p NumFrames:%d NumErrorFrames:%d",
             __FUNCTION__,__LINE__, 
             EncSts.audio.pBufferBase,
             EncSts.audio.pMetadataBufferBase,
             EncSts.audio.numFrames,
             EncSts.audio.numErrorFrames);

}

void
OMXNexusAudioDecoder::PrintAudioDecoderStatus()
{
    NEXUS_AudioDecoderStatus AudDecoSts;
    NEXUS_SimpleAudioDecoder_GetStatus(AudioDecoHandle,&AudDecoSts);

    LOG_INFO("%s[%d]: Started:%d Tsm:%d locked:%d Codec:%d SampleRate:%d FramesDecoded:%d FrameErrors:%d DummyFrames:%d NumBytesDecoded:%d CurrentPts:%d",
             __FUNCTION__,__LINE__,AudDecoSts.started,AudDecoSts.tsm,
             AudDecoSts.locked, AudDecoSts.codec,AudDecoSts.sampleRate,
             AudDecoSts.framesDecoded,AudDecoSts.frameErrors,
             AudDecoSts.dummyFrames,AudDecoSts.numBytesDecoded,
             AudDecoSts.pts);

    PrintAudioEncoderStatus();
}

bool
OMXNexusAudioDecoder::GetDecodedFrame(PDELIVER_AUDIO_FRAME pDeliverFr)
{
    Mutex::Autolock lock(mListLock);

    PrintAudioDecoderStatus();
    RetriveFrameFromHardware();

    if (!IsDecoderStarted()) 
    {
        LOG_ERROR("%s %d: Decoder Is Not Started",__FUNCTION__,__LINE__);
        LastErr= ErrStsDecoNotStarted;
        return false;
    }

    if ( (!pDeliverFr) ||  (!pDeliverFr->pAudioDataBuff) || (!pDeliverFr->SzAudDataBuff))
    {
        LOG_ERROR("%s: Invalid Parameters",__FUNCTION__);
        LastErr = ErrStsInvalidParam;
        return false;
    }

    //Check For the EOS First
    if (IsListEmpty(&DecodedFrList)) 
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


    if (!IsListEmpty(&DecodedFrList)) 
    {
        batom_cursor BatomCursor;
        batom_vec BatomVector[AUDIO_DECODE_DEPTH];

        PLIST_ENTRY DeliverEntry = RemoveTailList(&DecodedFrList);
        DecodedFrListLen--;

        BCMOMX_DBG_ASSERT(DeliverEntry);

        PNEXUS_DECODED_AUDIO_FRAME pNxDecAudFr = 
                CONTAINING_RECORD(DeliverEntry,NEXUS_DECODED_AUDIO_FRAME,ListEntry);

        BCMOMX_DBG_ASSERT(pNxDecAudFr);
        BCMOMX_DBG_ASSERT(pNxDecAudFr->BaseAddr);

        if (pNxDecAudFr->CombinedSz > pDeliverFr->SzAudDataBuff) 
        {
            LOG_ERROR("%s: Provided Buffer Too Small To Hold The Audio Data",__FUNCTION__);
            BCMOMX_DBG_ASSERT( pDeliverFr->SzAudDataBuff <= pNxDecAudFr->CombinedSz );
            ReturnDecodedFrameSynchronized(pNxDecAudFr);
            LastErr = ErrStsOutOfResource;
            return false;
        }
        
        unsigned int NumVectors = pNxDecAudFr->FrameData->size();

        BCMOMX_DBG_ASSERT(NumVectors <= AUDIO_DECODE_DEPTH);

        for (unsigned int i = 0; i < NumVectors; i++) 
        {
            NEXUS_AudioMuxOutputFrame * pAudMuxOut = pNxDecAudFr->FrameData->editItemAt(i);
            BCMOMX_DBG_ASSERT(pAudMuxOut);

            batom_vec_init(&BatomVector[i], 
                           (void *)(pNxDecAudFr->BaseAddr + pAudMuxOut->offset), 
                           pAudMuxOut->length);
        }

        batom_cursor_from_vec(&BatomCursor, BatomVector, NumVectors);

        //Extract the Header Information
        batom_cursor_skip(&BatomCursor, 3);

        // 
        // Extract the sample rate from the last bit field in the last header byte
        // (See http://www.dvdforum.org/images/Guideline1394V10R0_20020911.pdf ) 
        //
        unsigned int Byte = batom_cursor_byte(&BatomCursor);
        unsigned int SampleRate=0;
        Byte >>=3; Byte &=0x07;
        if (1 == Byte) 
        {
            SampleRate = 44100;
        }else{
            SampleRate = 48000;
        }

        unsigned int SizeToCopy = batom_cursor_size(&BatomCursor); 

        if (SizeToCopy > pDeliverFr->SzAudDataBuff) 
        {
            LOG_ERROR("%s: Input Buffer Sz [%d] Not Enough To Hold Audio Data [Sz:%d]",
                      __FUNCTION__,pDeliverFr->SzAudDataBuff,SizeToCopy);

            BCMOMX_DBG_ASSERT(SizeToCopy <= pDeliverFr->SzAudDataBuff); 
            ReturnDecodedFrameSynchronized(pNxDecAudFr);
            LastErr = ErrStsOutOfResource;
            return false;
        }

        unsigned int CopiedSz=0;
        unsigned short * pSamples = (unsigned short *) pDeliverFr->pAudioDataBuff;
        for (unsigned int index=0; index < SizeToCopy; index += 2)
        {
            unsigned short Sample;
            Sample = batom_cursor_uint16_be(&BatomCursor);
            pSamples[index/2] = Sample;
            CopiedSz+=2;
        }

        if (CopiedSz != SizeToCopy) 
        {
            LOG_ERROR("%s: Cursor Copy Error Requested Size %d != Copied Sz:%d",
                      __FUNCTION__,SizeToCopy,CopiedSz);

            BCMOMX_DBG_ASSERT(CopiedSz == SizeToCopy); 
            ReturnDecodedFrameSynchronized(pNxDecAudFr);
            LastErr = ErrStsNexusReturnedErr;
            return false;
        }

        pDeliverFr->OutFlags = pNxDecAudFr->ClientFlags;
        pDeliverFr->usTimeStamp = pNxDecAudFr->usTimeStampOriginal;
        pDeliverFr->SzFilled = SizeToCopy;
        ReturnDecodedFrameSynchronized(pNxDecAudFr);
        LastErr = ErrStsSuccess;
        return true; 
    }


    return false;
}

bool
OMXNexusAudioDecoder::ReturnDecodedFrameSynchronized(PNEXUS_DECODED_AUDIO_FRAME pReturnFr)
{
    if (!pReturnFr) 
    {
        LOG_ERROR("%s %d:  Invlaid Parameters",__FUNCTION__,__LINE__);
        return false;
    }

    if (!IsDecoderStarted()) 
    {
        LOG_ERROR("%s %d: Decoder Is Not Started",__FUNCTION__,__LINE__);
        LastErr= ErrStsDecoNotStarted;
        return false;
    }

    unsigned int CntReturnFrame = pReturnFr->FrameData->size();
    FAST_INIT_DEC_AUD_FR(pReturnFr);    
    
    //The Vector should be empty now
    BCMOMX_DBG_ASSERT(0==pReturnFr->FrameData->size());
    
    InsertHeadList(&EmptyFrList,&pReturnFr->ListEntry);
    EmptyFrListLen++;

    if (CntReturnFrame) 
    {
        NEXUS_SimpleEncoder_AudioReadComplete(EncoderHandle,CntReturnFrame);

    }else{
        LOG_WARNING("%s :Returning A Frame With Zero Count [%d]",
                    __FUNCTION__,CntReturnFrame);

        BCMOMX_DBG_ASSERT(CntReturnFrame);
    }

    return true;
}

bool 
OMXNexusAudioDecoder::GetFrameStart( NEXUS_AudioMuxOutputFrame *pAudMuxOutFr,
                                        size_t    CntElements,
                                        unsigned int *OutIndex)
{
    if ( (!CntElements)  || (!pAudMuxOutFr))
    {
        LOG_ERROR("%s [%d]:Invalid Parameters",__FUNCTION__,__LINE__);
        BCMOMX_DBG_ASSERT(false);
        return false;
    }

    for (unsigned int Index=0; Index < CntElements; Index++) 
    {
        if (pAudMuxOutFr->flags & NEXUS_AUDIOMUXOUTPUTFRAME_FLAG_FRAME_START) 
        {
            *OutIndex=Index;
            return true;
        }
        pAudMuxOutFr++;
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
OMXNexusAudioDecoder::ShouldDiscardFrame(PNEXUS_DECODED_AUDIO_FRAME pCheckFr)
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

unsigned int 
OMXNexusAudioDecoder::RetriveFrameFromHardware()
{
    NEXUS_SimpleEncoderStatus EncSts;
    const NEXUS_AudioMuxOutputFrame *pFrames1=NULL, *pFrames2=NULL;
    size_t  SzFr1=0,SzFr2=0;
    unsigned int FramesRetrived=0;
    NEXUS_Error NxErrCode = NEXUS_SUCCESS;

    //Mutex::Autolock lock(mListLock); 
    if (!IsDecoderStarted()) 
    {
        LOG_ERROR("%s[%d]: Decoder Is Not Started",__FUNCTION__,__LINE__);
        LastErr= ErrStsDecoNotStarted;
        return 0;
    }

    if (false == CaptureFrameEnabled()) 
    {
        LOG_ERROR("%s[%d]: Capture Frame Not Enabled",__FUNCTION__,__LINE__);
        LastErr= ErrStsRetFramesNoFrProcessed;
        return 0;
    }

    // We will not Pull anything from hardware till we have something in the decoded list.
    // There is no guarantted way of determining duplicate frames. So we pull N frames, use them
    // and then return them back to hardware before we pull new ones. This cycle assures that we 
    // never get duplicate frames.
    if (!IsListEmpty(&DecodedFrList)) 
    {
        LOG_INFO("%s[%d]: We Still Have Frame To Deliver, Not Pulling New Frames",__FUNCTION__,__LINE__);
        LastErr= ErrStsDecodeListFull;
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

    NxErrCode = NEXUS_SimpleEncoder_GetAudioBuffer(EncoderHandle, 
                                                   &pFrames1, 
                                                   &SzFr1, 
                                                   &pFrames2, 
                                                   &SzFr2);

    if ( (NxErrCode != NEXUS_SUCCESS) || (0 == SzFr1 + SzFr2) || (0 == SzFr1)) 
    {
        LOG_ERROR("%s[%d]: No Decoded Frames From Decoder Sts:%d Sz1:%d Sz2:%d",
                    __FUNCTION__,__LINE__,
                  NxErrCode,SzFr1,SzFr2);

        LastErr= ErrStsRetFramesNoFrProcessed;
        return 0;
    }


    // SzFr1 should have something && the pointer should not be null. 
    // Assert if any one of the above condition is not true.
    BCMOMX_DBG_ASSERT( (SzFr1 > 0) && (NULL != pFrames1));

    unsigned int FrStartIndex=0xffffffff;

    if( false == GetFrameStart((NEXUS_AudioMuxOutputFrame *) pFrames1, 
                                 SzFr1, &FrStartIndex) )
    {
        //There is no Frame Start At All In Data That Was Returned
        LOG_ERROR("%s [%d]: Frame Start Not Found, Dropping %d Frames",
                  __FUNCTION__,__LINE__,SzFr1);

        //No Frame Start Found
        NEXUS_SimpleEncoder_AudioReadComplete(EncoderHandle, SzFr1);
        LastErr= ErrStsRetFramesNoFrProcessed;
        return 0;

    }else{
        //There is a Frame Start Somewhere. Return The Initial Non Aligned Frames, If Any
        if ( (FrStartIndex > 0) && (FrStartIndex != 0xffffffff) ) 
        {
            LOG_ERROR("%s [%d]: Dropping Misaligned Frames",__FUNCTION__,__LINE__);
            NEXUS_SimpleEncoder_AudioReadComplete(EncoderHandle, FrStartIndex);
            LastErr= ErrStsMisalignedFramesDropped;
            return 0;
        }
    }

    //Now We should always be at 0 index where the frame starts
    BCMOMX_DBG_ASSERT(0==FrStartIndex);

    const NEXUS_AudioMuxOutputFrame *pCurrAudMuxFr = pFrames1;
    unsigned int NumToProcess=SzFr1;
    unsigned int DiscardedCnt=0;

    PNEXUS_DECODED_AUDIO_FRAME  pEmptyFr=NULL;
    
    
    while (NumToProcess) 
    {
        if ( pCurrAudMuxFr[0].flags & NEXUS_AUDIOMUXOUTPUTFRAME_FLAG_FRAME_START )
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
                BCMOMX_DBG_ASSERT(AUDIO_DECODE_DEPTH == (EmptyFrListLen+DecodedFrListLen));
                return FramesRetrived;
            }

            PLIST_ENTRY pThisEntry = RemoveTailList(&EmptyFrList); 
            EmptyFrListLen--;
            BCMOMX_DBG_ASSERT(pThisEntry);
            pEmptyFr = CONTAINING_RECORD(pThisEntry,NEXUS_DECODED_AUDIO_FRAME,ListEntry);    
            FAST_INIT_DEC_AUD_FR(pEmptyFr);
        }

        if (pEmptyFr) 
        {
            if ( pCurrAudMuxFr[0].flags & NEXUS_AUDIOMUXOUTPUTFRAME_FLAG_ORIGINALPTS_VALID )
            {
                pEmptyFr->usTimeStampOriginal = (pCurrAudMuxFr[0].originalPts/45) * 1000;
            }

            if ( pCurrAudMuxFr[0].flags & NEXUS_AUDIOMUXOUTPUTFRAME_FLAG_PTS_VALID )
            {
                pEmptyFr->usTimeStampIntepolated = (pCurrAudMuxFr[0].pts/90) * 1000;
            }
            
            pEmptyFr->CombinedSz += pCurrAudMuxFr[0].length;
            pEmptyFr->BaseAddr = (unsigned int) EncSts.audio.pBufferBase;
            pEmptyFr->FrameData->add((NEXUS_AudioMuxOutputFrame *) pCurrAudMuxFr);
            
            if ( pCurrAudMuxFr[0].flags & NEXUS_AUDIOMUXOUTPUTFRAME_FLAG_FRAME_END )
            {
                if(false == ShouldDiscardFrame(pEmptyFr))
                {
                    InsertHeadList(&DecodedFrList, &pEmptyFr->ListEntry);
                    DecodedFrListLen++;
                    PrintFrame(pEmptyFr);
                    LOG_INFO("%s[%d]: Frame Queued DLL:%d FLL:%d",
                             __FUNCTION__,__LINE__,
                             DecodedFrListLen,EmptyFrListLen);
                    
                    FramesRetrived++;
                    pEmptyFr=NULL;

                }else{

                    FAST_INIT_DEC_AUD_FR(pEmptyFr);    
                    InsertHeadList(&EmptyFrList, &pEmptyFr->ListEntry);
                    EmptyFrListLen++;
                    DiscardedCnt++;

                    LOG_INFO("%s[%d]: Frame Not Accepted DLL:%d FLL:%d",
                             __FUNCTION__,__LINE__,DecodedFrListLen,EmptyFrListLen);

                    pEmptyFr=NULL;

                    // We have discarded a frame, Is It Becuase Of EOS?? If that is the case 
                    // WE dont want to capture anymore. 
                    if (false == CaptureFrameEnabled()) break;
                }
            }
        }

        pCurrAudMuxFr++;
        NumToProcess--;

        if(!NumToProcess)
        {
            pCurrAudMuxFr = pFrames2;
            NumToProcess = SzFr2;
        }
    }

    if(pEmptyFr)
    {
        LOG_INFO("%s [%d]: One Frame Partial Without End Flag Set==",
                 __FUNCTION__,__LINE__);

        FAST_INIT_DEC_AUD_FR(pEmptyFr);
        InsertHeadList(&EmptyFrList, &pEmptyFr->ListEntry);
        EmptyFrListLen++;
    }

    LOG_INFO("%s [%d]: Frames Retrieved :%d ==",
         __FUNCTION__,__LINE__,FramesRetrived);

    return FramesRetrived;
}


void 
OMXNexusAudioDecoder::InputEOSReceived(unsigned int InputFlags,unsigned long long NotUsedVariable)
{
    LOG_EOS_DBG("%s %d: EOS Received On The Input Side",__FUNCTION__,__LINE__);
    StartEOSDetection=true;
    ClientFlags = InputFlags;
    DownCnt = EOS_DOWN_CNT_DEPTH_AUDIO;
    return;
}

unsigned int 
OMXNexusAudioDecoder::DecEosDownCnt()
{
    if (DownCnt) DownCnt--;
    else LOG_EOS_DBG("%s %d: EOS Down Count NOW ZERO",__FUNCTION__,__LINE__);
    return DownCnt; 
}

bool
OMXNexusAudioDecoder::DetectEOS()
{
    if (false == IsDecoderStarted()) 
    {
        LOG_ERROR("%s %d: Decoder Is Not Started",__FUNCTION__,__LINE__);
        LastErr = ErrStsDecoNotStarted;
        return false;
    }

    LOG_EOS_DBG("%s: EOSDetect[%s] ",__FUNCTION__,StartEOSDetection ? "TRUE":"FALSE");

    if (true == StartEOSDetection) 
    {
        if(!DecEosDownCnt())
        {
            LOG_EOS_DBG("%s: --- EOS DETECTED ---",__FUNCTION__);
            StartEOSDetection = false;
            return true;
        }
    }

    return false;
}


bool
OMXNexusAudioDecoder::RegisterDecoderEventListener(DecoderEventsListener *pListener)
{
    if (!pListener) 
    {
        LOG_ERROR("%s: --Invalid Input Parameter--",__FUNCTION__);
        LastErr = ErrStsInvalidParam;
        return false;
    }

    if (DecoEvtLisnr) 
    {
        LOG_ERROR("%s: --Listener Already Registered--",__FUNCTION__);
        LastErr = ErrStsInvalidParam;
        return false;
    }

    DecoEvtLisnr = pListener;
    return true;
}

ErrorStatus
OMXNexusAudioDecoder::GetLastError()
{
    return LastErr;
}

bool
OMXNexusAudioDecoder::FlushDecoder()
{
    NEXUS_SimpleEncoderStatus EncSts;

    if (!IsDecoderStarted()) 
    {
        LOG_FLUSH_MSGS("%s Flush Called When Decoder Not Started", __FUNCTION__); 
        return false;
    }

    LOG_FLUSH_MSGS("%s Flushing Hardware Now Flushing PlayPump....", __FUNCTION__); 

    if (DecoEvtLisnr) DecoEvtLisnr->FlushStarted(); 
    StopDecoder();
    StartDecoder(NxPIDChannel);
    if(DecoEvtLisnr) DecoEvtLisnr->FlushDone(); 
    LOG_FLUSH_MSGS("%s Done Flushing Hardware", __FUNCTION__); 
    return true;
}

bool
OMXNexusAudioDecoder::Flush()
{
    Mutex::Autolock lock(mListLock); 
    FlushCnt++;

    LOG_FLUSH_MSGS("%s: Last Time Stamp Received :%lld  :%lld",
                   __FUNCTION__,
                   FrRepeatParams.TimeStamp,
                   FrRepeatParams.TimeStampInterpolated );

    LOG_FLUSH_MSGS("%s: FLUSH START FlushCnt:%d , Flushing Decoded List", __FUNCTION__,FlushCnt); 
    
    while (!IsListEmpty(&DecodedFrList)) 
    {
        PNEXUS_DECODED_AUDIO_FRAME pDecAudFr=NULL;
        PLIST_ENTRY ThisEntry =  RemoveTailList(&DecodedFrList);
        DecodedFrListLen--;
        BCMOMX_DBG_ASSERT(ThisEntry);
        pDecAudFr=CONTAINING_RECORD(ThisEntry,NEXUS_DECODED_AUDIO_FRAME,ListEntry);
        BCMOMX_DBG_ASSERT(pDecAudFr);
        LOG_FLUSH_MSGS("%s %d: Returning The Frame To Hardware",__FUNCTION__,__LINE__);
        NEXUS_SimpleEncoder_AudioReadComplete(EncoderHandle, 1);
        InsertHeadList(&EmptyFrList,&pDecAudFr->ListEntry);
        EmptyFrListLen++;
    }
       
    BCMOMX_DBG_ASSERT(EmptyFrListLen == AUDIO_DECODE_DEPTH);
    BCMOMX_DBG_ASSERT(0==DecodedFrListLen);
    LOG_FLUSH_MSGS("%s Decoded List Flushed", __FUNCTION__); 

    if (IsDecoderStarted()) 
    {
        FlushDecoder();
    }

    return true;
}

bool
OMXNexusAudioDecoder::PauseDecoder()
{
    LOG_ERROR("%s YET TO BE IMPLEMENTED", __FUNCTION__); 
    return false;
}

bool
OMXNexusAudioDecoder::UnPauseDecoder()
{
    LOG_ERROR("%s YET TO BE IMPLEMENTED", __FUNCTION__); 
    return false;
}


void 
OMXNexusAudioDecoder::StartCaptureFrames()
{
    CaptureFrames=true;
}

void 
OMXNexusAudioDecoder::StopCaptureFrames()
{
    CaptureFrames=false;
}

bool 
OMXNexusAudioDecoder::CaptureFrameEnabled()
{
    return CaptureFrames;
}

