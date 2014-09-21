/******************************************************************************
 *    (c)2010-2013 Broadcom Corporation
 * 
 * This program is the proprietary software of Broadcom Corporation and/or its licensors,
 * and may only be used, duplicated, modified or distributed pursuant to the terms and
 * conditions of a separate, written license agreement executed between you and Broadcom
 * (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 * no license (express or implied), right to use, or waiver of any kind with respect to the
 * Software, and Broadcom expressly reserves all rights in and to the Software and all
 * intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 * HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 * NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.  
 *  
 * Except as expressly set forth in the Authorized License,
 *  
 * 1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 * secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 * and to use this information only in connection with your use of Broadcom integrated circuit products.
 *  
 * 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS" 
 * AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR 
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO 
 * THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES 
 * OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, 
 * LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION 
 * OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF 
 * USE OR PERFORMANCE OF THE SOFTWARE.
 * 
 * 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS 
 * LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR 
 * EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR 
 * USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT 
 * ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE 
 * LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF 
 * ANY LIMITED REMEDY.
 ******************************************************************************/


/******************************************************************
 *   INCLUDE FILES
 ******************************************************************/
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "OMX_Component.h"
#include "OMX_BCM_Common.h"
#include "gralloc.h"
#include "GraphicBuffer.h"

   
#include <utils/Log.h>
#include <utils/List.h>

#undef LOG_TAG
#define LOG_TAG "BCM.VIDEO.DECODER"
#define USE_ANB_PRIVATE_DATA 1
//#define LOG_NDEBUG 0


struct CodecProfileLevel {
    OMX_U32 mProfile;
    OMX_U32 mLevel;
};

static const CodecProfileLevel kProfileLevels[] = {
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

typedef struct _CODEC_TO_MIME_MAP_
{
    OMX_VIDEO_CODINGTYPE    CodingType;
    OMX_STRING              Mime;
}CODEC_TO_MIME_MAP,*PCODEC_TO_MIME_MAP;

static const CODEC_TO_MIME_MAP CodecToMIME[] = 
{
    {OMX_VIDEO_CodingUnused,            NULL                    },
    {OMX_VIDEO_CodingAutoDetect,        NULL                    },
    {OMX_VIDEO_CodingMPEG2,             "video/mpeg2"           },
    {OMX_VIDEO_CodingH263,              "video/3gpp"            },
    {OMX_VIDEO_CodingMPEG4,             "video/mp4v-es"         },
    {OMX_VIDEO_CodingWMV,               "video/wmv3"            },
    {OMX_VIDEO_CodingRV,                "video/real"            },
    {OMX_VIDEO_CodingAVC,               "video/avc"             },
    {OMX_VIDEO_CodingMJPEG,             "video/mjpeg"           },
    {OMX_VIDEO_CodingVP8,               "video/x-vnd.on2.vp8"   },
    {OMX_VIDEO_CodingVP9,               "video/x-vnd.on2.vp9"   },
    {OMX_VIDEO_CodingHEVC,              "video/hevc"            },
#ifdef OMX_EXTEND_CODECS_SUPPORT
    {OMX_VIDEO_CodingVC1,               "video/wvc1"            },
    {OMX_VIDEO_CodingSPARK,             "video/spark"           },
    {OMX_VIDEO_CodingDIVX311,           "video/divx"            },
    {OMX_VIDEO_CodingH265,              "video/hevc"            },
#endif
};


typedef struct _OMX_TO_NEXUS_MAP_
{
    OMX_VIDEO_CODINGTYPE   OMxCoding;
    NEXUS_VideoCodec       NexusCodec; 
}OMX_TO_NEXUS_MAP, *POMX_TO_NEXUS_MAP;

static const OMX_TO_NEXUS_MAP OMXToNexusTable[] = {
    { OMX_VIDEO_CodingUnused,       NEXUS_VideoCodec_eUnknown},
    { OMX_VIDEO_CodingAutoDetect,   NEXUS_VideoCodec_eNone},
    { OMX_VIDEO_CodingMPEG2,        NEXUS_VideoCodec_eMpeg2},
    { OMX_VIDEO_CodingH263,         NEXUS_VideoCodec_eH263},
    { OMX_VIDEO_CodingMPEG4,        NEXUS_VideoCodec_eMpeg4Part2},
    { OMX_VIDEO_CodingWMV,          NEXUS_VideoCodec_eVc1SimpleMain},
    { OMX_VIDEO_CodingRV,           NEXUS_VideoCodec_eRv40},
    { OMX_VIDEO_CodingAVC,          NEXUS_VideoCodec_eH264},
    { OMX_VIDEO_CodingMJPEG,        NEXUS_VideoCodec_eMotionJpeg},
    { OMX_VIDEO_CodingVP8,          NEXUS_VideoCodec_eVp8},
    { OMX_VIDEO_CodingVP9,          NEXUS_VideoCodec_eNone},
	{OMX_VIDEO_CodingHEVC,          NEXUS_VideoCodec_eH265},
#ifdef OMX_EXTEND_CODECS_SUPPORT
	{ OMX_VIDEO_CodingVC1,          NEXUS_VideoCodec_eVc1},
    { OMX_VIDEO_CodingSPARK,        NEXUS_VideoCodec_eSpark},
    { OMX_VIDEO_CodingDIVX311,      NEXUS_VideoCodec_eDivx311},
    { OMX_VIDEO_CodingH265,         NEXUS_VideoCodec_eH265},
#endif
};

typedef struct _PID_INFO_
{
    OMX_U32 pid;
} PID_INFO;

#define OMX_IndexEnableAndroidNativeGraphicsBuffer      0x7F000001
#define OMX_IndexGetAndroidNativeBufferUsage            0x7F000002
#define OMX_IndexStoreMetaDataInBuffers                 0x7F000003
#define OMX_IndexUseAndroidNativeBuffer                 0x7F000004
#define OMX_IndexUseAndroidNativeBuffer2                0x7F000005
#define OMX_IndexDisplayFrameBuffer                     0x7F000006
#define OMX_IndexProcessID                              0x7F000007

extern "C"
{

    OMX_ERRORTYPE OMX_ComponentInit(OMX_HANDLETYPE hComponent);

    OMX_ERRORTYPE OMX_VDEC_SendCommand(OMX_IN OMX_HANDLETYPE hComponent,
            OMX_IN OMX_COMMANDTYPE eCmd,
            OMX_IN OMX_U32 nParam, OMX_IN OMX_PTR pCmdData);

    OMX_ERRORTYPE OMX_VDEC_GetExtensionIndex(OMX_IN OMX_HANDLETYPE hComponent,
            OMX_IN OMX_STRING cParameterName, OMX_OUT OMX_INDEXTYPE * pIndexType);


    OMX_ERRORTYPE OMX_VDEC_GetParameter(OMX_IN OMX_HANDLETYPE hComponent,
            OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pParamStruct);

    OMX_ERRORTYPE OMX_VDEC_SetParameter(OMX_IN OMX_HANDLETYPE hComponent,
            OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pParamStruct);

    OMX_ERRORTYPE OMX_VDEC_EmptyThisBuffer(OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHdr);

    OMX_ERRORTYPE OMX_VDEC_UseBuffer(OMX_IN OMX_HANDLETYPE hComponent,
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes,
            OMX_IN OMX_U8* pBuffer);

    OMX_ERRORTYPE OMX_VDEC_UseANativeWindowBuffer(OMX_IN OMX_HANDLETYPE hComponent,
            OMX_INOUT OMX_BUFFERHEADERTYPE **bufferHeader,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes,
            const sp<ANativeWindowBuffer>& nativeBuffer);

    OMX_ERRORTYPE OMX_VDEC_AllocateBuffer(OMX_IN OMX_HANDLETYPE hComponent,
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes);

    OMX_ERRORTYPE OMX_VDEC_FreeBuffer(OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_U32 nPortIndex,
            OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHdr);


    OMX_ERRORTYPE OMX_VDEC_FillThisBuffer(OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE * pBufferHdr);


    OMX_ERRORTYPE OMX_VDEC_SetCallbacks(OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_CALLBACKTYPE* pCallbacks, 
            OMX_IN  OMX_PTR pAppData);

    OMX_ERRORTYPE OMX_VDEC_DeInit(OMX_IN  OMX_HANDLETYPE hComponent);

    OMX_ERRORTYPE OMX_VDEC_GetState(OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_OUT OMX_STATETYPE* pState);

    OMX_ERRORTYPE OMX_VDEC_GetConfig(OMX_IN OMX_HANDLETYPE hComponent,
            OMX_IN OMX_INDEXTYPE nIndex,
            OMX_INOUT OMX_PTR pComponentConfigStructure);

    OMX_ERRORTYPE OMX_VDEC_SetConfig(OMX_IN OMX_HANDLETYPE hComponent,
            OMX_IN OMX_INDEXTYPE nIndex,
            OMX_IN OMX_PTR pComponentConfigStructure);

}

static
NEXUS_VideoCodec
MapOMXVideoCodecToNexus(OMX_VIDEO_CODINGTYPE OMxCodec)
{
    return OMXToNexusTable[OMxCodec].NexusCodec;
}

static
OMX_STRING
GetMimeFromOmxCodingType(OMX_VIDEO_CODINGTYPE OMXCodingType)
{
    ALOGV("%s: Identified Mime Type Is %s",__FUNCTION__, CodecToMIME[OMXCodingType].Mime);
    return CodecToMIME[OMXCodingType].Mime;
}

extern "C" OMX_ERRORTYPE OMX_ComponentInit(OMX_HANDLETYPE hComponent)
{
    trace t(__FUNCTION__);

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pComp = NULL;
    BCM_OMX_CONTEXT *pMyData;
    OMX_U32 err;
    uint nIndex;

    pComp = (OMX_COMPONENTTYPE *) hComponent;

    BKNI_Init();

    // Create private data
    pMyData = (BCM_OMX_CONTEXT *)BKNI_Malloc(sizeof(BCM_OMX_CONTEXT));
    if(pMyData == NULL)
    {
        LOGE("Error in allocating mem for pMyData\n");
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    BKNI_Memset(pMyData, 0x0, sizeof(BCM_OMX_CONTEXT));

    pComp->pComponentPrivate = (OMX_PTR)pMyData;
    pMyData->state = OMX_StateLoaded;
    pMyData->hSelf = hComponent;
    pMyData->bSetStartUpTimeDone=false;

    ALOGD("%s %d: Component Handle :%p BUILD-DATE:%s BUILD-TIME:%s ",
            __FUNCTION__,__LINE__,hComponent,__DATE__,__TIME__);

    ALOGD("%s Use Output Buffers PrivateData: %s",__FUNCTION__
#ifdef USE_ANB_PRIVATE_DATA
            ,"enabled");
#else
            ,"disabled");
#endif

    pComp->SetParameter         = OMX_VDEC_SetParameter;        
    pComp->GetParameter         = OMX_VDEC_GetParameter;
    pComp->GetExtensionIndex    = OMX_VDEC_GetExtensionIndex;
    pComp->SendCommand          = OMX_VDEC_SendCommand;
    pComp->SetCallbacks         = OMX_VDEC_SetCallbacks;
    pComp->ComponentDeInit      = OMX_VDEC_DeInit;
    pComp->GetState             = OMX_VDEC_GetState;
    pComp->GetConfig            = OMX_VDEC_GetConfig;
    pComp->SetConfig            = OMX_VDEC_SetConfig;
    pComp->UseBuffer            = OMX_VDEC_UseBuffer;
    pComp->AllocateBuffer       = OMX_VDEC_AllocateBuffer;
    pComp->FreeBuffer           = OMX_VDEC_FreeBuffer;
    pComp->EmptyThisBuffer      = OMX_VDEC_EmptyThisBuffer;
    pComp->FillThisBuffer       = OMX_VDEC_FillThisBuffer;


    // Empty Buffer Done
    // Fill Buffer Done

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
    pMyData->sInPortDef.nBufferCountMin = NUM_IN_BUFFERS;
    pMyData->sInPortDef.nBufferAlignment = 1;
    pMyData->sInPortDef.nBufferCountActual = NUM_IN_BUFFERS;
    pMyData->sInPortDef.nBufferSize =  (OMX_U32) INPUT_BUFFER_SIZE;
    pMyData->sInPortDef.format.video.cMIMEType = GetMimeFromOmxCodingType(OMX_COMPRESSION_FORMAT);
    pMyData->sInPortDef.format.video.pNativeRender = NULL;
    pMyData->sInPortDef.format.video.nFrameWidth = 320;
    pMyData->sInPortDef.format.video.nFrameHeight = 176;
    pMyData->sInPortDef.format.video.nStride = pMyData->sInPortDef.format.video.nFrameWidth;
    pMyData->sInPortDef.format.video.nSliceHeight = pMyData->sInPortDef.format.video.nFrameHeight;
    pMyData->sInPortDef.format.video.nBitrate = 0;
    pMyData->sInPortDef.format.video.xFramerate = 0;
    pMyData->sInPortDef.format.video.bFlagErrorConcealment = OMX_FALSE;
    pMyData->sInPortDef.format.video.eCompressionFormat = OMX_COMPRESSION_FORMAT;
    pMyData->sInPortDef.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pMyData->sInPortDef.format.video.pNativeWindow = NULL;

    //Set The Input Port Format Structure
    OMX_CONF_INIT_STRUCT_PTR(&pMyData->sInPortFormat, OMX_VIDEO_PARAM_PORTFORMATTYPE);
    pMyData->sInPortFormat.eCompressionFormat = OMX_COMPRESSION_FORMAT;
    pMyData->sInPortFormat.eColorFormat = OMX_COLOR_FormatUnused;
    pMyData->sInPortFormat.nIndex=0;            //N-1 Where N Is the number of formats supported. We only Support One format for Now.
    pMyData->sInPortFormat.xFramerate=0;
    pMyData->sInPortFormat.nPortIndex=0;    

    //Set The Output Port Format Structure
    OMX_CONF_INIT_STRUCT_PTR(&pMyData->sOutPortFormat, OMX_VIDEO_PARAM_PORTFORMATTYPE);
    pMyData->sOutPortFormat.eCompressionFormat = OMX_VIDEO_CodingUnused;
    pMyData->sOutPortFormat.eColorFormat = OMX_COLOR_FormatMonochrome;
    pMyData->sOutPortFormat.nIndex=0;            //N-1 Where N Is the number of formats supported. We only Support One format for Now.
    pMyData->sOutPortFormat.xFramerate=0;
    pMyData->sOutPortFormat.nPortIndex=1;    

    // Initialize the video parameters for output port
    OMX_CONF_INIT_STRUCT_PTR(&pMyData->sOutPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    pMyData->sOutPortDef.nPortIndex = 0x1;
    pMyData->sOutPortDef.bEnabled = OMX_TRUE;
    pMyData->sOutPortDef.bPopulated = OMX_FALSE;
    pMyData->sOutPortDef.eDomain = OMX_PortDomainVideo;
    pMyData->sOutPortDef.eDir = OMX_DirOutput;
    pMyData->sOutPortDef.nBufferCountMin = NUM_OUT_BUFFERS; 
    pMyData->sOutPortDef.nBufferCountActual = NUM_OUT_BUFFERS;


    pMyData->sOutPortDef.format.video.cMIMEType = "video/raw";
    pMyData->sOutPortDef.format.video.pNativeRender = NULL;

    pMyData->sOutPortDef.format.video.nFrameWidth = OUT_VIDEO_BUFF_WIDTH;
    pMyData->sOutPortDef.format.video.nFrameHeight = OUT_VIDEO_BUFF_HEIGHT;

    pMyData->sOutPortDef.format.video.nStride = pMyData->sOutPortDef.format.video.nFrameWidth;
    pMyData->sOutPortDef.format.video.nSliceHeight = pMyData->sOutPortDef.format.video.nFrameHeight;
    pMyData->sOutPortDef.format.video.nBitrate = 0;
    pMyData->sOutPortDef.format.video.xFramerate = 0;
    pMyData->sOutPortDef.format.video.bFlagErrorConcealment = OMX_FALSE;
    pMyData->sOutPortDef.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;

#if ORIGINAL_CODE
    pMyData->sOutPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    pMyData->sOutPortDef.nBufferSize = (pMyData->sOutPortDef.format.video.nFrameWidth *
            pMyData->sOutPortDef.format.video.nFrameHeight * 3) / 2;

#else
    /* We are getting eColorFormat value as pixelFormat in Gralloc without proper conversion. Appropriate value of
       eColorFormat would be OMX_COLOR_Format16bitRGB565. However, pixelFormat 
       HAL_PIXEL_FORMAT_RGBA_5551 is not supported in Gralloc. Hence setting ecolorFormat to
       OMX_COLOR_FormatMonochrome, which has same value as of HAL_PIXEL_FORMAT_RGBA_8888
       in pixelFormat and supported in Gralloc. */
    pMyData->sOutPortDef.format.video.eColorFormat = OMX_COLOR_FormatMonochrome;
    pMyData->sOutPortDef.nBufferSize = pMyData->sOutPortDef.format.video.nFrameWidth *
        pMyData->sOutPortDef.format.video.nFrameHeight * 2;
#endif

    pMyData->sOutPortDef.format.video.pNativeWindow = NULL;

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

    // Initialize the output buffer list 
    BKNI_Memset(&(pMyData->sOutBufList), 0x0, sizeof(BufferList));
    pMyData->sOutBufList.pBufHdr = (OMX_BUFFERHEADERTYPE**) 
        BKNI_Malloc (sizeof(OMX_BUFFERHEADERTYPE*) * MAX_NUM_OUT_BUFFERS); 

    pMyData->pOutBufHdrRes= (OMX_BUFFERHEADERTYPE**) 
        BKNI_Malloc (sizeof(OMX_BUFFERHEADERTYPE*) * MAX_NUM_OUT_BUFFERS); 


    if(pMyData->sOutBufList.pBufHdr == NULL)
    {
        LOGE("Error in allocating mem for pMyData->sOutBufList.pBufHdr\n");
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

    /**************************************************************
      THIS CODE BLOCK SHOULD BE MOVED ----> Do This when Entering the
      Executing State...Cleanyup When Exiting Executing State
     ****************************************************************/ 

    pMyData->pAndVidWindow = new AndroidVideoWindow();

    pMyData->pOMXNxDecoder = new OMXNexusDecoder(LOG_TAG,
            MapOMXVideoCodecToNexus(OMX_COMPRESSION_FORMAT),
            pMyData->pAndVidWindow);

    pMyData->pTstable = new bcmOmxTimestampTable(MAX_NUM_TIMESTAMPS);


    if (OMX_COMPRESSION_FORMAT == OMX_VIDEO_CodingWMV)
    {
        pMyData->pPESFeeder = new PESFeeder(LOG_TAG,
                            PES_VIDEO_PID,
                            NUM_IN_DESCRIPTORS,
                            NEXUS_TransportType_eAsf,
                            MapOMXVideoCodecToNexus(OMX_COMPRESSION_FORMAT));
    }
    else 
    {
        pMyData->pPESFeeder = new PESFeeder(LOG_TAG,
                            PES_VIDEO_PID,
                            NUM_IN_DESCRIPTORS,
                            NEXUS_TransportType_eMpeg2Pes,
                            MapOMXVideoCodecToNexus(OMX_COMPRESSION_FORMAT));
    }


    pMyData->pOMXNxDecoder->RegisterDecoderEventListener(pMyData->pPESFeeder);
    pMyData->pPESFeeder->RegisterFeederEventsListener(pMyData->pOMXNxDecoder);


    if(false == pMyData->pPESFeeder->StartDecoder(pMyData->pOMXNxDecoder))
    {
        ALOGE("%s: Failed To Start The Decoder",__FUNCTION__);
        goto EXIT;
    }

    /*****************************************************************************/        
    return eError;

EXIT:
    OMX_VDEC_DeInit(hComponent);
    return eError;
}

extern "C" OMX_ERRORTYPE OMX_VDEC_DeInit(OMX_IN  OMX_HANDLETYPE hComponent)
{

    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    ThrCmdType eCmd = Stop;
    OMX_U32 nIndex = 0;
    NEXUS_PlaypumpStatus playpumpStatus;

    trace t(__FUNCTION__);

    ALOGV("%s %d: Component Handle :%p BUILD-DATE:%s BUILD-TIME:%s",
            __FUNCTION__,__LINE__,hComponent,__DATE__,__TIME__);

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    // In case the client crashes, check for nAllocSize parameter.
    // If this is greater than zero, there are elements in the list that are not free'd.
    // In that case, free the elements.

    if (pMyData != NULL)
    {
        if (pMyData->sInBufList.nAllocSize > 0)
            ListFreeAllBuffers(pMyData->sInBufList, nIndex)  

                if (pMyData->sOutBufList.nAllocSize > 0)
                    ListFreeAllBuffers(pMyData->sOutBufList, nIndex)   

                        // Put the command and data in the pipe 
                        if(0!= pMyData->cmdpipe[1])
                            write(pMyData->cmdpipe[1], &eCmd, sizeof(eCmd));

        if(0!= pMyData->cmddatapipe[1])
            write(pMyData->cmddatapipe[1], &eCmd, sizeof(eCmd));

        // Wait for thread to exit so we can get the status into "error"
        if(0 != pMyData->thread_id)
            pthread_join(pMyData->thread_id, (void**)&eError);

        // close the pipe handles 
        if(pMyData->cmdpipe[0])
            close(pMyData->cmdpipe[0]);

        if(pMyData->cmdpipe[1])
            close(pMyData->cmdpipe[1]);

        if(pMyData->cmddatapipe[0])
            close(pMyData->cmddatapipe[0]);

        if(pMyData->cmddatapipe[1])
            close(pMyData->cmddatapipe[1]);

        if(pMyData->pAndVidWindow)
        {
            delete pMyData->pAndVidWindow;
            pMyData->pAndVidWindow=NULL;
        }

        if(pMyData->pPESFeeder)
        {
            delete pMyData->pPESFeeder;
            pMyData->pPESFeeder = NULL;
        }

        if(pMyData->pOMXNxDecoder)
        {
            delete pMyData->pOMXNxDecoder;
            pMyData->pOMXNxDecoder=NULL;
        }

        if(pMyData->pTstable)
        {
            delete pMyData->pTstable;
            pMyData->pTstable = NULL;
        }

        BKNI_Free(pMyData);

        ((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate=NULL;
    }

    return eError;
}


extern "C" OMX_ERRORTYPE OMX_VDEC_SendCommand(OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_COMMANDTYPE eCmd,
        OMX_IN OMX_U32 nParam, OMX_IN OMX_PTR pCmdData)
{

    trace t(__FUNCTION__);
    LOGV(" OMX_VDEC_SendCommand cmd is %d",eCmd);

    BCM_OMX_CONTEXT *pMyData;
    ThrCmdType eCmd1;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, 1, 1);
    if (eCmd == OMX_CommandMarkBuffer) 
        OMX_CONF_CHECK_CMD(pCmdData, 1, 1);

    if (pMyData->state == OMX_StateInvalid)
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInvalidState);

    switch (eCmd)
    {
        case OMX_CommandStateSet: 
            eCmd1 = SetState;
            break;
        case OMX_CommandFlush: 
            eCmd1 = Flush;
            if ((int)nParam > 1 && (int)nParam != -1)
                OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadPortIndex);
            break;
        case OMX_CommandPortDisable: 
            eCmd1 = StopPort;
            break;
        case OMX_CommandPortEnable:
            eCmd1 = RestartPort;
            break;
        case OMX_CommandMarkBuffer:
            eCmd1 = MarkBuf;
            if (nParam > 0)
                OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadPortIndex);
            break;
        default:
            break;
    }

    write(pMyData->cmdpipe[1], &eCmd, sizeof(eCmd));

    // In case of MarkBuf, the pCmdData parameter is used to carry the data.
    // In other cases, the nParam1 parameter carries the data.    
    if(eCmd == MarkBuf)
        write(pMyData->cmddatapipe[1], &pCmdData, sizeof(OMX_PTR));
    else 
        write(pMyData->cmddatapipe[1], &nParam, sizeof(nParam));

OMX_CONF_CMD_BAIL:
    return eError; 
}

extern "C" OMX_ERRORTYPE OMX_VDEC_GetState(OMX_IN  OMX_HANDLETYPE hComponent,
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
extern "C" OMX_ERRORTYPE OMX_VDEC_GetExtensionIndex(OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_STRING cParameterName, OMX_OUT OMX_INDEXTYPE * pIndexType)
{
    trace t(__FUNCTION__);
    ALOGV("%s: %s\n",__FUNCTION__, cParameterName);
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if(0 == strcmp("OMX.google.android.index.enableAndroidNativeBuffers", cParameterName))
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexEnableAndroidNativeGraphicsBuffer;

    else if(0 == strcmp("OMX.google.android.index.getAndroidNativeBufferUsage", cParameterName))
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexGetAndroidNativeBufferUsage;

//  else if(0 == strcmp("OMX.google.android.index.storeMetaDataInBuffers", cParameterName))
//      *pIndexType = (OMX_INDEXTYPE)OMX_IndexStoreMetaDataInBuffers;

    else if(0 == strcmp("OMX.google.android.index.useAndroidNativeBuffer", cParameterName))
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexUseAndroidNativeBuffer;
    else
    {
        ALOGE("[%s][%s][%d]Returning UnSupported Index For %s",__FILE__,__FUNCTION__,__LINE__,cParameterName);
        eError =  OMX_ErrorUnsupportedIndex;
    }
    return eError;
}

extern "C" OMX_ERRORTYPE OMX_VDEC_GetConfig(OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_INDEXTYPE nIndex,OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    ALOGV("%s: ENTER With Index:%d \n",__FUNCTION__,nIndex);
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    //    switch (nIndex)
    //    {
    //        case OMX_IndexConfigCommonOutputCrop:
    //        {
    //            OMX_CONFIG_RECTTYPE *rectParams = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;
    //            rectParams->nLeft       = 10;
    //            rectParams->nTop        = 10;
    //            rectParams->nWidth      = 100;
    //            rectParams->nHeight     = 100;
    //            eError                  = OMX_ErrorNone;
    //            break;
    //        }
    //
    //        default:
    //        {
    //            eError = OMX_ErrorUnsupportedIndex;
    //            break;
    //        }
    //    }

    ALOGV("%s: EXIT \n",__FUNCTION__);
    return OMX_ErrorUndefined;
}

extern "C" OMX_ERRORTYPE OMX_VDEC_SetConfig(OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_INDEXTYPE nIndex,OMX_IN OMX_PTR pComponentConfigStructure)
{
    ALOGV("%s: ENTER \n",__FUNCTION__);
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    ALOGV("%s: EXIT \n",__FUNCTION__);
    return OMX_ErrorUndefined;

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


extern "C" OMX_ERRORTYPE OMX_VDEC_GetParameter(OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pParamStruct)

{
    trace t(__FUNCTION__);

    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, pParamStruct, 1);

    if (pMyData->state == OMX_StateInvalid)
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);

    switch (nParamIndex)
    {
        case OMX_IndexParamVideoInit:
            BKNI_Memcpy(pParamStruct, &pMyData->sPortParam, sizeof(OMX_PORT_PARAM_TYPE));
            break;

        case OMX_IndexParamPortDefinition:

            if (((OMX_PARAM_PORTDEFINITIONTYPE *)(pParamStruct))->nPortIndex == pMyData->sInPortDef.nPortIndex)
            {
                //PrintPortDefInfo(&pMyData->sInPortDef);
                BKNI_Memcpy(pParamStruct, &pMyData->sInPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));

            }else if (((OMX_PARAM_PORTDEFINITIONTYPE *)(pParamStruct))->nPortIndex == pMyData->sOutPortDef.nPortIndex) {
                //PrintPortDefInfo(&pMyData->sOutPortDef);
               BKNI_Memcpy(pParamStruct, &pMyData->sOutPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));

            } else {
                eError = OMX_ErrorBadPortIndex;
            }

            break;

        case OMX_IndexParamPriorityMgmt:
            ALOGV("%s: OMX_IndexParamPriorityMgmt",__FUNCTION__);
            BKNI_Memcpy(pParamStruct, &pMyData->sPriorityMgmt, sizeof(OMX_PRIORITYMGMTTYPE));
            break;

        case OMX_IndexParamVideoMpeg2:
            ALOGV("%s: OMX_IndexParamVideoMpeg2",__FUNCTION__);
            if (((OMX_VIDEO_PARAM_MPEG2TYPE *)(pParamStruct))->nPortIndex == pMyData->sMpeg2.nPortIndex)
                BKNI_Memcpy(pParamStruct, &pMyData->sMpeg2, sizeof(OMX_VIDEO_PARAM_MPEG2TYPE));
            else
                eError = OMX_ErrorBadPortIndex;
            break;

        case OMX_IndexParamVideoProfileLevelQuerySupported:
            {

                ALOGV("%s: OMX_IndexParamVideoProfileLevelQuerySupported",__FUNCTION__);
                OMX_VIDEO_PARAM_PROFILELEVELTYPE *profileLevel =
                    (OMX_VIDEO_PARAM_PROFILELEVELTYPE *) pParamStruct;

                if (profileLevel->nPortIndex != pMyData->sInPortDef.nPortIndex)
                {
                    ALOGE("Invalid port index: %ld", profileLevel->nPortIndex);
                    return OMX_ErrorUnsupportedIndex;
                }

                size_t index = profileLevel->nProfileIndex;
                size_t nProfileLevels =
                    sizeof(kProfileLevels) / sizeof(kProfileLevels[0]);
                if (index >= nProfileLevels)
                {
                    return OMX_ErrorNoMore;
                }
                profileLevel->eProfile = kProfileLevels[index].mProfile;
                profileLevel->eLevel = kProfileLevels[index].mLevel;
                return OMX_ErrorNone;
            } 
        case OMX_IndexParamVideoPortFormat:
            {
                ALOGV("%s: OMX_IndexParamVideoPortFormat",__FUNCTION__);
                OMX_VIDEO_PARAM_PORTFORMATTYPE *pFormatParams=
                    (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pParamStruct; 

                if (pFormatParams->nPortIndex == pMyData->sInPortDef.nPortIndex)
                {
                    BKNI_Memcpy(pFormatParams,&pMyData->sInPortFormat,sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
                    ALOGV("%s: OMX_IndexParamVideoPortFormat InputPort Returning CompFmt:%d ColFmt:%d Index:%d PortIndex:%d xFrameRate:%d",
                            __FUNCTION__,
                            pFormatParams->eCompressionFormat,
                            pFormatParams->eColorFormat,
                            pFormatParams->nIndex,
                            pFormatParams->nPortIndex,
                            pFormatParams->xFramerate);

                }else{
                    /* Support only one color format (nIndex = 0). Return error on subsequent color format queries (nIndex > 0). */
                    if (pFormatParams->nIndex > 0)
                        return OMX_ErrorUnsupportedSetting;

                    BKNI_Memcpy(pFormatParams,&pMyData->sOutPortFormat,sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
                    ALOGV("%s: OMX_IndexParamVideoPortFormat OutPort Returning CompFmt:%d ColFmt:%d Index:%d PortIndex:%d xFrameRate:%d",
                            __FUNCTION__,
                            pFormatParams->eCompressionFormat,
                            pFormatParams->eColorFormat,
                            pFormatParams->nIndex,
                            pFormatParams->nPortIndex,
                            pFormatParams->xFramerate);
                }
                return OMX_ErrorNone;
            }
            break;
        case OMX_IndexGetAndroidNativeBufferUsage:
            ALOGV("%s: OMX_IndexGetAndroidNativeBufferUsage\n",__FUNCTION__);
            if (pMyData->sNativeBufParam.enable!=OMX_TRUE)
            {
                ALOGE("OMX_IndexGetAndroidNativeBufferUsage is called without calling OMX_IndexEnableAndroidNativeGraphicsBuffer");
                return OMX_ErrorUndefined;
            }
            ((GetAndroidNativeBufferUsageParams *)pParamStruct)->nUsage =
                    GRALLOC_USAGE_EXTERNAL_DISP | GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN;
            break;
        case OMX_IndexProcessID:
            ALOGV("%s: OMX_IndexProcessID\n",__FUNCTION__);
            ((PID_INFO*)pParamStruct)->pid = getpid();
            return OMX_ErrorNone;
            break;
        default:
            ALOGE("%s:  UNHANDLED PARAMETER INDEX %d",__FUNCTION__, nParamIndex);
            eError = OMX_ErrorUnsupportedIndex;
            break;
    }

OMX_CONF_CMD_BAIL:  
    return eError;
}


extern "C" OMX_ERRORTYPE OMX_VDEC_SetParameter(OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pParamStruct)
{
    trace t(__FUNCTION__);
    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, pParamStruct, 1);

    // Allow case of OMX_IndexDisplayFrameBuffer outside OMX_StateLoaded. This case is used
    // temporarily to display frames in media codec path. This will get moved out to seperate function
    // later.
    if ((pMyData->state != OMX_StateLoaded)&&(nParamIndex!=OMX_IndexDisplayFrameBuffer))
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);

    switch (nParamIndex)
    {
        case OMX_IndexParamVideoInit:
        {
            ALOGV("%s: OMX_IndexParamVideoInit",__FUNCTION__);
            BKNI_Memcpy(&pMyData->sPortParam, pParamStruct, sizeof(OMX_PORT_PARAM_TYPE));
            break;
        }
        case OMX_IndexParamPortDefinition:
        {
            ALOGV("%s: OMX_IndexParamPortDefinition",__FUNCTION__);
            PrintPortDefInfo((OMX_PARAM_PORTDEFINITIONTYPE *)(pParamStruct));
            if ( ((OMX_PARAM_PORTDEFINITIONTYPE *)(pParamStruct))->nPortIndex  == pMyData->sInPortDef.nPortIndex)
            {
                BKNI_Memcpy(&pMyData->sInPortDef, pParamStruct, sizeof(OMX_PARAM_PORTDEFINITIONTYPE)); 

            }else if ( ( (OMX_PARAM_PORTDEFINITIONTYPE *)(pParamStruct))->nPortIndex == pMyData->sOutPortDef.nPortIndex) {
                //
                // You cannot blindly copy the data You will need to raise a event here 
                // Saying port setting changed if the parameters are not same
                // 
                OMX_PARAM_PORTDEFINITIONTYPE * pNewPortSettings = (OMX_PARAM_PORTDEFINITIONTYPE *)pParamStruct;

                size_t OriginalBuffSz = pMyData->sOutPortDef.nBufferSize;

                if (OriginalBuffSz > pNewPortSettings->nBufferSize)
                {
                    ALOGV("%s:OutputPort Port : CurrentBuffSz:%d NewBuffSz:%d. We are already at minimum, Returning OMX_ErrorUnsupportedSetting",
                        __FUNCTION__, OriginalBuffSz, pNewPortSettings->nBufferSize);

                    OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorUnsupportedSetting);
                }

                BKNI_Memcpy(&pMyData->sOutPortDef, pParamStruct, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
                // Frame width and height are used to setup ANtaiveWindow and allocate memory for those buffers.
                // We need to keep the frame width and height constant.

                ALOGV("%s:OutputPort Port : NumOutBuffs[Min]:%d NumOutBuffs[Actual]:%d Width:%d Height:%d BuffSz:%d",
                        __FUNCTION__,
                        pMyData->sOutPortDef.nBufferCountMin,
                        pMyData->sOutPortDef.nBufferCountActual,
                        pMyData->sOutPortDef.format.video.nFrameWidth,
                        pMyData->sOutPortDef.format.video.nFrameHeight,
                        pMyData->sOutPortDef.nBufferSize);

#if 0
                pMyData->sOutPortDef.format.video.nFrameWidth = OUT_VIDEO_BUFF_WIDTH;
                pMyData->sOutPortDef.format.video.nFrameHeight = OUT_VIDEO_BUFF_HEIGHT;
                pMyData->sOutPortDef.format.video.nStride = pMyData->sOutPortDef.format.video.nFrameWidth;
                pMyData->sOutPortDef.format.video.nSliceHeight = pMyData->sOutPortDef.format.video.nFrameHeight;

#endif // 0

                //pMyData->sOutPortDef.format.video.nFrameWidth = ((OMX_PARAM_PORTDEFINITIONTYPE *)(pParamStruct))->format.video.nFrameWidth;
                //pMyData->sOutPortDef.format.video.nFrameHeight != ((OMX_PARAM_PORTDEFINITIONTYPE *)(pParamStruct))->format.video.nFrameHeight;
                //pMyData->sOutPortDef.format.video.nStride = pMyData->sOutPortDef.format.video.nFrameWidth;
                //pMyData->sOutPortDef.format.video.nSliceHeight = pMyData->sOutPortDef.format.video.nFrameHeight;

                //                }

            }else { 
                eError = OMX_ErrorBadPortIndex; 
            }

            break;
        }

        case OMX_IndexParamPriorityMgmt:
        {
            ALOGV("%s: OMX_IndexParamPriorityMgmt",__FUNCTION__);
            BKNI_Memcpy(&pMyData->sPriorityMgmt, pParamStruct, sizeof (OMX_PRIORITYMGMTTYPE));
            break;
        }
        case OMX_IndexParamVideoMpeg2:
        {
            ALOGV("%s: OMX_IndexParamVideoMpeg2",__FUNCTION__);
            if (((OMX_VIDEO_PARAM_MPEG2TYPE *)(pParamStruct))->nPortIndex == pMyData->sMpeg2.nPortIndex)
                BKNI_Memcpy(&pMyData->sMpeg2, pParamStruct, sizeof (OMX_VIDEO_PARAM_MPEG2TYPE));
            else
                eError = OMX_ErrorBadPortIndex;
            break;
        }

        case OMX_IndexParamVideoPortFormat:
        {
            ALOGV("%s: OMX_IndexParamVideoPortFormat",__FUNCTION__);
            OMX_VIDEO_PARAM_PORTFORMATTYPE *pFormatParams =
                (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pParamStruct;


            if (pFormatParams->nPortIndex == pMyData->sOutPortDef.nPortIndex)
            {
                ALOGV("%s: OMX_IndexParamVideoPortFormat Output Port Setting CompFmt:%d ColFmt:%d Index:%d PortIndex:%d xFrameRate:%d",
                        __FUNCTION__,
                        pFormatParams->eCompressionFormat,
                        pFormatParams->eColorFormat,
                        pFormatParams->nIndex,
                        pFormatParams->nPortIndex,
                        pFormatParams->xFramerate);

                BKNI_Memcpy(&pMyData->sOutPortFormat,pFormatParams,sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));

            }else if(pFormatParams->nPortIndex == pMyData->sInPortDef.nPortIndex){

                ALOGV("%s: OMX_IndexParamVideoPortFormat InputPort Setting CompFmt:%d ColFmt:%d Index:%d PortIndex:%d xFrameRate:%d",
                        __FUNCTION__,
                        pFormatParams->eCompressionFormat,
                        pFormatParams->eColorFormat,
                        pFormatParams->nIndex,
                        pFormatParams->nPortIndex,
                        pFormatParams->xFramerate);

                BKNI_Memcpy(&pMyData->sInPortFormat,pFormatParams,sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
            }else{
                return OMX_ErrorUndefined;
            }

            return OMX_ErrorNone;
            //break;
        }   

        case OMX_IndexEnableAndroidNativeGraphicsBuffer:
        {
            ALOGV("%s: OMX_IndexEnableAndroidNativeGraphicsBuffer\n",__FUNCTION__);
            ALOGV("%s, nSize = %lu, nPortIndex = %lu", __FUNCTION__,((struct EnableAndroidNativeBuffersParams  *)pParamStruct)->nSize,
                    ((struct EnableAndroidNativeBuffersParams  *)pParamStruct)->nPortIndex);
            BKNI_Memcpy(&pMyData->sNativeBufParam, pParamStruct, sizeof(struct EnableAndroidNativeBuffersParams));
            break;
        }

        case OMX_IndexStoreMetaDataInBuffers:
        {
            ALOGV("%s: OMX_IndexStoreMetaDataInBuffers\n",__FUNCTION__);
            struct PrepareForAdaptivePlaybackParams *pAdaptivePlayStruct=NULL;
            pAdaptivePlayStruct = (struct PrepareForAdaptivePlaybackParams *) pParamStruct;
            ALOGD("%s[%d]: AdaptivePlayback Enable:%d MaxWidth:%d MaxHeight:%d",
                  __FUNCTION__,__LINE__,
                  pAdaptivePlayStruct->bEnable,
                  pAdaptivePlayStruct->nMaxFrameWidth,
                  pAdaptivePlayStruct->nMaxFrameHeight);
            //BCMOMX_DBG_ASSERT(false);
            break;
        }
        case OMX_IndexUseAndroidNativeBuffer:
        {
            ALOGV("%s: OMX_IndexUseAndroidNativeBuffer\n", __FUNCTION__);
            eError = OMX_VDEC_UseANativeWindowBuffer(hComponent,
                    ((struct UseAndroidNativeBufferParams  *)pParamStruct)->bufferHeader,
                    ((struct UseAndroidNativeBufferParams  *)pParamStruct)->nPortIndex,
                    ((struct UseAndroidNativeBufferParams  *)pParamStruct)->pAppPrivate,
                    ((struct UseAndroidNativeBufferParams  *)pParamStruct)->nSize,
                    ((struct UseAndroidNativeBufferParams  *)pParamStruct)->nativeBuffer);
            break;
        }
        case OMX_IndexUseAndroidNativeBuffer2:
        {
            ALOGV("%s: OMX_IndexUseAndroidNativeBuffer2\n",__FUNCTION__);
            return OMX_ErrorUnsupportedSetting;
            break;
        }
        case OMX_IndexDisplayFrameBuffer:
        {
            // Display video frames for media codec path.
            // This code should moved out of OMX_VDEC_SetParameter later.
            PDISPLAY_FRAME pDispFrame = (PDISPLAY_FRAME)pParamStruct;
            pDispFrame->pDisplayFn(pDispFrame, pDispFrame->DisplayCnxt);
            break;
        }
        default: // one case for enable graphics if required check ????
        {
            //eError = OMX_ErrorUnsupportedIndex;

            ALOGE("%s: UNHANDLED PARAMETER INDEX %d",__FUNCTION__,nParamIndex);
            eError = OMX_ErrorNone;
            break;
        }
    }

OMX_CONF_CMD_BAIL:  
    return eError;
}

extern "C" OMX_ERRORTYPE OMX_VDEC_UseBuffer(OMX_IN OMX_HANDLETYPE hComponent,
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
    OMX_U32 nIndex = 0x0;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, ppBufferHdr, pBuffer);

    if (nPortIndex == pMyData->sInPortDef.nPortIndex)
    {
        pPortDef = &pMyData->sInPortDef;
    } else if (nPortIndex == pMyData->sOutPortDef.nPortIndex) {
        pPortDef = &pMyData->sOutPortDef;
    } else {
        ALOGE("%s %d: Invalid Port Index !!",__FUNCTION__,__LINE__);
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadParameter);
    }

    if (!pPortDef->bEnabled)
    {
        ALOGE("%s %d: Invalid Port Index [Curr State :%d]" 
              "\n =============PRINTING PORT INFO ON ERROR ==============="
            , __FUNCTION__, __LINE__, pMyData->state);
        PrintPortDefInfo(pPortDef);
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);
    }

    if (nSizeBytes != pPortDef->nBufferSize || pPortDef->bPopulated)
    {
        ALOGE("%s %d: Buff Sz Mismatch Or Port NotPopulated [Curr State :%d]"
            "nSizeBytes:%d, pPortDef->nBufferSize:%d pPortDef->bPopulated:%d"
            "\n =============PRINTING PORT INFO ON ERROR ==============="
            , __FUNCTION__, __LINE__, pMyData->state, nSizeBytes, pPortDef->nBufferSize,pPortDef->bPopulated);
        PrintPortDefInfo(pPortDef);
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadParameter);
    }

    // Find an empty position in the BufferList and allocate memory for the buffer header. 
    // Use the buffer passed by the client to initialize the actual buffer 
    // inside the buffer header. 
    if (nPortIndex == pMyData->sInPortDef.nPortIndex)
    {
        ListAllocate(pMyData->sInBufList, nIndex);
        if (pMyData->sInBufList.pBufHdr[nIndex] == NULL)
        {         
            pMyData->sInBufList.pBufHdr[nIndex] = (OMX_BUFFERHEADERTYPE*) 
                malloc(sizeof(OMX_BUFFERHEADERTYPE)) ;
            if (!pMyData->sInBufList.pBufHdr[nIndex])
                OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);  

            OMX_CONF_INIT_STRUCT_PTR (pMyData->sInBufList.pBufHdr[nIndex], OMX_BUFFERHEADERTYPE);
        }  
        pMyData->sInBufList.pBufHdr[nIndex]->pBuffer = pBuffer;
        LoadBufferHeader(pMyData->sInBufList, pMyData->sInBufList.pBufHdr[nIndex], pAppPrivate, 
                nSizeBytes, nPortIndex, *ppBufferHdr, pPortDef);    
    }
    else
    {
        ListAllocate(pMyData->sOutBufList,  nIndex);
        if (pMyData->sOutBufList.pBufHdr[nIndex] == NULL)
        {         
            pMyData->sOutBufList.pBufHdr[nIndex] = (OMX_BUFFERHEADERTYPE*) 
                malloc(sizeof(OMX_BUFFERHEADERTYPE));
            if (!pMyData->sOutBufList.pBufHdr[nIndex])
                OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);  

            OMX_CONF_INIT_STRUCT_PTR (pMyData->sOutBufList.pBufHdr[nIndex], OMX_BUFFERHEADERTYPE);
        }  
        pMyData->sOutBufList.pBufHdr[nIndex]->pBuffer = pBuffer; 
        LoadBufferHeader(pMyData->sOutBufList, pMyData->sOutBufList.pBufHdr[nIndex],  
                pAppPrivate, nSizeBytes, nPortIndex, *ppBufferHdr, pPortDef);
        CopyBufferHeaderList(pMyData->sOutBufList, pPortDef, pMyData->pOutBufHdrRes);

    }  

OMX_CONF_CMD_BAIL:  
    return eError;
}

extern "C" OMX_ERRORTYPE OMX_VDEC_UseANativeWindowBuffer(OMX_IN OMX_HANDLETYPE hComponent,
        OMX_INOUT OMX_BUFFERHEADERTYPE **bufferHeader,
        OMX_IN OMX_U32 nPortIndex,
        OMX_IN OMX_PTR pAppPrivate,
        OMX_IN OMX_U32 nSizeBytes,
        const sp<ANativeWindowBuffer>& nativeBuffer)
{
    trace t(__FUNCTION__);
    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    OMX_U32 nIndex = 0x0;
    GraphicBuffer *pGraphicBuffer;
    PDISPLAY_FRAME pDispFr;

    ALOGV("Ajitabh: %s, nPortIndex = %lu, nSizeBytes = %lu Format:%d NativeBuffer->Width:%d NativeBuffer->Height:%d NativeWindowBufferHandle:%p",
            __FUNCTION__, nPortIndex, nSizeBytes, nativeBuffer->format, nativeBuffer->width, nativeBuffer->height, nativeBuffer->handle);

    if (!hComponent) {
        ALOGE("ERROR at %s line %d",__FUNCTION__,__LINE__);
        return OMX_ErrorBadParameter;
    }

    if (!bufferHeader) {
        ALOGE("ERROR at %s line %d",__FUNCTION__,__LINE__);
        return OMX_ErrorBadParameter;
    }

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    //OMX_CONF_CHECK_CMD(pMyData, bufferHeader, nativeBuffer);

    if (!pMyData) {
        ALOGE("ERROR at %s line %d",__FUNCTION__,__LINE__);
        return OMX_ErrorBadParameter;
    }

    if (nPortIndex == pMyData->sInPortDef.nPortIndex)
        pPortDef = &pMyData->sInPortDef;
    else if (nPortIndex == pMyData->sOutPortDef.nPortIndex)
        pPortDef = &pMyData->sOutPortDef;
    else
    {
        ALOGE("ERROR at %s line %d",__FUNCTION__,__LINE__);
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadParameter);
    }

    if (!pPortDef) {
        ALOGE("ERROR at %s line %d",__FUNCTION__,__LINE__);
        return OMX_ErrorBadParameter;
    }

    if (!pPortDef->bEnabled)
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);


    // Find an empty position in the BufferList and allocate memory for the buffer header. 
    // Use the buffer passed by the client to initialize the actual buffer 
    // inside the buffer header. 
    ListAllocate(pMyData->sOutBufList,  nIndex);
    if (pMyData->sOutBufList.pBufHdr[nIndex] == NULL)
    {         
        pMyData->sOutBufList.pBufHdr[nIndex] = (OMX_BUFFERHEADERTYPE*) 
            malloc(sizeof(OMX_BUFFERHEADERTYPE));
        if (!pMyData->sOutBufList.pBufHdr[nIndex])
            OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);  

        OMX_CONF_INIT_STRUCT_PTR (pMyData->sOutBufList.pBufHdr[nIndex], OMX_BUFFERHEADERTYPE);
    }  


    BCMOMX_DBG_ASSERT(pMyData->sNativeBufParam.enable==OMX_TRUE);

    pGraphicBuffer = new GraphicBuffer(OUT_VIDEO_BUFF_WIDTH,//nativeBuffer->width,
            OUT_VIDEO_BUFF_HEIGHT,//nativeBuffer->height,
            nativeBuffer->format,
            nativeBuffer->usage,
            nativeBuffer->stride,
            (native_handle_t*) nativeBuffer->handle,
            false);

#ifdef USE_ANB_PRIVATE_DATA
    pMyData->sOutBufList.pBufHdr[nIndex]->pBuffer = 
       (OMX_U8*) GetDisplayFrameFromANB(pGraphicBuffer->getNativeBuffer());
    pDispFr = (PDISPLAY_FRAME)pMyData->sOutBufList.pBufHdr[nIndex]->pBuffer;
    memset(pDispFr, 0, sizeof(DISPLAY_FRAME));
#else
    pGraphicBuffer->lock(GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_OFTEN, (void**)&pMyData->sOutBufList.pBufHdr[nIndex]->pBuffer);
    pDispFr = (PDISPLAY_FRAME)pMyData->sOutBufList.pBufHdr[nIndex]->pBuffer;
    memset(pDispFr, 0, sizeof(DISPLAY_FRAME));
    pGraphicBuffer->unlock();
#endif

    pMyData->sOutBufList.pBufHdr[nIndex]->pInputPortPrivate = (OMX_PTR) pGraphicBuffer;
    pMyData->sOutBufList.pBufHdr[nIndex]->pPlatformPrivate = (OMX_PTR) &nativeBuffer;

    LoadBufferHeader(pMyData->sOutBufList, pMyData->sOutBufList.pBufHdr[nIndex],  
            pAppPrivate, nSizeBytes, nPortIndex, *bufferHeader, pPortDef);

    CopyBufferHeaderList(pMyData->sOutBufList, pPortDef, pMyData->pOutBufHdrRes);
OMX_CONF_CMD_BAIL:  
    return eError;
}


extern "C" OMX_ERRORTYPE OMX_VDEC_AllocateBuffer(OMX_IN OMX_HANDLETYPE hComponent,
        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
        OMX_IN OMX_U32 nPortIndex,
        OMX_IN OMX_PTR pAppPrivate,
        OMX_IN OMX_U32 nSizeBytes)
{
    trace t(__FUNCTION__);

    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_S8 nIndex = 0x0;
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, ppBufferHdr, 1);


    if (nPortIndex == pMyData->sInPortDef.nPortIndex)
    {
        pPortDef = &pMyData->sInPortDef;
    }else if (nPortIndex == pMyData->sOutPortDef.nPortIndex){
        pPortDef = &pMyData->sOutPortDef;
    }else{
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadParameter);
    }

    if (!pPortDef->bEnabled)
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);

    if (nSizeBytes != pPortDef->nBufferSize || pPortDef->bPopulated)
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadParameter);  

    // Find an empty position in the BufferList and allocate memory for the buffer header 
    // and the actual buffer 
    if (nPortIndex == pMyData->sInPortDef.nPortIndex)
    {
        NEXUS_Error errCode;
        ListAllocate(pMyData->sInBufList,  nIndex);

        //First Allocate The Buffer Header
        if (pMyData->sInBufList.pBufHdr[nIndex] == NULL)
        {         
            pMyData->sInBufList.pBufHdr[nIndex] = (OMX_BUFFERHEADERTYPE*) 
                malloc(sizeof(OMX_BUFFERHEADERTYPE)) ;

            if (!pMyData->sInBufList.pBufHdr[nIndex])
                OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);  

            OMX_CONF_INIT_STRUCT_PTR (pMyData->sInBufList.pBufHdr[nIndex], OMX_BUFFERHEADERTYPE);  
        } 

        ALOGV("%s: Allocating Input Buffer :%lu",__FUNCTION__,nSizeBytes);

        //  Now Allocate The Memory For The Buffer
        //  We Do not Need Nexus Memory For This Buffers. This 
        // Buffer will contain the ES data after container parsing.
        pMyData->sInBufList.pBufHdr[nIndex]->pBuffer = (OMX_U8*)
            BKNI_Malloc(nSizeBytes);

        if (!pMyData->sInBufList.pBufHdr[nIndex]->pBuffer)
        {
            ALOGE("%s: Failed To Allocate Nexus Memory For Input Buffer",__FUNCTION__);
            OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);
        }

        //
        // Along with this allocate the Memory for the input context that we maintain 
        // for each buffer. We do not need nexus memory for this as well.
        //
        pMyData->sInBufList.pBufHdr[nIndex]->pInputPortPrivate = 
            (OMX_PTR) BKNI_Malloc(sizeof(NEXUS_INPUT_CONTEXT));

        if (!pMyData->sInBufList.pBufHdr[nIndex]->pInputPortPrivate) 
        {
            ALOGE("%s: Failed To Allocate Nexus Memory For CONTEXT ",__FUNCTION__);
            OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);
        }

        PNEXUS_INPUT_CONTEXT pInNxCnxt =  
            (PNEXUS_INPUT_CONTEXT ) 
            pMyData->sInBufList.pBufHdr[nIndex]->pInputPortPrivate;

        //
        // Allocate a PES buffer that will be consumed by the 
        // transport block. 
        //
        pInNxCnxt->SzPESBuffer = PES_BUFFER_SIZE(nSizeBytes);
        pInNxCnxt->PESData = (OMX_U8 *)
            pMyData->pPESFeeder->AllocatePESBuffer(pInNxCnxt->SzPESBuffer);


        if (!pInNxCnxt->PESData)
        {
            ALOGE("%s: Failed To Allocate Nexus Memory For Input Buffer PES Header",__FUNCTION__);
            OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);
        }

        LoadBufferHeader(pMyData->sInBufList, pMyData->sInBufList.pBufHdr[nIndex], pAppPrivate, 
                nSizeBytes, nPortIndex, *ppBufferHdr, pPortDef);      

        CopyBufferHeaderList(pMyData->sInBufList, pPortDef, pMyData->pInBufHdrRes);
    }else{

        while (1) 
        {
            ALOGE("%s: Allocating Memory On Output Port--- ERROR WE ARE NOT ADVERTISING THIS QUIRK !!", 
                    __FUNCTION__);

            BKNI_Sleep(10);
        }
        /**************************************************************** 
          We may need this code later on if we ever advertise this stuff...
          ListAllocate(pMyData->sOutBufList,  nIndex);

        //First Allocate The Buffer Header
        if (pMyData->sOutBufList.pBufHdr[nIndex] == NULL)
        {
        pMyData->sOutBufList.pBufHdr[nIndex] = (OMX_BUFFERHEADERTYPE*) 
        BKNI_Malloc(sizeof(OMX_BUFFERHEADERTYPE)) ;

        if (!pMyData->sOutBufList.pBufHdr[nIndex])
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);  

        OMX_CONF_INIT_STRUCT_PTR(pMyData->sOutBufList.pBufHdr[nIndex],OMX_BUFFERHEADERTYPE);    
        }  

        //Now Allocate The Memory For The Buffer
        pMyData->sOutBufList.pBufHdr[nIndex]->pBuffer = (OMX_U8*) malloc(sizeof(NEXUS_DECODED_FRAME));

        PNEXUS_DECODED_FRAME pNxDecoFr = (PNEXUS_DECODED_FRAME) pMyData->sOutBufList.pBufHdr[nIndex]->pBuffer;


        pNxDecoFr->BuffState = BufferState_Init;
        pNxDecoFr->DispIface.pDisplayFn = DisplayBuffer;
        pNxDecoFr->DispIface.DecodedFr = pNxDecoFr;
        pNxDecoFr->DispIface.DisplayCnxt = pMyData->pOMXNxDecoder;

        ALOGD("%s: Allocated OutBuff[nIndex:%d] Buffer:%p ",
        __FUNCTION__,nIndex,pMyData->sOutBufList.pBufHdr[nIndex]->pBuffer);

#if ORIGINAL_CODE
pMyData->sOutBufList.pBufHdr[nIndex]->pBuffer = (OMX_U8*) 
BKNI_Malloc(nSizeBytes);
#endif

if (!pMyData->sOutBufList.pBufHdr[nIndex]->pBuffer) 
OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);

LoadBufferHeader(pMyData->sOutBufList, pMyData->sOutBufList.pBufHdr[nIndex],  
pAppPrivate, nSizeBytes, nPortIndex, *ppBufferHdr, pPortDef);
         ****************************************************************/
    }  

OMX_CONF_CMD_BAIL:  
    return eError;
}

extern "C" OMX_ERRORTYPE OMX_VDEC_FreeBuffer(OMX_IN  OMX_HANDLETYPE hComponent,
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

    ALOGV("%s, hComponent = %p, nPortIndex = %lu, pBufferHdr = %p",__FUNCTION__, hComponent, nPortIndex, pBufferHdr);

    // Match the pBufferHdr to the appropriate entry in the BufferList 
    // and free the allocated memory 

    if (nPortIndex == pMyData->sInPortDef.nPortIndex)
    {
        pPortDef = &pMyData->sInPortDef;

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

        unsigned int prevSize = pMyData->sInBufList.nAllocSize;
        ListFreeBuffer(pMyData->sInBufList, pBufferHdr, pPortDef)
        ListFreeBufferExt(pMyData->sInBufList, pBufferHdr, pPortDef,prevSize,pMyData->pInBufHdrRes)

    } else if (nPortIndex == pMyData->sOutPortDef.nPortIndex) {

        pPortDef = &pMyData->sOutPortDef;
        unsigned int prevSize = pMyData->sOutBufList.nAllocSize;
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

extern "C" OMX_ERRORTYPE OMX_VDEC_EmptyThisBuffer(OMX_IN  OMX_HANDLETYPE hComponent,
        OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHdr)
{
    //trace t(__FUNCTION__);
    BCM_OMX_CONTEXT *pMyData;
    ThrCmdType eCmd = EmptyBuf;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    size_t SzValidPESData;
    uint64_t nTimestamp;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    PNEXUS_INPUT_CONTEXT pNxInputCnxt = 
        (PNEXUS_INPUT_CONTEXT)pBufferHdr->pInputPortPrivate;

    OMX_CONF_CHECK_CMD(pMyData, pBufferHdr, 1);
    OMX_CONF_CHK_VERSION(pBufferHdr, OMX_BUFFERHEADERTYPE, eError);

    if (!pMyData->sInPortDef.bEnabled)
    {
        ALOGE("%s %d: Invalid Param - ",__FUNCTION__,__LINE__);
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);
    }

    if (pBufferHdr->nInputPortIndex != 0x0  || pBufferHdr->nOutputPortIndex != OMX_NOPORT)
    {
        ALOGE("%s %d: Invalid Param - ",__FUNCTION__,__LINE__);
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadPortIndex);
    }

    if (pMyData->state != OMX_StateExecuting && pMyData->state != OMX_StatePause)
    {
        ALOGE("%s %d: Invalid Param - ",__FUNCTION__,__LINE__);
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);
    }

    if (!pBufferHdr->pInputPortPrivate) 
    {
        ALOGE("%s %d: Invalid Param - ",__FUNCTION__,__LINE__);
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadParameter);
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
    // Take care of it
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
        ALOGD("%s: FilledLen:%d pInBufHdr->nTimeStamp:%lld",
            __FUNCTION__, pBufferHdr->nFilledLen, pBufferHdr->nTimeStamp);
    }

    if (pBufferHdr->nFilledLen) 
    {
        bool SendConfigDataToHw=false;

        // For WMV we save the configuration data differently...
        if (!((OMX_VIDEO_CodingWMV == OMX_COMPRESSION_FORMAT) 
#ifdef OMX_EXTEND_CODECS_SUPPORT
            || (OMX_VIDEO_CodingDIVX311 == OMX_COMPRESSION_FORMAT)
#endif          
            ))
        {
            if (pBufferHdr->nFlags & OMX_BUFFERFLAG_CODECCONFIG) 
            {
                if(false == pMyData->pPESFeeder->SaveCodecConfigData(pBufferHdr->pBuffer,pBufferHdr->nFilledLen))
                {
                    ALOGE("%s: ==ERROR== Failed To Save The Config Data, Flush May Not work",__FUNCTION__);
                }
            }
        }

        nTimestamp = pMyData->pTstable->store(pBufferHdr->nTimeStamp);
        if(false == pMyData->bSetStartUpTimeDone)
        {
            if (!(pBufferHdr->nFlags & OMX_BUFFERFLAG_CODECCONFIG)) 
            {
                pMyData->bSetStartUpTimeDone=true;

                ALOGD("%s: Setting The Startup Time [OriginalTime:%lld] [TimeToDecoder:%d]",
                      __FUNCTION__,pBufferHdr->nTimeStamp,nTimestamp); 
                pMyData->pOMXNxDecoder->SetStartupTime(nTimestamp);    
            }
        }

        switch (OMX_COMPRESSION_FORMAT) {
            case OMX_VIDEO_CodingVP8:
#ifdef OMX_EXTEND_CODECS_SUPPORT
            case OMX_VIDEO_CodingSPARK:
#endif
                {
                    uint8_t es_header[10]= {'B', 'C', 'M', 'V', 0, 0, 0, 0, 0, 0};
                    uint32_t es_header_size = 10;

                    B_MEDIA_SAVE_UINT32_BE(&es_header[4], pBufferHdr->nFilledLen);

                    if (!(pBufferHdr->nFlags & OMX_BUFFERFLAG_CODECCONFIG)) 
                    {
                        SzValidPESData = pMyData->pPESFeeder->ProcessESData(nTimestamp,
                                es_header,
                                es_header_size,
                                pBufferHdr->pBuffer,
                                pBufferHdr->nFilledLen,
                                pNxInputCnxt->PESData,
                                pNxInputCnxt->SzPESBuffer);
                    }
                    break;
                }

#ifdef OMX_EXTEND_CODECS_SUPPORT
            case OMX_VIDEO_CodingVC1:
                {
                    uint8_t es_header[4]= {0x00, 0x00, 0x01, 0x0D};
                    uint32_t es_header_size = 4;

                    if (!(pBufferHdr->nFlags & OMX_BUFFERFLAG_CODECCONFIG)) 
                    {
                        SzValidPESData = pMyData->pPESFeeder->ProcessESData(nTimestamp,
                                es_header,
                                es_header_size,
                                pBufferHdr->pBuffer,
                                pBufferHdr->nFilledLen,
                                pNxInputCnxt->PESData,
                                pNxInputCnxt->SzPESBuffer);
                    }
                    break;
                }
            case OMX_VIDEO_CodingWMV:
                {
                    uint8_t payload_header[4]= {0x00, 0x00, 0x01, 0x0D};
                    uint32_t payload_header_size = 4;

                    if (pBufferHdr->nFlags & OMX_BUFFERFLAG_CODECCONFIG)
                    {
                        uint8_t *config_header, *hdr;
                        size_t config_header_length;

                        config_header_length = 8 /*Header Size*/ + pBufferHdr->nFilledLen /*Codec Data */+ 7 /* Zero Pad */;
                        config_header = hdr = (uint8_t*)malloc(config_header_length);

                        *hdr++ = 0x00;
                        *hdr++ = 0x00;
                        *hdr++ = 0x01;
                        *hdr++ = 0x0F;
                        *hdr++ = (pMyData->sOutPortDef.format.video.nFrameWidth >> 8)&0xFF;
                        *hdr++ = (pMyData->sOutPortDef.format.video.nFrameWidth)&0xFF;
                        *hdr++ = (pMyData->sOutPortDef.format.video.nFrameHeight >> 8)&0xFF;
                        *hdr++ = (pMyData->sOutPortDef.format.video.nFrameHeight)&0xFF;

                        memcpy(hdr, pBufferHdr->pBuffer, pBufferHdr->nFilledLen);
                        hdr += pBufferHdr->nFilledLen;
                        memset(hdr, 0, 7);

                        // Save Codec Config Data will pes convert internally. And will issue the config data to 
                        // hardware on every flush.
                        if(false == pMyData->pPESFeeder->SaveCodecConfigData(config_header, config_header_length))
                        {
                            ALOGE("%s: ==ERROR== Failed To Save The Config Data, Flush May Not work",__FUNCTION__);
                        }

                        SzValidPESData = pMyData->pPESFeeder->ProcessESData(-1,
                                           config_header,
                                           config_header_length,
                                           pNxInputCnxt->PESData,
                                           pNxInputCnxt->SzPESBuffer);

                        SendConfigDataToHw=true;

                        free(config_header);
                    }
                    else
                    {
                        SzValidPESData = pMyData->pPESFeeder->ProcessESData(nTimestamp,
                                payload_header,
                                payload_header_size,
                                pBufferHdr->pBuffer,
                                pBufferHdr->nFilledLen,
                                pNxInputCnxt->PESData,
                                pNxInputCnxt->SzPESBuffer);
                    }
                    break;
                }

            case OMX_VIDEO_CodingDIVX311:
                {
                    uint8_t payload_header[4]= {0x00, 0x00, 0x01, 0xB6};
                    uint32_t payload_header_size = 4;

                    if (pBufferHdr->nFlags & OMX_BUFFERFLAG_CODECCONFIG)
                    {
                        uint8_t sequence_header[30+14] = {0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x20, 0x08, 0xC8, 0x0D, 0x40, 0x00, 0x53, 0x88, 0x40,
                            0x0C, 0x40, 0x01, 0x90, 0x00, 0x97, 0x53, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x30, 0x7F,
                            /*sequence_user_data*/ 0x00, 0x00, 0x01, 0xB2, 0x44, 0x69, 0x76, 0x58, 0x33, 0x31, 0x31, 0x41, 0x4E, 0x44};
                        uint32_t sequence_header_size = 30+14;
                        //uint8_t  sequence_user_data[14] = {0x00, 0x00, 0x01, 0xB2, 0x44, 0x69, 0x76, 0x58, 0x33, 0x31, 0x31, 0x41, 0x4E, 0x44};

                        sequence_header[24] = (pMyData->sOutPortDef.format.video.nFrameWidth >> 4)&0xFF;
                        sequence_header[25] = (((pMyData->sOutPortDef.format.video.nFrameWidth)&0xF)<<4) | 0x08 /* '10'<<2 */ | ((pMyData->sOutPortDef.format.video.nFrameHeight >> 10) & 0x3);
                        sequence_header[26] = (pMyData->sOutPortDef.format.video.nFrameHeight >> 2)&0xFF;
                        sequence_header[27] = (((pMyData->sOutPortDef.format.video.nFrameHeight)&0x3) << 6)| 0x20 /*'100000'*/;

                        // Save Codec Config Data will pes convert internally. And will issue the config data to 
                        // hardware on every flush.
                        if(false == pMyData->pPESFeeder->SaveCodecConfigData(sequence_header, sequence_header_size))
                        {
                            ALOGE("%s: ==ERROR== Failed To Save The Config Data, Flush May Not work",__FUNCTION__);
                        }

                        SzValidPESData = pMyData->pPESFeeder->ProcessESData(-1,
                                sequence_header,
                                sequence_header_size,
                                pNxInputCnxt->PESData,
                                pNxInputCnxt->SzPESBuffer);

                        SendConfigDataToHw=true;
                    }
                    else
                    {
                        SzValidPESData = pMyData->pPESFeeder->ProcessESData(nTimestamp,
                                payload_header,
                                payload_header_size,
                                pBufferHdr->pBuffer,
                                pBufferHdr->nFilledLen,
                                pNxInputCnxt->PESData,
                                pNxInputCnxt->SzPESBuffer);
                    }

                    break;
                }
#endif

            default:
                {
                    if (!(pBufferHdr->nFlags & OMX_BUFFERFLAG_CODECCONFIG))
                    {
                        SzValidPESData = pMyData->pPESFeeder->ProcessESData(nTimestamp,
                                pBufferHdr->pBuffer,
                                pBufferHdr->nFilledLen,
                                pNxInputCnxt->PESData,
                                pNxInputCnxt->SzPESBuffer);
                    }
                }
        }

        // This is valid data size need to transfer to hardware
        pNxInputCnxt->SzValidPESData = SzValidPESData;
        pNxInputCnxt->FramePTS=nTimestamp;
        if (pBufferHdr->nFlags & OMX_BUFFERFLAG_CODECCONFIG)
        {
            //We do not want to send the config data alone
            ALOGD("Codec Config Data Flag Set ----Setting The size To Zero");
            if (false == SendConfigDataToHw)
            {
                pBufferHdr->nFilledLen = 0; 
            }
        }
    }

    // Put the command and data in the pipe 
    write(pMyData->cmdpipe[1], &eCmd, sizeof(eCmd));
    write(pMyData->cmddatapipe[1], &pBufferHdr, sizeof(OMX_BUFFERHEADERTYPE*));

OMX_CONF_CMD_BAIL:  
    return eError;
}


extern "C" OMX_ERRORTYPE OMX_VDEC_FillThisBuffer(OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE * pBufferHdr)
{
    //trace t(__FUNCTION__);
    BCM_OMX_CONTEXT *pMyData;
    ThrCmdType eCmd = FillBuf;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    void * pBuffer=NULL;
    GraphicBuffer *pGraphicBuffer=NULL;
    bool unLockBuffer=false;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, pBufferHdr, 1);
    OMX_CONF_CHK_VERSION(pBufferHdr, OMX_BUFFERHEADERTYPE, eError);

    if (!pMyData->sOutPortDef.bEnabled)
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);

    if (pBufferHdr->nOutputPortIndex != 0x1 || pBufferHdr->nInputPortIndex != OMX_NOPORT)
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadPortIndex);

    if (pMyData->state != OMX_StateExecuting && pMyData->state != OMX_StatePause)
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);


    if(pMyData->sNativeBufParam.enable == true)
    {
        pGraphicBuffer = (GraphicBuffer *) pBufferHdr->pInputPortPrivate;
        BCMOMX_DBG_ASSERT(pGraphicBuffer);

#ifdef USE_ANB_PRIVATE_DATA
        pBuffer = GetDisplayFrameFromANB(pGraphicBuffer->getNativeBuffer());
        BCMOMX_DBG_ASSERT(pBuffer == pBufferHdr->pBuffer);
#else
        pGraphicBuffer->lock(GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_OFTEN, (void**)&pBuffer);
        unLockBuffer=true;
#endif
    }else{
        // This is a non-ANativeWindow Buffer !!
        pBuffer = pBufferHdr->pBuffer;
    }

    //At this point we should always have something in pBuffer.
    BCMOMX_DBG_ASSERT(pBuffer);
    /**************************************************************
     *The Dropped Frames are recycled Back to the component without                                                            
     *Actually any notification. We have an internal state in the                                                                                                                          
     *frame that tells us if the frame we consumed or just recycled.                                                                                                                                                                                     
     *We drop the frames here which are not consumed.                                                                                                                                                                                                                                                   
     ***************************************************************/
    pMyData->pOMXNxDecoder->CheckAndClearBufferState((PDISPLAY_FRAME) pBuffer);
    if(unLockBuffer)
    {
        BCMOMX_DBG_ASSERT(pGraphicBuffer);
        pGraphicBuffer->unlock();
    }

    pMyData->pOMXNxDecoder->GetFramesFromHardware();

    // Put the command and data in the pipe 
    write(pMyData->cmdpipe[1], &eCmd, sizeof(eCmd));
    write(pMyData->cmddatapipe[1], &pBufferHdr, sizeof(OMX_BUFFERHEADERTYPE*));

OMX_CONF_CMD_BAIL:   
    return eError;
}


extern "C" OMX_ERRORTYPE OMX_VDEC_SetCallbacks(OMX_IN  OMX_HANDLETYPE hComponent,
        OMX_IN  OMX_CALLBACKTYPE* pCallbacks, 
        OMX_IN  OMX_PTR pAppData)

{
    trace t(__FUNCTION__);
    LOGV("OMX_ADEC_SetCallbacks \n");
    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, pCallbacks, pAppData);

    pMyData->pCallbacks = pCallbacks;
    pMyData->pAppData = pAppData;

OMX_CONF_CMD_BAIL:  
    return eError;
}

static bool portSettingChanged(BCM_OMX_CONTEXT* pMyData)
{
    NEXUS_Error nxErr = NEXUS_SUCCESS;
    NEXUS_VideoDecoderStatus vdecStatus; 
    trace t(__FUNCTION__);
    if(false == pMyData->pOMXNxDecoder->GetDecoSts(&vdecStatus))
    {
        //Get the last error here to find out more.
        return false;
    }

    //    nxErr = NEXUS_SimpleVideoDecoder_GetStatus(mvideoDecoderhandle, &vdecStatus);

    LOGD("%s: Nexus Returned W :%d HEIGHT:%d",__FUNCTION__, vdecStatus.display.width,vdecStatus.display.height);

    if(!vdecStatus.display.width || !vdecStatus.display.height)
    {
        LOGD("%s: Nexus Returned ZERO W :%d HEIGHT:%d",__FUNCTION__, vdecStatus.display.width,vdecStatus.display.height);
        return false;
    }

    //      OMX_PARAM_PORTDEFINITIONTYPE *def = &editPortInfo(0)->mDef;
    //      def->format.video.nFrameWidth = mWidth;
    //      def->format.video.nFrameHeight = mHeight;
    //      def->format.video.nStride = def->format.video.nFrameWidth;
    //      def->format.video.nSliceHeight = def->format.video.nFrameHeight;
    // 
    //      def = &editPortInfo(1)->mDef;
    //      def->format.video.nFrameWidth = mWidth;
    //      def->format.video.nFrameHeight = mHeight;
    //      def->format.video.nStride = def->format.video.nFrameWidth;
    //      def->format.video.nSliceHeight = def->format.video.nFrameHeight;
    // 
    //      def->nBufferSize =
    //          (((def->format.video.nFrameWidth + 15) & -16)
    //          * ((def->format.video.nFrameHeight + 15) & -16) * 3) / 2;


    if( (pMyData->sOutPortDef.format.video.nFrameWidth != vdecStatus.display.width) ||
            (pMyData->sOutPortDef.format.video.nFrameHeight != vdecStatus.display.height))
    {
        pMyData->sInPortDef.format.video.nFrameWidth = vdecStatus.display.width;
        pMyData->sInPortDef.format.video.nFrameHeight = vdecStatus.display.height;
        pMyData->sInPortDef.format.video.nStride = vdecStatus.display.width;
        pMyData->sInPortDef.format.video.nSliceHeight = vdecStatus.display.height;

        pMyData->sOutPortDef.format.video.nFrameWidth = vdecStatus.display.width;
        pMyData->sOutPortDef.format.video.nFrameHeight = vdecStatus.display.height;
        pMyData->sOutPortDef.format.video.nStride = vdecStatus.display.width;
        pMyData->sOutPortDef.format.video.nSliceHeight = vdecStatus.display.height;

#ifdef ORIGINAL
        pMyData->sOutPortDef.nBufferSize =     (pMyData->sOutPortDef.format.video.nFrameWidth * pMyData->sOutPortDef.format.video.nFrameHeight * 3)/2;

#else
        pMyData->sOutPortDef.nBufferSize =     pMyData->sOutPortDef.format.video.nFrameWidth *
            pMyData->sOutPortDef.format.video.nFrameHeight * 2;
#endif        

        ALOGD("%s %d: Emitting Port Settings Changed Event",__FUNCTION__,__LINE__);
        pMyData->pCallbacks->EventHandler(pMyData->hSelf,pMyData->pAppData, 
                OMX_EventPortSettingsChanged, 1, 0, NULL);

        return true;
    }

    return false;
}


    static bool
FlushInput(BCM_OMX_CONTEXT *pBcmContext)
{
    // On Flush, We will start from a different 
    // Location, Need to set the start up time 
    // again.
    pBcmContext->bSetStartUpTimeDone = false;
    if(pBcmContext->pPESFeeder)
    {
        if(false == pBcmContext->pPESFeeder->Flush())
        {
            ALOGE("==FLUSH FAILED ON INPUT PORT==");
            return false;
        }
    }
    ListFlushEntries(pBcmContext->sInBufList, pBcmContext);
    return true;
}

    static void 
CleanUpOutputBuffer(OMX_BUFFERHEADERTYPE *pOMXBuffhdr)
{
    void *pBuffer=NULL;
    bool unLockBuffer=false;
    GraphicBuffer *pGraphicBuffer=NULL;
    
    if (!pOMXBuffhdr) 
    {
        ALOGV("%s: EXIT-1 == \n\n ",__FUNCTION__);
        return;
    }

    //Clear the Filled Len and 
    pOMXBuffhdr->nFilledLen =0;
    pGraphicBuffer = (GraphicBuffer *) pOMXBuffhdr->pInputPortPrivate;
    BCMOMX_DBG_ASSERT(pGraphicBuffer);

#ifdef USE_ANB_PRIVATE_DATA
        pBuffer = GetDisplayFrameFromANB(pGraphicBuffer->getNativeBuffer());
        BCMOMX_DBG_ASSERT(pBuffer == pOMXBuffhdr->pBuffer);
#else
        pGraphicBuffer->lock(GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_OFTEN, (void**)&pBuffer);
        unLockBuffer = true;
#endif

    // Now Clear the Buffer.
    PDISPLAY_FRAME pNxDecoFr = (PDISPLAY_FRAME) pBuffer;
    BCMOMX_DBG_ASSERT(pNxDecoFr);
    if (pNxDecoFr) 
    {
        memset(pNxDecoFr ,0,sizeof(DISPLAY_FRAME));
    }

    if(unLockBuffer)
    {
        BCMOMX_DBG_ASSERT(pGraphicBuffer);
        pGraphicBuffer->unlock();
    }
    return;
}

static bool
FlushOutput(BCM_OMX_CONTEXT *pBcmContext)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    pPortDef = &pBcmContext->sOutPortDef;
    if (pBcmContext->pOMXNxDecoder) 
    {   
        if (false == pBcmContext->pOMXNxDecoder->Flush()) 
        {
            ALOGE("==FLUSH FAILED ON OUTPUT PORT==");     
            return false;
        }
    }
    
    ListFlushEntriesWithCleanup(pBcmContext->sOutBufList, pBcmContext,CleanUpOutputBuffer);
    //ListFlushEntries(pBcmContext->sOutBufList, pBcmContext);
    pBcmContext->pTstable->flush();
    return true;
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
                ALOGV("OMXVideo: SET STATE COMMAND: To %d\n",cmddata);

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
                                    nTimeout = 0x0;
                                    while (1)
                                    {
                                        // Transition happens only when the ports are unpopulated
                                        if (!pMyData->sInPortDef.bPopulated && 
                                                !pMyData->sOutPortDef.bPopulated)
                                        {
                                            ALOGV("%s %d: State Transition Event: [To: State_Loaded] [Completed] [Timeouts:%d]",
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

                                    ALOGE("%s %d: State Transition Event: [To: State_Loaded] [ERROR OMX_ErrorIncorrectStateTransition]",
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
                                    while (1)
                                    {
                                        /* Ports have to be populated before transition completes*/
                                        if ((!pMyData->sInPortDef.bEnabled && !pMyData->sOutPortDef.bEnabled)||              
                                                (pMyData->sInPortDef.bPopulated && pMyData->sOutPortDef.bPopulated)) 
                                        {
                                            pMyData->state = OMX_StateIdle;
                                            ALOGV("%s %d: State Transition Event: [To: OMX_StateIdle] [Completed]",
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

                                    ALOGV("%s %d: State Transition Event: [To: OMX_StateExecuting] [Completed]",
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

                                    ALOGV("%s %d: State Transition Event: [To: OMX_StatePause] [Completed]",
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
                                    ALOGV("%s %d: State Transition Event: [To: OMX_StateWaitForResources] [Completed]",
                                            __FUNCTION__,__LINE__);

                                    pMyData->pCallbacks->EventHandler(pMyData->hSelf,pMyData->pAppData, 
                                            OMX_EventCmdComplete, OMX_CommandStateSet, pMyData->state, NULL);
                                } else {
                                    ALOGV("%s %d: State Transition Event: [To: OMX_StateWaitForResources] [ERROR]",
                                            __FUNCTION__,__LINE__);

                                    pMyData->pCallbacks->EventHandler(pMyData->hSelf,pMyData->pAppData, 
                                            OMX_EventError, OMX_ErrorIncorrectStateTransition, 0 , NULL);
                                }
                                break;
                            }   
                        default :
                            ALOGE("%s %d: State Transition Event: ==INVALID STATE ==",
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

                if (cmddata == 0x0 || cmddata == -1)
                {
                    // Return all input buffers 
                    FlushInput(pMyData);

                    // Disable port 
                    pMyData->sInPortDef.bEnabled = OMX_FALSE;
                }
                if (cmddata == 0x1 || cmddata == -1)
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
                    if (cmddata == -1 &&  !pMyData->sInPortDef.bPopulated && 
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

                ALOGV("%s: RESTART PORT PortIndex : %d", __FUNCTION__, cmddata);

                if (cmddata == 0x0 || cmddata == -1)
                    pMyData->sInPortDef.bEnabled = OMX_TRUE;

                if (cmddata == 0x1 || cmddata == -1)
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
                    else if (cmddata == -1 && (pMyData->state == OMX_StateLoaded || 
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

                ALOGV("%s: FLUSH PORT PortIndex : %d", __FUNCTION__, cmddata);
                if (cmddata == 0x0 || cmddata == -1)
                {
                    //                  // Return all Input buffers and send cmdcomplete
                    FlushInput(pMyData);
                    pMyData->pCallbacks->EventHandler(pMyData->hSelf, pMyData->pAppData, 
                            OMX_EventCmdComplete, OMX_CommandFlush, 0x0, NULL);
                }

                if (cmddata == 0x1 || cmddata == -1)
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

        // Input Buffer processing 
        // Only happens when the component is in executing state. 
        if (pMyData->state == OMX_StateExecuting && pMyData->sInPortDef.bEnabled && 
                (pMyData->sInBufList.nSizeOfList > 0))
        {
            ListGetEntry(pMyData->sInBufList, pInBufHdr)
                // If there is no output buffer, get one from list 
                // Check for EOS flag 
            if (pInBufHdr)
            {
                OMX_U32 nFlags=0;
                unsigned long long EOSTimeStamp=0;
                if (pInBufHdr->nFlags & OMX_BUFFERFLAG_EOS)
                {
                    nFlags =  pInBufHdr->nFlags;
                    pInBufHdr->nFlags = 0;
                    if(pInBufHdr->nTimeStamp <= 0) {
                        EOSTimeStamp = 0;
                    } else {
                        EOSTimeStamp = CONVERT_USEC_45KHZ(pInBufHdr->nTimeStamp);
                    }
                }

                // Check for mark buffers
                if (pInBufHdr->pMarkData)
                {
                    // Copy mark to output buffer header
                    if (pOutBufHdr)
                    {
                        pOutBufHdr->pMarkData = pInBufHdr->pMarkData;
                        // Copy handle to output buffer header
                        pOutBufHdr->hMarkTargetComponent = pInBufHdr->hMarkTargetComponent;
                    }
                }

                // Trigger event handler
                if (pInBufHdr->hMarkTargetComponent == pMyData->hSelf && pInBufHdr->pMarkData)
                {
                    pMyData->pCallbacks->EventHandler(pMyData->hSelf, pMyData->pAppData,
                            OMX_EventMark, 0, 0, pInBufHdr->pMarkData);
                }

                if(pInBufHdr->nFilledLen)
                {
                    PNEXUS_INPUT_CONTEXT pNxInputCnxt =
                        (PNEXUS_INPUT_CONTEXT)pInBufHdr->pInputPortPrivate;

                    BCMOMX_DBG_ASSERT(pNxInputCnxt->SzValidPESData);

                    // fill in the Done context and be ready for
                    // Firing the Empty Buffer done once the DMA is complete

                    pInBufHdr->nFilledLen =0;
                    pNxInputCnxt->DoneContext.Param1 = (unsigned int) pMyData->hSelf;
                    pNxInputCnxt->DoneContext.Param2 = (unsigned int) pMyData->pAppData;
                    pNxInputCnxt->DoneContext.Param3 = (unsigned int) pInBufHdr;

                    pNxInputCnxt->DoneContext.pFnDoneCallBack =
                        (BUFFER_DONE_CALLBACK) pMyData->pCallbacks->EmptyBufferDone;

                    if(false == pMyData->pPESFeeder->SendPESDataToHardware(pNxInputCnxt))
                    {
                        LOGE("%s: Sending The Data To The Hardware Failed",__FUNCTION__);
                        LOGE("%s: Call back Will Not Be Fired, Playback May Stop",__FUNCTION__);
                        BCMOMX_DBG_ASSERT(false);
                    }

                }else{
                        pMyData->pCallbacks->EmptyBufferDone(pMyData->hSelf, pMyData->pAppData, pInBufHdr);
                }

                // Do EOS only after the data is transferred....
                if (nFlags & OMX_BUFFERFLAG_EOS)
                {
                    /**
                     * 1. Copy flag to output buffer header.
                     * 2. RAISE THE EVENT WHEN EOS IS SENT FROM OUTPUT PORT
                     * 3. THE EVENT NEEDS SHOULD BE GENERATED USING THIS nFlags
                     * FIELD.
                     * 4. The Input Command Should Not Contain The FLAG. So we clear
                     * it.
                     */
                    ALOGD("====EOS On Input====");

                    pMyData->pPESFeeder->NotifyEOS(nFlags,EOSTimeStamp);
                    nFlags = 0;
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

            if(pOutBufHdr)
            {
                unsigned int EOSMarkerFlags=0;
                void *pBuffer=NULL;
                bool unLockBuffer=false;
                GraphicBuffer *pGraphicBuffer=NULL;
                if(pMyData->sNativeBufParam.enable == true)
                {
                    pGraphicBuffer = (GraphicBuffer *) pOutBufHdr->pInputPortPrivate;
                    BCMOMX_DBG_ASSERT(pGraphicBuffer);

#ifdef USE_ANB_PRIVATE_DATA
                    pBuffer = GetDisplayFrameFromANB(pGraphicBuffer->getNativeBuffer());
                    BCMOMX_DBG_ASSERT(pBuffer == pOutBufHdr->pBuffer);
#else
                    pGraphicBuffer->lock(GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_OFTEN, (void**)&pBuffer);
                    unLockBuffer = true;
#endif
                }else{
                    //This is the case of Non-ANativeWindowBuffer....
                    pBuffer = pOutBufHdr->pBuffer;
                }

                BCMOMX_DBG_ASSERT(pBuffer);
                PDISPLAY_FRAME pNxDecoFr = (PDISPLAY_FRAME) pBuffer;

                if (pMyData->pOMXNxDecoder->GetDecodedFrame(pNxDecoFr))
                {
                    uint32_t nTimeStamp;
                    pOutBufHdr->nFilledLen = sizeof(DISPLAY_FRAME);
                    nTimeStamp = pMyData->pOMXNxDecoder->GetFrameTimeStampMs(pNxDecoFr);
                    pOutBufHdr->nTimeStamp= pMyData->pTstable->retrieve(nTimeStamp);
                }else{

                    // This may be a frame with NULL data
                    pOutBufHdr->nFilledLen = 0;
                }

                if (pNxDecoFr->OutFlags & OMX_BUFFERFLAG_EOS)
                {
                    ALOGD("EOS MARKED ON THE OUTPUT FRAME:%d",
                            pNxDecoFr->OutFlags);

                    pOutBufHdr->nFlags |= OMX_BUFFERFLAG_EOS;
                    EOSMarkerFlags = pNxDecoFr->OutFlags;
                }

                if(unLockBuffer)
                {
                    BCMOMX_DBG_ASSERT(pGraphicBuffer);
                    pGraphicBuffer->unlock();
                }

                if(pOutBufHdr->pPlatformPrivate) 
                {
                    //We Store the pointer to the anb in pPlatformPrivate
                    // We cannot use that pointer here becuase the pointer 
                    // changes after the allocation. We get a new ANB from
                    // The Graphic buffer itself.
                    ANativeWindowBuffer *anb=NULL;
                    anb = pGraphicBuffer->getNativeBuffer();
                    BCMOMX_DBG_ASSERT(anb);
                    pMyData->pAndVidWindow->SetPrivData(anb, 0); 
                    pOutBufHdr->pPlatformPrivate=NULL;
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
            }//if (pOutBufHdr)
        }////Output Buffer Processing...
    }//while (1)

EXIT:
    return (void*)OMX_ErrorNone;

}//ComponentThread(void* pThreadData)


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





