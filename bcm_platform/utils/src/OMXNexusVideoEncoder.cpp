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

#include "OMXNexusVideoEncoder.h"
#include <hardware/gralloc.h>

#undef LOG_TAG
#define LOG_TAG "BCM_OMX_VIDEO_ENC"

//Path level Debug Messages
#define LOG_START_STOP_DBG      ALOGD
#define LOG_EOS_DBG             ALOGD
#define LOG_FLUSH_MSGS          ALOGD
#define LOG_CREATE_DESTROY      ALOGD
#define LOG_CONVERT
#define LOG_RETRIVE_FRAME
#define LOG_DUMP_DBG

//Path independent Debug Messages
#define LOG_WARNING             ALOGD
#define LOG_ERROR               ALOGD
#define LOG_INFO

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#define NAL_UNIT_TYPE_SPS       7
#define NAL_UNIT_TYPE_PPS       8

// Macro to extract NAL unit type from NEXUS_VideoEncoderDescriptor.dataUnitTye
// NAL unit type is the LSB 5 bits value
#define GET_NAL_UNIT_TYPE(x)    (x & 0x1F)
static void imageBufferCallbackDispatcher(void *context, int param)
{
    UNUSED(param);

    OMXNexusVideoEncoder    *pEncoder = (OMXNexusVideoEncoder *)context;
    pEncoder->imageBufferCallback();
}

// Set enable_es_dump to "true" to dump encoded ES frames.
static bool enable_es_dump = false;

// Set dump_once to 'false' to dump first pair of input/output frames from frame conversion.
static bool dump_once = true;
bool OMXNexusVideoEncoder::convertOMXPixelFormatToCrYCbY(PNEXUS_VIDEO_ENCODER_INPUT_CONTEXT pNxInputContext)
{
    NEXUS_SurfaceHandle hDst = pNxInputContext->pNxSurface->handle;
    uint8_t *pSrc = (uint8_t *)pNxInputContext->bufPtr;
    unsigned int width = pNxInputContext->width;
    unsigned int height = pNxInputContext->height;
    OMX_COLOR_FORMATTYPE colorFormat = pNxInputContext->colorFormat;
    bool bMetaData = (colorFormat == OMX_COLOR_FormatAndroidOpaque);

    bool bRet = true;
    bool bSwapAlpha = false;
    uint8_t *pSrcBuf = NULL;
    uint8_t *pDstBuf = NULL;
    NEXUS_SurfaceMemory mem;
    gralloc_module_t *pGralloc = NULL;
    buffer_handle_t hBuffer = NULL;
    private_handle_t *pBuffer = NULL;
    int res;

    const char *pDumpSrcName = NULL, *pDumpDstName = NULL;
    const uint8_t *pDumpSrcBuf = NULL, *pDumpDstBuf = NULL;
    int32_t dumpSrcSize = 0, dumpDstSize = 0;

    /* Currently we support only the following color formats */
    BCMOMX_DBG_ASSERT(colorFormat == OMX_COLOR_FormatYUV420Planar ||
                      colorFormat == OMX_COLOR_FormatAndroidOpaque);

    if (bMetaData)
    {
        unsigned int expectedBufSize = sizeof(buffer_handle_t) + 4;
        if (pNxInputContext->bufSize < expectedBufSize)
        {
            ALOGE("%s: Invalid metadata - bufSize=%u expected=%d",
                  __FUNCTION__,
                  pNxInputContext->bufSize,
                  expectedBufSize);
            bRet = false;
            goto EXIT;
        }

        if (mpGrallocFd == NULL &&
            hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t **)&mpGrallocFd))
        {
            ALOGE("%s: Error opening gralloc module", __FUNCTION__);
            bRet = false;
            goto EXIT;
        }
        BCMOMX_DBG_ASSERT(mpGrallocFd != NULL);

        pGralloc = (gralloc_module_t *)mpGrallocFd;
        hBuffer = *(buffer_handle_t *)(pSrc + 4);
        pBuffer = (private_handle_t *)hBuffer;

        res = pGralloc->lock(pGralloc,
                             hBuffer,
                             GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_NEVER,
                             0,
                             0,
                             width,
                             height,
                             (void **)&pSrcBuf);
        if (res != 0)
        {
            ALOGE("%s: Error locking gralloc buffer - err=%d", __FUNCTION__, res);
            bRet = false;
            goto EXIT;
        }

        LOG_CONVERT("%s: Surface - color format = %d height=%d width=%d",
                    __FUNCTION__,
                    pBuffer->format,
                    height,
                    width);
        switch (pBuffer->format)
        {
            case HAL_PIXEL_FORMAT_RGBA_8888:
            case HAL_PIXEL_FORMAT_RGBX_8888:
            {
                colorFormat = OMX_COLOR_Format32bitARGB8888;
                bSwapAlpha = true;
            }
            break;
            case HAL_PIXEL_FORMAT_YV12:
            {
                colorFormat = OMX_COLOR_FormatYUV420Planar;
            }
            break;
            default:
            {
                ALOGE("%s: Unsupported android opaque format %d", __FUNCTION__, pBuffer->format);
                bRet = false;
                goto EXIT;
            }
            break;
        }

    }
    else
    {
        pSrcBuf = pSrc;
    }

    NEXUS_Surface_GetMemory(hDst, &mem);
    BCMOMX_DBG_ASSERT(mem.buffer);
    pDstBuf = (uint8_t *)mem.buffer;

    if (enable_es_dump || !dump_once)
    {
        pDumpSrcBuf = pSrcBuf;
    }

    switch (colorFormat)
    {
        case OMX_COLOR_FormatYUV420Planar:
        {
            int32_t totalPix = width * height;

            convertYuvToCrYCbY(pSrcBuf, pDstBuf, width, height);

            if (!dump_once)
            {
                pDumpSrcName = "yuv420.dat";
                pDumpDstName = "yuv420_crycby.dat";
                pDumpDstBuf = pDstBuf;
                dumpSrcSize = totalPix + totalPix / 2;
                dumpDstSize = totalPix << 1;
            }

            if (enable_es_dump)
            {
                dumpSrcSize = totalPix + totalPix / 2;
            }
        }
        break;

        case OMX_COLOR_Format32bitARGB8888:
        {
#if 0 /* TODO: Enable HW conversion later */
            int32_t nSrcBufSize = (width * height) * 4;
            int32_t nDstBufSize = nSrcBufSize / 2;
            NEXUS_Error rc;

            /* Create source surface for conversion (if needed)  */
            if (mhSrcSurface == NULL)
            {
                NEXUS_SurfaceCreateSettings surfaceCfg;
                NEXUS_Surface_GetDefaultCreateSettings(&surfaceCfg);
                surfaceCfg.pixelFormat = bSwapAlpha ? NEXUS_PixelFormat_eR8_G8_B8_X8 : NEXUS_PixelFormat_eX8_R8_G8_B8;
                surfaceCfg.width = width;
                surfaceCfg.height = height;
                mhSrcSurface = NEXUS_Surface_Create(&surfaceCfg);
                BCMOMX_DBG_ASSERT(mhSrcSurface);
            }

            /* Open graphics2d module (if needed) */
            if (mhGfx == NULL)
            {
                NEXUS_Graphics2DSettings gfxSettings;
                mhGfx = NEXUS_Graphics2D_Open(0, NULL);
                BCMOMX_DBG_ASSERT(mhGfx);

                NEXUS_Graphics2D_GetSettings(mhGfx, &gfxSettings);
                gfxSettings.pollingCheckpoint = true;
                rc = NEXUS_Graphics2D_SetSettings(mhGfx, &gfxSettings);
                BCMOMX_DBG_ASSERT(!rc);
            }

            /* Fill source surface with gralloc buffer */
            NEXUS_Surface_GetMemory(mhSrcSurface, &mem);
            BCMOMX_DBG_ASSERT(mem.buffer);
            uint8_t *pSrcSurface = (uint8_t *)mem.buffer;

            memcpy(pSrcSurface, pSrcBuf, nSrcBufSize);
            NEXUS_Surface_Flush(mhSrcSurface);

            /* Start conversion */
            NEXUS_Graphics2DBlitSettings blitSettings;
            NEXUS_Graphics2D_GetDefaultBlitSettings(&blitSettings);
            blitSettings.source.surface = mhSrcSurface;
            blitSettings.output.surface = hDst;
            blitSettings.colorOp = NEXUS_BlitColorOp_eCopySource;
            blitSettings.alphaOp = NEXUS_BlitAlphaOp_eCopySource;
            rc = NEXUS_Graphics2D_Blit(mhGfx, &blitSettings);
            BDBG_ASSERT(!rc);

            if (!dump_once)
            {
                NEXUS_Surface_GetMemory(hDst, &mem);
                BCMOMX_DBG_ASSERT(mem.buffer);

                pDumpSrcName = "rgba8888.dat";
                pDumpDstName = "crycby.dat";
                pDumpDstBuf = (uint8_t *)mem.buffer;
                dumpSrcSize = (height * width) * 4;
                dumpDstSize = (height * width) * 2;
            }

            if (enable_es_dump)
            {
                dumpSrcSize = height * width * 4;
            }
#endif
            /* Convert to YUV420 first using BT.601 */
            int32_t totalPix = width * height;
            uint8_t *pYuvBuf = (uint8_t *)malloc(totalPix + totalPix / 2);
            BCMOMX_DBG_ASSERT(pYuvBuf);
            memset(pYuvBuf, 0, totalPix + totalPix / 2);

            uint8_t *dstY = pYuvBuf;
            uint8_t *dstU = dstY + totalPix;
            uint8_t *dstV = dstU + (width >> 1) * (height >> 1);
            uint8_t *srcRGB = pSrcBuf;

            const size_t redOffset   = bSwapAlpha ? 0 : 1;
            const size_t greenOffset = bSwapAlpha ? 1 : 2;
            const size_t blueOffset  = bSwapAlpha ? 2 : 3;

            for (size_t y = 0; y < height; ++y)
            {
                for (size_t x = 0; x < width; ++x)
                {
                    unsigned red = srcRGB[redOffset];
                    unsigned green = srcRGB[greenOffset];
                    unsigned blue = srcRGB[blueOffset];

                    unsigned luma = ((red * 66 + green * 129 + blue * 25) >> 8) + 16;
                    dstY[x] = luma;

                    if ((x & 1) == 0 && (y & 1) == 0)
                    {
                        unsigned U = ((-red * 38 - green * 74 + blue * 112) >> 8) + 128;
                        unsigned V = ((red * 112 - green * 94 - blue * 18) >> 8) + 128;

                        dstU[x >> 1] = U;
                        dstV[x >> 1] = V;
                    }
                    srcRGB += 4;
                }

                if ((y & 1) == 0)
                {
                    dstU += width >> 1;
                    dstV += width >> 1;
                }

                //srcRGB += width - 4 * width;
                dstY += width;
            }

            convertYuvToCrYCbY(pYuvBuf, pDstBuf, width, height);

            free(pYuvBuf);

            if (!dump_once)
            {
                pDumpSrcName = "rgb32.dat";
                pDumpDstName = "rgb32_crycby.dat";
                pDumpDstBuf = pDstBuf;
                dumpSrcSize = totalPix << 2;
                dumpDstSize = totalPix << 1;
            }

            if (enable_es_dump)
            {
                dumpSrcSize = totalPix << 2;
            }
        }
        break;

        default:
        {
            ALOGE("%s: Unsupported color format %d", __FUNCTION__, colorFormat);
            bRet = false;
            goto EXIT;
        }
        break;
    }

EXIT:
    if (bMetaData)
    {
        if (pGralloc && hBuffer && pSrcBuf)
        {
            res = pGralloc->unlock(pGralloc, hBuffer);
            if (res != 0)
            {
                ALOGE("%s: Failed to unlock gralloc buffer %d", __FUNCTION__, res);
                bRet = false;
            }
        }
    }

    NEXUS_Surface_Flush(hDst);

    if (!dump_once && pDumpSrcName && pDumpDstName && pDumpSrcBuf && pDumpDstBuf && dumpSrcSize && dumpDstSize)
    {
        DumpData d0(pDumpSrcName);
        d0.WriteData((void *)pDumpSrcBuf, dumpSrcSize);

        DumpData d1(pDumpDstName);
        d1.WriteData((void *)pDumpDstBuf, dumpDstSize);
        dump_once = true;
    }

    if (enable_es_dump && pdumpESsrc)
    {
        LOG_DUMP_DBG("%s:Dumping ES raw data. Buffer Address = %p, Buffer Size = %d",
                     __FUNCTION__,
                     pDumpSrcBuf,
                     dumpSrcSize);

        pdumpESsrc->WriteData((void *)pDumpSrcBuf , dumpSrcSize);
    }

    return bRet;
}

void OMXNexusVideoEncoder::convertYuvToCrYCbY(const uint8_t *pSrcBuf, uint8_t *pDstBuf, unsigned int width, unsigned height)
{
    int32_t inYsize = width * height;
    const uint8_t *iny = pSrcBuf;
    const uint8_t *incb = pSrcBuf + inYsize;
    const uint8_t *incr = pSrcBuf + inYsize + (inYsize >> 2);

    for (uint32_t i = 0; i < height/2; i++)
    {
        for (uint32_t j = 0; j < width/2; j++)
        {
            uint8_t y0, y1, cr, cb;

            y0 = *(iny + 2 * i * width + 2*j);
            cb = *(incb + i * width/2 + j);
            y1 = *(iny + 2 * i * width + 2*j + 1);
            cr = *(incr + i * width/2 + j);

            *((uint32_t *)pDstBuf + 2 * i * width/2 + j) = cr << 24 | y1 << 16 | cb << 8 | y0;

            y0 = *(iny + (2*i+1) * width + 2*j);
            cb = *(incb + i * width/2 + j);
            y1 = *(iny + (2*i+1) * width + 2*j + 1);
            cr = *(incr + i * width/2 + j);

            *((uint32_t *)pDstBuf + (2*i+1)* width/2 + j) = cr << 24 | y1 << 16 | cb << 8 | y0;
        }
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

NEXUS_VideoCodecProfile OMXNexusVideoEncoder::convertOMXProfileTypetoNexus(OMX_VIDEO_AVCPROFILETYPE profile)
{
    for (unsigned int i = 0; i < sizeof(ProfileMapTable)/sizeof(ProfileMapTable[0]); i++)
    {
        if (ProfileMapTable[i].omxProfile==profile)
            return ProfileMapTable[i].nexusProfile;
    }

    return NEXUS_VideoCodecProfile_eBaseline;
}

NEXUS_VideoCodecLevel OMXNexusVideoEncoder::convertOMXLevelTypetoNexus(OMX_VIDEO_AVCLEVELTYPE level)
{
    for(unsigned int i = 0; i < sizeof(LevelMapTable)/sizeof(LevelMapTable[0]); i++)
    {
        if(LevelMapTable[i].omxLevel==level)
            return LevelMapTable[i].nexusLevel;
    }

    return NEXUS_VideoCodecLevel_e31;
}

OMXNexusVideoEncoder::OMXNexusVideoEncoder(const char *callerName, int numInBuf)
    : EncoderHandle(NULL),
    NumNxSurfaces(numInBuf),
    EmptyFrListLen(0),EncodedFrListLen(0),
    SurfaceAvailListLen(0),
    LastErr(ErrStsSuccess),
    EncoderStarted(false),
    pdumpESsrc(NULL),
    pdumpESdst(NULL),
    mpGrallocFd(NULL),
    mhSrcSurface(NULL),
    mhGfx(NULL)
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
    NxIPCClient = NexusIPCClientFactory::getClient(callerName);

    BKNI_Memset(&config, 0, sizeof(config));
    BKNI_Snprintf(config.name.string,sizeof(config.name.string),callerName);

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
    connectSettings.simpleEncoder[0].nonRealTime = true;
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
    {
        pdumpESsrc = new DumpData("videnc_es_src.dat");
        pdumpESdst = new DumpData("videnc_es_dst.dat");
    }

    LastErr = ErrStsSuccess;
    LOG_CREATE_DESTROY("%s: OMXNexusVideoEncoder Created!!",__FUNCTION__);

}

OMXNexusVideoEncoder::~OMXNexusVideoEncoder()
{
    LOG_CREATE_DESTROY("%s: ",__FUNCTION__);
    unsigned int NumEntriesFreed=0;

    Mutex::Autolock lock(mListLock);

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

    if (DecoderHandle)
    {
        LOG_CREATE_DESTROY("%s: Releasing Video Decoder",__FUNCTION__);
        NEXUS_SimpleVideoDecoder_Release(DecoderHandle);
        DecoderHandle = NULL;
    }

    if (StcChannel)
    {
        LOG_CREATE_DESTROY("%s: Destroying STC channel",__FUNCTION__);
        NEXUS_SimpleStcChannel_Destroy(StcChannel);
        StcChannel = NULL;
    }

    if (mhSrcSurface)
    {
        NEXUS_Surface_Destroy(mhSrcSurface);
        mhSrcSurface = NULL;
    }

    if (mhGfx)
    {
        NEXUS_Graphics2D_Close(mhGfx);
        mhGfx = NULL;
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

    if (pdumpESsrc)
    {
        delete(pdumpESsrc);
    }

    if (pdumpESdst)
    {
        delete(pdumpESdst);
    }

    if (NumEntriesFreed != VIDEO_ENCODE_DEPTH)
    {
        ALOGW("%s: Freeing only %u frames in shutdown", __FUNCTION__, NumEntriesFreed);
    }

    LOG_CREATE_DESTROY("%s: Deleting The NxIPCClient",__FUNCTION__);
    delete NxIPCClient;
}


bool OMXNexusVideoEncoder::StartInput(PVIDEO_ENCODER_START_PARAMS pStartParams)
{
    Mutex::Autolock lock(mListLock);

    return StartInput_l(pStartParams);
}

bool OMXNexusVideoEncoder::StartInput_l(PVIDEO_ENCODER_START_PARAMS pStartParams)
{
    NEXUS_Error errCode;
    NEXUS_VideoImageInputSettings imageInputSetting;
    NEXUS_SurfaceCreateSettings surfaceCfg;
    NEXUS_ClientConfiguration clientConfig;
    NEXUS_VideoImageInputStatus imageInputStatus;
    NEXUS_SimpleVideoDecoderStartSettings DecoderStartSettings;

    // TODO: Error Handling
    LOG_INFO("%s: Using NRT mode...",__FUNCTION__);
    NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(&DecoderStartSettings);
    DecoderStartSettings.lowDelayImageInput = false;    /* Low delay mode bypasses xdm display management */

    ImageInput = NEXUS_SimpleVideoDecoder_StartImageInput(DecoderHandle, &DecoderStartSettings);
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
    for (int i=0;i<NEXUS_MAX_HEAPS;i++) {
        NEXUS_MemoryStatus s;
        if (!clientConfig.heap[i] || NEXUS_Heap_GetStatus(clientConfig.heap[i], &s)) continue;
        if (s.memcIndex == imageInputStatus.memcIndex && (s.memoryType & NEXUS_MemoryType_eApplication) && s.largestFreeBlock >= 960*1080*2) {
            surfaceCfg.heap = clientConfig.heap[i];
            LOG_INFO("found heap[%d] on MEMC%d for VideoImageInput", i, s.memcIndex);
            break;
        }
    }
    if (!surfaceCfg.heap) {
        LOG_ERROR("no heap found. RTS failure likely.");
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
        InsertHeadList(&SurfaceAvailList, &pSurface[i]->ListEntry);
        SurfaceAvailListLen++;
    }
    LOG_CREATE_DESTROY("%s: Nexus surfaces created ***",__FUNCTION__);

    NEXUS_VideoImageInput_GetSettings(ImageInput, &imageInputSetting);
    imageInputSetting.imageCallback.callback = imageBufferCallbackDispatcher;
    imageInputSetting.imageCallback.context  = static_cast <void *>(this);
    NEXUS_VideoImageInput_SetSettings(ImageInput, &imageInputSetting);

    LastErr = ErrStsSuccess;
    return true;
}

bool
OMXNexusVideoEncoder::StartOutput(PVIDEO_ENCODER_START_PARAMS pStartParams)
{
    NEXUS_Error errCode;
    NEXUS_SimpleEncoderStartSettings EncoderStartSettings;
    NEXUS_SimpleEncoderSettings encoderSettings;

    // TODO: Error Handling
    NEXUS_SimpleEncoder_GetSettings(EncoderHandle, &encoderSettings);
    encoderSettings.video.width = pStartParams->width;
    encoderSettings.video.height = pStartParams->height;
    //TODO: Remove hardcoding for refresh rate
    encoderSettings.video.refreshRate=60000;

    encoderSettings.videoEncoder.frameRate = pStartParams->frameRate;
#if 0 /* TODO: VBR does not work. Only CBR with a bit rate of 2.5Mbps works. */
    encoderSettings.videoEncoder.bitrateMax = pStartParams->bitRateParams.nTargetBitrate;
    switch (pStartParams->bitRateParams.eControlRate)
    {
        case OMX_Video_ControlRateVariable:
        {
            encoderSettings.videoEncoder.bitrateTarget = pStartParams->bitRateParams.nTargetBitrate;
            encoderSettings.videoEncoder.variableFrameRate = false;
        }
        break;

        case OMX_Video_ControlRateConstant:
        {
            encoderSettings.videoEncoder.bitrateTarget = 0;
            encoderSettings.videoEncoder.variableFrameRate = false;
        }
        break;

        case OMX_Video_ControlRateVariableSkipFrames:
        {
            encoderSettings.videoEncoder.bitrateTarget = pStartParams->bitRateParams.nTargetBitrate;
            encoderSettings.videoEncoder.variableFrameRate = true;
        }
        break;

        case OMX_Video_ControlRateConstantSkipFrames:
        {
            encoderSettings.videoEncoder.bitrateTarget = 0;
            encoderSettings.videoEncoder.variableFrameRate = true;
        }
        break;

        case OMX_Video_ControlRateDisable:
        default:
        {
            LOG_WARNING("%s: No rate control", __FUNCTION__);
            encoderSettings.videoEncoder.bitrateTarget = 0;
            encoderSettings.videoEncoder.variableFrameRate = false;
        }
        break;
    }
#endif
    LOG_WARNING("%s: FrameRate=%d BitRateMax=%d BitRateTarget=%d bVarFrameRate=%d",
                __FUNCTION__,
                encoderSettings.videoEncoder.frameRate,
                encoderSettings.videoEncoder.bitrateMax,
                encoderSettings.videoEncoder.bitrateTarget,
                encoderSettings.videoEncoder.variableFrameRate);

    NEXUS_SimpleEncoder_SetSettings(EncoderHandle, &encoderSettings);
    LOG_INFO("Encoder setup done");

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
    EncoderStartSettings.output.video.settings.nonRealTime = true;
    EncoderStartSettings.output.video.settings.interlaced = false;

    LOG_WARNING("%s: Profile = %d, Level = %d, width = %d, height = %d",
                __FUNCTION__,
                EncoderStartSettings.output.video.settings.profile,
                EncoderStartSettings.output.video.settings.level,
                pStartParams->width,
                pStartParams->height);

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
    }
    else
    {
        LOG_START_STOP_DBG("%s[%d]: SimpleEncoder Started Successfully [Err:%d]!!",
            __FUNCTION__, __LINE__, errCode);
    }

    //Reset codec config done flag.
    CodecConfigDone = false;

    LastErr = ErrStsSuccess;
    return true;
}

bool
OMXNexusVideoEncoder::StartEncoder(PVIDEO_ENCODER_START_PARAMS pStartParams)
{
    Mutex::Autolock lock(mListLock);
    bool ret = true;

    if (EncoderStarted)
    {
        LOG_START_STOP_DBG("%s[%d]: Encoder Already Started",
                           __FUNCTION__, __LINE__);

        LastErr = ErrStsStartEncoFailed;
        return false;
    }

    ret = StartOutput(pStartParams);
    if (ret==false)
        return ret;

    ret = StartInput_l(pStartParams);
    if (ret==false)
        return ret;

    LastErr = ErrStsSuccess;
    EncoderStartParams = *pStartParams;
    EncoderStarted = true;
    return true;
}

void OMXNexusVideoEncoder::StopInput()
{
    Mutex::Autolock lock(mListLock);
    StopInput_l();
}

void OMXNexusVideoEncoder::StopInput_l()
{
    NEXUS_SimpleVideoDecoder_StopImageInput(DecoderHandle);
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
}

void
OMXNexusVideoEncoder::StopOutput()
{
    NEXUS_SimpleEncoder_Stop(EncoderHandle);
}

void
OMXNexusVideoEncoder::StopEncoder()
{
    Mutex::Autolock lock(mListLock);
    if (EncoderStarted)
    {

        StopInput_l();
        StopOutput();

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
OMXNexusVideoEncoder::FlushInput()
{
    Mutex::Autolock lock(mListLock);
    bool ret = true;
    StopInput_l();
    while(!IsListEmpty(&InputContextList))
    {
        PNEXUS_VIDEO_ENCODER_INPUT_CONTEXT  pContext = NULL;
        PLIST_ENTRY ThisEntry =  RemoveTailList(&InputContextList);
        BCMOMX_DBG_ASSERT(ThisEntry);
        pContext = CONTAINING_RECORD(ThisEntry,NEXUS_VIDEO_ENCODER_INPUT_CONTEXT,ListEntry);
        BCMOMX_DBG_ASSERT(pContext);

        if (pContext->DoneContext.pFnDoneCallBack)
        {
            //Fire The Call Back
            pContext->DoneContext.pFnDoneCallBack(pContext->DoneContext.Param1,
                    pContext->DoneContext.Param2,
                    pContext->DoneContext.Param3);

            pContext->DoneContext.pFnDoneCallBack = NULL;
        }
    }

    ret = StartInput_l(&EncoderStartParams);
    return ret;
}

bool
OMXNexusVideoEncoder::FlushOutput()
{
    Mutex::Autolock lock(mListLock);
    bool ret = true;
    StopOutput();
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

    ret = StartOutput(&EncoderStartParams);
    return ret;
}

bool
OMXNexusVideoEncoder::EncodeFrame(PNEXUS_VIDEO_ENCODER_INPUT_CONTEXT pNxInputContext)
{
    bool bRet = true;
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


    NEXUS_VideoImageInput_GetDefaultSurfaceSettings(&surfSettings);
    if (pNxInputContext->flags & OMX_BUFFERFLAG_EOS)
    {
        NEXUS_Error err;
        surfSettings.endOfStream = true;
        err = NEXUS_VideoImageInput_PushSurface(ImageInput, NULL, &surfSettings);
        if (err)
        {
            LOG_ERROR("%s: Error sending EOS to HW. err = %d",__FUNCTION__, err);
            BCMOMX_DBG_ASSERT(!IsListEmpty(&InputContextList));
            PLIST_ENTRY pDoneEntry = RemoveTailList(&InputContextList);
            BCMOMX_DBG_ASSERT(pDoneEntry);
            InsertHeadList(&SurfaceAvailList, &pNxSurface->ListEntry);
            SurfaceAvailListLen++;
            bRet = false;
        }
    }
    else
    {
        bRet = convertOMXPixelFormatToCrYCbY(pNxInputContext);
        if (!bRet)
        {
            LOG_ERROR("%s: Error converting color format", __FUNCTION__);

        }

        NEXUS_Error err;
        surfSettings.pts = pNxInputContext->uSecTS;
        LOG_ERROR("%s:PTS = %d",__FUNCTION__, pNxInputContext->uSecTS);
        surfSettings.ptsValid = true;
        // TODO: Remove frame rate hardcoding.
        surfSettings.frameRate = NEXUS_VideoFrameRate_e15;
        err = NEXUS_VideoImageInput_PushSurface(ImageInput, pNxInputContext->pNxSurface->handle, &surfSettings);
        if (err)
        {
            LOG_ERROR("%s: Error Push Surface. err = %d", err);
            BCMOMX_DBG_ASSERT(!IsListEmpty(&InputContextList));
            PLIST_ENTRY pDoneEntry = RemoveTailList(&InputContextList);
            BCMOMX_DBG_ASSERT(pDoneEntry);
            InsertHeadList(&SurfaceAvailList, &pNxSurface->ListEntry);
            SurfaceAvailListLen++;
            bRet = false;
        }
    }

    return bRet;
}

bool
OMXNexusVideoEncoder::GetEncodedFrame(PDELIVER_ENCODED_FRAME pDeliverFr)
{
    Mutex::Autolock lock(mListLock);

    PrintVideoEncoderStatus();
    RetrieveFrameFromHardware_l();

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
            ReturnEncodedFrameSynchronized_l(pNxVidEncFr);
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
            ReturnEncodedFrameSynchronized_l(pNxVidEncFr);
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
            ReturnEncodedFrameSynchronized_l(pNxVidEncFr);
            LastErr = ErrStsNexusReturnedErr;
            return false;
        }

        pDeliverFr->OutFlags = pNxVidEncFr->ClientFlags;
        pDeliverFr->usTimeStamp = pNxVidEncFr->usTimeStampOriginal;
        LOG_INFO("%s:OUT PTS = %d",__FUNCTION__, pDeliverFr->usTimeStamp);

        pDeliverFr->SzFilled = SizeToCopy;
        ReturnEncodedFrameSynchronized_l(pNxVidEncFr);

        if (enable_es_dump)
        {
            LOG_DUMP_DBG("%s:Dumping ES data. Buffer Address = %p, Buffer Size = %d",
                         __FUNCTION__,
                         pDeliverFr->pVideoDataBuff,
                         pDeliverFr->SzVidDataBuff);

            pdumpESdst->WriteData((void *) pDeliverFr->pVideoDataBuff, pDeliverFr->SzVidDataBuff);
        }

        LastErr = ErrStsSuccess;
        return true;
    }

    return false;
}

bool OMXNexusVideoEncoder::ReturnEncodedFrameSynchronized_l(PNEXUS_ENCODED_VIDEO_FRAME pReturnFr)
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
            if(pVidEncDescr->flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_EOS)
            {
                LOG_INFO("%s: Found EOS on output.",__FUNCTION__);
            }
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

unsigned int OMXNexusVideoEncoder::RetrieveFrameFromHardware_l()
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
    bool frameComplete = false;

    if (!IsEncoderStarted())
    {
        LOG_ERROR("%s[%d]: Encoder Is Not Started",__FUNCTION__,__LINE__);
        LastErr= ErrStsEncoNotStarted;
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
        LOG_INFO("%s[%d]: No Encoded Frames From Encoder Sts:%d Sz1:%d Sz2:%d",
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
        if(pCurrVidEncDescr[0].flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_EOS)
        {
            LOG_INFO("%s: Received EOS on output. Flags = %x",__FUNCTION__, pCurrVidEncDescr[0].flags);
        }
//        LOG_ERROR("%s: Flags = %x",__FUNCTION__, pCurrVidEncDescr[0].flags);

//        if (((pCurrVidEncDescr[0].flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_FRAME_START) && (frameStartSeen==false))||
//            (pCurrVidEncDescr[0].flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_EOS))
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

            if(pCurrVidEncDescr[0].flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_EOS)
            {
                LOG_INFO("%s: Received EOS on output.",__FUNCTION__);
                pEmptyFr->ClientFlags |= OMX_BUFFERFLAG_EOS;
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
                if ( pCurrVidEncDescr[0].flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_ORIGINALPTS_VALID )
                {
                    pEmptyFr->usTimeStampOriginal = pCurrVidEncDescr[0].originalPts;
                }

                if ( pCurrVidEncDescr[0].flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_PTS_VALID )
                {
                    pEmptyFr->usTimeStampIntepolated = pCurrVidEncDescr[0].pts;
                }
            }
            else if (CodecConfigDone==true)
            {
                frameComplete = true;
            }
            if (pCurrVidEncDescr[0].flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_EOS)
            {
                frameComplete = true;
            }
            if (frameComplete)
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
                frameComplete = false;
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
    //ALOGD("%s",__FUNCTION__);
    Mutex::Autolock lock(mListLock);

    NEXUS_SurfaceHandle freeSurface=NULL;
    unsigned num_entries = 0;
    NEXUS_VideoImageInput_RecycleSurface(ImageInput, &freeSurface , 1, &num_entries);
    //ALOGE("%s: num_entries = %d",__FUNCTION__, num_entries);
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

        pDoneCnxt->DoneContext.pFnDoneCallBack = NULL;
    }

    InsertHeadList(&SurfaceAvailList, &pDoneCnxt->pNxSurface->ListEntry);
    SurfaceAvailListLen++;
}


ErrorStatus
OMXNexusVideoEncoder::GetLastError()
{
    return LastErr;
}
