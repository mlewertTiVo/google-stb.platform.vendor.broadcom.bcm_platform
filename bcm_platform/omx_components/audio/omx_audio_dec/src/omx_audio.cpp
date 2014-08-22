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
#include <utils/Log.h>
#include <utils/List.h>
 
//#define DUMP_INPUT_DATA 1
//#define DUMP_OUTPUT_DATA 1

//#define LOG_NDEBUG 0

#undef LOG_TAG
#define LOG_TAG "BCM_OMX_AUDIO_DEC"
#define COMPONENT_NAME "OMX.BCM.AUDIO.DECODER"


#define FRAMES_TO_DUMP 300
static DumpData *pDumpData=NULL;   


#define MAX_SUPPORTED_FREQ 13

//
// These the MPEG4 Supported Frequences
// http://wiki.multimedia.cx/index.php?title=MPEG-4_Audio#Sampling_Frequencies
// 
//

typedef struct _CODEC_TO_MIME_MAP_
{
    OMX_AUDIO_CODINGTYPE    CodingType;
    OMX_STRING              Mime;
}CODEC_TO_MIME_MAP,*PCODEC_TO_MIME_MAP;

static const CODEC_TO_MIME_MAP CodecToMIME[] = 
{
	{OMX_AUDIO_CodingAAC,	"audio/aac"},
	{OMX_AUDIO_CodingAC3,	"audio/ac3"}
};

typedef struct _OMX_TO_NEXUS_MAP_
{
    OMX_AUDIO_CODINGTYPE   OMxCoding;
    NEXUS_AudioCodec       NexusCodec; 
}OMX_TO_NEXUS_MAP, *POMX_TO_NEXUS_MAP;

static const OMX_TO_NEXUS_MAP OMXToNexusTable[] = {
	{OMX_AUDIO_CodingAAC, NEXUS_AudioCodec_eAac},
	{OMX_AUDIO_CodingAC3, NEXUS_AudioCodec_eAc3}
};

static
unsigned int SupportedFreqIndex[MAX_SUPPORTED_FREQ] =
{9600,88200,64000,48000,44100,32000,24000,22050,16000,12000,11025,8000,7350};

extern "C"
{

OMX_ERRORTYPE OMX_ComponentInit(OMX_HANDLETYPE hComponent);

OMX_ERRORTYPE OMX_ADEC_SendCommand(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_COMMANDTYPE eCmd,
    OMX_IN OMX_U32 nParam, OMX_IN OMX_PTR pCmdData);


OMX_ERRORTYPE OMX_ADEC_GetExtensionIndex(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_STRING cParameterName, OMX_OUT OMX_INDEXTYPE * pIndexType);


OMX_ERRORTYPE OMX_ADEC_GetParameter(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pParamStruct);

OMX_ERRORTYPE OMX_ADEC_SetParameter(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pParamStruct);

OMX_ERRORTYPE OMX_ADEC_EmptyThisBuffer(OMX_IN  OMX_HANDLETYPE hComponent,
                OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHdr);

OMX_ERRORTYPE OMX_ADEC_UseBuffer(OMX_IN OMX_HANDLETYPE hComponent,
              OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
              OMX_IN OMX_U32 nPortIndex,
              OMX_IN OMX_PTR pAppPrivate,
              OMX_IN OMX_U32 nSizeBytes,
              OMX_IN OMX_U8* pBuffer);


OMX_ERRORTYPE OMX_ADEC_AllocateBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                   OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
                   OMX_IN OMX_U32 nPortIndex,
                   OMX_IN OMX_PTR pAppPrivate,
                   OMX_IN OMX_U32 nSizeBytes);

OMX_ERRORTYPE OMX_ADEC_FreeBuffer(OMX_IN  OMX_HANDLETYPE hComponent,
               OMX_IN  OMX_U32 nPortIndex,
               OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHdr);


OMX_ERRORTYPE OMX_ADEC_FillThisBuffer(OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE * pBufferHdr);

 
OMX_ERRORTYPE OMX_ADEC_SetCallbacks(OMX_IN  OMX_HANDLETYPE hComponent,
                 OMX_IN  OMX_CALLBACKTYPE* pCallbacks, 
                 OMX_IN  OMX_PTR pAppData);

OMX_ERRORTYPE OMX_ADEC_DeInit(OMX_IN  OMX_HANDLETYPE hComponent);

OMX_ERRORTYPE OMX_ADEC_GetState(OMX_IN  OMX_HANDLETYPE hComponent,
             OMX_OUT OMX_STATETYPE* pState);


OMX_ERRORTYPE OMX_ADEC_GetConfig(OMX_IN OMX_HANDLETYPE hComponent,
                                 OMX_IN OMX_INDEXTYPE nIndex,
                                 OMX_INOUT OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE OMX_ADEC_SetConfig(OMX_IN OMX_HANDLETYPE hComponent,
                                 OMX_IN OMX_INDEXTYPE nIndex,
                                 OMX_IN OMX_PTR pComponentConfigStructure);


}

OMX_STRING
GetMimeFromOmxCodingType(OMX_AUDIO_CODINGTYPE OMXCodingType)
{
	int count = 0;
	for (count = 0; count < sizeof(CodecToMIME)/sizeof(CodecToMIME[0]);count++)
	{
		if (CodecToMIME[count].CodingType==OMXCodingType)
			break;
		//TODO: Error Handling
	}
    ALOGD("%s: Identified Mime Type Is %s",__FUNCTION__, CodecToMIME[count].Mime);
    return CodecToMIME[OMXCodingType].Mime;
}


NEXUS_AudioCodec
MapOMXVideoCodecToNexus(OMX_AUDIO_CODINGTYPE OMxCodec)
{
	int count = 0;
	for (count = 0; count < sizeof(OMXToNexusTable)/sizeof(OMXToNexusTable[0]);count++)
	{
		if (OMXToNexusTable[count].OMxCoding==OMxCodec)
			break;
		//TODO: Error Handling
	}
    return OMXToNexusTable[count].NexusCodec;
}

static void EndOfStream(void *context, int param)
{
    BSTD_UNUSED(param);
    LOGE("OpenMAX Audio Comp: EndOfStream Recvd\n");
    BKNI_SetEvent((BKNI_EventHandle)context);
}

unsigned char 
GetFreqIndex(unsigned int SampleRate)
{
    for (unsigned int i=0; i<MAX_SUPPORTED_FREQ;i++ ) 
    {
        if (SupportedFreqIndex[i] == SampleRate) 
        {
            return i;
        }
    }
    //Freq Not Found
    BCMOMX_DBG_ASSERT(false);
    return 0xff;
}

static void 
DumpADTSHeaderByteByByte(unsigned char *pAdtsHdr)
{
    unsigned char *pBuffer= (unsigned char *) pAdtsHdr;

    ALOGD("======== DUMPING ADTS HEADER ==================");
    ALOGD("%s:%d: [HEX DUMP] %x %x %x %x %x %x %x",
          __FUNCTION__,__LINE__,
          pBuffer[0],
          pBuffer[1],
          pBuffer[2],
          pBuffer[3],
          pBuffer[4],
          pBuffer[5],
          pBuffer[6]);

    ALOGD("======== DONE ==================");
}


static void CreateADTSHeader(BCM_OMX_CONTEXT *pBcmOmxCnxt,size_t BufferLen)
{
    unsigned char FreqIndex=0xff;
    unsigned char MpegVer=0;
    unsigned char ProtectionAbsent=1;
    unsigned char Channels=pBcmOmxCnxt->BCMAacParams.nChannels;
    unsigned char  Profile = pBcmOmxCnxt->BCMAacParams.eAACProfileType - 1; //MPEG Profile - 1
    FreqIndex = GetFreqIndex(pBcmOmxCnxt->BCMAacParams.nSampleRate);

    //MPEG-4 Supports 13 Frequencies.
    BCMOMX_DBG_ASSERT(FreqIndex>=0 && FreqIndex<=12);
    BCMOMX_DBG_ASSERT(sizeof(pBcmOmxCnxt->BCMAacParams.BCMAdtsHeader) == SIZEOF_BCMADTS_HEADER);

    if(pBcmOmxCnxt->BCMAacParams.eAACStreamFormat==OMX_AUDIO_AACStreamFormatMP2ADTS) 
        MpegVer=1;

    BufferLen+=SIZEOF_BCMADTS_HEADER;

    pBcmOmxCnxt->BCMAacParams.BCMAdtsHeader[0] = 0xff;
    pBcmOmxCnxt->BCMAacParams.BCMAdtsHeader[1] = 0xf0 | (MpegVer << 3) | ProtectionAbsent;
    pBcmOmxCnxt->BCMAacParams.BCMAdtsHeader[2] = (unsigned char) ((Profile << 6) + (FreqIndex<<2) + (Channels>>2));
    pBcmOmxCnxt->BCMAacParams.BCMAdtsHeader[3] = (unsigned char) (((Channels&3) << 6) + (BufferLen>>11));
    pBcmOmxCnxt->BCMAacParams.BCMAdtsHeader[4] = (unsigned char) ((BufferLen & 0x7FF) >> 3);
    pBcmOmxCnxt->BCMAacParams.BCMAdtsHeader[5] = (unsigned char) (((BufferLen & 0x7) << 5) + 0x1f);;
    pBcmOmxCnxt->BCMAacParams.BCMAdtsHeader[6] = (unsigned char) 0xFC;
    //DumpADTSHeaderByteByByte(pBcmOmxCnxt->BCMAacParams.BCMAdtsHeader);
    return;
}

extern "C" OMX_ERRORTYPE OMX_ComponentInit(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pComp = NULL;
    BCM_OMX_CONTEXT *pMyData = NULL;
    OMX_U32 err;
    uint nIndex;

    pComp = (OMX_COMPONENTTYPE *) hComponent;

    // Create private data
    pMyData = (BCM_OMX_CONTEXT *)BKNI_Malloc(sizeof(BCM_OMX_CONTEXT));
    
    if(pMyData == NULL)
    {
        LOGE("Error in allocating mem for pMyData\n");
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    
    BKNI_Memset(pMyData, 0x0, sizeof(BCM_OMX_CONTEXT));

    ALOGD("%s %d: Component Init Handle :%x BUILD-DATE:%s BUILD-TIME:%s ",
          __FUNCTION__,__LINE__,hComponent,__DATE__,__TIME__);

    pComp->pComponentPrivate = (OMX_PTR)pMyData;
    pMyData->state = OMX_StateLoaded;
    pMyData->hSelf = hComponent;
    
    pComp->SetParameter         = OMX_ADEC_SetParameter;        
    pComp->GetParameter         = OMX_ADEC_GetParameter;
    pComp->GetExtensionIndex    = OMX_ADEC_GetExtensionIndex;
    pComp->SendCommand          = OMX_ADEC_SendCommand;
    pComp->SetCallbacks         = OMX_ADEC_SetCallbacks;
    pComp->ComponentDeInit      = OMX_ADEC_DeInit;
    pComp->GetState             = OMX_ADEC_GetState;
    pComp->GetConfig            = OMX_ADEC_GetConfig;
    pComp->SetConfig            = OMX_ADEC_SetConfig;
    pComp->UseBuffer            = OMX_ADEC_UseBuffer;
    pComp->AllocateBuffer       = OMX_ADEC_AllocateBuffer;
    pComp->FreeBuffer           = OMX_ADEC_FreeBuffer;
    pComp->EmptyThisBuffer      = OMX_ADEC_EmptyThisBuffer;
    pComp->FillThisBuffer       = OMX_ADEC_FillThisBuffer;
    
    //Empty Buffer Done
    //Fill Buffer Done

    // Initialize component data structures to default values 
    OMX_CONF_INIT_STRUCT_PTR(&pMyData->sPortParam, OMX_PORT_PARAM_TYPE);
    pMyData->sPortParam.nPorts = 0x2;
    pMyData->sPortParam.nStartPortNumber = 0x0;

    // Initialize the audio parameters for input port 
    OMX_CONF_INIT_STRUCT_PTR(&pMyData->sInPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    pMyData->sInPortDef.nPortIndex = 0x0;
    pMyData->sInPortDef.bEnabled = OMX_TRUE;
    pMyData->sInPortDef.bPopulated = OMX_FALSE;
    pMyData->sInPortDef.eDomain = OMX_PortDomainAudio;
    pMyData->sInPortDef.format.audio.cMIMEType = GetMimeFromOmxCodingType(OMX_COMPRESSION_FORMAT);
    pMyData->sInPortDef.eDir = OMX_DirInput;
    pMyData->sInPortDef.nBufferCountMin = NUM_IN_AUDIO_BUFFERS;
    pMyData->sInPortDef.nBufferCountActual = NUM_IN_AUDIO_BUFFERS;
    pMyData->sInPortDef.nBufferSize =  INPUT_AUDIO_BUFFER_SIZE;
    pMyData->sInPortDef.format.audio.eEncoding =  OMX_AUDIO_CodingAAC; 
    pMyData->sInPortDef.format.audio.pNativeRender = NULL;
    pMyData->sInPortDef.format.audio.bFlagErrorConcealment = OMX_FALSE;
    
    // Initialize the audio parameters for output port
    OMX_CONF_INIT_STRUCT_PTR(&pMyData->sOutPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    pMyData->sOutPortDef.nPortIndex = 0x1;
    pMyData->sOutPortDef.bEnabled = OMX_TRUE;
    pMyData->sOutPortDef.bPopulated = OMX_FALSE;
    pMyData->sOutPortDef.eDomain = OMX_PortDomainAudio;
    pMyData->sOutPortDef.format.video.cMIMEType = "audio/raw";
    pMyData->sOutPortDef.eDir = OMX_DirOutput;
    pMyData->sOutPortDef.nBufferCountMin = NUM_OUT_AUDIO_BUFFERS; 
    pMyData->sOutPortDef.nBufferCountActual = NUM_OUT_AUDIO_BUFFERS;
    pMyData->sOutPortDef.nBufferSize =  OUTPUT_AUDIO_BUFFER_SIZE;
    pMyData->sOutPortDef.format.audio.eEncoding =  OMX_AUDIO_CodingPCM;
    pMyData->sOutPortDef.format.audio.pNativeRender = NULL;
    pMyData->sOutPortDef.format.audio.bFlagErrorConcealment = OMX_FALSE;

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
        
    for (nIndex = 0; nIndex < pMyData->sInPortDef.nBufferCountActual; nIndex++)
    {
        pMyData->sInBufList.pBufHdr[nIndex] = (OMX_BUFFERHEADERTYPE*) 
                                              malloc(sizeof(OMX_BUFFERHEADERTYPE));

        if(pMyData->sInBufList.pBufHdr[nIndex] == NULL)
        {
            LOGE("Error in allocating mem for pMyData->sInBufList.pBufHdr[nIndex]\n");
            eError = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
            
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
    BKNI_Malloc (sizeof(OMX_BUFFERHEADERTYPE*) * pMyData->sOutPortDef.nBufferCountActual); 
    
    if(pMyData->sOutBufList.pBufHdr == NULL)
    {
        LOGE("Error in allocating mem for pMyData->sOutBufList.pBufHdr\n");
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    
    for (nIndex = 0; nIndex < pMyData->sOutPortDef.nBufferCountActual; nIndex++)
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
    
    pMyData->pOMXNxAudioDec = new OMXNexusAudioDecoder("OMX_AUDIO_CODEC", 
										MapOMXVideoCodecToNexus(OMX_COMPRESSION_FORMAT));

    pMyData->pPESFeeder = new PESFeeder(LOG_TAG,
                                        PES_AUDIO_PID,
                                        NUM_IN_AUDIO_DESCRIPTORS,
                                        NEXUS_TransportType_eMpeg2Pes);


    pMyData->pOMXNxAudioDec->RegisterDecoderEventListener(pMyData->pPESFeeder);
    pMyData->pPESFeeder->RegisterFeederEventsListener(pMyData->pOMXNxAudioDec);

    pMyData->pPESFeeder->StartDecoder(pMyData->pOMXNxAudioDec);

    //Default Input AAC Parameters
    pMyData->BCMAacParams.eAACProfileType = OMX_AUDIO_AACObjectLC;
    pMyData->BCMAacParams.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4FF;
    pMyData->BCMAacParams.nChannels=2;
    pMyData->BCMAacParams.nSampleRate=48000;
    
#ifdef DUMP_INPUT_DATA
    pDumpData=new DumpData("InputAudioData");
#endif

#ifdef DUMP_OUTPUT_DATA
    pDumpData=new DumpData("OutputAudioData");
#endif

    LOGI("Audio Component Intialisation Completed\n");
    return eError;
    
EXIT:

    //TODO: take care of releasing all resources (memory, threads, nexus handles, etc) on error
    OMX_ADEC_DeInit(hComponent);
    return eError;
}

    
extern "C" OMX_ERRORTYPE OMX_ADEC_DeInit(OMX_IN  OMX_HANDLETYPE hComponent)
{

    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    ThrCmdType eCmd = Stop;
    OMX_U32 nIndex = 0;
    NEXUS_PlaybackStatus status;
    NEXUS_PlaypumpStatus playpumpStatus;

    ALOGD("%s %d: Component Handle :%p BUILD-DATE:%s BUILD-TIME:%s ",
          __FUNCTION__,__LINE__,hComponent,__DATE__,__TIME__);

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    if (pDumpData) 
    {
        delete pDumpData;
        pDumpData=NULL;
    }

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


    if (pMyData->pPESFeeder) 
    {
        delete pMyData->pPESFeeder;
        pMyData->pPESFeeder=NULL;
    }

    if (pMyData->pOMXNxAudioDec) 
    {
        delete pMyData->pOMXNxAudioDec;
        pMyData->pOMXNxAudioDec=NULL;
    }

    if (NULL != pMyData) 
        BKNI_Free(pMyData);

    return eError;
}


extern "C" OMX_ERRORTYPE OMX_ADEC_SendCommand(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_COMMANDTYPE eCmd,
    OMX_IN OMX_U32 nParam, OMX_IN OMX_PTR pCmdData)
{

    LOGV(" OMX_ADEC_SendCommand cmd is %d\n",eCmd);
    
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
            LOGE("Invalid OMX Command\n");
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

extern "C"
OMX_ERRORTYPE OMX_ADEC_GetConfig(OMX_IN OMX_HANDLETYPE hComponent,
                                 OMX_IN OMX_INDEXTYPE nIndex,
                                 OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    ALOGD("%s: ENTER \n",__FUNCTION__);
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    ALOGD("%s: EXIT \n",__FUNCTION__);
    return OMX_ErrorUndefined;
}

extern "C"
OMX_ERRORTYPE OMX_ADEC_SetConfig(OMX_IN OMX_HANDLETYPE hComponent,
                                 OMX_IN OMX_INDEXTYPE nIndex,
                                 OMX_IN OMX_PTR pComponentConfigStructure)
{
    ALOGD("%s: ENTER \n",__FUNCTION__);
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    ALOGD("%s: EXIT \n",__FUNCTION__);
    return OMX_ErrorUndefined;
}



extern "C" OMX_ERRORTYPE OMX_ADEC_GetState(OMX_IN  OMX_HANDLETYPE hComponent,
             OMX_OUT OMX_STATETYPE* pState)
{
    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, pState, 1);
    
    *pState = pMyData->state;

OMX_CONF_CMD_BAIL:  
    return eError;

}

extern "C" OMX_ERRORTYPE OMX_ADEC_GetExtensionIndex(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_STRING cParameterName, OMX_OUT OMX_INDEXTYPE * pIndexType)
{
    LOGV(" OMX_ADEC_GetExtensionIndex \n");
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    return eError;
}

extern "C" OMX_ERRORTYPE OMX_ADEC_GetParameter(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pParamStruct)

{
    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, pParamStruct, 1);

    if (pMyData->state == OMX_StateInvalid)
    {
        ALOGE(" %s [%d]: INVALID STATE BAILING OUT\n",__FUNCTION__,__LINE__);
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);
    }

    switch (nParamIndex)
    {
        case OMX_IndexParamPortDefinition:
        {
            ALOGE(" %s [%d] OMX_IndexParamPortDefinition: \n",__FUNCTION__,__LINE__);

            if (((OMX_PARAM_PORTDEFINITIONTYPE *)(pParamStruct))->nPortIndex == pMyData->sInPortDef.nPortIndex)
            {
                BKNI_Memcpy(pParamStruct, &pMyData->sInPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
            }else if (((OMX_PARAM_PORTDEFINITIONTYPE *)(pParamStruct))->nPortIndex == pMyData->sOutPortDef.nPortIndex) {
                BKNI_Memcpy(pParamStruct, &pMyData->sOutPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
            }else {
                eError = OMX_ErrorBadPortIndex;
            }
        }
        break;

       case OMX_IndexParamAudioPcm:
       {
            ALOGE(" %s [%d] OMX_IndexParamAudioPcm: \n",__FUNCTION__,__LINE__);

            OMX_AUDIO_PARAM_PCMMODETYPE *pcmParams =
                (OMX_AUDIO_PARAM_PCMMODETYPE *)pParamStruct;

            if (pcmParams->nPortIndex > 1)
            {
                ALOGE(" %s [%d] Invalid PCM Params: \n",__FUNCTION__,__LINE__);
                return OMX_ErrorUndefined;
            }

            pcmParams->eNumData = OMX_NumericalDataSigned;
            pcmParams->eEndian = OMX_EndianLittle;
            pcmParams->bInterleaved = OMX_TRUE;
            pcmParams->nBitPerSample = 16;
            pcmParams->ePCMMode = OMX_AUDIO_PCMModeLinear;
            pcmParams->eChannelMapping[0] = OMX_AUDIO_ChannelLF;
            pcmParams->eChannelMapping[1] = OMX_AUDIO_ChannelRF;
            pcmParams->nChannels = 2; 
            pcmParams->nSamplingRate = 44100;   
        }

        break;
        case OMX_IndexParamAudioAac:
        {
            OMX_AUDIO_PARAM_AACPROFILETYPE *aacParams = 
                (OMX_AUDIO_PARAM_AACPROFILETYPE *)pParamStruct;

            if (aacParams->nPortIndex !=0 ) 
            {
                eError = OMX_ErrorUndefined; 
                break;
            }

            ALOGD("%s %d: OMX_IndexParamAudioAac Port[%d] Returning AACFormat[%d] SampleRate:%d AACProfile:%d Channels:%d",
                  __FUNCTION__,
                  __LINE__,
                  aacParams->nPortIndex,
                  pMyData->BCMAacParams.eAACStreamFormat,
                  pMyData->BCMAacParams.nSampleRate,    
                  pMyData->BCMAacParams.eAACProfileType,
                  pMyData->BCMAacParams.nChannels);      

            aacParams->eAACStreamFormat = pMyData->BCMAacParams.eAACStreamFormat;
            aacParams->nSampleRate      = pMyData->BCMAacParams.nSampleRate;
            aacParams->eAACProfile      = pMyData->BCMAacParams.eAACProfileType;
            aacParams->nChannels        = pMyData->BCMAacParams.nChannels;
            eError = OMX_ErrorNone; 

        }
        break;

        case OMX_IndexParamPriorityMgmt:
            ALOGE(" %s [%d] OMX_IndexParamPriorityMgmt: \n",__FUNCTION__,__LINE__);
            BKNI_Memcpy(pParamStruct, &pMyData->sPriorityMgmt, sizeof(OMX_PRIORITYMGMTTYPE));
        break;
        default:
            ALOGE(" %s [%d] Default Case [%d]: \n",__FUNCTION__,__LINE__,nParamIndex);
            eError = OMX_ErrorUnsupportedIndex;
        break;
       }

OMX_CONF_CMD_BAIL:  
    return eError;
}

static void PrintPortDefInfo(OMX_PARAM_PORTDEFINITIONTYPE   *pPortDef)
{
    if (pPortDef) 
    {
        ALOGD("%s: nPortIndex:%d nBufferCountActual:%d nBufferCountMin:%d nBufferSize:%d ",
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

        ALOGD(">>>>>>>>>>>>>>>> nBufferAlignment:%d MimeType:%s NativeRender:%p ErrorConcelment:%d AudioEnCoding:%d",
              pPortDef->nBufferAlignment,
              pPortDef->format.audio.cMIMEType,
              pPortDef->format.audio.pNativeRender,
              pPortDef->format.audio.bFlagErrorConcealment,
              pPortDef->format.audio.eEncoding);
    }
}

static 
void PrintAACInputInfo(const OMX_AUDIO_PARAM_AACPROFILETYPE * pAACParams)
{
    if (pAACParams) 
    {
        ALOGD("%s:[%d]: PortIndex:%d nChannles:%d nSampleRate:%d nBitRate:%d nAudioBW:%d nFrameLen:%d eAACProfile:%d eStreamFmt:%d eChannelMode:%d",
              __FUNCTION__,__LINE__,pAACParams->nPortIndex,pAACParams->nChannels,
              pAACParams->nSampleRate,pAACParams->nBitRate,pAACParams->nAudioBandWidth,
              pAACParams->nFrameLength,pAACParams->eAACProfile,
              pAACParams->eAACStreamFormat,pAACParams->eChannelMode);
    }
}

extern "C" OMX_ERRORTYPE OMX_ADEC_SetParameter(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pParamStruct)
{
    
    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    
    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, pParamStruct, 1);

    if (pMyData->state != OMX_StateLoaded)
       OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);

    switch (nParamIndex)
    {
        case OMX_IndexParamPortDefinition:
            ALOGD("%s %d: OMX_IndexParamPortDefinition",__FUNCTION__,__LINE__);

            if (((OMX_PARAM_PORTDEFINITIONTYPE *)(pParamStruct))->nPortIndex  == pMyData->sInPortDef.nPortIndex)
            {
                //Right now we are blindly copying the structures which we cannot do.
                // Look for the parameter to detect what has changed. There can be change in 
                // Size of the output buffer etc.

                PrintPortDefInfo((OMX_PARAM_PORTDEFINITIONTYPE *)(pParamStruct));
                BKNI_Memcpy(&pMyData->sInPortDef, pParamStruct, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));

            }else if (((OMX_PARAM_PORTDEFINITIONTYPE *)(pParamStruct))->nPortIndex == pMyData->sOutPortDef.nPortIndex) {
                //Right now we are blindly copying the structures which we cannot do.
                // Look for the parameter to detect what has changed. There can be change in 
                // Size of the output buffer etc.
                PrintPortDefInfo((OMX_PARAM_PORTDEFINITIONTYPE *)(pParamStruct));
                BKNI_Memcpy(&pMyData->sOutPortDef, pParamStruct, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
            }else{
                eError = OMX_ErrorBadPortIndex;
            }

        break;
           
        case OMX_IndexParamPriorityMgmt:
            ALOGD("%s %d: OMX_IndexParamPriorityMgmt",__FUNCTION__,__LINE__);
            BKNI_Memcpy(&pMyData->sPriorityMgmt, pParamStruct, sizeof (OMX_PRIORITYMGMTTYPE));
        break;
        case OMX_IndexParamStandardComponentRole:
        {
            eError = OMX_ErrorNone;
            const OMX_PARAM_COMPONENTROLETYPE *pRolesParam = (OMX_PARAM_COMPONENTROLETYPE *)pParamStruct;
            if( strncmp((const char *)pRolesParam->cRole,"audio_decoder.aac",OMX_MAX_STRINGNAME_SIZE-1))
            {
                eError = OMX_ErrorUndefined;
            }
        }
        break; 

        case OMX_IndexParamAudioAac:
        {
            ALOGD("%s %d: OMX_IndexParamAudioAac",__FUNCTION__,__LINE__);

            const OMX_AUDIO_PARAM_AACPROFILETYPE *aacParams = 
                (const OMX_AUDIO_PARAM_AACPROFILETYPE *)pParamStruct;

            if (aacParams->nPortIndex !=0 ) 
            {
                eError = OMX_ErrorUndefined; 
                break;
            }
            
            PrintAACInputInfo(aacParams);
            pMyData->BCMAacParams.nSampleRate = aacParams->nSampleRate;
            pMyData->BCMAacParams.nChannels = aacParams->nChannels;
            pMyData->BCMAacParams.eAACStreamFormat = aacParams->eAACStreamFormat;
            pMyData->BCMAacParams.eAACProfileType = aacParams->eAACProfile;

			eError = OMX_ErrorNone;

            if( (aacParams->eAACStreamFormat != OMX_AUDIO_AACStreamFormatMP2ADTS) && 
                (aacParams->eAACStreamFormat != OMX_AUDIO_AACStreamFormatMP4FF) && 
                (aacParams->eAACStreamFormat != OMX_AUDIO_AACStreamFormatRAW)  )
            {
                ALOGD("%s %d: OMX_IndexParamAudioAac: UnSupported AAC Input Format[%d]",
                      __FUNCTION__,__LINE__,
                      aacParams->eAACStreamFormat);

                eError = OMX_ErrorUnsupportedSetting;
                break;
            }
        }
        break;

        default:
            ALOGD("%s %d: Default Case For UnSupported Index[%d]",__FUNCTION__,__LINE__,nParamIndex);
            eError = OMX_ErrorUnsupportedIndex;
        break;
    }
    
OMX_CONF_CMD_BAIL:  
    return eError;
}

extern "C" OMX_ERRORTYPE OMX_ADEC_UseBuffer(OMX_IN OMX_HANDLETYPE hComponent,
              OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
              OMX_IN OMX_U32 nPortIndex,
              OMX_IN OMX_PTR pAppPrivate,
              OMX_IN OMX_U32 nSizeBytes,
              OMX_IN OMX_U8* pBuffer)
{  
    
    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    OMX_U32 nIndex = 0x0;
    
    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, ppBufferHdr, pBuffer);

    if (nPortIndex == pMyData->sInPortDef.nPortIndex)
        pPortDef = &pMyData->sInPortDef;

    else if (nPortIndex == pMyData->sOutPortDef.nPortIndex)
        pPortDef = &pMyData->sOutPortDef;

    else
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadParameter);

    if (!pPortDef->bEnabled)
       OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);

    if (nSizeBytes != pPortDef->nBufferSize || pPortDef->bPopulated)
       OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadParameter);  
    
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
          {
              ALOGE("%s: Failed To Allocate Memory For Buffer Header",__FUNCTION__);
              OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);
          }
          
          OMX_CONF_INIT_STRUCT_PTR (pMyData->sInBufList.pBufHdr[nIndex], OMX_BUFFERHEADERTYPE);
       }  

//Now Allocate the memory for Input context That we maintain for every Input IO
       pMyData->sInBufList.pBufHdr[nIndex]->pInputPortPrivate = 
                       (OMX_PTR) BKNI_Malloc(sizeof(NEXUS_INPUT_CONTEXT));

       if (!pMyData->sInBufList.pBufHdr[nIndex]->pInputPortPrivate) 
       {
           ALOGE("%s: Failed To Allocate Nexus Memory For CONTEXT ",__FUNCTION__);
           OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);
       }

//Now Allocate the Memory For The PES buffer Which will be consumed by the PES buffer

       PNEXUS_INPUT_CONTEXT pInNxCnxt = (PNEXUS_INPUT_CONTEXT )
                                            pMyData->sInBufList.pBufHdr[nIndex]->pInputPortPrivate;

       pInNxCnxt->SzPESBuffer = PES_BUFFER_SIZE(nSizeBytes);

       pInNxCnxt->PESData = (OMX_U8 *)
           pMyData->pPESFeeder->AllocatePESBuffer(pInNxCnxt->SzPESBuffer);

       pMyData->sInBufList.pBufHdr[nIndex]->pBuffer = pBuffer;

       ALOGE("%s %d: PortIndex:%d BufferIndex:%d pBuffHeader:%p pBuffHeader->pBuffer:%p pInNxCnxt:%p pInNxCnxt->PESData[Sz]:%p[%d] ",
             __FUNCTION__,__LINE__,
             nPortIndex,nIndex,pMyData->sInBufList.pBufHdr[nIndex],pBuffer,
             pInNxCnxt,pInNxCnxt->PESData,pInNxCnxt->SzPESBuffer); 

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

       ALOGE("%s %d: PortIndex:%d BufferIndex:%d pBuffHeader:%p  pBuffHeader->pBuffer:%p",
             __FUNCTION__,__LINE__,
             nPortIndex,nIndex,pMyData->sInBufList.pBufHdr[nIndex],
             pBuffer); 

       LoadBufferHeader(pMyData->sOutBufList, pMyData->sOutBufList.pBufHdr[nIndex],  
                                   pAppPrivate, nSizeBytes, nPortIndex, *ppBufferHdr, pPortDef);
    }  

OMX_CONF_CMD_BAIL:  
    return eError;
}


extern "C" OMX_ERRORTYPE OMX_ADEC_AllocateBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                   OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
                   OMX_IN OMX_U32 nPortIndex,
                   OMX_IN OMX_PTR pAppPrivate,
                   OMX_IN OMX_U32 nSizeBytes)
{
    
    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_S8 nIndex = 0x0;
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, ppBufferHdr, 1);

    if (nPortIndex == pMyData->sInPortDef.nPortIndex)
    {
        pPortDef = &pMyData->sInPortDef;
    } else if (nPortIndex == pMyData->sOutPortDef.nPortIndex) {
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
        while (1) 
        {
            ALOGE("%s: Allocating Memory On Input Port--- ERROR WE ARE NOT ADVERTISING THIS QUIRK !!", 
              __FUNCTION__);

            BKNI_Sleep(10);
        }

//      ListAllocate(pMyData->sInBufList,  nIndex);
//
//      /*Allocate the buffer header if its not there already*/
//      if (pMyData->sInBufList.pBufHdr[nIndex] == NULL)
//      {
//          pMyData->sInBufList.pBufHdr[nIndex] = (OMX_BUFFERHEADERTYPE*)
//                              BKNI_Malloc(sizeof(OMX_BUFFERHEADERTYPE)) ;
//
//          if (!pMyData->sInBufList.pBufHdr[nIndex])
//              OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);
//
//          OMX_CONF_INIT_STRUCT_PTR (pMyData->sInBufList.pBufHdr[nIndex], OMX_BUFFERHEADERTYPE);
//      }
//
//      // Allocate the CPU accessible buffer now in which the client will fill in the
//      // data.
//      pMyData->sInBufList.pBufHdr[nIndex]->pBuffer = (OMX_U8*)
//                                     BKNI_Malloc(nSizeBytes);
//
//      if (!pMyData->sInBufList.pBufHdr[nIndex]->pBuffer)
//      {
//          ALOGE("%s: Failed To Allocate Nexus Memory For Input Buffer",__FUNCTION__);
//          OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);
//      }
//
//      // Allocate the memory for the input context in which we keep the context for
//      // Every input frame that we fire.
//      pMyData->sInBufList.pBufHdr[nIndex]->pInputPortPrivate =
//                      (OMX_PTR) BKNI_Malloc(sizeof(NEXUS_INPUT_CONTEXT));
//
//      if (!pMyData->sInBufList.pBufHdr[nIndex]->pInputPortPrivate)
//      {
//          ALOGE("%s: Failed To Allocate Nexus Memory For INPUT_CONTEXT ",__FUNCTION__);
//          OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);
//      }
//
//      PNEXUS_INPUT_CONTEXT pInNxCnxt =
//          (PNEXUS_INPUT_CONTEXT )
//              pMyData->sInBufList.pBufHdr[nIndex]->pInputPortPrivate;
//
//      pInNxCnxt->SzPESBuffer = PES_BUFFER_SIZE(nSizeBytes);
//
//      pInNxCnxt->PESData = (OMX_U8 *)
//          pMyData->pPESFeeder->AllocatePESBuffer(pInNxCnxt->SzPESBuffer);
//
//      if (!pInNxCnxt->PESData)
//      {
//          ALOGE("%s: Failed To Allocate Nexus Memory For Input Buffer PES Header",__FUNCTION__);
//          OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);
//      }
//
//      LoadBufferHeader(pMyData->sInBufList, pMyData->sInBufList.pBufHdr[nIndex], pAppPrivate,
//                      nSizeBytes, nPortIndex, *ppBufferHdr, pPortDef);
    }
    else
    {
        while (1) 
        {
            ALOGE("%s: Allocating Memory On Output Port--- ERROR WE ARE NOT ADVERTISING THIS QUIRK !!", 
              __FUNCTION__);

            BKNI_Sleep(10);
        }

/********************************************
       ListAllocate(pMyData->sOutBufList,  nIndex);
       if (pMyData->sOutBufList.pBufHdr[nIndex] == NULL)
       {         
          pMyData->sOutBufList.pBufHdr[nIndex] = (OMX_BUFFERHEADERTYPE*) 
                                                BKNI_Malloc(sizeof(OMX_BUFFERHEADERTYPE)) ;
          
          if (!pMyData->sOutBufList.pBufHdr[nIndex])
             OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);  

          OMX_CONF_INIT_STRUCT_PTR(pMyData->sOutBufList.pBufHdr[nIndex],OMX_BUFFERHEADERTYPE);    
       }  
       
       pMyData->sOutBufList.pBufHdr[nIndex]->pBuffer = (OMX_U8*) 
                                                            BKNI_Malloc(nSizeBytes);
       if (!pMyData->sOutBufList.pBufHdr[nIndex]->pBuffer) 
          OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorInsufficientResources);
       
       LoadBufferHeader(pMyData->sOutBufList, pMyData->sOutBufList.pBufHdr[nIndex],  
                                   pAppPrivate, nSizeBytes, nPortIndex, *ppBufferHdr, pPortDef);
 ****************************************************/
    }  

OMX_CONF_CMD_BAIL:  
    return eError;
}

extern "C" OMX_ERRORTYPE OMX_ADEC_FreeBuffer(OMX_IN  OMX_HANDLETYPE hComponent,
               OMX_IN  OMX_U32 nPortIndex,
               OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHdr)
{   
    
    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    
    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, pBufferHdr, 1);
    OMX_CONF_CHK_VERSION(pBufferHdr, OMX_BUFFERHEADERTYPE, eError);


    ALOGE("%s [%d]: Freeing %p On Port nPortIndex:%d",
          __FUNCTION__,__LINE__,pBufferHdr,nPortIndex);

    // Match the pBufferHdr to the appropriate entry in the BufferList 
    // and free the allocated memory 
    if (nPortIndex == pMyData->sInPortDef.nPortIndex)
    {
        // There is a custom allocation on the input side.
        // We need custom free as well.
       pPortDef = &pMyData->sInPortDef;
       
// We do not care about the pBuffer as we are using UseBuffer and Not AllocateBuffer
//     if(pBufferHdr->pBuffer)
//     {
//         ALOGE("%s [%d]: ",__FUNCTION__,__LINE__);
//         BKNI_Free(pBufferHdr->pBuffer);
//         ALOGE("%s [%d]: ",__FUNCTION__,__LINE__);
//         pBufferHdr->pBuffer=NULL;
//     }

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

       //ListFreeBuffer Frees the buffer header only
       ListFreeBuffer(pMyData->sInBufList, pBufferHdr, pPortDef)
       ALOGD("%s[%d]: pMyData->sInPortDef.bPopulated:%d",
             __FUNCTION__,__LINE__,pMyData->sInPortDef.bPopulated);

    } else if (nPortIndex == pMyData->sOutPortDef.nPortIndex) {
        pPortDef = &pMyData->sOutPortDef;
        //ListFreeBuffer Frees the buffer header only
        ListFreeBuffer(pMyData->sOutBufList, pBufferHdr, pPortDef)

        ALOGD("%s[%d]: pMyData->sOutPortDef.bPopulated:%d",
              __FUNCTION__,__LINE__,pMyData->sOutPortDef.bPopulated);

    } else {
        OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadParameter);
    }

    if (pPortDef->bEnabled && pMyData->state != OMX_StateIdle)
       OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);

OMX_CONF_CMD_BAIL:  
    return eError;
}

extern "C" OMX_ERRORTYPE OMX_ADEC_EmptyThisBuffer(OMX_IN  OMX_HANDLETYPE hComponent,
                OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHdr)
{
    BCM_OMX_CONTEXT *pMyData;
    ThrCmdType eCmd = EmptyBuf;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
	size_t SzValidPESData = 0;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    PNEXUS_INPUT_CONTEXT pNxInputCnxt = 
               (PNEXUS_INPUT_CONTEXT)pBufferHdr->pInputPortPrivate;

    OMX_CONF_CHECK_CMD(pMyData, pBufferHdr, 1);
    OMX_CONF_CHK_VERSION(pBufferHdr, OMX_BUFFERHEADERTYPE, eError);

    if (!pMyData->sInPortDef.bEnabled)
       OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);

    if (pBufferHdr->nInputPortIndex != 0x0  || pBufferHdr->nOutputPortIndex != OMX_NOPORT)
       OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorBadPortIndex);

    if (pMyData->state != OMX_StateExecuting && pMyData->state != OMX_StatePause)
       OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorIncorrectStateOperation);

    if (!pBufferHdr->pInputPortPrivate) 
    {
        ALOGE("%s: Invalid Param - ",__FUNCTION__);
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

#ifdef DUMP_INPUT_DATA    
    if (pDumpData) 
    {
        if (FRAMES_TO_DUMP == pDumpData->WriteData(pBufferHdr->pBuffer,
                                                        pBufferHdr->nFilledLen))
        {
            delete pDumpData;
            pDumpData=NULL;
        }
    }
#endif
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

    if (pBufferHdr->nFilledLen) 
    {
        if (pBufferHdr->nFlags & OMX_BUFFERFLAG_CODECCONFIG) 
        {
            if(false == pMyData->pPESFeeder->SaveCodecConfigData(pBufferHdr->pBuffer,pBufferHdr->nFilledLen))
            {
                ALOGE("%s: ==ERROR== Failed To Save The Config Data, Flush May Not work",__FUNCTION__);
            }
        }

//      size_t SzValidPESData = pMyData->pPESFeeder->ProcessESData(pBufferHdr->nTimeStamp,
//                                                              pBufferHdr->pBuffer,
//                                                              pBufferHdr->nFilledLen,
//                                                              pNxInputCnxt->PESData,
//                                                              pNxInputCnxt->SzPESBuffer);


		if (OMX_COMPRESSION_FORMAT==OMX_AUDIO_CodingAAC)
		{
			CreateADTSHeader(pMyData,pBufferHdr->nFilledLen);
			SzValidPESData = pMyData->pPESFeeder->ProcessESData(pBufferHdr->nTimeStamp,
																	   (unsigned char *)&pMyData->BCMAacParams.BCMAdtsHeader,
																		sizeof(pMyData->BCMAacParams.BCMAdtsHeader),	
																		pBufferHdr->pBuffer,
																		pBufferHdr->nFilledLen,
																		pNxInputCnxt->PESData,
																		pNxInputCnxt->SzPESBuffer);
		}
		else
		{
			SzValidPESData = pMyData->pPESFeeder->ProcessESData(pBufferHdr->nTimeStamp,
																		pBufferHdr->pBuffer,
																		pBufferHdr->nFilledLen,
																		pNxInputCnxt->PESData,
																		pNxInputCnxt->SzPESBuffer);
		}

        // This is valid data size need to transfer to hardware
        pNxInputCnxt->SzValidPESData = SzValidPESData; 
    }

    // Put the command and data in the pipe 
    write(pMyData->cmdpipe[1], &eCmd, sizeof(eCmd));
    write(pMyData->cmddatapipe[1], &pBufferHdr, sizeof(OMX_BUFFERHEADERTYPE*));

OMX_CONF_CMD_BAIL:  
    return eError;

}

extern "C" OMX_ERRORTYPE OMX_ADEC_FillThisBuffer(OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE * pBufferHdr)
{
  
    LOGV("OMX_ADEC_FillThisBuffer");
    
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


//  if (pBufferHdr)
//  {
//      pBufferHdr->nFilledLen = 0;// pMyData->sOutPortDef.nBufferSize;
//      BKNI_Memset(pBufferHdr->pBuffer, 0,pBufferHdr->nFilledLen);
//      pMyData->pCallbacks->FillBufferDone(pMyData->hSelf, pMyData->pAppData, pBufferHdr);
//      pBufferHdr = NULL;
//  }
    
   //pMyData->pOMXNxAudioDec->RetriveFrameFromHardware();

    //Put the command and data in the pipe
   write(pMyData->cmdpipe[1], &eCmd, sizeof(eCmd));
   write(pMyData->cmddatapipe[1], &pBufferHdr, sizeof(OMX_BUFFERHEADERTYPE*));

OMX_CONF_CMD_BAIL:  
    return eError;
}


extern "C" OMX_ERRORTYPE OMX_ADEC_SetCallbacks(OMX_IN  OMX_HANDLETYPE hComponent,
                 OMX_IN  OMX_CALLBACKTYPE* pCallbacks, 
                 OMX_IN  OMX_PTR pAppData)

{
    LOGV("OMX_ADEC_SetCallbacks");
    BCM_OMX_CONTEXT *pMyData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pMyData = (BCM_OMX_CONTEXT *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    OMX_CONF_CHECK_CMD(pMyData, pCallbacks, pAppData);

    pMyData->pCallbacks = pCallbacks;
    pMyData->pAppData = pAppData;
    
OMX_CONF_CMD_BAIL:  
    return eError;
}

static bool
FlushInput(BCM_OMX_CONTEXT *pBcmContext)
{
    ALOGD("%s: ENTER =====\n\n",__FUNCTION__);
    if(pBcmContext->pPESFeeder)
    {
        if(false == pBcmContext->pPESFeeder->Flush())
        {
            ALOGE("==FLUSH FAILED ON INPUT PORT==");
            return false;
        }
    }

    ListFlushEntries(pBcmContext->sInBufList, pBcmContext);
    ALOGD("%s: EXIT=====\n\n",__FUNCTION__);
    return true;
}

//Need to revisit this
static bool
FlushOutput(BCM_OMX_CONTEXT *pBcmContext)
{
    ALOGD("%s: ENTER =====\n\n",__FUNCTION__);

    if ( false == pBcmContext->pOMXNxAudioDec->Flush())
    {
        ALOGE("==FLUSH FAILED ON Output PORT==");
        return false;
    }

    //Add the code to flush the decoder here
    ListFlushEntries(pBcmContext->sOutBufList, pBcmContext);
    ALOGD("%s: EXIT =====\n\n",__FUNCTION__);
    return true;
}

static void* ComponentThread(void* pThreadData)
{
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
                ALOGD("%s: SET STATE COMMAND: To %d\n",COMPONENT_NAME, cmddata);

                // If the parameter states a transition to the same state
                //   raise a same state transition error. 
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
                                    if (!pMyData->sInPortDef.bPopulated && !pMyData->sOutPortDef.bPopulated)
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
                                    } else if(nTimeout < OMX_MAX_TIMEOUTS) {
                                        BKNI_Sleep(1);
                                    }
                                }

                            } else {

                                ALOGE("%s %d: State Transition Event: [To: State_Loaded] [ERROR-InCorrectTransition] [Timeouts:%d]",
                                      __FUNCTION__,__LINE__,nTimeout);

                                pMyData->pCallbacks->EventHandler(pMyData->hSelf,pMyData->pAppData, 
                                                                  OMX_EventError, 
                                                                  OMX_ErrorIncorrectStateTransition, 
                                                                  0 , NULL);
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
                                                                    OMX_EventError, 
                                                                    OMX_ErrorIncorrectStateTransition, 
                                                                    0 , NULL);

                            }else {
                                // Return buffers if currently in pause and executing
                                if (pMyData->state == OMX_StatePause || pMyData->state == OMX_StateExecuting)
                                {  
//                                  ListFlushEntries(pMyData->sInBufList, pMyData)
//                                  ListFlushEntries(pMyData->sOutBufList, pMyData)
                                    FlushInput(pMyData);
                                    FlushOutput(pMyData);
                                }
                                nTimeout = 0x0;
                                while (1)
                                {

                                    /* Ports have to be populated before transition completes*/
                                    if ((!pMyData->sInPortDef.bEnabled &&  !pMyData->sOutPortDef.bEnabled)||              
                                                            (pMyData->sInPortDef.bPopulated && 
                                                                        pMyData->sOutPortDef.bPopulated)) 
                                    {
                                        pMyData->state = OMX_StateIdle;
                                        ALOGD("%s %d: State Transition Event: [To: OMX_StateIdle] [Completed]",
                                                    __FUNCTION__,__LINE__);

                                        pMyData->pCallbacks->EventHandler(pMyData->hSelf,pMyData->pAppData, OMX_EventCmdComplete, 
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
//                                  ListFlushEntries(pMyData->sInBufList, pMyData)
//                                  ListFlushEntries(pMyData->sOutBufList, pMyData)
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
                            if (pMyData->state == OMX_StateIdle || pMyData->state == OMX_StateExecuting)
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
                            } 
                            else
                                pMyData->pCallbacks->EventHandler(pMyData->hSelf,pMyData->pAppData, 
                                            OMX_EventError, OMX_ErrorIncorrectStateTransition, 0 , NULL);
                        break;
                        }
                        default :
                            LOGI("### Invalid State \n");
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
//                  ListFlushEntries(pMyData->sInBufList, pMyData)

                    FlushInput(pMyData);
                    // Disable port 
                    pMyData->sInPortDef.bEnabled = OMX_FALSE;
                }
                if (cmddata == 0x1 || cmddata == -1)
                {
                    // Return all output buffers 
//                  ListFlushEntries(pMyData->sOutBufList, pMyData)

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

                ALOGD("%s: RESTART PORT PortIndex : %d", __FUNCTION__, cmddata);

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
                        pMyData->pCallbacks->EventHandler(pMyData->hSelf,pMyData->pAppData, 
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
                if (cmddata == 0x0 || cmddata == -1)
                {
                    // Return all input buffers and send cmdcomplete
                    //ListFlushEntries(pMyData->sInBufList, pMyData)
                    FlushInput(pMyData);
                    pMyData->pCallbacks->EventHandler(pMyData->hSelf, pMyData->pAppData, 
                                              OMX_EventCmdComplete, OMX_CommandFlush, 0x0, NULL);
                }
                if (cmddata == 0x1 || cmddata == -1)
                {
                    // Return all output buffers and send cmdcomplete
                    //ListFlushEntries(pMyData->sOutBufList, pMyData)
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
        
        // Buffer processing starts
        // Only happens when the component is in executing state. 
        
        if (pMyData->state == OMX_StateExecuting && pMyData->sInPortDef.bEnabled && 
                            (pMyData->sInBufList.nSizeOfList > 0))
        {
            ListGetEntry(pMyData->sInBufList, pInBufHdr)
            // If there is no output buffer, get one from list 
            // Check for EOS flag 
            if (pInBufHdr)
            {
                if (pInBufHdr->nFlags & OMX_BUFFERFLAG_EOS)
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
                    pMyData->pPESFeeder->NotifyEOS(pInBufHdr->nFlags,pInBufHdr->nTimeStamp);

                    pInBufHdr->nFlags = 0;
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
                    pMyData->pCallbacks->EventHandler(pMyData->hSelf, 
                                                      pMyData->pAppData, 
                                                      OMX_EventMark, 
                                                      0, 0, 
                                                      pInBufHdr->pMarkData); 
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
                DELIVER_AUDIO_FRAME DeliverFr;
                DeliverFr.OutFlags=0;
                DeliverFr.pAudioDataBuff = pOutBufHdr->pBuffer;
                DeliverFr.SzAudDataBuff = pOutBufHdr->nAllocLen;
                DeliverFr.SzFilled=0;
                DeliverFr.usTimeStamp=0;

                //The function also copies the data to the application.
                if(pMyData->pOMXNxAudioDec->GetDecodedFrame(&DeliverFr))
                {
                    BCMOMX_DBG_ASSERT(DeliverFr.SzFilled);
                    pOutBufHdr->nFilledLen = DeliverFr.SzFilled;
                    pOutBufHdr->nTimeStamp = DeliverFr.usTimeStamp;

#ifdef DUMP_OUTPUT_DATA    
                    pDumpData->WriteData(pOutBufHdr->pBuffer,pOutBufHdr->nFilledLen);    
#endif

                }else{
                    if (DeliverFr.OutFlags & OMX_BUFFERFLAG_EOS) 
                    {
                        ALOGD("EOS MARKED ON THE OUTPUT FRAME:%d",DeliverFr.OutFlags);
                        pOutBufHdr->nFlags |= OMX_BUFFERFLAG_EOS;
                        EOSMarkerFlags = DeliverFr.OutFlags;
                    }
                    
                    pOutBufHdr->nFilledLen=0;
                }

                pMyData->pCallbacks->FillBufferDone(pMyData->hSelf,
                                                       pMyData->pAppData,
                                                       pOutBufHdr);

                pOutBufHdr = NULL;
                //Generate Additional Event in Case the EOS Flag + Any Other Flag Was Set.
                if (EOSMarkerFlags) 
                {
                    ALOGD("%s %d: EOS Event: [Emitted] Flags:0x%x",
                            __FUNCTION__,__LINE__,EOSMarkerFlags);

                    pMyData->pCallbacks->EventHandler(pMyData->hSelf, 
                                                      pMyData->pAppData,
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



