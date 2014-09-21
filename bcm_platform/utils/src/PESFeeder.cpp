
#include "PESFeeder.h"


#define LOG_START_STOP_DBG      ALOGD
#define LOG_EOS_DBG             ALOGD
#define LOG_FLUSH_MSGS          ALOGD
#define LOG_CREATE_DESTROY      ALOGD
#define LOG_CONFIG_MSGS         ALOGD

#define LOG_INFO                
#define LOG_ERROR               ALOGD
#define LOG_WARNING             ALOGD

static void DescDoneCallBack(void *context, int param)
{
    PESFeeder *pPESFeeder = (PESFeeder *) context;
    pPESFeeder->XFerDoneCallBack();
}

static void EOSXferDoneCallBack(unsigned int Param1, 
        unsigned int Param2, 
        unsigned int Param3)
{
    LOG_EOS_DBG("%s:  EOS Xfer Complete",__FUNCTION__);
    return;
}

#ifndef GENERATE_DUMMY_EOS
#define B_STREAM_ID 0xe0

static void FormBppPacket(unsigned char *pBuffer, uint32_t opcode)
{
    BKNI_Memset(pBuffer, 0, 184);
    /* PES Header */
    pBuffer[0] = 0x00;
    pBuffer[1] = 0x00;
    pBuffer[2] = 0x01;
    pBuffer[3] = B_STREAM_ID;
    pBuffer[4] = 0x00;
    pBuffer[5] = 178;
    pBuffer[6] = 0x81;
    pBuffer[7] = 0x01;
    pBuffer[8] = 0x14;
    pBuffer[9] = 0x80;
    pBuffer[10] = 0x42; /* B */
    pBuffer[11] = 0x52; /* R */
    pBuffer[12] = 0x43; /* C */
    pBuffer[13] = 0x4d; /* M */
    /* 14..25 are unused and set to 0 by above memset */
    pBuffer[26] = 0xff;
    pBuffer[27] = 0xff;
    pBuffer[28] = 0xff;
    /* sub-stream id - not used in this config */;
    /* Next 4 are opcode 0000_000ah = inline flush/tpd */
    pBuffer[30] = (opcode>>24) & 0xff;
    pBuffer[31] = (opcode>>16) & 0xff;
    pBuffer[32] = (opcode>>8) & 0xff;
    pBuffer[33] = (opcode>>0) & 0xff;
}
#endif

bool
PESFeeder::IsPlayPumpStarted()
{
    NEXUS_Error rc = 0;
    NEXUS_PlaypumpStatus NxPlayPumpStatus;
    rc = NEXUS_Playpump_GetStatus(NxPlayPumpHandle, &NxPlayPumpStatus);

    BCMOMX_DBG_ASSERT(rc == NEXUS_SUCCESS);
    return NxPlayPumpStatus.started;
}

void
PESFeeder::StopPlayPump()
{
    if (IsPlayPumpStarted()) 
    {
        LOG_START_STOP_DBG("[%s]%s: Issuing Stop To PlayPump", 
                ClientIDString,__FUNCTION__); 

        NEXUS_Playpump_Stop(NxPlayPumpHandle);
        LOG_START_STOP_DBG("[%s]%s: PlayPump Stopped",
                ClientIDString,__FUNCTION__);
    }
}

void
PESFeeder::StartPlayPump()
{
    if (false == IsPlayPumpStarted()) 
    {
        LOG_START_STOP_DBG("[%s]%s: Starting The PlayPump !!", ClientIDString, __FUNCTION__);
        if( NEXUS_SUCCESS != NEXUS_Playpump_Start(NxPlayPumpHandle))
        {
            LOG_ERROR("[%s]%s: === NEXUS_Playpump_Start FAILED ===",
                    ClientIDString,__FUNCTION__);
            return;
        }
        LOG_START_STOP_DBG("[%s]%s: PlayPump Started!!",ClientIDString,__FUNCTION__);
    }
}

PESFeeder::PESFeeder(char const *ClientName,
        unsigned int InputPID, 
        unsigned int NumDescriptors, 
        NEXUS_TransportType NxTransType,
        NEXUS_VideoCodec vidCdc) :
        FdrEvtLsnr(NULL), NexusPID(InputPID), CntDesc(NumDescriptors),
        FiredCnt(0), FlushCnt(0), SendCfgDataOnNextInput(false),
        pCfgDataMgr(NULL), vidCodec(vidCdc),pInterNotifyCnxt(NULL),
        LastInputTimeStamp(0)
#ifdef DEBUG_PES_DATA
    , DataBeforFlush(new DumpData("DataBeforFlush"))
    , DataAfterFlush(new DumpData("DataAfterFlush"))
#endif
{
    strcpy((char *)ClientIDString,ClientName);
    LOG_CREATE_DESTROY("[%s]%s: Enter",ClientIDString,__FUNCTION__);

    NEXUS_PlaypumpOpenSettings  PlayPumpOpenSettings;
    NEXUS_PlaypumpSettings      PlayPumpSettings;
    NEXUS_ClientConfiguration   clientConfig;

    InitializeListHead(&ActiveQ);

    NEXUS_Playpump_GetDefaultOpenSettings(&PlayPumpOpenSettings);


    PlayPumpOpenSettings.numDescriptors = NumDescriptors;
    PlayPumpOpenSettings.fifoSize =0; //We are not using playpump in FIFIO mode

    NxPlayPumpHandle = NEXUS_Playpump_Open(NEXUS_ANY_ID,&PlayPumpOpenSettings);
    if (NxPlayPumpHandle == NULL)
    {
        LOG_ERROR("[%s]%s: ====COULD NOT OPEN PLAYPUMP ====\n",ClientIDString,__FUNCTION__);
        return;
    }

    NEXUS_Playpump_GetDefaultSettings(&PlayPumpSettings);

    if (NxTransType == NEXUS_TransportType_eAsf)
    {
        PlayPumpSettings.transportType = NEXUS_TransportType_eMpeg2Pes;
        PlayPumpSettings.originalTransportType  = NxTransType;      
    }else{
        PlayPumpSettings.transportType = NxTransType;
    }
    PlayPumpSettings.dataCallback.callback = DescDoneCallBack;
    PlayPumpSettings.dataCallback.context = this;
    PlayPumpSettings.dataCallback.param = NULL;
    //PlayPumpSettings.dataNotCpuAccessible = true;
    NEXUS_Playpump_SetSettings(NxPlayPumpHandle, &PlayPumpSettings);

    NxVidPidChHandle = NEXUS_Playpump_OpenPidChannel(NxPlayPumpHandle, 
            NexusPID, 
            NULL);

    if(NxVidPidChHandle == NULL)
    {
        LOG_ERROR("Unable To Get The PID Channel Handle For PID:%d", NexusPID);
        NEXUS_Playpump_Close(NxPlayPumpHandle);
        return;
    }

    StartPlayPump();
    AtomFactory = batom_factory_create(bkni_alloc, 128);
    if (!AtomFactory)
    {
        AtomFactory = 0;
        LOG_ERROR("Constructor Failed batom_factory_create failed!!");
        NEXUS_Playpump_ClosePidChannel(NxPlayPumpHandle,NxVidPidChHandle);
        NEXUS_Playpump_Close(NxPlayPumpHandle);
        return;
    }

    AccumulatorObject = batom_accum_create(AtomFactory);
    if (!AccumulatorObject) 
    { 
        AccumulatorObject = 0;
        LOG_ERROR("Constructor Failed Failed to create accumulator object!!");
        NEXUS_Playpump_ClosePidChannel(NxPlayPumpHandle,NxVidPidChHandle);
        NEXUS_Playpump_Close(NxPlayPumpHandle);
        batom_factory_destroy(AtomFactory);
        return; 
    }
    
    //NEXUS_Memory_Allocate(PES_EOS_BUFFER_SIZE,NULL,(void **) &pPESEosBuffer);
    pPESEosBuffer = (unsigned char *) AllocatePESBuffer(PES_EOS_BUFFER_SIZE);
    NEXUS_Memory_Allocate(sizeof(NEXUS_INPUT_CONTEXT),NULL,(void **) &pInterNotifyCnxt);

    //
    // Prepare the EOS Data Once And Use It For EOS
    //
    PrepareEOSData();

    pCfgDataMgr = CreateCfgDataMgr(vidCodec,*this);
    LOG_CREATE_DESTROY("[%s]%s: Exit",ClientIDString,__FUNCTION__);
}

PESFeeder::~PESFeeder()
{
    LOG_CREATE_DESTROY("[%s]%s: ENTER",ClientIDString,__FUNCTION__);
    FdrEvtLsnr= NULL;

    if (NxPlayPumpHandle) 
    {
        NEXUS_PlaypumpStatus NxPlayPumpStatus;
        NEXUS_Playpump_GetStatus(NxPlayPumpHandle, &NxPlayPumpStatus);
        if (NxPlayPumpStatus.started) 
        {
            LOG_CREATE_DESTROY("[%s]%s: Calling StopPlayPump",ClientIDString,__FUNCTION__);
            StopPlayPump();

            LOG_CREATE_DESTROY("[%s]%s: Found PlayPump Started == ",ClientIDString,__FUNCTION__);
            if (NxVidPidChHandle) 
            {
                LOG_CREATE_DESTROY("[%s]%s: Closing PID Channel",ClientIDString,__FUNCTION__);
                NEXUS_Playpump_ClosePidChannel(NxPlayPumpHandle,NxVidPidChHandle);
                NxVidPidChHandle=NULL;
            }
        }
        LOG_CREATE_DESTROY("[%s]%s: Closing PlayPump",ClientIDString,__FUNCTION__);
        NEXUS_Playpump_Close(NxPlayPumpHandle);
        NxPlayPumpHandle=NULL;
    }

    /*                                                            
     * Just Make Sure That you do not have anything in the queue  
     * This is just a error checking code to make sure that we
     * do not destroy the object with user buffers with us.
     * In other words, when we destroy the object the fired Count
     * should always be zero.
     */ 
    while (!IsListEmpty(&ActiveQ)) 
    {
        PLIST_ENTRY pDoneEntry = RemoveTailList(&ActiveQ);
        FiredCnt--;
    }
    if(FiredCnt)
    {
        LOG_ERROR("[%s]%s: Destroying Object But There Are Commands That are Fire To Hardware!! \n ++ASSERTING++",ClientIDString,__FUNCTION__);
        BCMOMX_DBG_ASSERT(FiredCnt==0);
    }

    if (AccumulatorObject) 
    {
        batom_accum_destroy(AccumulatorObject);
        AccumulatorObject=NULL;
    }

    if(AtomFactory)
    {
        batom_factory_destroy(AtomFactory);
        AtomFactory=NULL;
    }

    if (pInterNotifyCnxt)
    {
        NEXUS_Memory_Free(pInterNotifyCnxt);
        pInterNotifyCnxt=NULL;
    }

    if (pPESEosBuffer)
    {
        FreePESBuffer(pPESEosBuffer);
        pPESEosBuffer=NULL;
    }

    if (pCfgDataMgr)
    {
        delete pCfgDataMgr;
        pCfgDataMgr=NULL;
    }

#ifdef DEBUG_PES_DATA
    delete DataBeforFlush;
    delete DataAfterFlush;
#endif

    LOG_CREATE_DESTROY("[%s]%s: EXIT", ClientIDString,__FUNCTION__); 
}

void * 
PESFeeder::AllocatePESBuffer(size_t SzPESBuffer)
{
    void * retMem=NULL;
    NEXUS_Error errCode=NEXUS_SUCCESS;
    NEXUS_ClientConfiguration               clientConfig;
    NEXUS_MemoryAllocationSettings          memSetting;

    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    NEXUS_Memory_GetDefaultAllocationSettings(&memSetting);
    memSetting.heap = clientConfig.heap[1];
    errCode = NEXUS_Memory_Allocate(SzPESBuffer,
            &memSetting,
            &retMem);   


    BCMOMX_DBG_ASSERT( (errCode==NEXUS_SUCCESS) || retMem);
    if ( (errCode != NEXUS_SUCCESS)  || (!retMem))
    {
        LOG_ERROR("[%s]%s: Nexus Memory Allocation Failure Requested:%d",
                ClientIDString,__FUNCTION__,SzPESBuffer);

        return NULL;
    }

    return retMem; 
}

void 
PESFeeder::FreePESBuffer(void *pMem)
{
    NEXUS_Memory_Free(pMem);
}


bool
PESFeeder::RegisterFeederEventsListener(FeederEventsListener *pInEvtLisnr)
{
    if (!pInEvtLisnr) 
    {
        LOG_ERROR("[%s]%s: Invalid Input Parameter",ClientIDString,__FUNCTION__);
        return false;
    }

    if (FdrEvtLsnr) 
    {
        LOG_ERROR("[%s]%s: Feeder Event Listener Already Registered",ClientIDString,__FUNCTION__);
        return false;
    }

    FdrEvtLsnr = pInEvtLisnr;
    return true;
}

bool
PESFeeder::StartDecoder(StartDecoderIFace *pStartDecoIface)
{
    if (!pStartDecoIface)
    {
        LOG_ERROR("[%s]%s: Failed To Start The Decoder",ClientIDString,__FUNCTION__);
        return false;
    }

    return pStartDecoIface->StartDecoder((unsigned int)NxVidPidChHandle);
}

size_t 
PESFeeder::InitiatePESHeader(unsigned int pts45KHz,
        size_t SzDataBuff,
        unsigned char *pHeaderBuff,
        size_t SzHdrBuff)
{
    bmedia_pes_info pes_info;
    size_t SzPESHeader=0;

    if( (!SzDataBuff) || (!pHeaderBuff) || (!SzHdrBuff) )
    {
        LOG_ERROR("[%s]%s: Invalid Parameters",ClientIDString,__FUNCTION__);
        return 0;
    }

    /*Initialize the PES info structure*/
    bmedia_pes_info_init(&pes_info, NexusPID);

    if(pts45KHz == -1)  /* VC1 simple and main profile will use it for config data special handling */
    {
        pes_info.pts_valid = true;
        pes_info.pts = (uint32_t)0xFFFFFFFFF;
    }
    else 
    {
        pes_info.pts_valid = true;
        pes_info.pts = (uint32_t)pts45KHz;
    }

    SzPESHeader = bmedia_pes_header_init(pHeaderBuff, SzDataBuff, &pes_info);
    if (SzPESHeader > SzHdrBuff) 
    {
        LOG_ERROR("[%s]%s: Header Length More Than The Provided Header Buffer",
                ClientIDString,__FUNCTION__);

        BCMOMX_DBG_ASSERT(false);
        return 0;
    }

    batom_accum_clear(AccumulatorObject); 
    batom_accum_add_range(AccumulatorObject, pHeaderBuff, SzPESHeader); 
    return SzPESHeader;
}   

//
// Prepare the Data And Copy it to the output 
// Buffer. 
//
size_t
PESFeeder::ProcessESData(
        unsigned int    pts, 
        unsigned char   *pDataBuffer, 
        size_t          SzDataBuff,
        unsigned char   *pOutData,
        size_t          SzOutData)
{
    unsigned int SzPESHeader=0;
    unsigned int SzTotalPESData=0,SzTotalInData=0;
    unsigned char PESHeader[PES_HEADER_SIZE];
    size_t SzCfgData=0;

    if( (!pDataBuffer) || (!pOutData) || (!SzDataBuff) || (!SzOutData))
    {
        LOG_ERROR("[%s]%s: Invalid Parameters",ClientIDString,__FUNCTION__);
        return 0;
    }

    SzTotalInData = SzDataBuff;
    SzCfgData = pCfgDataMgr->GetConfigDataSz();

    BCMOMX_DBG_ASSERT(!(SendCfgDataOnNextInput && 0==SzCfgData));

    if (SendCfgDataOnNextInput) 
    {
        SzTotalInData = SzDataBuff + SzCfgData;
    }

    SzPESHeader = InitiatePESHeader(pts, SzTotalInData, PESHeader, PES_HEADER_SIZE); 
    if( (!SzPESHeader) || (SzPESHeader > SzOutData))
    {
        LOG_ERROR("[%s]%s: Failed to Initiate the PES header",ClientIDString,__FUNCTION__);
        return 0;
    }

    if (SendCfgDataOnNextInput) 
    {
        //Add The Range For Saved ConfigData
        LOG_FLUSH_MSGS("[%s]%s: Adding Configuration Data To Payload :%d",
                ClientIDString,__FUNCTION__,
                SzCfgData);

        pCfgDataMgr->AccumulateConfigData(AccumulatorObject);
        SendCfgDataOnNextInput=false;
    }

    //Add the Range For actual Data
    batom_accum_add_range(AccumulatorObject, pDataBuffer, SzDataBuff);

    //Get The Cursor
    batom_cursor_from_accum(&CursorToAccum, AccumulatorObject);

    //Get the size of the cursor
    SzTotalPESData = batom_cursor_size(&CursorToAccum);

    if ( (SzTotalPESData > SzOutData)  ||
            (SzTotalPESData < (SzPESHeader + SzDataBuff)) )
    {
        LOG_ERROR("[%s]%s: Failed to Initiate the PES header",ClientIDString,__FUNCTION__);
        return 0;
    }

    size_t returnSz =  CopyPESData(pOutData, SzOutData); 

    BCMOMX_DBG_ASSERT(returnSz == SzTotalPESData);
    //Copy the PES data to the ouput buffer...
    return returnSz;
}

size_t
PESFeeder::ProcessESData(unsigned int pts,
        unsigned char *pHeaderToInsert,
        size_t SzHdrBuff,
        unsigned char *pDataBuffer, 
        size_t SzDataBuff,
        unsigned char *pOutData,
        size_t SzOutData)
{ 
    unsigned int SzPESHeader=0;
    unsigned int SzTotalPESData=0,SzTotalInData=0;
    unsigned char PESHeader[PES_HEADER_SIZE];
    size_t SzCfgData=0;

    if ( (!pHeaderToInsert) || (!SzHdrBuff) || (!pDataBuffer) || (!SzDataBuff) || (!pOutData) ||  (!SzOutData) )
    {
        LOG_ERROR("%s[%d] Invalid Input Parameters",__FUNCTION__,__LINE__);
        return 0;
    }

    SzTotalInData = SzDataBuff + SzHdrBuff; 

    SzCfgData = pCfgDataMgr->GetConfigDataSz();

    BCMOMX_DBG_ASSERT(!(SendCfgDataOnNextInput && 0==SzCfgData));

    if (SendCfgDataOnNextInput) 
    {
        SzTotalInData += SzCfgData; //Add The Config Data Size....
    }

    SzPESHeader = InitiatePESHeader(pts, SzTotalInData, PESHeader, PES_HEADER_SIZE); 
    if( (!SzPESHeader) || (SzPESHeader > SzOutData))
    {
        LOG_ERROR("[%s]%s: Failed to Initiate the PES header Increase the Outbuffer Size",ClientIDString,__FUNCTION__);
        return 0;
    }

    //Add the Range For Additional Header
    batom_accum_add_range(AccumulatorObject, pHeaderToInsert, SzHdrBuff);

    if (SendCfgDataOnNextInput) 
    {
        //Add The Range For Saved ConfigData
        LOG_FLUSH_MSGS("[%s]%s: Adding Configuration Data To Payload :%d",
                ClientIDString,__FUNCTION__,
                SzCfgData);

        pCfgDataMgr->AccumulateConfigData(AccumulatorObject);
        SendCfgDataOnNextInput=false;
    }

    //Add the Range For actual Data
    batom_accum_add_range(AccumulatorObject, pDataBuffer, SzDataBuff);

    //Get The Cursor
    batom_cursor_from_accum(&CursorToAccum, AccumulatorObject);

    //Get the size of the cursor
    SzTotalPESData = batom_cursor_size(&CursorToAccum);

    if ( (SzTotalPESData > SzOutData)  ||
            (SzTotalPESData < (SzPESHeader + SzDataBuff + SzHdrBuff)) )
    {
        LOG_ERROR("[%s]%s: Failed to Initiate the PES header",ClientIDString,__FUNCTION__);
        return 0;
    }

    size_t returnSz =  CopyPESData(pOutData, SzOutData); 
    BCMOMX_DBG_ASSERT(returnSz == SzTotalPESData);

    //Copy the PES data to the ouput buffer...
    return returnSz;
}

/************************************************************************
 * Summary: The function copies the PES data to the provided buffer. 
 * Returns Amount of data to be copied which may be less than the 
 * required size in case there is less data. This is a safe copy
 * In case the data is more and the provided buffer is less only 
 * the data equal to the size of the provided buffer will be 
 * copied.   
 *************************************************************************/

size_t 
PESFeeder::CopyPESData(void *pBuffer, size_t bufferSz)
{
    size_t copiedSz=0;

    if( (!pBuffer) || (!bufferSz))
    {
        LOG_ERROR("[%s]%s: Invalid Parameters",ClientIDString,__FUNCTION__);
        return 0;
    }

    copiedSz = batom_cursor_copy(&CursorToAccum, pBuffer, bufferSz);
    return copiedSz;
}

bool
PESFeeder::Flush()
{
    NEXUS_PlaypumpStatus NxPlayPumpStatus;

    Mutex::Autolock lock(mListLock);
    NEXUS_Playpump_GetStatus(NxPlayPumpHandle, &NxPlayPumpStatus);
    FlushCnt++;

    Pauser PausePlayPump(NxPlayPumpHandle);

    LOG_FLUSH_MSGS("[%s]%s: Fired %d Descriptors, Flushing Them",
            ClientIDString,__FUNCTION__,FiredCnt);

    if(NxPlayPumpStatus.started)
    {
        //Discard All The Data In The Play Pump
        NEXUS_Playpump_Flush(NxPlayPumpHandle);
        LOG_FLUSH_MSGS("[%s]%s: PlayPump Is Flushed",ClientIDString,__FUNCTION__);
    }

    while (!IsListEmpty(&ActiveQ)) 
    {
        PLIST_ENTRY pDoneEntry = RemoveTailList(&ActiveQ);
        BCMOMX_DBG_ASSERT(pDoneEntry);

        PNEXUS_INPUT_CONTEXT pDoneCnxt = 
            CONTAINING_RECORD(pDoneEntry,NEXUS_INPUT_CONTEXT,ListEntry);

        if (pDoneCnxt->DoneContext.pFnDoneCallBack) 
        {
            //Notify the Caller that the Buffer is Done
            pDoneCnxt->DoneContext.pFnDoneCallBack(pDoneCnxt->DoneContext.Param1,
                    pDoneCnxt->DoneContext.Param2,
                    pDoneCnxt->DoneContext.Param3); 
        }

        FiredCnt--;
    }

    if (FiredCnt) 
    {
        LOG_ERROR("[%s]%s: Invalid FIRED Count [%d] After Flush ",
                ClientIDString,__FUNCTION__,
                FiredCnt);
        BCMOMX_DBG_ASSERT(0 == FiredCnt); 
        return false;
    }

    LOG_FLUSH_MSGS("[%s]%s: [%d] Complete ",ClientIDString,__FUNCTION__,FlushCnt);    
    return true;
}

void 
PESFeeder::FlushStarted()
{
    LOG_FLUSH_MSGS("[%s]%s: Decoder Flush Started",
            ClientIDString,__FUNCTION__);

    StopPlayPump();
    return;
}

void 
PESFeeder::FlushDone()
{
    size_t SzCfgData;
    LOG_FLUSH_MSGS("[%s]%s: Decoder Flush Done Notified--Config Data Will Be Sent On Next IO",
            ClientIDString,__FUNCTION__);

    StartPlayPump();

    SzCfgData = pCfgDataMgr->GetConfigDataSz();

    //Send Configuration if there is anything to send
    if(SzCfgData) 
    {
        SendCfgDataOnNextInput=true;
        pCfgDataMgr->SendConfigDataToHW();
    }
}

void 
PESFeeder::EOSDelivered()
{
    /* 
     * The Decoder has actually sent the last frame Marked with EOS
     * The Playback Will stop Now. Invalidate the Config Data.
     */
    LOG_EOS_DBG("[%s]%s: EOS Delivered To Output", ClientIDString,__FUNCTION__);

    /* need the config data while looping playback */
    //DiscardConfigData();
}

void 
PESFeeder::XFerDoneCallBack()
{
    unsigned int DescDoneCnt=0;
    NEXUS_PlaypumpStatus NxPlayPumpStatus;

    Mutex::Autolock lock(mListLock);
    NEXUS_Playpump_GetStatus(NxPlayPumpHandle, &NxPlayPumpStatus);

    if (!NxPlayPumpStatus.started || !FiredCnt) 
    {
        // Spurious Callback can be fired when we stop the playpump 
        // And it has some IO pending. 

        LOG_INFO("[%s]%s: ==WARNING==>> Playpump Stated:%s FiredCnt:%d !!",
                ClientIDString,__FUNCTION__,
                NxPlayPumpStatus.started ? "true":"false", 
                FiredCnt);
        return;
    }

    if (FiredCnt < NxPlayPumpStatus.descFifoDepth) 
    {
        LOG_ERROR("[%s]%s: ##==ERROR==## Fired Count[%d] < NxPlayPumpStatus.descFifoDepth[%d]",
                ClientIDString,__FUNCTION__,FiredCnt,NxPlayPumpStatus.descFifoDepth);

        return;
    }

    DescDoneCnt = FiredCnt -  NxPlayPumpStatus.descFifoDepth; 

    LOG_INFO("[%s]%s: DescDoneCnt:%d = [%d-%d]",
            ClientIDString,__FUNCTION__,
            DescDoneCnt,
            FiredCnt,
            NxPlayPumpStatus.descFifoDepth);

    for (unsigned int i=0; i < DescDoneCnt; i++) 
    {
        BCMOMX_DBG_ASSERT(!IsListEmpty(&ActiveQ));    
        PLIST_ENTRY pDoneEntry = RemoveTailList(&ActiveQ);
        BCMOMX_DBG_ASSERT(pDoneEntry);                    
        PNEXUS_INPUT_CONTEXT pDoneCnxt = CONTAINING_RECORD(pDoneEntry,NEXUS_INPUT_CONTEXT,ListEntry);

        if (pDoneCnxt->DoneContext.pFnDoneCallBack) 
        {
            //Fire The Call Back
            pDoneCnxt->DoneContext.pFnDoneCallBack(pDoneCnxt->DoneContext.Param1,
                    pDoneCnxt->DoneContext.Param2,
                    pDoneCnxt->DoneContext.Param3); 
        }

        FiredCnt--;
    }

    return; 
}

bool
PESFeeder::SendPESDataToHardware(PNEXUS_INPUT_CONTEXT pNxInCnxt)
{
    size_t NumConsumed=0;
    NEXUS_Error errCode;

    if (!pNxInCnxt || !pNxInCnxt->SzValidPESData) 
    {
        LOG_ERROR("[%s]%s: Invalid Parameters = NO VALID DATA TO SEND %p Sz:%d",
                ClientIDString,__FUNCTION__,
                pNxInCnxt,
                pNxInCnxt ? pNxInCnxt->SzValidPESData:0);

        return false;
    }

    Mutex::Autolock lock(mListLock);
    InitializeListHead(&pNxInCnxt->ListEntry);
    if (FiredCnt > CntDesc) 
    {
        LOG_ERROR("[%s]%s: == ERROR We Have Already Fired All The Descriptors [HOW??]== ",
                ClientIDString,__FUNCTION__);
        return false;
    }

    pNxInCnxt->NxDesc[0].addr       = pNxInCnxt->PESData;
    pNxInCnxt->NxDesc[0].length     = pNxInCnxt->SzValidPESData;

    // The Last Time Stamp That We sent To Hardware...
    if(pNxInCnxt->FramePTS)
    {
        LOG_INFO("%s: Updating LastTimeStamp: %lld", LastInputTimeStamp);
        LastInputTimeStamp = pNxInCnxt->FramePTS;
    }

#ifdef DEBUG_PES_DATA
    if (!FlushCnt) {
        DataBeforFlush->WriteData(pNxInCnxt->PESData,pNxInCnxt->SzValidPESData);
    }else if(1==FlushCnt){
        DataAfterFlush->WriteData(pNxInCnxt->PESData,pNxInCnxt->SzValidPESData);
    }
#endif

    errCode = NEXUS_Playpump_SubmitScatterGatherDescriptor(NxPlayPumpHandle, 
            pNxInCnxt->NxDesc,
            1,   
            &NumConsumed);

    if (NEXUS_SUCCESS != errCode) 
    {
        LOG_ERROR("[%s]%s: FAILED TO FIRE DESCRIPTOR ",ClientIDString,__FUNCTION__);
        return false;
    }

    if (1 != NumConsumed) 
    {
        LOG_ERROR("[%s]%s: Number Of Descriptor Consumed[%d] != 1 ",
                ClientIDString,
                __FUNCTION__,
                NumConsumed);
        return false;
    }

    FiredCnt++;

    //INSERT THE CONTEXT IN THE ACTIVEQ
    InsertHeadList(&ActiveQ,&pNxInCnxt->ListEntry);
    return true;
}

#ifndef GENERATE_DUMMY_EOS
bool
PESFeeder::PrepareEOSData()
{
    FormBppPacket(pPESEosBuffer, 0xa); /* Inline flush / TPD */
    FormBppPacket(pPESEosBuffer+184, 0x82); /* LAST */
    FormBppPacket(pPESEosBuffer+368, 0xa); /* Inline flush / TPD */
    NEXUS_FlushCache(pPESEosBuffer, PES_EOS_BUFFER_SIZE);
    return true;
}
#else
bool
PESFeeder::PrepareEOSData()
{
    unsigned char PESHeader[PES_HEADER_SIZE];
    size_t seqEndCodeSize;
    unsigned char *pSeqEndCode;
    unsigned char b_eos_h264[4] = 
    {
        0x00, 0x00, 0x01, 0x0A
    };

    unsigned char b_eos_h263[4] = 
    {
        0x00, 0x00, 0x01, 0x1F
    };

    unsigned char b_eos_Mpeg4Part2[4] = 
    {
        0x00, 0x00, 0x01, 0xB1
    };

    unsigned char b_eos_h265[5] = 
    {
        0x00, 0x00, 0x01, 0x4A, 0x01
    };

    


    switch(vidCodec)
    {
        case NEXUS_VideoCodec_eH263:
        pSeqEndCode = b_eos_h263;
        seqEndCodeSize = sizeof(b_eos_h263);
        break;
        case NEXUS_VideoCodec_eMpeg4Part2:
            pSeqEndCode = b_eos_Mpeg4Part2;
            seqEndCodeSize = sizeof(b_eos_Mpeg4Part2);
            break;
        case NEXUS_VideoCodec_eH265:
            pSeqEndCode = b_eos_h265;
            seqEndCodeSize = sizeof(b_eos_h265);
            break;            
        case NEXUS_VideoCodec_eVp8:
        case NEXUS_VideoCodec_eH264:
        default:
            pSeqEndCode = b_eos_h264;
            seqEndCodeSize = sizeof(b_eos_h264);
    }

    InitiatePESHeader(0, seqEndCodeSize,PESHeader,PES_HEADER_SIZE);

    batom_accum_add_range(AccumulatorObject, pSeqEndCode, seqEndCodeSize);
    batom_cursor_from_accum(&CursorToAccum, AccumulatorObject);

    unsigned int CopiedSz = CopyPESData(pPESEosBuffer,PES_EOS_BUFFER_SIZE);

    memset (PESHeader,0,PES_HEADER_SIZE);
    InitiatePESHeader(0, 16 * 256,PESHeader,PES_HEADER_SIZE);

    for(unsigned i = 0; i < 16; i++) 
    {
        batom_accum_add_range(AccumulatorObject, pSeqEndCode, 256);
    }

    batom_cursor_from_accum(&CursorToAccum, AccumulatorObject);
    CopyPESData(pPESEosBuffer+CopiedSz,PES_EOS_BUFFER_SIZE-CopiedSz);
    return true;
}
#endif

//
// This Will Work Only if memory allocated for EOS is
// from heap[1]. Use AllocatePESBuffer function for allocating this memory.
//
bool
PESFeeder::NotifyEOS(unsigned int ClientFlags, unsigned long long EOSFrameKey)
{
    LOG_EOS_DBG("[%s]%s: Notifying EOS To Hardware",ClientIDString,__FUNCTION__);

    ////Setup the internal Transfer Struct
    pInterNotifyCnxt->SzPESBuffer =  PES_EOS_BUFFER_SIZE;
    pInterNotifyCnxt->SzValidPESData = PES_EOS_BUFFER_SIZE;
    pInterNotifyCnxt->PESData = pPESEosBuffer;
    pInterNotifyCnxt->DoneContext.Param1 = NULL;
    pInterNotifyCnxt->DoneContext.Param2 = NULL;
    pInterNotifyCnxt->DoneContext.Param3 = NULL;
    pInterNotifyCnxt->FramePTS=0;
    pInterNotifyCnxt->DoneContext.pFnDoneCallBack = EOSXferDoneCallBack;

    if (FdrEvtLsnr) 
    {
        if(!EOSFrameKey)
        {
            ALOGD("%s: EOS Buffer Has TimeStamp Of Zero.Using Last Buffer's Time Stamp:%lld",
                    __PRETTY_FUNCTION__,LastInputTimeStamp);
            EOSFrameKey = LastInputTimeStamp;
        }

        FdrEvtLsnr->InputEOSReceived(ClientFlags,EOSFrameKey);
    }

    SendPESDataToHardware(pInterNotifyCnxt);
    return true;
}

void 
PESFeeder::DiscardConfigData()
{
    LOG_FLUSH_MSGS("[%s]%s: Discarding Coddec Config Sz:%d",
            ClientIDString,__FUNCTION__,
            pCfgDataMgr->GetConfigDataSz() );

    pCfgDataMgr->DiscardConfigData();
    SendCfgDataOnNextInput=false;
}

bool
PESFeeder::SaveCodecConfigData(void *pData,size_t SzData)
{
    unsigned char *pDstBuff;
    size_t  SzSavedConfigData=0;

    if (!pData || !SzData)
    {
          LOG_ERROR("[%s] %s: %p %d Invalid Parameters",
                  ClientIDString,__FUNCTION__,pData,SzData);
      return false;
    }

    SzSavedConfigData = pCfgDataMgr->GetConfigDataSz();
    if ((SzData + SzSavedConfigData) > CODEC_CONFIG_BUFFER_SIZE)
    {
        LOG_ERROR(" [%s]%s: No Buffer To Save More Config Data. ReqSz:[(%d)+(%d)=(%d)] MaxSz:%d",
              ClientIDString,__FUNCTION__,SzData,SzSavedConfigData,
              (SzData + SzSavedConfigData),
              CODEC_CONFIG_BUFFER_SIZE);

        return false;
    }

    if(false == pCfgDataMgr->SaveConfigData(pData,SzData))
    {
      LOG_ERROR("[%s] Failed To Save Config Data ==",__PRETTY_FUNCTION__);
      return false;
    }

    SzSavedConfigData = pCfgDataMgr->GetConfigDataSz();
    SendCfgDataOnNextInput=true;
    LOG_CONFIG_MSGS("[%s]%s: Config Data Saved MAXSZ:%d RequestedSz:%d CurrSz:%d",
          ClientIDString,__FUNCTION__,
          CODEC_CONFIG_BUFFER_SIZE,
          SzData,
          SzSavedConfigData);

    return true;
}

PESFeeder::Pauser::Pauser(NEXUS_PlaypumpHandle NxPlayPumpHandle)
{
    NEXUS_Error NxErr=NEXUS_SUCCESS;
    NEXUS_PlaypumpStatus NxPlayPumpStatus;

    NEXUS_Playpump_GetStatus(NxPlayPumpHandle, &NxPlayPumpStatus);

    if(false == NxPlayPumpStatus.started)
    {
        LOG_ERROR("%s: WTF?? Playpump Not Started !!",__FUNCTION__);
        NxPPHandle = NULL;
        return;
    }

    NxErr = NEXUS_Playpump_SetPause(NxPlayPumpHandle,true);
    if (NEXUS_SUCCESS != NxErr) 
    {
        LOG_ERROR("%s: Pausing the Playpump Failed",__FUNCTION__);
        BCMOMX_DBG_ASSERT(NEXUS_SUCCESS == NxErr); 

        return;
    }

    LOG_ERROR("%s: Playpump Paused",__FUNCTION__);
    NxPPHandle = NxPlayPumpHandle;
    return;
}

PESFeeder::Pauser::~Pauser()
{
    NEXUS_Error NxErr=NEXUS_SUCCESS;

    if (NULL == NxPPHandle) 
    {
        LOG_ERROR("%s: No Need To UnPause",__FUNCTION__);
        return;
    }

    NxErr = NEXUS_Playpump_SetPause(NxPPHandle, false); 
    if (NEXUS_SUCCESS != NxErr) 
    {
        LOG_ERROR("%s: UnPausing the Playpump Failed",__FUNCTION__);
        BCMOMX_DBG_ASSERT(NEXUS_SUCCESS == NxErr); 
        return;
    }

    LOG_ERROR("%s: Playpump UnPaused",__FUNCTION__);
    return;
}


/****Configuration Data Management Functions****/

//Default Configuration Data Manager Class
ConfigDataMgr::ConfigDataMgr() 
    :SzCodecConfigData(0),pCodecConfigData(NULL)
{
    LOG_ERROR("%s: Default Config Data Mgr Created",__PRETTY_FUNCTION__); 

    NEXUS_Memory_Allocate(CODEC_CONFIG_BUFFER_SIZE,
                          NULL,
                          (void **) &pCodecConfigData);    

}

ConfigDataMgr::~ConfigDataMgr()
{
    LOG_ERROR("%s: Default Config Data Mgr Destroyed",__PRETTY_FUNCTION__);     
    if (pCodecConfigData)
    {
        NEXUS_Memory_Free(pCodecConfigData); 
        pCodecConfigData=NULL;
    }
}

bool
ConfigDataMgr::SaveConfigData(void *pData, size_t SzData)
{
    unsigned char *pDstBuff;
    if (!pData || !SzData) 
    {
        LOG_ERROR("%s: %p %d Invalid Parameters",
                __PRETTY_FUNCTION__,pData,SzData);

        return false;
    }

    if ((SzData + SzCodecConfigData) > CODEC_CONFIG_BUFFER_SIZE) 
    {
        LOG_ERROR(" %s: Can't Save The Codec Config Data. ReqSz:[(%d)+(%d)=(%d)] MaxSz:%d",
                __PRETTY_FUNCTION__,SzData,SzCodecConfigData,
                (SzData + SzCodecConfigData), 
                CODEC_CONFIG_BUFFER_SIZE);

        return false;
    }

    // Where we want to copy....StartAdd+ValidSz
    // We are Appending the data
    pDstBuff = pCodecConfigData + SzCodecConfigData;
    memcpy(pDstBuff,pData,SzData);
    SzCodecConfigData += SzData;

    LOG_CONFIG_MSGS("[%s]: PES Config Data Saved ReqSz[%d] TotalCfgDataSz[%d]"
                    "MAXOUTBUFFSZ[%d]",
                    __PRETTY_FUNCTION__,SzData,SzCodecConfigData,
                    CODEC_CONFIG_BUFFER_SIZE);
    return true;
}

bool
ConfigDataMgr::AccumulateConfigData(batom_accum_t Accumulator)
{
    if (!Accumulator) 
    {
        LOG_ERROR("[%s]: Accumulator Object Null", __PRETTY_FUNCTION__);
        return false;
    }

    if (!pCodecConfigData || !SzCodecConfigData)
    {
        LOG_ERROR("[%s]: No Config Data Saved", __PRETTY_FUNCTION__);
        return false;
    }

    batom_accum_add_range(Accumulator, pCodecConfigData, SzCodecConfigData);
    return true;
}

size_t
ConfigDataMgr::GetConfigDataSz()
{
    return SzCodecConfigData;
}

void 
ConfigDataMgr::DiscardConfigData()
{
    SzCodecConfigData=0;
}


/*********************************************************************************/
//Configuration Data Manager With Send Class
/*********************************************************************************/

ConfigDataMgrWithSend::ConfigDataMgrWithSend(DataSender& SenderObj,
                                             unsigned int ptsToUse)
    : Sender(SenderObj), ptsToUseForPES(ptsToUse),
      SzCodecConfigData(0),pCodecConfigData(NULL),
      pPESCodecConfigData(NULL),SzPESCodecConfigData(0),
      allocedSzPESBuffer(PES_BUFFER_SIZE(CODEC_CONFIG_BUFFER_SIZE))
{
    LOG_INFO("%s: Default Config Data Mgr Created",__PRETTY_FUNCTION__); 

    NEXUS_Memory_Allocate(CODEC_CONFIG_BUFFER_SIZE,
                            NULL,
                          (void **) &pCodecConfigData);    

    pPESCodecConfigData = 
        (unsigned char *) Sender.AllocatePESBuffer(allocedSzPESBuffer);

    BCMOMX_DBG_ASSERT(pPESCodecConfigData || pCodecConfigData);
    BKNI_CreateEvent(&XferDoneEvt); 
}


ConfigDataMgrWithSend::~ConfigDataMgrWithSend()
{
    LOG_INFO("%s: Default Config Data Mgr Created",__PRETTY_FUNCTION__); 
    if (pCodecConfigData)
    {
        NEXUS_Memory_Free(pCodecConfigData);
        pCodecConfigData=NULL;
    }

    if (pPESCodecConfigData)
    {
        Sender.FreePESBuffer(pPESCodecConfigData);
        pPESCodecConfigData=NULL;
    }

    if(XferDoneEvt)
    {
        BKNI_DestroyEvent(XferDoneEvt);
        XferDoneEvt=NULL;
    }
}

bool
ConfigDataMgrWithSend::SaveConfigData(void *pData,size_t SzData)
{
    unsigned char *pDstBuff;

    LOG_CONFIG_MSGS("%s: %p %d Saving Config Data As PES",
                    __PRETTY_FUNCTION__,pData,SzData);

    if (!pData || !SzData) 
    {
        LOG_ERROR("%s: %p %d Invalid Parameters",__FUNCTION__,pData,SzData);
        return false;
    }

    if ((SzData + SzCodecConfigData) > CODEC_CONFIG_BUFFER_SIZE) 
    {
        LOG_ERROR(" %s: Can't Save The Codec Config Data. ReqSz:[(%d)+(%d)=(%d)] MaxSz:%d",
                __FUNCTION__,SzData,SzCodecConfigData,
                (SzData + SzCodecConfigData), 
                CODEC_CONFIG_BUFFER_SIZE);

        return false;
    }

    // Where we want to copy....StartAdd+ValidSz
    // We are Appending the data
    pDstBuff = pCodecConfigData + SzCodecConfigData;
    memcpy(pDstBuff,pData,SzData);
    SzCodecConfigData += SzData;

    SzPESCodecConfigData = Sender.ProcessESData(ptsToUseForPES,
                                                 pCodecConfigData,
                                                 SzCodecConfigData,
                                                 pPESCodecConfigData,
                                                 allocedSzPESBuffer);
    
    LOG_CONFIG_MSGS("[%s]: PES Config Data Saved ReqSz[%d] TotalCfgDataSz[%d]"
                    " PESConvertedSz[%d] MAXOUTBUFFSZ[%d]",
                        __PRETTY_FUNCTION__,SzData,SzCodecConfigData,
                    SzPESCodecConfigData,allocedSzPESBuffer);

    return true;
}

void
ConfigDataMgrWithSend::SendConfigDataToHW()
{
    //Should never come here with Zero Size
    BCMOMX_DBG_ASSERT(SzPESCodecConfigData); 

    BKNI_ResetEvent(XferDoneEvt); 
    XferContext.SzPESBuffer =  allocedSzPESBuffer;
    XferContext.SzValidPESData = SzPESCodecConfigData;
    XferContext.PESData = pPESCodecConfigData;
    XferContext.DoneContext.Param1 = (unsigned int) this;
    XferContext.DoneContext.Param2 = NULL;
    XferContext.DoneContext.Param3 = NULL;
    XferContext.DoneContext.pFnDoneCallBack = ConfigDataSentAsPES;
    LOG_CONFIG_MSGS("[%s]: -Calling Send Data To Hardware",__PRETTY_FUNCTION__);
    Sender.SendPESDataToHardware(&XferContext);
   
    // Specifically using non-interuptable wait becuase we need to make sure that 
    // the transfer happens or else the seek would not work.
    BERR_Code WaitErr = BKNI_WaitForEvent(XferDoneEvt,500);
    if (BERR_SUCCESS != WaitErr  )
    {
        LOG_ERROR("%s: Wait Failed For Config Transfer Err:%d\n",
                  __PRETTY_FUNCTION__,WaitErr );
        BCMOMX_DBG_ASSERT(BERR_SUCCESS == WaitErr);
        return;
    }

    LOG_CONFIG_MSGS("[%s]: CFG Data Xfered To HW",__PRETTY_FUNCTION__);
    return;
}

void
ConfigDataMgrWithSend::XferDoneNotification()
{
    LOG_CONFIG_MSGS("[%s]: -Setting XferDoneEvt Now",
                    __PRETTY_FUNCTION__);

    BKNI_SetEvent(XferDoneEvt);
}

size_t
ConfigDataMgrWithSend::GetConfigDataSz()
{
    return SzCodecConfigData;
}

void 
ConfigDataMgrWithSend::DiscardConfigData()
{
    SzCodecConfigData=0;
    SzPESCodecConfigData=0;
}

static 
void 
ConfigDataSentAsPES(unsigned int Param1, 
                     unsigned int Param2, 
                     unsigned int Param3)
{
    LOG_CONFIG_MSGS("[%s]: Config Data Callback Called",__PRETTY_FUNCTION__);
    ConfigDataMgrWithSend *thisPtr = (ConfigDataMgrWithSend *) Param1;
    thisPtr->XferDoneNotification();
    return;
}



/********************************/
// Factory class for ConfigData
 
CfgMgr* 
ConfigDataMgrFactory::CreateConfigDataMgr(NEXUS_VideoCodec vidCodec,
                                          DataSender& Sender)
{
    CfgMgr* retObj=NULL;

    switch(vidCodec)
    {
        // NEXUS_VideoCodec_eNone is the default value for which we want to 
        // have default behaviour which is sending the config data with input IO
        // We need to handle this becuase for Audio we need this default behaviour
        case NEXUS_VideoCodec_eNone:  
        case NEXUS_VideoCodec_eH264:
        case NEXUS_VideoCodec_eMpeg2:
        case NEXUS_VideoCodec_eH263:
        case NEXUS_VideoCodec_eMpeg4Part2:
        case NEXUS_VideoCodec_eRv40:
        case NEXUS_VideoCodec_eMotionJpeg:
        case NEXUS_VideoCodec_eVp8:       
        case NEXUS_VideoCodec_eSpark:     
        case NEXUS_VideoCodec_eH265:      
        case NEXUS_VideoCodec_eVc1:
        {
            LOG_CONFIG_MSGS("[%s]:- Creating CfgMgr Without Send",__PRETTY_FUNCTION__);
            retObj = new  ConfigDataMgr();
            break;
        }
        case NEXUS_VideoCodec_eDivx311:
        case NEXUS_VideoCodec_eVc1SimpleMain:
        {
            /** For our hardware we will need to use specific TS value **/
            LOG_CONFIG_MSGS("[%s]:- Creating CfgMgr With Send",__PRETTY_FUNCTION__);
            retObj = new ConfigDataMgrWithSend(Sender, 0xffffffff);
            break;
        }
        default: 
        {
            LOG_ERROR("[%s]:Invalid Codec Information",__FUNCTION__);
            BCMOMX_DBG_ASSERT(false);
        }
    }

    BCMOMX_DBG_ASSERT(retObj);
    return retObj;
}

static
CfgMgr*
CreateCfgDataMgr(NEXUS_VideoCodec vidCodec, DataSender& Sender)
{
    return ConfigDataMgrFactory::CreateConfigDataMgr(vidCodec,Sender);
}

