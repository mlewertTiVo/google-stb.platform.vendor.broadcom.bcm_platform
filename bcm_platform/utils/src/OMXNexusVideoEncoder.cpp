#include "OMXNexusVideoEncoder.h"

#undef LOG_TAG
#define LOG_TAG "BCM_OMX_VIDEO_ENC"

//Path level Debug Messages
#define LOG_START_STOP_DBG      ALOGD
#define LOG_EOS_DBG             ALOGD
#define LOG_FLUSH_MSGS          ALOGD
#define LOG_CREATE_DESTROY      ALOGD
#define LOG_RETRIVE_FRAME

//Path independent Debug Messages
#define LOG_WARNING             ALOGD
#define LOG_ERROR               ALOGD
#define LOG_INFO                ALOGD

#define NAL_UNIT_TYPE_SPS       7
#define NAL_UNIT_TYPE_PPS       8

// Macro to extract NAL unit type from NEXUS_VideoEncoderDescriptor.dataUnitTye
// NAL unit type is the LSB 5 bits value
#define GET_NAL_UNIT_TYPE(x)    (x & 0x1F)
static void imageBufferCallbackDispatcher(void *context, int param)
{
    OMXNexusVideoEncoder    *pEncoder = (OMXNexusVideoEncoder *)context;
    pEncoder->imageBufferCallback();
}

// Set enable_es_dump to "true" to dump encoded ES frames.
static bool enable_es_dump = false;

// Set dump_once to 0 to dump first pair of input/output frames from frame conversion.
static bool dump_once = 1;
static void convertOMXPixelFormatToCrYCbY(uint8_t *dst, uint8_t *src, unsigned int width, unsigned int height, OMX_COLOR_FORMATTYPE colorFormat)
{
    //Currently we support only one format
    BCMOMX_DBG_ASSERT(colorFormat==OMX_COLOR_FormatYUV420Planar);

    int32_t inYsize = width * height;
    uint8_t *iny = src;
    uint8_t *incb = src + inYsize;
    uint8_t *incr = src + inYsize + (inYsize >> 2);
    uint8_t *temp;

    if (dump_once==0)
        temp = dst;

    for (uint32_t i = 0; i < height/2; i++)
    {
        for (uint32_t j = 0; j < width/2; j++)
        {
            uint8_t y0, y1, cr, cb;

            y0 = *(iny + 2 * i * width + 2*j);
            cb = *(incb + i * width/2 + j);
            y1 = *(iny + 2 * i * width + 2*j + 1);
            cr = *(incr + i * width/2 + j);

            *((uint32_t *)dst + 2 * i * width/2 + j) = cr << 24 | y1 << 16 | cb << 8 | y0;

            y0 = *(iny + (2*i+1) * width + 2*j);
            cb = *(incb + i * width/2 + j);
            y1 = *(iny + (2*i+1) * width + 2*j + 1);
            cr = *(incr + i * width/2 + j);

            *((uint32_t *)dst + (2*i+1)* width/2 + j) = cr << 24 | y1 << 16 | cb << 8 | y0;

        }
    }

    if (dump_once==0)
    {
        DumpData d0("yuv420.dat");
        d0.WriteData((void *)src,inYsize + inYsize/2);

        DumpData d1("crycby.dat");
        d1.WriteData((void *)temp, inYsize * 4);
        dump_once = 1;
    }
}

typedef struct _OMX_TO_NEXUS_PROFILE_TYPE_
{
    OMX_VIDEO_AVCPROFILETYPE omxProfile;
    NEXUS_VideoCodecProfile nexusProfile;
} OMX_TO_NEXUS_PROFILE_TYPE;

typedef struct _OMX_TO_NEXUS_LEVEL_TYPE_
{
    OMX_VIDEO_AVCLEVELTYPE omxLevel;
    NEXUS_VideoCodecLevel nexusLevel;
} OMX_TO_NEXUS_LEVEL_TYPE;

static const OMX_TO_NEXUS_PROFILE_TYPE ProfileMapTable[] =
{
    {OMX_VIDEO_AVCProfileBaseline,      NEXUS_VideoCodecProfile_eBaseline},
    {OMX_VIDEO_AVCProfileMain,          NEXUS_VideoCodecProfile_eMain},
    {OMX_VIDEO_AVCProfileHigh,          NEXUS_VideoCodecProfile_eHigh}
};

static const OMX_TO_NEXUS_LEVEL_TYPE LevelMapTable[] =
{
    {OMX_VIDEO_AVCLevel1,                NEXUS_VideoCodecLevel_e10},
    {OMX_VIDEO_AVCLevel1b,               NEXUS_VideoCodecLevel_e1B},
    {OMX_VIDEO_AVCLevel11,               NEXUS_VideoCodecLevel_e11},
    {OMX_VIDEO_AVCLevel12,               NEXUS_VideoCodecLevel_e12},
    {OMX_VIDEO_AVCLevel13,               NEXUS_VideoCodecLevel_e13},
    {OMX_VIDEO_AVCLevel2,                NEXUS_VideoCodecLevel_e20},
    {OMX_VIDEO_AVCLevel21,               NEXUS_VideoCodecLevel_e21},
    {OMX_VIDEO_AVCLevel22,               NEXUS_VideoCodecLevel_e22},
    {OMX_VIDEO_AVCLevel3,                NEXUS_VideoCodecLevel_e30},
    {OMX_VIDEO_AVCLevel31,               NEXUS_VideoCodecLevel_e31},
    {OMX_VIDEO_AVCLevel32,               NEXUS_VideoCodecLevel_e32},
    {OMX_VIDEO_AVCLevel4,                NEXUS_VideoCodecLevel_e40},
    {OMX_VIDEO_AVCLevel41,               NEXUS_VideoCodecLevel_e41},
    {OMX_VIDEO_AVCLevel42,               NEXUS_VideoCodecLevel_e42},
    {OMX_VIDEO_AVCLevel5,                NEXUS_VideoCodecLevel_e50},
    {OMX_VIDEO_AVCLevel51,               NEXUS_VideoCodecLevel_e51},
};

static NEXUS_VideoCodecProfile convertOMXProfileTypetoNexus(OMX_VIDEO_AVCPROFILETYPE profile)
{
    for (unsigned int i = 0; i < sizeof(ProfileMapTable)/sizeof(ProfileMapTable[0]); i++)
    {
        if (ProfileMapTable[i].omxProfile==profile)
            return ProfileMapTable[i].nexusProfile;
    }

    return NEXUS_VideoCodecProfile_eBaseline;
}

static NEXUS_VideoCodecLevel convertOMXLevelTypetoNexus(OMX_VIDEO_AVCLEVELTYPE level)
{
    for(unsigned int i = 0; i < sizeof(LevelMapTable)/sizeof(LevelMapTable[0]); i++)
    {
        if(LevelMapTable[i].omxLevel==level)
            return LevelMapTable[i].nexusLevel;
    }

    return NEXUS_VideoCodecLevel_e31;
}

OMXNexusVideoEncoder::OMXNexusVideoEncoder(char *CallerName, int numInBuf)
    : EncoderHandle(NULL),
    NumNxSurfaces(numInBuf),
    EmptyFrListLen(0),EncodedFrListLen(0),
    SurfaceAvailListLen(0),
    LastErr(ErrStsSuccess),
    EncoderStarted(false),
    CaptureFrames(true)
{
    LOG_CREATE_DESTROY("%s: ENTER ===",__FUNCTION__);
    InitializeListHead(&EmptyFrList);
    InitializeListHead(&EncodedFrList);
    InitializeListHead(&SurfaceBusyList);
    InitializeListHead(&SurfaceAvailList);
    InitializeListHead(&InputContextList);

    b_refsw_client_client_configuration         config;
    b_refsw_client_client_info                  client_info;
    b_refsw_client_connect_resource_settings    connectSettings;
    NxIPCClient = NexusIPCClientFactory::getClient(CallerName);
    NEXUS_SurfaceCreateSettings surfaceCfg;

    BKNI_Memset(&config, 0, sizeof(config));
    BKNI_Snprintf(config.name.string,sizeof(config.name.string),CallerName);

    config.resources.encoder = true;
    config.resources.audioDecoder = false;
    config.resources.audioPlayback = false;
#ifdef ENCODE_DISPLAY
    config.resources.videoDecoder = false;
#else
    config.resources.videoDecoder = true;
#endif
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
#ifdef ENCODE_DISPLAY
    connectSettings.simpleEncoder[0].display = true;
#else
    connectSettings.simpleEncoder[0].display = false;
    connectSettings.simpleVideoDecoder[0].id = client_info.videoDecoderId;
    connectSettings.simpleVideoDecoder[0].windowCaps.encoder = true;
#endif

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
        LOG_ERROR("%s Unable to acquire Simple Encoder \n",__FUNCTION__);
        LastErr = ErrStsDecoAcquireFail;
        return;
    }else{
        LOG_CREATE_DESTROY("%s: Acquired Encoder Handle", __FUNCTION__);
    }

    DecoderHandle = NxIPCClient->acquireVideoDecoderHandle();
    if ( NULL == DecoderHandle )
    {
        LOG_ERROR("%s Unable to acquire Simple Video Decoder \n",__FUNCTION__);
        LastErr = ErrStsDecoAcquireFail;
        return;
    }else{
        LOG_CREATE_DESTROY("%s: Acquired Simple Video Decoder Handle", __FUNCTION__);
    }

    NEXUS_SimpleStcChannelSettings stcSettings;
    NEXUS_Error rc;

    StcChannel = NEXUS_SimpleStcChannel_Create(NULL);
    NEXUS_SimpleStcChannel_GetSettings(StcChannel, &stcSettings);
    stcSettings.mode = NEXUS_StcChannelMode_eAuto;
    rc = NEXUS_SimpleStcChannel_SetSettings(StcChannel, &stcSettings);
    if (rc)
    {
        LOG_ERROR("%s",__FUNCTION__);
        LastErr = ErrStSTCChannelFailed;
        return;
    } else {
            LOG_CREATE_DESTROY("%s: Set STC Channel ", __FUNCTION__);
    }

    rc = NEXUS_SimpleVideoDecoder_SetStcChannel(DecoderHandle, StcChannel);
    if (rc)
    {
        LOG_ERROR("%s",__FUNCTION__);
        LastErr = ErrStSTCChannelFailed;
        return;
    } else {
            LOG_CREATE_DESTROY("%s: Set STC Channel ", __FUNCTION__);
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

    if (enable_es_dump)
        pdumpES = new DumpData("videnc_es.dat");

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

    if (pdumpES)
        delete(pdumpES);

    BCMOMX_DBG_ASSERT(NumEntriesFreed==VIDEO_ENCODE_DEPTH);
    LOG_CREATE_DESTROY("%s: Deleting The NxIPCClient",__FUNCTION__);
    delete NxIPCClient;
}


bool
OMXNexusVideoEncoder::StartEncoder(PVIDEO_ENCODER_START_PARAMS pStartParams)
{
    NEXUS_Error errCode;
    NEXUS_SimpleEncoderStartSettings EncoderStartSettings;
    NEXUS_VideoImageInputSettings imageInputSetting;
    NEXUS_SimpleEncoderSettings encoderSettings;
    NEXUS_SurfaceCreateSettings surfaceCfg;
    NEXUS_ClientConfiguration clientConfig;
    int i;
    NEXUS_VideoImageInputStatus imageInputStatus;

    if (EncoderStarted)
    {
        LOG_START_STOP_DBG("%s[%d]: Encoder Already Started",
                           __FUNCTION__, __LINE__);

        LastErr = ErrStsStartEncoFailed;
        return false;
    }

    NEXUS_SimpleEncoder_GetSettings(EncoderHandle, &encoderSettings);
    encoderSettings.video.width = pStartParams->width;
    encoderSettings.video.height = pStartParams->height;
    encoderSettings.videoEncoder.frameRate = pStartParams->frameRate;
    NEXUS_SimpleEncoder_SetSettings(EncoderHandle, &encoderSettings);
    BDBG_MSG(("Encoder setup done"));

    NEXUS_SimpleEncoder_GetDefaultStartSettings(&EncoderStartSettings);
#ifdef ENCODE_DISPLAY
    EncoderStartSettings.input.display = true;
#else
    EncoderStartSettings.input.video = DecoderHandle;
#endif
    EncoderStartSettings.output.video.settings.codec = NEXUS_VideoCodec_eH264;
    EncoderStartSettings.output.transport.type = NEXUS_TransportType_eEs;
    EncoderStartSettings.output.video.settings.profile = convertOMXProfileTypetoNexus(pStartParams->avcParams.eProfile);
    EncoderStartSettings.output.video.settings.level = convertOMXLevelTypetoNexus(pStartParams->avcParams.eLevel);

    ALOGE("%s: Profile = %d, Level = %d, width = %d, height = %d",__FUNCTION__, EncoderStartSettings.output.video.settings.profile, EncoderStartSettings.output.video.settings.level, pStartParams->width, pStartParams->height);
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

    ImageInput = NEXUS_SimpleVideoDecoder_StartImageInput(DecoderHandle, NULL);
    if (!ImageInput)
    {
        LOG_ERROR("%s[%d]: NEXUS_SimpleVideoDecoder_StartImageInput Failed !!",
            __FUNCTION__, __LINE__);

        LastErr = ErrStsStartEncoFailed;
        return false;
    } else
    {
        LOG_START_STOP_DBG("%s[%d]: Image Input Started Successfully !!",
            __FUNCTION__, __LINE__);
    }

    NEXUS_VideoImageInput_GetStatus(ImageInput, &imageInputStatus);

    NEXUS_Surface_GetDefaultCreateSettings(&surfaceCfg);

    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    for (i=0;i<NEXUS_MAX_HEAPS;i++) {
        NEXUS_MemoryStatus s;
        if (!clientConfig.heap[i] || NEXUS_Heap_GetStatus(clientConfig.heap[i], &s)) continue;
        if (s.memcIndex == imageInputStatus.memcIndex && (s.memoryType & NEXUS_MemoryType_eApplication) && s.largestFreeBlock >= 960*1080*2) {
            surfaceCfg.heap = clientConfig.heap[i];
            BDBG_WRN(("found heap[%d] on MEMC%d for VideoImageInput", i, s.memcIndex));
            break;
        }
    }
    if (!surfaceCfg.heap) {
        BDBG_ERR(("no heap found. RTS failure likely."));
    }
    //TODO: Remove width/height hardcoding
    surfaceCfg.width  = pStartParams->width;
    surfaceCfg.height = pStartParams->height;
    surfaceCfg.pixelFormat = NEXUS_PixelFormat_eCr8_Y18_Cb8_Y08;


    pSurface = (PNEXUS_SURFACE*) malloc(sizeof(PNEXUS_SURFACE)*NumNxSurfaces);

    for (unsigned int i = 0; i < NumNxSurfaces; i++)
    {
        pSurface[i] = (PNEXUS_SURFACE) malloc(sizeof(NEXUS_SURFACE));
        if (!pSurface[i])
        {
            LastErr = ErrStsNexusSurfaceFailed;
            return false;
        }
        pSurface[i]->handle = NEXUS_Surface_Create(&surfaceCfg);
        if (!pSurface[i]->handle)
        {
            LastErr = ErrStsNexusSurfaceFailed;
            return false;
        }
        InsertHeadList(&SurfaceAvailList, &(pSurface[i]->ListEntry));
        SurfaceAvailListLen++;
    }
    LOG_CREATE_DESTROY("%s: Nexus surfaces created",__FUNCTION__);

    NEXUS_VideoImageInput_GetSettings(ImageInput, &imageInputSetting);
    imageInputSetting.imageCallback.callback = imageBufferCallbackDispatcher;
    imageInputSetting.imageCallback.context  = static_cast <void *>(this);
    NEXUS_VideoImageInput_SetSettings(ImageInput, &imageInputSetting);

    //Reset codec config done flag.
    CodecConfigDone = false;

    LastErr = ErrStsSuccess;
    EncoderStartParams = *pStartParams;
    EncoderStarted = true;
    return true;
}

void
OMXNexusVideoEncoder::StopEncoder()
{
    if (EncoderStarted)
    {
        NEXUS_SimpleVideoDecoder_StopImageInput(DecoderHandle);
        NEXUS_SimpleEncoder_Stop(EncoderHandle);

        while(!IsListEmpty(&SurfaceAvailList))
        {
            PNEXUS_SURFACE pSurf = NULL;
            PLIST_ENTRY ThisEntry =  RemoveTailList(&SurfaceAvailList);
            SurfaceAvailListLen--;
            BCMOMX_DBG_ASSERT(ThisEntry);
            pSurf = CONTAINING_RECORD(ThisEntry,NEXUS_SURFACE,ListEntry);
            BCMOMX_DBG_ASSERT(pSurf);
        }
            BCMOMX_DBG_ASSERT(SurfaceAvailListLen==0);

        for (unsigned int i = 0; i < NumNxSurfaces; i++)
        {
            if (pSurface[i]->handle)
            {
                NEXUS_Surface_Destroy(pSurface[i]->handle);
                pSurface[i]->handle = NULL;
            }

            if (pSurface[i])
            {
                free(pSurface[i]);
                pSurface[i] = NULL;
            }
        }

        if (pSurface)
        {
            free(pSurface);
            pSurface = NULL;
        }

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
OMXNexusVideoEncoder::Flush()
{
    while(!IsListEmpty(&EncodedFrList))
    {
        PNEXUS_ENCODED_VIDEO_FRAME  pEncFrame = NULL;
        PLIST_ENTRY ThisEntry =  RemoveTailList(&EncodedFrList);
        EncodedFrListLen--;
        BCMOMX_DBG_ASSERT(ThisEntry);
        pEncFrame = CONTAINING_RECORD(ThisEntry,NEXUS_ENCODED_VIDEO_FRAME,ListEntry);
        BCMOMX_DBG_ASSERT(pEncFrame);
        InsertHeadList(&EmptyFrList,&pEncFrame->ListEntry);
        EmptyFrListLen++;
    }
    BCMOMX_DBG_ASSERT(EncodedFrListLen==0);
    BCMOMX_DBG_ASSERT(EmptyFrListLen==VIDEO_ENCODE_DEPTH);

    while(!IsListEmpty(&InputContextList))
    {
        PNEXUS_VIDEO_ENCODER_INPUT_CONTEXT  pContext = NULL;
        PLIST_ENTRY ThisEntry =  RemoveTailList(&InputContextList);
        BCMOMX_DBG_ASSERT(ThisEntry);
        pContext = CONTAINING_RECORD(ThisEntry,NEXUS_VIDEO_ENCODER_INPUT_CONTEXT,ListEntry);
        BCMOMX_DBG_ASSERT(pContext);
    }

    StopEncoder();
    StartEncoder(&EncoderStartParams);
    return true;
}

bool
OMXNexusVideoEncoder::EncodeFrame(PNEXUS_VIDEO_ENCODER_INPUT_CONTEXT pNxInputContext)
{
    NEXUS_SurfaceMemory mem;
    NEXUS_VideoImageInputSurfaceSettings surfSettings;
    Mutex::Autolock lock(mListLock);
    InsertHeadList(&InputContextList,&pNxInputContext->ListEntry);

    // Assign a nexus surface
    BCMOMX_DBG_ASSERT(!IsListEmpty(&SurfaceAvailList));
    PLIST_ENTRY pSurfaceEntry = RemoveTailList(&SurfaceAvailList);
    SurfaceAvailListLen--;
    BCMOMX_DBG_ASSERT(pSurfaceEntry);
    PNEXUS_SURFACE pNxSurface = CONTAINING_RECORD(pSurfaceEntry, NEXUS_SURFACE, ListEntry);
    pNxInputContext->pNxSurface = pNxSurface;
    // Maybe SurfaceBusyList is redundant? Enable if needed.
    //InsertHeadList(&SurfaceBusyList, &pNxSurface->ListEntry);
    NEXUS_Surface_GetMemory(pNxInputContext->pNxSurface->handle, &mem);
    BCMOMX_DBG_ASSERT(mem.buffer);
    convertOMXPixelFormatToCrYCbY((uint8_t *)mem.buffer, (uint8_t *)pNxInputContext->bufPtr, pNxInputContext->width, pNxInputContext->height, pNxInputContext->colorFormat);
    NEXUS_Surface_Flush(pNxInputContext->pNxSurface->handle);

    NEXUS_VideoImageInput_GetDefaultSurfaceSettings(&surfSettings);
    surfSettings.pts = CONVERT_USEC_45KHZ(pNxInputContext->uSecTS);
    NEXUS_VideoImageInput_PushSurface(ImageInput, pNxInputContext->pNxSurface->handle, &surfSettings);

    StartCaptureFrames();

    return true;
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

        if ((enable_es_dump) && CaptureFrameEnabled())
        {
            ALOGD("%s:Dumping ES data. Buffer Address = %p, Buffer Size = %d",__FUNCTION__, pDeliverFr->pVideoDataBuff, pDeliverFr->SzVidDataBuff);
            pdumpES->WriteData((void *) pDeliverFr->pVideoDataBuff,pDeliverFr->SzVidDataBuff);
        }

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
    bool codeConfigFrame = pReturnFr->ClientFlags & OMX_BUFFERFLAG_CODECCONFIG;
    FAST_INIT_ENC_VID_FR(pReturnFr);

    //The Vector should be empty now
    BCMOMX_DBG_ASSERT(0==pReturnFr->FrameData->size());

    InsertHeadList(&EmptyFrList,&pReturnFr->ListEntry);
    EmptyFrListLen++;

    // We extract and deliver codec config data at the very beginning. This data comes in middle of
    // first encoded frame. We don't want to return codec config descriptors to hardware so that
    // we can read and deliver complete first frame in next cycle.
    if (codeConfigFrame)
    {
        ALOGE("Codec config descriptors. Not returning to hardware.");
        return true;
    }

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
                                        Encode_Frame_Type frameType,
                                        unsigned int *OutIndex)
{
    if ( (!CntElements)  || (!pVidEncDescr))
    {
        LOG_ERROR("%s [%d]:Invalid Parameters",__FUNCTION__,__LINE__);
        BCMOMX_DBG_ASSERT(false);
        return false;
    }

    if (frameType==Encode_Frame_Type_SPS)
    {
        for (unsigned int Index=0; Index < CntElements; Index++)
        {
            if (GET_NAL_UNIT_TYPE(pVidEncDescr->dataUnitType)==NAL_UNIT_TYPE_SPS)
            {
                *OutIndex=Index;
                return true;
            }
            pVidEncDescr++;
        }
    }
    else if (frameType==Encode_Frame_Type_PPS)
    {
        for (unsigned int Index=0; Index < CntElements; Index++)
        {
            if (GET_NAL_UNIT_TYPE(pVidEncDescr->dataUnitType)==NAL_UNIT_TYPE_PPS)
            {
                *OutIndex=Index;
                return true;
            }
            pVidEncDescr++;
        }
    }
    else
    {
        for (unsigned int Index=0; Index < CntElements; Index++)
        {
            if (pVidEncDescr->flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_FRAME_START)
            {
                *OutIndex=Index;
                return true;
            }
            pVidEncDescr++;
        }
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

unsigned int
OMXNexusVideoEncoder::RetriveFrameFromHardware()
{
    NEXUS_SimpleEncoderStatus EncSts;
    const NEXUS_VideoEncoderDescriptor *pFrames1=NULL, *pFrames2=NULL;
    size_t  SzFr1=0,SzFr2=0;
    unsigned int FramesRetrived=0;
    NEXUS_Error NxErrCode = NEXUS_SUCCESS;
    void *pData;
    bool frameStartSeen = false, SPSSent = false;
    bool codecConfigNotDelivered = false;
    Encode_Frame_Type frametype;

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
    NxErrCode = NEXUS_MemoryBlock_Lock(EncSts.video.bufferBlock, &pData);
    // TODO: Error handling on NxErrCode.
    EncSts.video.pBufferBase = pData;

    NxErrCode = NEXUS_SimpleEncoder_GetVideoBuffer(EncoderHandle,
                                                   &pFrames1,
                                                   &SzFr1,
                                                   &pFrames2,
                                                   &SzFr2);

    LOG_INFO("%s: pFrames1 = %p, SzFr1 = %d, pFrames2 = %p, SzFr2 = %d",__FUNCTION__, pFrames1, SzFr1, pFrames2, SzFr2);

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

    frametype = Encode_Frame_Type_Picture;

    if( false == GetFrameStart((NEXUS_VideoEncoderDescriptor *) pFrames1,
                                 SzFr1, frametype, &FrStartIndex) )
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
        LOG_RETRIVE_FRAME("%s: pCurrVidEncDescr[0].flags=%x, pCurrVidEncDescr[0].videoFlags = %x, pCurrVidEncDescr[0].length = %d, pCurrVidEncDescr[0].dataUnitType = %d",__FUNCTION__, pCurrVidEncDescr[0].flags, pCurrVidEncDescr[0].videoFlags, pCurrVidEncDescr[0].length, pCurrVidEncDescr[0].dataUnitType);

        if ((pCurrVidEncDescr[0].flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_FRAME_START) && (frameStartSeen==false))
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

            if (CodecConfigDone==false)
            {
                // We set CodecConfigDone on extracting SPS and PPS data from encoder bitstream. we use variable
                // codecConfigNotDelivered to collect and deliver codec config data seperately from picture data.
                codecConfigNotDelivered = true;

                // We have to send  SPS and PPS codec config data in a single buffer.
                // Assumption: PPS always follows SPS.

                // Get to the SPS/PPS frame.
                if (false==SPSSent)
                {
                    frametype = Encode_Frame_Type_SPS;
                }
                else
                {
                    frametype = Encode_Frame_Type_PPS;
                }
                if( false == GetFrameStart((NEXUS_VideoEncoderDescriptor *) pFrames1,
                                             SzFr1, frametype, &FrStartIndex) )
                {
                    //There is codec config At All In Data That Was Returned
                    LOG_ERROR("%s [%d]: Codec Config Not Found.",
                              __FUNCTION__,__LINE__);

                    //No Frame Start Found
                    LastErr= ErrStsRetFramesNoFrProcessed;
                    return 0;

                }

                LOG_INFO("Found %s codec config at index %d", (frametype==Encode_Frame_Type_SPS)?"SPS":"PPS",FrStartIndex);
                pCurrVidEncDescr = pFrames1 + FrStartIndex;
                LOG_RETRIVE_FRAME("pCurrVidEncDescr = %p, pFrames1 = %p", pCurrVidEncDescr, pFrames1);

                if (GET_NAL_UNIT_TYPE(pCurrVidEncDescr[0].dataUnitType)==NAL_UNIT_TYPE_SPS)
                {
                    LOG_INFO("Sending SPS");
                    SPSSent = true;
                    for (unsigned int cnt = 0; cnt < pCurrVidEncDescr[0].length; cnt+=4)
                        LOG_RETRIVE_FRAME("SPS:%x %x %x %x", *((char*) EncSts.video.pBufferBase+pCurrVidEncDescr[0].offset+cnt),
                        *((char*) EncSts.video.pBufferBase+pCurrVidEncDescr[0].offset+cnt+1),
                        *((char*) EncSts.video.pBufferBase+pCurrVidEncDescr[0].offset+cnt+2),
                        *((char*) EncSts.video.pBufferBase+pCurrVidEncDescr[0].offset+cnt+3));
                }
                else if (GET_NAL_UNIT_TYPE(pCurrVidEncDescr[0].dataUnitType)==NAL_UNIT_TYPE_PPS)
                {
                    LOG_INFO("Sending PPS");
                    CodecConfigDone = true;
                    // Assumption: Frame start flag is not set on PPS descriptor. Otherwise this descriptor will get added twice to pEmptyFr.
                    BCMOMX_DBG_ASSERT(!(pCurrVidEncDescr[0].flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_FRAME_START));
                    for (unsigned int cnt = 0; cnt < pCurrVidEncDescr[0].length; cnt+=4)
                        LOG_RETRIVE_FRAME("PPS:%x %x %x %x", *((char*) EncSts.video.pBufferBase+pCurrVidEncDescr[0].offset+cnt),
                        *((char*) EncSts.video.pBufferBase+pCurrVidEncDescr[0].offset+cnt+1),
                        *((char*) EncSts.video.pBufferBase+pCurrVidEncDescr[0].offset+cnt+2),
                        *((char*) EncSts.video.pBufferBase+pCurrVidEncDescr[0].offset+cnt+3));
                }
                else
                {
                    // Should never come here.
                    BCMOMX_DBG_ASSERT(false);
                }
                pEmptyFr->ClientFlags |= OMX_BUFFERFLAG_CODECCONFIG;
                // Codec config flag is expected to be set only once after start of encode.
                pEmptyFr->CombinedSz += pCurrVidEncDescr[0].length;
                pEmptyFr->BaseAddr = (unsigned int) EncSts.video.pBufferBase;
                pEmptyFr->FrameData->add((NEXUS_VideoEncoderDescriptor *) pCurrVidEncDescr);
            }

            // Combine the descriptors until we hit next frame start. From video encoder, we don't get frame end flag.
            // Don't return any data to client until we get codec config data.
            if (!((pCurrVidEncDescr[0].flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_FRAME_START) && (frameStartSeen==true)) &&
                (CodecConfigDone==true) && (codecConfigNotDelivered==false))
            {
                frameStartSeen = true;
                pEmptyFr->CombinedSz += pCurrVidEncDescr[0].length;
                pEmptyFr->BaseAddr = (unsigned int) EncSts.video.pBufferBase;
                pEmptyFr->FrameData->add((NEXUS_VideoEncoderDescriptor *) pCurrVidEncDescr);
            }
            else if (CodecConfigDone==true)
            {
                // TODO: Enable below check
//                if ((false == ShouldDiscardFrame(pEmptyFr))||(pEmptyFr->ClientFlags & OMX_BUFFERFLAG_CODECCONFIG))
                if (1)
                {
                    bool breakLoop = false;
                    InsertHeadList(&EncodedFrList, &pEmptyFr->ListEntry);
                    EncodedFrListLen++;
                    //PrintFrame(pEmptyFr);
                    LOG_INFO("%s[%d]: Frame Queued Encoded List Length:%d Empty List Length:%d",
                             __FUNCTION__,__LINE__,
                             EncodedFrListLen,EmptyFrListLen);
                    FramesRetrived++;
                    breakLoop = pEmptyFr->ClientFlags & OMX_BUFFERFLAG_CODECCONFIG;
                    pEmptyFr=NULL;

                    // Exit out of descriptor processing loop to deliver config data. We want to back off and process
                    // the frame again which contained config data to deliver that frame data completely.
                    if (breakLoop)
                        break;
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
OMXNexusVideoEncoder::imageBufferCallback()
{
    ALOGD("%s",__FUNCTION__);

    NEXUS_SurfaceHandle freeSurface=NULL;
    unsigned num_entries = 0;
    NEXUS_VideoImageInput_RecycleSurface(ImageInput, &freeSurface , 1, &num_entries);
    ALOGE("%s: num_entries = %d",__FUNCTION__, num_entries);
    if (!num_entries)
        return;

    BCMOMX_DBG_ASSERT(!IsListEmpty(&InputContextList));
    PLIST_ENTRY pDoneEntry = RemoveTailList(&InputContextList);
    BCMOMX_DBG_ASSERT(pDoneEntry);
    PNEXUS_VIDEO_ENCODER_INPUT_CONTEXT pDoneCnxt = CONTAINING_RECORD(pDoneEntry,NEXUS_VIDEO_ENCODER_INPUT_CONTEXT,ListEntry);

    BCMOMX_DBG_ASSERT(num_entries==1);
    LOG_INFO("%s:pDoneCnxt = %p, freeSurface = %p, pDoneCnxt->pNxSurface->handle = %p",__FUNCTION__, pDoneCnxt, freeSurface, pDoneCnxt->pNxSurface->handle);
    BCMOMX_DBG_ASSERT(freeSurface==pDoneCnxt->pNxSurface->handle);

    if (pDoneCnxt->DoneContext.pFnDoneCallBack)
    {
        //Fire The Call Back
        pDoneCnxt->DoneContext.pFnDoneCallBack(pDoneCnxt->DoneContext.Param1,
                pDoneCnxt->DoneContext.Param2,
                pDoneCnxt->DoneContext.Param3);
    }

    InsertHeadList(&SurfaceAvailList, &pDoneCnxt->pNxSurface->ListEntry);
    SurfaceAvailListLen++;
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

