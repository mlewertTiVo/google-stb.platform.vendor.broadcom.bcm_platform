/******************************************************************************
* (c) 2014 Broadcom Corporation
*
* This program is the proprietary software of Broadcom Corporation and/or its
* licensors, and may only be used, duplicated, modified or distributed pursuant
* to the terms and conditions of a separate, written license agreement executed
* between you and Broadcom (an "Authorized License").  Except as set forth in
* an Authorized License, Broadcom grants no license (express or implied), right
* to use, or waiver of any kind with respect to the Software, and Broadcom
* expressly reserves all rights in and to the Software and all intellectual
* property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
* HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
* NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
*
* Except as expressly set forth in the Authorized License,
*
* 1. This program, including its structure, sequence and organization,
*    constitutes the valuable trade secrets of Broadcom, and you shall use all
*    reasonable efforts to protect the confidentiality thereof, and to use
*    this information only in connection with your use of Broadcom integrated
*    circuit products.
*
* 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
*    AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
*    WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT
*    TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED
*    WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A
*    PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET
*    ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME
*    THE ENTIRE RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
*
* 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
*    LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT,
*    OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO
*    YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN
*    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS
*    OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER
*    IS GREATER. THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF
*    ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
******************************************************************************/

#include "PESFeeder.h"


#define LOG_START_STOP_DBG      ALOGD
#define LOG_EOS_DBG             ALOGD
#define LOG_FLUSH_MSGS          ALOGD
#define LOG_CREATE_DESTROY      ALOGD
#define LOG_CONFIG_MSGS         ALOGD

//Fast Path Messages
#define LOG_CMD_COMPLETE
#define LOG_CMD_SEND            ALOGD
#define LOG_INFO
#define LOG_ERROR               ALOGD
#define LOG_WARNING             ALOGD

#define UNUSED __attribute__((unused))

#ifdef SECURE_BUFFERS
#include "nexus_security.h"
#endif

static void DescDoneCallBack(void *context, int param UNUSED)
{
    PESFeeder *pPESFeeder = (PESFeeder *) context;
    pPESFeeder->XFerDoneCallBack();
}

static void EOSXferDoneCallBack(unsigned int Param1 UNUSED,
        unsigned int Param2 UNUSED,
        unsigned int Param3 UNUSED)
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
        NEXUS_VideoCodec vidCdc,
        bool bSecure) :
        FdrEvtLsnr(NULL), NexusPID(InputPID),
        SendCfgDataOnNextInput(false),
        pCfgDataMgr(NULL), vidCodec(vidCdc),pInterNotifyCnxt(NULL),
        CntDesc(NumDescriptors),
        FiredCnt(0), FlushCnt(0),
#ifdef DEBUG_PES_DATA
        DataBeforFlush(new DumpData("DataBeforFlush")),
        DataAfterFlush(new DumpData("DataAfterFlush")),
#endif
        LastHighestInputTS(0),
        bSecureDecoding(bSecure)
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
#ifdef SECURE_BUFFERS
    if (bSecureDecoding)
        PlayPumpOpenSettings.dataNotCpuAccessible = true;
#endif
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
    PlayPumpSettings.dataCallback.param = 0; /* unused */
    NEXUS_Playpump_SetSettings(NxPlayPumpHandle, &PlayPumpSettings);

    NxVidPidChHandle = NEXUS_Playpump_OpenPidChannel(NxPlayPumpHandle,
            NexusPID,
            NULL);
#ifdef SECURE_BUFFERS
    if (bSecureDecoding)
        NEXUS_SetPidChannelBypassKeyslot(NxVidPidChHandle, NEXUS_BypassKeySlot_eGR2R);

#ifdef PES_IN_SECURE_BUFFER
    if (bSecureDecoding)
    {
        CommonCryptoSettings  cmnCryptoSettings;
        CommonCrypto_GetDefaultSettings(&cmnCryptoSettings);

        m_CommonCryptoHandle = CommonCrypto_Open(&cmnCryptoSettings);
        if (m_CommonCryptoHandle == NULL) {
            ALOGE("%s: failed to open CommonCrypto", __FUNCTION__);
        }
    }
#endif

#endif

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

    pPESEosBuffer = (unsigned char *) AllocatePESBuffer(PES_EOS_BUFFER_SIZE);
    NEXUS_Memory_Allocate(sizeof(NEXUS_INPUT_CONTEXT),NULL,(void **) &pInterNotifyCnxt);

    pInterNotifyCnxt->pSecureData=NULL;
    pInterNotifyCnxt->SzSecureData=0;

    // Prepare the EOS Data Once And Use It For EOS
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

        PNEXUS_INPUT_CONTEXT pDoneCnxt =
            CONTAINING_RECORD(pDoneEntry,NEXUS_INPUT_CONTEXT,ListEntry);

        FiredCnt-=pDoneCnxt->NumDescFired;
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

#ifdef PES_IN_SECURE_BUFFER
    if(bSecureDecoding && m_CommonCryptoHandle) {
        CommonCrypto_Close(m_CommonCryptoHandle);
        m_CommonCryptoHandle = NULL;
    }
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

    if(pts45KHz == (unsigned int)-1)  /* VC1 simple and main profile will use it for config data special handling */
    {
        pes_info.pts_valid = false;
        pes_info.pts = 0;
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

// Prepare the Data And Copy it to the output
// Buffer.
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

    if((!pOutData) || (!SzDataBuff) || (!SzOutData))
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

    if (pDataBuffer)
    {
        //Add the Range For actual Data
        batom_accum_add_range(AccumulatorObject, pDataBuffer, SzDataBuff);
    }

    //Get The Cursor
    batom_cursor_from_accum(&CursorToAccum, AccumulatorObject);

    //Get the size of the cursor
    SzTotalPESData = batom_cursor_size(&CursorToAccum);

    if (pDataBuffer)
    {
        if ((SzTotalPESData > SzOutData)  ||
                    (SzTotalPESData < (SzPESHeader + SzDataBuff)))
        {
                LOG_ERROR("[%s]%s: Failed to Initiate the PES header",ClientIDString,__FUNCTION__);
                return 0;
        }
        else if ( (SzTotalPESData > SzOutData)  ||
             (SzTotalPESData < SzPESHeader) )
        {
                LOG_ERROR("[%s]%s: Failed to Initiate the PES header",ClientIDString,__FUNCTION__);
                return 0;
        }
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

    if ( (!pHeaderToInsert) || (!SzHdrBuff) ||
         (!SzDataBuff) || (!pOutData) ||
         (!SzOutData) )
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

    if (pDataBuffer)
    {
        //Add the Range For actual Data
        batom_accum_add_range(AccumulatorObject, pDataBuffer, SzDataBuff);
    }

    //Get The Cursor
    batom_cursor_from_accum(&CursorToAccum, AccumulatorObject);

    //Get the size of the cursor
    SzTotalPESData = batom_cursor_size(&CursorToAccum);

    if (pDataBuffer)
    {
        if ((SzTotalPESData > SzOutData)  ||
                (SzTotalPESData < (SzPESHeader + SzDataBuff + SzHdrBuff)) )
        {
            LOG_ERROR("[%s]%s: Failed to Initiate the PES header",ClientIDString,__FUNCTION__);
            return 0;
        }
        else if ( (SzTotalPESData > SzOutData)  ||
                (SzTotalPESData < (SzPESHeader + SzHdrBuff)) )
        {
            LOG_ERROR("[%s]%s: Failed to Initiate the PES header",ClientIDString,__FUNCTION__);
            return 0;
        }
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
    size_t copiedSz = 0;

    if( !pBuffer || !bufferSz)
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

    LOG_FLUSH_MSGS("[%s]%s: Fired %d Descriptors, Flushing Them. Reset LastInputTimeStamp From:%d to Zero",
            ClientIDString,__FUNCTION__,FiredCnt,LastHighestInputTS);

    LastHighestInputTS = 0;
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

        FiredCnt -= pDoneCnxt->NumDescFired;
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
    size_t SzCfgData=0;
    SzCfgData = pCfgDataMgr->GetConfigDataSz();
    LOG_FLUSH_MSGS("[%s]%s: Decoder Flush Started CfgDataSz:%d",
            ClientIDString,__FUNCTION__,SzCfgData);

    StopPlayPump();
    return;
}

void 
PESFeeder::FlushDone()
{
    size_t SzCfgData=0;
    LOG_FLUSH_MSGS("[%s]%s: Decoder Flush Done Notified--Config Data Will Be Sent On Next IO",
            ClientIDString,__FUNCTION__);

    StartPlayPump();
    SzCfgData = pCfgDataMgr->GetConfigDataSz();

    LOG_CONFIG_MSGS("%s: CfgDataSz On flush Done :%d ",
                    __PRETTY_FUNCTION__,SzCfgData);
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

    LOG_CMD_COMPLETE("[%s]%s: DescDoneCnt:%d = [FiredCnt[%d]-descFifoDepth[%d]]",
            ClientIDString,__FUNCTION__,
            DescDoneCnt,
            FiredCnt,
            NxPlayPumpStatus.descFifoDepth);

    if (!DescDoneCnt)
    {
        LOG_WARNING("%s: Spurious Callback- No Descriptors Completed",__FUNCTION__);
        return;
    }



    while (DescDoneCnt)
    {
        BCMOMX_DBG_ASSERT(!IsListEmpty(&ActiveQ));
        PLIST_ENTRY pDoneEntry = RemoveTailList(&ActiveQ);
        BCMOMX_DBG_ASSERT(pDoneEntry);
        PNEXUS_INPUT_CONTEXT pDoneCnxt = CONTAINING_RECORD(pDoneEntry,NEXUS_INPUT_CONTEXT,ListEntry);
        BCMOMX_DBG_ASSERT(pDoneCnxt->NumDescFired);

        LOG_CMD_COMPLETE("%s: Processing %p: NumFired:%d DescDoneCnt:%d",
                         __FUNCTION__, pDoneCnxt, pDoneCnxt->NumDescFired,DescDoneCnt);

        while (DescDoneCnt && pDoneCnxt->NumDescFired)
        {
            pDoneCnxt->NumDescFired--;
            FiredCnt--;
            DescDoneCnt--;
            LOG_CMD_COMPLETE("%s: After Decrement %p: NumFired:%d DescDoneCnt:%d FiredCnt:%d",
                             __FUNCTION__, pDoneCnxt,
                             pDoneCnxt->NumDescFired,
                             DescDoneCnt,FiredCnt);
        }

        if ( (pDoneCnxt->NumDescFired) && (DescDoneCnt) )
        {
            LOG_ERROR("%s: ===ERROR ASSERTING== : If There Are More Desc Competed, You should be "
                      "Able to Complete This (or More) Commands"
                      "pDoneCnxt->NumDescFired: %d DescDoneCnt:%d====",
                      __FUNCTION__, pDoneCnxt->NumDescFired, DescDoneCnt);

            BCMOMX_DBG_ASSERT(false);
        }


        if (0 == pDoneCnxt->NumDescFired)
        {
            LOG_CMD_COMPLETE("%s: Completing %p: NumFired:%d Remaing DescDoneDnt:%d",
                             __FUNCTION__, pDoneCnxt, pDoneCnxt->NumDescFired,DescDoneCnt);

            //Command Completed
            if (pDoneCnxt->DoneContext.pFnDoneCallBack)
            {
                LOG_CMD_COMPLETE("%s: Firing Callback DescDoneCnt:%d",__FUNCTION__,DescDoneCnt);

                //Fire The Call Back
                pDoneCnxt->DoneContext.pFnDoneCallBack(pDoneCnxt->DoneContext.Param1,
                        pDoneCnxt->DoneContext.Param2,
                        pDoneCnxt->DoneContext.Param3);

                LOG_CMD_COMPLETE("%s: Callback Done DescDoneCnt:%d",__FUNCTION__,DescDoneCnt);
            }else{
                LOG_ERROR("%s: No Call Back DescDontCnt:%d",__FUNCTION__,DescDoneCnt);
                BCMOMX_DBG_ASSERT(false);
            }

        }else{
            //Command Did not Complete...put it back in the Active List From Tail
            LOG_CMD_COMPLETE("%s: Partial Cmd Completion %p: NumFired:%d Remaing DescDoneDnt:%d",
                             __FUNCTION__, pDoneCnxt, pDoneCnxt->NumDescFired, DescDoneCnt);

            // You should never have a partial complete with DescDoneCount Set.
            // If there were more descriptors completed, then the command should have
            // completed fully, not partially.
            BCMOMX_DBG_ASSERT(0==DescDoneCnt);
            InsertTailList(&ActiveQ,pDoneEntry);
        }
    }

    //You should not Come Out of this function with something in DescDoneCnt!=0
    // We have to make sure That All the Descriptors are processed.
    LOG_CMD_COMPLETE("%s: Completion Done DescDoneCnt:%d \n\n", __FUNCTION__, DescDoneCnt);
    BCMOMX_DBG_ASSERT(0==DescDoneCnt);
    return; 
}

bool
PESFeeder::SendPESDataToHardware(PNEXUS_INPUT_CONTEXT pNxInCnxt)
{
    size_t NumFired=0,NumConsumed=0;
    NEXUS_Error errCode;

    if (!pNxInCnxt || !pNxInCnxt->SzValidPESData)
    {
        LOG_ERROR("[%s]%s: Invalid Parameters = NO VALID DATA TO SEND %p Sz:%d",
                ClientIDString,__FUNCTION__,
                pNxInCnxt,
                pNxInCnxt ? pNxInCnxt->SzValidPESData:0);

        return false;
    }

    if (((pNxInCnxt->pSecureData) && (!pNxInCnxt->SzSecureData)) ||
        ((NULL == pNxInCnxt->pSecureData) && (pNxInCnxt->SzSecureData)))
    {
        LOG_ERROR("[%s]%s: Invalid Parameters pSecureData=%p Sz:%d",
                ClientIDString,__FUNCTION__,
                pNxInCnxt->pSecureData,
                pNxInCnxt->SzSecureData);
        return false;
    }
    Mutex::Autolock lock(mListLock);
    pNxInCnxt->NumDescFired=0;
    InitializeListHead(&pNxInCnxt->ListEntry);
    if (FiredCnt > CntDesc)
    {
        LOG_ERROR("[%s]%s: == ERROR We Have Already Fired All The Descriptors [HOW??]== ",
                ClientIDString,__FUNCTION__);
        return false;
    }

    pNxInCnxt->NxDesc[0].addr       = pNxInCnxt->PESData;
    pNxInCnxt->NxDesc[0].length     = pNxInCnxt->SzValidPESData;
    pNxInCnxt->NumDescFired++;

    // copy the PES header to a segment of the secure buffer
#ifdef PES_IN_SECURE_BUFFER
    if (bSecureDecoding)
    {
        int nbBlks = 0;
        uint8_t *dest = pNxInCnxt->pSecureData;
        dest = (uint8_t*)(((uint32_t)dest + pNxInCnxt->SzSecureData + 4096) & ~(4096 - 1));
        NEXUS_DmaJobBlockSettings blkSettings[1];
        NEXUS_DmaJob_GetDefaultBlockSettings(&blkSettings[nbBlks]);
        blkSettings[nbBlks].pSrcAddr = pNxInCnxt->PESData;
        blkSettings[nbBlks].pDestAddr = dest;
        blkSettings[nbBlks].blockSize = pNxInCnxt->SzValidPESData;
        blkSettings[nbBlks].resetCrypto = true;
        blkSettings[nbBlks].scatterGatherCryptoStart = true;
        blkSettings[nbBlks].scatterGatherCryptoEnd = true;
        blkSettings[nbBlks].cached = true;
        nbBlks++;

        // Do the DMA Xfer
        CommonCryptoJobSettings cryptoJobSettings;
        CommonCrypto_GetDefaultJobSettings(&cryptoJobSettings);
        NEXUS_Error err = CommonCrypto_DmaXfer(m_CommonCryptoHandle,
                                               &cryptoJobSettings, blkSettings, nbBlks);
        ALOGD("CommonCrypto_DmaXfer returned:%d", err);
        pNxInCnxt->NxDesc[0].addr  = dest;
    }
#endif

    if (pNxInCnxt->pSecureData)
    {
        pNxInCnxt->NxDesc[1].addr   = pNxInCnxt->pSecureData;
        pNxInCnxt->NxDesc[1].length = pNxInCnxt->SzSecureData;
        pNxInCnxt->NumDescFired++;
    }

    NumFired = pNxInCnxt->NumDescFired;

    // The Last Time Stamp That We sent To Hardware...
    if(pNxInCnxt->FramePTS && !pNxInCnxt->IsConfig)
    {
        LOG_INFO("%s: Updating LastTimeStamp: %lld", __FUNCTION__, LastHighestInputTS);
        if (pNxInCnxt->FramePTS > LastHighestInputTS)
        {
            LastHighestInputTS = pNxInCnxt->FramePTS; 
        }
    }

#ifdef DEBUG_PES_DATA
    if (!FlushCnt) {
        DataBeforFlush->WriteData(pNxInCnxt->PESData,pNxInCnxt->SzValidPESData);
    }else if(1==FlushCnt){
        DataAfterFlush->WriteData(pNxInCnxt->PESData,pNxInCnxt->SzValidPESData);
    }
#endif

    LOG_CMD_SEND("%s: Firing Descriptors pNxInCnxt:%p Addr[0]:%p len[0]:%d addr[1]:%p length[1]:%d NumFired:%d",
                 __FUNCTION__,pNxInCnxt,
                 pNxInCnxt->NxDesc[0].addr,
                 pNxInCnxt->NxDesc[0].length,
                 pNxInCnxt->NxDesc[1].addr,
                 pNxInCnxt->NxDesc[1].length,
                 NumFired);

    errCode = NEXUS_Playpump_SubmitScatterGatherDescriptor(NxPlayPumpHandle,
                                                           pNxInCnxt->NxDesc,
                                                           NumFired,
                                                           &NumConsumed);

    if (NEXUS_SUCCESS != errCode)
    {
        LOG_ERROR("[%s]%s: FAILED TO FIRE DESCRIPTOR ",ClientIDString,__FUNCTION__);
        return false;
    }

    if (NumFired != NumConsumed)
    {
        LOG_ERROR("[%s]%s: Number Of Descriptor Consumed[%d] != NumFired [%d] ",
                ClientIDString,
                __FUNCTION__,
                NumConsumed, NumFired);
        return false;
    }

    FiredCnt+=NumFired;

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
    pInterNotifyCnxt->pSecureData=NULL;
    pInterNotifyCnxt->SzSecureData=0;
    pInterNotifyCnxt->SzPESBuffer =  PES_EOS_BUFFER_SIZE;
    pInterNotifyCnxt->SzValidPESData = PES_EOS_BUFFER_SIZE;
    pInterNotifyCnxt->PESData = pPESEosBuffer;
    pInterNotifyCnxt->DoneContext.Param1 = 0;
    pInterNotifyCnxt->DoneContext.Param2 = 0;
    pInterNotifyCnxt->DoneContext.Param3 = 0;
    pInterNotifyCnxt->FramePTS=0;
    pInterNotifyCnxt->DoneContext.pFnDoneCallBack = EOSXferDoneCallBack;

    if (FdrEvtLsnr)
    {
        if(!EOSFrameKey)
        {
            ALOGD("%s: EOS Buffer Has TimeStamp Of Zero.Using Last Buffer's Time Stamp:%lld",
                    __PRETTY_FUNCTION__,LastHighestInputTS);
            EOSFrameKey = LastHighestInputTS;
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
    SendCfgDataOnNextInput = false;
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
    SendCfgDataOnNextInput = true;
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
    LOG_ERROR("%s:  ConfigDataMgr Created",__PRETTY_FUNCTION__); 

    NEXUS_Memory_Allocate(CODEC_CONFIG_BUFFER_SIZE,
                          NULL,
                          (void **) &pCodecConfigData);

}

ConfigDataMgr::~ConfigDataMgr()
{
    LOG_ERROR("%s: Default ConfigDataMgr Destroyed",__PRETTY_FUNCTION__);     
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
    LOG_CONFIG_MSGS("%s: Buffer:%p Size:%d ",
                    __PRETTY_FUNCTION__,pData,SzData);

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

    LOG_CONFIG_MSGS("[%s]: ConfigData Saved ReqSz[%d] TotalCfgDataSz[%d] MAXOUTBUFFSZ[%d]",
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
    : SzCodecConfigData(0),pCodecConfigData(NULL),
      allocedSzPESBuffer(PES_BUFFER_SIZE(CODEC_CONFIG_BUFFER_SIZE)),
      SzPESCodecConfigData(0),pPESCodecConfigData(NULL),
      Sender(SenderObj), ptsToUseForPES(ptsToUse)
{
    LOG_INFO("%s: ConfigDataMgrWithSend Created",__PRETTY_FUNCTION__); 

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
    LOG_INFO("%s: ConfigDataMgrWithSend Destroyed",__PRETTY_FUNCTION__); 
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

    LOG_CONFIG_MSGS("%s [AsPES]: Buffer:%p Size:%d ",
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
    XferContext.pSecureData = NULL;
    XferContext.SzSecureData=0;
    XferContext.SzPESBuffer =  allocedSzPESBuffer;
    XferContext.SzValidPESData = SzPESCodecConfigData;
    XferContext.PESData = pPESCodecConfigData;
    XferContext.DoneContext.Param1 = (unsigned int) this;
    XferContext.DoneContext.Param2 = 0;
    XferContext.DoneContext.Param3 = 0;
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
                     unsigned int Param2 UNUSED,
                     unsigned int Param3 UNUSED)
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

