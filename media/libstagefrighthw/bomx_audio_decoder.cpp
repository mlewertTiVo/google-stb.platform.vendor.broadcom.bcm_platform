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
 *
 *****************************************************************************/
//#define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "bomx_audio_decoder"

#include <fcntl.h>
#include <log/log.h>
#include <cutils/properties.h>
#include <cutils/atomic.h>

#include "bomx_audio_decoder.h"
#include "nexus_platform.h"
#include "OMX_IndexExt.h"
#include "OMX_AudioExt.h"
#include "nexus_base_mmap.h"
#include "nexus_audio_decoder.h"
#include "nexus_audio_decode_to_memory.h"
#include "nexus_core_utils.h"
#include "nx_ashmem.h"
#include "bomx_log.h"
#include "bomx_pes_formatter.h"

#define BOMX_INPUT_MSG(x)

#define B_DRC_DEFAULT_MODE ("rf")
#define B_DRC_DEFAULT_MODE_DD ("rf_23")
#define B_DRC_DEFAULT_REF_LEVEL (92)    // -23dB - SW decoder targets -16dB with higher compression for mobile devices but this leaves enough headroom to avoid saturation
#define B_DRC_DEFAULT_CUT (127)         // Max
#define B_DRC_DEFAULT_BOOST (127)       // Max
#define B_DRC_DEFAULT_ENC_LEVEL -1      // Unknown

#define B_DRC_TO_NEXUS(cutboost) (((cutboost)*100)/127)
#define B_DRC_FROM_NEXUS(cutboost) (((cutboost)*127)/100)

#define B_DATA_BUFFER_SIZE (12288)       // Value may be large for aac/mp3 but it's needed to accommodate high bitrates for eac3
#define B_OUTPUT_BUFFER_SIZE (2048*2*8) // 16-bit 5.1 with 2048 samples/frame for AAC, also fits 7.1 with 1536 samples/frame (ac3/eac3)
#define B_NUM_BUFFERS (6)
#define B_NUM_INPUT_BUFFERS_SECURE (4)
#define B_STREAM_ID 0xc0
#define B_FRAME_TIMER_INTERVAL (32)
#define B_INPUT_BUFFERS_RETURN_INTERVAL (5000)
#define B_AAC_ADTS_HEADER_LEN (7)

#define B_MAX_AUDIO_DECODERS (2)
#define B_MAX_SECURE_AUDIO_DECODERS (1)

/****************************************************************************
 * The descriptors used per-frame are laid out as follows:
 * [PES Header] [Codec Config Data] [Codec Header] [Payload] = 4 descriptors
 * [PES Header] [Payload] Repeats until we have completed the input frame.
 *                        This requires 2 descriptors and we can fit a max
 *                        of 65535-3 bytes of payload in each based on
 *                        the 16-bit PES length field.
 ****************************************************************************/
#define B_MAX_EXTRAFRAMES_PER_BUFFER (0)
#define B_MAX_DESCRIPTORS_PER_BUFFER (4+(B_MAX_EXTRAFRAMES_PER_BUFFER*3)+(2*(B_DATA_BUFFER_SIZE/(B_MAX_PES_PACKET_LENGTH-(B_PES_HEADER_LENGTH_WITHOUT_PTS-B_PES_HEADER_START_BYTES)))))

#define OMX_IndexParamAllocNativeHandle                      0x7F00000B

using namespace android;

enum BOMX_AudioDecoderEventType
{
    BOMX_AudioDecoderEventType_ePlaypump=0,
    BOMX_AudioDecoderEventType_eDataReady,
    BOMX_AudioDecoderEventType_eCheckpoint,
    BOMX_AudioDecoderEventType_eFifoOverflow,
    BOMX_AudioDecoderEventType_eFifoUnderflow,
    BOMX_AudioDecoderEventType_ePlaypumpErrorCallback,
    BOMX_AudioDecoderEventType_ePlaypumpCcError,
    BOMX_AudioDecoderEventType_eMax
};

static int32_t g_instanceNum;
static int32_t g_activeInstancesNum;

static bool g_nxStandBy = false;
static Mutex g_mutexStandBy;

#define ERROR_OUT_ON_NEXUS_ACTIVE_STANDBY \
   {  /* scope for the lock. */                                 \
      Mutex::Autolock autoLock(g_mutexStandBy);                 \
      if (g_nxStandBy)                                          \
         return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources); \
   }

extern "C" bool BOMX_AudioDecoder_StandbyMon(void *ctx)
{
   nxwrap_pwr_state state;
   bool fetch = false;

   (void)ctx;

   Mutex::Autolock autoLock(g_mutexStandBy);
   fetch = nxwrap_get_pwr_info(&state, NULL);

   if (fetch && (state >= ePowerState_S3)) {
      g_nxStandBy = true;
   } else {
      g_nxStandBy = false;
   }

   // always ack'ed okay.
   return true;
}

extern "C" OMX_ERRORTYPE BOMX_AudioDecoder_CreateAc3(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    NEXUS_AudioCapabilities audioCaps;
    NxWrap *pNxWrap = NULL;
    BOMX_AudioDecoder *pAudioDecoder = NULL;

    pNxWrap = new NxWrap(pName);
    if (pNxWrap == NULL)
    {
        ALOGW("Unable to determine presence of AC3 hardware!");
    }
    else
    {
        pNxWrap->join(BOMX_AudioDecoder_StandbyMon, NULL);
        NEXUS_GetAudioCapabilities(&audioCaps);
        if ( !audioCaps.dsp.codecs[NEXUS_AudioCodec_eAc3].decode &&
             !audioCaps.dsp.codecs[NEXUS_AudioCodec_eAc3Plus].decode )
        {
            ALOGW("AC3 hardware support is not available");
            goto error;
        }
    }

    pAudioDecoder = new BOMX_AudioDecoder(pComponentTpe, pName, pAppData, pCallbacks,
                                          pNxWrap,
                                          false, BOMX_AUDIO_GET_ROLE_COUNT(g_ac3Role), g_ac3Role,
                                          BOMX_AudioDecoder_GetRoleAc3);
    if ( NULL == pAudioDecoder )
    {
        goto error;
    }
    else
    {
        OMX_ERRORTYPE constructorError = pAudioDecoder->IsValid();
        if ( constructorError == OMX_ErrorNone )
        {
            return OMX_ErrorNone;
        }
        else
        {
            delete pAudioDecoder;
            return BOMX_ERR_TRACE(constructorError);
        }
    }

error:
    if (pNxWrap)
    {
        pNxWrap->leave();
        delete pNxWrap;
    }
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

extern "C" const char *BOMX_AudioDecoder_GetRoleAc3(unsigned roleIndex)
{
    return BOMX_AUDIO_GET_ROLE_NAME(g_ac3Role, roleIndex);
}

extern "C" OMX_ERRORTYPE BOMX_AudioDecoder_CreateEAc3(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    NEXUS_AudioCapabilities audioCaps;
    NxWrap *pNxWrap = NULL;
    BOMX_AudioDecoder *pAudioDecoder = NULL;

    pNxWrap = new NxWrap(pName);
    if (pNxWrap == NULL)
    {
        ALOGW("Unable to determine presence of EAC3 hardware!");
    }
    else
    {
        pNxWrap->join(BOMX_AudioDecoder_StandbyMon, NULL);
        NEXUS_GetAudioCapabilities(&audioCaps);
        if ( !audioCaps.dsp.codecs[NEXUS_AudioCodec_eAc3].decode &&
             !audioCaps.dsp.codecs[NEXUS_AudioCodec_eAc3Plus].decode )
        {
            ALOGW("EAC3 hardware support is not available");
            goto error;
        }
    }

    pAudioDecoder = new BOMX_AudioDecoder(pComponentTpe, pName, pAppData, pCallbacks,
                                          pNxWrap,
                                          false, BOMX_AUDIO_GET_ROLE_COUNT(g_eac3Role), g_eac3Role,
                                          BOMX_AudioDecoder_GetRoleEAc3);
    if ( NULL == pAudioDecoder )
    {
        goto error;
    }
    else
    {
        OMX_ERRORTYPE constructorError = pAudioDecoder->IsValid();
        if ( constructorError == OMX_ErrorNone )
        {
            return OMX_ErrorNone;
        }
        else
        {
            delete pAudioDecoder;
            return BOMX_ERR_TRACE(constructorError);
        }
    }

error:
    if (pNxWrap)
    {
        pNxWrap->leave();
        delete pNxWrap;
    }
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

extern "C" const char *BOMX_AudioDecoder_GetRoleEAc3(unsigned roleIndex)
{
    return BOMX_AUDIO_GET_ROLE_NAME(g_eac3Role, roleIndex);
}

extern "C" OMX_ERRORTYPE BOMX_AudioDecoder_CreateMp3(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    NEXUS_AudioCapabilities audioCaps;
    NxWrap *pNxWrap = NULL;
    BOMX_AudioDecoder *pAudioDecoder = NULL;

    pNxWrap = new NxWrap(pName);
    if (pNxWrap == NULL)
    {
        ALOGW("Unable to determine presence of MP3 hardware!");
    }
    else
    {
        pNxWrap->join(BOMX_AudioDecoder_StandbyMon, NULL);
        NEXUS_GetAudioCapabilities(&audioCaps);
        if ( !audioCaps.dsp.codecs[NEXUS_AudioCodec_eMp3].decode &&
             !audioCaps.dsp.codecs[NEXUS_AudioCodec_eMpeg].decode )
        {
            ALOGW("MP3 hardware support is not available");
            goto error;
        }
    }

    pAudioDecoder = new BOMX_AudioDecoder(pComponentTpe, pName, pAppData, pCallbacks,
                                          pNxWrap,
                                          false, BOMX_AUDIO_GET_ROLE_COUNT(g_mp3Role), g_mp3Role,
                                          BOMX_AudioDecoder_GetRoleMp3);
    if ( NULL == pAudioDecoder )
    {
        goto error;
    }
    else
    {
        OMX_ERRORTYPE constructorError = pAudioDecoder->IsValid();
        if ( constructorError == OMX_ErrorNone )
        {
            return OMX_ErrorNone;
        }
        else
        {
            delete pAudioDecoder;
            return BOMX_ERR_TRACE(constructorError);
        }
    }

error:
    if (pNxWrap)
    {
        pNxWrap->leave();
        delete pNxWrap;
    }
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

extern "C" const char *BOMX_AudioDecoder_GetRoleMp3(unsigned roleIndex)
{
    return BOMX_AUDIO_GET_ROLE_NAME(g_mp3Role, roleIndex);
}

extern "C" OMX_ERRORTYPE BOMX_AudioDecoder_CreateAac(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    NEXUS_AudioCapabilities audioCaps;
    NxWrap *pNxWrap = NULL;
    BOMX_AudioDecoder *pAudioDecoder = NULL;

    pNxWrap = new NxWrap(pName);
    if (pNxWrap == NULL)
    {
        ALOGW("Unable to determine presence of AAC hardware!");
    }
    else
    {
        pNxWrap->join(BOMX_AudioDecoder_StandbyMon, NULL);
        NEXUS_GetAudioCapabilities(&audioCaps);
        if ( !audioCaps.dsp.codecs[NEXUS_AudioCodec_eAacAdts].decode &&
             !audioCaps.dsp.codecs[NEXUS_AudioCodec_eAacPlusAdts].decode )
        {
            ALOGW("AAC hardware support is not available");
            goto error;
        }
    }

    pAudioDecoder = new BOMX_AudioDecoder(pComponentTpe, pName, pAppData, pCallbacks,
                                          pNxWrap,
                                          false, BOMX_AUDIO_GET_ROLE_COUNT(g_aacRole), g_aacRole,
                                          BOMX_AudioDecoder_GetRoleAac);
    if ( NULL == pAudioDecoder )
    {
        goto error;
    }
    else
    {
        OMX_ERRORTYPE constructorError = pAudioDecoder->IsValid();
        if ( constructorError == OMX_ErrorNone )
        {
            return OMX_ErrorNone;
        }
        else
        {
            delete pAudioDecoder;
            return BOMX_ERR_TRACE(constructorError);
        }
    }

error:
    if (pNxWrap)
    {
        pNxWrap->leave();
        delete pNxWrap;
    }
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

extern "C" const char *BOMX_AudioDecoder_GetRoleAac(unsigned roleIndex)
{
    return BOMX_AUDIO_GET_ROLE_NAME(g_aacRole, roleIndex);
}

static void BOMX_AudioDecoder_PlaypumpEvent(void *pParam)
{
    BOMX_AudioDecoder *pDecoder = static_cast <BOMX_AudioDecoder *> (pParam);

    BOMX_INPUT_MSG(("PlaypumpEvent"));

    pDecoder->PlaypumpEvent();
    pDecoder->OutputFrameEvent();
}

static void BOMX_AudioDecoder_EventCallback(void *pParam, int param)
{
    B_EventHandle hEvent;

    static const char *pEventMsg[BOMX_AudioDecoderEventType_eMax] = {
        "Playpump",
        "Data Ready",
        "Checkpoint",
        "FifoOverflow",
        "FifoUnderflow",
        "PlaypumpErrorCallback",
        "PlaypumpCcError"
    };

    hEvent = static_cast <B_EventHandle> (pParam);
    if ( param < BOMX_AudioDecoderEventType_eMax )
    {
        ALOGV("EventCallback - %s", pEventMsg[param]);
    }
    else
    {
        ALOGW("Unkonwn EventCallbackType %d", param);
    }

    ALOG_ASSERT(NULL != hEvent);
    B_Event_Set(hEvent);
}

static void BOMX_AudioDecoder_OutputFrameEvent(void *pParam)
{
    BOMX_AudioDecoder *pDecoder = static_cast <BOMX_AudioDecoder *> (pParam);

    ALOGV("OutputFrameEvent");

    pDecoder->OutputFrameEvent();
}

static void BOMX_AudioDecoder_FifoOverflowEvent(void *pParam)
{
    BOMX_AudioDecoder *pDecoder = static_cast <BOMX_AudioDecoder *> (pParam);

    ALOGV("FifoOverflowEvent");

    pDecoder->FifoOverflowEvent();
}

static void BOMX_AudioDecoder_FifoUnderflowEvent(void *pParam)
{
    BOMX_AudioDecoder *pDecoder = static_cast <BOMX_AudioDecoder *> (pParam);

    ALOGV("FifoUnderflowEvent");

    pDecoder->FifoUnderflowEvent();
}

static void BOMX_AudioDecoder_PlaypumpErrorCallbackEvent(void *pParam)
{
    BOMX_AudioDecoder *pDecoder = static_cast <BOMX_AudioDecoder *> (pParam);

    ALOGV("PlaypumpErrorCallbackEvent");

    pDecoder->PlaypumpErrorCallbackEvent();
}

static void BOMX_AudioDecoder_PlaypumpCcErrorEvent(void *pParam)
{
    BOMX_AudioDecoder *pDecoder = static_cast <BOMX_AudioDecoder *> (pParam);

    ALOGV("PlaypumpCcErrorEvent");

    pDecoder->PlaypumpCcErrorEvent();
}

static void BOMX_AudioDecoder_InputBuffersTimer(void *pParam)
{
    BOMX_AudioDecoder *pDecoder = static_cast <BOMX_AudioDecoder *> (pParam);
    pDecoder->OutputFrameEvent();
    pDecoder->InputBufferTimeoutCallback();
}

static OMX_ERRORTYPE BOMX_AudioDecoder_InitMimeType(OMX_AUDIO_CODINGTYPE eCompressionFormat, char *pMimeType)
{
    const char *pMimeTypeStr;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    ALOG_ASSERT(NULL != pMimeType);

    switch ( (int)eCompressionFormat )
    {
    case OMX_AUDIO_CodingAndroidAC3:
        pMimeTypeStr = "audio/ac3";
        break;
    case OMX_AUDIO_CodingAndroidEAC3:
        pMimeTypeStr = "audio/eac3";
        break;
    case OMX_AUDIO_CodingMP3:
        pMimeTypeStr = "audio/mp3";
        break;
    case OMX_AUDIO_CodingAAC:
        pMimeTypeStr = "audio/aac";
        break;
    case OMX_AUDIO_CodingPCM:
        pMimeTypeStr = "audio/x-raw";
        break;
    default:
        ALOGW("Unable to find MIME type for eCompressionFormat %u", eCompressionFormat);
        pMimeTypeStr = "unknown";
        err = OMX_ErrorBadParameter;
        break;
    }
    strncpy(pMimeType, pMimeTypeStr, OMX_MAX_STRINGNAME_SIZE);
    return err;
}

BOMX_AudioDecoder::BOMX_AudioDecoder(
    OMX_COMPONENTTYPE *pComponentType,
    const OMX_STRING pName,
    const OMX_PTR pAppData,
    const OMX_CALLBACKTYPE *pCallbacks,
    NxWrap *pNxWrap,
    bool secure,
    unsigned numRoles,
    const BOMX_AudioDecoderRole *pRoles,
    const char *(*pGetRole)(unsigned roleIndex)
    ) :
    BOMX_Component(pComponentType, pName, pAppData, pCallbacks, pGetRole),
    m_hAudioDecoder(NULL),
    m_hPlaypump(NULL),
    m_hPidChannel(NULL),
    m_hPlaypumpEvent(NULL),
    m_playpumpEventId(NULL),
    m_playpumpTimerId(NULL),
    m_hOutputFrameEvent(NULL),
    m_outputFrameEventId(NULL),
    m_inputBuffersTimerId(NULL),
    m_hFifoOverflowEvent(NULL),
    m_fifoOverflowEventId(NULL),
    m_hFifoUnderflowEvent(NULL),
    m_fifoUnderflowEventId(NULL),
    m_hPlaypumpErrorCallbackEvent(NULL),
    m_playpumpErrorCallbackEventId(NULL),
    m_hPlaypumpCcErrorEvent(NULL),
    m_playpumpCcErrorEventId(NULL),
    m_submittedDescriptors(0),
    m_decoderState(BOMX_AudioDecoderState_eStopped),
    m_pBufferTracker(NULL),
    m_AvailInputBuffers(0),
    m_pNxWrap(pNxWrap),
    m_pPesFile(NULL),
    m_pInputFile(NULL),
    m_pOutputFile(NULL),
    m_pPes(NULL),
    m_instanceNum(0),
    m_pEosBuffer(NULL),
    m_eosPending(false),
    m_eosDelivered(false),
    m_eosReceived(false),
    m_eosStandalone(false),
    m_eosReady(false),
    m_eosTimeStamp(0),
    m_formatChangePending(false),
    m_secureDecoder(secure),
    m_allocNativeHandle(secure),
    m_pRoles(NULL),
    m_numRoles(0),
    m_pFrameStatus(NULL),
    m_pMemoryBlocks(NULL),
    m_sampleRate(0),
    m_bitsPerSample(0),
    m_numPcmChannels(0),
    m_codecDelayAdjusted(0),
    m_pConfigBuffer(NULL),
    m_configBufferState(ConfigBufferState_eAccumulating),
    m_configBufferSize(0)
{
    unsigned i;
    NEXUS_Error errCode;
    NEXUS_ClientConfiguration clientConfig;
    char property[PROPERTY_VALUE_MAX];
    int32_t temp;

    #define MAX_PORT_FORMATS (2)
    #define MAX_OUTPUT_PORT_FORMATS (1)

    BDBG_CASSERT(MAX_OUTPUT_PORT_FORMATS <= MAX_PORT_FORMATS);

    OMX_AUDIO_PORTDEFINITIONTYPE portDefs;
    OMX_AUDIO_PARAM_PORTFORMATTYPE portFormats[MAX_PORT_FORMATS];

    m_audioPortBase = 0;    // Android seems to require this - was: BOMX_COMPONENT_PORT_BASE(BOMX_ComponentId_eAudioDecoder, OMX_PortDomainAudio);
    m_instanceNum = android_atomic_inc(&g_instanceNum);

    if ( android_atomic_inc(&g_activeInstancesNum) >= (m_secureDecoder ? B_MAX_SECURE_AUDIO_DECODERS : B_MAX_AUDIO_DECODERS) )
    {
        ALOGE("Max Audio Decoders currently exsist");
        this->Invalidate(OMX_ErrorInsufficientResources);
        return;
    }

    if ( !property_get_int32(BCM_RO_MEDIA_ADEC_ENABLED, 1) )
    {
        ALOGD("ADEC Disabled");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    if ( numRoles==0 || pRoles==NULL )
    {
        ALOGE("Must specify at least one role");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }
    if ( numRoles > MAX_PORT_FORMATS )
    {
        ALOGW("Warning, exceeded max number of audio decoder input port formats");
        numRoles = MAX_PORT_FORMATS;
    }

    m_numRoles = numRoles;
    m_pRoles = new BOMX_AudioDecoderRole[numRoles];
    if ( NULL == m_pRoles )
    {
        ALOGE("Unable to allocate role memory");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }
    BKNI_Memcpy(m_pRoles, pRoles, numRoles*sizeof(BOMX_AudioDecoderRole));

    memset(&portDefs, 0, sizeof(portDefs));
    portDefs.eEncoding = (OMX_AUDIO_CODINGTYPE)pRoles[0].omxCodec;
    portDefs.cMIMEType = m_inputMimeType;
    portDefs.bFlagErrorConcealment = OMX_TRUE;
    (void)BOMX_AudioDecoder_InitMimeType(portDefs.eEncoding, m_inputMimeType);
    for ( i = 0; i < numRoles; i++ )
    {
        memset(&portFormats[i], 0, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
        BOMX_STRUCT_INIT(&portFormats[i]);
        portFormats[i].nPortIndex = m_audioPortBase;
        portFormats[i].nIndex = i;
        portFormats[i].eEncoding = (OMX_AUDIO_CODINGTYPE)pRoles[i].omxCodec;
    }

    SetRole(m_pRoles[0].name);
    m_numAudioPorts = 0;
    unsigned numInputBuffers = m_secureDecoder ? B_NUM_INPUT_BUFFERS_SECURE : B_NUM_BUFFERS;
    m_pAudioPorts[0] = new BOMX_AudioPort(m_audioPortBase, OMX_DirInput, numInputBuffers, B_DATA_BUFFER_SIZE, false, 0, &portDefs, portFormats, numRoles);
    if ( NULL == m_pAudioPorts[0] )
    {
        ALOGW("Unable to create audio input port");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }
    m_numAudioPorts = 1;
    memset(&portDefs, 0, sizeof(portDefs));
    portDefs.eEncoding = OMX_AUDIO_CodingPCM;
    portDefs.bFlagErrorConcealment = OMX_FALSE;
    (void)BOMX_AudioDecoder_InitMimeType(OMX_AUDIO_CodingPCM, m_outputMimeType);
    portDefs.cMIMEType = m_outputMimeType;
    for ( i = 0; i < MAX_OUTPUT_PORT_FORMATS; i++ )
    {
        memset(&portFormats[i], 0, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
        BOMX_STRUCT_INIT(&portFormats[i]);
        portFormats[i].nIndex = i;
        portFormats[i].nPortIndex = m_audioPortBase+1;
        portFormats[i].eEncoding = OMX_AUDIO_CodingPCM;
    }
    m_pAudioPorts[1] = new BOMX_AudioPort(m_audioPortBase+1, OMX_DirOutput, B_NUM_BUFFERS, B_OUTPUT_BUFFER_SIZE, false, 0, &portDefs, portFormats, MAX_OUTPUT_PORT_FORMATS);
    if ( NULL == m_pAudioPorts[1] )
    {
        ALOGW("Unable to create audio output port");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }
    m_numAudioPorts = 2;

    // Init AAC Params
    BKNI_Memset(&m_aacParams, 0, sizeof(m_aacParams));
    BOMX_STRUCT_INIT(&m_aacParams);
    m_aacParams.nChannels = 2;

    m_pFrameStatus = new NEXUS_AudioDecoderFrameStatus[B_NUM_BUFFERS];
    if ( NULL == m_pFrameStatus )
    {
        ALOGW("Unable to allocate frame status");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    m_pMemoryBlocks = new NEXUS_MemoryBlockHandle[B_NUM_BUFFERS];
    if ( NULL == m_pMemoryBlocks )
    {
        ALOGW("Unable to allocate memory blocks");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    m_hPlaypumpEvent = B_Event_Create(NULL);
    if ( NULL == m_hPlaypumpEvent )
    {
        ALOGW("Unable to create playpump event");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    m_hOutputFrameEvent = B_Event_Create(NULL);
    if ( NULL == m_hOutputFrameEvent )
    {
        ALOGW("Unable to create output frame event");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    m_outputFrameEventId = this->RegisterEvent(m_hOutputFrameEvent, BOMX_AudioDecoder_OutputFrameEvent, static_cast <void *> (this));
    if ( NULL == m_outputFrameEventId )
    {
        ALOGW("Unable to register output frame event");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    m_hFifoOverflowEvent = B_Event_Create(NULL);
    if ( NULL == m_hFifoOverflowEvent )
    {
        ALOGW("Unable to create audio input fifo overflow event");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    m_fifoOverflowEventId = this->RegisterEvent(m_hFifoOverflowEvent, BOMX_AudioDecoder_FifoOverflowEvent, static_cast <void *> (this));
    if ( NULL == m_fifoOverflowEventId )
    {
        ALOGW("Unable to register audio input fifo overflow event");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    m_hFifoUnderflowEvent = B_Event_Create(NULL);
    if ( NULL == m_hFifoUnderflowEvent )
    {
        ALOGW("Unable to create audio input fifo underflow event");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    m_fifoUnderflowEventId = this->RegisterEvent(m_hFifoUnderflowEvent, BOMX_AudioDecoder_FifoUnderflowEvent, static_cast <void *> (this));
    if ( NULL == m_fifoUnderflowEventId )
    {
        ALOGW("Unable to register audio input fifo underflow event");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    m_hPlaypumpErrorCallbackEvent = B_Event_Create(NULL);
    if ( NULL == m_hPlaypumpErrorCallbackEvent )
    {
        ALOGW("Unable to create playpump error callback event");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    m_hPlaypumpCcErrorEvent = B_Event_Create(NULL);
    if ( NULL == m_hPlaypumpCcErrorEvent )
    {
        ALOGW("Unable to create playpump continuity count  error  event");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    m_pBufferTracker = new BOMX_BufferTracker(this);
    if ( NULL == m_pBufferTracker || !m_pBufferTracker->Valid() )
    {
        ALOGW("Unable to create buffer tracker");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    if (m_pNxWrap == NULL)
    {
        m_pNxWrap = new NxWrap(pName);
        if ( NULL == m_pNxWrap )
        {
            ALOGW("Unable to create client wrap");
            this->Invalidate(OMX_ErrorUndefined);
            return;
        }
        else
        {
            m_pNxWrap->join(BOMX_AudioDecoder_StandbyMon, NULL);
        }
    }

    NEXUS_MemoryAllocationSettings          memorySettings;

    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    NEXUS_Memory_GetDefaultAllocationSettings(&memorySettings);
    memorySettings.heap = clientConfig.heap[NXCLIENT_FULL_HEAP];
    if (memorySettings.heap == NULL)
    {
       memorySettings.heap = clientConfig.heap[NXCLIENT_DEFAULT_HEAP];
    }
    errCode = NEXUS_Memory_Allocate(BOMX_AUDIO_EOS_LEN, &memorySettings, &m_pEosBuffer);
    if ( errCode )
    {
        ALOGW("Unable to allocate EOS buffer");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    m_pPes = new BOMX_PesFormatter(B_STREAM_ID);
    if ( NULL == m_pPes )
    {
        ALOGW("Unable to create PES formatter");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    m_pPes->FormBppPacket((char *)m_pEosBuffer, 0x82); /* LAST */

    NEXUS_AudioDecoderOpenSettings openSettings;
    NEXUS_AudioDecoder_GetDefaultOpenSettings(&openSettings);
    openSettings.type = NEXUS_AudioDecoderType_eDecodeToMemory;
    if (m_secureDecoder)
        openSettings.cdbHeap = clientConfig.heap[NXCLIENT_VIDEO_SECURE_HEAP];
    m_hAudioDecoder = NEXUS_AudioDecoder_Open(NEXUS_ANY_ID, &openSettings);
    if ( NULL == m_hAudioDecoder )
    {
        ALOGE(("Unable to open audio decoder"));
        this->Invalidate(OMX_ErrorInsufficientResources);
        return;
    }

    NEXUS_AudioDecoderSettings audioSettings;
    NEXUS_AudioDecoder_GetSettings(m_hAudioDecoder, &audioSettings);
    audioSettings.fifoOverflow.callback = BOMX_AudioDecoder_EventCallback;
    audioSettings.fifoOverflow.context = static_cast<void *>(m_hFifoOverflowEvent);
    audioSettings.fifoOverflow.param = BOMX_AudioDecoderEventType_eFifoOverflow;
    audioSettings.fifoUnderflow.callback = BOMX_AudioDecoder_EventCallback;
    audioSettings.fifoUnderflow.context = static_cast<void *>(m_hFifoUnderflowEvent);
    audioSettings.fifoUnderflow.param = BOMX_AudioDecoderEventType_eFifoUnderflow;
    NEXUS_AudioDecoder_SetSettings(m_hAudioDecoder, &audioSettings);

    NEXUS_AudioDecoderDecodeToMemorySettings decSettings;
    NEXUS_AudioDecoder_GetDecodeToMemorySettings(m_hAudioDecoder, &decSettings);
    decSettings.bitsPerSample = 16; // Android defaults
    decSettings.numPcmChannels = 2;
    decSettings.maxBuffers = B_NUM_BUFFERS;
    decSettings.channelLayout[0] = NEXUS_AudioChannel_eLeft;
    decSettings.channelLayout[1] = NEXUS_AudioChannel_eRight;
    decSettings.channelLayout[2] = NEXUS_AudioChannel_eCenter;
    decSettings.channelLayout[3] = NEXUS_AudioChannel_eLfe;
    decSettings.channelLayout[4] = NEXUS_AudioChannel_eLeftSurround;
    decSettings.channelLayout[5] = NEXUS_AudioChannel_eRightSurround;
    decSettings.channelLayout[6] = NEXUS_AudioChannel_eLeftRear;
    decSettings.channelLayout[7] = NEXUS_AudioChannel_eRightRear;
    decSettings.bufferComplete.callback = BOMX_AudioDecoder_EventCallback;
    decSettings.bufferComplete.context = static_cast<void *>(m_hOutputFrameEvent);
    decSettings.bufferComplete.param = BOMX_AudioDecoderEventType_eDataReady;
    errCode = NEXUS_AudioDecoder_SetDecodeToMemorySettings(m_hAudioDecoder, &decSettings);
    if ( errCode )
    {
        (void)BOMX_BERR_TRACE(errCode);
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }
    m_bitsPerSample = decSettings.bitsPerSample;
    m_numPcmChannels = decSettings.numPcmChannels;

    // Set AAC DRC defaults
    NEXUS_AudioDecoderCodecSettings codecSettings;
    NEXUS_AudioDecoder_GetCodecSettings(m_hAudioDecoder, NEXUS_AudioCodec_eAacAdts, &codecSettings);
    if ( property_get(BCM_RO_MEDIA_ADEC_DRC_MODE, property, B_DRC_DEFAULT_MODE) )
    {
        if ( !strcmp(property, "line") )
        {
            codecSettings.codecSettings.aac.drcMode = NEXUS_AudioDecoderDolbyPulseDrcMode_eLine;
        }
        else
        {
            codecSettings.codecSettings.aac.drcMode = NEXUS_AudioDecoderDolbyPulseDrcMode_eRf;
        }
    }
    codecSettings.codecSettings.aac.cut = B_DRC_TO_NEXUS(property_get_int32(BCM_RO_MEDIA_ADEC_DRC_CUT, B_DRC_DEFAULT_CUT));
    codecSettings.codecSettings.aac.boost = B_DRC_TO_NEXUS(property_get_int32(BCM_RO_MEDIA_ADEC_DRC_BOOST, B_DRC_DEFAULT_BOOST));
    codecSettings.codecSettings.aac.drcTargetLevel = property_get_int32(BCM_RO_MEDIA_ADEC_DRC_REF_LEVEL, B_DRC_DEFAULT_REF_LEVEL);
    temp = property_get_int32(BCM_RO_MEDIA_ADEC_DRC_ENC_LEVEL, B_DRC_DEFAULT_ENC_LEVEL);
    if ( temp >= 0 )
    {
        codecSettings.codecSettings.aac.drcDefaultLevel = temp;
    }
    else
    {
        codecSettings.codecSettings.aac.drcDefaultLevel = B_DRC_DEFAULT_REF_LEVEL;
    }

    errCode = NEXUS_AudioDecoder_SetCodecSettings(m_hAudioDecoder, &codecSettings);
    if ( errCode )
    {
        // Report and keep going
        (void)BOMX_BERR_TRACE(errCode);
    }

    // Set AC3 DRC defaults
    NEXUS_AudioDecoder_GetCodecSettings(m_hAudioDecoder, NEXUS_AudioCodec_eAc3, &codecSettings);
    if ( property_get(BCM_RO_MEDIA_ADEC_DRC_MODE_DD, property, B_DRC_DEFAULT_MODE_DD) )
    {
        if ( !strcmp(property, "line") )
        {
            codecSettings.codecSettings.ac3.drcMode = NEXUS_AudioDecoderDolbyDrcMode_eLine;
        }
        else if ( !strcmp(property, "rf_23") )
        {
            codecSettings.codecSettings.ac3.drcMode = NEXUS_AudioDecoderDolbyDrcMode_eCustomTarget;
            codecSettings.codecSettings.ac3.customTargetLevel = 23;
            codecSettings.codecSettings.ac3.customTargetLevelDownmix = 23;
        }
        else if ( !strcmp(property, "rf_24") )
        {
            codecSettings.codecSettings.ac3.drcMode = NEXUS_AudioDecoderDolbyDrcMode_eCustomTarget;
            codecSettings.codecSettings.ac3.customTargetLevel = 24;
            codecSettings.codecSettings.ac3.customTargetLevelDownmix = 24;
        }
        else
        {
            codecSettings.codecSettings.ac3.drcMode = NEXUS_AudioDecoderDolbyDrcMode_eRf;
        }
    }
    codecSettings.codecSettings.ac3.cut = B_DRC_TO_NEXUS(property_get_int32(BCM_RO_MEDIA_ADEC_DRC_CUT, B_DRC_DEFAULT_CUT));
    codecSettings.codecSettings.ac3.boost = B_DRC_TO_NEXUS(property_get_int32(BCM_RO_MEDIA_ADEC_DRC_BOOST, B_DRC_DEFAULT_BOOST));

    errCode = NEXUS_AudioDecoder_SetCodecSettings(m_hAudioDecoder, &codecSettings);
    if ( errCode )
    {
        // Report and keep going
        (void)BOMX_BERR_TRACE(errCode);
    }

    // Set AC3+ DRC defaults
    NEXUS_AudioDecoder_GetCodecSettings(m_hAudioDecoder, NEXUS_AudioCodec_eAc3Plus, &codecSettings);
    if ( property_get(BCM_RO_MEDIA_ADEC_DRC_MODE_DD, property, B_DRC_DEFAULT_MODE_DD) )
    {
        if ( !strcmp(property, "line") )
        {
            codecSettings.codecSettings.ac3Plus.drcMode = NEXUS_AudioDecoderDolbyDrcMode_eLine;
        }
        else if ( !strcmp(property, "rf_23") )
        {
            codecSettings.codecSettings.ac3Plus.drcMode = NEXUS_AudioDecoderDolbyDrcMode_eCustomTarget;
            codecSettings.codecSettings.ac3Plus.customTargetLevel = 23;
            codecSettings.codecSettings.ac3Plus.customTargetLevelDownmix = 23;
        }
        else if ( !strcmp(property, "rf_24") )
        {
            codecSettings.codecSettings.ac3Plus.drcMode = NEXUS_AudioDecoderDolbyDrcMode_eCustomTarget;
            codecSettings.codecSettings.ac3Plus.customTargetLevel = 24;
            codecSettings.codecSettings.ac3Plus.customTargetLevelDownmix = 24;
        }
        else
        {
            codecSettings.codecSettings.ac3Plus.drcMode = NEXUS_AudioDecoderDolbyDrcMode_eRf;
        }
    }
    codecSettings.codecSettings.ac3Plus.cut = B_DRC_TO_NEXUS(property_get_int32(BCM_RO_MEDIA_ADEC_DRC_CUT, B_DRC_DEFAULT_CUT));
    codecSettings.codecSettings.ac3Plus.boost = B_DRC_TO_NEXUS(property_get_int32(BCM_RO_MEDIA_ADEC_DRC_BOOST, B_DRC_DEFAULT_BOOST));

    errCode = NEXUS_AudioDecoder_SetCodecSettings(m_hAudioDecoder, &codecSettings);
    if ( errCode )
    {
        // Report and keep going
        (void)BOMX_BERR_TRACE(errCode);
    }

    // Disable mono stream attenuations
    NEXUS_AudioDecoder_GetCodecSettings(m_hAudioDecoder, NEXUS_AudioCodec_eMpeg, &codecSettings);
    codecSettings.codecSettings.mpeg.attenuateMonoToStereo = false;
    errCode = NEXUS_AudioDecoder_SetCodecSettings(m_hAudioDecoder, &codecSettings);
    if ( errCode )
    {
        // Report and keep going
        (void)BOMX_BERR_TRACE(errCode);
    }
    NEXUS_AudioDecoder_GetCodecSettings(m_hAudioDecoder, NEXUS_AudioCodec_eMp3, &codecSettings);
    codecSettings.codecSettings.mp3.attenuateMonoToStereo = false;
    errCode = NEXUS_AudioDecoder_SetCodecSettings(m_hAudioDecoder, &codecSettings);
    if ( errCode )
    {
        // Report and keep going
        (void)BOMX_BERR_TRACE(errCode);
    }

    // Init BCMA header for some codecs
    BKNI_Memset(m_pBcmaHeader, 0, sizeof(m_pBcmaHeader));
    m_pBcmaHeader[0] = 'B';
    m_pBcmaHeader[1] = 'C';
    m_pBcmaHeader[2] = 'M';
    m_pBcmaHeader[3] = 'A';

    int32_t pesDebug, inputDebug, outputDebug;
    pesDebug = property_get_int32(BCM_RO_MEDIA_ADEC_PES_DEBUG, 0);
    inputDebug = property_get_int32(BCM_RO_MEDIA_ADEC_INPUT_DEBUG, 0);
    outputDebug = property_get_int32(BCM_RO_MEDIA_ADEC_OUTPUT_DEBUG, 0);
    // Setup file to debug input, output or PES data packing if required
    if ( pesDebug || inputDebug || outputDebug )
    {
        time_t rawtime;
        struct tm * timeinfo;
        char fname[100];

        // Generate unique file name
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        if ( pesDebug )
        {
            strftime(fname, sizeof(fname), "/data/nxmedia/adec-%F_%H_%M_%S.pes", timeinfo);
            ALOGD("PES debug output file:%s", fname);
            m_pPesFile = fopen(fname, "wb+");
            if ( NULL == m_pPesFile )
            {
                ALOGW("Error creating PES debug file %s (%s)", fname, strerror(errno));
                // Just keep going without debug
            }
        }
        if ( inputDebug )
        {
            strftime(fname, sizeof(fname), "/data/nxmedia/adec-%F_%H_%M_%S.input", timeinfo);
            ALOGD("Input debug output file:%s", fname);
            m_pInputFile = fopen(fname, "wb+");
            if ( NULL == m_pInputFile )
            {
                ALOGW("Error creating input debug file %s (%s)", fname, strerror(errno));
                // Just keep going without debug
            }
        }
        if ( outputDebug )
        {
            strftime(fname, sizeof(fname), "/data/nxmedia/adec-%F_%H_%M_%S.output", timeinfo);
            ALOGD("Output debug output file:%s", fname);
            m_pOutputFile = fopen(fname, "wb+");
            if ( NULL == m_pOutputFile )
            {
                ALOGW("Error creating output debug file %s (%s)", fname, strerror(errno));
                // Just keep going without debug
            }
        }
    }
}

BOMX_AudioDecoder::~BOMX_AudioDecoder()
{
    unsigned i;

    ShutdownScheduler();

    Lock();

    if ( m_pPesFile )
    {
        fclose(m_pPesFile);
        m_pPesFile = NULL;
    }

    if ( m_pInputFile )
    {
        fclose(m_pInputFile);
        m_pInputFile = NULL;
    }

    if ( m_pOutputFile )
    {
        fclose(m_pOutputFile);
        m_pOutputFile = NULL;
    }

    if ( m_pPes )
    {
        delete m_pPes;
        m_pPes = NULL;
    }

    ClosePidChannel();

    if ( m_hAudioDecoder )
    {
        NEXUS_AudioDecoder_Close(m_hAudioDecoder);
    }
    if ( m_hPlaypump )
    {
        NEXUS_Playpump_Close(m_hPlaypump);
    }
    if ( m_playpumpEventId )
    {
        UnregisterEvent(m_playpumpEventId);
    }
    if ( m_hPlaypumpEvent )
    {
        B_Event_Destroy(m_hPlaypumpEvent);
    }
    if ( m_outputFrameEventId )
    {
        UnregisterEvent(m_outputFrameEventId);
    }
    if ( m_inputBuffersTimerId )
    {
        CancelTimer(m_inputBuffersTimerId);
    }
    if ( m_hOutputFrameEvent )
    {
        B_Event_Destroy(m_hOutputFrameEvent);
    }
    if ( m_fifoOverflowEventId )
    {
        UnregisterEvent(m_fifoOverflowEventId);
    }
    if ( m_hFifoOverflowEvent )
    {
        B_Event_Destroy(m_hFifoOverflowEvent);
    }
    if ( m_fifoUnderflowEventId )
    {
        UnregisterEvent(m_fifoUnderflowEventId);
    }
    if ( m_hFifoUnderflowEvent )
    {
        B_Event_Destroy(m_hFifoUnderflowEvent);
    }
    if ( m_playpumpErrorCallbackEventId )
    {
        UnregisterEvent(m_playpumpErrorCallbackEventId);
    }
    if ( m_hPlaypumpErrorCallbackEvent )
    {
        B_Event_Destroy(m_hPlaypumpErrorCallbackEvent);
    }
    if ( m_playpumpCcErrorEventId )
    {
        UnregisterEvent(m_playpumpCcErrorEventId);
    }
    if ( m_hPlaypumpCcErrorEvent )
    {
        B_Event_Destroy(m_hPlaypumpCcErrorEvent);
    }
    for ( i = 0; i < m_numAudioPorts; i++ )
    {
        if ( m_pAudioPorts[i] )
        {
            CleanupPortBuffers(i);
            delete m_pAudioPorts[i];
        }
    }
    if ( m_pEosBuffer )
    {
        NEXUS_Memory_Free(m_pEosBuffer);
    }
    if ( m_pConfigBuffer )
    {
        NEXUS_Memory_Free(m_pConfigBuffer);
    }
    if ( IsValid() == OMX_ErrorNone && m_pNxWrap )
    {
        m_pNxWrap->leave();
        delete m_pNxWrap;
    }
    if ( m_pBufferTracker )
    {
        delete m_pBufferTracker;
    }
    if ( m_pRoles )
    {
        delete m_pRoles;
    }
    if ( m_pFrameStatus )
    {
        delete m_pFrameStatus;
    }
    if ( m_pMemoryBlocks )
    {
        delete m_pMemoryBlocks;
    }

    android_atomic_dec(&g_activeInstancesNum);

    Unlock();
}

OMX_ERRORTYPE BOMX_AudioDecoder::GetParameter(
        OMX_IN  OMX_INDEXTYPE nParamIndex,
        OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    ERROR_OUT_ON_NEXUS_ACTIVE_STANDBY;

    switch ( (int)nParamIndex )
    {
    case OMX_IndexParamAudioProfileQuerySupported:
        {
            OMX_AUDIO_PARAM_ANDROID_PROFILETYPE *pProfile = (OMX_AUDIO_PARAM_ANDROID_PROFILETYPE *)pComponentParameterStructure;
            ALOGV("GetParameter OMX_IndexParamAudioProfileQuerySupported");
            BOMX_STRUCT_VALIDATE(pProfile);
            if ( pProfile->nPortIndex != m_videoPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }

            switch ( (int)GetCodec() )
            {
            case OMX_AUDIO_CodingAAC:
                switch ( pProfile->nProfileIndex )
                {
                case 0:
                    pProfile->eProfile = (OMX_U32)OMX_AUDIO_AACObjectMain;
                    break;
                case 1:
                    pProfile->eProfile = (OMX_U32)OMX_AUDIO_AACObjectLC;
                    break;
                case 2:
                    pProfile->eProfile = (OMX_U32)OMX_AUDIO_AACObjectSSR;
                    break;
                case 3:
                    pProfile->eProfile = (OMX_U32)OMX_AUDIO_AACObjectLTP;
                    break;
                case 4:
                    pProfile->eProfile = (OMX_U32)OMX_AUDIO_AACObjectHE;
                    break;
                default:
                    return OMX_ErrorNoMore;
                }
                break;
            default:
                // Only certain codecs support this interface
                return OMX_ErrorNoMore;
            }
            return OMX_ErrorNone;
        }
    case OMX_IndexParamAudioAndroidAc3:
    case OMX_IndexParamAudioAndroidEac3:
        {
            OMX_AUDIO_PARAM_ANDROID_AC3TYPE *pAc3 = NULL;
            OMX_AUDIO_PARAM_ANDROID_EAC3TYPE *pEAc3 = NULL;
            unsigned nChannels = 0;
            NEXUS_AudioDecoderStatus decStatus;
            if ((int)nParamIndex == OMX_IndexParamAudioAndroidAc3) {
                pAc3 = (OMX_AUDIO_PARAM_ANDROID_AC3TYPE *)pComponentParameterStructure;
                BOMX_STRUCT_VALIDATE(pAc3);
                if ( pAc3->nPortIndex != m_audioPortBase )
                {
                    return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
                }
            } else {
                pEAc3 = (OMX_AUDIO_PARAM_ANDROID_EAC3TYPE *)pComponentParameterStructure;
                BOMX_STRUCT_VALIDATE(pEAc3);
                if ( pEAc3->nPortIndex != m_audioPortBase )
                {
                    return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
                }
            }
            ALOGV("GetParameter %d", nParamIndex);
            NEXUS_AudioDecoder_GetStatus(m_hAudioDecoder, &decStatus);
            switch ( decStatus.codecStatus.ac3.acmod )
            {
            default:
                nChannels = 0;
                break;
            case NEXUS_AudioAc3Acmod_eOneCenter_1_0_C:
                nChannels = 1;
                break;
            case NEXUS_AudioAc3Acmod_eTwoMono_1_ch1_ch2:
            case NEXUS_AudioAc3Acmod_eTwoChannel_2_0_L_R:
                nChannels = 2;
                break;
             case NEXUS_AudioAc3Acmod_eThreeChannel_3_0_L_C_R:
             case NEXUS_AudioAc3Acmod_eThreeChannel_2_1_L_R_S:
                nChannels = 3;
                break;
            case NEXUS_AudioAc3Acmod_eFourChannel_3_1_L_C_R_S:
            case NEXUS_AudioAc3Acmod_eFourChannel_2_2_L_R_SL_SR:
                nChannels = 4;
                break;
            case NEXUS_AudioAc3Acmod_eFiveChannel_3_2_L_C_R_SL_SR:
                nChannels = 5;
                break;
            }
            if ( decStatus.codecStatus.ac3.lfe )
            {
                nChannels++;
            }
            if ((int)nParamIndex == OMX_IndexParamAudioAndroidAc3) {
                pAc3->nChannels = nChannels;
                pAc3->nSampleRate = decStatus.sampleRate > 0 ? decStatus.sampleRate : m_sampleRate;
            } else {
                pEAc3->nChannels = nChannels;
                pEAc3->nSampleRate = decStatus.sampleRate > 0 ? decStatus.sampleRate : m_sampleRate;
            }
            return OMX_ErrorNone;
        }
    case OMX_IndexParamAudioMp3:
        {
            OMX_AUDIO_PARAM_MP3TYPE *pMp3 = (OMX_AUDIO_PARAM_MP3TYPE *)pComponentParameterStructure;
            NEXUS_AudioDecoderStatus decStatus;
            ALOGV("GetParameter OMX_IndexParamAudioMp3");
            BOMX_STRUCT_VALIDATE(pMp3);
            if ( pMp3->nPortIndex != m_audioPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            NEXUS_AudioDecoder_GetStatus(m_hAudioDecoder, &decStatus);
            switch ( decStatus.codecStatus.mpeg.channelMode )
            {
            default:
                pMp3->nChannels = 0;
                pMp3->eChannelMode = OMX_AUDIO_ChannelModeMax;
                break;
            case NEXUS_AudioMpegChannelMode_eStereo:
                pMp3->nChannels = 2;
                pMp3->eChannelMode = OMX_AUDIO_ChannelModeStereo;
                break;
            case NEXUS_AudioMpegChannelMode_eIntensityStereo:
                pMp3->nChannels = 2;
                pMp3->eChannelMode = OMX_AUDIO_ChannelModeJointStereo;
                break;
            case NEXUS_AudioMpegChannelMode_eDualChannel:
                pMp3->nChannels = 2;
                pMp3->eChannelMode = OMX_AUDIO_ChannelModeDual;
                break;
            case NEXUS_AudioMpegChannelMode_eSingleChannel:
                pMp3->nChannels = 1;
                pMp3->eChannelMode = OMX_AUDIO_ChannelModeMono;
                break;
            }
            pMp3->nSampleRate = decStatus.sampleRate > 0 ? decStatus.sampleRate : m_sampleRate;
            pMp3->nAudioBandWidth = 0;
            if ( decStatus.sampleRate >= 32000 )
            {
                // "Standard" sampling rates are MP1.
                pMp3->eFormat = OMX_AUDIO_MP3StreamFormatMP1Layer3;
            }
            else
            {
                // Low/Quarter sampling rate is MP2.  We don't support 2.5.
                pMp3->eFormat = OMX_AUDIO_MP3StreamFormatMP2Layer3;
            }
            return OMX_ErrorNone;
        }
    case OMX_IndexParamAudioAac:
        {
            OMX_AUDIO_PARAM_AACPROFILETYPE *pAac = (OMX_AUDIO_PARAM_AACPROFILETYPE *)pComponentParameterStructure;
            NEXUS_AudioDecoderStatus decStatus;
            ALOGV("GetParameter OMX_IndexParamAudioAac");
            BOMX_STRUCT_VALIDATE(pAac);
            if ( pAac->nPortIndex != m_audioPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            *pAac = m_aacParams;
            NEXUS_AudioDecoder_GetStatus(m_hAudioDecoder, &decStatus);
            if ( decStatus.started )
            {
                pAac->eChannelMode = OMX_AUDIO_ChannelModeStereo;   // Default from SW decoder
                switch ( decStatus.codecStatus.aac.acmod )
                {
                default:
                    pAac->nChannels = 0;
                    break;
                case NEXUS_AudioAacAcmod_eOneCenter_1_0_C:
                    pAac->eChannelMode = OMX_AUDIO_ChannelModeMono;
                    pAac->nChannels = 1;
                    break;
                case NEXUS_AudioAacAcmod_eTwoMono_1_ch1_ch2:
                    pAac->eChannelMode = OMX_AUDIO_ChannelModeDual;
                    // Fall through
                case NEXUS_AudioAacAcmod_eTwoChannel_2_0_L_R:
                    pAac->nChannels = 2;
                    break;
                case NEXUS_AudioAacAcmod_eThreeChannel_3_0_L_C_R:
                    pAac->nChannels = 3;
                    break;
                case NEXUS_AudioAacAcmod_eFourChannel_3_1_L_C_R_S:
                case NEXUS_AudioAacAcmod_eFourChannel_2_2_L_R_SL_SR:
                    pAac->nChannels = 4;
                    break;
                case NEXUS_AudioAacAcmod_eFiveChannel_3_2_L_C_R_SL_SR:
                    pAac->nChannels = 5;
                    break;
                }
                if ( decStatus.codecStatus.aac.lfe )
                {
                    pAac->nChannels++;
                }
                pAac->nBitRate = decStatus.codecStatus.aac.bitrate;
                pAac->nAudioBandWidth = 0;
                pAac->nFrameLength = 0;
                pAac->nAACtools = 0;
                pAac->nAACERtools = 0;
                pAac->nSampleRate = decStatus.sampleRate > 0 ? decStatus.sampleRate : m_sampleRate;
                m_aacParams = *pAac;
            }
            return OMX_ErrorNone;
        }
    case OMX_IndexParamAudioPcm:
        {
            OMX_AUDIO_PARAM_PCMMODETYPE *pPcm = (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure;
            ALOGV("GetParameter OMX_IndexParamAudioPcm");
            BOMX_STRUCT_VALIDATE(pPcm);
            if ( pPcm->nPortIndex != m_audioPortBase+1 )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            NEXUS_AudioDecoderDecodeToMemorySettings decSettings;
            NEXUS_AudioDecoderStatus decStatus;
            NEXUS_AudioDecoder_GetDecodeToMemorySettings(m_hAudioDecoder, &decSettings);
            NEXUS_AudioDecoder_GetStatus(m_hAudioDecoder, &decStatus);
            pPcm->eNumData = OMX_NumericalDataSigned;
            pPcm->eEndian = OMX_EndianLittle;
            pPcm->bInterleaved = OMX_TRUE;
            pPcm->nBitPerSample = decSettings.bitsPerSample;
            pPcm->nSamplingRate = m_sampleRate;
            pPcm->ePCMMode = OMX_AUDIO_PCMModeLinear;
            pPcm->nChannels = m_numPcmChannels;
            for ( unsigned i = 0; i < OMX_AUDIO_MAXCHANNELS; i++ )
            {
                if ( i >= NEXUS_AudioChannel_eMax || i >= m_numPcmChannels )
                {
                    pPcm->eChannelMapping[i] = OMX_AUDIO_ChannelNone;
                }
                else if ( i == 0 && m_numPcmChannels == 1 )
                {
                    pPcm->eChannelMapping[i] = OMX_AUDIO_ChannelCF;
                }
                else
                {
                    switch ( decSettings.channelLayout[i] )
                    {
                    case NEXUS_AudioChannel_eLeft:          pPcm->eChannelMapping[i] = OMX_AUDIO_ChannelLF; break;
                    case NEXUS_AudioChannel_eRight:         pPcm->eChannelMapping[i] = OMX_AUDIO_ChannelRF; break;
                    case NEXUS_AudioChannel_eLeftSurround:  pPcm->eChannelMapping[i] = OMX_AUDIO_ChannelLR; break; // Note: Android reverses the definition of Rear and Surround
                    case NEXUS_AudioChannel_eRightSurround: pPcm->eChannelMapping[i] = OMX_AUDIO_ChannelRR; break;
                    case NEXUS_AudioChannel_eLeftRear:      pPcm->eChannelMapping[i] = OMX_AUDIO_ChannelLS; break;
                    case NEXUS_AudioChannel_eRightRear:     pPcm->eChannelMapping[i] = OMX_AUDIO_ChannelRS; break;
                    case NEXUS_AudioChannel_eCenter:        pPcm->eChannelMapping[i] = OMX_AUDIO_ChannelCF; break;
                    case NEXUS_AudioChannel_eLfe:           pPcm->eChannelMapping[i] = OMX_AUDIO_ChannelLFE; break;
                    default:                                pPcm->eChannelMapping[i] = OMX_AUDIO_ChannelNone; break;
                    }
                }
            }
            return OMX_ErrorNone;
        }
    case OMX_IndexParamAllocNativeHandle:
        {
            AllocateNativeHandleParams* allocateNativeHandleParams = (AllocateNativeHandleParams *) pComponentParameterStructure;
            BOMX_STRUCT_VALIDATE(allocateNativeHandleParams);
            ALOGV("GetParameter OMX_IndexParamAllocNativeHandle %u", allocateNativeHandleParams->enable);

            if ( allocateNativeHandleParams->nPortIndex != m_audioPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            allocateNativeHandleParams->enable = m_allocNativeHandle ? OMX_TRUE : OMX_FALSE;

            return OMX_ErrorNone;
        }
    default:
        ALOGV("GetParameter %#x Deferring to base class", nParamIndex);
        return BOMX_ERR_TRACE(BOMX_Component::GetParameter(nParamIndex, pComponentParameterStructure));
    }
}

OMX_ERRORTYPE BOMX_AudioDecoder::SetParameter(
        OMX_IN  OMX_INDEXTYPE nIndex,
        OMX_IN  OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE err;
    NEXUS_Error errCode;

    ERROR_OUT_ON_NEXUS_ACTIVE_STANDBY;

    switch ( (int)nIndex )
    {
    case OMX_IndexParamStandardComponentRole:
        {
            OMX_PARAM_COMPONENTROLETYPE *pRole = (OMX_PARAM_COMPONENTROLETYPE *)pComponentParameterStructure;
            unsigned i;
            BOMX_STRUCT_VALIDATE(pRole);
            ALOGV("SetParameter OMX_IndexParamStandardComponentRole '%s'", pRole->cRole);
            // The spec says this should reset the component to default setings for the role specified.
            // It is technically redundant for this component, changing the input codec has the same
            // effect - but this will also set the input codec accordingly.
            for ( i = 0; i < m_numRoles; i++ )
            {
                if ( !strcmp(m_pRoles[i].name, (const char *)pRole->cRole) )
                {
                    // Set input port to matching compression std.
                    OMX_AUDIO_PARAM_PORTFORMATTYPE format;
                    memset(&format, 0, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
                    BOMX_STRUCT_INIT(&format);
                    format.nPortIndex = m_audioPortBase;
                    format.eEncoding = (OMX_AUDIO_CODINGTYPE)m_pRoles[i].omxCodec;
                    err = SetParameter(OMX_IndexParamAudioPortFormat, &format);
                    if ( err != OMX_ErrorNone )
                    {
                        return BOMX_ERR_TRACE(err);
                    }
                    SetRole((const char *)pRole->cRole);
                    return OMX_ErrorNone;
                }
            }
            return BOMX_ERR_TRACE(OMX_ErrorComponentNotFound);
        }
    case OMX_IndexParamAudioPortFormat:
        {
            BOMX_Port *pPort;
            OMX_AUDIO_PARAM_PORTFORMATTYPE *pFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
            ALOGV("SetParameter OMX_IndexParamAudioPortFormat");
            BOMX_STRUCT_VALIDATE(pFormat);
            pPort = FindPortByIndex(pFormat->nPortIndex);
            if ( NULL == pPort )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            if ( pPort->GetDir() == OMX_DirInput )
            {
                ALOGV("Set Input Port Compression Format to %u (%#x)", pFormat->eEncoding, pFormat->eEncoding);
                err = BOMX_AudioDecoder_InitMimeType(pFormat->eEncoding, m_inputMimeType);
                if ( err )
                {
                    return BOMX_ERR_TRACE(err);
                }
                // Per the OMX spec you are supposed to initialize the port defs to defaults when changing format
                // The only thing we change is the format itself though - the MIME type is set above.
                OMX_AUDIO_PORTDEFINITIONTYPE portDefs;
                memset(&portDefs, 0, sizeof(portDefs));
                portDefs.eEncoding = pFormat->eEncoding;
                portDefs.cMIMEType = m_inputMimeType;
                err = m_pAudioPorts[0]->SetPortFormat(pFormat, &portDefs);
                if ( err )
                {
                    return BOMX_ERR_TRACE(err);
                }
            }
            else
            {
                ALOGV("Set Output Port Encoding to %u (%#x)", pFormat->eEncoding, pFormat->eEncoding);
                if ( pFormat->eEncoding != OMX_AUDIO_CodingPCM )
                {
                    ALOGE("Invalid audio encoding %#x on output port", pFormat->eEncoding);
                    return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
                }
                // Per the OMX spec you are supposed to initialize the port defs to defaults when changing format
                OMX_AUDIO_PORTDEFINITIONTYPE portDefs;
                portDefs = pPort->GetDefinition()->format.audio;
                portDefs.eEncoding = pFormat->eEncoding;
                portDefs.bFlagErrorConcealment = OMX_FALSE;
                err = m_pAudioPorts[1]->SetPortFormat(pFormat, &portDefs);
                if ( err )
                {
                    return BOMX_ERR_TRACE(err);
                }
            }
            return OMX_ErrorNone;
        }
    case OMX_IndexParamPortDefinition:
    {
        OMX_PARAM_PORTDEFINITIONTYPE *pDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        ALOGV("SetParameter OMX_IndexParamPortDefinition");
        BOMX_STRUCT_VALIDATE(pDef);
        if ( pDef->nPortIndex == m_audioPortBase && pDef->format.audio.cMIMEType != m_inputMimeType )
        {
            ALOGW("Audio input MIME type cannot be changed in the application");
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        else if ( pDef->nPortIndex == (m_audioPortBase+1) && pDef->format.audio.cMIMEType != m_outputMimeType )
        {
            ALOGW("Audio output MIME type cannot be changed in the application");
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        BOMX_Port *pPort = FindPortByIndex(pDef->nPortIndex);
        if ( NULL == pPort )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if ( pDef->nPortIndex == m_audioPortBase && pDef->format.audio.eEncoding != GetCodec() )
        {
            ALOGW("Audio compression format cannot be changed in the port defintion.  Change Port Format instead.");
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        if ( pDef->nPortIndex == m_audioPortBase + 1 )
        {
            // Reallocate arrays based on port size
            if ( m_pFrameStatus )
            {
                delete m_pFrameStatus;
                m_pFrameStatus = NULL;
            }
            if ( m_pMemoryBlocks )
            {
                m_pMemoryBlocks = NULL;
            }
            m_pFrameStatus = new NEXUS_AudioDecoderFrameStatus[pDef->nBufferCountActual];
            if ( NULL == m_pFrameStatus )
            {
                return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
            }
            m_pMemoryBlocks = new NEXUS_MemoryBlockHandle[pDef->nBufferCountActual];
            if ( NULL == m_pMemoryBlocks )
            {
                return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
            }
            // Update decoder with new buffer count
            NEXUS_AudioDecoderDecodeToMemorySettings decSettings;
            NEXUS_AudioDecoder_GetDecodeToMemorySettings(m_hAudioDecoder, &decSettings);
            decSettings.maxBuffers = pDef->nBufferCountActual;
            errCode = NEXUS_AudioDecoder_SetDecodeToMemorySettings(m_hAudioDecoder, &decSettings);
            if ( errCode )
            {
                return BOMX_ERR_TRACE(OMX_ErrorUndefined);
            }
        }
        // Handle update in base class
        err = BOMX_Component::SetParameter(nIndex, (OMX_PTR)pDef);
        if ( err != OMX_ErrorNone )
        {
            return BOMX_ERR_TRACE(err);
        }
        return OMX_ErrorNone;
    }
    case OMX_IndexParamAudioPcm:
        {
            OMX_AUDIO_PARAM_PCMMODETYPE *pPcm = (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure;
            ALOGV("SetParameter OMX_IndexParamAudioPcm");
            BOMX_STRUCT_VALIDATE(pPcm);
            if ( pPcm->nPortIndex != m_audioPortBase+1 )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            NEXUS_AudioDecoderDecodeToMemorySettings decSettings;
            NEXUS_AudioDecoder_GetDecodeToMemorySettings(m_hAudioDecoder, &decSettings);
            if ( pPcm->eNumData != OMX_NumericalDataSigned )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
            }
            if ( pPcm->eEndian != OMX_EndianLittle )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
            }
            if ( pPcm->nChannels == 0 || pPcm->nChannels > 8 )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
            }
            if ( pPcm->bInterleaved == OMX_FALSE && pPcm->nChannels > 1 )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
            }
            if ( pPcm->ePCMMode != OMX_AUDIO_PCMModeLinear )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
            }
            if ( pPcm->nSamplingRate % 8000 && pPcm->nSamplingRate % 11025 && pPcm->nSamplingRate % 12000 )
            {
                // Only multiples of 8kHz, 11.025kHz, 12kHz are supported
                return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
            }
            if ( pPcm->nSamplingRate > 48000 )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
            }
            ALOGI("Configuring decoder for %u channels", pPcm->nChannels);
            if ( pPcm->nChannels >= 2 )
            {
                // Round up to nearest supported channel count of 2/6/8.
                decSettings.numPcmChannels = pPcm->nChannels > 6 ? 8 : pPcm->nChannels > 2 ? 6 : 2;
                for ( unsigned i = 0; i < OMX_AUDIO_MAXCHANNELS && i < decSettings.numPcmChannels; i++ )
                {
                    if ( i >= pPcm->nChannels )
                    {
                        NEXUS_AudioChannel chan;
                        // Find first unused nexus channel value to use.  You can't duplicate channels.
                        for ( chan = NEXUS_AudioChannel_eLeft; chan < NEXUS_AudioChannel_eMax; chan = (NEXUS_AudioChannel)((int)chan + 1) )
                        {
                            unsigned j;
                            for ( j = 0; j < i; j++ )
                            {
                                if ( decSettings.channelLayout[j] == chan )
                                {
                                    break;
                                }
                            }

                            if ( j >= i )
                            {
                                break;
                            }
                        }
                        decSettings.channelLayout[i] = chan;
                        ALOGV("PCM Channel %u -> Unused / %u nexus", i, decSettings.channelLayout[i]);
                    }
                    else
                    {
                        switch ( pPcm->eChannelMapping[i] )
                        {
                        case OMX_AUDIO_ChannelLF:               decSettings.channelLayout[i] = NEXUS_AudioChannel_eLeft; break;
                        case OMX_AUDIO_ChannelRF:               decSettings.channelLayout[i] = NEXUS_AudioChannel_eRight; break;
                        case OMX_AUDIO_ChannelLR:               decSettings.channelLayout[i] = NEXUS_AudioChannel_eLeftSurround; break; // Note: Android reverses the definition of Rear and Surround
                        case OMX_AUDIO_ChannelRR:               decSettings.channelLayout[i] = NEXUS_AudioChannel_eRightSurround; break;
                        case OMX_AUDIO_ChannelLS:               decSettings.channelLayout[i] = NEXUS_AudioChannel_eLeftRear; break;
                        case OMX_AUDIO_ChannelRS:               decSettings.channelLayout[i] = NEXUS_AudioChannel_eRightRear; break;
                        case OMX_AUDIO_ChannelCF:               decSettings.channelLayout[i] = NEXUS_AudioChannel_eCenter; break;
                        case OMX_AUDIO_ChannelLFE:              decSettings.channelLayout[i] = NEXUS_AudioChannel_eLfe; break;
                        default:                                decSettings.channelLayout[i] = NEXUS_AudioChannel_eMax; break;
                        }
                        ALOGV("PCM Channel %u -> %u OMX %u nexus", i, pPcm->eChannelMapping[i], decSettings.channelLayout[i]);
                    }
                }
            }
            else
            {
                // Force mono to output as stereo, allow right channel only for dual-mono, otherwise pick left.
                ALOGV("PCM config mono -> stereo");
                decSettings.numPcmChannels = 2;
                if ( pPcm->eChannelMapping[0] == OMX_AUDIO_ChannelRF )
                {
                    decSettings.channelLayout[0] = NEXUS_AudioChannel_eRight;
                    decSettings.channelLayout[1] = NEXUS_AudioChannel_eLeft;
                }
                else
                {
                    decSettings.channelLayout[0] = NEXUS_AudioChannel_eLeft;
                    decSettings.channelLayout[1] = NEXUS_AudioChannel_eRight;
                }
            }
            decSettings.bitsPerSample = pPcm->nBitPerSample;
            errCode = NEXUS_AudioDecoder_SetDecodeToMemorySettings(m_hAudioDecoder, &decSettings);
            if ( errCode )
            {
                (void)BOMX_BERR_TRACE(errCode);
                return OMX_ErrorUndefined;
            }
            m_sampleRate = pPcm->nSamplingRate;
            m_bitsPerSample = decSettings.bitsPerSample;
            m_numPcmChannels = pPcm->nChannels;
            return OMX_ErrorNone;
        }
    case OMX_IndexParamAudioAndroidAc3:
        {
            OMX_AUDIO_PARAM_ANDROID_AC3TYPE *pAc3 = (OMX_AUDIO_PARAM_ANDROID_AC3TYPE *)pComponentParameterStructure;
            ALOGV("SetParameter OMX_IndexParamAudioAndroidAc3");
            BOMX_STRUCT_VALIDATE(pAc3);
            if ( pAc3->nPortIndex != m_audioPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            // Do nothing, just make android happy we accepted it.
            return OMX_ErrorNone;
        }
    case OMX_IndexParamAudioAndroidEac3:
        {
            OMX_AUDIO_PARAM_ANDROID_EAC3TYPE *pEAc3 = (OMX_AUDIO_PARAM_ANDROID_EAC3TYPE *)pComponentParameterStructure;
            ALOGV("SetParameter OMX_IndexParamAudioAndroidEAc3");
            BOMX_STRUCT_VALIDATE(pEAc3);
            if ( pEAc3->nPortIndex != m_audioPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            // Do nothing, just make android happy we accepted it.`
            return OMX_ErrorNone;
        }
    case OMX_IndexParamAudioAac:
        {
            OMX_AUDIO_PARAM_AACPROFILETYPE *pAac = (OMX_AUDIO_PARAM_AACPROFILETYPE *)pComponentParameterStructure;
            ALOGV("SetParameter OMX_IndexParamAudioAac");
            BOMX_STRUCT_VALIDATE(pAac);
            if ( pAac->nPortIndex != m_audioPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            switch ( pAac->eAACStreamFormat )
            {
            case OMX_AUDIO_AACStreamFormatMP2ADTS:
            case OMX_AUDIO_AACStreamFormatMP4ADTS:
            case OMX_AUDIO_AACStreamFormatMP4FF:
                break;
            default:
                ALOGE("Unsupported AAC Stream Format.  Only ADTS/MP4FF are supported");
                return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
            }
            m_aacParams = *pAac;
            return OMX_ErrorNone;
        }
    case OMX_IndexParamAudioAndroidAacPresentation:
        {
            NEXUS_AudioDecoderCodecSettings codecSettings;

            OMX_AUDIO_PARAM_ANDROID_AACPRESENTATIONTYPE *pAac = (OMX_AUDIO_PARAM_ANDROID_AACPRESENTATIONTYPE *)pComponentParameterStructure;
            ALOGV("SetParameter OMX_IndexParamAudioAndroidAacPresentation");

            NEXUS_AudioDecoder_GetCodecSettings(m_hAudioDecoder, NEXUS_AudioCodec_eAacAdts, &codecSettings);
            if ( pAac->nDrcCut >= 0 )
            {
                codecSettings.codecSettings.aac.cut = B_DRC_TO_NEXUS(pAac->nDrcCut);
            }
            if ( pAac->nDrcBoost >= 0 )
            {
                codecSettings.codecSettings.aac.boost = B_DRC_TO_NEXUS(pAac->nDrcBoost);
            }
            if ( pAac->nTargetReferenceLevel >= 0 )
            {
                codecSettings.codecSettings.aac.drcTargetLevel = pAac->nTargetReferenceLevel;
            }
            if ( pAac->nEncodedTargetLevel >= 0 )
            {
                codecSettings.codecSettings.aac.drcDefaultLevel = pAac->nEncodedTargetLevel;
            }
            errCode = NEXUS_AudioDecoder_SetCodecSettings(m_hAudioDecoder, &codecSettings);
            if ( errCode )
            {
                ALOGW("Unable to update AAC presentation settings");
                // ACodec will assert if this actually fails so just warn.
            }
            return OMX_ErrorNone;
        }
    case OMX_IndexParamAllocNativeHandle:
        {
            AllocateNativeHandleParams* allocateNativeHandleParams = (AllocateNativeHandleParams *) pComponentParameterStructure;
            BOMX_STRUCT_VALIDATE(allocateNativeHandleParams);
            ALOGV("SetParameter OMX_IndexParamAllocNativeHandle %u", allocateNativeHandleParams->enable);

            if ( allocateNativeHandleParams->nPortIndex != m_audioPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            if (allocateNativeHandleParams->enable == OMX_TRUE && !m_secureDecoder)
            {
                ALOGE("OMX_IndexParamAllocNativeHandle cannot enable with non-secure decoder");
                return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
            }
            m_allocNativeHandle = allocateNativeHandleParams->enable == OMX_TRUE;
            return OMX_ErrorNone;
        }
    default:
        ALOGV("SetParameter %#x - Defer to base class", nIndex);
        return BOMX_ERR_TRACE(BOMX_Component::SetParameter(nIndex, pComponentParameterStructure));
    }
}

NEXUS_AudioCodec BOMX_AudioDecoder::GetNexusCodec()
{
    return GetNexusCodec((OMX_AUDIO_CODINGTYPE)GetCodec());
}

NEXUS_AudioCodec BOMX_AudioDecoder::GetNexusCodec(OMX_AUDIO_CODINGTYPE omxType)
{
    switch ( (int)omxType )
    {
    case OMX_AUDIO_CodingAndroidAC3:
        return NEXUS_AudioCodec_eAc3;
    case OMX_AUDIO_CodingAndroidEAC3:
        return NEXUS_AudioCodec_eAc3Plus;
    case OMX_AUDIO_CodingMP3:
        return NEXUS_AudioCodec_eMp3;
    case OMX_AUDIO_CodingAAC:
        return NEXUS_AudioCodec_eAacAdts;
    default:
        ALOGW("Unknown audio codec %u (%#x)", GetCodec(), GetCodec());
        return NEXUS_AudioCodec_eUnknown;
    }
}

NEXUS_Error BOMX_AudioDecoder::StartDecoder()
{
    NEXUS_AudioDecoderStartSettings adecStartSettings;
    NEXUS_Error errCode;

    if ( m_decoderState == BOMX_AudioDecoderState_eStopped )
    {
        NEXUS_AudioDecoder_GetDefaultStartSettings(&adecStartSettings);
        adecStartSettings.pidChannel = m_hPidChannel;
        adecStartSettings.codec = GetNexusCodec();
        adecStartSettings.targetSyncEnabled = NEXUS_AudioDecoderTargetSyncMode_eDisabledAfterLock;
        ALOGV("%d Start Decoder codec %u", m_instanceNum, adecStartSettings.codec);
        errCode = NEXUS_AudioDecoder_Start(m_hAudioDecoder, &adecStartSettings);
        if ( errCode )
        {
            return BOMX_BERR_TRACE(errCode);
        }
        m_codecDelayAdjusted = 0;
        m_submittedDescriptors = 0;
        errCode = NEXUS_Playpump_Start(m_hPlaypump);
        if ( errCode )
        {
            NEXUS_AudioDecoder_Stop(m_hAudioDecoder);
            m_eosPending = false;
            m_eosDelivered = false;
            m_eosReceived = false;
            m_eosStandalone = false;
            m_eosReady = false;
            return BOMX_BERR_TRACE(errCode);
        }
        m_decoderState = BOMX_AudioDecoderState_eStarted;
        return BERR_SUCCESS;
    }
    else
    {
        return BOMX_BERR_TRACE(BERR_NOT_SUPPORTED);
    }
}

NEXUS_Error BOMX_AudioDecoder::StopDecoder()
{
    if ( m_decoderState == BOMX_AudioDecoderState_eStarted )
    {
        NEXUS_Playpump_Stop(m_hPlaypump);
        NEXUS_AudioDecoder_Stop(m_hAudioDecoder);

        m_submittedDescriptors = 0;
        m_eosPending = false;
        m_eosDelivered = false;
        m_eosReceived = false;
        m_eosStandalone = false;
        m_eosReady = false;
        m_pBufferTracker->Flush();
        InputBufferCounterReset();
        m_decoderState = BOMX_AudioDecoderState_eStopped;
        ReturnPortBuffers(m_pAudioPorts[0]);
        return BERR_SUCCESS;
    }
    else
    {
        return BOMX_BERR_TRACE(BERR_NOT_SUPPORTED);
    }
}

NEXUS_Error BOMX_AudioDecoder::SetInputPortState(OMX_STATETYPE newState)
{
    ALOGD_IF((m_logMask & B_LOG_ADEC_TRANS_PORT), "%d Setting Input Port State to %s", m_instanceNum, BOMX_StateName(newState));
    // Loaded means stop and release all resources
    if ( newState == OMX_StateLoaded )
    {
        // Shutdown and free resources
        if ( m_hPlaypump )
        {
            if ( m_decoderState == BOMX_AudioDecoderState_eSuspended )
            {
                NEXUS_AudioDecoder_Resume(m_hAudioDecoder);
                m_decoderState = BOMX_AudioDecoderState_eStarted;
            }
            if ( m_decoderState == BOMX_AudioDecoderState_eStarted )
            {
                StopDecoder();
            }

            ClosePidChannel();
            NEXUS_Playpump_Close(m_hPlaypump);
            UnregisterEvent(m_playpumpEventId);
            m_playpumpEventId = NULL;
            UnregisterEvent(m_playpumpErrorCallbackEventId);
            m_playpumpErrorCallbackEventId = NULL;
            UnregisterEvent(m_playpumpCcErrorEventId);
            m_playpumpCcErrorEventId = NULL;
            m_hPlaypump = NULL;
            CancelTimerId(m_playpumpTimerId);
        }
    }
    else
    {
        NEXUS_AudioDecoderStatus adecStatus;
        NEXUS_Error errCode;

        // All states other than loaded require a playpump and audio decoder handle
        if ( NULL == m_hPlaypump )
        {
            NEXUS_PlaypumpOpenSettings playpumpOpenSettings;
            NEXUS_PlaypumpSettings playpumpSettings;
            NEXUS_Error errCode;

            NEXUS_Playpump_GetDefaultOpenSettings(&playpumpOpenSettings);
            playpumpOpenSettings.fifoSize = 0;
            playpumpOpenSettings.dataNotCpuAccessible = true;
            playpumpOpenSettings.numDescriptors = 1+(B_MAX_DESCRIPTORS_PER_BUFFER*m_pAudioPorts[0]->GetDefinition()->nBufferCountActual);   // Extra 1 is for EOS
            m_hPlaypump = NEXUS_Playpump_Open(NEXUS_ANY_ID, &playpumpOpenSettings);
            if ( NULL == m_hPlaypump )
            {
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }
            NEXUS_Playpump_GetSettings(m_hPlaypump, &playpumpSettings);
            playpumpSettings.transportType = NEXUS_TransportType_eMpeg2Pes;
            playpumpSettings.dataNotCpuAccessible = true;
            playpumpSettings.dataCallback.callback = BOMX_AudioDecoder_EventCallback;
            playpumpSettings.dataCallback.context = static_cast <void *> (m_hPlaypumpEvent);
            playpumpSettings.dataCallback.param = (int)BOMX_AudioDecoderEventType_ePlaypump;
            playpumpSettings.errorCallback.callback = BOMX_AudioDecoder_EventCallback;
            playpumpSettings.errorCallback.context = static_cast <void *> (m_hPlaypumpErrorCallbackEvent);
            playpumpSettings.errorCallback.param = (int)BOMX_AudioDecoderEventType_ePlaypumpErrorCallback;
            playpumpSettings.ccError.callback = BOMX_AudioDecoder_EventCallback;
            playpumpSettings.ccError.context = static_cast <void *> (m_hPlaypumpCcErrorEvent);
            playpumpSettings.ccError.param = (int)BOMX_AudioDecoderEventType_ePlaypumpCcError;
            errCode = NEXUS_Playpump_SetSettings(m_hPlaypump, &playpumpSettings);
            if ( errCode )
            {
                NEXUS_Playpump_Close(m_hPlaypump);
                m_hPlaypump = NULL;
                return BOMX_BERR_TRACE(errCode);
            }

            errCode = OpenPidChannel(B_STREAM_ID);
            if ( errCode )
            {
                NEXUS_Playpump_Close(m_hPlaypump);
                m_hPlaypump = NULL;
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }
            m_playpumpEventId = RegisterEvent(m_hPlaypumpEvent, BOMX_AudioDecoder_PlaypumpEvent, static_cast <void *> (this));
            if ( NULL == m_playpumpEventId )
            {
                ALOGW("Unable to register playpump event");
                ClosePidChannel();
                NEXUS_Playpump_Close(m_hPlaypump);
                m_hPlaypump = NULL;
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }
            m_playpumpErrorCallbackEventId = RegisterEvent(m_hPlaypumpErrorCallbackEvent, BOMX_AudioDecoder_PlaypumpErrorCallbackEvent, static_cast <void *> (this));
            if ( NULL == m_playpumpErrorCallbackEventId )
            {
                ALOGW("Unable to register playpump error callback event");
                ClosePidChannel();
                NEXUS_Playpump_Close(m_hPlaypump);
                m_hPlaypump = NULL;
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }
            m_playpumpCcErrorEventId = RegisterEvent(m_hPlaypumpCcErrorEvent, BOMX_AudioDecoder_PlaypumpCcErrorEvent, static_cast <void *> (this));
            if ( NULL == m_playpumpCcErrorEventId )
            {
                ALOGW("Unable to register playpump continuity count  error  event");
                ClosePidChannel();
                NEXUS_Playpump_Close(m_hPlaypump);
                m_hPlaypump = NULL;
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }
            // Safe to initialize here the number of input buffers available
            m_AvailInputBuffers = m_pAudioPorts[0]->GetDefinition()->nBufferCountActual;
        }

        (void)NEXUS_AudioDecoder_GetStatus(m_hAudioDecoder, &adecStatus);
        switch ( newState )
        {
        case OMX_StateIdle:
            switch ( m_decoderState )
            {
            default:
                ALOG_ASSERT(0);
                break;
            case BOMX_AudioDecoderState_eStopped:
                break;
            case BOMX_AudioDecoderState_eSuspended:
                // Resuming the decoder before stopping it completely
                NEXUS_AudioDecoder_Resume(m_hAudioDecoder);
                m_decoderState = BOMX_AudioDecoderState_eStarted;
            case BOMX_AudioDecoderState_eStarted:
                StopDecoder();
                RemoveOutputBuffers();
                break;
            }
            break;
        case OMX_StateExecuting:
            switch ( m_decoderState )
            {
            case BOMX_AudioDecoderState_eStopped:
                errCode = StartDecoder();
                if ( errCode )
                {
                    return BOMX_BERR_TRACE(errCode);
                }
                if ( m_pAudioPorts[1]->IsEnabled() )
                {
                    // Kick off the decoder frame queue
                    AddOutputBuffers();
                    OutputFrameEvent();
                }
                else
                {
                    errCode = NEXUS_AudioDecoder_Suspend(m_hAudioDecoder);
                    if ( errCode )
                    {
                        StopDecoder();
                        return BOMX_BERR_TRACE(errCode);
                    }
                    m_decoderState = BOMX_AudioDecoderState_eSuspended;
                }
                break;
            case BOMX_AudioDecoderState_eSuspended:
                // Technically this shouldn't happen
                if ( m_pAudioPorts[1]->IsEnabled() )
                {
                    // Kick off the decoder frame queue
                    NEXUS_AudioDecoder_Resume(m_hAudioDecoder);
                    AddOutputBuffers();
                    OutputFrameEvent();
                }
                else
                {
                    // Do nothing
                    ALOGW("Unexpected state transition - suspended while output port is enabled");
                }
                break;
            case BOMX_AudioDecoderState_eStarted:
                ALOGE("Invalid state transition started->started");
                return BOMX_BERR_TRACE(BERR_NOT_SUPPORTED);
            default:
                ALOGE("Invalid decoder state");
                ALOG_ASSERT(false);
                break;
            }
            break;
        case OMX_StatePause:
            if ( adecStatus.started )
            {
                // Executing -> Paused = Pause -- Not required by android
            }
            else
            {
                // Ignore Idle -> Pause.  We will start when set to Executing.
            }
            break;
        default:
            return BOMX_BERR_TRACE(BERR_NOT_SUPPORTED);
        }
    }
    return NEXUS_SUCCESS;
}

NEXUS_Error BOMX_AudioDecoder::SetOutputPortState(OMX_STATETYPE newState)
{
    NEXUS_Error errCode;
    ALOGD_IF((m_logMask & B_LOG_ADEC_TRANS_PORT), "%d Setting Output Port State to %s", m_instanceNum, BOMX_StateName(newState));
    if ( newState == OMX_StateLoaded )
    {
        CancelTimerId(m_inputBuffersTimerId);
        m_formatChangePending = false;

        switch ( m_decoderState )
        {
        case BOMX_AudioDecoderState_eStarted:
            errCode = NEXUS_AudioDecoder_Suspend(m_hAudioDecoder);
            if ( errCode ) { return BOMX_BERR_TRACE(errCode); }
            m_decoderState = BOMX_AudioDecoderState_eSuspended;
            break;
        case BOMX_AudioDecoderState_eSuspended:
        case BOMX_AudioDecoderState_eStopped:
            break;
        default:
            ALOG_ASSERT(0);
            break;
        }

        RemoveOutputBuffers();

        // Return all pending buffers to the client
        ReturnPortBuffers(m_pAudioPorts[1]);
    }
    else
    {
        // For any other state change, kick off the frame check if the decoder is running
        if ( newState == OMX_StateIdle )
        {
            switch ( m_decoderState )
            {
            case BOMX_AudioDecoderState_eStarted:
                errCode = NEXUS_AudioDecoder_Suspend(m_hAudioDecoder);
                if ( errCode ) { return BOMX_BERR_TRACE(errCode); }
                m_decoderState = BOMX_AudioDecoderState_eSuspended;
                break;
            case BOMX_AudioDecoderState_eSuspended:
            case BOMX_AudioDecoderState_eStopped:
                break;
            default:
                ALOG_ASSERT(0);
                break;
            }

            RemoveOutputBuffers();

            // Return all pending buffers to the client
            ReturnPortBuffers(m_pAudioPorts[1]);
        }
        else
        {
            switch ( m_decoderState )
            {
            case BOMX_AudioDecoderState_eStarted:
                ALOGE("Unexpected state transition started->started on output port");
                return BOMX_BERR_TRACE(BERR_NOT_SUPPORTED);
            case BOMX_AudioDecoderState_eSuspended:
                NEXUS_AudioDecoder_Resume(m_hAudioDecoder);
                m_decoderState = BOMX_AudioDecoderState_eStarted;
                AddOutputBuffers();
                // Return all the available input buffers because the output buffers might have been removed
                // during port reconfiguration so there might be a case where no event is triggered to return the available
                // input buffers.
                ReturnInputBuffers(0, true);
                break;
            case BOMX_AudioDecoderState_eStopped:
                // This is fine, the decoder will start later when the input port starts.
                break;
            default:
                ALOG_ASSERT(0);
                break;
            }
        }
    }
    return NEXUS_SUCCESS;
}

OMX_ERRORTYPE BOMX_AudioDecoder::CommandStateSet(
    OMX_STATETYPE newState,
    OMX_STATETYPE oldState)
{
    OMX_ERRORTYPE err;
    NEXUS_Error errCode;

    ALOGD_IF((m_logMask & B_LOG_ADEC_TRANS_STATE), "%u Begin State Change %s->%s", m_instanceNum, BOMX_StateName(oldState), BOMX_StateName(newState));

    switch ( newState )
    {
    case OMX_StateLoaded:
    {
        // If we are returning to loaded, we need to first flush all enabled ports and return their buffers
        bool inputPortEnabled, outputPortEnabled;

        // Save port state
        inputPortEnabled = m_pAudioPorts[0]->IsEnabled();
        outputPortEnabled = m_pAudioPorts[1]->IsEnabled();

        // Disable all ports
        err = CommandPortDisable(OMX_ALL);
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }

        // Now all resources have been released.  Reset port enable state
        if ( inputPortEnabled )
        {
            (void)m_pAudioPorts[0]->Enable();
        }
        if ( outputPortEnabled )
        {
            (void)m_pAudioPorts[1]->Enable();
        }

        ALOGD_IF((m_logMask & B_LOG_ADEC_TRANS_STATE), "%d End State Change %s->%s", m_instanceNum, BOMX_StateName(oldState), BOMX_StateName(newState));
        return OMX_ErrorNone;
    }
    case OMX_StateIdle:
    {
        // Reset saved codec config on transition to idle
        err = ConfigBufferInit();
        if (err != OMX_ErrorNone)
            return BOMX_ERR_TRACE(err);

        // Transitioning from Loaded->Idle requires us to allocate all required resources
        if ( oldState == OMX_StateLoaded )
        {
            ALOGV("%d Waiting for port population...", m_instanceNum);
            PortWaitBegin();
            // Now we need to wait for all enabled ports to become populated
            while ( (m_pAudioPorts[0]->IsEnabled() && !m_pAudioPorts[0]->IsPopulated()) ||
                    (m_pAudioPorts[1]->IsEnabled() && !m_pAudioPorts[1]->IsPopulated()) )
            {
                if ( PortWait() != B_ERROR_SUCCESS )
                {
                    ALOGW("%d Timeout waiting for ports to be populated", m_instanceNum);
                    PortWaitEnd();
                    return BOMX_ERR_TRACE(OMX_ErrorTimeout);
                }
            }
            PortWaitEnd();
            ALOGV("%d Done waiting for port population", m_instanceNum);

            bool inputEnabled = m_pAudioPorts[0]->IsEnabled();
            bool outputEnabled = m_pAudioPorts[1]->IsEnabled();
            if ( m_pAudioPorts[1]->IsPopulated() && m_pAudioPorts[1]->IsEnabled() )
            {
                errCode = SetOutputPortState(OMX_StateIdle);
                if ( errCode )
                {
                    (void)CommandPortDisable(OMX_ALL);
                    if ( inputEnabled ) {m_pAudioPorts[0]->Enable();}
                    if ( outputEnabled ) {m_pAudioPorts[1]->Enable();}
                    return BOMX_ERR_TRACE(OMX_ErrorUndefined);
                }
            }
            if ( m_pAudioPorts[0]->IsPopulated() && m_pAudioPorts[0]->IsEnabled() )
            {
                errCode = SetInputPortState(OMX_StateIdle);
                if ( errCode )
                {
                    (void)CommandPortDisable(OMX_ALL);
                    if ( inputEnabled ) {m_pAudioPorts[0]->Enable();}
                    if ( outputEnabled ) {m_pAudioPorts[1]->Enable();}
                    return BOMX_ERR_TRACE(OMX_ErrorUndefined);
                }
            }
        }
        else
        {
            // Transitioning to idle from any state except loaded is equivalent to stop
            if ( m_pAudioPorts[0]->IsPopulated() )
            {
                (void)SetInputPortState(OMX_StateIdle);
            }
            if ( m_pAudioPorts[1]->IsPopulated() )
            {
                (void)SetOutputPortState(OMX_StateIdle);
            }
        }
        ALOGD_IF((m_logMask & B_LOG_ADEC_TRANS_STATE), "%d End State Change %s->%s", m_instanceNum, BOMX_StateName(oldState), BOMX_StateName(newState));
        return OMX_ErrorNone;
    }
    case OMX_StateExecuting:
    case OMX_StatePause:
        if ( m_pAudioPorts[1]->IsPopulated() && m_pAudioPorts[1]->IsEnabled() )
        {
            errCode = SetOutputPortState(newState);
            if ( errCode )
            {
                return BOMX_ERR_TRACE(OMX_ErrorUndefined);
            }
        }
        if ( m_pAudioPorts[0]->IsPopulated() && m_pAudioPorts[0]->IsEnabled() )
        {
            errCode = SetInputPortState(newState);
            if ( errCode )
            {
                return BOMX_ERR_TRACE(OMX_ErrorUndefined);
            }
        }
        ALOGD_IF((m_logMask & B_LOG_ADEC_TRANS_STATE), "%d End State Change %s->%s", m_instanceNum, BOMX_StateName(oldState), BOMX_StateName(newState));
        return OMX_ErrorNone;
    default:
        ALOGW("%d Unsupported state %u", m_instanceNum, newState);
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
    }
}

OMX_ERRORTYPE BOMX_AudioDecoder::CommandFlush(
    OMX_U32 portIndex)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    if ( StateChangeInProgress() )
    {
        ALOGW("Flush should not be called during a state change");
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
    }

    // Handle case for OMX_ALL by calling flush on each port
    if ( portIndex == OMX_ALL )
    {
        ALOGV("%d Flushing all ports", m_instanceNum);

        err = CommandFlush(m_audioPortBase);
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }
        err = CommandFlush(m_audioPortBase+1);
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }
    }
    else
    {
        BOMX_Port *pPort = FindPortByIndex(portIndex);
        if ( NULL == pPort )
        {
            ALOGW("Invalid port %u", portIndex);
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        ALOGV("%d Flushing %s port", m_instanceNum, pPort->GetDir() == OMX_DirInput ? "input" : "output");
        if ( portIndex == m_audioPortBase )
        {
            // Input port
            if ( m_hAudioDecoder )
            {
                (void)SetInputPortState(OMX_StateIdle);
                (void)SetInputPortState(StateGet());
                m_configBufferState = ConfigBufferState_eFlushed;
            }
        }
        else
        {
            // Output port
            if ( m_pAudioPorts[1]->IsEnabled() && m_pAudioPorts[1]->IsPopulated() )
            {
                (void)SetOutputPortState(OMX_StateIdle);
                (void)SetOutputPortState(StateGet());
            }
        }
        ReturnPortBuffers(pPort);
    }

    return err;
}

OMX_ERRORTYPE BOMX_AudioDecoder::CommandPortEnable(
    OMX_U32 portIndex)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    // Handle case for OMX_ALL by calling enable on each port
    if ( portIndex == OMX_ALL )
    {
        ALOGV("%d Enabling all ports", m_instanceNum);

        // Enable output first
        if ( !m_pAudioPorts[1]->IsEnabled() )
        {
            err = CommandPortEnable(m_audioPortBase+1);
            if ( err )
            {
                return BOMX_ERR_TRACE(err);
            }
        }
        // Enable input
        if ( !m_pAudioPorts[0]->IsEnabled() )
        {
            err = CommandPortEnable(m_audioPortBase);
            if ( err )
            {
                return BOMX_ERR_TRACE(err);
            }
        }
    }
    else
    {
        BOMX_Port *pPort = FindPortByIndex(portIndex);
        if ( NULL == pPort )
        {
            ALOGW("Invalid port %u", portIndex);
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if ( pPort->IsEnabled() )
        {
            ALOGW("Port %u is already enabled", portIndex);
            return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
        }
        ALOGV("%d Enabling %s port", m_instanceNum, pPort->GetDir() == OMX_DirInput ? "input" : "output");
        err = pPort->Enable();
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }
        if ( StateGet() != OMX_StateLoaded )
        {
            // Wait for port to become populated
            ALOGV("%d Waiting for port to populate", m_instanceNum);
            PortWaitBegin();
            // Now we need to wait for all enabled ports to become populated
            while ( !pPort->IsPopulated() )
            {
                if ( PortWait() != B_ERROR_SUCCESS )
                {
                    ALOGW("Timeout waiting for port %u to be populated", portIndex);
                    PortWaitEnd();
                    (void)pPort->Disable();
                    return BOMX_ERR_TRACE(OMX_ErrorTimeout);
                }
            }
            PortWaitEnd();
            ALOGV("%d Done waiting for port to populate", m_instanceNum);

            NEXUS_Error errCode;
            // Handle port specifics
            if ( pPort->GetDir() == OMX_DirInput )
            {
                errCode = SetInputPortState(StateGet());
                if ( errCode )
                {
                    (void)CommandPortDisable(portIndex);
                    return BOMX_ERR_TRACE(OMX_ErrorUndefined);
                }
            }
            else
            {
                errCode = SetOutputPortState(StateGet());
                if ( errCode )
                {
                    (void)CommandPortDisable(portIndex);
                    return BOMX_ERR_TRACE(OMX_ErrorUndefined);
                }
            }
        }
    }

    return err;
}

OMX_ERRORTYPE BOMX_AudioDecoder::CommandPortDisable(
    OMX_U32 portIndex)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    // Handle case for OMX_ALL by calling enable on each port
    if ( portIndex == OMX_ALL )
    {
        ALOGV("%d Disabling all ports", m_instanceNum);

        if ( m_pAudioPorts[0]->IsEnabled() )
        {
            err = CommandPortDisable(m_audioPortBase);
            if ( err )
            {
                return BOMX_ERR_TRACE(err);
            }
        }
        if ( m_pAudioPorts[1]->IsEnabled() )
        {
            err = CommandPortDisable(m_audioPortBase+1);
            if ( err )
            {
                return BOMX_ERR_TRACE(err);
            }
        }
    }
    else
    {
        BOMX_Port *pPort = FindPortByIndex(portIndex);
        if ( NULL == pPort )
        {
            ALOGW("Invalid port %u", portIndex);
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if ( pPort->IsDisabled() )
        {
            ALOGW("Port %u is already disabled", portIndex);
            return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
        }
        ALOGV("%d Disabling %s port", m_instanceNum, pPort->GetDir() == OMX_DirInput ? "input" : "output");
        err = pPort->Disable();
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }
        if ( StateGet() != OMX_StateLoaded )
        {
            NEXUS_Error errCode;
            // Release port resources
            if ( pPort->GetDir() == OMX_DirInput )
            {
                errCode = SetInputPortState(OMX_StateLoaded);
                ALOG_ASSERT(errCode == NEXUS_SUCCESS);
            }
            else
            {
                errCode = SetOutputPortState(OMX_StateLoaded);
                ALOG_ASSERT(errCode == NEXUS_SUCCESS);
            }

            // Return port buffers to client
            ReturnPortBuffers(pPort);

            // Wait for port to become de-populated
            ALOGV("%d Waiting for port to de-populate", m_instanceNum);
            PortWaitBegin();
            // Now we need to wait for all enabled ports to become populated
            while ( !pPort->IsEmpty() )
            {
                if ( PortWait() != B_ERROR_SUCCESS )
                {
                    ALOGW("Timeout waiting for port %u to be de-populated", portIndex);
                    PortWaitEnd();
                    (void)pPort->Enable();
                    return BOMX_ERR_TRACE(OMX_ErrorTimeout);
                }
            }
            PortWaitEnd();
            ALOGV("%d Done waiting for port to de-populate", m_instanceNum);
        }
    }

    return err;
}

OMX_ERRORTYPE BOMX_AudioDecoder::AddInputPortBuffer(
        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
        OMX_IN OMX_U32 nPortIndex,
        OMX_IN OMX_PTR pAppPrivate,
        OMX_IN OMX_U32 nSizeBytes,
        OMX_IN OMX_U8* pBuffer,
        bool componentAllocated)
{
    BOMX_Port *pPort;
    BOMX_AudioDecoderInputBufferInfo *pInfo;
    OMX_ERRORTYPE err;
    NEXUS_Error errCode;
    size_t headerBufferSize;
    NEXUS_ClientConfiguration               clientConfig;
    NEXUS_MemoryAllocationSettings          memorySettings;

    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    NEXUS_Memory_GetDefaultAllocationSettings(&memorySettings);
    memorySettings.heap = clientConfig.heap[NXCLIENT_FULL_HEAP];
    if (memorySettings.heap == NULL)
    {
       memorySettings.heap = clientConfig.heap[NXCLIENT_DEFAULT_HEAP];
    }

    if ( NULL == ppBufferHdr )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    pPort = FindPortByIndex(nPortIndex);
    if ( NULL == pPort || pPort->GetDir() != OMX_DirInput )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }

    if ( pPort->IsTunneled() )
    {
        ALOGW("Cannot add buffers to a tunneled port");
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }

    pInfo = new BOMX_AudioDecoderInputBufferInfo;
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    headerBufferSize = B_PES_HEADER_LENGTH_WITH_PTS;  // Basic PES header w/PTS.
    switch ( GetCodec() )
    {
    case OMX_AUDIO_CodingAAC:
        headerBufferSize += B_AAC_ADTS_HEADER_LEN;
        break;
    default:
        break;
    }
    // To avoid unbounded PES, create enough headers to fit each input buffer into individual PES packets.  Subsequent PES packets will not have a timestamp so they are only 9 bytes.
    headerBufferSize += B_PES_HEADER_LENGTH_WITHOUT_PTS * ((B_MAX_DESCRIPTORS_PER_BUFFER-4)/2);
    pInfo->maxHeaderLen = headerBufferSize;

    errCode = NEXUS_Memory_Allocate(headerBufferSize, &memorySettings, &pInfo->pHeader);
    if ( errCode )
    {
        delete pInfo;
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    if ( componentAllocated )
    {
        pInfo->pPayload = NULL;
    }
    else
    {
        errCode = AllocateInputBuffer(nSizeBytes, pInfo->pPayload);
        if ( errCode )
        {
            NEXUS_Memory_Free(pInfo->pHeader);
            delete pInfo;
            return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
        }
    }
    err = pPort->AddBuffer(ppBufferHdr, pAppPrivate, nSizeBytes, pBuffer, pInfo, componentAllocated);
    if ( OMX_ErrorNone != err )
    {
        if ( pInfo->pPayload )
        {
            NEXUS_Memory_Free(pInfo->pPayload);
        }
        NEXUS_Memory_Free(pInfo->pHeader);
        delete pInfo;
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    PortStatusChanged();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_AudioDecoder::AddOutputPortBuffer(
        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
        OMX_IN OMX_U32 nPortIndex,
        OMX_IN OMX_PTR pAppPrivate,
        OMX_IN OMX_U32 nSizeBytes,
        OMX_IN OMX_U8* pBuffer)
{
    BOMX_Port *pPort;
    BOMX_AudioDecoderOutputBufferInfo *pInfo;
    OMX_ERRORTYPE err;
    NEXUS_Error errCode;
    NEXUS_ClientConfiguration               clientConfig;
    NEXUS_MemoryAllocationSettings          memorySettings;

    NEXUS_Platform_GetClientConfiguration(&clientConfig);

    if ( NULL == ppBufferHdr )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    pPort = FindPortByIndex(nPortIndex);
    if ( NULL == pPort || pPort->GetDir() != OMX_DirOutput )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }

    if ( pPort->IsTunneled() )
    {
        ALOGW("Cannot add buffers to a tunneled port");
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }

    pInfo = new BOMX_AudioDecoderOutputBufferInfo;
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }
    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->pClientMemory = pBuffer;
    pInfo->hMemoryBlock = NEXUS_MemoryBlock_Allocate(
       !clientConfig.heap[NXCLIENT_FULL_HEAP] ? clientConfig.heap[NXCLIENT_DEFAULT_HEAP] : clientConfig.heap[NXCLIENT_FULL_HEAP],
       nSizeBytes, 0, NULL);
    if ( NULL == pInfo->hMemoryBlock )
    {
        delete pInfo;
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }
    errCode = NEXUS_MemoryBlock_Lock(pInfo->hMemoryBlock, &pInfo->pNexusMemory);
    if ( errCode )
    {
        NEXUS_MemoryBlock_Free(pInfo->hMemoryBlock);
        delete pInfo;
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }
    NEXUS_FlushCache(pInfo->pNexusMemory, nSizeBytes);

    err = pPort->AddBuffer(ppBufferHdr, pAppPrivate, nSizeBytes, pBuffer, pInfo, false);
    if ( OMX_ErrorNone != err )
    {
        NEXUS_MemoryBlock_Free(pInfo->hMemoryBlock);
        delete pInfo;
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    PortStatusChanged();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_AudioDecoder::UseBuffer(
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes,
    OMX_IN OMX_U8* pBuffer)
{
    OMX_ERRORTYPE err;

    if ( NULL == pBuffer )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    if ( nPortIndex == m_audioPortBase )
    {
        err = AddInputPortBuffer(ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes, pBuffer, false);
        if ( err != OMX_ErrorNone )
        {
            return BOMX_ERR_TRACE(err);
        }
    }
    else
    {
        err = AddOutputPortBuffer(ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes, pBuffer);
        if ( err != OMX_ErrorNone )
        {
            return BOMX_ERR_TRACE(err);
        }
    }

    return OMX_ErrorNone;
}


OMX_ERRORTYPE BOMX_AudioDecoder::AllocateBuffer(
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes)
{
    OMX_ERRORTYPE err;
    NEXUS_Error errCode;

    if ( NULL == ppBuffer )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    if ( nPortIndex == m_audioPortBase )
    {
        void *pBuffer;
        errCode = AllocateInputBuffer(nSizeBytes, pBuffer);

        if ( errCode )
        {
            return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
        }
        err = AddInputPortBuffer(ppBuffer, nPortIndex, pAppPrivate, nSizeBytes, (OMX_U8 *)pBuffer, true);
        if ( err != OMX_ErrorNone )
        {
            FreeInputBuffer(pBuffer);
            return BOMX_ERR_TRACE(err);
        }
    }
    else
    {
        // TODO: Implement if required
        ALOGW("AllocateBuffer is not supported for output ports");
        return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_AudioDecoder::FreeBuffer(
    OMX_IN  OMX_U32 nPortIndex,
    OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    BOMX_Port *pPort;
    BOMX_Buffer *pBuffer;
    OMX_ERRORTYPE err;

    pPort = FindPortByIndex(nPortIndex);
    if ( NULL == pPort )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }
    if ( NULL == pBufferHeader )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    pBuffer = BOMX_BUFFERHEADER_TO_BUFFER(pBufferHeader);
    if ( NULL == pBufferHeader )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    if ( pPort->GetDir() == OMX_DirInput )
    {
        BOMX_AudioDecoderInputBufferInfo *pInfo;
        pInfo = (BOMX_AudioDecoderInputBufferInfo *)pBuffer->GetComponentPrivate();
        if ( NULL == pInfo )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        err = pPort->RemoveBuffer(pBufferHeader);
        if ( err != OMX_ErrorNone )
        {
            return BOMX_ERR_TRACE(err);
        }
        NEXUS_Memory_Free(pInfo->pHeader);
        if ( pInfo->pPayload )
        {
            // UseBuffer
            FreeInputBuffer(pInfo->pPayload);
        }
        else
        {
            // AllocateBuffer
            void* buff = pBufferHeader->pBuffer;
            FreeInputBuffer(buff);
        }
        delete pInfo;
    }
    else
    {
        BOMX_AudioDecoderOutputBufferInfo *pInfo;
        pInfo = (BOMX_AudioDecoderOutputBufferInfo *)pBuffer->GetComponentPrivate();
        if ( NULL == pInfo )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        err = pPort->RemoveBuffer(pBufferHeader);
        if ( err != OMX_ErrorNone )
        {
            return BOMX_ERR_TRACE(err);
        }
        NEXUS_MemoryBlock_Unlock(pInfo->hMemoryBlock);
        NEXUS_MemoryBlock_Free(pInfo->hMemoryBlock);
        delete pInfo;
    }
    PortStatusChanged();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_AudioDecoder::BuildInputFrame(
    OMX_BUFFERHEADERTYPE *pBufferHeader,
    bool first,
    unsigned chunkLength,
    uint32_t pts,
    void *pCodecHeader,
    size_t codecHeaderLength,
    NEXUS_PlaypumpScatterGatherDescriptor *pDescriptors,
    unsigned maxDescriptors,
    size_t *pNumDescriptors
    )
{
    BOMX_Buffer *pBuffer;
    BOMX_AudioDecoderInputBufferInfo *pInfo;
    size_t headerBytes;
    size_t bufferBytesRemaining;
    unsigned numDescriptors = 0;
    size_t chunkBytesAvailable;
    uint8_t *pPesHeader;
    uint8_t *pPayload;
    bool configSubmitted=false;

    /********************************************************************************************************************
    * This is what we are constructing here.  Not all codecs can support unbounded PES input, so we need to write
    * multiple bounded PES packets for frames that will not fit in a single packet
    *
    * [PES Header w/PTS] | [Cached Codec Config Data] | [Codec Frame Header (e.g. BCMA)] | [First Frame Payload Chunk] |
    *   [PES Header(s) w/o PTS] | [Next Frame Payload Chunk] (repeats until frame is complete)
    *****************************************************************/

    pBuffer = BOMX_BUFFERHEADER_TO_BUFFER(pBufferHeader);
    ALOG_ASSERT(NULL != pBuffer);
    pInfo = (BOMX_AudioDecoderInputBufferInfo *)pBuffer->GetComponentPrivate();
    ALOG_ASSERT(NULL != pInfo);

    bufferBytesRemaining = chunkLength;
    ALOGV("%d Input Frame Offset %u, Length %u, PTS %#x first=%d", m_instanceNum, pBufferHeader->nOffset, chunkLength, pts, first ? 1 : 0);

    if ( maxDescriptors < 4 )
    {
        ALOGE("Insufficient descriptors available");
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    // Handle copying frame data into HW buffer if need be
    if ( NULL != pInfo->pPayload )
    {
        pPayload = (uint8_t *)pInfo->pPayload;
        if ( bufferBytesRemaining > 0 )
        {
            BKNI_Memcpy(pPayload, (const void *)(pBufferHeader->pBuffer + pBufferHeader->nOffset), bufferBytesRemaining);
        }
    }
    else
    {
        pPayload = pBufferHeader->pBuffer + pBufferHeader->nOffset;
    }

    // If this is the first frame, reset header length and flush data cache
    if ( first )
    {
        pInfo->headerLen = 0;
        // Flush data cache for frame payload
        if ( !m_secureDecoder && pBufferHeader->nFilledLen > 0 )
        {
            NEXUS_FlushCache(pPayload, pBufferHeader->nFilledLen);
        }
    }

    // Begin first PES packet
    pPesHeader = ((uint8_t *)pInfo->pHeader)+pInfo->headerLen;
    headerBytes = m_pPes->InitHeader(pPesHeader, (pInfo->maxHeaderLen-pInfo->headerLen), pts);
    if ( 0 == headerBytes )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    pInfo->headerLen += headerBytes;
    chunkBytesAvailable = B_MAX_PES_PACKET_LENGTH-(headerBytes-B_PES_HEADER_START_BYTES);  /* Max PES length - bytes used in header that affect packet length field */
    pDescriptors[0].addr = pPesHeader;
    pDescriptors[0].length = headerBytes;
    numDescriptors = 1;

    // Add codec config if required
    if ( m_configBufferState != ConfigBufferState_eSubmitted )
    {
        ALOG_ASSERT(chunkBytesAvailable >= m_configBufferSize); // This should always be true, the config buffer is very small.
        if ( m_configBufferSize > 0 )
        {
            pDescriptors[numDescriptors].addr = m_pConfigBuffer;
            pDescriptors[numDescriptors].length = m_configBufferSize;
            chunkBytesAvailable -= m_configBufferSize;
            numDescriptors++;
        }
        m_configBufferState = ConfigBufferState_eSubmitted;
        configSubmitted = true;
    }

    // Add codec header if required
    if ( codecHeaderLength > 0 )
    {
        pDescriptors[numDescriptors].addr = (void *)((uint8_t *)pInfo->pHeader + pInfo->headerLen);
        pDescriptors[numDescriptors].length = codecHeaderLength;
        ALOG_ASSERT(chunkBytesAvailable >= codecHeaderLength);
        chunkBytesAvailable -= codecHeaderLength;
        BKNI_Memcpy(pDescriptors[numDescriptors].addr, pCodecHeader, codecHeaderLength);
        pInfo->headerLen += codecHeaderLength;
        numDescriptors++;
    }

    // Add as much frame payload as possible into remaining chunk bytes
    if ( bufferBytesRemaining > 0 )
    {
        pDescriptors[numDescriptors].addr = (void *)pPayload;
        if ( chunkBytesAvailable >= bufferBytesRemaining )
        {
            pDescriptors[numDescriptors].length = bufferBytesRemaining;
        }
        else
        {
            pDescriptors[numDescriptors].length = chunkBytesAvailable;
        }
        chunkBytesAvailable -= pDescriptors[numDescriptors].length;
        bufferBytesRemaining -= pDescriptors[numDescriptors].length;
        pPayload += pDescriptors[numDescriptors].length;
        numDescriptors++;
    }

    // Update PES length now that we know it
    m_pPes->SetPayloadLength(pPesHeader, B_MAX_PES_PACKET_LENGTH-chunkBytesAvailable);

    // Now, we deal with residual data.  This is simpler, we just build PES packets and
    // write as much data as we can in each one.
    while ( bufferBytesRemaining > 0 )
    {
        if ( maxDescriptors - numDescriptors < 2 )
        {
            ALOGE("Insufficient descriptors available");
            if ( configSubmitted ) { m_configBufferState = ConfigBufferState_eAccumulating; }
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }

        // PES Header
        pPesHeader = (uint8_t *)pInfo->pHeader + pInfo->headerLen;
        headerBytes = m_pPes->InitHeader(pPesHeader, pInfo->maxHeaderLen-pInfo->headerLen);
        if ( 0 == headerBytes )
        {
            if ( configSubmitted ) { m_configBufferState = ConfigBufferState_eAccumulating; }
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        pInfo->headerLen += headerBytes;
        chunkBytesAvailable = B_MAX_PES_PACKET_LENGTH-(headerBytes-B_PES_HEADER_START_BYTES);
        pDescriptors[numDescriptors].addr = pPesHeader;
        pDescriptors[numDescriptors].length = headerBytes;
        numDescriptors++;

        // Payload
        pDescriptors[numDescriptors].addr = pPayload;
        if ( chunkBytesAvailable >= bufferBytesRemaining )
        {
            pDescriptors[numDescriptors].length = bufferBytesRemaining;
        }
        else
        {
            pDescriptors[numDescriptors].length = chunkBytesAvailable;
        }
        chunkBytesAvailable -= pDescriptors[numDescriptors].length;
        bufferBytesRemaining -= pDescriptors[numDescriptors].length;
        pPayload += pDescriptors[numDescriptors].length;
        numDescriptors++;

        // Update PES length now that we know it
        m_pPes->SetPayloadLength(pPesHeader, B_MAX_PES_PACKET_LENGTH-chunkBytesAvailable);
    }

    // Update buffer descriptor with amount consumed
    pBuffer->UpdateBuffer(chunkLength);

    // Finally, flush header buffer
    NEXUS_FlushCache(pInfo->pHeader, pInfo->maxHeaderLen);

    *pNumDescriptors = numDescriptors;
    return OMX_ErrorNone;
}

struct InputBufferHeader
{
    char tag[4];
    uint32_t length; // not including header
    uint32_t flags;
    uint32_t timeHi;
    uint32_t timeLo;
    uint32_t pad[3];
};

void BOMX_AudioDecoder::DumpInputBuffer(OMX_BUFFERHEADERTYPE *pHeader)
{
    InputBufferHeader hdr;
    ALOG_ASSERT(NULL != pHeader);

    if ( NULL == m_pInputFile || m_secureDecoder )
        return;

    hdr.tag[0] = 'O';
    hdr.tag[1] = 'M';
    hdr.tag[2] = 'X';
    hdr.tag[3] = 'I';
    hdr.flags = pHeader->nFlags;
    hdr.length = pHeader->nFilledLen;
    hdr.timeHi = (uint32_t)(pHeader->nTimeStamp >> 32);
    hdr.timeLo = (uint32_t)(pHeader->nTimeStamp);
    hdr.pad[0] = hdr.pad[1] = hdr.pad[2] = 0;

    fwrite(&hdr, sizeof(hdr), 1, m_pInputFile);
    fwrite(pHeader->pBuffer + pHeader->nOffset, pHeader->nFilledLen, 1, m_pInputFile);
}

OMX_ERRORTYPE BOMX_AudioDecoder::EmptyThisBuffer(
    OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    OMX_ERRORTYPE err;
    BOMX_Buffer *pBuffer;
    BOMX_AudioDecoderInputBufferInfo *pInfo;
    size_t payloadLen;
    void *pPayload;
    NEXUS_PlaypumpScatterGatherDescriptor desc[B_MAX_DESCRIPTORS_PER_BUFFER];
    NEXUS_Error errCode;
    size_t numConsumed, numRequested;
    uint8_t *pCodecHeader = NULL;
    size_t codecHeaderLength = 0;
    unsigned frame, numFrames=1;
    uint32_t frameLength[B_MAX_EXTRAFRAMES_PER_BUFFER+1];
    bool emptyBuffer = false;
    frameLength[0] = pBufferHeader->nFilledLen;

    if ( NULL == pBufferHeader )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    pBuffer = BOMX_BUFFERHEADER_TO_BUFFER(pBufferHeader);
    if ( NULL == pBuffer || pBuffer->GetDir() != OMX_DirInput )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    if ( (pBufferHeader->nFilledLen + pBufferHeader->nOffset) > pBufferHeader->nAllocLen )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    if ( pBufferHeader->nInputPortIndex != m_audioPortBase )
    {
        ALOGW("Invalid buffer");
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    pInfo = (BOMX_AudioDecoderInputBufferInfo *)pBuffer->GetComponentPrivate();
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    ERROR_OUT_ON_NEXUS_ACTIVE_STANDBY;

    pInfo->numDescriptors = 0;
    pInfo->complete = false;

    ALOGD_IF((m_logMask & B_LOG_ADEC_IN_FEED), "%s, comp:%s %d, buff:%p len:%d ts:%d flags:0x%x", __FUNCTION__, GetName(), m_instanceNum, pBufferHeader->pBuffer, pBufferHeader->nFilledLen, (int)pBufferHeader->nTimeStamp, pBufferHeader->nFlags);

    if ( m_pInputFile )
    {
        DumpInputBuffer(pBufferHeader);
    }

    if ( pBufferHeader->nFlags & OMX_BUFFERFLAG_CODECCONFIG )
    {
        if ( GetCodec() == OMX_AUDIO_CodingAAC && m_aacParams.eAACStreamFormat == OMX_AUDIO_AACStreamFormatMP4FF )
        {
            // Parse ASC (AudioSpecificConfig) block and extract info required to generate ADTS headers
            BOMX_AAC_ParseASC(pBufferHeader->pBuffer + pBufferHeader->nOffset, pBufferHeader->nFilledLen,  &m_ascInfo);
        }
        if ( m_configBufferState != ConfigBufferState_eAccumulating )
        {
            // If the app re-sends config data after we have delivered it to the decoder we
            // may be receiving a dynamic resolution change.  Overwrite old config data with
            // the new data
            ALOGV("%d Invalidating cached config buffer ", m_instanceNum);
            ConfigBufferInit();
        }

        ALOGV("%d Accumulating %u bytes of codec config data", m_instanceNum, pBufferHeader->nFilledLen);

        err = ConfigBufferAppend(pBufferHeader->pBuffer + pBufferHeader->nOffset, pBufferHeader->nFilledLen);
        if ( err != OMX_ErrorNone )
        {
            return BOMX_ERR_TRACE(err);
        }

        // Config buffers aren't sent individually, they are just appended above and sent with the next frame.  Queue this buffer with no descriptors so it will be returned immediately.
        InputBufferNew();
        err = m_pAudioPorts[0]->QueueBuffer(pBufferHeader);
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }
        return OMX_ErrorNone;
    }

    // If we get here, we have an actual frame to send.

    if ( pBufferHeader->nFilledLen > 0 )
    {
        // Track the buffer
        if ( !m_pBufferTracker->Add(pBufferHeader, &pInfo->pts) )
        {
            ALOGW("Unable to track buffer");
            pInfo->pts = BOMX_TickToPts(&pBufferHeader->nTimeStamp);
        }

        for ( frame = 0; frame < numFrames; frame++ )
        {
            switch ( (int)GetCodec() )
            {
            case OMX_AUDIO_CodingAAC:
                if ( m_aacParams.eAACStreamFormat == OMX_AUDIO_AACStreamFormatMP4FF )
                {
                    // Create ADTS header for this frame
                    pCodecHeader = m_pBcmaHeader;
                    BOMX_AAC_SetupAdtsHeader(pCodecHeader, sizeof(m_pBcmaHeader),  &codecHeaderLength, frameLength[frame], &m_ascInfo);
                }
                break;
            default:
                break;
            }

            // Build up descriptor list
            err = BuildInputFrame(pBufferHeader, (frame == 0), frameLength[frame], pInfo->pts, pCodecHeader, codecHeaderLength, desc, B_MAX_DESCRIPTORS_PER_BUFFER, &numRequested);
            if ( err != OMX_ErrorNone )
            {
                return BOMX_ERR_TRACE(err);
            }
            ALOG_ASSERT(numRequested <= B_MAX_DESCRIPTORS_PER_BUFFER);

            if ( numRequested > 0 )
            {
                unsigned i;
                // Log data to file if requested
                if ( NULL != m_pPesFile )
                {
                    for ( i = 0; i < numRequested; i++ )
                    {
                        fwrite(desc[i].addr, 1, desc[i].length, m_pPesFile);
                    }
                }
                for ( i = 0; i < numRequested; i++ )
                {
                    unsigned j;
                    for ( j = 0; j < numRequested; j++ )
                    {
                        if ( i == j ) continue;

                        // Check for overlap
                        if ( desc[i].addr == desc[j].addr )
                        {
                            ALOGE("Error, desc %u and %u share the same address", i, j);
                        }
                        else if ( desc[i].addr < desc[j].addr && (uint8_t *)desc[i].addr + desc[i].length > desc[j].addr )
                        {
                            ALOGE("Error, desc %u intrudes in to desc %u", i, j);
                        }
                        else if ( desc[j].addr < desc[i].addr && (uint8_t *)desc[j].addr + desc[j].length > desc[i].addr )
                        {
                            ALOGE("Error, desc %u intrudes into desc %u", j, i);
                        }
                    }
                }

                // Submit to playpump
                errCode = NEXUS_Playpump_SubmitScatterGatherDescriptor(m_hPlaypump, desc, numRequested, &numConsumed);
                if ( errCode )
                {
                    return BOMX_ERR_TRACE(OMX_ErrorUndefined);
                }
                ALOG_ASSERT(numConsumed == numRequested);
                pInfo->numDescriptors += numRequested;
                m_submittedDescriptors += numConsumed;
            }
        }
        InputBufferNew();
        err = m_pAudioPorts[0]->QueueBuffer(pBufferHeader);
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }
    }
    else
    {
        emptyBuffer = true;
        // Mark the empty frame as queued.
        InputBufferNew();
        err = m_pAudioPorts[0]->QueueBuffer(pBufferHeader);
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }
        // Force check of playpump events
        B_Event_Set(m_hPlaypumpEvent);
    }

    if ( pBufferHeader->nFlags & OMX_BUFFERFLAG_EOS )
    {
        numRequested = 1;
        desc[0].addr = m_pEosBuffer;
        desc[0].length = BOMX_AUDIO_EOS_LEN;
        // The audio DSP doesn't currently use this, but it will force RAVE to flush any pending data
        errCode = NEXUS_Playpump_SubmitScatterGatherDescriptor(m_hPlaypump, desc, numRequested, &numConsumed);
        if ( errCode )
        {
            return BOMX_ERR_TRACE(OMX_ErrorUndefined);
        }
        ALOG_ASSERT(numConsumed == numRequested);
        pInfo->numDescriptors += numRequested;
        m_submittedDescriptors += numConsumed;
        m_eosPending = true;
        if ( NULL != m_pPesFile )
        {
            fwrite(m_pEosBuffer, 1, BOMX_AUDIO_EOS_LEN, m_pPesFile);
        }
        if ( emptyBuffer )
        {
            ALOGV("%d Queued standalone EOS", m_instanceNum);
            m_eosStandalone = true;
        }
        else
        {
            ALOGV("%d Queued EOS buffer w/payload", m_instanceNum);
            m_eosStandalone = false;
        }
    }

    if ( m_pAudioPorts[1]->QueueDepth() > 0 )
    {
        // Force a check for new output frames each time a new input frame arrives if we have output buffers ready to fill
        B_Event_Set(m_hOutputFrameEvent);
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_AudioDecoder::FillThisBuffer(
    OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    BOMX_Buffer *pBuffer;
    BOMX_AudioDecoderOutputBufferInfo *pInfo;
    OMX_ERRORTYPE err;

    if ( NULL == pBufferHeader )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    pBuffer = BOMX_BUFFERHEADER_TO_BUFFER(pBufferHeader);
    if ( NULL == pBuffer || pBuffer->GetDir() != OMX_DirOutput )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    pInfo = (BOMX_AudioDecoderOutputBufferInfo *)pBuffer->GetComponentPrivate();
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    ERROR_OUT_ON_NEXUS_ACTIVE_STANDBY;

    ALOGV("Fill Buffer, comp:%s %d ts %u us pInfo %p HDR %p", GetName(), m_instanceNum, (unsigned int)pBufferHeader->nTimeStamp, pInfo, pBufferHeader);
    // Determine what to do with the buffer
    pBuffer->Reset();

    if ( m_decoderState == BOMX_AudioDecoderState_eStarted && !pInfo->nexusOwned )
    {
        NEXUS_Error errCode;
        errCode = NEXUS_AudioDecoder_QueueBuffer(m_hAudioDecoder, pInfo->hMemoryBlock, 0, pBufferHeader->nAllocLen);
        if ( errCode )
        {
            ALOGE("Unable to queue output buffer hDecoder %p block %p len %u (%p) - rc %u", m_hAudioDecoder, pInfo->hMemoryBlock, pBufferHeader->nAllocLen, pBufferHeader, errCode);
            return BOMX_ERR_TRACE(OMX_ErrorUndefined);
        }
        pInfo->nexusOwned = true;
    }

    err = m_pAudioPorts[1]->QueueBuffer(pBufferHeader);
    ALOG_ASSERT(err == OMX_ErrorNone);

    // Signal thread to look for more
    B_Event_Set(m_hOutputFrameEvent);

    return OMX_ErrorNone;
}

static bool FindBufferFromPts(BOMX_Buffer *pBuffer, void *pData)
{
    BOMX_AudioDecoderInputBufferInfo *pInfo;
    uint32_t *pPts = (uint32_t *)pData;

    ALOG_ASSERT(NULL != pBuffer);
    ALOG_ASSERT(NULL != pPts);

    pInfo = (BOMX_AudioDecoderInputBufferInfo *)pBuffer->GetComponentPrivate();
    ALOG_ASSERT(NULL != pInfo);

    return (pInfo->pts == *pPts) ? true : false;
}

void BOMX_AudioDecoder::CancelTimerId(B_SchedulerTimerId& timerId)
{
    if ( timerId )
    {
        CancelTimer(timerId);
        timerId = NULL;
    }
}

void BOMX_AudioDecoder:: CleanupPortBuffers(OMX_U32 nPortIndex)
{
    BOMX_Port *pPort;

    pPort = FindPortByIndex(nPortIndex);
    ALOG_ASSERT (NULL != pPort);

    // Make sure there are no buffers in the port queue before freeing them
    BOMX_Buffer *pBuffer = pPort->GetBuffer();
    while ( pBuffer != NULL )
    {
        BOMX_Buffer *pNextBuffer = pPort->GetNextBuffer(pBuffer);
        pPort->BufferComplete(pBuffer);
        pBuffer = pNextBuffer;
    }
    while ( !pPort->IsEmpty() )
    {
        BOMX_Buffer *pBuffer = pPort->GetPortBuffer();
        ALOG_ASSERT(NULL != pBuffer);
        OMX_ERRORTYPE err = FreeBuffer((pPort->GetDir() == OMX_DirInput) ? m_videoPortBase : m_videoPortBase+1,  pBuffer->GetHeader());
        ALOGE_IF(err != OMX_ErrorNone, "Failed to free buffer %d", err);
    }
}

void BOMX_AudioDecoder::PlaypumpEvent()
{
    NEXUS_PlaypumpStatus playpumpStatus;
    unsigned numComplete;

    CancelTimerId(m_playpumpTimerId);
    if ( NULL == m_hPlaypump )
    {
        // This can happen with rapid startup/shutdown sequences such as random action tests
        ALOGW("Playpump event received, but playpump has been closed.");
        return;
    }

    NEXUS_Playpump_GetStatus(m_hPlaypump, &playpumpStatus);
    if ( playpumpStatus.started )
    {
        BOMX_Buffer *pBuffer, *pFifoHead=NULL;

        if ( m_hAudioDecoder )
        {
            NEXUS_AudioDecoderStatus fifoStatus;
            NEXUS_Error errCode = NEXUS_AudioDecoder_GetStatus(m_hAudioDecoder, &fifoStatus);
            if ( NEXUS_SUCCESS == errCode && fifoStatus.ptsType == NEXUS_PtsType_eCoded )
            {
                pFifoHead = m_pAudioPorts[0]->FindBuffer(FindBufferFromPts, (void *)&fifoStatus.pts);
                if ( pFifoHead )
                {
                    pFifoHead = m_pAudioPorts[0]->GetNextBuffer(pFifoHead);
                }
            }
        }

        numComplete = m_submittedDescriptors - playpumpStatus.descFifoDepth;
        ALOGV("submitted %u fifoDepth %zu -> %u completed", m_submittedDescriptors, playpumpStatus.descFifoDepth, numComplete);

        for (pBuffer = m_pAudioPorts[0]->GetBuffer();
             (pBuffer != NULL) && (pBuffer != pFifoHead);
             pBuffer = m_pAudioPorts[0]->GetNextBuffer(pBuffer))
        {
            BOMX_AudioDecoderInputBufferInfo *pInfo =
            (BOMX_AudioDecoderInputBufferInfo *)pBuffer->GetComponentPrivate();
            ALOG_ASSERT(NULL != pInfo);
            if (pInfo->complete)
                continue;
            if ( numComplete >= pInfo->numDescriptors )
            {
                numComplete -= pInfo->numDescriptors;
                ALOG_ASSERT(m_submittedDescriptors >= pInfo->numDescriptors);
                ALOGV("completed %u, marking buffer done - %u remain", pInfo->numDescriptors, numComplete);
                m_submittedDescriptors -= pInfo->numDescriptors;
                pInfo->complete = true;
            }
            else
            {
                // Some descriptors are still pending for this buffer
                ALOGV("completed %u, buffer requires %u - waiting", numComplete, pInfo->numDescriptors);
                break;
            }
        }

        // If there are still input buffers waiting in rave, set the timer to try again later.
        if ( pFifoHead )
        {
            BOMX_INPUT_MSG(("Data still pending in RAVE.  Starting Timer."));
            m_playpumpTimerId = StartTimer(B_FRAME_TIMER_INTERVAL, BOMX_AudioDecoder_PlaypumpEvent, static_cast<void *>(this));
        }
    }
}

void BOMX_AudioDecoder::InputBufferNew()
{
    ALOG_ASSERT(m_AvailInputBuffers > 0);
    --m_AvailInputBuffers;
    if (m_AvailInputBuffers == 0) {
        ALOGD_IF((m_logMask & B_LOG_ADEC_IN_RET), "%s: (%d) reached zero input buffers, minputBuffersTimerId:%p",
              __FUNCTION__, m_instanceNum, m_inputBuffersTimerId);
        CancelTimerId(m_inputBuffersTimerId);
        m_inputBuffersTimerId = StartTimer(B_INPUT_BUFFERS_RETURN_INTERVAL,
                                BOMX_AudioDecoder_InputBuffersTimer, static_cast<void *>(this));
    }
}

void BOMX_AudioDecoder::InputBufferReturned()
{
    unsigned totalBuffers = m_pAudioPorts[0]->GetDefinition()->nBufferCountActual;
    CancelTimerId(m_inputBuffersTimerId);
    ++m_AvailInputBuffers;
    ALOG_ASSERT(m_AvailInputBuffers <= totalBuffers);
}

void BOMX_AudioDecoder::InputBufferCounterReset()
{
    CancelTimerId(m_inputBuffersTimerId);
    m_AvailInputBuffers = m_pAudioPorts[0]->GetDefinition()->nBufferCountActual;
}

void BOMX_AudioDecoder::ReturnInputBuffers(OMX_TICKS decodeTs, bool causedByTimeout)
{
    uint32_t count = 0;
    BOMX_AudioDecoderInputBufferInfo *pInfo;
    BOMX_Buffer *pNextBuffer, *pReturnBuffer = NULL, *pBuffer = m_pAudioPorts[0]->GetBuffer();

    PlaypumpEvent();

    while ( pBuffer != NULL )
    {
        pInfo = (BOMX_AudioDecoderInputBufferInfo *)pBuffer->GetComponentPrivate();
        ALOG_ASSERT(NULL != pInfo);
        if ( !pInfo->complete )
        {
            ALOGV("Buffer:%p not completed yet", pBuffer);
            break;
        }

        if ( causedByTimeout || pReturnBuffer == NULL )
        {
            pReturnBuffer = pBuffer;
        }
        else if ( *pBuffer->GetTimestamp() == decodeTs )
        {
            pReturnBuffer = pBuffer;
            break;
        }

        pBuffer = m_pAudioPorts[0]->GetNextBuffer(pBuffer);
    }

    if ( pReturnBuffer != NULL )
    {
        pBuffer = m_pAudioPorts[0]->GetBuffer();
        while ( pBuffer != pReturnBuffer )
        {
            pNextBuffer = m_pAudioPorts[0]->GetNextBuffer(pBuffer);

            // If the buffer has codec config flags it shouldn't be added
            // to count. These buffers don't have a corresponding output frame
            // so it's ok to return them as soon as possible.
            if ( ReturnInputPortBuffer(pBuffer) )
            {
                ++count;
            }

            pBuffer = pNextBuffer;
        }

        // The last returned buffer
        if ( ReturnInputPortBuffer(pBuffer) )
        {
            ++count;
        }
    }

    ALOGD_IF((m_logMask & B_LOG_ADEC_IN_RET), "%s %d: returned %u buffers, %u available", __FUNCTION__, m_instanceNum, count, m_AvailInputBuffers);
}

bool BOMX_AudioDecoder::ReturnInputPortBuffer(BOMX_Buffer *pBuffer)
{
    ReturnPortBuffer(m_pAudioPorts[0], pBuffer);
    InputBufferReturned();

    // If the buffer has codec config flags it shouldn't be added
    // to count. These buffers don't have a corresponding output frame
    // so it's ok to return them as soon as possible.
    OMX_BUFFERHEADERTYPE *pHeader = pBuffer->GetHeader();
    ALOG_ASSERT(pHeader != NULL);
    return (!(pHeader->nFlags & OMX_BUFFERFLAG_CODECCONFIG ));
}

void BOMX_AudioDecoder::InputBufferTimeoutCallback()
{
    CancelTimerId(m_inputBuffersTimerId);
    ALOGD_IF((m_logMask & B_LOG_ADEC_IN_RET), "%s: Run out of input buffers. Returning all completed buffers...", __FUNCTION__);
    ReturnInputBuffers(0, true);
}

void BOMX_AudioDecoder::OutputFrameEvent()
{
    // Check for new frames
    PollDecodedFrames();
}

void BOMX_AudioDecoder::FifoOverflowEvent()
{
    ALOGE("%s: the audio input FIFO overflows", __FUNCTION__);
}

void BOMX_AudioDecoder::FifoUnderflowEvent()
{
    ALOGE("%s: the audio input FIFO underflows", __FUNCTION__);
}

void BOMX_AudioDecoder::PlaypumpErrorCallbackEvent()
{
    ALOGE("%s: playpump detects an error in processing of the stream data", __FUNCTION__);
}

void BOMX_AudioDecoder::PlaypumpCcErrorEvent()
{
    ALOGE("%s: cc error, continuity counter of next packet does not have the next counter value", __FUNCTION__);
}

static const struct {
    const char *pName;
    int index;
} g_extensions[] =
{
    {"OMX.google.android.index.allocateNativeHandle", (int)OMX_IndexParamAllocNativeHandle},
    {NULL,0}
};

OMX_ERRORTYPE BOMX_AudioDecoder::GetExtensionIndex(
    OMX_IN  OMX_STRING cParameterName,
    OMX_OUT OMX_INDEXTYPE* pIndexType)
{
    int i;

    if ( NULL == cParameterName || NULL == pIndexType )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    ALOGV("Looking for extension %s", cParameterName);

    for ( i = 0; g_extensions[i].pName != NULL; i++ )
    {
        if ( !strcmp(g_extensions[i].pName, cParameterName) )
        {
            if ( !m_secureDecoder && (g_extensions[i].index == OMX_IndexParamAllocNativeHandle) )
            {
                ALOGD("Interface %s not supported in non secure decoder", g_extensions[i].pName);
                return OMX_ErrorUnsupportedIndex;
            }
            else
            {
                *pIndexType = (OMX_INDEXTYPE)g_extensions[i].index;
                return OMX_ErrorNone;
            }
        }
    }

    ALOGI("Extension %s is not supported", cParameterName);
    return OMX_ErrorUnsupportedIndex;
}

OMX_ERRORTYPE BOMX_AudioDecoder::GetConfig(
        OMX_IN  OMX_INDEXTYPE nIndex,
        OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    BSTD_UNUSED(pComponentConfigStructure);
    switch ( (int)nIndex )
    {
    case OMX_IndexConfigAndroidVendorExtension:
    {
        OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *pAndroidVendorExtensionType =
            (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)pComponentConfigStructure;
        BOMX_STRUCT_VALIDATE(pAndroidVendorExtensionType);
        ALOGV("GetConfig OMX_IndexConfigAndroidVendorExtension");
        pAndroidVendorExtensionType->nParamCount = 0;
        return OMX_ErrorNoMore;
    }
    default:
        ALOGW("Config index %#x is not supported", nIndex);
        return BOMX_ERR_TRACE(OMX_ErrorUnsupportedIndex);
    }
}

OMX_ERRORTYPE BOMX_AudioDecoder::SetConfig(
        OMX_IN  OMX_INDEXTYPE nIndex,
        OMX_IN  OMX_PTR pComponentConfigStructure)
{
    BSTD_UNUSED(pComponentConfigStructure);
    switch ( (int)nIndex )
    {
    case OMX_IndexConfigAndroidVendorExtension:
    {
        OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *pAndroidVendorExtensionType =
            (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *)pComponentConfigStructure;
        BOMX_STRUCT_VALIDATE(pAndroidVendorExtensionType);
        ALOGV("SetConfig OMX_IndexConfigAndroidVendorExtension");
        ALOGW("ConfigAndroidVendorExtension: ignoring...");
        return OMX_ErrorUnsupportedIndex;
    }
    default:
        ALOGW("Config index %#x is not supported", nIndex);
        return BOMX_ERR_TRACE(OMX_ErrorUnsupportedIndex);
    }
}

void BOMX_AudioDecoder::AddOutputBuffers()
{
    BOMX_Buffer *pBuffer;

    for ( pBuffer = m_pAudioPorts[1]->GetBuffer();
          NULL != pBuffer;
          pBuffer = m_pAudioPorts[1]->GetNextBuffer(pBuffer) )
    {
        BOMX_AudioDecoderOutputBufferInfo *pInfo;
        pInfo = (BOMX_AudioDecoderOutputBufferInfo *)pBuffer->GetComponentPrivate();
        if ( !pInfo->nexusOwned )
        {
            NEXUS_Error errCode;
            OMX_BUFFERHEADERTYPE *pBufferHeader = pBuffer->GetHeader();
            ALOG_ASSERT(NULL != pBufferHeader);
            errCode = NEXUS_AudioDecoder_QueueBuffer(m_hAudioDecoder, pInfo->hMemoryBlock, 0, pBufferHeader->nAllocLen);
            if ( errCode )
            {
                (void)BOMX_ERR_TRACE(OMX_ErrorUndefined);
                return;
            }
            pInfo->nexusOwned = true;
        }
    }
}

static bool FindBufferFromBlock(BOMX_Buffer *pBuffer, void *pData)
{
    BOMX_AudioDecoderOutputBufferInfo *pInfo;
    NEXUS_MemoryBlockHandle hBlock = (NEXUS_MemoryBlockHandle)pData;

    ALOG_ASSERT(NULL != pBuffer);
    ALOG_ASSERT(NULL != hBlock);

    pInfo = (BOMX_AudioDecoderOutputBufferInfo *)pBuffer->GetComponentPrivate();
    ALOG_ASSERT(NULL != pInfo);

    return (pInfo->hMemoryBlock == hBlock) ? true : false;
}

void BOMX_AudioDecoder::RemoveOutputBuffers()
{
    NEXUS_Error errCode;
    unsigned numFrames=0;
    unsigned i;

    errCode = NEXUS_AudioDecoder_GetDecodedFrames(m_hAudioDecoder, m_pMemoryBlocks, m_pFrameStatus, m_pAudioPorts[1]->GetDefinition()->nBufferCountActual, &numFrames);
    if ( errCode )
    {
        (void)BOMX_BERR_TRACE(errCode);
        return;
    }

    ALOGV("%d Removing %u output buffers from nexus", m_instanceNum, numFrames);

    for ( i = 0; i < numFrames; i++ )
    {
        BOMX_Buffer *pBuffer = m_pAudioPorts[1]->FindBuffer(FindBufferFromBlock, (void *)m_pMemoryBlocks[i]);
        if ( NULL != pBuffer )
        {
            BOMX_AudioDecoderOutputBufferInfo *pInfo;
            pInfo = (BOMX_AudioDecoderOutputBufferInfo *)pBuffer->GetComponentPrivate();
            ALOG_ASSERT(NULL != pInfo);
            pInfo->nexusOwned = false;
        }
    }

    if ( numFrames > 0 )
    {
        NEXUS_AudioDecoder_ConsumeDecodedFrames(m_hAudioDecoder, numFrames);
    }
}

void BOMX_AudioDecoder::PollDecodedFrames()
{
    NEXUS_Error errCode;
    unsigned numFrames=0;
    unsigned i;

    if ( m_formatChangePending || m_eosDelivered )
    {
        return;
    }

    // Check if we have client buffers ready to be delivered
    if ( m_pAudioPorts[1]->QueueDepth() > 0 )
    {
        int numSample;
        BOMX_Buffer *pBuffer;
        OMX_BUFFERHEADERTYPE *pHeader;
        errCode = NEXUS_AudioDecoder_GetDecodedFrames(m_hAudioDecoder, m_pMemoryBlocks, m_pFrameStatus, m_pAudioPorts[1]->GetDefinition()->nBufferCountActual, &numFrames);
        if ( errCode )
        {
            (void)BOMX_BERR_TRACE(errCode);
            return;
        }
        ALOGV("%d Decoder has %u frames ready", m_instanceNum, numFrames);

        // Scan buffers returned for matches in OMX buffers.  These should really be in order.
        for ( i = 0; i < numFrames; i++ )
        {
            ALOGV("%d Frame %u: pts=%u bytes=%zu@%u sr=%u", m_instanceNum, i, m_pFrameStatus[i].pts, m_pFrameStatus[i].filledBytes, m_pFrameStatus[i].bufferOffset, m_pFrameStatus[i].sampleRate);

            if ( m_pFrameStatus[i].sampleRate != m_sampleRate && m_pFrameStatus[i].filledBytes > 0 )
            {
                ALOGI("Sample rate change %u->%u", m_sampleRate, m_pFrameStatus[i].sampleRate);
                m_formatChangePending = true;
                m_sampleRate = m_pFrameStatus[i].sampleRate;
                PortFormatChanged(m_pAudioPorts[1]);
                return;
            }

            pBuffer = m_pAudioPorts[1]->FindBuffer(FindBufferFromBlock, (void *)m_pMemoryBlocks[i]);
            if ( NULL != pBuffer )
            {
                BOMX_AudioDecoderOutputBufferInfo *pInfo;
                pInfo = (BOMX_AudioDecoderOutputBufferInfo *)pBuffer->GetComponentPrivate();
                ALOG_ASSERT(NULL != pInfo);
                pInfo->nexusOwned = false;

                pHeader = pBuffer->GetHeader();
                pHeader->nOffset = 0;

                if ( !m_pBufferTracker->Remove(m_pFrameStatus[i].pts, pHeader) )
                {
                    ALOGW("Unable to find tracker entry for pts %#x", m_pFrameStatus[i].pts);
                    pHeader->nFlags = 0;
                    BOMX_PtsToTick(m_pFrameStatus[i].pts, &pHeader->nTimeStamp);
                }
                if ( (pHeader->nFlags & OMX_BUFFERFLAG_EOS) && !m_pBufferTracker->Last(pHeader->nTimeStamp) )
                {
                    // Defer EOS until the last output frame (with the latest timestamp) is returned [should not happen in audio]
                    m_eosReceived = true;
                    pHeader->nFlags &= ~OMX_BUFFERFLAG_EOS;
                }
                else if ( m_eosReceived && m_pBufferTracker->Last(pHeader->nTimeStamp) )
                {
                    m_eosReceived = false;
                    pHeader->nFlags |= OMX_BUFFERFLAG_EOS;
                }

                size_t filledBytes = m_pFrameStatus[i].filledBytes;

                if ( filledBytes > 0 && NULL != pInfo->pClientMemory )
                {
                    NEXUS_FlushCache(pInfo->pNexusMemory, filledBytes);
                    switch ( m_numPcmChannels )
                    {
                    case 2:
                    case 6:
                    case 8:
                        /* The decoder natively supports these modes */
                        BKNI_Memcpy(pInfo->pClientMemory, pInfo->pNexusMemory, m_pFrameStatus[i].filledBytes);
                        break;
                    default:
                        /* Other conversions, copy each bunch of valid channels into output skipping invalid channels */
                        {
                            unsigned copyBytes = (m_bitsPerSample * m_numPcmChannels)/8;
                            unsigned actualChannels = m_numPcmChannels > 6 ? 8 : m_numPcmChannels > 2 ? 6 : 2; // Round up to the next valid 2/6/8 channels amount
                            unsigned srcSkipBytes = (m_bitsPerSample * actualChannels)/8;
                            unsigned bytesToFill = (m_pFrameStatus[i].filledBytes * m_numPcmChannels) / actualChannels;
                            uint8_t *pDest = (uint8_t *)(pInfo->pClientMemory);
                            uint8_t *pSrc = (uint8_t *)(pInfo->pNexusMemory);

                            for ( filledBytes = 0; filledBytes < bytesToFill; filledBytes += copyBytes, pSrc += srcSkipBytes, pDest += copyBytes )
                            {
                                BKNI_Memcpy(pDest, pSrc, copyBytes);
                            }
                        }
                        break;
                    }
                }

                if ( filledBytes > 0 )
                {
                    int delaySamples = GetCodecDelay();

                    delaySamples -= m_codecDelayAdjusted;

                    if ( delaySamples > 0 )
                    {
                        int frameSamples = filledBytes / ((m_bitsPerSample/8) * m_numPcmChannels);

                        if ( delaySamples > frameSamples )
                        {
                            delaySamples = frameSamples;
                        }

                        pHeader->nOffset = delaySamples * (m_bitsPerSample/8) * m_numPcmChannels;
                        if ( pHeader->nOffset > filledBytes )
                        {
                            pHeader->nOffset = filledBytes;
                        }
                        m_codecDelayAdjusted += delaySamples;
                    }
                }

                pHeader->nFilledLen = filledBytes - pHeader->nOffset;
                if ( NULL != m_pOutputFile && pHeader->nFilledLen > 0 )
                {
                    fwrite(pHeader->pBuffer + pHeader->nOffset, pHeader->nFilledLen, 1, m_pOutputFile);
                }

                if ( pHeader->nFlags & OMX_BUFFERFLAG_EOS )
                {
                    ALOGV("EOS frame received");
                    if ( m_eosPending && !m_eosReady )
                    {
                        ALOG_ASSERT(!m_eosStandalone);

                        // Defer EOS for the next zero padded frame
                        pHeader->nFlags &= ~OMX_BUFFERFLAG_EOS;
                        m_eosReady = true;
                    }
                    else
                    {
                        // Fatal error - we did something wrong.
                        ALOGW("Additional frames received after EOS");
                        ALOG_ASSERT(true == m_eosPending);
                        return;
                    }
                }

                if ( pHeader->nTimeStamp != 0 )
                {
                    // Update the expected timestamp if the next frame is an EOS frame
                    numSample = pHeader->nFilledLen / (m_bitsPerSample / 8 * m_numPcmChannels);
                    ALOGV("m_eosTimeStamp %llu->%llu", m_eosTimeStamp, pHeader->nTimeStamp + (numSample * 1000000ll / m_pFrameStatus[i].sampleRate));
                    m_eosTimeStamp = pHeader->nTimeStamp + (numSample * 1000000ll / m_pFrameStatus[i].sampleRate);
                }

                ALOGD_IF((m_logMask & B_LOG_ADEC_OUTPUT), "%d Return output buffer TS %llu bytes %zu (%u@%u) flags 0x%x", m_instanceNum, pHeader->nTimeStamp, m_pFrameStatus[i].filledBytes, pHeader->nFilledLen, pHeader->nOffset, pHeader->nFlags);
                ReturnPortBuffer(m_pAudioPorts[1], pBuffer);
                // Try to return processed input buffers
                ReturnInputBuffers(pHeader->nTimeStamp, false);
                // Consume this from the decoder's queue
                NEXUS_AudioDecoder_ConsumeDecodedFrames(m_hAudioDecoder, 1);
            }
        }
        if ( m_eosPending && ( m_eosStandalone || m_eosReady ) )
        {
            if ( m_pAudioPorts[0]->QueueDepth() <= 1 &&
                 m_pAudioPorts[1]->QueueDepth() > 0 )
            {
                errCode = NEXUS_AudioDecoder_GetDecodedFrames(m_hAudioDecoder, m_pMemoryBlocks, m_pFrameStatus, m_pAudioPorts[1]->GetDefinition()->nBufferCountActual, &numFrames);
                if ( errCode )
                {
                    (void)BOMX_BERR_TRACE(errCode);
                    return;
                }
                // No frames left in decoder's output queue, send EOS
                if ( numFrames == 0 )
                {
                    int delaySamples = m_codecDelayAdjusted;
                    pBuffer = m_pAudioPorts[1]->GetBuffer();
                    pHeader = pBuffer->GetHeader();
                    if ( delaySamples > 0 )
                    {
                        pHeader->nFilledLen = delaySamples * (m_bitsPerSample/8) * m_numPcmChannels;
                        BKNI_Memset(pHeader->pBuffer, 0, pHeader->nFilledLen);
                        if ( NULL != m_pOutputFile )
                        {
                            fwrite(pHeader->pBuffer, pHeader->nFilledLen, 1, m_pOutputFile);
                        }
                    }
                    else
                    {
                        pHeader->nFilledLen = 0;
                    }
                    pHeader->nTimeStamp = m_eosTimeStamp;
                    pHeader->nFlags = OMX_BUFFERFLAG_EOS;
                    m_pAudioPorts[1]->BufferComplete(pBuffer);
                    m_eosPending = false;
                    m_eosStandalone = false;
                    m_eosReady = false;
                    m_eosDelivered = true;
                    m_eosTimeStamp = 0;
                    ALOGD_IF((m_logMask & B_LOG_ADEC_OUTPUT), "%d Return EOS output buffer %llu bytes %u", m_instanceNum, pHeader->nTimeStamp, pHeader->nFilledLen);
                    ReturnPortBuffer(m_pAudioPorts[1], pBuffer);
                    // Force remaining input buffers to be flushed
                    ReturnInputBuffers(0, true);
                    if ( m_callbacks.EventHandler )
                    {
                        (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate, OMX_EventBufferFlag, m_audioPortBase+1, pHeader->nFlags, NULL);
                    }
                }
            }
        }
    }
    else
    {
        ALOGV("No buffers on output port");
    }
}

OMX_ERRORTYPE BOMX_AudioDecoder::ConfigBufferInit()
{
    NEXUS_Error errCode;
    NEXUS_ClientConfiguration   clientConfig;
    NEXUS_MemoryAllocationSettings  memorySettings;

    if (m_pConfigBuffer == NULL)
    {
        NEXUS_Platform_GetClientConfiguration(&clientConfig);
        NEXUS_Memory_GetDefaultAllocationSettings(&memorySettings);
        memorySettings.heap = clientConfig.heap[NXCLIENT_FULL_HEAP];
        if (memorySettings.heap == NULL)
        {
           memorySettings.heap = clientConfig.heap[NXCLIENT_DEFAULT_HEAP];
        }
        errCode = NEXUS_Memory_Allocate(BOMX_AUDIO_CODEC_CONFIG_BUFFER_SIZE, &memorySettings, &m_pConfigBuffer);
        if ( errCode )
        {
            ALOGW("Unable to allocate codec config buffer");
            return BOMX_ERR_TRACE(OMX_ErrorUndefined);
        }
    }

    m_configBufferState = ConfigBufferState_eAccumulating;
    m_configBufferSize = 0;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_AudioDecoder::ConfigBufferAppend(const void *pBuffer, size_t length)
{
    ALOG_ASSERT(NULL != pBuffer);
    if ( m_configBufferSize + length > BOMX_AUDIO_CODEC_CONFIG_BUFFER_SIZE )
    {
        return BOMX_ERR_TRACE(OMX_ErrorOverflow);
    }
    BKNI_Memcpy(((uint8_t *)m_pConfigBuffer) + m_configBufferSize, pBuffer, length);
    m_configBufferSize += length;
    NEXUS_FlushCache(m_pConfigBuffer, m_configBufferSize);
    return OMX_ErrorNone;
}

NEXUS_Error BOMX_AudioDecoder::AllocateInputBuffer(uint32_t nSize, void*& pBuffer)
{
    NEXUS_ClientConfiguration               clientConfig;
    NEXUS_MemoryAllocationSettings          memorySettings;

    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    NEXUS_Memory_GetDefaultAllocationSettings(&memorySettings);
    memorySettings.heap = clientConfig.heap[NXCLIENT_FULL_HEAP];
    if (memorySettings.heap == NULL)
    {
       memorySettings.heap = clientConfig.heap[NXCLIENT_DEFAULT_HEAP];
    }
    return NEXUS_Memory_Allocate(nSize, &memorySettings, &pBuffer);
}

void BOMX_AudioDecoder::FreeInputBuffer(void*& pBuffer)
{
    NEXUS_Memory_Free(pBuffer);
    pBuffer = NULL;
}

NEXUS_Error BOMX_AudioDecoder::OpenPidChannel(uint32_t pid)
{
    if ( m_hPlaypump )
    {
        ALOG_ASSERT(NULL == m_hPidChannel);

        NEXUS_PlaypumpOpenPidChannelSettings pidSettings;
        NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&pidSettings);
        pidSettings.pidType = NEXUS_PidType_eAudio;
        m_hPidChannel = NEXUS_Playpump_OpenPidChannel(m_hPlaypump, pid, &pidSettings);
        if ( m_hPidChannel )
        {
            return NEXUS_SUCCESS;
        }
    }
    return BOMX_BERR_TRACE(BERR_UNKNOWN);
}

void BOMX_AudioDecoder::ClosePidChannel()
{
    if ( m_hPidChannel )
    {
        ALOG_ASSERT(NULL != m_hPlaypump);

        NEXUS_Playpump_ClosePidChannel(m_hPlaypump, m_hPidChannel);
        m_hPidChannel = NULL;
    }
}


int BOMX_AudioDecoder::GetCodecDelay()
{
    // Delay compensation for android media framework and cts compliance
    switch ( (int)GetCodec() )
    {
    case OMX_AUDIO_CodingMP3:
        return property_get_int32(BCM_RO_MEDIA_MP3_DELAY, 529);
    case OMX_AUDIO_CodingAAC:
        return property_get_int32(BCM_RO_MEDIA_AAC_DELAY, 0); // Our HW AAC decoders do not add delay like the SW decoder does
    default:
        return 0;
    }
}
