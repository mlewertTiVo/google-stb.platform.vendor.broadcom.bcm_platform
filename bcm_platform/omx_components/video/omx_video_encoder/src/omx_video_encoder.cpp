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

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "OMX_Component.h"
#include "OMX_BCM_Common.h"

#include <utils/Log.h>
#include <utils/List.h>

//Move below inclusion to OMXNexusEncoder.h
#include "BcmDebug.h"


#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

/* Supported AVC profile Level table */
typedef struct _OMX_PROFILE_LEVEL_
{
    OMX_U32 mProfile;
    OMX_U32 mLevel;
} OMX_PROFILE_LEVEL, *POMX_PROFILE_LEVEL;

static const OMX_PROFILE_LEVEL ProfileLevels[] =
{
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel1  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel1b },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel11 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel12 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel13 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel2  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel21 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel22 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel3  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel31 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel32 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel4  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel41 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel42 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel5  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel51 },
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel31 },
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel4 }
};

/* Supported input port color formats */
#define NUM_SUPPORTED_INPUT_FORMAT (sizeof(PortInputFormats) / sizeof(PortInputFormats[0]))
static const OMX_COLOR_FORMATTYPE PortInputFormats[] =
{
    OMX_COLOR_FormatYUV420Planar,
    OMX_COLOR_FormatAndroidOpaque
};

typedef struct _CODEC_TO_MIME_MAP_
{
    OMX_VIDEO_CODINGTYPE    CodingType;
    OMX_STRING              Mime;
}CODEC_TO_MIME_MAP,*PCODEC_TO_MIME_MAP;

static const CODEC_TO_MIME_MAP CodecToMIME[] =
{
    {OMX_VIDEO_CodingUnused,            NULL                                },
    {OMX_VIDEO_CodingAutoDetect,        NULL                                },
    {OMX_VIDEO_CodingMPEG2,             (OMX_STRING)("video/mpeg2")         },
    {OMX_VIDEO_CodingH263,              (OMX_STRING)("video/3gpp")          },
    {OMX_VIDEO_CodingMPEG4,             (OMX_STRING)("video/mp4v-es")       },
    {OMX_VIDEO_CodingWMV,               (OMX_STRING)("video/wmv3")          },
    {OMX_VIDEO_CodingRV,                (OMX_STRING)("video/real")          },
    {OMX_VIDEO_CodingAVC,               (OMX_STRING)("video/avc")           },
    {OMX_VIDEO_CodingMJPEG,             (OMX_STRING)("video/mjpeg")         },
    {OMX_VIDEO_CodingVP8,               (OMX_STRING)("video/x-vnd.on2.vp8") },
    {OMX_VIDEO_CodingVP9,               (OMX_STRING)("video/x-vnd.on2.vp9") },
#ifdef OMX_EXTEND_CODECS_SUPPORT
    {OMX_VIDEO_CodingVC1,               (OMX_STRING)("video/wvc1")          },
    {OMX_VIDEO_CodingSPARK,             (OMX_STRING)("video/spark")         },
    {OMX_VIDEO_CodingDIVX311,           (OMX_STRING)("video/divx")          },
    {OMX_VIDEO_CodingH265,              (OMX_STRING)("video/hevc")          },
#endif
};

typedef struct _OMX_TO_NEXUS_FRAMERATE_MAP_
{
    OMX_U32     OmxFramerate;
    NEXUS_VideoFrameRate    NxFrameRate;
} OMX_TO_NEXUS_FRAMERATE_MAP, *POMX_TO_NEXUS_FRAMERATE_MAP;

static const OMX_TO_NEXUS_FRAMERATE_MAP frameRateTypeConversion[] =
{
        {983040,    NEXUS_VideoFrameRate_e15}
 //TODO: Populate other values
};

// Debug Message Macros
#define CONFIG_LOG_MSG                  ALOGD

#define OMX_IndexStoreMetaDataInBuffers                 0x7F000001

extern "C"
{

OMX_ERRORTYPE OMX_ComponentInit(OMX_HANDLETYPE hComponent);

OMX_ERRORTYPE OMX_VENC_SendCommand(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_COMMANDTYPE eCmd,
    OMX_IN OMX_U32 nParam, OMX_IN OMX_PTR pCmdData);

OMX_ERRORTYPE OMX_VENC_GetExtensionIndex(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_STRING cParameterName, OMX_OUT OMX_INDEXTYPE * pIndexType);


OMX_ERRORTYPE OMX_VENC_GetParameter(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pParamStruct);

OMX_ERRORTYPE OMX_VENC_SetParameter(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pParamStruct);

OMX_ERRORTYPE OMX_VENC_EmptyThisBuffer(OMX_IN  OMX_HANDLETYPE hComponent,
                OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHdr);

OMX_ERRORTYPE OMX_VENC_UseBuffer(OMX_IN OMX_HANDLETYPE hComponent,
              OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
              OMX_IN OMX_U32 nPortIndex,
              OMX_IN OMX_PTR pAppPrivate,
              OMX_IN OMX_U32 nSizeBytes,
              OMX_IN OMX_U8* pBuffer);

#if 0
OMX_ERRORTYPE OMX_VENC_UseANativeWindowBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                OMX_INOUT OMX_BUFFERHEADERTYPE **bufferHeader,
                OMX_IN OMX_U32 nPortIndex,
                OMX_IN OMX_PTR pAppPrivate,
                OMX_IN OMX_U32 nSizeBytes,
                const sp<ANativeWindowBuffer>& nativeBuffer);
#endif

OMX_ERRORTYPE OMX_VENC_AllocateBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                   OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
                   OMX_IN OMX_U32 nPortIndex,
                   OMX_IN OMX_PTR pAppPrivate,
                   OMX_IN OMX_U32 nSizeBytes);

OMX_ERRORTYPE OMX_VENC_FreeBuffer(OMX_IN  OMX_HANDLETYPE hComponent,
               OMX_IN  OMX_U32 nPortIndex,
               OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHdr);


OMX_ERRORTYPE OMX_VENC_FillThisBuffer(OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE * pBufferHdr);


OMX_ERRORTYPE OMX_VENC_SetCallbacks(OMX_IN  OMX_HANDLETYPE hComponent,
                 OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
                 OMX_IN  OMX_PTR pAppData);

OMX_ERRORTYPE OMX_VENC_DeInit(OMX_IN  OMX_HANDLETYPE hComponent);

OMX_ERRORTYPE OMX_VENC_GetState(OMX_IN  OMX_HANDLETYPE hComponent,
             OMX_OUT OMX_STATETYPE* pState);

OMX_ERRORTYPE OMX_VENC_GetConfig(OMX_IN OMX_HANDLETYPE hComponent,
                                 OMX_IN OMX_INDEXTYPE nIndex,
                                 OMX_INOUT OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE OMX_VENC_SetConfig(OMX_IN OMX_HANDLETYPE hComponent,
                                 OMX_IN OMX_INDEXTYPE nIndex,
                                 OMX_IN OMX_PTR pComponentConfigStructure);

}

OMX_STRING
GetMimeFromOmxCodingType(OMX_VIDEO_CODINGTYPE OMXCodingType)
{
    ALOGD("%s: Identified Mime Type Is %s",__FUNCTION__, CodecToMIME[OMXCodingType].Mime);
    return CodecToMIME[OMXCodingType].Mime;
}

NEXUS_VideoFrameRate
MapOMXFrameRateToNexus(OMX_U32 omxFR)
{
    for (unsigned int i = 0; i < sizeof(frameRateTypeConversion)/sizeof(frameRateTypeConversion[0]); i++)
    {
        if (frameRateTypeConversion[i].OmxFramerate==omxFR)
            return frameRateTypeConversion[i].NxFrameRate;
    }
    return NEXUS_VideoFrameRate_eUnknown;
}

static bool
StartEncoder(BCM_OMX_CONTEXT *pBcmContext)
{
    bool ret = false;
    VIDEO_ENCODER_START_PARAMS startParams;

    startParams.width = pBcmContext->sInPortDef.format.video.nFrameWidth;
    startParams.height = pBcmContext->sInPortDef.format.video.nFrameHeight;
    startParams.frameRate = MapOMXFrameRateToNexus(pBcmContext->sInPortDef.format.video.xFramerate);
    if (startParams.frameRate==NEXUS_VideoFrameRate_eUnknown)
    {
        ALOGE("%s:Unsupported frame rate.",__FUNCTION__);
        return false;
    }
    startParams.avcParams = pBcmContext->sAvcVideoParams;
    startParams.bitRateParams = pBcmContext->sRateParams;

    ret = pBcmContext->pOMXNxVidEnc->StartEncoder(&startParams);
    return ret;
}

static void
StopEncoder(BCM_OMX_CONTEXT *pBcmContext)
{
    pBcmContext->pOMXNxVidEnc->StopEncoder();
}

static bool
FlushInput(BCM_OMX_CONTEXT *pBcmContext)
{
    if (false==pBcmContext->pOMXNxVidEnc->FlushInput())
    {
        ALOGE("==FLUSH FAILED ON INPUT PORT==");
        return false;
    }
    ListFlushEntries(pBcmContext->sInBufList, pBcmContext);
    return true;
}

static bool
FlushOutput(BCM_OMX_CONTEXT *pBcmContext)
{
    if (false==pBcmContext->pOMXNxVidEnc->FlushOutput())
    {
        ALOGE("==FLUSH FAILED ON OUTPUT PORT==");
        return false;
    }
    ListFlushEntries(pBcmContext->sOutBufList, pBcmContext);
    return true;
}

extern "C" OMX_ERRORTYPE OMX_ComponentInit(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pComp = NULL;
    BCM_OMX_CONTEXT *pMyData;
    OMX_U32 err;
    uint nIndex;
    ErrorStatus eStatus;

    trace t(__FUNCTION__);

    pComp = (OMX_COMPONENTTYPE *) hComponent;

    // Create private data
    pMyData = (BCM_OMX_CONTEXT *)BKNI_Malloc(sizeof(BCM_OMX_CONTEXT));
    if(pMyData == NULL)
    {
        ALOGE("Error in allocating mem for pMyData\n");
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    BKNI_Memset(pMyData, 0x0, sizeof(BCM_OMX_CONTEXT));

    pComp->pComponentPrivate = (OMX_PTR)pMyData;
    pMyData->state = OMX_StateLoaded;
    pMyData->hSelf = hComponent;

    ALOGD("%s %d: Component Handle :%p BUILD-DATE:%s BUILD-TIME:%s ",
          __FUNCTION__,__LINE__,hComponent,__DATE__,__TIME__);

    pComp->SetParameter         = OMX_VENC_SetParameter;
    pComp->GetParameter         = OMX_VENC_GetParameter;
    pComp->GetExtensionIndex    = OMX_VENC_GetExtensionIndex;
    pComp->SendCommand          = OMX_VENC_SendCommand;
    pComp->SetCallbacks         = OMX_VENC_SetCallbacks;
    pComp->ComponentDeInit      = OMX_VENC_DeInit;
    pComp->GetState             = OMX_VENC_GetState;
    pComp->GetConfig            = OMX_VENC_GetConfig;
    pComp->SetConfig            = OMX_VENC_SetConfig;
    pComp->UseBuffer            = OMX_VENC_UseBuffer;
    pComp->AllocateBuffer       = OMX_VENC_AllocateBuffer;
    pComp->FreeBuffer           = OMX_VENC_FreeBuffer;
    pComp->EmptyThisBuffer      = OMX_VENC_EmptyThisBuffer;
    pComp->FillThisBuffer       = OMX_VENC_FillThisBuffer;

     // Initialize component data structures to default values
     OMX_CONF_INIT_STRUCT_PTR(&pMyData->sPortParam, OMX_PORT_PARAM_TYPE);
     pMyData->sPortParam.nPorts = 0x2;
     pMyData->sPortParam.nStartPortNumber = 0x0;

    // Initialize the video parameters for input port
     OMX_CONF_INIT_STRUCT_PTR(&pMyData->sInPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
     pMyData->sInPortDef.nPortIndex = 0x0;
     pMyData->sInPortDef.bEnabled = OMX_TRUE;
     pMyData->sInPortDef.bPopulated = OMX_FALSE;
     pMyData->sInPortDef.eDomain = OMX_PortDomainVideo;
     pMyData->sInPortDef.eDir = OMX_DirInput;
     pMyData->sInPortDef.format.video.cMIMEType = (OMX_STRING)"video/raw";
     pMyData->sInPortDef.format.video.pNativeRender = NULL;
     pMyData->sInPortDef.format.video.nFrameWidth = 176;
     pMyData->sInPortDef.format.video.nFrameHeight = 144;
     pMyData->sInPortDef.format.video.nStride = pMyData->sInPortDef.format.video.nFrameWidth;
     pMyData->sInPortDef.format.video.nSliceHeight = pMyData->sInPortDef.format.video.nFrameHeight;
     pMyData->sInPortDef.format.video.nBitrate = 0;
     pMyData->sInPortDef.format.video.xFramerate = VIDEO_ENCODER_DEFAULT_FRAMERATE;
     pMyData->sInPortDef.format.video.bFlagErrorConcealment = OMX_FALSE;
     pMyData->sInPortDef.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
     pMyData->sInPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
     pMyData->sInPortDef.format.video.pNativeWindow = NULL;
     pMyData->sInPortDef.nBufferCountMin = VIDEO_ENCODER_NUM_IN_BUFFERS;
     pMyData->sInPortDef.nBufferAlignment = 1;
     pMyData->sInPortDef.nBufferCountActual = VIDEO_ENCODER_NUM_IN_BUFFERS;
     pMyData->sInPortDef.nBufferSize =
                                (OMX_U32) (pMyData->sInPortDef.format.video.nFrameWidth *
                                           pMyData->sInPortDef.format.video.nFrameHeight * 3) / 2;

    //Set The Input Port Format Structure
    OMX_CONF_INIT_STRUCT_PTR(&pMyData->sInPortFormat, OMX_VIDEO_PARAM_PORTFORMATTYPE);
    pMyData->sInPortFormat.eCompressionFormat = OMX_VIDEO_CodingUnused;
    pMyData->sInPortFormat.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    pMyData->sInPortFormat.nIndex=0;
    pMyData->sInPortFormat.xFramerate=VIDEO_ENCODER_DEFAULT_FRAMERATE;
    pMyData->sInPortFormat.nPortIndex=0;

    //Set The Output Port Format Structure
    OMX_CONF_INIT_STRUCT_PTR(&pMyData->sOutPortFormat, OMX_VIDEO_PARAM_PORTFORMATTYPE);
    pMyData->sOutPortFormat.eCompressionFormat = OMX_VIDEO_CodingAVC;
    pMyData->sOutPortFormat.eColorFormat = OMX_COLOR_FormatUnused;
    pMyData->sOutPortFormat.nIndex=0;            //N-1 Where N Is the number of formats supported. We only Support One format for Now.
    pMyData->sOutPortFormat.xFramerate=VIDEO_ENCODER_DEFAULT_FRAMERATE;
    pMyData->sOutPortFormat.nPortIndex=1;

    // Initialize the video parameters for output port
    OMX_CONF_INIT_STRUCT_PTR(&pMyData->sOutPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    pMyData->sOutPortDef.nPortIndex = 0x1;
    pMyData->sOutPortDef.bEnabled = OMX_TRUE;
    pMyData->sOutPortDef.bPopulated = OMX_FALSE;
    pMyData->sOutPortDef.eDomain = OMX_PortDomainVideo;
    pMyData->sOutPortDef.eDir = OMX_DirOutput;
    pMyData->sOutPortDef.nBufferCountMin = VIDEO_ENCODER_NUM_OUT_BUFFERS;
    pMyData->sOutPortDef.nBufferCountActual = VIDEO_ENCODER_NUM_OUT_BUFFERS;


    pMyData->sOutPortDef.format.video.cMIMEType = GetMimeFromOmxCodingType(OMX_COMPRESSION_FORMAT);
    pMyData->sOutPortDef.format.video.pNativeRender = NULL;

    //This will be over written later in SetParams
    pMyData->sOutPortDef.format.video.nFrameWidth = 176;
    pMyData->sOutPortDef.format.video.nFrameHeight = 144;

    pMyData->sOutPortDef.format.video.nStride = pMyData->sOutPortDef.format.video.nFrameWidth;
    pMyData->sOutPortDef.format.video.nSliceHeight = pMyData->sOutPortDef.format.video.nFrameHeight;
    pMyData->sOutPortDef.format.video.nBitrate = 64000;
    pMyData->sOutPortDef.format.video.xFramerate = VIDEO_ENCODER_DEFAULT_FRAMERATE;
    pMyData->sOutPortDef.format.video.bFlagErrorConcealment = OMX_FALSE;
    pMyData->sOutPortDef.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;

    pMyData->sOutPortDef.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pMyData->sOutPortDef.nBufferSize = VIDEO_ENCODER_OUTPUT_BUFFER_SIZE;
    pMyData->sOutPortDef.format.video.pNativeWindow = NULL;

    pMyData->sAvcVideoParams.nPortIndex = pMyData->sOutPortDef.nPortIndex;
    pMyData->sAvcVideoParams.eProfile = OMX_VIDEO_AVCProfileMain;
    pMyData->sAvcVideoParams.eLevel = OMX_VIDEO_AVCLevel31;

    // Initialize the input buffer list
    BKNI_Memset(&(pMyData->sInBufList), 0x0, sizeof(BufferList));
    pMyData->sInBufList.pBufHdr = (OMX_BUFFERHEADERTYPE**)
    BKNI_Malloc(sizeof(OMX_BUFFERHEADERTYPE*)* pMyData->sInPortDef.nBufferCountActual);

    if(pMyData->sInBufList.pBufHdr == NULL)
    {
        LOGE("Error in allocating mem for pMyData->sInBufList.pBufHdr\n");
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pMyData->pInBufHdrRes = (OMX_BUFFERHEADERTYPE**)
        BKNI_Malloc (sizeof(OMX_BUFFERHEADERTYPE*) * NUM_IN_BUFFERS);

    if(pMyData->pInBufHdrRes == NULL)
    {
        LOGE("Error in allocating mem for pMyData->pInBufHdrRes\n");
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    for (nIndex = 0; nIndex < pMyData->sInPortDef.nBufferCountActual; nIndex++)
    {
        pMyData->sInBufList.pBufHdr[nIndex] = (OMX_BUFFERHEADERTYPE*)
                                                 malloc(sizeof(OMX_BUFFERHEADERTYPE));
        OMX_CONF_INIT_STRUCT_PTR (pMyData->sInBufList.pBufHdr[nIndex], OMX_BUFFERHEADERTYPE);
    }

    pMyData->sInBufList.nSizeOfList = 0;
    pMyData->sInBufList.nAllocSize = 0;
    pMyData->sInBufList.nListEnd = -1;
    pMyData->sInBufList.nWritePos = -1;
    pMyData->sInBufList.nReadPos = -1;
    pMyData->sInBufList.eDir = OMX_DirInput;
    pthread_mutex_init(&pMyData->sInBufList.mutex, NULL);

    // Initialize the output buffer list
    BKNI_Memset(&(pMyData->sOutBufList), 0x0, sizeof(BufferList));
    pMyData->sOutBufList.pBufHdr = (OMX_BUFFERHEADERTYPE**)
    BKNI_Malloc (sizeof(OMX_BUFFERHEADERTYPE*) * MAX_NUM_OUT_BUFFERS);

    if(pMyData->sOutBufList.pBufHdr == NULL)
    {
        LOGE("Error in allocating mem for pMyData->sOutBufList.pBufHdr\n");
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pMyData->pOutBufHdrRes= (OMX_BUFFERHEADERTYPE**)
    BKNI_Malloc (sizeof(OMX_BUFFERHEADERTYPE*) * MAX_NUM_OUT_BUFFERS);

    if(pMyData->pOutBufHdrRes == NULL)
    {
        LOGE("Error in allocating mem for pMyData->pOutBufHdrRes\n");
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    for (nIndex = 0; nIndex < MAX_NUM_OUT_BUFFERS; nIndex++)
    {
        pMyData->sOutBufList.pBufHdr[nIndex] = (OMX_BUFFERHEADERTYPE*)
                                                 malloc(sizeof(OMX_BUFFERHEADERTYPE));

        if(pMyData->sOutBufList.pBufHdr[nIndex] == NULL)
        {
            LOGE("Error in allocating mem for pMyData->sOutBufList.pBufHdr[nIndex]\n");
            eError = OMX_ErrorInsufficientResources;
            goto EXIT;

        }
        OMX_CONF_INIT_STRUCT_PTR (pMyData->sOutBufList.pBufHdr[nIndex], OMX_BUFFERHEADERTYPE);
    }

    pMyData->sOutBufList.nSizeOfList = 0;
    pMyData->sOutBufList.nAllocSize = 0;
    pMyData->sOutBufList.nListEnd = -1;
    pMyData->sOutBufList.nWritePos = -1;
    pMyData->sOutBufList.nReadPos = -1;
    pMyData->sOutBufList.eDir = OMX_DirOutput;
    pthread_mutex_init(&pMyData->sOutBufList.mutex, NULL);

    // Create the pipe used to send commands to the thread
    err = pipe((int*)pMyData->cmdpipe);
    if (err)
    {
       eError = OMX_ErrorInsufficientResources;
       goto EXIT;
    }

    // Create the pipe used to send command data to the thread
    err = pipe((int*)pMyData->cmddatapipe);
    if (err)
    {
       eError = OMX_ErrorInsufficientResources;
       goto EXIT;
    }

    // Create the component thread
    err = pthread_create(&pMyData->thread_id, NULL, ComponentThread, pMyData);
    if( err || !pMyData->thread_id )
    {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pMyData->pOMXNxVidEnc = new OMXNexusVideoEncoder(LOG_TAG, VIDEO_ENCODER_NUM_IN_BUFFERS);
    eStatus = pMyData->pOMXNxVidEnc->GetLastError();
    if (eStatus != ErrStsSuccess)
    {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pMyData->pTstable = new bcmOmxTimestampTable(MAX_NUM_TIMESTAMPS);

#if 0
    if (false == pMyData->pOMXNxVidEnc->StartEncoder())
    {
        ALOGE("%s: Failed To Start The Encoder",__FUNCTION__);
        goto EXIT;
    }
#endif

    return eError;

EXIT:
    OMX_VENC_DeInit(hComponent);
    return eError;
}

extern "C" OMX_ERRORTYPE OMX_VENC_DeInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    void *pError = (void *)&eError;
    ThrCmdType eCmd = Stop;
    OMX_S32 nIndex = 0;
    NEXUS_PlaypumpStatus playpumpStatus;

    trace t(__FUNCTION__);

    ALOGD("%s %d: Component Handle :%p BUILD-DATE:%s BUILD-TIME:%s",
          __FUNCTION__,__LINE__,hComponent,__DATE__,__TIME__);

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    // In case the client crashes, check for nAllocSize parameter.
    // If this is greater than zero, there are elements in the list that are not free'd.
    // In that case, free the elements.

    if (pMyData != NULL)
    {
        // Put the command and data in the pipe
        if (0!= pMyData->cmdpipe[1])
        {
            write(pMyData->cmdpipe[1], &eCmd, sizeof(eCmd));
        }

        if (0!= pMyData->cmddatapipe[1])
        {
            write(pMyData->cmddatapipe[1], &eCmd, sizeof(eCmd));
        }

        // Wait for thread to exit so we can get the status into "error"
        if (0 != pMyData->thread_id)
        {
            pthread_join(pMyData->thread_id, &pError);
        }

        // close the pipe handles
        if (pMyData->cmdpipe[0])
        {
            close(pMyData->cmdpipe[0]);
        }

        if (pMyData->cmdpipe[1])
        {
            close(pMyData->cmdpipe[1]);
        }

        if (pMyData->cmddatapipe[0])
        {
            close(pMyData->cmddatapipe[0]);
        }

        if (pMyData->cmddatapipe[1])
        {
            close(pMyData->cmddatapipe[1]);
        }

        /* Free up all buffers */
        if (pMyData->sInBufList.nAllocSize > 0)
        {
            ListFreeAllBuffers(pMyData->sInBufList, nIndex);
        }
        pthread_mutex_destroy(&pMyData->sInBufList.mutex);

        if (pMyData->sOutBufList.nAllocSize > 0)
        {
            ListFreeAllBuffers(pMyData->sOutBufList, nIndex);
        }
        pthread_mutex_destroy(&pMyData->sOutBufList.mutex);

        if (pMyData->pTstable)
        {
            delete pMyData->pTstable;
            pMyData->pTstable = NULL;
        }

        if (pMyData->pOMXNxVidEnc)
        {
            delete pMyData->pOMXNxVidEnc;
            pMyData->pOMXNxVidEnc = NULL;
        }

        BKNI_Free(pMyData);

        ((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate = NULL;
    }

    return eError;
}

extern "C" OMX_ERRORTYPE OMX_VENC_SendCommand(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_COMMANDTYPE eCmd,
    OMX_IN OMX_U32 nParam, OMX_IN OMX_PTR pCmdData)
{
    trace t(__FUNCTION__);
    LOGV(" OMX_VENC_SendCommand cmd is %d",eCmd);

    BCM_OMX_CONTEXT *pMyData;
    ThrCmdType eCmd1 = Undefined;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, 1, 1);
    if (eCmd == OMX_CommandMarkBuffer)
    {
       OMX_CONF_CHECK_CMD(pCmdData, 1, 1);
    }

    if (pMyData->state == OMX_StateInvalid)
    {
       OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInvalidState);
    }

    switch (eCmd)
    {
        case OMX_CommandStateSet:
        {
           eCmd1 = SetState;
        }
        break;
        case OMX_CommandFlush:
        {
           eCmd1 = Flush;
           if ((int)nParam > 1 && (int)nParam != -1)
           {
              OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadPortIndex);
           }
        }
        break;
        case OMX_CommandPortDisable:
        {
           eCmd1 = StopPort;
        }
        break;
        case OMX_CommandPortEnable:
        {
           eCmd1 = RestartPort;
        }
        break;
        case OMX_CommandMarkBuffer:
        {
           eCmd1 = MarkBuf;
           if (nParam > 0)
           {
                OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadPortIndex);
           }
        }
        break;
        default:
        {
        }
        break;
    }

    write(pMyData->cmdpipe[1], &eCmd, sizeof(eCmd));

    // In case of MarkBuf, the pCmdData parameter is used to carry the data.
    // In other cases, the nParam1 parameter carries the data.
    if (eCmd1 == MarkBuf)
    {
        write(pMyData->cmddatapipe[1], &pCmdData, sizeof(OMX_PTR));
    }
    else
    {
        write(pMyData->cmddatapipe[1], &nParam, sizeof(nParam));
    }

OMX_CONF_CMD_BAIL:
    return eError;
}

#define OMX_ANDROID_IDX_EXT_META_IN_BUF     "OMX.google.android.index.storeMetaDataInBuffers"
extern "C" OMX_ERRORTYPE OMX_VENC_GetExtensionIndex(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_STRING cParameterName, OMX_OUT OMX_INDEXTYPE * pIndexType)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    UNUSED(hComponent);

    trace t(__FUNCTION__);

    ALOGD("%s %s", __FUNCTION__, cParameterName);

    if (!strncasecmp(OMX_ANDROID_IDX_EXT_META_IN_BUF,
                     cParameterName,
                     strlen(OMX_ANDROID_IDX_EXT_META_IN_BUF)))
    {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexStoreMetaDataInBuffers;
    }
    else
    {
        eError = OMX_ErrorUnsupportedIndex;
    }

    return eError;
}

static void PrintPortDefInfo(OMX_PARAM_PORTDEFINITIONTYPE   *pPortDef)
{
    if (pPortDef)
    {
        ALOGD("%s: nPortIndex:%lu nBufferCountActual:%lu nBufferCountMin:%lu nBufferSize:%lu ",
            __FUNCTION__,
            pPortDef->nPortIndex,
            pPortDef->nBufferCountActual,
            pPortDef->nBufferCountMin,
            pPortDef->nBufferSize);

        ALOGD(">>>>>>>>>>>>>>>> bEnabled:%d bPopulated:%d eDomain:%d bBuffersContiguous:%d ",
            pPortDef->bEnabled,
            pPortDef->bPopulated,
            pPortDef->eDomain,
            pPortDef->bBuffersContiguous);

        ALOGD(">>>>>>>>>>>>>>>> nBufferAlignment:%lu pNativeRender:%p nFrameWidth:%lu nFrameHeight:%lu cMimeType:%s",
              pPortDef->nBufferAlignment,
              pPortDef->format.video.pNativeRender,
              pPortDef->format.video.nFrameWidth,
              pPortDef->format.video.nFrameHeight,
              pPortDef->format.video.cMIMEType);

        ALOGD(">>>>>>>>>>>>>>>> nStride:%ld nSliceHeight:%lu nBitrate:%lu xFramerate:%lu ",
              pPortDef->format.video.nStride,
              pPortDef->format.video.nSliceHeight,
              pPortDef->format.video.nBitrate,
              pPortDef->format.video.xFramerate);

        ALOGD(">>>>>>>>>>>>>>>> bFlagErrorConcealment:%d eCompressionFormat:%d eColorFormat:%d pNativeWindow:%p ",
              pPortDef->format.video.bFlagErrorConcealment,
              pPortDef->format.video.eCompressionFormat,
              pPortDef->format.video.eColorFormat,
              pPortDef->format.video.pNativeWindow);
    }
}

extern "C" OMX_ERRORTYPE OMX_VENC_GetParameter(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pParamStruct)
{
    trace t(__FUNCTION__);

    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, pParamStruct, 1);

    if (pMyData->state == OMX_StateInvalid)
    {
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);
    }

    switch (nParamIndex)
    {
        case OMX_IndexParamVideoInit:
        {
            CONFIG_LOG_MSG("%s: OMX_IndexParamVideoInit",__FUNCTION__);
            BKNI_Memcpy(pParamStruct, &pMyData->sPortParam, sizeof(OMX_PORT_PARAM_TYPE));
        }
        break;

        case OMX_IndexParamPortDefinition:
        {
            if (((OMX_PARAM_PORTDEFINITIONTYPE *)(pParamStruct))->nPortIndex == pMyData->sInPortDef.nPortIndex)
            {
                CONFIG_LOG_MSG("%s: OMX_IndexParamPortDefinition: Input Port",__FUNCTION__);
                PrintPortDefInfo(&pMyData->sInPortDef);
                BKNI_Memcpy(pParamStruct, &pMyData->sInPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
            }
            else if (((OMX_PARAM_PORTDEFINITIONTYPE *)(pParamStruct))->nPortIndex == pMyData->sOutPortDef.nPortIndex)
            {
                CONFIG_LOG_MSG("%s: OMX_IndexParamPortDefinition: Output Port",__FUNCTION__);
                PrintPortDefInfo(&pMyData->sOutPortDef);
                BKNI_Memcpy(pParamStruct, &pMyData->sOutPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
            }
            else
            {
                eError = OMX_ErrorBadPortIndex;
            }
        }
        break;

        case OMX_IndexParamPriorityMgmt:
        {
           CONFIG_LOG_MSG("%s: OMX_IndexParamPriorityMgmt",__FUNCTION__);
           BKNI_Memcpy(pParamStruct, &pMyData->sPriorityMgmt, sizeof(OMX_PRIORITYMGMTTYPE));
        }
        break;

        case OMX_IndexParamVideoMpeg2:
        {
            CONFIG_LOG_MSG("%s: OMX_IndexParamVideoMpeg2",__FUNCTION__);
            if (((OMX_VIDEO_PARAM_MPEG2TYPE *)(pParamStruct))->nPortIndex == pMyData->sMpeg2.nPortIndex)
            {
                BKNI_Memcpy(pParamStruct, &pMyData->sMpeg2, sizeof(OMX_VIDEO_PARAM_MPEG2TYPE));
            }
            else
            {
                eError = OMX_ErrorBadPortIndex;
            }
        }
        break;

        case OMX_IndexParamVideoProfileLevelQuerySupported:
        {
            CONFIG_LOG_MSG("%s: OMX_IndexParamVideoProfileLevelQuerySupported",__FUNCTION__);

            OMX_VIDEO_PARAM_PROFILELEVELTYPE *profileLevel =
                    (OMX_VIDEO_PARAM_PROFILELEVELTYPE *) pParamStruct;

            if (profileLevel->nPortIndex != pMyData->sOutPortDef.nPortIndex)
            {
                ALOGE("Invalid port index: %ld", profileLevel->nPortIndex);
                eError = OMX_ErrorUnsupportedIndex;
                break;
            }

            size_t index = profileLevel->nProfileIndex;
            size_t nProfileLevels =
                    sizeof(ProfileLevels) / sizeof(ProfileLevels[0]);
            if (index >= nProfileLevels)
            {
                eError = OMX_ErrorNoMore;
                break;
            }
            profileLevel->eProfile = ProfileLevels[index].mProfile;
            profileLevel->eLevel = ProfileLevels[index].mLevel;
        }
        break;

        case OMX_IndexParamVideoPortFormat:
        {
            CONFIG_LOG_MSG("%s: OMX_IndexParamVideoPortFormat",__FUNCTION__);
            OMX_VIDEO_PARAM_PORTFORMATTYPE *pFormatParams=
                (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pParamStruct;

            if (pFormatParams->nPortIndex == pMyData->sInPortDef.nPortIndex)
            {
                if (pFormatParams->nIndex >= NUM_SUPPORTED_INPUT_FORMAT)
                {
                    eError = OMX_ErrorNoMore;
                    break;
                }

                OMX_U32 uIdx = pFormatParams->nIndex;

                BKNI_Memcpy(pFormatParams,&pMyData->sInPortFormat,sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
                pFormatParams->nIndex = uIdx;
                pFormatParams->eColorFormat = PortInputFormats[uIdx];

                ALOGD("%s: OMX_IndexParamVideoPortFormat InputPort Returning CompFmt:%d ColFmt:%d Index:%d PortIndex:%d xFrameRate:%d",
                    __FUNCTION__,
                    pFormatParams->eCompressionFormat,
                    pFormatParams->eColorFormat,
                    pFormatParams->nIndex,
                    pFormatParams->nPortIndex,
                    pFormatParams->xFramerate);
            }
            else
            {
                /* Support only one color format (nIndex = 0). Return error on subsequent color format queries (nIndex > 0). */
                if (pFormatParams->nIndex > 0)
                {
                    eError = OMX_ErrorUnsupportedSetting;
                    break;
                }

                BKNI_Memcpy(pFormatParams,&pMyData->sOutPortFormat,sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
                ALOGD("%s: OMX_IndexParamVideoPortFormat OutPort Returning CompFmt:%d ColFmt:%d Index:%d PortIndex:%d xFrameRate:%d",
                    __FUNCTION__,
                    pFormatParams->eCompressionFormat,
                    pFormatParams->eColorFormat,
                    pFormatParams->nIndex,
                    pFormatParams->nPortIndex,
                    pFormatParams->xFramerate);
            }
        }
        break;

        case OMX_IndexParamVideoAvc:
        {
            CONFIG_LOG_MSG("%s: OMX_IndexParamVideoAvc",__FUNCTION__);
            OMX_VIDEO_PARAM_AVCTYPE *pFormatParams =
                (OMX_VIDEO_PARAM_AVCTYPE *)pParamStruct;
            if (pFormatParams->nPortIndex == pMyData->sOutPortDef.nPortIndex)
            {
                BKNI_Memcpy(pParamStruct, &pMyData->sAvcVideoParams, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
                ALOGD("Profile = %d, Level = %d", pFormatParams->eProfile, pFormatParams->eLevel);
            }
            else
            {
                eError = OMX_ErrorUnsupportedSetting;
            }
        }
        break;

        case OMX_IndexParamVideoBitrate:
        {
            OMX_VIDEO_PARAM_BITRATETYPE *pRateParams =
                        (OMX_VIDEO_PARAM_BITRATETYPE *)pParamStruct;

            if (pRateParams->nPortIndex == pMyData->sOutPortDef.nPortIndex)
            {
                BKNI_Memcpy(pParamStruct, &pMyData->sRateParams, sizeof(OMX_VIDEO_PARAM_BITRATETYPE));

                CONFIG_LOG_MSG("%s: OMX_IndexParamVideoBitrate - control=%d bitrate=%d",
                               __FUNCTION__,
                               pMyData->sRateParams.eControlRate,
                               pMyData->sRateParams.nTargetBitrate);
            }
            else
            {
                ALOGE("%s: Cannot get bit rate setting from input port", __FUNCTION__);
                eError = OMX_ErrorUnsupportedIndex;
            }
        }
        break;

        default:
        {
            ALOGD("%s:  UNHANDLED PARAMETER INDEX %d",__FUNCTION__, nParamIndex);
            eError = OMX_ErrorUnsupportedIndex;
        }
        break;
    }

OMX_CONF_CMD_BAIL:
    return eError;
}

extern "C" OMX_ERRORTYPE OMX_VENC_SetParameter(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pParamStruct)
{
    trace t(__FUNCTION__);

    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, pParamStruct, 1);

    if (pMyData->state != OMX_StateLoaded)
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);

    switch ((int)nParamIndex)
    {
        case OMX_IndexParamStandardComponentRole:
        {
            const OMX_PARAM_COMPONENTROLETYPE *pRoleParam = (const OMX_PARAM_COMPONENTROLETYPE *)pParamStruct;

            CONFIG_LOG_MSG("%s: OMX_IndexParamStandardComponentRole", __FUNCTION__);

            if (strncasecmp((const char *)(pRoleParam->cRole), "video_encoder.avc", OMX_MAX_STRINGNAME_SIZE - 1))
            {
                ALOGE("%s: Cannot support component role '%s'", __FUNCTION__, pRoleParam->cRole);
                eError = OMX_ErrorUndefined;
            }
        }
        break;

        case OMX_IndexParamVideoInit:
        {
            CONFIG_LOG_MSG("%s: OMX_IndexParamVideoInit",__FUNCTION__);
            BKNI_Memcpy(&pMyData->sPortParam, pParamStruct, sizeof(OMX_PORT_PARAM_TYPE));
        }
        break;

        case OMX_IndexParamPortDefinition:
        {
            CONFIG_LOG_MSG("%s: OMX_IndexParamPortDefinition",__FUNCTION__);
            PrintPortDefInfo((OMX_PARAM_PORTDEFINITIONTYPE *)(pParamStruct));
            if ( ((OMX_PARAM_PORTDEFINITIONTYPE *)(pParamStruct))->nPortIndex  == pMyData->sInPortDef.nPortIndex)
            {
                BKNI_Memcpy(&pMyData->sInPortDef, pParamStruct, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
            }
            else if ( ( (OMX_PARAM_PORTDEFINITIONTYPE *)(pParamStruct))->nPortIndex == pMyData->sOutPortDef.nPortIndex)
            {
                //
                // You cannot blindly copy the data You will need to raise a event here
                // Saying port setting changed if the parameters are not same
                //

                BKNI_Memcpy(&pMyData->sOutPortDef, pParamStruct, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
            }
            else
            {
                eError = OMX_ErrorBadPortIndex;
            }
        }
        break;

        case OMX_IndexParamPriorityMgmt:
        {
            CONFIG_LOG_MSG("%s: OMX_IndexParamPriorityMgmt",__FUNCTION__);

            BKNI_Memcpy(&pMyData->sPriorityMgmt, pParamStruct, sizeof (OMX_PRIORITYMGMTTYPE));
        }
        break;

        case OMX_IndexParamVideoMpeg2:
        {
            CONFIG_LOG_MSG("%s: OMX_IndexParamVideoMpeg2",__FUNCTION__);

            if (((OMX_VIDEO_PARAM_MPEG2TYPE *)(pParamStruct))->nPortIndex == pMyData->sMpeg2.nPortIndex)
            {
                BKNI_Memcpy(&pMyData->sMpeg2, pParamStruct, sizeof (OMX_VIDEO_PARAM_MPEG2TYPE));
            }
            else
            {
                eError = OMX_ErrorBadPortIndex;
            }
        }
        break;

        case OMX_IndexParamVideoPortFormat:
        {
            CONFIG_LOG_MSG("%s: OMX_IndexParamVideoPortFormat",__FUNCTION__);
            OMX_VIDEO_PARAM_PORTFORMATTYPE *pFormatParams =
            (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pParamStruct;

            if (pFormatParams->nPortIndex == pMyData->sOutPortDef.nPortIndex)
            {
                CONFIG_LOG_MSG("%s: OMX_IndexParamVideoPortFormat OutputPort Setting CompFmt:%d ColFmt:%d Index:%d PortIndex:%d xFrameRate:%d",
                    __FUNCTION__,
                    pFormatParams->eCompressionFormat,
                    pFormatParams->eColorFormat,
                    pFormatParams->nIndex,
                    pFormatParams->nPortIndex,
                    pFormatParams->xFramerate);

                BKNI_Memcpy(&pMyData->sOutPortFormat,pFormatParams,sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));

            }
            else if (pFormatParams->nPortIndex == pMyData->sInPortDef.nPortIndex)
            {
                CONFIG_LOG_MSG("%s: OMX_IndexParamVideoPortFormat InputPort Setting CompFmt:%d ColFmt:%d Index:%d PortIndex:%d xFrameRate:%d",
                    __FUNCTION__,
                    pFormatParams->eCompressionFormat,
                    pFormatParams->eColorFormat,
                    pFormatParams->nIndex,
                    pFormatParams->nPortIndex,
                    pFormatParams->xFramerate);

                BKNI_Memcpy(&pMyData->sInPortFormat,pFormatParams,sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
            }
            else
            {
                eError = OMX_ErrorUndefined;
            }
        }
        break;

        case OMX_IndexParamVideoAvc:
        {
            OMX_VIDEO_PARAM_AVCTYPE *pFormatParams =
                (OMX_VIDEO_PARAM_AVCTYPE *)pParamStruct;
            CONFIG_LOG_MSG("%s: OMX_IndexParamVideoAvc", __FUNCTION__);
            if (pFormatParams->nPortIndex == pMyData->sOutPortDef.nPortIndex)
            {
                BKNI_Memcpy(&pMyData->sAvcVideoParams, pParamStruct, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
                ALOGD("Profile = %d, Level = %d", pFormatParams->eProfile, pFormatParams->eLevel);
            }
            else
            {
                BKNI_Memcpy(&pMyData->sAvcVideoParams, pParamStruct, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
                ALOGD("Profile = %d, Level = %d", pFormatParams->eProfile, pFormatParams->eLevel);
                //ALOGE("%s: AVC parameters can only be configured at output port", __FUNCTION__);
                //eError = OMX_ErrorUnsupportedSetting;
            }
        }
        break;

        case OMX_IndexParamVideoBitrate:
        {
            const OMX_VIDEO_PARAM_BITRATETYPE *pRateParams =
                (const OMX_VIDEO_PARAM_BITRATETYPE *)pParamStruct;

            BKNI_Memcpy(&pMyData->sRateParams, pParamStruct, sizeof(OMX_VIDEO_PARAM_BITRATETYPE));

            CONFIG_LOG_MSG("%s: OMX_IndexParamVideoBitrate - control=%d bitrate=%d",
                           __FUNCTION__,
                           pRateParams->eControlRate,
                           pRateParams->nTargetBitrate);
        }
        break;

        case OMX_IndexStoreMetaDataInBuffers:
        {
            const StoreMetaDataInBuffersParams *pMetaDataParam =
                            (const StoreMetaDataInBuffersParams *)pParamStruct;

            CONFIG_LOG_MSG("%s: OMX_IndexStoreMetaDataInBuffers", __FUNCTION__);

            if (pMetaDataParam->nPortIndex != pMyData->sInPortDef.nPortIndex)
            {
                ALOGE("%s: Meta data not supported for output port", __FUNCTION__);
                eError = OMX_ErrorUndefined;
                break;
            }

            pMyData->bMetaDataInBuffer = (pMetaDataParam->bStoreMetaData == OMX_TRUE);

            CONFIG_LOG_MSG("%s:         Store %s in buffers",
                           __FUNCTION__,
                           pMyData->bMetaDataInBuffer ? "metadata" : "frame data");
        }
        break;

        default: // one case for enable graphics if required check ????
        {
            //eError = OMX_ErrorUnsupportedIndex;
            ALOGW("%s: UNHANDLED PARAMETER INDEX indexType=0x%X", __FUNCTION__, nParamIndex);
            eError = OMX_ErrorNone;
            break;
        }
    }

OMX_CONF_CMD_BAIL:
    return eError;
}

extern "C" OMX_ERRORTYPE OMX_VENC_EmptyThisBuffer(OMX_IN  OMX_HANDLETYPE hComponent,
                OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHdr)
{
    BCM_OMX_CONTEXT *pMyData;
    ThrCmdType eCmd = EmptyBuf;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    uint64_t nTimestamp;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    PNEXUS_VIDEO_ENCODER_INPUT_CONTEXT pNxInputCnxt =
               (PNEXUS_VIDEO_ENCODER_INPUT_CONTEXT)pBufferHdr->pInputPortPrivate;

    OMX_CONF_CHECK_CMD(pMyData, pBufferHdr, 1);
    OMX_CONF_CHK_VERSION(pBufferHdr, OMX_BUFFERHEADERTYPE, eError);

    if (!pMyData->sInPortDef.bEnabled)
    {
       OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);
    }

    if (pBufferHdr->nInputPortIndex != 0x0  || pBufferHdr->nOutputPortIndex != OMX_NOPORT)
    {
       OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadPortIndex);
    }

    if (pMyData->state != OMX_StateExecuting && pMyData->state != OMX_StatePause)
    {
       OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);
    }

/* The flags that we need to process
 * OMX_BUFFERFLAG_EOS - Taken Care.
 * -----> Tells the decoder that this input frames is the last input for this stream
 * -----> When the decoder outputs the last frame it need to set this flag to
 * -----> tell the above layer that this is the last output frame. Other falgs which
 * -----> comes in with EOS should also be copied to the above layer with EOS output.
 * OMX_BUFFERFLAG_ENDOFFRAME: Taken Care.
 * -----> Marks the frame boundary on the input side.
 * OMX_BUFFERFLAG_CODECCONFIG is an optional flag that is set by an output port when all bytes
 * in the buffer form part or all of a set of codec specific configuration data.
 * OMX_BUFFERFLAG_STARTTIME - Set by the source of a stream on the buffer that
 * contains the starting timestamp for the stream. The starting timestamp
 * corresponds to the first data that should be displayed at startup or
 * after a seek operation.
 * OMX_BUFFERFLAG_DECODEONLY is set by the source of a stream (e.g., a de-multiplexing component)
 * on any buffer that should be decoded but not rendered. This flag is used, for instance,
 * when a source seeks to a target interframe that requires decoding of frames preceding the
 * target to facilitate reconstruction of the target. In this case, the source would emit
 * the frames preceding the target downstream but mark them as decode only.
 * The OMX_BUFFERFLAG_DECODEONLY flag is associated with buffer data and propagated in a manner
 * identical to that of the buffer timestamp. A component that renders data should ignore all
 * buffers with the OMX_BUFFERFLAG_DECODEONLY flag set.
 * OMX_BUFFERFLAG_DATACORRUPT flag is set when the data in the associated buffer is identified
 * as corrupt.
 * OMX_BUFFERFLAG_SYNCFRAME should be set by an output port to indicate that the buffer content
 * contains a coded synchronization frame. A coded synchronization frame is a frame that can be
 * reconstructed without reference to any other frame information. An example of a video
 * synchronization frame is an MPEG4 I-VOP. If the OMX_BUFFERFLAG_SYNCFRAME flag is set then
 * the buffer may only contain one frame.
 * The OMX_BUFFERFLAG_EXTRADATA is used to identify the availability of additional information
 * after the buffer payload. Each extra block of data is preceded by an OMX_OTHER_EXTRADATATYPE
 * structure, which provides specific information about the extra data. Extra data shall only be
 * present when the OMX_BUFFERFLAG_EXTRADATA is signaled.
 */

    // Print the Unhandled flag So that we realize that we might need to
    // Take care of it later
    if(pBufferHdr->nFlags & OMX_BUFFERFLAG_DECODEONLY)
    {
        ALOGE("%s: OMX_BUFFERFLAG_DECODEONLY FLAG SET FOR INPUT :%d ",__FUNCTION__,pBufferHdr->nFlags);
    }

    if(pBufferHdr->nFlags & OMX_BUFFERFLAG_DATACORRUPT)
    {
        ALOGE("%s: OMX_BUFFERFLAG_DATACORRUPT FLAG SET FOR INPUT :%d ",__FUNCTION__,pBufferHdr->nFlags);
    }

    if(pBufferHdr->nFlags & OMX_BUFFERFLAG_EXTRADATA)
    {
        ALOGE("%s: OMX_BUFFERFLAG_EXTRADATA FLAG SET FOR INPUT :%d ",__FUNCTION__,pBufferHdr->nFlags);
    }

    if(pBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)
    {
        ALOGE("%s: OMX_BUFFERFLAG_EOS FLAG SET FOR INPUT :%d ",__FUNCTION__,pBufferHdr->nFlags);
    }

    if (pBufferHdr->nFlags & OMX_BUFFERFLAG_CODECCONFIG)
    {
        ALOGE("%s: OMX_BUFFERFLAG_EOS FLAG SET FOR INPUT :%d ",__FUNCTION__,pBufferHdr->nFlags);
    }

    nTimestamp = pMyData->pTstable->store(pBufferHdr->nTimeStamp);
    ALOGD("%s:IN PTS = %lld",__FUNCTION__, pBufferHdr->nTimeStamp);
    pNxInputCnxt->uSecTS = nTimestamp;

    // Put the command and data in the pipe
    write(pMyData->cmdpipe[1], &eCmd, sizeof(eCmd));
    write(pMyData->cmddatapipe[1], &pBufferHdr, sizeof(OMX_BUFFERHEADERTYPE*));

OMX_CONF_CMD_BAIL:
    return eError;

}

extern "C" OMX_ERRORTYPE OMX_VENC_UseBuffer(OMX_IN OMX_HANDLETYPE hComponent,
              OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
              OMX_IN OMX_U32 nPortIndex,
              OMX_IN OMX_PTR pAppPrivate,
              OMX_IN OMX_U32 nSizeBytes,
              OMX_IN OMX_U8* pBuffer)
{
    trace t(__FUNCTION__);
    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    OMX_S32 nIndex = 0x0;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, ppBufferHdr, pBuffer);

    if (nPortIndex == pMyData->sInPortDef.nPortIndex)
       pPortDef = &pMyData->sInPortDef;
    else if (nPortIndex == pMyData->sOutPortDef.nPortIndex)
        pPortDef = &pMyData->sOutPortDef;
    else
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadParameter);

    if (!pPortDef->bEnabled)
    {
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);
    }

    if (nSizeBytes != pPortDef->nBufferSize || pPortDef->bPopulated)
    {
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadParameter);
    }

    // Find an empty position in the BufferList and allocate memory for the buffer header.
    // Use the buffer passed by the client to initialize the actual buffer
    // inside the buffer header.
    if (nPortIndex == pMyData->sInPortDef.nPortIndex)
    {
        ListAllocate(pMyData->sInBufList, nIndex);

        pthread_mutex_lock(&pMyData->sInBufList.mutex);
        if (pMyData->sInBufList.pBufHdr[nIndex] == NULL)
        {
            pMyData->sInBufList.pBufHdr[nIndex] = (OMX_BUFFERHEADERTYPE*)
                                        malloc(sizeof(OMX_BUFFERHEADERTYPE));
            if (!pMyData->sInBufList.pBufHdr[nIndex])
            {
                pthread_mutex_unlock(&pMyData->sInBufList.mutex);
                OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);
            }

            OMX_CONF_INIT_STRUCT_PTR (pMyData->sInBufList.pBufHdr[nIndex], OMX_BUFFERHEADERTYPE);
        }

        pMyData->sInBufList.pBufHdr[nIndex]->pInputPortPrivate =
            (OMX_PTR) malloc(sizeof(NEXUS_VIDEO_ENCODER_INPUT_CONTEXT));
        if (!pMyData->sInBufList.pBufHdr[nIndex]->pInputPortPrivate)
        {
            pthread_mutex_unlock(&pMyData->sInBufList.mutex);
            OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);
        }

        pMyData->sInBufList.pBufHdr[nIndex]->pBuffer = pBuffer;
        pthread_mutex_unlock(&pMyData->sInBufList.mutex);

        LoadBufferHeader(pMyData->sInBufList, pMyData->sInBufList.pBufHdr[nIndex], pAppPrivate,
                                            nSizeBytes, nPortIndex, *ppBufferHdr, pPortDef);
    }
    else
    {
        ListAllocate(pMyData->sOutBufList,  nIndex);

        pthread_mutex_lock(&pMyData->sOutBufList.mutex);
        if (pMyData->sOutBufList.pBufHdr[nIndex] == NULL)
        {
            pMyData->sOutBufList.pBufHdr[nIndex] = (OMX_BUFFERHEADERTYPE*)
                                          malloc(sizeof(OMX_BUFFERHEADERTYPE));
            if (!pMyData->sOutBufList.pBufHdr[nIndex])
            {
                pthread_mutex_unlock(&pMyData->sOutBufList.mutex);
                OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);
            }

            OMX_CONF_INIT_STRUCT_PTR (pMyData->sOutBufList.pBufHdr[nIndex], OMX_BUFFERHEADERTYPE);
        }
        pMyData->sOutBufList.pBufHdr[nIndex]->pBuffer = pBuffer;
        pthread_mutex_unlock(&pMyData->sOutBufList.mutex);

        LoadBufferHeader(pMyData->sOutBufList, pMyData->sOutBufList.pBufHdr[nIndex],
                       pAppPrivate, nSizeBytes, nPortIndex, *ppBufferHdr, pPortDef);
        CopyBufferHeaderList(pMyData->sOutBufList, pPortDef, pMyData->pOutBufHdrRes);

    }

OMX_CONF_CMD_BAIL:
    return eError;
}

#if 0
extern "C" OMX_ERRORTYPE OMX_VENC_UseANativeWindowBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                OMX_INOUT OMX_BUFFERHEADERTYPE **bufferHeader,
                OMX_IN OMX_U32 nPortIndex,
                OMX_IN OMX_PTR pAppPrivate,
                OMX_IN OMX_U32 nSizeBytes,
                const sp<ANativeWindowBuffer>& nativeBuffer)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    trace t(__FUNCTION__);
    return eError;
}
#endif

extern "C" OMX_ERRORTYPE OMX_VENC_AllocateBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                   OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
                   OMX_IN OMX_U32 nPortIndex,
                   OMX_IN OMX_PTR pAppPrivate,
                   OMX_IN OMX_U32 nSizeBytes)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    UNUSED(hComponent);
    UNUSED(ppBufferHdr);
    UNUSED(nPortIndex);
    UNUSED(pAppPrivate);
    UNUSED(nSizeBytes);

    trace t(__FUNCTION__);
    BCMOMX_DBG_ASSERT(OMX_FALSE);
    return eError;
}

extern "C" OMX_ERRORTYPE OMX_VENC_FreeBuffer(OMX_IN  OMX_HANDLETYPE hComponent,
               OMX_IN  OMX_U32 nPortIndex,
               OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHdr)
{
        trace t(__FUNCTION__);

        BCM_OMX_CONTEXT *pMyData;
        OMX_ERRORTYPE eError = OMX_ErrorNone;
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;

        pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
        OMX_CONF_CHECK_CMD(pMyData, pBufferHdr, 1);
        OMX_CONF_CHK_VERSION(pBufferHdr, OMX_BUFFERHEADERTYPE, eError);

        ALOGD("%s, hComponent = %p, nPortIndex = %lu, pBufferHdr = %p",__FUNCTION__, hComponent, nPortIndex, pBufferHdr);

        // Match the pBufferHdr to the appropriate entry in the BufferList
        // and free the allocated memory

        if (nPortIndex == pMyData->sInPortDef.nPortIndex)
        {
            pPortDef = &pMyData->sInPortDef;

#if 0
            if(pBufferHdr->pBuffer)
            {
                BKNI_Free(pBufferHdr->pBuffer);
                pBufferHdr->pBuffer=NULL;
            }

            if(pBufferHdr->pInputPortPrivate)
            {
                PNEXUS_INPUT_CONTEXT pInNxCnxt =
                    (PNEXUS_INPUT_CONTEXT) pBufferHdr->pInputPortPrivate;

                if (pInNxCnxt->PESData)
                {
                    pMyData->pPESFeeder->FreePESBuffer(pInNxCnxt->PESData);
                    pInNxCnxt->PESData=NULL;
                }

                BKNI_Free(pBufferHdr->pInputPortPrivate);
                pBufferHdr->pInputPortPrivate = NULL;
            }
#endif
            ListFreeBuffer(pMyData->sInBufList, pBufferHdr, pPortDef)

        } else if (nPortIndex == pMyData->sOutPortDef.nPortIndex) {

#if 0
            if(pBufferHdr->pBuffer)
            {
                BKNI_Free(pBufferHdr->pBuffer);
                pBufferHdr->pBuffer=NULL;
            }
#endif

            pPortDef = &pMyData->sOutPortDef;
            OMX_S32 prevSize = pMyData->sOutBufList.nAllocSize;
            ListFreeBuffer(pMyData->sOutBufList, pBufferHdr, pPortDef)
            ListFreeBufferExt(pMyData->sOutBufList, pBufferHdr, pPortDef,prevSize,pMyData->pOutBufHdrRes)
        } else{

            OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadParameter);
        }

        if (pPortDef->bEnabled && pMyData->state != OMX_StateIdle)
        {
            ALOGE("%s: Setting Error To OMX_ErrorIncorrectStateOperation",__FUNCTION__);
            OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);
        }

    OMX_CONF_CMD_BAIL:
        return eError;
}


extern "C" OMX_ERRORTYPE OMX_VENC_FillThisBuffer(OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE * pBufferHdr)
{
     //trace t(__FUNCTION__);
     BCM_OMX_CONTEXT *pMyData;
     ThrCmdType eCmd = FillBuf;
     OMX_ERRORTYPE eError = OMX_ErrorNone;

     pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
     OMX_CONF_CHECK_CMD(pMyData, pBufferHdr, 1);
     OMX_CONF_CHK_VERSION(pBufferHdr, OMX_BUFFERHEADERTYPE, eError);

     if (!pMyData->sOutPortDef.bEnabled)
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);

     if (pBufferHdr->nOutputPortIndex != 0x1 || pBufferHdr->nInputPortIndex != OMX_NOPORT)
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadPortIndex);

     if (pMyData->state != OMX_StateExecuting && pMyData->state != OMX_StatePause)
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);

     // Put the command and data in the pipe
     write(pMyData->cmdpipe[1], &eCmd, sizeof(eCmd));
     write(pMyData->cmddatapipe[1], &pBufferHdr, sizeof(OMX_BUFFERHEADERTYPE*));

    OMX_CONF_CMD_BAIL:
     return eError;
}


extern "C" OMX_ERRORTYPE OMX_VENC_SetCallbacks(OMX_IN  OMX_HANDLETYPE hComponent,
                 OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
                 OMX_IN  OMX_PTR pAppData)
{
    trace t(__FUNCTION__);
    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, pCallbacks, pAppData);

    pMyData->pCallbacks = pCallbacks;
    pMyData->pAppData = pAppData;

OMX_CONF_CMD_BAIL:
    return eError;
}

extern "C" OMX_ERRORTYPE OMX_VENC_GetState(OMX_IN  OMX_HANDLETYPE hComponent,
             OMX_OUT OMX_STATETYPE* pState)
{
    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    trace t(__FUNCTION__);
    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    OMX_CONF_CHECK_CMD(pMyData, pState, 1);

    *pState = pMyData->state;

OMX_CONF_CMD_BAIL:
    return eError;
}

extern "C" OMX_ERRORTYPE OMX_VENC_GetConfig(OMX_IN OMX_HANDLETYPE hComponent,
                                 OMX_IN OMX_INDEXTYPE nIndex,
                                 OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    trace t(__FUNCTION__);

    OMX_ERRORTYPE eError = OMX_ErrorNone;

    UNUSED(hComponent);
    UNUSED(nIndex);
    UNUSED(pComponentConfigStructure);

    return OMX_ErrorUndefined;
}


extern "C" OMX_ERRORTYPE OMX_VENC_SetConfig(OMX_IN OMX_HANDLETYPE hComponent,
                                 OMX_IN OMX_INDEXTYPE nIndex,
                                 OMX_IN OMX_PTR pComponentConfigStructure)
{
    trace t(__FUNCTION__);

    OMX_ERRORTYPE eError = OMX_ErrorNone;

    UNUSED(hComponent);
    UNUSED(nIndex);
    UNUSED(pComponentConfigStructure);

    return OMX_ErrorUndefined;
}


static void* ComponentThread(void* pThreadData)
{
    trace t(__FUNCTION__);


    int i, fd1;
    fd_set rfds;
    OMX_U32 cmddata;
    ThrCmdType cmd;


    OMX_BUFFERHEADERTYPE *pInBufHdr = NULL;
    OMX_BUFFERHEADERTYPE *pOutBufHdr = NULL;
    OMX_MARKTYPE *pMarkBuf = NULL;
    OMX_U8 *pInBuf = NULL;
    OMX_U32 nInBufSize;
    void *buffer;
    size_t buffer_size;
    NEXUS_Error errCode;

    // Variables related to decoder timeouts
    OMX_HANDLETYPE hTimeout;
    OMX_BOOL bTimeout;
    OMX_U32 nTimeout;

    // Recover the pointer to my component specific data
    BCM_OMX_CONTEXT* pMyData = (BCM_OMX_CONTEXT*)pThreadData;

    //OMX_OSAL_EventCreate(&hTimeout);

    while (1)
    {
        fd1 = pMyData->cmdpipe[0];
        FD_ZERO(&rfds);
        FD_SET(fd1,&rfds);

        // Check for new command
        i = select(pMyData->cmdpipe[0]+1, &rfds, NULL, NULL, NULL);

        if (FD_ISSET(pMyData->cmdpipe[0], &rfds))
        {
            // retrieve command and data from pipe
            read(pMyData->cmdpipe[0], &cmd, sizeof(cmd));
            read(pMyData->cmddatapipe[0], &cmddata, sizeof(cmddata));


            // State transition command
            if (cmd == SetState)
            {
                // If the parameter states a transition to the same state
                //   raise a same state transition error.
                ALOGD("OMXVideoEncoder: SET STATE COMMAND: To %d\n",cmddata);

                if (pMyData->state == (OMX_STATETYPE)(cmddata))
                {
                    pMyData->pCallbacks->EventHandler(pMyData->hSelf, pMyData->pAppData,
                                                    OMX_EventError, OMX_ErrorSameState, 0 , NULL);
                }else{
                    // transitions/callbacks made based on state transition table
                        // cmddata contains the target state
                    switch ((OMX_STATETYPE)(cmddata))
                    {
                        case OMX_StateInvalid:
                        {
                            pMyData->state = OMX_StateInvalid;
                            pMyData->pCallbacks->EventHandler(pMyData->hSelf, pMyData->pAppData,
                                                                OMX_EventError, OMX_ErrorInvalidState, 0 , NULL);

                            pMyData->pCallbacks->EventHandler(pMyData->hSelf,pMyData->pAppData,
                                                            OMX_EventCmdComplete, OMX_CommandStateSet, pMyData->state, NULL);
                            break;
                        }

                        case OMX_StateLoaded:
                        {
                            if (pMyData->state == OMX_StateIdle || pMyData->state == OMX_StateWaitForResources)
                            {
                                if (pMyData->state == OMX_StateIdle)
                                {

                                    StopEncoder(pMyData);
                                }
                                nTimeout = 0x0;
                                while (1)
                                {
                                    // Transition happens only when the ports are unpopulated
                                    if (!pMyData->sInPortDef.bPopulated &&
                                                     !pMyData->sOutPortDef.bPopulated)
                                    {
                                        ALOGD("%s %d: State Transition Event: [To: State_Loaded] [Completed] [Timeouts:%d]",
                                              __FUNCTION__,__LINE__,nTimeout);

                                        pMyData->state = OMX_StateLoaded;
                                        pMyData->pCallbacks->EventHandler(pMyData->hSelf,
                                                           pMyData->pAppData, OMX_EventCmdComplete,
                                                               OMX_CommandStateSet, pMyData->state, NULL);

                                        break;
                                    } else if (nTimeout++ > OMX_MAX_TIMEOUTS) {

                                        ALOGE("%s %d: State Transition Event: [To: State_Loaded] [ERROR] [Timeouts:%d]",
                                              __FUNCTION__,__LINE__,nTimeout);

                                        pMyData->pCallbacks->EventHandler(
                                                            pMyData->hSelf,pMyData->pAppData, OMX_EventError,
                                                                        OMX_ErrorInsufficientResources, 0 , NULL);

                                        break;
                                    } else if (nTimeout < OMX_MAX_TIMEOUTS) {
                                        BKNI_Sleep(100);
                                    }
                                }
                            }else{

                                ALOGD("%s %d: State Transition Event: [To: State_Loaded] [ERROR OMX_ErrorIncorrectStateTransition]",
                                      __FUNCTION__,__LINE__);

                                pMyData->pCallbacks->EventHandler(pMyData->hSelf,pMyData->pAppData,
                                            OMX_EventError, OMX_ErrorIncorrectStateTransition, 0 , NULL);
                            }
                            break;
                        }

                        case OMX_StateIdle:
                        {
                            if (pMyData->state == OMX_StateInvalid)
                            {
                                ALOGE("%s %d: State Transition Event: [To: OMX_StateIdle] [ERROR]",
                                      __FUNCTION__,__LINE__);

                                pMyData->pCallbacks->EventHandler(pMyData->hSelf,pMyData->pAppData,
                                                OMX_EventError, OMX_ErrorIncorrectStateTransition, 0 , NULL);
                            }else{
                                // Return buffers if currently in pause or executing
                                if (pMyData->state == OMX_StatePause || pMyData->state == OMX_StateExecuting)
                                {
                                    FlushInput(pMyData);
                                    FlushOutput(pMyData);
                                }
                                nTimeout = 0x0;

                                if (pMyData->state == OMX_StateLoaded || pMyData->state == OMX_StateWaitForResources)
                                {
                                    StartEncoder(pMyData);
                                }
                                while (1)
                                {
                                    /* Ports have to be populated before transition completes*/
                                    if ((!pMyData->sInPortDef.bEnabled && !pMyData->sOutPortDef.bEnabled)||
                                        (pMyData->sInPortDef.bPopulated && pMyData->sOutPortDef.bPopulated))
                                    {
                                        pMyData->state = OMX_StateIdle;
                                        ALOGD("%s %d: State Transition Event: [To: OMX_StateIdle] [Completed]",
                                                    __FUNCTION__,__LINE__);

                                        pMyData->pCallbacks->EventHandler(pMyData->hSelf,
                                                                pMyData->pAppData, OMX_EventCmdComplete,
                                                                        OMX_CommandStateSet, pMyData->state, NULL);

                                        break;
                                    }
                                    BKNI_Sleep(2);
                                }
                            }
                            break;
                        }
                        case OMX_StateExecuting:
                        {
                            // Transition can only happen from pause or idle state
                            if (pMyData->state == OMX_StateIdle ||  pMyData->state == OMX_StatePause)
                            {
                                // Return buffers if currently in pause
                                if (pMyData->state == OMX_StatePause)
                                {
                                    FlushInput(pMyData);
                                    FlushOutput(pMyData);
                                }

                                ALOGD("%s %d: State Transition Event: [To: OMX_StateExecuting] [Completed]",
                                            __FUNCTION__,__LINE__);

                                pMyData->state = OMX_StateExecuting;
                                pMyData->pCallbacks->EventHandler(pMyData->hSelf,pMyData->pAppData,
                                    OMX_EventCmdComplete, OMX_CommandStateSet, pMyData->state, NULL);
                            }else{

                                ALOGE("%s %d: State Transition Event: [To: OMX_StateExecuting] [Error]",
                                            __FUNCTION__,__LINE__);

                                pMyData->pCallbacks->EventHandler(pMyData->hSelf,pMyData->pAppData,
                                    OMX_EventError, OMX_ErrorIncorrectStateTransition, 0 , NULL);
                            }
                            break;
                        }
                        case OMX_StatePause:
                        {
                            // Transition can only happen from idle or executing state
                            if (pMyData->state == OMX_StateIdle ||pMyData->state == OMX_StateExecuting)
                            {
                                pMyData->state = OMX_StatePause;

                                ALOGD("%s %d: State Transition Event: [To: OMX_StatePause] [Completed]",
                                            __FUNCTION__,__LINE__);

                                pMyData->pCallbacks->EventHandler(pMyData->hSelf,pMyData->pAppData,
                                    OMX_EventCmdComplete, OMX_CommandStateSet, pMyData->state, NULL);
                            }else {

                                ALOGE("%s %d: State Transition Event: [To: OMX_StatePause] [Error]",
                                            __FUNCTION__,__LINE__);

                                pMyData->pCallbacks->EventHandler(pMyData->hSelf,pMyData->pAppData,
                                    OMX_EventError, OMX_ErrorIncorrectStateTransition, 0 , NULL);
                            }
                            break;
                        }
                        case OMX_StateWaitForResources:
                        {

                            if (pMyData->state == OMX_StateLoaded)
                            {
                                pMyData->state = OMX_StateWaitForResources;
                                ALOGD("%s %d: State Transition Event: [To: OMX_StateWaitForResources] [Completed]",
                                            __FUNCTION__,__LINE__);

                                pMyData->pCallbacks->EventHandler(pMyData->hSelf,pMyData->pAppData,
                                    OMX_EventCmdComplete, OMX_CommandStateSet, pMyData->state, NULL);
                            } else {
                                ALOGD("%s %d: State Transition Event: [To: OMX_StateWaitForResources] [ERROR]",
                                            __FUNCTION__,__LINE__);

                                pMyData->pCallbacks->EventHandler(pMyData->hSelf,pMyData->pAppData,
                                    OMX_EventError, OMX_ErrorIncorrectStateTransition, 0 , NULL);
                            }
                            break;
                        }
                        default :
                            ALOGD("%s %d: State Transition Event: ==INVALID STATE ==",
                                            __FUNCTION__,__LINE__);

                    }
               }
            }
            else if (cmd == StopPort)
            {
                // Stop Port(s)
                // cmddata contains the port index to be stopped.
                // It is assumed that 0 is input and 1 is output port for this component
                // The cmddata value -1 means that both input and output ports will be stopped.

                ALOGD("%s: STOP PORT PortIndex : %d", __FUNCTION__, cmddata);

                if (cmddata == 0x0 || cmddata == 0xFFFFFFFF)
                {
                    // Return all input buffers
                    FlushInput(pMyData);

                    // Disable port
                    pMyData->sInPortDef.bEnabled = OMX_FALSE;
                }
                if (cmddata == 0x1 || cmddata == 0xFFFFFFFF)
                {
                    // Return all output buffers
                    FlushOutput(pMyData);

                    // Disable port
                    pMyData->sOutPortDef.bEnabled = OMX_FALSE;
                }
                // Wait for all buffers to be freed
                nTimeout = 0x0;
                while (1)
                {
                    if (cmddata == 0x0 && !pMyData->sInPortDef.bPopulated)
                    {
                        // Return cmdcomplete event if input unpopulated
                        pMyData->pCallbacks->EventHandler(pMyData->hSelf, pMyData->pAppData,
                                             OMX_EventCmdComplete, OMX_CommandPortDisable, 0x0, NULL);
                        break;
                    }
                    if (cmddata == 0x1 && !pMyData->sOutPortDef.bPopulated)
                    {
                        // Return cmdcomplete event if output unpopulated
                        pMyData->pCallbacks->EventHandler(pMyData->hSelf, pMyData->pAppData,
                                             OMX_EventCmdComplete, OMX_CommandPortDisable, 0x1, NULL);
                        break;
                    }
                    if (cmddata == 0xFFFFFFFF &&
                        !pMyData->sInPortDef.bPopulated &&
                        !pMyData->sOutPortDef.bPopulated)
                    {
                        // Return cmdcomplete event if inout & output unpopulated
                        pMyData->pCallbacks->EventHandler(pMyData->hSelf, pMyData->pAppData,
                                         OMX_EventCmdComplete, OMX_CommandPortDisable, 0x0, NULL);
                        pMyData->pCallbacks->EventHandler(pMyData->hSelf, pMyData->pAppData,
                                         OMX_EventCmdComplete, OMX_CommandPortDisable, 0x1, NULL);
                        break;
                    }
                    if (nTimeout++ > OMX_MAX_TIMEOUTS)
                    {
                        pMyData->pCallbacks->EventHandler(pMyData->hSelf,pMyData->pAppData,
                                   OMX_EventError, OMX_ErrorPortUnresponsiveDuringDeallocation,
                                                                                     0 , NULL);
                        break;
                    }
                    BKNI_Sleep(5);
                }
            }
            else if (cmd == RestartPort)
            {

                // Restart Port(s)
                // cmddata contains the port index to be restarted.
                // It is assumed that 0 is input and 1 is output port for this component.
                // The cmddata value -1 means both input and output ports will be restarted.

                ALOGD("%s: RESTART PORT PortIndex : %d", __FUNCTION__, cmddata);

                if (cmddata == 0x0 || cmddata == 0xFFFFFFFF)
                    pMyData->sInPortDef.bEnabled = OMX_TRUE;

                if (cmddata == 0x1 || cmddata == 0xFFFFFFFF)
                    pMyData->sOutPortDef.bEnabled = OMX_TRUE;

                // Wait for port to be populated
                nTimeout = 0x0;
                while (1)
                {
                    // Return cmdcomplete event if input port populated
                    if (cmddata == 0x0 && (pMyData->state == OMX_StateLoaded ||
                                        pMyData->sInPortDef.bPopulated))
                    {
                        pMyData->pCallbacks->EventHandler(pMyData->hSelf, pMyData->pAppData,
                            OMX_EventCmdComplete, OMX_CommandPortEnable, 0x0, NULL);
                        break;
                    }
                    // Return cmdcomplete event if output port populated
                    else if (cmddata == 0x1 && (pMyData->state == OMX_StateLoaded ||
                                                                 pMyData->sOutPortDef.bPopulated))
                    {
                        pMyData->pCallbacks->EventHandler(pMyData->hSelf,
                                          pMyData->pAppData, OMX_EventCmdComplete,
                                                      OMX_CommandPortEnable, 0x1, NULL);
                        break;
                    }
                     // Return cmdcomplete event if input and output ports populated
                    else if (cmddata == 0xFFFFFFFF &&
                             (pMyData->state == OMX_StateLoaded ||
                              (pMyData->sInPortDef.bPopulated &&
                               pMyData->sOutPortDef.bPopulated)))
                    {
                        pMyData->pCallbacks->EventHandler(pMyData->hSelf,
                                        pMyData->pAppData, OMX_EventCmdComplete,
                                                    OMX_CommandPortEnable, 0x0, NULL);
                        pMyData->pCallbacks->EventHandler(pMyData->hSelf,
                                        pMyData->pAppData, OMX_EventCmdComplete,
                                                     OMX_CommandPortEnable, 0x1, NULL);
                        break;
                    }
                    else if (nTimeout++ > OMX_MAX_TIMEOUTS)
                    {
                          pMyData->pCallbacks->EventHandler(
                                           pMyData->hSelf,pMyData->pAppData,
                                           OMX_EventError,
                                        OMX_ErrorPortUnresponsiveDuringAllocation,0,NULL);
                          break;
                    }
                    BKNI_Sleep(5);
                }
             }
            else if (cmd == Flush)
            {
                // Flush port(s)
                // cmddata contains the port index to be flushed.
                // It is assumed that 0 is input and 1 is output port for this component
                // The cmddata value -1 means that both input and output ports will be flushed.

                ALOGD("%s: FLUSH PORT PortIndex : %d", __FUNCTION__, cmddata);
                if (cmddata == 0x0 || cmddata == 0xFFFFFFFF)
                {
//                  // Return all Input buffers and send cmdcomplete
                    FlushInput(pMyData);
                    pMyData->pCallbacks->EventHandler(pMyData->hSelf, pMyData->pAppData,
                                          OMX_EventCmdComplete, OMX_CommandFlush, 0x0, NULL);
                }

                if (cmddata == 0x1 || cmddata == 0xFFFFFFFF)
                {
//                  // Return all output buffers and send cmdcomplete
                    FlushOutput(pMyData);
                    pMyData->pCallbacks->EventHandler(pMyData->hSelf, pMyData->pAppData,
                                          OMX_EventCmdComplete, OMX_CommandFlush, 0x1, NULL);
                }
            }
            else if (cmd == Stop)
            {
                // Kill thread
                ALOGD("%s: STOP CALLED--- EXITING THE THREAD", __FUNCTION__);
                goto EXIT;
            }
            else if (cmd == FillBuf)
            {
                // Fill buffer

                ListSetEntry(pMyData->sOutBufList, ((OMX_BUFFERHEADERTYPE*) cmddata))
            }
            else if (cmd == EmptyBuf)
            {

                // Empty buffer
                ListSetEntry(pMyData->sInBufList, ((OMX_BUFFERHEADERTYPE *) cmddata))
                // Mark current buffer if there is outstanding command
                if (pMarkBuf)
                {
                    ((OMX_BUFFERHEADERTYPE *)(cmddata))->hMarkTargetComponent =
                                                       pMarkBuf->hMarkTargetComponent;

                    ((OMX_BUFFERHEADERTYPE *)(cmddata))->pMarkData = pMarkBuf->pMarkData;
                    pMarkBuf = NULL;
                }
            }
            else if (cmd == MarkBuf)
            {
                ALOGD("%s: MARK DATA COMMAND", __FUNCTION__);
                if (!pMarkBuf)
                pMarkBuf = (OMX_MARKTYPE *)(cmddata);
            }
        }


        //Input Buffer Processing...

        if (pMyData->state == OMX_StateExecuting && pMyData->sInPortDef.bEnabled &&
                (pMyData->sInBufList.nSizeOfList > 0))
        {
            ListGetEntry(pMyData->sInBufList, pInBufHdr)
                // If there is no output buffer, get one from list
                // Check for EOS flag
            if (pInBufHdr)
            {
                if((pInBufHdr->nFilledLen)||(pInBufHdr->nFlags & OMX_BUFFERFLAG_EOS))
                {
                    PNEXUS_VIDEO_ENCODER_INPUT_CONTEXT pNxInputCnxt =
                        (PNEXUS_VIDEO_ENCODER_INPUT_CONTEXT)pInBufHdr->pInputPortPrivate;

                    // fill in the Done context and be ready for
                    // Firing the Empty Buffer done once the DMA is complete

                    pNxInputCnxt->bufPtr = pInBufHdr->pBuffer + pInBufHdr->nOffset;
                    pNxInputCnxt->bufSize = pInBufHdr->nFilledLen;
                    pNxInputCnxt->colorFormat = pMyData->sInPortDef.format.video.eColorFormat;
                    pNxInputCnxt->flags = pInBufHdr->nFlags;
                    pNxInputCnxt->height = pMyData->sInPortDef.format.video.nFrameHeight;
                    pNxInputCnxt->width = pMyData->sInPortDef.format.video.nFrameWidth;
                    pNxInputCnxt->DoneContext.Param1 = (unsigned int) pMyData->hSelf;
                    pNxInputCnxt->DoneContext.Param2 = (unsigned int) pMyData->pAppData;
                    pNxInputCnxt->DoneContext.Param3 = (unsigned int) pInBufHdr;

                    pNxInputCnxt->DoneContext.pFnDoneCallBack =
                        (BUFFER_DONE_CALLBACK) pMyData->pCallbacks->EmptyBufferDone;

                    pInBufHdr->nFilledLen =0;

                    if(false == pMyData->pOMXNxVidEnc->EncodeFrame(pNxInputCnxt))
                    {
                        LOGE("%s: Sending The Data To The Hardware Failed",__FUNCTION__);
                        ListSetEntry(pMyData->sInBufList, pInBufHdr);
                        //LOGE("%s: Call back Will Not Be Fired, Encode May Stop",__FUNCTION__);
                        //BCMOMX_DBG_ASSERT(false);
                    }

                }else{
                        pMyData->pCallbacks->EmptyBufferDone(pMyData->hSelf, pMyData->pAppData, pInBufHdr);
                }

                pInBufHdr = NULL;
            }
        }

        //Output Buffer Processing...
        if (pMyData->state == OMX_StateExecuting  &&  pMyData->sOutPortDef.bEnabled &&
            ((pMyData->sOutBufList.nSizeOfList > 0) || pOutBufHdr))
        {
            if (!pOutBufHdr)
                ListGetEntry(pMyData->sOutBufList, pOutBufHdr)

            if (pOutBufHdr)
            {
                unsigned int EOSMarkerFlags=0;
                DELIVER_ENCODED_FRAME DeliverFr;
                DeliverFr.OutFlags=0;
                DeliverFr.pVideoDataBuff = pOutBufHdr->pBuffer;
                DeliverFr.SzVidDataBuff = pOutBufHdr->nAllocLen;
                DeliverFr.SzFilled=0;
                DeliverFr.usTimeStamp=0;


                if (pMyData->pOMXNxVidEnc->GetEncodedFrame(&DeliverFr))
                {
                    uint32_t nTimeStamp;
                    //pOutBufHdr->nFilledLen = sizeof(DELIVER_ENCODED_FRAME);
                    pOutBufHdr->nFilledLen = DeliverFr.SzFilled;
                    nTimeStamp = DeliverFr.usTimeStamp;
                    if (!(DeliverFr.OutFlags & OMX_BUFFERFLAG_CODECCONFIG))
                        pOutBufHdr->nTimeStamp= pMyData->pTstable->retrieve(nTimeStamp);
                    ALOGD("%s:OUT PTS = %lld",__FUNCTION__, pOutBufHdr->nTimeStamp);
                    pOutBufHdr->nFlags |= DeliverFr.OutFlags;
                    if (DeliverFr.OutFlags & OMX_BUFFERFLAG_EOS)
                        ALOGD("EOS MARKED ON THE OUTPUT FRAME:%d",DeliverFr.OutFlags);
                }else{
                    if (DeliverFr.OutFlags & OMX_BUFFERFLAG_EOS)
                    {
                        ALOGD("EOS MARKED ON THE OUTPUT FRAME:%d",
                            DeliverFr.OutFlags);

                        pOutBufHdr->nFlags |= OMX_BUFFERFLAG_EOS;
                        EOSMarkerFlags = DeliverFr.OutFlags;
                    }
                    // This may be a frame with NULL data
                    pOutBufHdr->nFilledLen = 0;
                }

               pMyData->pCallbacks->FillBufferDone(pMyData->hSelf,
                                                      pMyData->pAppData,
                                                      pOutBufHdr);
               pOutBufHdr = NULL;

               if(EOSMarkerFlags)
               {
                    ALOGD("%s %d: EOS Event: [Emitted] Flags:0x%x",
                            __FUNCTION__,__LINE__,EOSMarkerFlags);

                    pMyData->pCallbacks->EventHandler(pMyData->hSelf, pMyData->pAppData,
                                                      OMX_EventBufferFlag, 0x1,
                                                      EOSMarkerFlags, NULL);
                    EOSMarkerFlags=0;
               }
            }
        }
     }

EXIT:

    return (void*)OMX_ErrorNone;
}


bcmOmxTimestampTable::bcmOmxTimestampTable(unsigned int n_size)
    :size(n_size), usage_count(0)
{
    pTimestamps = (bcmOmxTimestampEntry *) malloc(n_size * sizeof(bcmOmxTimestampEntry));
    memset(pTimestamps, 0, n_size * sizeof(bcmOmxTimestampEntry));
}

bcmOmxTimestampTable::~bcmOmxTimestampTable()
{
    free(pTimestamps);
}

uint32_t
bcmOmxTimestampTable::store(uint64_t orig_ts)
{
    unsigned int i;

    if (usage_count==size)
    {
        bcmOmxTimestampEntry *temp;
        ALOGD("%s: Increasing timestamp table size from %d to %d",__FUNCTION__, size, size + 32);
        temp = (bcmOmxTimestampEntry *) malloc((size + 32) * sizeof(bcmOmxTimestampEntry));
        memset(temp, 0, (size + 32) * sizeof(bcmOmxTimestampEntry));
        memcpy(temp, pTimestamps, size * sizeof(bcmOmxTimestampEntry));
        free(pTimestamps);
        pTimestamps = temp;
        size += 32;
    }

    for (i = 0; i < size; i++)
    {
        if(pTimestamps[i].entryUsed==false)
        {
            pTimestamps[i].entryUsed = true;
            pTimestamps[i].originalTimestamp = orig_ts;
            pTimestamps[i].convertedTimestamp = CONVERT_USEC_45KHZ(orig_ts);
            usage_count++;
            break;
        }
    }

    return CONVERT_USEC_45KHZ(orig_ts);
}

uint64_t
bcmOmxTimestampTable::retrieve(uint32_t conv_ts)
{
    unsigned int i;
    uint64_t orig_ts;

    for (i = 0; i < size; i++)
    {
        if (pTimestamps[i].convertedTimestamp==conv_ts)
        {
            pTimestamps[i].entryUsed = false;
            orig_ts = pTimestamps[i].originalTimestamp;
            usage_count--;
            break;
        }
    }

    if (i==size)
    {
        orig_ts = CONVERT_45KHZ_USEC(conv_ts);
        ALOGD("%s: Couldn't get original timestamp for %d",__FUNCTION__,conv_ts);
    }

    return orig_ts;

}

void
bcmOmxTimestampTable::flush()
{
    memset(pTimestamps, 0, size * sizeof(bcmOmxTimestampEntry));
    usage_count = 0;
}

bool
bcmOmxTimestampTable::isFull()
{
    ALOGD("%s: usage_count = %d",__FUNCTION__, usage_count);
    return (usage_count==size) ? true : false;
}


////// END OF FILE






