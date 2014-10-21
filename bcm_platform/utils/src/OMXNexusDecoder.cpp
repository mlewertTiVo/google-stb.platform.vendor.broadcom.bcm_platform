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

#include "OMXNexusDecoder.h"
#include <utils/Log.h>
#include <cutils/properties.h>

// Property to control whether to disable 4K. It is enabled by default .
#define BCM_OMX_H265_DISABLE_4K "bcm.omx.265.disable4k"
#define BCM_OMX_H265_DISABLE_4K_DEFAULT "0"

//Path level Debug Messages
#define LOG_START_STOP_DBG      ALOGD
#define LOG_EOS_DBG             ALOGD
#define LOG_FLUSH_MSGS          ALOGD
#define LOG_CREATE_DESTROY      ALOGD

//Path independent Debug Messages
#define LOG_WARNING             ALOGD
#define LOG_ERROR               ALOGD
#define LOG_INFO

#define LOG_EVERY_DELIVERY

//Enable logging for output time stamps on every frame.8
#define LOG_OUTPUT_TS

//Enable logging of the decoder state
#define LOG_DECODER_STATE       ALOGD
#define DECODER_STATE_FREQ     50      //Print decoder state after every XX output frames.
List<OMXNexusDecoder *> OMXNexusDecoder::DecoderInstanceList;
Mutex   OMXNexusDecoder::mDecoderIniLock;

#ifndef GENERATE_DUMMY_EOS
static void SourceChangedCallbackDispatcher(void *pParam, int param)
{
    BSTD_UNUSED(param);
    LOG_EOS_DBG("%s",__FUNCTION__);
    OMXNexusDecoder *pNxDecoder = (OMXNexusDecoder *)pParam;
    pNxDecoder->SourceChangedCallback();
}
#endif

OMXNexusDecoder::OMXNexusDecoder(char const *CallerName, NEXUS_VideoCodec NxCodec,
                                 PaltformSpecificContext *pSetPlatformContext) :
    DecoderID(0),
    EOSState(EOS_Init),
    DecoEvtLisnr(NULL),
    decoHandle(NULL), ipcclient(NULL), nexus_client(NULL),
    NextExpectedFr(1),
    ClientFlags(0), EOSFrameKey(0),
    StartupTime(STARTUP_TIME_INVALID),
#ifdef GENERATE_DUMMY_EOS
    DownCnt(DOWN_CNT_DEPTH),
#endif
    EmptyFrListLen(0), DecodedFrListLen(0),
    LastErr(ErrStsSuccess),
    DebugCounter(0),
    FlushCnt(0)
{
    Mutex::Autolock lock(mDecoderIniLock);
    AddDecoderToList();
    LOG_CREATE_DESTROY("%s Creating Decoder ID:%d ===",__FUNCTION__,DecoderID);
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
        LOG_ERROR("%s Decoder[%d]: Could not create client \"%s\" context!",
                  __FUNCTION__,DecoderID, ipcclient->getClientName());
        LastErr = ErrStsClientCreateFail;
        return;
    }else {
        LOG_CREATE_DESTROY("%s Decoder[%d]: OMX client \"%s\" context created successfully: client=0x%p",
                 __FUNCTION__, DecoderID, ipcclient->getClientName(), nexus_client);
    }

    decoHandle = ipcclient->acquireVideoDecoderHandle();
    if ( NULL == decoHandle )
    {
        LOG_ERROR("%s Decoder[%d]:Unable to acquire Simple Video Decoder \n",__FUNCTION__,DecoderID);
        LastErr = ErrStsDecoAcquireFail;
        return;
    }else{
        LOG_CREATE_DESTROY("%s Decoder[%d]: Acquired Video Decoder Handle", __FUNCTION__,DecoderID);
    }
    ipcclient->getClientInfo(nexus_client, &client_info);

    // Now connect the client resources
    ipcclient->getDefaultConnectClientSettings(&connectSettings);
    LOG_CREATE_DESTROY("%s Decoder[%d]: Server Assigned Decoder ID:%p", __FUNCTION__,DecoderID,client_info.videoDecoderId);
    connectSettings.simpleVideoDecoder[0].id = client_info.videoDecoderId;
    connectSettings.simpleVideoDecoder[0].surfaceClientId = client_info.surfaceClientId;
    connectSettings.simpleVideoDecoder[0].windowId = DecoderID;

    if(NxVideoCodec == NEXUS_VideoCodec_eH265) {
        char value[PROPERTY_VALUE_MAX];
        property_get(BCM_OMX_H265_DISABLE_4K, value, BCM_OMX_H265_DISABLE_4K_DEFAULT);
        if (!strcmp(value, "1")) {
            ALOGE("OMX 4k playback is disabled");
        } else {
            connectSettings.simpleVideoDecoder[0].decoderCaps.maxWidth = 3840;
            connectSettings.simpleVideoDecoder[0].decoderCaps.maxHeight = 2160;
            ALOGD("OMX 4k playback is enabled");
        }
    }

    connectSettings.simpleVideoDecoder[0].windowCaps.type =
        (DecoderID == 0) ? eVideoWindowType_eMain : eVideoWindowType_ePip;
    if (true != ipcclient->connectClientResources(nexus_client, &connectSettings))
    {
        LOG_ERROR("%s Decoder[%d]: Could not connect client \"%s\" resources!",
            __FUNCTION__,DecoderID, ipcclient->getClientName());

        LastErr = ErrStsConnectResourcesFail;
        return;
    }else{
        LOG_CREATE_DESTROY("%s Decoder[%d]: Connect Client \"%s\" Resources Success!",
            __FUNCTION__, DecoderID, ipcclient->getClientName());
    }

    LOG_CREATE_DESTROY("%s Decoder[%d]: Making Video Window Invisible On Startup",
            __FUNCTION__, DecoderID);

    b_video_window_settings window_settings;
    ipcclient->getVideoWindowSettings(nexus_client, DecoderID, &window_settings);
    window_settings.visible = false;
    ipcclient->setVideoWindowSettings(nexus_client, DecoderID, &window_settings);


#ifndef GENERATE_DUMMY_EOS
    NEXUS_VideoDecoderSettings vidDecSettings;
    NEXUS_Error errCode;

    NEXUS_SimpleVideoDecoder_GetSettings(decoHandle, &vidDecSettings);
    vidDecSettings.sourceChanged.callback = SourceChangedCallbackDispatcher;
    vidDecSettings.sourceChanged.context = static_cast <void *>(this);
    errCode = NEXUS_SimpleVideoDecoder_SetSettings(decoHandle, &vidDecSettings);
    if(errCode != NEXUS_SUCCESS)
    {
        LOG_WARNING("%s Decoder[%d]:Failed To Install The Source Changes CallBack",
                    __FUNCTION__, DecoderID);
    }

#endif

    for (unsigned int i=0; i < DECODE_DEPTH; i++)
    {
        PNEXUS_DECODED_FRAME pDecoFr = (PNEXUS_DECODED_FRAME)
                        malloc(sizeof(NEXUS_DECODED_FRAME));
        if (!pDecoFr)
        {
            LOG_ERROR("%s Decoder[%d]: Memory Allocation Failure",__FUNCTION__, DecoderID);
            RemoveDecoderFromList();
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

    if ( LastErr ) LOG_ERROR("%s Decoder[%d]: COMEPLETION WITH ERROR Error:%d", __FUNCTION__,DecoderID, LastErr);

#ifdef GENERATE_DUMMY_EOS
    LOG_WARNING("%s Decoder[%d]: WIll Use Dummy EOS Generation Logic",__FUNCTION__,DecoderID);
#else
    LOG_WARNING("%s Decoder[%d]: Will Use Hardware EOS Generation Logic",__FUNCTION__,DecoderID);
#endif

    LOG_CREATE_DESTROY("%s Decoder[%d]: EXIT ===",__FUNCTION__,DecoderID);
}

OMXNexusDecoder::~OMXNexusDecoder()
{
    LOG_CREATE_DESTROY("%s: ENTER Destroying DecoderID:%d ===",__FUNCTION__,DecoderID);
    Mutex::Autolock lock(mDecoderIniLock);
    DecoEvtLisnr=NULL;
    RemoveDecoderFromList();
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
        LOG_ERROR("THIS IS A BUG. ALL THE FRAMES FROM THE DECODED LIST SHOULD BE:-"
                "\nRETURNED TO THE HARDWARE AND MOVED TO THE FREE LIST."
                "\nWE SHOULD NOT HAVE ANYTHING IN THE DECODED LIST HERE" );

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
        LOG_ERROR("THIS IS A BUG. ALL THE FRAMES FROM THE DELIVERED LIST SHOULD BE:-"
                "\nRETURNED TO THE HARDWARE AND MOVED TO THE FREE LIST."
                "\nWE SHOULD NOT HAVE ANYTHING IN THE DELIVERED LIST HERE" );
        PLIST_ENTRY ThisEntry =  RemoveTailList(&DeliveredFrList);
        PNEXUS_DECODED_FRAME pDecoFr = CONTAINING_RECORD(ThisEntry,NEXUS_DECODED_FRAME,ListEntry);
        free(pDecoFr);
    }

    delete ipcclient;
    LOG_CREATE_DESTROY("%s Decoder[%d]: EXIT ===",__FUNCTION__,DecoderID);
}


bool
OMXNexusDecoder::RegisterDecoderEventListener(DecoderEventsListener * RgstLstnr)
{
    if (!RgstLstnr)
    {
        LOG_ERROR("%s Decoder[%d]: --Invalid Input Parameter--",__FUNCTION__,DecoderID);
        LastErr = ErrStsInvalidParam;
        return false;
    }

    if (DecoEvtLisnr)
    {
        LOG_ERROR("%s Decoder[%d]: --Listener Already Registered--",__FUNCTION__,DecoderID);
        LastErr = ErrStsInvalidParam;
        return false;
    }

    DecoEvtLisnr = RgstLstnr;
    return true;
}


bool
OMXNexusDecoder::StartDecoder(unsigned int ClientParam)
{
    LOG_START_STOP_DBG("%s %d Decoder[%d]: With PID:%p",
                       __FUNCTION__,__LINE__,DecoderID,ClientParam);
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
        LOG_ERROR("%s Decoder[%d]: DECODER Already Started",__FUNCTION__,DecoderID);
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
        char value[PROPERTY_VALUE_MAX];
        property_get(BCM_OMX_H265_DISABLE_4K, value, BCM_OMX_H265_DISABLE_4K_DEFAULT);

        if (!strcmp(value, "1")) {
            ALOGE("OMX 4k playback is disabled");
        } else {
            videoProgram.maxWidth  = 3840;
            videoProgram.maxHeight = 2160;
            ALOGD("OMX 4k playback is enabled");
        }

    }

    LOG_START_STOP_DBG("%s %d Decoder[%d]: Starting Decoder For PID:%p",
                       __FUNCTION__,__LINE__,DecoderID,videoPIDChannel);
    NexusSts = NEXUS_SimpleVideoDecoder_Start(decoHandle, &videoProgram);
    if(NexusSts != NEXUS_SUCCESS)
    {
        LOG_ERROR("%s Decoder[%d]: SimeplVideoDecoder Failed To Start \n",__FUNCTION__,DecoderID);
        LastErr = ErrStsStartDecoFailed;
        return false;
    }

    LOG_START_STOP_DBG("%s Decoder[%d]:Simple Decoder Started Successfully With PIDChannel: 0x%x",
                       __FUNCTION__,DecoderID, VideoPid);
    LastErr = ErrStsSuccess;
    return true;
}

void
OMXNexusDecoder::StopDecoder()
{
    NEXUS_Error nxErr = NEXUS_SUCCESS;

    if(IsDecoderStarted())
    {
        LOG_START_STOP_DBG("%s %d Decoder[%d]: Issuing StopDecoder To hardware 0x%x",
                           __FUNCTION__,__LINE__,DecoderID,VideoPid);
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
        LOG_ERROR("%s Decoder[%d]: Decoder Already UnPaused",__FUNCTION__,DecoderID);
        return true; //Not Really An Error
    }

    DecoTrickState.rate = NEXUS_NORMAL_DECODE_RATE;
    NxErr = NEXUS_SimpleVideoDecoder_SetTrickState(decoHandle,&DecoTrickState);
    if (NEXUS_SUCCESS == NxErr)
    {
        LOG_INFO("%s Decoder[%d]: Decoder UnPaused ==",__FUNCTION__,DecoderID);
        return true;
    }

    LOG_ERROR("%s Decoder[%d]: Decoder UnPause Failed ==",__FUNCTION__,DecoderID);
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
        LOG_ERROR("%s Decoder[%d]: Decoder Already Paused",__FUNCTION__,DecoderID);
        return true; //Not Really An Error
    }

    DecoTrickState.rate = 0;
    NxErr = NEXUS_SimpleVideoDecoder_SetTrickState(decoHandle,&DecoTrickState);
    if (NEXUS_SUCCESS == NxErr)
    {
        LOG_INFO("%s Decoder[%d]: Decoder Paused ==",__FUNCTION__,DecoderID);
        return true;
    }

    LOG_ERROR("%s Decoder[%d]: Decoder Pause Failed ==",__FUNCTION__,DecoderID);
    return false;
}

bool
OMXNexusDecoder::FlushDecoder()
{
    if (DecoEvtLisnr) DecoEvtLisnr->FlushStarted();

    StopDecoder();
    if (!StartDecoder(VideoPid))
    {
        LOG_ERROR("%s Decoder[%d]: Re-Starting Decoder For Flush Failed",__FUNCTION__,DecoderID);
        return false;
    }

    if (DecoEvtLisnr) DecoEvtLisnr->FlushDone();

    return true;
}

bool
OMXNexusDecoder::Flush()
{
    LOG_FLUSH_MSGS("%s Decoder[%d]: FLUSH START", __FUNCTION__,DecoderID);
    Mutex::Autolock lock(mListLock);
    FlushCnt++;

    // On RX Side We are not holding the commands from the above layer
    // We just need to make sure that we have all our buffers in Free State

    LOG_FLUSH_MSGS("%s Decoder[%d]: Flushing Delivered List: NextExpectedFrame:%d FlushCnt:%d",
                   __FUNCTION__,DecoderID, NextExpectedFr,FlushCnt);

    // Start the NextExpectedFr again from 1, we do not know what decoder will return
    // accept anything that decoder returns after flush. The counter will automatically
    // normalize itself WRT the first frame received.
    NextExpectedFr=1;


    LOG_FLUSH_MSGS("%s Decoder[%d]: Flushing Delivered List",__FUNCTION__,DecoderID);
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

        LOG_FLUSH_MSGS("%s Decoder[%d]: Return [FLUSH-DROP] Frame [%d] Displayed:%s PTS:%lld [PTSValid:%d]",
                  __FUNCTION__,DecoderID,pFrFromList->FrStatus.serialNumber,
                       RetFrSetting.display ? "true":"false" ,
                       pFrFromList->MilliSecTS,
                       pFrFromList->FrStatus.ptsValid);

        NEXUS_SimpleVideoDecoder_ReturnDecodedFrames(decoHandle,&RetFrSetting,1);
        InsertHeadList(&EmptyFrList,&pFrFromList->ListEntry);
        EmptyFrListLen++;
        BCMOMX_DBG_ASSERT(EmptyFrListLen <= DECODE_DEPTH);
    }

    LOG_FLUSH_MSGS("%s Decoder[%d]: Flushing Decoded List",__FUNCTION__,DecoderID);
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
        LOG_FLUSH_MSGS("%s Decoder[%d]: Return [FLUSH-DROP] Frame [%d] Displayed:%s PTS:%lld [PTSValid:%d]",
                  __FUNCTION__,DecoderID,pFrFromList->FrStatus.serialNumber,
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
        LOG_ERROR("%s Decoder[%d]: WE HAVE LOST SOME FRAMES ExpectedFLL[%d] RealFLL[%d]",
                 __FUNCTION__,DecoderID,
                 EmptyFrListLen,
                 DECODE_DEPTH);

        BCMOMX_DBG_ASSERT(DECODE_DEPTH == EmptyFrListLen);
        return false;
    }

    LOG_FLUSH_MSGS("%s Decoder[%d]: Now Flushing Hardware",__FUNCTION__,DecoderID);
    if(IsDecoderStarted())
    {
        FlushDecoder();
    }

    LOG_FLUSH_MSGS("%s Decoder[%d]: FLUSH DONE", __FUNCTION__,DecoderID);
    return true;
}


#ifndef GENERATE_DUMMY_EOS
// Return the BTP packet to the hardware if required....
// Always called with lock held....From ReturnFrameSynchronised, we returning the
// last packet to the hardware.

void
OMXNexusDecoder::OnEOSMoveAllDecodedToDeliveredList()
{


    while (!IsListEmpty(&DecodedFrList))
    {
        PNEXUS_DECODED_FRAME pRetPacket = NULL;
        PLIST_ENTRY ThisEntry =  RemoveTailList(&DecodedFrList);
        DecodedFrListLen--;

        pRetPacket = CONTAINING_RECORD(ThisEntry, NEXUS_DECODED_FRAME,
                                       ListEntry);
        // This is the last packet that should be there in
        pRetPacket->BuffState = BufferState_Delivered;
        InsertHeadList(&DeliveredFrList, &pRetPacket->ListEntry);
    }
}

bool OMXNexusDecoder::ReturnPacketsAfterEOS()
{
    while (!IsListEmpty(&DeliveredFrList))
    {
        PNEXUS_DECODED_FRAME pRetPacket = NULL;
        PLIST_ENTRY ThisEntry = RemoveTailList(&DeliveredFrList);
        pRetPacket = CONTAINING_RECORD(ThisEntry, NEXUS_DECODED_FRAME,
                                       ListEntry);

        NEXUS_VideoDecoderReturnFrameSettings RetFrSetting;
        RetFrSetting.display = false;

        LOG_EOS_DBG("%s Decoder[%d]: Returning Packet After EOS SeqNo:%d ClientFlags:%d Pts:%d",
            __FUNCTION__, DecoderID,
            pRetPacket->FrStatus.serialNumber,
            pRetPacket->ClientFlags,
            pRetPacket->FrStatus.pts);

        NEXUS_SimpleVideoDecoder_ReturnDecodedFrames(decoHandle,&RetFrSetting,1);
            pRetPacket->BuffState = BufferState_Init;

    //Return the Frame Back To Empty List
        InsertHeadList(&EmptyFrList, &pRetPacket->ListEntry);
        EmptyFrListLen++;
    }

    //All the frames should be present in the Free list...
    BCMOMX_DBG_ASSERT(EmptyFrListLen==DECODE_DEPTH);
    return true;
}
#endif

//
// Lock is Already Taken
//private
bool
OMXNexusDecoder::ReturnFrameSynchronized(PNEXUS_DECODED_FRAME pNxDecFrame, bool FlagDispFrame)
{
    if(false == IsDecoderStarted())
    {
        LastErr = ErrStsDecoNotStarted;
        LOG_ERROR("%s Decoder[%d]: Decoder Not Started",__FUNCTION__,DecoderID);
        BCMOMX_DBG_ASSERT(false);
        return false;
    }

    if( IsListEmpty(&DeliveredFrList) )
    {
        //Nothing in the list
        LastErr = ErrStsDecodeListEmpty;
        LOG_ERROR("%s Decoder[%d]: Nothing To Return From List, This May Happen After Flush",
                 __FUNCTION__,DecoderID);
        BCMOMX_DBG_ASSERT(false);
        return false;
    }

    if (!FrameAlreadyExist(&DeliveredFrList, pNxDecFrame))
    {
        //Frame does not exists..Nothing to remove
        LOG_ERROR("%s Decoder[%d]: Return Frame Called But Frame Does Not Exists, pFrame:%p Display:%d",
                    __FUNCTION__,DecoderID,
                    pNxDecFrame,
                    FlagDispFrame);

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
                    LOG_ERROR("%s Decoder[%d]:: |**ERROR**| Frame Number Match But Addresses Dont!! This Should Never Happen!!",
                              __FUNCTION__,DecoderID);

                    BCMOMX_DBG_ASSERT(pFrFromList == pNxDecFrame);
                }
            }

            if (!RetFrSetting.display)
            {
                LOG_WARNING("%s Decoder[%d]: Return [DROP] Frame [Addr:%p][SeqNo:%d] Displayed:%s PTS:%d [PTSValid:%d]",
                            __FUNCTION__,DecoderID ,pFrFromList,
                            pFrFromList->FrStatus.serialNumber,
                            RetFrSetting.display ? "true":"false" ,
                            pFrFromList->FrStatus.pts,
                            pFrFromList->FrStatus.ptsValid);
            }else{
                LOG_INFO("%s Decoder[%d]: Return [DISPLAY] Frame [%d] Displayed:%s PTS:%d [PTSValid:%d]",
                                    __FUNCTION__, DecoderID,pFrFromList->FrStatus.serialNumber,
                                    RetFrSetting.display ? "true":"false" ,
                                    pFrFromList->FrStatus.pts,
                                    pFrFromList->FrStatus.ptsValid);
            }

            //Validate the Buffer State
            BCMOMX_DBG_ASSERT(pFrFromList->BuffState==BufferState_Delivered);
            pFrFromList->BuffState = BufferState_Init;

#ifndef GENERATE_DUMMY_EOS
            // The BTP Packet is never delivered to client, it is sitting in
            // decoded list. This means that we should never come here with
            // lastPicture flag set. Lets validate this condition.
            BCMOMX_DBG_ASSERT(!pFrFromList->FrStatus.lastPicture);

            //We should never get in to the condition where we are returning
            // a packet with a seqNumber greater than the seq number of BTP packet
            bool returnRemainingPackets = (IsEOSComplete()
                                           && (pFrFromList->ClientFlags));

#endif

            NEXUS_SimpleVideoDecoder_ReturnDecodedFrames(decoHandle,&RetFrSetting,1);
            //Return the Frame Back To Empty List
            InsertHeadList(&EmptyFrList,&pFrFromList->ListEntry);
            EmptyFrListLen++;
            BCMOMX_DBG_ASSERT(EmptyFrListLen <= DECODE_DEPTH);

#ifndef GENERATE_DUMMY_EOS
            if (returnRemainingPackets)
            {
                LOG_EOS_DBG("%s [%d] Decoder[%d]: Returning All Packets To Hardware After EOS",
                            __FUNCTION__,__LINE__,DecoderID);

                ReturnPacketsAfterEOS();
            }
#endif

            if (Done) return true;

        }else{
            LOG_ERROR("%s Decoder[%d]: **ERROR** DeliveredFrList NOT IN PROPER ORDER, Not returning Anything!!",
                      __FUNCTION__,DecoderID);

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
        LOG_ERROR("%s Decoder[%d]: Invalid Parameter In Display Frame",__FUNCTION__,DecoderID);
        LastErr = ErrStsInvalidParam;
        return false;
    }

    if (!pDispFr->DecodedFr || (pDispFr->DisplayCnxt != this) )
    {
        LOG_ERROR("%s Decoder[%d]: Nothing To Display %p %p %p!!",
                  __FUNCTION__,DecoderID,
                  pDispFr->DecodedFr,
                  pDispFr->DisplayCnxt,
                  this);

        LastErr = ErrStsInvalidParam;
        return false;
    }

    if (false == IsDecoderStarted())
    {
        LastErr = ErrStsDecoNotStarted;
        LOG_ERROR("%s Decoder[%d]: Decoder Not Started",__FUNCTION__,DecoderID);
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
        LOG_ERROR("%s Decoder[%d]: Decoder Not Started",__FUNCTION__,DecoderID);
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
            LOG_ERROR("%s Decoder[%d]: Drop Frame Failed!!\n\n",__FUNCTION__,DecoderID);
            BCMOMX_DBG_ASSERT(false);
        }
    }

    if(BufferState_Init != pProcessFr->BuffState)
    {
        LOG_ERROR("%s Decoder[%d]: INVALID BUFFER STATE----ASSERTING NOW!!!!\n\n",__FUNCTION__,DecoderID);
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
        LOG_ERROR("%s Decoder[%d]: ==ERROR== Trying To Fetch [%d] More Than The DECODE_DEPTH[%d] !!",
                  __FUNCTION__,DecoderID,NumFramesToFetch,DECODE_DEPTH);

        LastErr = ErrStsOutOfResource;
        return false;
    }

    if (IsEOSReceivedFromHardware())
    {
        LOG_EOS_DBG("%s Decoder[%d]: We Have Already Received EOS From HArdware,"
                    " Not pulling anymore frames!!", __FUNCTION__,DecoderID);
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
                    ALOGE("%s: error", __FUNCTION__);
                    continue;
                }

                if( (StartupTime != STARTUP_TIME_INVALID) &&
                    (OutFrameStatus[FrCnt].pts < StartupTime) &&
                    (false == OutFrameStatus[FrCnt].lastPicture) &&
                    (OutFrameStatus[FrCnt].ptsValid) )
                {
                    LOG_WARNING("%s Decoder[%d]:  ========= WARNING THIS SHOULD NEVER HAPPEN ========== "
                                "SeqNo[%d] FramePts[%d] < StartupTime[%d]: Returning Frame Back To Hardware",
                              __FUNCTION__, DecoderID,
                                OutFrameStatus[FrCnt].serialNumber,
                                OutFrameStatus[FrCnt].pts,
                                StartupTime);

                    BCMOMX_DBG_ASSERT(false);

#if 0
                    if ( (!FrCnt) &&                        //This is the First Frame We Got
                         IsListEmpty(&DecodedFrList) &&     //We Dont have any other frames in the Decoded List
                         IsListEmpty(&DeliveredFrList) )    //We Dont have any other frames in the delivered list
                    {
                        NEXUS_VideoDecoderReturnFrameSettings RetFrSetting;
                        RetFrSetting.display = false;
                        NEXUS_SimpleVideoDecoder_ReturnDecodedFrames(decoHandle,&RetFrSetting,1);
                    }else{
                        LOG_WARNING("%s: FramePts[%d] < StartupTime[%d]: Waiting For Queues To Drain",
                                  __FUNCTION__,
                                  OutFrameStatus[FrCnt].pts,
                                  StartupTime);
                    }

                    return true;
#endif
                }

                //Get A Free Entry...
                pListEntry =  RemoveTailList(&EmptyFrList);
                if (!pListEntry)
                {
                    LOG_ERROR("%s Decoder[%d]: ==== ERROR Got A Frame But No Entry In Free List[%d]===",
                              __FUNCTION__,DecoderID,EmptyFrListLen);
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

#ifndef GENERATE_DUMMY_EOS
                if ((pFreeFr->FrStatus.lastPicture) || (IsEOSReceivedOnInput() &&
                     (pFreeFr->MilliSecTS == EOSFrameKey)))
                {
                    LOG_EOS_DBG("%s [%d] Decoder[%d]: Detected Last Frame On SeqN- %d LastPicFlag:%d"
                        "EOSState:0x%x ClientFlags:%d pFreeFr->MilliSecTS:%lld EOSFrameKey:%lld",
                                __FUNCTION__, __LINE__,DecoderID,
                        pFreeFr->FrStatus.serialNumber, pFreeFr->FrStatus.lastPicture,
                        EOSState, ClientFlags, pFreeFr->MilliSecTS, EOSFrameKey);


                    // We should always have the EOS on the input side
                    // Because only then we have sent the BTP/BPP packet to the hardware.
                    if(!EOSFrameKey)
                    {
                         LOG_WARNING("%s [%d] Decoder[%d]: EOS Frame Key is Zero[%lld]. ",
                                     __FUNCTION__, __LINE__, DecoderID,EOSFrameKey);
                    }

                    BCMOMX_DBG_ASSERT(ClientFlags);
                    EOSReceivedFromHardware();
                    pFreeFr->ClientFlags = ClientFlags;
                }
#endif

                //Insert At Head...
                InsertHeadList(&DecodedFrList,&pFreeFr->ListEntry);
                DecodedFrListLen++;
                BCMOMX_DBG_ASSERT(DecodedFrListLen <= DECODE_DEPTH);

                LastErr = ErrStsSuccess;

                LOG_OUTPUT_TS("%s Decoder[%d]: Accepted FRAME SeqNo:%d NxTimeStamp:%d Ts[Ms]:%lld ClientFlags:%d!!",
                              __FUNCTION__, DecoderID,
                              pFreeFr->FrStatus.serialNumber,
                              pFreeFr->FrStatus.pts,
                              pFreeFr->MilliSecTS,
                              pFreeFr->ClientFlags);
                DebugCounter = (DebugCounter + 1) % DECODER_STATE_FREQ;
                if(!DebugCounter) PrintDecoStats();
            }
        }else{
            LOG_ERROR("%s Decoder[%d]: ===NEXUS RETURNED ERROR WHILE GETTING FRAMES=== ",__FUNCTION__,DecoderID);
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
        LOG_ERROR("%s Decoder[%d]: Invalid Parameters",__FUNCTION__,DecoderID);
        LastErr = ErrStsInvalidParam;
        return false;
    }

    Mutex::Autolock lock(mListLock);
    memset(pOutFrame,0,sizeof(DISPLAY_FRAME));

#ifndef GENERATE_DUMMY_EOS
    if(IsEOSDeliveredOnOutput())
    {
        // We do not deliver Any Frames After The EOS Is Set On Output, So The BTP packet will always
        // be in the decoded list and we remove it from there when the
        // we return the EOS marked packet to the hardware.
        //pOutFrame->OutFlags = ClientFlags;
        LOG_EOS_DBG("%s Decoder[%d]: EOS Delivered To Output, Not Delivering Anything",
                    __FUNCTION__,DecoderID);
        return false;
    }

#endif

    //Deliver From DecodedQueue Tail (size -1)
    if(!IsListEmpty(&DecodedFrList))
    {
        PLIST_ENTRY DeliverEntry = RemoveTailList(&DecodedFrList);
        bool retVal = false;
        DecodedFrListLen--;
        BCMOMX_DBG_ASSERT(DecodedFrListLen <= DECODE_DEPTH);

        if (DeliverEntry)
        {
            PNEXUS_DECODED_FRAME pDecoFr = CONTAINING_RECORD(DeliverEntry,
                                                             NEXUS_DECODED_FRAME, ListEntry);
            pDecoFr->BuffState = BufferState_Delivered;

            //Clear the out flags.
            pOutFrame->OutFlags       = 0;
#ifndef GENERATE_DUMMY_EOS
            pOutFrame->OutFlags = DetectEOS(pDecoFr);
#endif
            if (!pDecoFr->FrStatus.lastPicture)
            {
                pOutFrame->DisplayCnxt = this;
                pOutFrame->DecodedFr = pDecoFr;
                pOutFrame->pDisplayFn = DisplayBuffer;
                InsertHeadList(&DeliveredFrList, &pDecoFr->ListEntry);
                LOG_EVERY_DELIVERY("%s Decoder[%d]: Delivered FRAME SeqNo:%d TimeStamp:%d pOutFrame->OutFlags:%d StartupTime:%d!!",
                                   __FUNCTION__, DecoderID,
                                   pDecoFr->FrStatus.serialNumber,
                                   pDecoFr->FrStatus.pts,
                                   pOutFrame->OutFlags,
                                   StartupTime);
                retVal = true;
            } else {
                LOG_EVERY_DELIVERY("%s  Decoder[%d]: lastPicture Set, Not Delivering To Client Frame FRAME SeqNo:%d!!",
                                   __FUNCTION__, DecoderID,
                                   pDecoFr->FrStatus.serialNumber);
                InsertHeadList(&DeliveredFrList, &pDecoFr->ListEntry);
                retVal = false;
            }

#ifndef GENERATE_DUMMY_EOS
            if (pOutFrame->OutFlags)
            {
                // Looks like the EOS is set...Move All The Frames To Delivered List
                // Logically, all the frames should be in delivered list when we hit the EOS.
                OnEOSMoveAllDecodedToDeliveredList();
            }
#endif
            return retVal;
        }
    }

#ifdef GENERATE_DUMMY_EOS
    // If there is anything to deliver or display, We do not process EOS.
    // Let both the list get empty and then we will process the EOS.
    if(IsListEmpty(&DecodedFrList) && IsListEmpty(&DeliveredFrList))
    {
        BCMOMX_DBG_ASSERT(!DecodedFrListLen);
        pOutFrame->OutFlags=0;

        if (DetectEOS())
        {
            LOG_EOS_DBG("%s Decoder[%d]: Delivering EOS Frame ClientFlags[%d]",__FUNCTION__,DecoderID,ClientFlags);
            pOutFrame->OutFlags = ClientFlags;
            if (DecoEvtLisnr) DecoEvtLisnr->EOSDelivered();
            ClientFlags=0;
        }
    }else{
        DownCnt=DOWN_CNT_DEPTH;
    }
#endif

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

void
OMXNexusDecoder::SetStartupTime(unsigned int StartTimeStamp)
{
    StartupTime = StartTimeStamp;
    ReSetEOSState();
}

bool
OMXNexusDecoder::IsDecoderStarted()
{
    NEXUS_VideoDecoderStatus vdecStatus;

    if(GetDecoSts(&vdecStatus))
    {
        return vdecStatus.started;
    }else{
        LOG_ERROR("%s Decoder[%d]: Failed To Get Decoder State",__FUNCTION__,DecoderID);
    }

    return false;
}

void
OMXNexusDecoder::PrintDecoStats()
{
    NEXUS_VideoDecoderStatus vdecStatus;
    if(GetDecoSts(&vdecStatus))
    {
        LOG_DECODER_STATE("%s Decoder[%d]: ====== DECODER STATS ====== \n Started:%d FrameRate:%d TsmMode:%s PTS-Type:%d "
                          "PTSErrCnt:%d PicsDecoded:%d BuffLatency[Ms]:%d numDispDrops:%d\n===============",
                         __FUNCTION__, DecoderID,
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

    LOG_ERROR("%s Decoder[%d]: Failed To Get Decoder State",__FUNCTION__,DecoderID);
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

void OMXNexusDecoder::InputEOSReceived(unsigned int ClientFlagsData,
                                       unsigned long long EOSFrameKeyData)
{
    LOG_EOS_DBG("%s Decoder[%d]: Entering EOS Detection Mode=== ClientFlagData:%d EOSFrameKeyData%lld",
                __FUNCTION__, DecoderID, ClientFlagsData, EOSFrameKeyData);

    EOSReceivedOnInput();
    //Save The Flags that Client Wants Us To Set In The
    //Output frames On Detection Of EOS.
    ClientFlags = ClientFlagsData;
    EOSFrameKey = EOSFrameKeyData;
    if(!EOSFrameKey)
    {
        LOG_WARNING("%s [%d] Decoder[%d]: EOS Frame Key Received is Zero[%lld]. "
                    "Next Frame With Zero TimeStamp Will be EOS Frame"
                    ,__FUNCTION__,
                    __LINE__, DecoderID,
                    EOSFrameKey);
    }
    // Looks like Zero Time Stamp Is a Valid Test Case For the CTS Test. We cannot assert on that condition.
    //BCMOMX_DBG_ASSERT(EOSFrameKey);

#ifdef GENERATE_DUMMY_EOS
    DownCnt = DOWN_CNT_DEPTH;
#endif

}

#ifdef GENERATE_DUMMY_EOS
bool
OMXNexusDecoder::DetectEOS()
{
    NEXUS_VideoDecoderStatus  VidDecoSts;
    if(false == IsEOSReceivedOnInput())
    {
        return false;
    }

    LOG_EOS_DBG("%s Decoder[%d]: EOSDetect[%s] ",
                __FUNCTION__, DecoderID,
                IsEOSReceivedOnInput() ? "TRUE":"FALSE");

    GetDecoSts(&VidDecoSts);
    if (VidDecoSts.started)
    {
        if (DownCnt)
        {
            DownCnt--;
        }else{

            LOG_EOS_DBG("%s Decoder[%d]: --- EOS DETECTED ---",__FUNCTION__,DecoderID);

            //Just have to detect the EOS only once
            ReSetEOSState();
            return true;
        }
    }

    return false;
}
#else
unsigned int OMXNexusDecoder::DetectEOS(PNEXUS_DECODED_FRAME pDecoFr)
{
    if (pDecoFr == NULL) return 0;

    if(IsEOSReceivedOnInput() && IsEOSReceivedFromHardware())
    {
        if (pDecoFr->ClientFlags)
        {
            LOG_EOS_DBG(
                "%s Decoder[%d]:  EOS [Flags= %d] Marked On FRAME SeqNo:%d LastPicture:%d!! EOSFrameKey:%lld pDecoFr->MilliSecTS:%lld",
                __FUNCTION__, DecoderID, pDecoFr->ClientFlags,
                    pDecoFr->FrStatus.serialNumber,
                pDecoFr->FrStatus.lastPicture,
                EOSFrameKey,
                pDecoFr->MilliSecTS);

            EOSDeliveredToOutput();
            if (DecoEvtLisnr) DecoEvtLisnr->EOSDelivered();
            return pDecoFr->ClientFlags; //Mark this as EOS because this is the seq number that we wanted to mark
        }
    }


    return 0;
}

#endif

void
OMXNexusDecoder::SourceChangedCallback()
{
    NEXUS_VideoDecoderStatus vdecStatus;
    if(GetDecoSts(&vdecStatus))
    {
        LOG_EOS_DBG("%s Decoder[%d]: Number Pictures Decoded:- %d",
                __FUNCTION__,DecoderID,vdecStatus.numDecoded);

    }else{
        LOG_ERROR("%s Decoder[%d]: Failed To Get Decoder State",__FUNCTION__,DecoderID);
    }

}

unsigned int
OMXNexusDecoder::GetDecoderID()
{
    return DecoderID;
}
void
OMXNexusDecoder::AddDecoderToList()
{
    List<OMXNexusDecoder *>::iterator it = DecoderInstanceList.begin();
    unsigned int minId = 0;
    while (it != DecoderInstanceList.end())
    {
        if ((*it)->DecoderID == minId)
        {
            minId++;
            it = DecoderInstanceList.begin();
        } else {
            it++;
        }
    }
    DecoderID = minId;
    DecoderInstanceList.push_back(this);
    return;
}
void
OMXNexusDecoder::RemoveDecoderFromList()
{
    List<OMXNexusDecoder *>::iterator it;
    for (it = DecoderInstanceList.begin(); it != DecoderInstanceList.end(); it++)
    {
        if ((*it)->DecoderID == DecoderID)
        {
            DecoderInstanceList.erase(it);
            break;
        }
    }
}
bool
DisplayBuffer (void *pInFrame, void *pInOMXNxDeco)
{
    PDISPLAY_FRAME pFrame = (PDISPLAY_FRAME) pInFrame;
    OMXNexusDecoder *pOMXNxDeco = (OMXNexusDecoder *) pInOMXNxDeco;

    if(!pFrame || !pOMXNxDeco || !pFrame->DecodedFr)
    {
        LOG_ERROR("%s : Invalid Parameters To Display Function %p %p %p",
                  __FUNCTION__,
                  pFrame,
                  pOMXNxDeco,
                  pFrame->DecodedFr);

        return false;
    }

    return pOMXNxDeco->DisplayFrame(pFrame);
}

