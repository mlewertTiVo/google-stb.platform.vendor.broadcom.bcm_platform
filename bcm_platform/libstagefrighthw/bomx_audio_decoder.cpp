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
 * $brcm_Workfile: $
 * $brcm_Revision: $
 * $brcm_Date: $
 *
 * Module Description:
 *
 * Revision History:
 *
 * $brcm_Log: $
 *
 *****************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "bomx_audio_decoder"

#include "bomx_audio_decoder.h"
#include "nexus_platform.h"
#include "nexus_simple_audio_decoder_server.h"
#include "OMX_AudioExt.h"
#include "OMX_IndexExt.h"

#define B_HEADER_BUFFER_SIZE (128)  // PES Header + BCMA packet if required
#define B_DATA_BUFFER_SIZE (8192)
#define B_NUM_BUFFERS ((1*1024*1024)/B_DATA_BUFFER_SIZE)
#define B_STREAM_ID 0xc0
#define B_MAX_FRAMES (16)

static const char *g_roles[] = {"audio_decoder.aac", "audio_decoder.mp3", "audio_decoder.mpeg", "audio_decoder.ac3", "audio_decoder.eac3"};
static const unsigned int g_numRoles = sizeof(g_roles)/sizeof(const char *);
static int g_roleCodec[] = {OMX_AUDIO_CodingAAC, OMX_AUDIO_CodingMP3, OMX_AUDIO_CodingMPEG, OMX_AUDIO_CodingAC3, OMX_AUDIO_CodingEAC3};

#define BOMX_AUDIO_EOS_TIMEOUT 64

// Uncomment this to enable output logging to a file
//#define DEBUG_FILE_OUTPUT 1

struct BOMX_AudioDecoderBufferInfo
{
    void *pHeader;
    void *pPayload;
};

#include <stdio.h>
static FILE *g_pDebugFile;

extern "C" OMX_ERRORTYPE BOMX_AudioDecoder_Create(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    BOMX_AudioDecoder *pAudioDecoder = new BOMX_AudioDecoder(pComponentTpe, pName, pAppData, pCallbacks);
    if ( NULL == pAudioDecoder )
    {
        return BOMX_ERR_TRACE(OMX_ErrorUndefined);
    }
    else
    {
        if ( pAudioDecoder->IsValid() )
        {
            #ifdef DEBUG_FILE_OUTPUT
            if ( NULL == g_pDebugFile )
            {
                g_pDebugFile = fopen("/data/videos/audio_decoder.debug.pes", "wb+");
            }
            #endif
            return OMX_ErrorNone;
        }
        else
        {
            delete pAudioDecoder;
            return BOMX_ERR_TRACE(OMX_ErrorUndefined);
        }
    }
}

extern "C" const char *BOMX_AudioDecoder_GetRole(unsigned roleIndex)
{
    if ( roleIndex >= g_numRoles )
    {
        return NULL;
    }
    else
    {
        return g_roles[roleIndex];
    }
}

static void BOMX_AudioDecoder_PlaypumpDataCallback(void *pParam, int param)
{
    B_EventHandle hEvent;
    BSTD_UNUSED(param);
    hEvent = static_cast <B_EventHandle> (pParam);
    BOMX_ASSERT(NULL != hEvent);
    B_Event_Set(hEvent);
    BOMX_MSG(("PlaypumpDataCallback"));
}

static void BOMX_AudioDecoder_PlaypumpEvent(void *pParam)
{
    BOMX_AudioDecoder *pDecoder = static_cast <BOMX_AudioDecoder *> (pParam);

    BOMX_MSG(("PlaypumpEvent"));

    pDecoder->PlaypumpEvent();
}

static void BOMX_AudioDecoder_EosTimer(void *pParam)
{
    BOMX_AudioDecoder *pDecoder = static_cast <BOMX_AudioDecoder *> (pParam);

    BOMX_MSG(("EOS Timer"));

    pDecoder->EosTimer();
}

static void BOMX_AudioDecoder_SourceChangedCallback(void *pParam, int param)
{
    B_EventHandle hEvent;
    BSTD_UNUSED(param);
    hEvent = static_cast <B_EventHandle> (pParam);
    BOMX_ASSERT(NULL != hEvent);
    B_Event_Set(hEvent);
    BOMX_MSG(("SourceChangedCallback"));
}

static void BOMX_AudioDecoder_SourceChangedEvent(void *pParam)
{
    BOMX_AudioDecoder *pDecoder = static_cast <BOMX_AudioDecoder *> (pParam);

    BOMX_MSG(("SourceChangedEvent"));

    pDecoder->SourceChangedEvent();
}

static OMX_ERRORTYPE BOMX_AudioDecoder_InitMimeType(OMX_AUDIO_CODINGTYPE eCompressionFormat, char *pMimeType)
{
    const char *pMimeTypeStr;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    BOMX_ASSERT(NULL != pMimeType);

    switch ( (int)eCompressionFormat ) // Typecasted to int to avoid warning about OMX_AUDIO_CodingVP8 not in enum
    {
    case OMX_AUDIO_CodingAAC:
        pMimeTypeStr = "audio/aac";
        break;
    case OMX_AUDIO_CodingMP3:
        pMimeTypeStr = "audio/mpeg";
        break;
    default:
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
    const OMX_CALLBACKTYPE *pCallbacks) : BOMX_Component(pComponentType, pName, pAppData, pCallbacks, BOMX_AudioDecoder_GetRole)
{
    unsigned i;
    #define MAX_PORT_FORMATS (2)

    OMX_AUDIO_PORTDEFINITIONTYPE portDefs;
    OMX_AUDIO_PARAM_PORTFORMATTYPE portFormats[MAX_PORT_FORMATS];

    memset(&portDefs, 0, sizeof(portDefs));
    portDefs.eEncoding = OMX_AUDIO_CodingAAC;
    portDefs.cMIMEType = m_inputMimeType;
    (void)BOMX_AudioDecoder_InitMimeType(portDefs.eEncoding, m_inputMimeType);
    for ( i = 0; i < MAX_PORT_FORMATS; i++ )
    {
        memset(&portFormats[i], 0, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
        BOMX_STRUCT_INIT(&portFormats[i]);
        portFormats[i].nIndex = i;
        switch ( i )
        {
        default:
        case 0:
            portFormats[i].eEncoding = OMX_AUDIO_CodingAAC; break;
        case 1:
            portFormats[i].eEncoding = OMX_AUDIO_CodingMP3; break;
        }
    }

    SetRole(g_roles[0]);
    m_audioPortBase = BOMX_COMPONENT_PORT_BASE(BOMX_ComponentId_eAudioDecoder, OMX_PortDomainAudio);
    m_numAudioPorts = 0;
    m_pAudioPorts[0] = new BOMX_AudioPort(m_audioPortBase, OMX_DirInput, B_NUM_BUFFERS, B_DATA_BUFFER_SIZE, false, 0, &portDefs, portFormats, MAX_PORT_FORMATS);
    if ( NULL == m_pAudioPorts[0] )
    {
        BOMX_ERR(("Unable to create audio input port"));
        this->Invalidate();
        return;
    }
    m_numAudioPorts = 1;
    portDefs.eEncoding = OMX_AUDIO_CodingPCM;
    strcpy(m_outputMimeType, "audio/wave");
    portDefs.cMIMEType = m_outputMimeType;
    memset(&portFormats[0], 0, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    BOMX_STRUCT_INIT(&portFormats[0]);
    portFormats[0].nIndex = 0;
    portFormats[0].eEncoding = OMX_AUDIO_CodingPCM;
    m_pAudioPorts[1] = new BOMX_AudioPort(m_audioPortBase+1, OMX_DirOutput, B_MAX_FRAMES, sizeof(NEXUS_StripedSurfaceHandle), false, 0, &portDefs, portFormats, 1);
    if ( NULL == m_pAudioPorts[1] )
    {
        BOMX_ERR(("Unable to create audio output port"));
        this->Invalidate();
        return;
    }
    m_numAudioPorts = 2;

    m_hPlaypumpEvent = B_Event_Create(NULL);
    if ( NULL == m_hPlaypumpEvent )
    {
        BOMX_ERR(("Unable to create playpump event"));
        this->Invalidate();
        return;
    }

    m_hSourceChangedEvent = B_Event_Create(NULL);
    if ( NULL == m_hSourceChangedEvent )
    {
        BOMX_ERR(("Unable to create source changed event"));
        this->Invalidate();
        return;
    }

    m_sourceChangedEventId = this->RegisterEvent(m_hSourceChangedEvent, BOMX_AudioDecoder_SourceChangedEvent, static_cast <void *> (this));
    if ( NULL == m_sourceChangedEventId )
    {
        BOMX_ERR(("Unable to register source changed event"));
        this->Invalidate();
        return;
    }

    BOMX_STRUCT_INIT(&m_aacParams);
    m_aacParams.nPortIndex = m_audioPortBase;
    m_aacParams.nChannels = 2;
    m_aacParams.nSampleRate = 0;
    m_aacParams.nBitRate = 0;
    m_aacParams.nAudioBandWidth = 0;
    m_aacParams.nFrameLength = 0;
    m_aacParams.nAACtools = OMX_AUDIO_AACToolNone;
    m_aacParams.nAACERtools = OMX_AUDIO_AACERNone;
    m_aacParams.eAACProfile = OMX_AUDIO_AACObjectHE_PS;
    m_aacParams.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4ADTS;
    m_aacParams.eChannelMode = OMX_AUDIO_ChannelModeStereo;

    BOMX_STRUCT_INIT(&m_mp3Params);
    m_mp3Params.nPortIndex = m_audioPortBase;
    m_mp3Params.nChannels = 2;
    m_mp3Params.nSampleRate = 0;
    m_mp3Params.nBitRate = 0;
    m_mp3Params.nAudioBandWidth = 0;
    m_mp3Params.eChannelMode = OMX_AUDIO_ChannelModeStereo;
    m_mp3Params.eFormat = OMX_AUDIO_MP3StreamFormatMP1Layer3;
}

BOMX_AudioDecoder::~BOMX_AudioDecoder()
{
    unsigned i;
    if ( m_sourceChangedEventId )
    {
        UnregisterEvent(m_sourceChangedEventId);
    }
    if ( m_hSourceChangedEvent )
    {
        B_Event_Destroy(m_hSourceChangedEvent);
    }
    if ( m_hPlaypumpEvent )
    {
        B_Event_Destroy(m_hPlaypumpEvent);
    }
    for ( i = 0; i < m_numAudioPorts; i++ )
    {
        if ( m_pAudioPorts[i] )
        {
            delete m_pAudioPorts[i];
        }
    }
}

OMX_ERRORTYPE BOMX_AudioDecoder::GetParameter(
        OMX_IN  OMX_INDEXTYPE nParamIndex,
        OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    switch ( nParamIndex )
    {
    case OMX_IndexParamAudioAac:
        {
            OMX_AUDIO_PARAM_AACPROFILETYPE *pAac = (OMX_AUDIO_PARAM_AACPROFILETYPE *)pComponentParameterStructure;
            BOMX_STRUCT_VALIDATE(pAac);
            if ( pAac->nPortIndex != m_audioPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            *pAac = m_aacParams;
            if ( m_hSimpleAudioDecoder )
            {
                NEXUS_AudioDecoderStatus decoderStatus;
                NEXUS_SimpleAudioDecoder_GetStatus(m_hSimpleAudioDecoder, &decoderStatus);
                pAac->nSampleRate = decoderStatus.sampleRate;
                switch ( decoderStatus.codec )
                {
                case NEXUS_AudioCodec_eAacAdts:
                case NEXUS_AudioCodec_eAacPlusAdts:
                case NEXUS_AudioCodec_eAacLoas:
                case NEXUS_AudioCodec_eAacPlusLoas:
                    pAac->nBitRate = decoderStatus.codecStatus.aac.bitrate;
                    if ( decoderStatus.codecStatus.aac.profile == NEXUS_AudioAacProfile_eLowComplexity )
                    {
                        pAac->eAACProfile = OMX_AUDIO_AACObjectLC;
                    }
                    else
                    {
                        pAac->eAACProfile = OMX_AUDIO_AACObjectHE;
                    }
                    switch ( decoderStatus.codecStatus.aac.acmod )
                    {
                    case NEXUS_AudioAacAcmod_eTwoMono_1_ch1_ch2:
                        pAac->eChannelMode = OMX_AUDIO_ChannelModeDual;
                        break;
                    case NEXUS_AudioAacAcmod_eOneCenter_1_0_C:
                        pAac->eChannelMode = OMX_AUDIO_ChannelModeMono;
                        break;
                    default:
                        pAac->eChannelMode = OMX_AUDIO_ChannelModeStereo;
                        break;
                    }
                    break;
                default:
                    /* Not AAC */
                    return OMX_ErrorNone;
                }
                // No need to return format here.  The codec is set according to the incoming stream format setting.
            }
            return OMX_ErrorNone;
        }
    case OMX_IndexParamAudioMp3:
        {
            OMX_AUDIO_PARAM_MP3TYPE *pMp3 = (OMX_AUDIO_PARAM_MP3TYPE *)pComponentParameterStructure;
            BOMX_STRUCT_VALIDATE(pMp3);
            if ( pMp3->nPortIndex != m_audioPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            *pMp3 = m_mp3Params;
            if ( m_hSimpleAudioDecoder )
            {
                NEXUS_AudioDecoderStatus decoderStatus;
                NEXUS_SimpleAudioDecoder_GetStatus(m_hSimpleAudioDecoder, &decoderStatus);
                pMp3->nSampleRate = decoderStatus.sampleRate;
                pMp3->nBitRate = decoderStatus.codecStatus.mpeg.bitrate;
                switch ( decoderStatus.codecStatus.mpeg.channelMode )
                {
                case NEXUS_AudioMpegChannelMode_eSingleChannel:
                    pMp3->nChannels = 1;
                    pMp3->eChannelMode = OMX_AUDIO_ChannelModeMono;
                    break;
                case NEXUS_AudioMpegChannelMode_eDualChannel:
                    pMp3->nChannels = 2;
                    pMp3->eChannelMode = OMX_AUDIO_ChannelModeDual;
                    break;
                case NEXUS_AudioMpegChannelMode_eIntensityStereo:
                    pMp3->nChannels = 2;
                    pMp3->eChannelMode = OMX_AUDIO_ChannelModeStereo;
                    break;
                default:
                    pMp3->nChannels = 2;
                    pMp3->eChannelMode = OMX_AUDIO_ChannelModeStereo;
                    break;
                }
            }
            return OMX_ErrorNone;
        }
    case OMX_IndexParamAudioPcm:
        {
            int i;
            OMX_AUDIO_PARAM_PCMMODETYPE *pPcm = (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure;
            BOMX_STRUCT_VALIDATE(pPcm);
            if ( pPcm->nPortIndex != (m_audioPortBase+1) )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            // Set Defaults
            pPcm->nChannels = 2;
            pPcm->eNumData = OMX_NumericalDataSigned;
#if BSTD_CPU_ENDIAN == BSTD_ENDIAN_LITTLE
            pPcm->eEndian = OMX_EndianLittle;
#else
            pPcm->eEndian = OMX_EndianBig;
#endif
            pPcm->bInterleaved = m_pAudioPorts[1]->IsTunneled() ? OMX_FALSE : OMX_TRUE;
            pPcm->nBitPerSample = m_pAudioPorts[1]->IsTunneled() ? 32 : 16;
            pPcm->nSamplingRate = 0;
            pPcm->eChannelMapping[0] = OMX_AUDIO_ChannelLF;
            pPcm->eChannelMapping[1] = OMX_AUDIO_ChannelRF;
            for ( i = 2; i < OMX_AUDIO_MAXCHANNELS; i++ )
            {
                pPcm->eChannelMapping[i] = OMX_AUDIO_ChannelNone;
            }
            if ( m_hSimpleAudioDecoder )
            {
                NEXUS_AudioDecoderStatus decoderStatus;
                NEXUS_SimpleAudioDecoder_GetStatus(m_hSimpleAudioDecoder, &decoderStatus);
                pPcm->nSamplingRate = decoderStatus.sampleRate;
            }
            return OMX_ErrorNone;
        }
    default:
        return BOMX_ERR_TRACE(BOMX_Component::GetParameter(nParamIndex, pComponentParameterStructure));
    }
}

OMX_ERRORTYPE BOMX_AudioDecoder::SetParameter(
        OMX_IN  OMX_INDEXTYPE nIndex,
        OMX_IN  OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE err;
    switch ( nIndex )
    {
    case OMX_IndexParamStandardComponentRole:
        {
            OMX_PARAM_COMPONENTROLETYPE *pRole = (OMX_PARAM_COMPONENTROLETYPE *)pComponentParameterStructure;
            unsigned i;
            BOMX_STRUCT_VALIDATE(pRole);
            // The spec says this should reset the component to default setings for the role specified.
            // It is technically redundant for this component, changing the input codec has the same
            // effect - but this will also set the input codec accordingly.
            for ( i = 0; i < g_numRoles; i++ )
            {
                if ( !strcmp(g_roles[i], (const char *)pRole->cRole) )
                {
                    // Set input port to matching compression std.
                    OMX_AUDIO_PARAM_PORTFORMATTYPE format;
                    memset(&format, 0, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
                    BOMX_STRUCT_INIT(&format);
                    format.nPortIndex = m_audioPortBase;
                    format.eEncoding = (OMX_AUDIO_CODINGTYPE)g_roleCodec[i];
                    err = SetParameter(OMX_IndexParamAudioPortFormat, &format);
                    if ( err != OMX_ErrorNone )
                    {
                        return BOMX_ERR_TRACE(err);
                    }
                    SetRole((const char *)pRole->cRole);
                    return OMX_ErrorNone;
                }
            }
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
    case OMX_IndexParamAudioPortFormat:
        {
            OMX_AUDIO_PARAM_PORTFORMATTYPE *pFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
            BOMX_STRUCT_VALIDATE(pFormat);
            if ( pFormat->nPortIndex != m_audioPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            BOMX_AudioPort *pPort = m_pAudioPorts[0];
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
            err = pPort->SetPortFormat(pFormat, &portDefs);
            if ( err )
            {
                return BOMX_ERR_TRACE(err);
            }
            return OMX_ErrorNone;
        }
    case OMX_IndexParamPortDefinition:
    {
        OMX_PARAM_PORTDEFINITIONTYPE *pDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pDef);
        if ( pDef->nPortIndex == m_audioPortBase && pDef->format.audio.cMIMEType != m_inputMimeType )
        {
            BOMX_ERR(("Audio input MIME type cannot be changed in the application"));
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        else if ( pDef->nPortIndex == (m_audioPortBase+1) && pDef->format.audio.cMIMEType != m_outputMimeType )
        {
            BOMX_ERR(("Audio output MIME type cannot be changed in the application"));
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        BOMX_Port *pPort = FindPortByIndex(pDef->nPortIndex);
        if ( NULL == pPort )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if ( pDef->nPortIndex == m_audioPortBase && pDef->format.audio.eEncoding != GetCodec() )
        {
            BOMX_ERR(("Audio compression format cannot be changed in the port defintion.  Change Port Format instead."));
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        // Handle remainder in base class
        return BOMX_ERR_TRACE(BOMX_Component::SetParameter(nIndex, (OMX_PTR)pDef));
    }
    case OMX_IndexParamAudioAac:
        {
            OMX_AUDIO_PARAM_AACPROFILETYPE *pAac = (OMX_AUDIO_PARAM_AACPROFILETYPE *)pComponentParameterStructure;
            BOMX_STRUCT_VALIDATE(pAac);
            if ( pAac->nPortIndex != m_audioPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            switch ( pAac->eAACStreamFormat )
            {
            case OMX_AUDIO_AACStreamFormatMP2ADTS:
            case OMX_AUDIO_AACStreamFormatMP4ADTS:
            case OMX_AUDIO_AACStreamFormatMP4LOAS:
                break;
            default:
                BOMX_ERR(("AAC stream format %u is not supported.  Only ADTS/LOAS formats are supported.", pAac->eAACStreamFormat));
                return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
            }
            m_aacParams = *pAac;
            return OMX_ErrorNone;
        }
    case OMX_IndexParamAudioMp3:
        {
            OMX_AUDIO_PARAM_MP3TYPE *pMp3 = (OMX_AUDIO_PARAM_MP3TYPE *)pComponentParameterStructure;
            BOMX_STRUCT_VALIDATE(pMp3);
            if ( pMp3->nPortIndex != m_audioPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            m_mp3Params = *pMp3;
            return OMX_ErrorNone;
        }
    case OMX_IndexParamAudioPcm:
        {
            // Ignore PCM settings - the output settings are fixed
            return OMX_ErrorNone;
        }
    default:
        return BOMX_ERR_TRACE(BOMX_Component::SetParameter(nIndex, pComponentParameterStructure));
    }
}

NEXUS_AudioCodec BOMX_AudioDecoder::GetNexusCodec()
{
    switch ( (int)GetCodec() )
    {

    case OMX_AUDIO_CodingAAC:
        switch ( m_aacParams.eAACStreamFormat )
        {
        case OMX_AUDIO_AACStreamFormatMP2ADTS:
        case OMX_AUDIO_AACStreamFormatMP4ADTS:
            return NEXUS_AudioCodec_eAacPlusAdts;
        case OMX_AUDIO_AACStreamFormatMP4LOAS:
            return NEXUS_AudioCodec_eAacPlusLoas;
        default:
            BOMX_ERR(("Unsupported AAC audio format.  Only ADTS/LOAS are supported."));
            return NEXUS_AudioCodec_eUnknown;
        }
    case OMX_AUDIO_CodingMP3:
    case OMX_AUDIO_CodingMPEG:
        return NEXUS_AudioCodec_eMp3;
    case OMX_AUDIO_CodingAC3:
        return NEXUS_AudioCodec_eAc3;
    case OMX_AUDIO_CodingEAC3:
        return NEXUS_AudioCodec_eAc3Plus;
    default:
        BOMX_ERR(("Unsupported audio codec"));
        return NEXUS_AudioCodec_eUnknown;
    }
}

NEXUS_Error BOMX_AudioDecoder::SetNexusState(OMX_STATETYPE state)
{
    BOMX_MSG(("Setting Nexus State to %s", BOMX_StateName(state)));
    if ( state == OMX_StateLoaded )
    {
        if ( m_hSimpleAudioDecoder )
        {
            NEXUS_SimpleAudioDecoder_Stop(m_hSimpleAudioDecoder);
            (void)NEXUS_SimpleAudioDecoder_SetStcChannel(m_hSimpleAudioDecoder, NULL);
            // TODO: Figure this out
            //NEXUS_SimpleAudioDecoder_Release(m_hSimpleAudioDecoder);
            //m_hSimpleAudioDecoder = NULL;
        }
        if ( m_hPlaypump )
        {
            NEXUS_Playpump_Stop(m_hPlaypump);
            if ( m_hPidChannel )
            {
                NEXUS_Playpump_ClosePidChannel(m_hPlaypump, m_hPidChannel);
                m_hPidChannel = NULL;
            }
            if ( m_playpumpEventId )
            {
                UnregisterEvent(m_playpumpEventId);
                m_playpumpEventId = NULL;
            }
            NEXUS_Playpump_Close(m_hPlaypump);
            m_hPlaypump = NULL;
        }
    }
    else
    {
        NEXUS_AudioDecoderStatus adecStatus;
        NEXUS_SimpleAudioDecoderStartSettings adecStartSettings;
        NEXUS_AudioDecoderTrickState adecTrickState;
        NEXUS_Error errCode;

        // All states other than loaded require a playpump handle
        if ( NULL == m_hPlaypump )
        {
            NEXUS_PlaypumpOpenSettings playpumpOpenSettings;
            NEXUS_PlaypumpSettings playpumpSettings;

            NEXUS_Playpump_GetDefaultOpenSettings(&playpumpOpenSettings);
            playpumpOpenSettings.fifoSize = 0;
            playpumpOpenSettings.dataNotCpuAccessible = true;
            playpumpOpenSettings.numDescriptors = 2*m_pAudioPorts[0]->GetDefinition()->nBufferCountActual;
            m_hPlaypump = NEXUS_Playpump_Open(NEXUS_ANY_ID, &playpumpOpenSettings);
            if ( NULL == m_hPlaypump )
            {
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }
            NEXUS_Playpump_GetSettings(m_hPlaypump, &playpumpSettings);
#if 1
            playpumpSettings.transportType = NEXUS_TransportType_eMpeg2Pes;
#else
            playpumpSettings.transportType = NEXUS_TransportType_eEs;
#endif
            playpumpSettings.dataNotCpuAccessible = true;
            playpumpSettings.dataCallback.callback = BOMX_AudioDecoder_PlaypumpDataCallback;
            playpumpSettings.dataCallback.context = static_cast <void *> (m_hPlaypumpEvent);
            errCode = NEXUS_Playpump_SetSettings(m_hPlaypump, &playpumpSettings);
            if ( errCode )
            {
                NEXUS_Playpump_Close(m_hPlaypump);
                m_hPlaypump = NULL;
                return BOMX_BERR_TRACE(errCode);
            }

            NEXUS_PlaypumpOpenPidChannelSettings pidSettings;
            NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&pidSettings);
            pidSettings.pidType = NEXUS_PidType_eAudio;
            m_hPidChannel = NEXUS_Playpump_OpenPidChannel(m_hPlaypump, B_STREAM_ID, &pidSettings);
            if ( NULL == m_hPidChannel )
            {
                NEXUS_Playpump_Close(m_hPlaypump);
                m_hPlaypump = NULL;
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }

            m_playpumpEventId = RegisterEvent(m_hPlaypumpEvent, BOMX_AudioDecoder_PlaypumpEvent, static_cast <void *> (this));
            if ( NULL == m_playpumpEventId )
            {
                BOMX_ERR(("Unable to register playpump event"));
                NEXUS_Playpump_ClosePidChannel(m_hPlaypump, m_hPidChannel);
                m_hPidChannel = NULL;
                NEXUS_Playpump_Close(m_hPlaypump);
                m_hPlaypump = NULL;
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }
        }

        (void)NEXUS_SimpleAudioDecoder_GetStatus(m_hSimpleAudioDecoder, &adecStatus);
        switch ( state )
        {
        case OMX_StateIdle:
            if ( adecStatus.started )
            {
                NEXUS_SimpleAudioDecoder_Stop(m_hSimpleAudioDecoder);
                (void)NEXUS_SimpleAudioDecoder_SetStcChannel(m_hSimpleAudioDecoder, NULL);
                NEXUS_Playpump_Stop(m_hPlaypump);
            }
            break;
        case OMX_StateExecuting:
            if ( adecStatus.started )
            {
                NEXUS_SimpleAudioDecoder_GetTrickState(m_hSimpleAudioDecoder, &adecTrickState);
                adecTrickState.rate = NEXUS_NORMAL_DECODE_RATE;
                errCode = NEXUS_SimpleAudioDecoder_SetTrickState(m_hSimpleAudioDecoder, &adecTrickState);
                if ( errCode )
                {
                    return BOMX_BERR_TRACE(errCode);
                }
            }
            else
            {
                errCode = NEXUS_SimpleAudioDecoder_SetStcChannel(m_hSimpleAudioDecoder, GetStcChannel());
                if ( errCode )
                {
                    return BOMX_BERR_TRACE(errCode);
                }
                m_eosPending = false;
                NEXUS_SimpleAudioDecoder_GetDefaultStartSettings(&adecStartSettings);
                adecStartSettings.primary.stcChannel = NULL;
                adecStartSettings.primary.pidChannel = m_hPidChannel;
                adecStartSettings.primary.codec = GetNexusCodec();
                adecStartSettings.primary.targetSyncEnabled = false;    /* Disable target sync for EOS */
                errCode = NEXUS_SimpleAudioDecoder_Start(m_hSimpleAudioDecoder, &adecStartSettings);
                if ( errCode )
                {
                    (void)NEXUS_SimpleAudioDecoder_SetStcChannel(m_hSimpleAudioDecoder, NULL);
                    return BOMX_BERR_TRACE(errCode);
                }
                errCode = NEXUS_Playpump_Start(m_hPlaypump);
                if ( errCode )
                {
                    NEXUS_SimpleAudioDecoder_Stop(m_hSimpleAudioDecoder);
                    (void)NEXUS_SimpleAudioDecoder_SetStcChannel(m_hSimpleAudioDecoder, NULL);
                }
            }
            break;
        case OMX_StatePause:
            // TODO: Check idle->pause->executing
            NEXUS_SimpleAudioDecoder_GetTrickState(m_hSimpleAudioDecoder, &adecTrickState);
            adecTrickState.rate = 0;
            errCode = NEXUS_SimpleAudioDecoder_SetTrickState(m_hSimpleAudioDecoder, &adecTrickState);
            if ( errCode )
            {
                return BOMX_BERR_TRACE(errCode);
            }
            break;
        default:
            return BOMX_BERR_TRACE(BERR_NOT_SUPPORTED);
        }
    }
    return NEXUS_SUCCESS;
}

OMX_ERRORTYPE BOMX_AudioDecoder::CommandStateSet(
    OMX_STATETYPE newState,
    OMX_STATETYPE oldState)
{
    OMX_ERRORTYPE err;

    BOMX_MSG(("Begin State Change %s->%s", BOMX_StateName(oldState), BOMX_StateName(newState)));

    switch ( newState )
    {
    case OMX_StateLoaded:
    {
        // If we are returning to loaded, we need to first flush all enabled ports and return their buffers
        bool inputPortEnabled, outputPortEnabled;

        // Save port state
        inputPortEnabled = m_pAudioPorts[0]->IsEnabled();
        outputPortEnabled = m_pAudioPorts[1]->IsEnabled();

        // Stop nexus components if they're active
        (void)SetNexusState(OMX_StateIdle);

        // Disable all ports
        err = CommandPortDisable(OMX_ALL);
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }

        // Release nexus resources
        (void)SetNexusState(OMX_StateLoaded);

        ReleaseResources();

        // Now all resources have been released.  Reset port enable state
        if ( inputPortEnabled )
        {
            (void)m_pAudioPorts[0]->Enable();
        }
        if ( outputPortEnabled )
        {
            (void)m_pAudioPorts[1]->Enable();
        }
        BOMX_MSG(("End State Change %s->%s", BOMX_StateName(oldState), BOMX_StateName(newState)));
        return OMX_ErrorNone;
    }
    case OMX_StateIdle:
    {
        // Transitioning from Loaded->Idle requires us to allocate all required resources
        if ( oldState == OMX_StateLoaded )
        {
            NEXUS_Error errCode = AllocateResources();
            if ( errCode )
            {
                return BOMX_ERR_TRACE(OMX_ErrorUndefined);
            }
            m_hSimpleAudioDecoder = NEXUS_SimpleAudioDecoder_Acquire(m_nxClientAllocResults.simpleAudioDecoder.id);
            BOMX_WRN(("SimpleAudioDecoder %#x", m_hSimpleAudioDecoder));
            if ( NULL == m_hSimpleAudioDecoder )
            {
                ReleaseResources();
                return BOMX_ERR_TRACE(OMX_ErrorUndefined);
            }

            NEXUS_SimpleAudioDecoderSettings adecSettings;

            NEXUS_SimpleAudioDecoder_GetSettings(m_hSimpleAudioDecoder, &adecSettings);
            adecSettings.primary.sourceChanged.callback = BOMX_AudioDecoder_SourceChangedCallback;
            adecSettings.primary.sourceChanged.context = static_cast <void *> (m_hSourceChangedEvent);
            NEXUS_SimpleAudioDecoder_SetSettings(m_hSimpleAudioDecoder, &adecSettings);

            BOMX_MSG(("Waiting for port population..."));
            PortWaitBegin();
            // Now we need to wait for all enabled ports to become populated
            while ( (m_pAudioPorts[0]->IsEnabled() && !m_pAudioPorts[0]->IsPopulated()) ||
                    (m_pAudioPorts[1]->IsEnabled() && !m_pAudioPorts[1]->IsPopulated()) )
            {
                if ( PortWait() != B_ERROR_SUCCESS )
                {
                    BOMX_ERR(("Timeout waiting for ports to be populated"));
                    PortWaitEnd();
                    NEXUS_SimpleAudioDecoder_Release(m_hSimpleAudioDecoder);
                    m_hSimpleAudioDecoder = NULL;
                    ReleaseResources();
                    return BOMX_ERR_TRACE(OMX_ErrorTimeout);
                }
            }
            PortWaitEnd();
            BOMX_MSG(("Done waiting for port population"));

            if ( m_pAudioPorts[0]->IsPopulated() && m_pAudioPorts[1]->IsPopulated() )
            {
                errCode = SetNexusState(OMX_StateIdle);
                if ( errCode )
                {
                    // Return buffers to application
                    (void)CommandPortDisable(OMX_ALL);
                    m_pAudioPorts[0]->Enable();
                    m_pAudioPorts[1]->Enable();
                    NEXUS_SimpleAudioDecoder_Release(m_hSimpleAudioDecoder);
                    m_hSimpleAudioDecoder = NULL;
                    ReleaseResources();
                    return BOMX_ERR_TRACE(OMX_ErrorUndefined);
                }
            }
        }
        else
        {
            // Transitioning to idle from any state except loaded is equivalent to stop
            if ( m_pAudioPorts[0]->IsPopulated() && m_pAudioPorts[1]->IsPopulated() )
            {
                (void)SetNexusState(OMX_StateIdle);
            }
            (void)CommandFlush(OMX_ALL);
        }
        BOMX_MSG(("End State Change %s->%s", BOMX_StateName(oldState), BOMX_StateName(newState)));
        return OMX_ErrorNone;
    }
    case OMX_StateExecuting:
    case OMX_StatePause:
        if ( m_pAudioPorts[0]->IsPopulated() && m_pAudioPorts[1]->IsPopulated() )
        {
            NEXUS_Error errCode = SetNexusState(newState);
            if ( errCode )
            {
                return BOMX_ERR_TRACE(OMX_ErrorUndefined);
            }
        }
        BOMX_MSG(("End State Change %s->%s", BOMX_StateName(oldState), BOMX_StateName(newState)));
        return OMX_ErrorNone;
    default:
        BOMX_ERR(("Unsupported state %u", newState));
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
    }
}

OMX_ERRORTYPE BOMX_AudioDecoder::CommandFlush(
    OMX_U32 portIndex)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    // Handle case for OMX_ALL by calling flush on each port
    if ( portIndex == OMX_ALL )
    {
        BOMX_MSG(("Flushing all ports"));

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
            BOMX_ERR(("Invalid port %u", portIndex));
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        BOMX_MSG(("Flushing %s port", pPort->GetDir() == OMX_DirInput ? "input" : "output"));
        if ( portIndex == m_audioPortBase )
        {
            // Input port.
            if ( m_hPlaypump )
            {
                (void)NEXUS_Playpump_Flush(m_hPlaypump);
            }
            if ( m_hSimpleAudioDecoder )
            {
                NEXUS_SimpleAudioDecoder_Flush(m_hSimpleAudioDecoder);
            }
        }
        else
        {
            // Output port.
            if ( !pPort->IsTunneled() )
            {
                // Need to return all outstanding frames
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
        BOMX_MSG(("Enabling all ports"));

        err = CommandPortEnable(m_audioPortBase);
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }
        err = CommandPortEnable(m_audioPortBase+1);
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
            BOMX_ERR(("Invalid port %u", portIndex));
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if ( pPort->IsEnabled() )
        {
            BOMX_ERR(("Port %u is already enabled", portIndex));
            return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
        }
        BOMX_MSG(("Enabling %s port", pPort->GetDir() == OMX_DirInput ? "input" : "output"));
        err = pPort->Enable();
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }
        if ( StateGet() != OMX_StateLoaded )
        {
            // Wait for port to become populated
            BOMX_MSG(("Waiting for port to populate"));
            PortWaitBegin();
            // Now we need to wait for all enabled ports to become populated
            while ( !pPort->IsPopulated() )
            {
                if ( PortWait() != B_ERROR_SUCCESS )
                {
                    BOMX_ERR(("Timeout waiting for port %u to be populated", portIndex));
                    PortWaitEnd();
                    (void)pPort->Disable();
                    return BOMX_ERR_TRACE(OMX_ErrorTimeout);
                }
            }
            PortWaitEnd();
            BOMX_MSG(("Done waiting for port to populate"));

            // If all ports are enabled, time to do something
            if ( m_pAudioPorts[0]->IsPopulated() && m_pAudioPorts[1]->IsPopulated() )
            {
                NEXUS_Error errCode;

                errCode = SetNexusState(StateGet());
                {
                    if ( errCode )
                    {
                        (void)CommandPortDisable(portIndex);
                        return BOMX_ERR_TRACE(OMX_ErrorUndefined);
                    }
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
        BOMX_MSG(("Disabling all ports"));

        err = CommandPortDisable(m_audioPortBase);
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }
        err = CommandPortDisable(m_audioPortBase+1);
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
            BOMX_ERR(("Invalid port %u", portIndex));
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if ( pPort->IsDisabled() )
        {
            BOMX_ERR(("Port %u is already disabled", portIndex));
            return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
        }
        BOMX_MSG(("Disabling %s port", pPort->GetDir() == OMX_DirInput ? "input" : "output"));
        err = pPort->Disable();
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }
        if ( StateGet() != OMX_StateLoaded )
        {
            // Stop nexus components
            (void)SetNexusState(OMX_StateIdle);

            // Return port buffers to client
            ReturnPortBuffers(pPort);

            // Wait for port to become de-populated
            BOMX_MSG(("Waiting for port to de-populate"));
            PortWaitBegin();
            // Now we need to wait for all enabled ports to become populated
            while ( !pPort->IsEmpty() )
            {
                if ( PortWait() != B_ERROR_SUCCESS )
                {
                    BOMX_ERR(("Timeout waiting for port %u to be de-populated", portIndex));
                    PortWaitEnd();
                    (void)pPort->Enable();
                    return BOMX_ERR_TRACE(OMX_ErrorTimeout);
                }
            }
            PortWaitEnd();
            BOMX_MSG(("Done waiting for port to de-populate"));
        }
    }

    return err;
}

void BOMX_AudioDecoder::GetNxClientAllocSettings(NxClient_AllocSettings *pSettings)
{
    BOMX_ASSERT(NULL != pSettings);
    pSettings->simpleAudioDecoder = 1;
}

void BOMX_AudioDecoder::GetNxClientConnectSettings(NxClient_ConnectSettings *pSettings)
{
    BOMX_ASSERT(NULL != pSettings);
    pSettings->simpleAudioDecoder.id = m_nxClientAllocResults.simpleAudioDecoder.id;
}

OMX_ERRORTYPE BOMX_AudioDecoder::ComponentTunnelRequest(
    OMX_IN  OMX_U32 nPort,
    OMX_IN  OMX_HANDLETYPE hTunneledComp,
    OMX_IN  OMX_U32 nTunneledPort,
    OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
    BOMX_Port *pPort, *pTheirPort;
    BOMX_Component *pComp;
    BSTD_UNUSED(pTunnelSetup);

    pPort = FindPortByIndex(nPort);
    if ( NULL == pPort )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }
    if ( pPort->IsEnabled() && StateGet() != OMX_StateLoaded )
    {
        BOMX_ERR(("Cannot tunnel unless component is in loaded state or port is disabled"));
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
    }
    if ( pPort->GetDir() == OMX_DirInput )
    {
        BOMX_ERR(("Only output port supports tunneling"));
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }
    if ( NULL == hTunneledComp )
    {
        (void)pPort->SetTunnel(NULL, NULL);
    }
    else
    {
        // Check if ports are compatible
        pComp = BOMX_HANDLE_TO_COMPONENT(hTunneledComp);
        const char *pName = pComp->GetName();
        pTheirPort = pComp->FindPortByIndex(nTunneledPort);
        if ( NULL == pTheirPort )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if ( pPort->GetDir() == pTheirPort->GetDir() )
        {
            BOMX_ERR(("Two ports of the same direction can not be tunneled"));
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if ( !strcmp(pName, "OMX.broadcom.nxclient.audio_renderer") )
        {
            // Valid tunnel request
            OMX_ERRORTYPE err;
            err = pPort->SetTunnel(pComp, pTheirPort);
            if ( err != OMX_ErrorNone )
            {
                return BOMX_ERR_TRACE(err);
            }
        }
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_AudioDecoder::AddPortBuffer(
        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
        OMX_IN OMX_U32 nPortIndex,
        OMX_IN OMX_PTR pAppPrivate,
        OMX_IN OMX_U32 nSizeBytes,
        OMX_IN OMX_U8* pBuffer,
        bool componentAllocated)
{
    BOMX_Port *pPort;
    BOMX_AudioDecoderBufferInfo *pInfo;
    OMX_ERRORTYPE err;
    NEXUS_Error errCode;

    if ( NULL == ppBufferHdr )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    pPort = FindPortByIndex(nPortIndex);
    if ( NULL == pPort )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }

    if ( pPort->IsTunneled() )
    {
        BOMX_ERR(("Cannot add buffers to a tunneled port"));
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }

    if ( pPort->IsEnabled() && StateGet() != OMX_StateLoaded )
    {
        BOMX_ERR(("Can only add buffers to an enabled port while transitioning from loaded->idle (state=%u)", StateGet()));
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
    }

    pInfo = new BOMX_AudioDecoderBufferInfo;
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    errCode = NEXUS_Memory_Allocate(B_HEADER_BUFFER_SIZE, NULL, &pInfo->pHeader);
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
        errCode = NEXUS_Memory_Allocate(nSizeBytes, NULL, &pInfo->pPayload);
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


OMX_ERRORTYPE BOMX_AudioDecoder::UseBuffer(
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes,
    OMX_IN OMX_U8* pBuffer)
{
    OMX_ERRORTYPE err;

    // TODO: Handle output port

    if ( NULL == pBuffer )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    err = AddPortBuffer(ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes, pBuffer, false);
    if ( err != OMX_ErrorNone )
    {
        return BOMX_ERR_TRACE(err);
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
    void *pBuffer;

    // TODO: Handle output port

    if ( NULL == pBuffer )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    errCode = NEXUS_Memory_Allocate(nSizeBytes, NULL, &pBuffer);
    if ( errCode )
    {
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    err = AddPortBuffer(ppBuffer, nPortIndex, pAppPrivate, nSizeBytes, (OMX_U8*)pBuffer, true);
    if ( err != OMX_ErrorNone )
    {
        NEXUS_Memory_Free(pBuffer);
        return BOMX_ERR_TRACE(err);
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_AudioDecoder::FreeBuffer(
    OMX_IN  OMX_U32 nPortIndex,
    OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    BOMX_Port *pPort;
    BOMX_Buffer *pBuffer;
    BOMX_AudioDecoderBufferInfo *pInfo;
    OMX_ERRORTYPE err;

    // TODO: Handle output port

    pPort = FindPortByIndex(nPortIndex);
    if ( NULL == pPort )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }
    pBuffer = BOMX_BUFFERHEADER_TO_BUFFER(pBufferHeader);
    if ( NULL == pBufferHeader )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    pInfo = (BOMX_AudioDecoderBufferInfo *)pBuffer->GetComponentPrivate();
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
        NEXUS_Memory_Free(pInfo->pPayload);
    }
    delete pInfo;

    PortStatusChanged();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_AudioDecoder::EmptyThisBuffer(
    OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    OMX_ERRORTYPE err;
    BOMX_Buffer *pBuffer;
    BOMX_AudioDecoderBufferInfo *pInfo;
    size_t headerLen=0, payloadLen;
    void *pPayload;

    if ( NULL == pBufferHeader )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    pBuffer = BOMX_BUFFERHEADER_TO_BUFFER(pBufferHeader);
    if ( NULL == pBuffer || pBuffer->GetDir() != OMX_DirInput )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    if ( pBufferHeader->nInputPortIndex != m_audioPortBase )
    {
        BOMX_ERR(("Invalid buffer"));
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    pInfo = (BOMX_AudioDecoderBufferInfo *)pBuffer->GetComponentPrivate();
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    BOMX_MSG(("Empty %#x", pBufferHeader));

    // Form PES Header
    if ( BOMX_FormPesHeader(pBufferHeader, pInfo->pHeader, B_HEADER_BUFFER_SIZE, B_STREAM_ID, &headerLen) )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    if ( NULL != g_pDebugFile )
    {
        fwrite(pInfo->pHeader, 1, headerLen, g_pDebugFile);
    }
    NEXUS_FlushCache(pInfo->pHeader, headerLen);
    pPayload = pBufferHeader->pBuffer + pBufferHeader->nOffset;
    payloadLen = pBufferHeader->nFilledLen - pBufferHeader->nOffset;
    if ( NULL != pInfo->pPayload )
    {
        BKNI_Memcpy(pInfo->pPayload, (const void *)(pBufferHeader->pBuffer + pBufferHeader->nOffset), payloadLen);
        pPayload = pInfo->pPayload;
    }
    if ( NULL != g_pDebugFile )
    {
        fwrite(pPayload, 1, payloadLen, g_pDebugFile);
        fflush(g_pDebugFile);
    }
    NEXUS_FlushCache(pPayload, payloadLen);

    // Give buffers to transport
    NEXUS_PlaypumpScatterGatherDescriptor desc[2];
    NEXUS_Error errCode;
    size_t numConsumed;
    #if 1
    desc[0].addr = pInfo->pHeader;
    desc[0].length = headerLen;
    if ( payloadLen > 0 )
    {
        numConsumed=2;
        desc[1].addr = pPayload;
        desc[1].length = payloadLen;
    }
    else
    {
        BOMX_WRN(("Discarding empty payload"));
        numConsumed = 1;
    }
    #else
    numConsumed=1;
    desc[0].addr = pPayload;
    desc[0].length = payloadLen;
    #endif
    errCode = NEXUS_Playpump_SubmitScatterGatherDescriptor(m_hPlaypump, desc, numConsumed, &numConsumed);
    if ( errCode )
    {
        return BOMX_ERR_TRACE(OMX_ErrorUndefined);
    }
    err = m_pAudioPorts[0]->QueueBuffer(pBufferHeader);
    BOMX_ASSERT(err == OMX_ErrorNone);

    if ( pBufferHeader->nFlags & OMX_BUFFERFLAG_EOS )
    {
        m_eosPending = true;
        m_prevFifoDepth = (unsigned)-1;
        m_staticFifoDepthCount = 0;
        m_eosTimer = StartTimer(BOMX_AUDIO_EOS_TIMEOUT, BOMX_AudioDecoder_EosTimer, static_cast <void *> (this));
        BOMX_ASSERT(NULL != m_eosTimer);
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_AudioDecoder::FillThisBuffer(
    OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    BOMX_Buffer *pBuffer;

    if ( NULL == pBufferHeader )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    pBuffer = BOMX_BUFFERHEADER_TO_BUFFER(pBufferHeader);
    if ( NULL == pBuffer || pBuffer->GetDir() != OMX_DirOutput )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    // TODO: Handle non-tunneled
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

void BOMX_AudioDecoder::PlaypumpEvent()
{
    NEXUS_PlaypumpStatus playpumpStatus;
    unsigned numComplete=0, numPending, numQueued, i;

    do {
        NEXUS_Playpump_GetStatus(m_hPlaypump, &playpumpStatus);
        #if 1
        numPending = (playpumpStatus.descFifoDepth+1)/2;
        #else
        numPending = playpumpStatus.descFifoDepth;
        #endif
        numQueued = m_pAudioPorts[0]->QueueDepth();
        if ( numQueued > numPending )
        {
            numComplete = numQueued - numPending;
        }
        else
        {
            numComplete = 0;
        }

        if ( numComplete > 0 )
        {
            BOMX_MSG(("Returning %u buffers (%u queued, %u pending)", numComplete, numQueued, numPending));
        }
        for ( i = 0; i < numComplete; i++ )
        {
            BOMX_Buffer *pBuffer = m_pAudioPorts[0]->GetBuffer();
            BOMX_ASSERT(NULL != pBuffer);
            ReturnPortBuffer(m_pAudioPorts[0], pBuffer);
        }
    } while ( numComplete > 0 );
}

void BOMX_AudioDecoder::GetMediaTime(OMX_TICKS *pTicks)
{
    BOMX_ASSERT(NULL != pTicks);

    // Initialize
    BOMX_Component::GetMediaTime(pTicks);

    Lock();
    if ( NULL != m_hSimpleAudioDecoder )
    {
        NEXUS_AudioDecoderStatus status;
        NEXUS_SimpleAudioDecoder_GetStatus(m_hSimpleAudioDecoder, &status);
        if ( status.started )
        {
            BOMX_PtsToTick(status.pts, pTicks);
        }
    }
    Unlock();
}

NEXUS_SimpleStcChannelHandle BOMX_AudioDecoder::GetStcChannel()
{
    if ( m_pAudioPorts[1]->IsTunneled() )
    {
        return m_pAudioPorts[1]->GetTunnelComponent()->GetStcChannel();
    }
    return NULL;
}

void BOMX_AudioDecoder::SourceChangedEvent()
{
    NEXUS_AudioDecoderStatus adecStatus;

    if ( m_hSimpleAudioDecoder )
    {
        (void)NEXUS_SimpleAudioDecoder_GetStatus(m_hSimpleAudioDecoder, &adecStatus);
        if ( adecStatus.started && adecStatus.locked )
        {
            if ( m_callbacks.EventHandler )
            {
                OMX_EVENT_AUDIOINFODATA audioInfo;
                BKNI_Memset(&audioInfo, 0, sizeof(audioInfo));  // Init channel map and nChannels to 0
                audioInfo.eCodingType = GetCodec(); /* I assume this is source codec, not PCM output... */
                audioInfo.nSampleRate = adecStatus.sampleRate;
                switch ( (int)audioInfo.eCodingType )
                {
                case OMX_AUDIO_CodingAAC:
                    switch ( adecStatus.codecStatus.aac.acmod )
                    {
                    case NEXUS_AudioAacAcmod_eOneCenter_1_0_C:
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelCF] = OMX_AUDIO_ChannelCF;
                        audioInfo.nChannels = 1;
                        break;
                    case NEXUS_AudioAacAcmod_eTwoMono_1_ch1_ch2:
                    case NEXUS_AudioAacAcmod_eTwoChannel_2_0_L_R:
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelLF] = OMX_AUDIO_ChannelLF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelRF] = OMX_AUDIO_ChannelRF;
                        audioInfo.nChannels = 2;
                        break;
                    case NEXUS_AudioAacAcmod_eThreeChannel_3_0_L_C_R:
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelLF] = OMX_AUDIO_ChannelLF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelRF] = OMX_AUDIO_ChannelRF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelCF] = OMX_AUDIO_ChannelCF;
                        audioInfo.nChannels = 3;
                        break;
                    case NEXUS_AudioAacAcmod_eThreeChannel_2_1_L_R_S:
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelLF] = OMX_AUDIO_ChannelLF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelRF] = OMX_AUDIO_ChannelRF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelCS] = OMX_AUDIO_ChannelCS;
                        audioInfo.nChannels = 3;
                        break;
                    case NEXUS_AudioAacAcmod_eFourChannel_3_1_L_C_R_S:
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelLF] = OMX_AUDIO_ChannelLF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelRF] = OMX_AUDIO_ChannelRF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelCF] = OMX_AUDIO_ChannelCF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelCS] = OMX_AUDIO_ChannelCS;
                        audioInfo.nChannels = 4;
                        break;
                    case NEXUS_AudioAacAcmod_eFourChannel_2_2_L_R_SL_SR:
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelLF] = OMX_AUDIO_ChannelLF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelRF] = OMX_AUDIO_ChannelRF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelLS] = OMX_AUDIO_ChannelLS;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelRS] = OMX_AUDIO_ChannelRS;
                        audioInfo.nChannels = 4;
                        break;
                    case NEXUS_AudioAacAcmod_eFiveChannel_3_2_L_C_R_SL_SR:
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelLF] = OMX_AUDIO_ChannelLF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelRF] = OMX_AUDIO_ChannelRF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelCF] = OMX_AUDIO_ChannelCF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelLS] = OMX_AUDIO_ChannelLS;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelRS] = OMX_AUDIO_ChannelRS;
                        audioInfo.nChannels = 5;
                    default:
                        break;
                    }
                    if ( adecStatus.codecStatus.aac.lfe )
                    {
                        audioInfo.nChannels++;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelLFE] = OMX_AUDIO_ChannelLFE;
                    }
                    audioInfo.nBitPerSample = 16;
                    break;
                case OMX_AUDIO_CodingMP3:
                case OMX_AUDIO_CodingMPEG:
                    switch ( adecStatus.codecStatus.mpeg.channelMode )
                    {
                    case NEXUS_AudioMpegChannelMode_eSingleChannel:
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelCF] = OMX_AUDIO_ChannelCF;
                        audioInfo.nChannels = 1;
                        break;
                    default:
                    case NEXUS_AudioMpegChannelMode_eStereo:
                    case NEXUS_AudioMpegChannelMode_eIntensityStereo:
                    case NEXUS_AudioMpegChannelMode_eDualChannel:
                        audioInfo.nChannels = 2;
                        break;
                    }
                    audioInfo.nBitPerSample = 16;
                    audioInfo.eCodingType = (OMX_AUDIO_CODINGTYPE)(adecStatus.codecStatus.mpeg.layer == NEXUS_AudioMpegLayer_e3 ? (int)OMX_AUDIO_CodingMP3 : (int)OMX_AUDIO_CodingMPEG);
                    break;
                case OMX_AUDIO_CodingAC3:
                case OMX_AUDIO_CodingEAC3:
                    switch ( adecStatus.codecStatus.ac3.acmod )
                    {
                    case NEXUS_AudioAc3Acmod_eOneCenter_1_0_C:
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelCF] = OMX_AUDIO_ChannelCF;
                        audioInfo.nChannels = 1;
                        break;
                    case NEXUS_AudioAc3Acmod_eTwoMono_1_ch1_ch2:
                    case NEXUS_AudioAc3Acmod_eTwoChannel_2_0_L_R:
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelLF] = OMX_AUDIO_ChannelLF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelRF] = OMX_AUDIO_ChannelRF;
                        audioInfo.nChannels = 2;
                        break;
                    case NEXUS_AudioAc3Acmod_eThreeChannel_3_0_L_C_R:
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelLF] = OMX_AUDIO_ChannelLF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelRF] = OMX_AUDIO_ChannelRF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelCF] = OMX_AUDIO_ChannelCF;
                        audioInfo.nChannels = 3;
                        break;
                    case NEXUS_AudioAc3Acmod_eThreeChannel_2_1_L_R_S:
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelLF] = OMX_AUDIO_ChannelLF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelRF] = OMX_AUDIO_ChannelRF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelCS] = OMX_AUDIO_ChannelCS;
                        audioInfo.nChannels = 3;
                        break;
                    case NEXUS_AudioAc3Acmod_eFourChannel_3_1_L_C_R_S:
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelLF] = OMX_AUDIO_ChannelLF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelRF] = OMX_AUDIO_ChannelRF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelCF] = OMX_AUDIO_ChannelCF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelCS] = OMX_AUDIO_ChannelCS;
                        audioInfo.nChannels = 4;
                        break;
                    case NEXUS_AudioAc3Acmod_eFourChannel_2_2_L_R_SL_SR:
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelLF] = OMX_AUDIO_ChannelLF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelRF] = OMX_AUDIO_ChannelRF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelLS] = OMX_AUDIO_ChannelLS;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelRS] = OMX_AUDIO_ChannelRS;
                        audioInfo.nChannels = 4;
                        break;
                    case NEXUS_AudioAc3Acmod_eFiveChannel_3_2_L_C_R_SL_SR:
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelLF] = OMX_AUDIO_ChannelLF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelRF] = OMX_AUDIO_ChannelRF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelCF] = OMX_AUDIO_ChannelCF;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelLS] = OMX_AUDIO_ChannelLS;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelRS] = OMX_AUDIO_ChannelRS;
                        audioInfo.nChannels = 5;
                    default:
                        break;
                    }
                    if ( adecStatus.codecStatus.ac3.lfe )
                    {
                        audioInfo.nChannels++;
                        audioInfo.eChannelMapping[OMX_AUDIO_ChannelLFE] = OMX_AUDIO_ChannelLFE;
                    }
                    audioInfo.nBitPerSample = 24;
                    audioInfo.eCodingType = (OMX_AUDIO_CODINGTYPE)(adecStatus.codecStatus.ac3.bitStreamId == 16 ? (int)OMX_AUDIO_CodingEAC3 : (int)OMX_AUDIO_CodingAC3);
                    break;
                default:
                    break;
                }
                (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate,
                                               (OMX_EVENTTYPE)((int)OMX_EventAudioInfoAvailable), 0, 0, &audioInfo);
            }
        }
    }
}

void BOMX_AudioDecoder::EosTimer()
{
    m_eosTimer = NULL;
    BOMX_MSG(("EOS Timer (eosPending=%u)", m_eosPending));
    if ( m_eosPending )
    {
        NEXUS_PlaypumpStatus playpumpStatus;

        if ( m_hPlaypump == NULL || m_hSimpleAudioDecoder == NULL )
        {
            BOMX_MSG(("m_hPlaypump %#x m_hDecoder %#x", m_hPlaypump, m_hSimpleAudioDecoder));
            m_eosPending = false;
            return;
        }

        NEXUS_Playpump_GetStatus(m_hPlaypump, &playpumpStatus);
        BOMX_MSG(("descFifoDepth %u", playpumpStatus.descFifoDepth));
        if ( playpumpStatus.descFifoDepth <= 1 )
        {
            NEXUS_AudioDecoderStatus adecStatus;
            NEXUS_SimpleAudioDecoder_GetStatus(m_hSimpleAudioDecoder, &adecStatus);
            BOMX_MSG(("Audio FIFO Depth %u", adecStatus.fifoDepth));
            if ( adecStatus.fifoDepth == m_prevFifoDepth )
            {
                m_staticFifoDepthCount++;
                BOMX_MSG(("Static for %u timers", m_staticFifoDepthCount));
                if ( m_staticFifoDepthCount >= 3 )
                {
                    m_eosPending = false;
                    HandleEOS();
                }
            }
            else
            {
                m_prevFifoDepth = adecStatus.fifoDepth;
                m_staticFifoDepthCount = 0;
            }
        }
    }

    if ( m_eosPending )
    {
        BOMX_MSG(("Re-Starting EOS Timer"));
        m_eosTimer = StartTimer(BOMX_AUDIO_EOS_TIMEOUT, BOMX_AudioDecoder_EosTimer, static_cast <void *> (this));
    }
}
