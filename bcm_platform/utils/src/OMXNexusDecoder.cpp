
#include "OMXNexusDecoder.h"

//Path level Debug Messages
#define LOG_START_STOP_DBG  
#define LOG_EOS_DBG             ALOGD         
#define LOG_FLUSH_MSGS          ALOGD
#define LOG_CREATE_DESTROY      ALOGD

//Path independent Debug Messages
#define LOG_WARNING             ALOGD
#define LOG_ERROR               ALOGD
#define LOG_INFO

//Enable logging for output time stamps on every frame.8
#define LOG_OUTPUT_TS

//Enable logging of the decoder state
#define LOG_DECODER_STATE
#define DECODER_STATE_FREQ     30      //Print decoder state after every XX output frames.


OMXNexusDecoder::OMXNexusDecoder(char * CallerName, 
                                 NEXUS_VideoCodec NxCodec,
                                 PaltformSpecificContext *pSetPlatformContext) 
    : decoHandle(NULL),  ipcclient(NULL), DebugCounter(0),
    nexus_client(NULL), LastErr(ErrStsSuccess),
    NextExpectedFr(1), EmptyFrListLen(0), DecodedFrListLen(0),
    FlushCnt(0), ClientFlags(0), DownCnt(DOWN_CNT_DEPTH),
    StartEOSDetection(false), DecoEvtLisnr(NULL)
{
    LOG_CREATE_DESTROY("%s: ENTER ===",__FUNCTION__);
    InitializeListHead(&EmptyFrList);
    InitializeListHead(&DecodedFrList);
    InitializeListHead(&DeliveredFrList);

    b_refsw_client_client_configuration         config;
    b_refsw_client_client_info                  client_info;
    b_refsw_client_connect_resource_settings    connectSettings;

    //Validate the Nexus Codec Passed.
    BCMOMX_DBG_ASSERT((NxCodec > NEXUS_VideoCodec_eNone) && (NxCodec < NEXUS_VideoCodec_eMax)); 

    NxVideoCodec = NxCodec;

    ipcclient= NexusIPCClientFactory::getClient(CallerName);
    
    BKNI_Memset(&config, 0, sizeof(config));
    BKNI_Snprintf(config.name.string,sizeof(config.name.string),CallerName);
    config.resources.screen.required = true;
    config.resources.audioDecoder = false;
    config.resources.audioPlayback = false;            
    config.resources.videoDecoder = true;
    nexus_client = ipcclient->createClientContext(&config);

    if (nexus_client == NULL) 
    {
        LOG_ERROR("%s: Could not create client \"%s\" context!", __FUNCTION__, ipcclient->getClientName());
        LastErr = ErrStsClientCreateFail;
        return;
    }else {
        LOG_CREATE_DESTROY("%s: OMX client \"%s\" context created successfully: client=0x%p", 
                 __FUNCTION__, ipcclient->getClientName(), nexus_client);
    }

    ipcclient->getClientInfo(nexus_client, &client_info);

    // Now connect the client resources
    ipcclient->getDefaultConnectClientSettings(&connectSettings);
    connectSettings.simpleVideoDecoder[0].id = client_info.videoDecoderId;
    connectSettings.simpleVideoDecoder[0].surfaceClientId = client_info.surfaceClientId;
    connectSettings.simpleVideoDecoder[0].windowId = 0; /* Main Window */

    if (true != ipcclient->connectClientResources(nexus_client, &connectSettings)) 
    {
        LOG_ERROR("%s: Could not connect client \"%s\" resources!",
            __FUNCTION__, ipcclient->getClientName());

        LastErr = ErrStsConnectResourcesFail;
        return;
    }else{
        LOG_CREATE_DESTROY("%s: Connect Client \"%s\" Resources Success!",
            __FUNCTION__, ipcclient->getClientName());
    }

    decoHandle = ipcclient->acquireVideoDecoderHandle();
    if ( NULL == decoHandle )    
    {        
        LOG_ERROR("%s Unable to acquire Simple Video Decoder \n",__FUNCTION__); 
        LastErr = ErrStsDecoAcquireFail;
        return;
    }else{
        LOG_CREATE_DESTROY("%s: Acquired Video Decoder Handle", __FUNCTION__);
    }

    for (unsigned int i=0; i < DECODE_DEPTH; i++) 
    {
        PNEXUS_DECODED_FRAME pDecoFr = (PNEXUS_DECODED_FRAME) 
                        malloc(sizeof(NEXUS_DECODED_FRAME));
        if (!pDecoFr) 
        {
            LastErr = ErrStsOutOfResource;
            return;
        }
        pDecoFr->BuffState = BufferState_Init; 
        InitializeListHead(&pDecoFr->ListEntry);
        InsertHeadList(&EmptyFrList,&pDecoFr->ListEntry);
        EmptyFrListLen++;
        BCMOMX_DBG_ASSERT(EmptyFrListLen <= DECODE_DEPTH);
    }

    if(pSetPlatformContext)
    {
        pSetPlatformContext->SetPlatformSpecificContext(nexus_client);
    }

    if ( LastErr ) LOG_ERROR("%s: COMEPLETION WITH ERROR Error:%d", __FUNCTION__, LastErr); 
    LOG_CREATE_DESTROY("%s: EXIT ===",__FUNCTION__);
}

OMXNexusDecoder::~OMXNexusDecoder()
{
    LOG_CREATE_DESTROY("%s: ENTER ===",__FUNCTION__);
    DecoEvtLisnr=NULL;
    StopDecoder();
    NEXUS_SimpleVideoDecoder_Release(decoHandle);
    ipcclient->disconnectClientResources(nexus_client);
    ipcclient->destroyClientContext(nexus_client);

    //The Whole Free List Goes Away
    while (!IsListEmpty(&EmptyFrList)) 
    {
        PLIST_ENTRY ThisEntry =  RemoveTailList(&EmptyFrList);
        EmptyFrListLen--;
        PNEXUS_DECODED_FRAME pDecoFr = CONTAINING_RECORD(ThisEntry,NEXUS_DECODED_FRAME,ListEntry);
        free(pDecoFr);
    }

    //See If We are leaking some memory here....
    BCMOMX_DBG_ASSERT(!EmptyFrListLen);

    //Remaining in decoded list goes away
    while (!IsListEmpty(&DecodedFrList))
    {
        PLIST_ENTRY ThisEntry =  RemoveTailList(&DecodedFrList);
        DecodedFrListLen--;
        PNEXUS_DECODED_FRAME pDecoFr = CONTAINING_RECORD(ThisEntry,NEXUS_DECODED_FRAME,ListEntry);
        free(pDecoFr);
    }

    //See If We are leaking some memory here....
    BCMOMX_DBG_ASSERT(!DecodedFrListLen);

    //Remaining in Delivered list goes away
    while (!IsListEmpty(&DeliveredFrList))
    {
        PLIST_ENTRY ThisEntry =  RemoveTailList(&DeliveredFrList);
        PNEXUS_DECODED_FRAME pDecoFr = CONTAINING_RECORD(ThisEntry,NEXUS_DECODED_FRAME,ListEntry);
        free(pDecoFr);
    }
    
    delete ipcclient;
    LOG_CREATE_DESTROY("%s: EXIT ===",__FUNCTION__);
}


bool
OMXNexusDecoder::RegisterDecoderEventListener(DecoderEventsListener * RgstLstnr)
{
    if (!RgstLstnr) 
    {
        LOG_ERROR("%s: --Invalid Input Parameter--");
        LastErr = ErrStsInvalidParam;
        return false;
    }

    if (DecoEvtLisnr) 
    {
        LOG_ERROR("%s: --Listener Already Registered--");
        LastErr = ErrStsInvalidParam;
        return false;
    }

    DecoEvtLisnr = RgstLstnr;
    return true;
}


bool 
OMXNexusDecoder::StartDecoder(unsigned int ClientParam)
{
    return StartDecoder((NEXUS_PidChannelHandle)ClientParam);
}

bool
OMXNexusDecoder::StartDecoder(NEXUS_PidChannelHandle videoPIDChannel)
{
    NEXUS_Error NexusSts;
    NEXUS_SimpleVideoDecoderStartSettings videoProgram;
    VideoPid = (unsigned int) videoPIDChannel;

    if(IsDecoderStarted())
    {
        //Alreaday Started
        LOG_ERROR("%s DECODER Already Started",__FUNCTION__);
        LastErr = ErrStsSuccess;
        return true; //Not Really An Error
    }

    NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(&videoProgram);
    //videoProgram.settings.codec = NEXUS_VideoCodec_eH264;
    videoProgram.settings.codec = NxVideoCodec; 
    videoProgram.settings.pidChannel = videoPIDChannel;
    videoProgram.settings.appDisplayManagement = true; /* Set this so that you can get the frames back.*/
    videoProgram.displayEnabled=true;                  /* False for soft gfx */ 

    /* can't support two decoders if all are H265 format. */
    if (NEXUS_VideoCodec_eH265 == NxVideoCodec)
    {
        videoProgram.maxWidth  = 3840;
        videoProgram.maxHeight = 2160;
    }

    LOG_START_STOP_DBG("%s %d Issuing StartDecoder To hardware",__FUNCTION__,__LINE__);
    NexusSts = NEXUS_SimpleVideoDecoder_Start(decoHandle, &videoProgram);
    if(NexusSts != NEXUS_SUCCESS)
    {
        LOG_ERROR("%s: SimeplVideoDecoder Failed To \n",__FUNCTION__);
        LastErr = ErrStsStartDecoFailed;
        return false;
    }

    LOG_START_STOP_DBG("%s Simple Decoder Started Successfully With PID: 0x%x",__FUNCTION__,VideoPid);
    LastErr = ErrStsSuccess;
    return true;
}

void
OMXNexusDecoder::StopDecoder()
{
    NEXUS_Error nxErr = NEXUS_SUCCESS;
    
    if(IsDecoderStarted())
    {
        LOG_START_STOP_DBG("%s %d Issuing StopDecoder To hardware",__FUNCTION__,__LINE__);
        NEXUS_SimpleVideoDecoder_Stop(decoHandle);
    }
}

bool
OMXNexusDecoder::FrameAlreadyExist(PLIST_ENTRY pListHead, PNEXUS_DECODED_FRAME pNxDecFrame)
{
    if (IsListEmpty(pListHead)) 
    {
        return false;
    }

    return EntryExists(pListHead,&pNxDecFrame->ListEntry);
}

bool
OMXNexusDecoder::UnPauseDecoder()
{
    NEXUS_Error NxErr = NEXUS_SUCCESS;
    NEXUS_VideoDecoderTrickState DecoTrickState;
    NEXUS_SimpleVideoDecoder_GetTrickState(decoHandle,&DecoTrickState);

    if (DecoTrickState.rate) 
    {
        LOG_ERROR("%s Decoder Already UnPaused",__FUNCTION__);
        return true; //Not Really An Error
    }

    DecoTrickState.rate = NEXUS_NORMAL_DECODE_RATE;
    NxErr = NEXUS_SimpleVideoDecoder_SetTrickState(decoHandle,&DecoTrickState);
    if (NEXUS_SUCCESS == NxErr) 
    {
        LOG_INFO("%s: Decoder UnPaused ==",__FUNCTION__);
        return true; 
    }

    LOG_ERROR("%s Decoder UnPause Failed ==",__FUNCTION__);
    return false;
}

bool
OMXNexusDecoder::PauseDecoder()
{
    NEXUS_Error NxErr = NEXUS_SUCCESS;
    NEXUS_VideoDecoderTrickState DecoTrickState;
    NEXUS_SimpleVideoDecoder_GetTrickState(decoHandle,&DecoTrickState);

    if (!DecoTrickState.rate) 
    {
        LOG_ERROR("%s Decoder Already Paused",__FUNCTION__);
        return true; //Not Really An Error
    }

    DecoTrickState.rate = 0;
    NxErr = NEXUS_SimpleVideoDecoder_SetTrickState(decoHandle,&DecoTrickState);
    if (NEXUS_SUCCESS == NxErr) 
    {
        LOG_INFO("%s: Decoder Paused ==",__FUNCTION__);
        return true; 
    }

    LOG_ERROR("%s Decoder Pause Failed ==",__FUNCTION__);
    return false;
}

bool
OMXNexusDecoder::FlushDecoder()
{
    if (DecoEvtLisnr) DecoEvtLisnr->FlushStarted(); 

    StopDecoder();
    if (!StartDecoder(VideoPid))
    {
        LOG_ERROR("%s: Re-Starting Decoder For Flush Failed",__FUNCTION__);
        return false;
    }

    if (DecoEvtLisnr) DecoEvtLisnr->FlushDone(); 

    return true;
}

bool
OMXNexusDecoder::Flush()
{
    LOG_FLUSH_MSGS("%s FLUSH START", __FUNCTION__); 
    Mutex::Autolock lock(mListLock);
    FlushCnt++;

    // On RX Side We are not holding the commands from the above layer
    // We just need to make sure that we have all our buffers in Free State

    LOG_FLUSH_MSGS("%s: Flushing Delivered List: NextExpectedFrame:%d FlushCnt:%d",
                   __FUNCTION__,NextExpectedFr,FlushCnt);

    // Start the NextExpectedFr again from 1, we do not know what decoder will return
    // accept anything that decoder returns after flush. The counter will automatically
    // normalize itself WRT the first frame received.
    NextExpectedFr=1;
    
 
    LOG_FLUSH_MSGS("%s: Flushing Delivered List",__FUNCTION__);
    //Process the Delivered List
    while (!IsListEmpty(&DeliveredFrList)) 
    {
        PNEXUS_DECODED_FRAME pFrFromList = NULL;
        PLIST_ENTRY ThisEntry =  RemoveTailList(&DeliveredFrList);
        BCMOMX_DBG_ASSERT(ThisEntry);
        pFrFromList = CONTAINING_RECORD(ThisEntry,NEXUS_DECODED_FRAME,ListEntry);
        BCMOMX_DBG_ASSERT(pFrFromList);
        BCMOMX_DBG_ASSERT(pFrFromList->BuffState==BufferState_Delivered);
        pFrFromList->BuffState = BufferState_Init; 

        NEXUS_VideoDecoderReturnFrameSettings RetFrSetting;
        RetFrSetting.display = false;
        
        LOG_FLUSH_MSGS("%s Return [FLUSH-DROP] Frame [%d] Displayed:%s PTS:%lld [PTSValid:%d]",
                  __FUNCTION__,pFrFromList->FrStatus.serialNumber,
                       RetFrSetting.display ? "true":"false" ,
                       pFrFromList->MilliSecTS,
                       pFrFromList->FrStatus.ptsValid);

        NEXUS_SimpleVideoDecoder_ReturnDecodedFrames(decoHandle,&RetFrSetting,1);
        InsertHeadList(&EmptyFrList,&pFrFromList->ListEntry);
        EmptyFrListLen++;
        BCMOMX_DBG_ASSERT(EmptyFrListLen <= DECODE_DEPTH);
    }

    LOG_FLUSH_MSGS("%s: Flushing Decoded List",__FUNCTION__);
    //Process the Decoded List
    while (!IsListEmpty(&DecodedFrList)) 
    {
        PNEXUS_DECODED_FRAME pFrFromList = NULL;
        PLIST_ENTRY ThisEntry =  RemoveTailList(&DecodedFrList);
        DecodedFrListLen--;
        BCMOMX_DBG_ASSERT(ThisEntry);
        pFrFromList = CONTAINING_RECORD(ThisEntry,NEXUS_DECODED_FRAME,ListEntry);
        BCMOMX_DBG_ASSERT(pFrFromList);
        BCMOMX_DBG_ASSERT(pFrFromList->BuffState==BufferState_Decoded);
        pFrFromList->BuffState = BufferState_Init; 

        NEXUS_VideoDecoderReturnFrameSettings RetFrSetting;
        RetFrSetting.display = false;
        LOG_FLUSH_MSGS("%s Return [FLUSH-DROP] Frame [%d] Displayed:%s PTS:%lld [PTSValid:%d]",
                  __FUNCTION__,pFrFromList->FrStatus.serialNumber,
                       RetFrSetting.display ? "true":"false" ,
                       pFrFromList->MilliSecTS,
                       pFrFromList->FrStatus.ptsValid);

        NEXUS_SimpleVideoDecoder_ReturnDecodedFrames(decoHandle,&RetFrSetting,1);
        InsertHeadList(&EmptyFrList,&pFrFromList->ListEntry);
        EmptyFrListLen++;
        BCMOMX_DBG_ASSERT(EmptyFrListLen <= DECODE_DEPTH);
    }

    if (EmptyFrListLen != DECODE_DEPTH) 
    {
        LOG_ERROR("%s WE HAVE LOST SOME FRAMES ExpectedFLL[%d] RealFLL[%d]",
                 __FUNCTION__,
                 EmptyFrListLen,
                 DECODE_DEPTH);

        BCMOMX_DBG_ASSERT(DECODE_DEPTH == EmptyFrListLen);
        return false;
    }

    LOG_FLUSH_MSGS("%s: Now Flushing Hardware",__FUNCTION__);
    if(IsDecoderStarted())
    {
        FlushDecoder();
    }

    LOG_FLUSH_MSGS("%s FLUSH DONE", __FUNCTION__); 
    return true;
}

//
// Lock is Already Taken
//private
bool
OMXNexusDecoder::ReturnFrameSynchronized(PNEXUS_DECODED_FRAME pNxDecFrame, bool FlagDispFrame)
{
    if(false == IsDecoderStarted())
    {
        LastErr = ErrStsDecoNotStarted;
        LOG_ERROR("%s Decoder Not Started",__FUNCTION__);
        BCMOMX_DBG_ASSERT(false);
        return false;
    }

    if( IsListEmpty(&DeliveredFrList) )
    {
        //Nothing in the list
        LastErr = ErrStsDecodeListEmpty;
        LOG_ERROR("%s Nothing To Return From List, This May Happen After Flush",
                 __FUNCTION__);
        BCMOMX_DBG_ASSERT(false);
        return false;
    }

    if (!FrameAlreadyExist(&DeliveredFrList, pNxDecFrame))
    {
        //Frame does not exists..Nothing to remove
        LOG_ERROR("%s Return Frame Called But Frame Does Not Exists",__FUNCTION__);
        BCMOMX_DBG_ASSERT(false);
        return false;
    }

    while (!IsListEmpty(&DeliveredFrList)) 
    {
        bool Done=false;
        PNEXUS_DECODED_FRAME pFrFromList = NULL;
        PLIST_ENTRY ThisEntry =  RemoveTailList(&DeliveredFrList);
        pFrFromList = CONTAINING_RECORD(ThisEntry,NEXUS_DECODED_FRAME,ListEntry);

        if ( (pFrFromList) && 
             (pFrFromList->FrStatus.serialNumber <= pNxDecFrame->FrStatus.serialNumber))
        {
            NEXUS_VideoDecoderReturnFrameSettings RetFrSetting;
            RetFrSetting.display = false;

            if (pFrFromList->FrStatus.serialNumber == pNxDecFrame->FrStatus.serialNumber)     
            {
                Done = true;
                if(true == FlagDispFrame) RetFrSetting.display = true;
                
                if (pFrFromList != pNxDecFrame) 
                {
                    LOG_ERROR("%s: |**ERROR**| Frame Number Match But Addresses Dont!! This Should Never Happen!!",
                              __FUNCTION__);

                    BCMOMX_DBG_ASSERT(pFrFromList == pNxDecFrame);
                }
            }

            if (!RetFrSetting.display) 
            {
                LOG_WARNING("%s Return [DROP] Frame [%d] Displayed:%s PTS:%d [PTSValid:%d]", 
                            __FUNCTION__,pFrFromList->FrStatus.serialNumber,
                            RetFrSetting.display ? "true":"false" ,
                            pFrFromList->FrStatus.pts,
                            pFrFromList->FrStatus.ptsValid);
            }
            //Validate the Buffer State
            BCMOMX_DBG_ASSERT(pFrFromList->BuffState==BufferState_Delivered);
            pFrFromList->BuffState = BufferState_Init; 
            
            NEXUS_SimpleVideoDecoder_ReturnDecodedFrames(decoHandle,&RetFrSetting,1);   

            //Return the Frame Back To Empty List
            InsertHeadList(&EmptyFrList,&pFrFromList->ListEntry);
            EmptyFrListLen++;
            BCMOMX_DBG_ASSERT(EmptyFrListLen <= DECODE_DEPTH);
            if (Done) return true;

        }else{
            LOG_ERROR("%s: **ERROR** DeliveredFrList NOT IN PROPER ORDER, Not returning Anything!!",
                      __FUNCTION__);

            //Inserting back in Tail ONLY HERE so that the list state is not
            //changed. 
            if(ThisEntry) InsertTailList(&DeliveredFrList,ThisEntry);   
            return false; 
        }
    }
    
    LastErr = ErrStsRetFramesNoFrProcessed;
    return false;
}

bool
OMXNexusDecoder::DisplayFrame(PDISPLAY_FRAME pDispFr)
{
    Mutex::Autolock lock(mListLock);

    if (!pDispFr) 
    {
        LOG_ERROR("%s: Invalid Parameter In Display Frame",__FUNCTION__);
        LastErr = ErrStsInvalidParam;
        return false;
    }

    if (!pDispFr->DecodedFr || (pDispFr->DisplayCnxt != this) )  
    {
        LOG_ERROR("%s: Nothing To Display %p %p %p!!",
                  __FUNCTION__,
                  pDispFr->DecodedFr,
                  pDispFr->DisplayCnxt,
                  this);

        LastErr = ErrStsInvalidParam;
        return false;
    }

    if (false == IsDecoderStarted()) 
    {
        LastErr = ErrStsDecoNotStarted;
        LOG_ERROR("%s Decoder Not Started",__FUNCTION__);
        return false;
    }

    LastErr = ErrStsSuccess;
    PNEXUS_DECODED_FRAME pNxDecFrame = 
        (PNEXUS_DECODED_FRAME) pDispFr->DecodedFr; 

    return ReturnFrameSynchronized(pNxDecFrame ,true);
}

//Private
bool
OMXNexusDecoder::DropFrame(PNEXUS_DECODED_FRAME pNxDecFrame)
{
    if(false == IsDecoderStarted())
    {
        LastErr = ErrStsDecoNotStarted;
        LOG_ERROR("%s Decoder Not Started",__FUNCTION__);
        return false;
    }

    LastErr = ErrStsSuccess;
    return ReturnFrameSynchronized(pNxDecFrame,false);
}

void
OMXNexusDecoder::CheckAndClearBufferState(PDISPLAY_FRAME pOutputFr)
{
    PNEXUS_DECODED_FRAME pProcessFr;

    Mutex::Autolock lock(mListLock);
    
    //If we have not delivered anything, we have nothing to drop.
    if(IsListEmpty(&DeliveredFrList))
    {
        // This is not an error. We are checking every frame for its state 
        return; 
    }

    pProcessFr = (PNEXUS_DECODED_FRAME) pOutputFr->DecodedFr;

    if(!pProcessFr)
    {
        //This is not an Error, We are just chekcing an Empty buffer.
        LastErr = ErrStsInvalidParam;
        return;
    }

    // If The frame is delivered and just recycle
    // We need to drop that.
    if(BufferState_Delivered == pProcessFr->BuffState)
    {
        if(false == DropFrame(pProcessFr))
        {
            LOG_ERROR("%s: Drop Frame Failed!!\n\n",__FUNCTION__);    
            BCMOMX_DBG_ASSERT(false);
        }
    }
    
    if(BufferState_Delivered == pProcessFr->BuffState)
    {
        LOG_ERROR("%s: INVALID BUFFER STATE----ASSERTING NOW!!!!\n\n",__FUNCTION__);    
        BCMOMX_DBG_ASSERT(pProcessFr->BuffState == BufferState_Init);
    }
}

bool
OMXNexusDecoder::GetFramesFromHardware()
{
    PLIST_ENTRY             pListEntry = NULL;
    unsigned int            NumFramesToFetch=0;
    NEXUS_Error             errCode = NEXUS_NOT_SUPPORTED;

    Mutex::Autolock lock(mListLock);

    if (!IsListEmpty(&EmptyFrList)) 
        NumFramesToFetch = EmptyFrListLen;

    //
    // Decide on how many frame can you fetch...
    //
    if (NumFramesToFetch > DECODE_DEPTH) 
    {
        LOG_ERROR("%s ==ERROR== Trying To Fetch [%d] More Than The DECODE_DEPTH[%d] !!",
                  __FUNCTION__,NumFramesToFetch,DECODE_DEPTH);

        LastErr = ErrStsOutOfResource;
        return false;
    }

    if (NumFramesToFetch) 
    {
        unsigned int NumFramesOut;
        NEXUS_VideoDecoderFrameStatus OutFrameStatus[DECODE_DEPTH];

        errCode = NEXUS_SimpleVideoDecoder_GetDecodedFrames(decoHandle,
                                                    OutFrameStatus,
                                                    NumFramesToFetch, 
                                                    &NumFramesOut);

        if (errCode == NEXUS_SUCCESS)
        {
            for (unsigned int FrCnt=0 ; FrCnt < NumFramesOut ; FrCnt++) 
            {
                PNEXUS_DECODED_FRAME    pFreeFr=NULL;

                if(OutFrameStatus[FrCnt].serialNumber < NextExpectedFr)
                {
                    continue;
                }

                //Get A Free Entry...
                pListEntry =  RemoveTailList(&EmptyFrList);
                if (!pListEntry) 
                {
                    LOG_ERROR("%s: ==== ERROR Got A Frame But No Entry In Free List[%d]===",
                              __FUNCTION__,EmptyFrListLen);
                    LastErr = ErrStsListCorrupted;
                    BCMOMX_DBG_ASSERT(false);
                    return false;
                }

                EmptyFrListLen--;

                pFreeFr = CONTAINING_RECORD(pListEntry, NEXUS_DECODED_FRAME, ListEntry); 
                pFreeFr->FrStatus = OutFrameStatus[FrCnt];
                pFreeFr->ClientFlags=0;
                pFreeFr->BuffState = BufferState_Decoded; 
                pFreeFr->MilliSecTS =0;
                if(pFreeFr->FrStatus.ptsValid)
                {
                    // Time Stamp Is in 45 KHz time domain.
                    pFreeFr->MilliSecTS = pFreeFr->FrStatus.pts;
                }

                NextExpectedFr =  pFreeFr->FrStatus.serialNumber + 1;

                //Insert At Head... 
                InsertHeadList(&DecodedFrList,&pFreeFr->ListEntry);
                DecodedFrListLen++;
                BCMOMX_DBG_ASSERT(DecodedFrListLen <= DECODE_DEPTH);

                LastErr = ErrStsSuccess;

                LOG_OUTPUT_TS("%s  Accepted FRAME SeqNo:%d NxTimeStamp:%d Ts[Ms]:%lld!!",
                             __FUNCTION__, pFreeFr->FrStatus.serialNumber,
                             pFreeFr->FrStatus.pts, pFreeFr->MilliSecTS);

                DebugCounter = (DebugCounter + 1) % DECODER_STATE_FREQ;
                if(!DebugCounter) PrintDecoStats();
            }
        }else{
            LOG_ERROR("%s: ===NEXUS RETURNED ERROR WHILE GETTING FRAMES=== ",__FUNCTION__);
            LastErr = ErrStsNexusReturnedErr;
            return false;
        }
    }

    return true;
}

bool
OMXNexusDecoder::GetDecodedFrame(PDISPLAY_FRAME pOutFrame)
{
    if (!pOutFrame)
    {
        LOG_ERROR("%s: Invalid Parameters",__FUNCTION__);
        LastErr = ErrStsInvalidParam;
        return false;
    }

    Mutex::Autolock lock(mListLock);

    // Clear the output memory
    memset(pOutFrame,0,sizeof(DISPLAY_FRAME));

    //Deliver From DecodedQueue Tail (size -1)
    if(!IsListEmpty(&DecodedFrList)) 
    {
        PLIST_ENTRY DeliverEntry = RemoveTailList(&DecodedFrList);
        DecodedFrListLen--;

        BCMOMX_DBG_ASSERT(DecodedFrListLen <= DECODE_DEPTH);

        if (DeliverEntry) 
        {
            PNEXUS_DECODED_FRAME pDecoFr = CONTAINING_RECORD(DeliverEntry, NEXUS_DECODED_FRAME, ListEntry); 
            pDecoFr->BuffState = BufferState_Delivered; 

          //Clear the out flags.
            pOutFrame->OutFlags       = 0;
            pOutFrame->DisplayCnxt    = this;
            pOutFrame->DecodedFr      = pDecoFr;
            pOutFrame->pDisplayFn     = DisplayBuffer;

            InsertHeadList(&DeliveredFrList,&pDecoFr->ListEntry);

            LOG_INFO("%s  Delivered FRAME SeqNo:%d!!",
            __FUNCTION__,pDecoFr->FrStatus.serialNumber);

            return true;
        }
    }

	// If there is anything to deliver or display, We do not process EOS.
	// Let both the list get empty and then we will process the EOS.
	if(IsListEmpty(&DecodedFrList) && IsListEmpty(&DeliveredFrList))
	{
		BCMOMX_DBG_ASSERT(!DecodedFrListLen);

		pOutFrame->OutFlags=0;
		if (DetectEOS())
		{
			LOG_EOS_DBG("%s: Delivering EOS Frame ClientFlags[%d]",__FUNCTION__,ClientFlags);
			pOutFrame->OutFlags = ClientFlags;
			if (DecoEvtLisnr) DecoEvtLisnr->EOSDelivered();
			ClientFlags=0;
		}
	}else{
		DownCnt=DOWN_CNT_DEPTH;
	}

    LastErr = ErrStsNexusGetDecodedFrFail;
    return false;
}

unsigned int
OMXNexusDecoder::GetFrameTimeStampMs(PDISPLAY_FRAME pFrame)
{
    PNEXUS_DECODED_FRAME pNxDecoFr;
    if (!pFrame)  return 0;

    pNxDecoFr = (PNEXUS_DECODED_FRAME) pFrame->DecodedFr;
    if (!pNxDecoFr) return 0;
    return pNxDecoFr->MilliSecTS;
}

bool
OMXNexusDecoder::IsDecoderStarted()
{
    NEXUS_VideoDecoderStatus vdecStatus;

    if(GetDecoSts(&vdecStatus))
    {
        return vdecStatus.started;
    }else{
        LOG_ERROR("%s Failed To Get Decoder State",__FUNCTION__);
    }

    return false;
}

void 
OMXNexusDecoder::PrintDecoStats()
{
    NEXUS_VideoDecoderStatus vdecStatus;
    if(GetDecoSts(&vdecStatus))
    {
        LOG_DECODER_STATE("%s: ====== DECODER STATS ====== \n Started:%d FrameRate:%d TsmMode:%s PTS-Type:%d "
                          "PTSErrCnt:%d PicsDecoded:%d BuffLatency[Ms]:%d numDispDrops:%d\n===============",
                         __FUNCTION__,
                         vdecStatus.started,
                         vdecStatus.frameRate,
                         vdecStatus.tsm ? "True":"False",
                         vdecStatus.ptsType,
                         vdecStatus.ptsErrorCount,
                         vdecStatus.numDecoded,
                         vdecStatus.bufferLatency,
                         vdecStatus.numDisplayDrops);
    }
}

ErrorStatus 
OMXNexusDecoder::GetLastError()
{
    return LastErr;
}

bool 
OMXNexusDecoder::GetDecoSts(NEXUS_VideoDecoderStatus *vdecStatus)
{
    NEXUS_Error nxErr = NEXUS_SUCCESS;
    nxErr = NEXUS_SimpleVideoDecoder_GetStatus(decoHandle, vdecStatus);
    if(nxErr== NEXUS_SUCCESS)
    {
        LastErr = ErrStsSuccess;
        return true;
    }
    
    LOG_ERROR("%s Failed To Get Decoder State",__FUNCTION__);
    LastErr = ErrStsNexusGetDecoStsFailed;
    return false;
}

/**
 * InputEOSReceived: 
 * @brief : This function notifies that the EOS is received on 
 *       the input side. Any logic to enable the EOS detection
 *       in this class should start at this point.
 * 
 *  
 * @param ClientFlags : The Flags that are requied to be passed 
 *                    back when the EOS Frame is detected.
 * 
 * @return bool 
 */

void 
OMXNexusDecoder::InputEOSReceived(unsigned int ClientFlagsData)
{
    LOG_EOS_DBG("%s: Entering EOS Detection Mode",__FUNCTION__);

    // Get in to EOS Detection Mode....
    StartEOSDetection=true;

    //Save The Flags that Client Wants Us To Set In The 
    //Output frames On Detection Of EOS.
    ClientFlags = ClientFlagsData;
    DownCnt = DOWN_CNT_DEPTH;
}

bool 
OMXNexusDecoder::DetectEOS()
{
    NEXUS_VideoDecoderStatus  VidDecoSts;

    if(false == StartEOSDetection)
    {
        return false;
    }
    
    LOG_EOS_DBG("%s: EOSDetect[%s] ",__FUNCTION__,StartEOSDetection ? "TRUE":"FALSE");
    
    GetDecoSts(&VidDecoSts);    
    if (VidDecoSts.started) 
    {
        if (DownCnt)
        {
            DownCnt--;
        }else{
            
            LOG_EOS_DBG("%s: --- EOS DETECTED ---",__FUNCTION__);

            //Just have to detect the EOS only once
            StartEOSDetection=false;
            return true;
        }
    }

    return false;
}

bool
DisplayBuffer (void *pInFrame, void *pInOMXNxDeco)
{
    PDISPLAY_FRAME pFrame = (PDISPLAY_FRAME) pInFrame;
    OMXNexusDecoder *pOMXNxDeco = (OMXNexusDecoder *) pInOMXNxDeco;

    if(!pFrame || !pOMXNxDeco || !pFrame->DecodedFr)
    {
        LOG_ERROR("%s Invalid Parameters To Display Function %p %p %p",
                  __FUNCTION__,
                  pFrame,
                  pOMXNxDeco,
                  pFrame->DecodedFr);

        return false;
    }
        
    return pOMXNxDeco->DisplayFrame(pFrame);
}

