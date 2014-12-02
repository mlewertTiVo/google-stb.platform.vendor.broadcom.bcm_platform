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
#undef LOG_TAG
#define LOG_TAG "bomx_video_decoder"

#include <cutils/log.h>

#include "bomx_video_decoder.h"
#include "nexus_platform.h"
#include "OMX_IndexExt.h"
#include "OMX_VideoExt.h"
#include "nexus_base_mmap.h"

#define BOMX_INPUT_MSG(x)

#define B_HEADER_BUFFER_SIZE (32+BOMX_BCMV_HEADER_SIZE)
#define B_DATA_BUFFER_SIZE (1024*1024)
#define B_NUM_BUFFERS ((6*1024*1024)/B_DATA_BUFFER_SIZE)
#define B_STREAM_ID 0xe0
#define B_MAX_FRAMES (4)
#define B_MAX_DECODED_FRAMES (16)
#define B_FRAME_TIMER_INTERVAL (32)

#define OMX_IndexParamEnableAndroidNativeGraphicsBuffer      0x7F000001
#define OMX_IndexParamGetAndroidNativeBufferUsage            0x7F000002
#define OMX_IndexParamStoreMetaDataInBuffers                 0x7F000003
#define OMX_IndexParamUseAndroidNativeBuffer                 0x7F000004
#define OMX_IndexParamUseAndroidNativeBuffer2                0x7F000005
#define OMX_IndexParamDescribeColorFormat                    0x7F000006
#define OMX_IndexParamConfigureVideoTunnelMode               0x7F000007

using namespace android;

static const char *g_roles[] = {"video_decoder.mpeg2", "video_decoder.h263", "video_decoder.avc", "video_decoder.mpeg4", "video_decoder.hevc", "video_decoder.vp8"};
static const unsigned int g_numRoles = sizeof(g_roles)/sizeof(const char *);
static int g_roleCodec[] = {OMX_VIDEO_CodingMPEG2, OMX_VIDEO_CodingH263, OMX_VIDEO_CodingAVC, OMX_VIDEO_CodingMPEG4, OMX_VIDEO_CodingHEVC, OMX_VIDEO_CodingVP8};

// Uncomment this to enable output logging to a file
//#define DEBUG_FILE_OUTPUT 1

#include <stdio.h>
static FILE *g_pDebugFile;

extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_Create(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    BOMX_VideoDecoder *pVideoDecoder = new BOMX_VideoDecoder(pComponentTpe, pName, pAppData, pCallbacks);
    if ( NULL == pVideoDecoder )
    {
        return BOMX_ERR_TRACE(OMX_ErrorUndefined);
    }
    else
    {
        if ( pVideoDecoder->IsValid() )
        {
            #ifdef DEBUG_FILE_OUTPUT
            if ( NULL == g_pDebugFile )
            {
                g_pDebugFile = fopen("/data/media/video_decoder.debug.pes", "wb+");
            }
            #endif
            return OMX_ErrorNone;
        }
        else
        {
            delete pVideoDecoder;
            return BOMX_ERR_TRACE(OMX_ErrorUndefined);
        }
    }
}

extern "C" const char *BOMX_VideoDecoder_GetRole(unsigned roleIndex)
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

static void BOMX_VideoDecoder_PlaypumpDataCallback(void *pParam, int param)
{
    B_EventHandle hEvent;
    BSTD_UNUSED(param);
    hEvent = static_cast <B_EventHandle> (pParam);
    BOMX_ASSERT(NULL != hEvent);
    B_Event_Set(hEvent);
    BOMX_INPUT_MSG(("PlaypumpDataCallback"));
}

static void BOMX_VideoDecoder_PlaypumpEvent(void *pParam)
{
    BOMX_VideoDecoder *pDecoder = static_cast <BOMX_VideoDecoder *> (pParam);

    BOMX_INPUT_MSG(("PlaypumpEvent"));

    pDecoder->PlaypumpEvent();
}

static void BOMX_VideoDecoder_SourceChangedCallback(void *pParam, int param)
{
    B_EventHandle hEvent;
    BSTD_UNUSED(param);
    hEvent = static_cast <B_EventHandle> (pParam);
    BOMX_ASSERT(NULL != hEvent);
    B_Event_Set(hEvent);
    BOMX_MSG(("SourceChangedCallback"));
}

static void BOMX_VideoDecoder_OutputFrameEvent(void *pParam)
{
    BOMX_VideoDecoder *pDecoder = static_cast <BOMX_VideoDecoder *> (pParam);

    BOMX_MSG(("OutputFrameEvent"));

    pDecoder->OutputFrameEvent();
}

static void BOMX_VideoDecoder_OutputFrameTimer(void *pParam)
{
    BOMX_VideoDecoder *pDecoder = static_cast <BOMX_VideoDecoder *> (pParam);

    BOMX_MSG(("OutputFrameTimer"));

    pDecoder->OutputFrameTimer();
}

static void BOMX_VideoDecoder_CheckpointComplete(void *pParam, int unused)
{
    B_EventHandle hEvent = static_cast<B_EventHandle>(pParam);
    BSTD_UNUSED(unused);

    BOMX_MSG(("Checkpoint Event"));

    B_Event_Set(hEvent);
}

static OMX_ERRORTYPE BOMX_VideoDecoder_InitMimeType(OMX_VIDEO_CODINGTYPE eCompressionFormat, char *pMimeType)
{
    const char *pMimeTypeStr;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    BOMX_ASSERT(NULL != pMimeType);

    switch ( (int)eCompressionFormat ) // Typecasted to int to avoid warning about OMX_VIDEO_CodingVP8 not in enum
    {
    case OMX_VIDEO_CodingMPEG2:
        pMimeTypeStr = "video/mpeg2";
        break;
    case OMX_VIDEO_CodingH263:
        pMimeTypeStr = "video/3gpp";
        break;
    case OMX_VIDEO_CodingMPEG4:
        pMimeTypeStr = "video/mp4v-es";
        break;
    case OMX_VIDEO_CodingHEVC:
        pMimeTypeStr = "video/hevc";
        break;
    case OMX_VIDEO_CodingAVC:
        pMimeTypeStr = "video/avc";
        break;
    case OMX_VIDEO_CodingVP8:
        pMimeTypeStr = "video/x-vnd.on2.vp8";
        break;
    case OMX_VIDEO_CodingVP9:
        pMimeTypeStr = "video/x-vnd.on2.vp9";
        break;
    default:
        BOMX_WRN(("Unable to find MIME type for eCompressionFormat %u", eCompressionFormat));
        pMimeTypeStr = "unknown";
        err = OMX_ErrorBadParameter;
        break;
    }
    strncpy(pMimeType, pMimeTypeStr, OMX_MAX_STRINGNAME_SIZE);
    return err;
}

static OMX_VIDEO_H263PROFILETYPE BOMX_H263ProfileFromNexus(NEXUS_VideoProtocolProfile nexusProfile)
{
    switch ( nexusProfile )
    {
    default:
    case NEXUS_VideoProtocolProfile_eBaseline:   return OMX_VIDEO_H263ProfileBaseline;
    }
}

static OMX_VIDEO_H263LEVELTYPE BOMX_H263LevelFromNexus(NEXUS_VideoProtocolLevel nexusLevel)
{
    switch ( nexusLevel )
    {
    default:
    case NEXUS_VideoProtocolLevel_e10: return OMX_VIDEO_H263Level10;
    case NEXUS_VideoProtocolLevel_e20: return OMX_VIDEO_H263Level20;
    case NEXUS_VideoProtocolLevel_e30: return OMX_VIDEO_H263Level30;
    case NEXUS_VideoProtocolLevel_e40: return OMX_VIDEO_H263Level40;
#if 0 // Not supported in nexus API
    case NEXUS_VideoProtocolLevel_e45: return OMX_VIDEO_H263Level45;
#endif
    case NEXUS_VideoProtocolLevel_e50: return OMX_VIDEO_H263Level50;
    case NEXUS_VideoProtocolLevel_e60: return OMX_VIDEO_H263Level60;
#if 0 // Not supported in nexus API
    case NEXUS_VideoProtocolLevel_e70: return OMX_VIDEO_H263Level70;
#endif
    }
}

static OMX_VIDEO_MPEG2PROFILETYPE BOMX_Mpeg2ProfileFromNexus(NEXUS_VideoProtocolProfile nexusProfile)
{
    switch ( nexusProfile )
    {
    default:
    case NEXUS_VideoProtocolProfile_eMain:   return OMX_VIDEO_MPEG2ProfileMain;
    case NEXUS_VideoProtocolProfile_eSimple: return OMX_VIDEO_MPEG2ProfileSimple;
    case NEXUS_VideoProtocolProfile_eHigh:   return OMX_VIDEO_MPEG2ProfileHigh;
    }
}

static OMX_VIDEO_MPEG2LEVELTYPE BOMX_Mpeg2LevelFromNexus(NEXUS_VideoProtocolLevel nexusLevel)
{
    switch ( nexusLevel )
    {
    default:
    case NEXUS_VideoProtocolLevel_eMain:     return OMX_VIDEO_MPEG2LevelML;
    case NEXUS_VideoProtocolLevel_eLow:      return OMX_VIDEO_MPEG2LevelLL;
    case NEXUS_VideoProtocolLevel_eHigh:     return OMX_VIDEO_MPEG2LevelHL;
    case NEXUS_VideoProtocolLevel_eHigh1440: return OMX_VIDEO_MPEG2LevelH14;
    }
}

static OMX_VIDEO_MPEG4PROFILETYPE BOMX_Mpeg4ProfileFromNexus(NEXUS_VideoProtocolProfile nexusProfile)
{
    switch ( nexusProfile )
    {
    default:
    case NEXUS_VideoProtocolProfile_eMain:             return OMX_VIDEO_MPEG4ProfileMain;
    case NEXUS_VideoProtocolProfile_eSimple:           return OMX_VIDEO_MPEG4ProfileSimple;
    case NEXUS_VideoProtocolProfile_eAdvancedSimple:   return OMX_VIDEO_MPEG4ProfileAdvancedSimple;
    }
}

static OMX_VIDEO_MPEG4LEVELTYPE BOMX_Mpeg4LevelFromNexus(NEXUS_VideoProtocolLevel nexusLevel)
{
    switch ( nexusLevel )
    {
    default:
    case NEXUS_VideoProtocolLevel_e00: return OMX_VIDEO_MPEG4Level0;
    case NEXUS_VideoProtocolLevel_e10: return OMX_VIDEO_MPEG4Level1;
    case NEXUS_VideoProtocolLevel_e20: return OMX_VIDEO_MPEG4Level2;
    case NEXUS_VideoProtocolLevel_e30: return OMX_VIDEO_MPEG4Level3;
    case NEXUS_VideoProtocolLevel_e40: return OMX_VIDEO_MPEG4Level4;
    case NEXUS_VideoProtocolLevel_e50: return OMX_VIDEO_MPEG4Level5;
    }
}

static OMX_VIDEO_AVCPROFILETYPE BOMX_AvcProfileFromNexus(NEXUS_VideoProtocolProfile nexusProfile)
{
    switch ( nexusProfile )
    {
    default:
    case NEXUS_VideoProtocolProfile_eMain:     return OMX_VIDEO_AVCProfileMain;
    case NEXUS_VideoProtocolProfile_eBaseline: return OMX_VIDEO_AVCProfileBaseline;
    case NEXUS_VideoProtocolProfile_eHigh:     return OMX_VIDEO_AVCProfileHigh;
    }
}

static OMX_VIDEO_AVCLEVELTYPE BOMX_AvcLevelFromNexus(NEXUS_VideoProtocolLevel nexusLevel)
{
    switch ( nexusLevel )
    {
    default:
    case NEXUS_VideoProtocolLevel_e10: return OMX_VIDEO_AVCLevel1;
    case NEXUS_VideoProtocolLevel_e1B: return OMX_VIDEO_AVCLevel1b;
    case NEXUS_VideoProtocolLevel_e11: return OMX_VIDEO_AVCLevel11;
    case NEXUS_VideoProtocolLevel_e12: return OMX_VIDEO_AVCLevel12;
    case NEXUS_VideoProtocolLevel_e13: return OMX_VIDEO_AVCLevel13;
    case NEXUS_VideoProtocolLevel_e20: return OMX_VIDEO_AVCLevel2;
    case NEXUS_VideoProtocolLevel_e21: return OMX_VIDEO_AVCLevel21;
    case NEXUS_VideoProtocolLevel_e22: return OMX_VIDEO_AVCLevel22;
    case NEXUS_VideoProtocolLevel_e30: return OMX_VIDEO_AVCLevel3;
    case NEXUS_VideoProtocolLevel_e31: return OMX_VIDEO_AVCLevel31;
    case NEXUS_VideoProtocolLevel_e32: return OMX_VIDEO_AVCLevel32;
    case NEXUS_VideoProtocolLevel_e40: return OMX_VIDEO_AVCLevel4;
    case NEXUS_VideoProtocolLevel_e41: return OMX_VIDEO_AVCLevel41;
    case NEXUS_VideoProtocolLevel_e42: return OMX_VIDEO_AVCLevel42;
    case NEXUS_VideoProtocolLevel_e50: return OMX_VIDEO_AVCLevel5;
    case NEXUS_VideoProtocolLevel_e51: return OMX_VIDEO_AVCLevel51;
    }
}

static OMX_VIDEO_HEVCPROFILETYPE BOMX_HevcProfileFromNexus(NEXUS_VideoProtocolProfile nexusProfile)
{
    switch ( nexusProfile )
    {
    default:
    case NEXUS_VideoProtocolProfile_eMain:     return OMX_VIDEO_HEVCProfileMain;
    }
}

static OMX_VIDEO_HEVCLEVELTYPE BOMX_HevcLevelFromNexus(NEXUS_VideoProtocolLevel nexusLevel)
{
    switch ( nexusLevel )
    {
    default: return OMX_VIDEO_HEVCLevelUnknown;
    case NEXUS_VideoProtocolLevel_e10: return OMX_VIDEO_HEVCMainTierLevel1;
    case NEXUS_VideoProtocolLevel_e20: return OMX_VIDEO_HEVCMainTierLevel2;
    case NEXUS_VideoProtocolLevel_e21: return OMX_VIDEO_HEVCMainTierLevel21;
    case NEXUS_VideoProtocolLevel_e30: return OMX_VIDEO_HEVCMainTierLevel3;
    case NEXUS_VideoProtocolLevel_e31: return OMX_VIDEO_HEVCMainTierLevel31;
    case NEXUS_VideoProtocolLevel_e40: return OMX_VIDEO_HEVCMainTierLevel4;
    case NEXUS_VideoProtocolLevel_e41: return OMX_VIDEO_HEVCMainTierLevel41;
    case NEXUS_VideoProtocolLevel_e50: return OMX_VIDEO_HEVCMainTierLevel5;
    case NEXUS_VideoProtocolLevel_e51: return OMX_VIDEO_HEVCMainTierLevel51;
    case NEXUS_VideoProtocolLevel_e60: return OMX_VIDEO_HEVCMainTierLevel6;
    case NEXUS_VideoProtocolLevel_e62: return OMX_VIDEO_HEVCMainTierLevel62;
    }
}

static OMX_VIDEO_VP8PROFILETYPE BOMX_VP8ProfileFromNexus(NEXUS_VideoProtocolProfile nexusProfile)
{
    switch ( nexusProfile )
    {
    default:
    case NEXUS_VideoProtocolProfile_eMain:   return OMX_VIDEO_VP8ProfileMain;
    }
}

static OMX_VIDEO_VP8LEVELTYPE BOMX_VP8LevelFromNexus(NEXUS_VideoProtocolLevel nexusLevel)
{
    switch ( nexusLevel )
    {
    case NEXUS_VideoProtocolLevel_e00: return OMX_VIDEO_VP8Level_Version0;
    case NEXUS_VideoProtocolLevel_e10: return OMX_VIDEO_VP8Level_Version1;
    case NEXUS_VideoProtocolLevel_e20: return OMX_VIDEO_VP8Level_Version2;
    case NEXUS_VideoProtocolLevel_e30: return OMX_VIDEO_VP8Level_Version3;
    default:                           return OMX_VIDEO_VP8LevelUnknown;
    }
}

static void BOMX_VideoDecoder_FormBppPacket(char *pBuffer, uint32_t opcode)
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

void OmxBinder::notify( int msg, int param1, int param2 )
{
   ALOGV( "%s: notify received: msg=%u, param1=0x%x, param2=0x%x",
          __FUNCTION__, msg, param1, param2 );

   if (cb)
      cb(cb_data, msg, param1, param2);
}

static void BOMX_OmxBinderNotify(int cb_data, int msg, int param1, int param2)
{
    BOMX_VideoDecoder *pComponent = (BOMX_VideoDecoder *)cb_data;

    BSTD_UNUSED(param1);
    BOMX_ASSERT(NULL != pComponent);

    switch (msg)
    {
    case HWC_BINDER_NTFY_DISPLAY:
        pComponent->DisplayFrame((unsigned)param2);
        break;
    default:
        break;
    }
}

BOMX_VideoDecoder::BOMX_VideoDecoder(
    OMX_COMPONENTTYPE *pComponentType,
    const OMX_STRING pName,
    const OMX_PTR pAppData,
    const OMX_CALLBACKTYPE *pCallbacks) :
    BOMX_Component(pComponentType, pName, pAppData, pCallbacks, BOMX_VideoDecoder_GetRole),
    m_hSimpleVideoDecoder(NULL),
    m_hPlaypump(NULL),
    m_hPidChannel(NULL),
    m_hPlaypumpEvent(NULL),
    m_playpumpEventId(NULL),
    m_hOutputFrameEvent(NULL),
    m_outputFrameEventId(NULL),
    m_outputFrameTimerId(NULL),
    m_hCheckpointEvent(NULL),
    m_hGraphics2d(NULL),
    m_submittedDescriptors(0),
    m_pBufferTracker(NULL),
    m_pIpcClient(NULL),
    m_pNexusClient(NULL),
    m_pEosBuffer(NULL),
    m_eosPending(false),
    m_eosDelivered(false),
    m_formatChangePending(false),
    m_nativeGraphicsEnabled(false),
    m_metadataEnabled(false),
    m_secureDecoder(false),
    m_pConfigBuffer(NULL),
    m_configRequired(false),
    m_configBufferSize(0),
    m_outputMode(BOMX_VideoDecoderOutputBufferType_eStandard)
{
    unsigned i;
    NEXUS_Error errCode;
    #define MAX_PORT_FORMATS (6)
    #define MAX_OUTPUT_PORT_FORMATS (2)

    BDBG_CASSERT(MAX_OUTPUT_PORT_FORMATS <= MAX_PORT_FORMATS);

    OMX_VIDEO_PORTDEFINITIONTYPE portDefs;
    OMX_VIDEO_PARAM_PORTFORMATTYPE portFormats[MAX_PORT_FORMATS];

    m_videoPortBase = 0;    // Android seems to require this - was: BOMX_COMPONENT_PORT_BASE(BOMX_ComponentId_eVideoDecoder, OMX_PortDomainVideo);

    memset(&portDefs, 0, sizeof(portDefs));
    portDefs.eCompressionFormat = OMX_VIDEO_CodingMPEG2;
    portDefs.cMIMEType = m_inputMimeType;
    (void)BOMX_VideoDecoder_InitMimeType(portDefs.eCompressionFormat, m_inputMimeType);
    for ( i = 0; i < MAX_PORT_FORMATS; i++ )
    {
        memset(&portFormats[i], 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        BOMX_STRUCT_INIT(&portFormats[i]);
        portFormats[i].nPortIndex = m_videoPortBase;
        portFormats[i].nIndex = i;
        switch ( i )
        {
        default:
        case 0:
            portFormats[i].eCompressionFormat = OMX_VIDEO_CodingMPEG2; break;
        case 1:
            portFormats[i].eCompressionFormat = OMX_VIDEO_CodingH263; break;
        case 2:
            portFormats[i].eCompressionFormat = OMX_VIDEO_CodingAVC; break;
        case 3:
            portFormats[i].eCompressionFormat = OMX_VIDEO_CodingMPEG4; break;
        case 4:
            portFormats[i].eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingHEVC; break;
        case 5:
            portFormats[i].eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVP8; break;
        }
    }

    SetRole(g_roles[0]);
    m_numVideoPorts = 0;
    m_pVideoPorts[0] = new BOMX_VideoPort(m_videoPortBase, OMX_DirInput, B_NUM_BUFFERS, B_DATA_BUFFER_SIZE, false, 0, &portDefs, portFormats, MAX_PORT_FORMATS);
    if ( NULL == m_pVideoPorts[0] )
    {
        BOMX_ERR(("Unable to create video input port"));
        this->Invalidate();
        return;
    }
    m_numVideoPorts = 1;
    memset(&portDefs, 0, sizeof(portDefs));
    portDefs.eCompressionFormat = OMX_VIDEO_CodingUnused;
    portDefs.eColorFormat = OMX_COLOR_Format16bitRGB565;
    strcpy(m_outputMimeType, "video/x-raw");
    portDefs.cMIMEType = m_outputMimeType;
    portDefs.nFrameWidth = 1920;
    portDefs.nFrameHeight = 1080;
    portDefs.nStride = 2*portDefs.nFrameWidth;
    portDefs.nSliceHeight = portDefs.nFrameHeight;
    for ( i = 0; i < MAX_OUTPUT_PORT_FORMATS; i++ )
    {
        memset(&portFormats[i], 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        BOMX_STRUCT_INIT(&portFormats[i]);
        portFormats[i].nIndex = i;
        portFormats[i].nPortIndex = m_videoPortBase+1;
        portFormats[i].eCompressionFormat = OMX_VIDEO_CodingUnused;
        switch ( i )
        {
        default:
        case 0: // OMX buffer format
            portFormats[i].eColorFormat = OMX_COLOR_Format16bitRGB565;
            break;
        case 1: // Native grpahics format
            portFormats[i].eColorFormat = (OMX_COLOR_FORMATTYPE)((int)PIXEL_FORMAT_RGB_565);
            break;
        }
    }
    m_pVideoPorts[1] = new BOMX_VideoPort(m_videoPortBase+1, OMX_DirOutput, B_MAX_FRAMES, portDefs.nFrameWidth*portDefs.nFrameHeight*2, false, 0, &portDefs, portFormats, MAX_OUTPUT_PORT_FORMATS);
    if ( NULL == m_pVideoPorts[1] )
    {
        BOMX_ERR(("Unable to create video output port"));
        this->Invalidate();
        return;
    }
    m_numVideoPorts = 2;

    m_hPlaypumpEvent = B_Event_Create(NULL);
    if ( NULL == m_hPlaypumpEvent )
    {
        BOMX_ERR(("Unable to create playpump event"));
        this->Invalidate();
        return;
    }

    m_hOutputFrameEvent = B_Event_Create(NULL);
    if ( NULL == m_hOutputFrameEvent )
    {
        BOMX_ERR(("Unable to create output frame event"));
        this->Invalidate();
        return;
    }

    m_outputFrameEventId = this->RegisterEvent(m_hOutputFrameEvent, BOMX_VideoDecoder_OutputFrameEvent, static_cast <void *> (this));
    if ( NULL == m_outputFrameEventId )
    {
        BOMX_ERR(("Unable to register output frame event"));
        this->Invalidate();
        return;
    }

    m_pBufferTracker = new BOMX_BufferTracker();
    if ( NULL == m_pBufferTracker || !m_pBufferTracker->Valid() )
    {
        BOMX_ERR(("Unable to create buffer tracker"));
        this->Invalidate();
        return;
    }

    m_pIpcClient = NexusIPCClientFactory::getClient(pName);
    if ( NULL == m_pIpcClient )
    {
        BOMX_ERR(("Unable to create client factory"));
        this->Invalidate();
        return;
    }

    b_refsw_client_client_configuration         config;

    BKNI_Memset(&config, 0, sizeof(config));
    BKNI_Snprintf(config.name.string, sizeof(config.name.string), pName);
    config.resources.screen.required = true;
    config.resources.videoDecoder = true;
    m_pNexusClient = m_pIpcClient->createClientContext(&config);

    if (m_pNexusClient == NULL)
    {
        BOMX_ERR(("Unable to create nexus client context"));
        this->Invalidate();
        return;
    }

    NEXUS_ClientConfiguration               clientConfig;
    NEXUS_MemoryAllocationSettings          memorySettings;

    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    NEXUS_Memory_GetDefaultAllocationSettings(&memorySettings);
    memorySettings.heap = clientConfig.heap[1];
    errCode = NEXUS_Memory_Allocate(BOMX_VIDEO_EOS_LEN, &memorySettings, &m_pEosBuffer);
    if ( errCode )
    {
        BOMX_ERR(("Unable to allocate EOS buffer"));
        this->Invalidate();
        return;
    }
    /* Populate EOS buffer */
    char *pBuffer = (char *)m_pEosBuffer;
    BOMX_VideoDecoder_FormBppPacket(pBuffer, 0xa); /* Inline flush / TPD */
    BOMX_VideoDecoder_FormBppPacket(pBuffer+184, 0x82); /* LAST */
    BOMX_VideoDecoder_FormBppPacket(pBuffer+368, 0xa); /* Inline flush / TPD */
    NEXUS_FlushCache(pBuffer, BOMX_VIDEO_EOS_LEN);

    m_hCheckpointEvent = B_Event_Create(NULL);
    if ( NULL == m_hCheckpointEvent )
    {
        BOMX_ERR(("Unable to create checkpoint event"));
        this->Invalidate();
        return;
    }
    m_hGraphics2d = NEXUS_Graphics2D_Open(NEXUS_ANY_ID, NULL);
    if ( NULL == m_hGraphics2d )
    {
        BOMX_ERR(("Unable to open graphics 2d"));
        this->Invalidate();
        return;
    }
    else
    {
        NEXUS_Graphics2DSettings gfxSettings;
        NEXUS_Graphics2D_GetSettings(m_hGraphics2d, &gfxSettings);
        gfxSettings.checkpointCallback.callback = BOMX_VideoDecoder_CheckpointComplete;
        gfxSettings.checkpointCallback.context = static_cast<void *>(m_hCheckpointEvent);
        gfxSettings.pollingCheckpoint = false;
        errCode = NEXUS_Graphics2D_SetSettings(m_hGraphics2d, &gfxSettings);
        if ( errCode )
        {
            errCode = BOMX_BERR_TRACE(errCode);
            this->Invalidate();
            return;
        }
    }

    BLST_Q_INIT(&m_frameBufferFreeList);
    BLST_Q_INIT(&m_frameBufferAllocList);

    for ( i = 0; i < B_MAX_DECODED_FRAMES; i++ )
    {
        BOMX_VideoDecoderFrameBuffer *pBuffer = new BOMX_VideoDecoderFrameBuffer;
        if ( NULL == pBuffer )
        {
            errCode = BOMX_BERR_TRACE(BERR_OUT_OF_SYSTEM_MEMORY);
            this->Invalidate();
            return;
        }
        BKNI_Memset(pBuffer, 0, sizeof(BOMX_VideoDecoderFrameBuffer));
        pBuffer->state = BOMX_VideoDecoderFrameBufferState_eInvalid;
        BLST_Q_INSERT_TAIL(&m_frameBufferFreeList, pBuffer, node);
    }

    // Init BCMV header for VP8 & other codecs
    BKNI_Memset(m_pBcmvHeader, 0, sizeof(m_pBcmvHeader));
    m_pBcmvHeader[0] = 'B';
    m_pBcmvHeader[1] = 'C';
    m_pBcmvHeader[2] = 'M';
    m_pBcmvHeader[3] = 'V';

    // connect to the HWC binder.
    m_omxHwcBinder = new OmxBinder_wrap;
    if ( NULL == m_omxHwcBinder )
    {
        BOMX_ERR(("Unable to connect to HwcBinder"));
        this->Invalidate();
        return;
    }
    m_omxHwcBinder->get()->register_notify(&BOMX_OmxBinderNotify, (int)this);
    m_omxHwcBinder->getvideo(0, m_surfaceClientId);
}

BOMX_VideoDecoder::~BOMX_VideoDecoder()
{
    unsigned i;
    BOMX_VideoDecoderFrameBuffer *pBuffer;

    // Stop listening to HWC. Note that HWC binder does need to be protected given
    // it's not updated during the decoder's lifetime.
    if (m_omxHwcBinder)
    {
        delete m_omxHwcBinder;
        m_omxHwcBinder = NULL;
    }

    Lock();

    ShutdownScheduler();

    if ( m_hPlaypumpEvent )
    {
        B_Event_Destroy(m_hPlaypumpEvent);
    }
    if ( m_outputFrameEventId )
    {
        UnregisterEvent(m_outputFrameEventId);
    }
    if ( m_outputFrameTimerId )
    {
        CancelTimer(m_outputFrameTimerId);
    }
    if ( m_hOutputFrameEvent )
    {
        B_Event_Destroy(m_hOutputFrameEvent);
    }
    if ( m_hGraphics2d )
    {
        NEXUS_Graphics2D_Close(m_hGraphics2d);
    }
    if ( m_hCheckpointEvent )
    {
        B_Event_Destroy(m_hCheckpointEvent);
    }
    for ( i = 0; i < m_numVideoPorts; i++ )
    {
        if ( m_pVideoPorts[i] )
        {
            delete m_pVideoPorts[i];
        }
    }
    if ( m_pEosBuffer )
    {
        NEXUS_Memory_Free(m_pEosBuffer);
    }
    if ( m_pConfigBuffer )
    {
        FreeInputBuffer(m_pConfigBuffer);
    }
    if ( m_pIpcClient )
    {
        if ( m_pNexusClient )
        {
            m_pIpcClient->destroyClientContext(m_pNexusClient);
        }
        delete m_pIpcClient;
    }
    if ( m_pBufferTracker )
    {
        delete m_pBufferTracker;
    }
    while ( (pBuffer = BLST_Q_FIRST(&m_frameBufferAllocList)) )
    {
        BLST_Q_REMOVE_HEAD(&m_frameBufferAllocList, node);
        delete pBuffer;
    }
    while ( (pBuffer = BLST_Q_FIRST(&m_frameBufferFreeList)) )
    {
        BLST_Q_REMOVE_HEAD(&m_frameBufferFreeList, node);
        delete pBuffer;
    }

    Unlock();
}

OMX_ERRORTYPE BOMX_VideoDecoder::GetParameter(
        OMX_IN  OMX_INDEXTYPE nParamIndex,
        OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    switch ( (int)nParamIndex )
    {
    case OMX_IndexParamVideoH263:
        {
            OMX_VIDEO_PARAM_H263TYPE *pH263 = (OMX_VIDEO_PARAM_H263TYPE *)pComponentParameterStructure;
            BOMX_MSG(("GetParameter OMX_IndexParamVideoH263"));
            BOMX_STRUCT_VALIDATE(pH263);
            if ( pH263->nPortIndex != m_videoPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            pH263->nPFrames = 0;
            pH263->nBFrames = 0;
            pH263->eProfile = OMX_VIDEO_H263ProfileBaseline;
            pH263->eLevel = OMX_VIDEO_H263Level10;
            pH263->bPLUSPTYPEAllowed = OMX_FALSE;
            pH263->nAllowedPictureTypes = (OMX_U32)OMX_VIDEO_PictureTypeI|(OMX_U32)OMX_VIDEO_PictureTypeP|(OMX_U32)OMX_VIDEO_PictureTypeB|(OMX_U32)OMX_VIDEO_PictureTypeSI|(OMX_U32)OMX_VIDEO_PictureTypeSP;
            pH263->bForceRoundingTypeToZero = OMX_TRUE;
            pH263->nPictureHeaderRepetition = 0;
            pH263->nGOBHeaderInterval = 0;
            if ( m_hSimpleVideoDecoder )
            {
                NEXUS_VideoDecoderStatus vdecStatus;
                NEXUS_SimpleVideoDecoder_GetStatus(m_hSimpleVideoDecoder, &vdecStatus);
                pH263->eProfile = BOMX_H263ProfileFromNexus(vdecStatus.protocolProfile);
                pH263->eLevel = BOMX_H263LevelFromNexus(vdecStatus.protocolLevel);
            }
            return OMX_ErrorNone;
        }
    case OMX_IndexParamVideoAvc:
        {
            OMX_VIDEO_PARAM_AVCTYPE *pAvc = (OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure;
            BOMX_MSG(("GetParameter OMX_IndexParamVideoAvc"));
            BOMX_STRUCT_VALIDATE(pAvc);
            if ( pAvc->nPortIndex != m_videoPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            // Much of this structure is not relevant.  Zero everything except for allowed picture types, profile, level.
            memset(pAvc, 0, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
            BOMX_STRUCT_INIT(pAvc);
            pAvc->nPortIndex = m_videoPortBase;
            pAvc->nRefFrames = 16;
            pAvc->eProfile = OMX_VIDEO_AVCProfileBaseline;
            pAvc->eLevel = OMX_VIDEO_AVCLevel1;
            pAvc->nAllowedPictureTypes = (OMX_U32)OMX_VIDEO_PictureTypeI|(OMX_U32)OMX_VIDEO_PictureTypeP|(OMX_U32)OMX_VIDEO_PictureTypeB|(OMX_U32)OMX_VIDEO_PictureTypeEI|(OMX_U32)OMX_VIDEO_PictureTypeEP;
            if ( m_hSimpleVideoDecoder )
            {
                NEXUS_VideoDecoderStatus vdecStatus;
                NEXUS_SimpleVideoDecoder_GetStatus(m_hSimpleVideoDecoder, &vdecStatus);
                pAvc->eProfile = BOMX_AvcProfileFromNexus(vdecStatus.protocolProfile);
                pAvc->eLevel = BOMX_AvcLevelFromNexus(vdecStatus.protocolLevel);
            }
            return OMX_ErrorNone;
        }
    case OMX_IndexParamVideoMpeg4:
        {
            OMX_VIDEO_PARAM_MPEG4TYPE *pMpeg4 = (OMX_VIDEO_PARAM_MPEG4TYPE *)pComponentParameterStructure;
            BOMX_MSG(("GetParameter OMX_IndexParamVideoMpeg4"));
            BOMX_STRUCT_VALIDATE(pMpeg4);
            if ( pMpeg4->nPortIndex != m_videoPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            // Much of this structure is not relevant.  Zero everything except for allowed picture types, profile, level.
            memset(pMpeg4, 0, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
            BOMX_STRUCT_INIT(pMpeg4);
            pMpeg4->nPortIndex = m_videoPortBase;
            pMpeg4->eProfile = OMX_VIDEO_MPEG4ProfileSimple;
            pMpeg4->eLevel = OMX_VIDEO_MPEG4Level1;
            pMpeg4->nAllowedPictureTypes = (OMX_U32)OMX_VIDEO_PictureTypeI|(OMX_U32)OMX_VIDEO_PictureTypeP|(OMX_U32)OMX_VIDEO_PictureTypeB|(OMX_U32)OMX_VIDEO_PictureTypeS;
            if ( m_hSimpleVideoDecoder )
            {
                NEXUS_VideoDecoderStatus vdecStatus;
                NEXUS_SimpleVideoDecoder_GetStatus(m_hSimpleVideoDecoder, &vdecStatus);
                pMpeg4->eProfile = BOMX_Mpeg4ProfileFromNexus(vdecStatus.protocolProfile);
                pMpeg4->eLevel = BOMX_Mpeg4LevelFromNexus(vdecStatus.protocolLevel);
            }
            return OMX_ErrorNone;
        }
    case OMX_IndexParamVideoVp8:
        {
            OMX_VIDEO_PARAM_VP8TYPE *pVp8 = (OMX_VIDEO_PARAM_VP8TYPE *)pComponentParameterStructure;
            BOMX_MSG(("GetParameter OMX_IndexParamVideoVp8"));
            memset(pVp8, 0, sizeof(*pVp8));
            BOMX_STRUCT_INIT(pVp8);
            pVp8->nPortIndex = m_videoPortBase;
            pVp8->eProfile = OMX_VIDEO_VP8ProfileUnknown;
            pVp8->eLevel = OMX_VIDEO_VP8LevelUnknown;
            if ( m_hSimpleVideoDecoder )
            {
                NEXUS_VideoDecoderStatus vdecStatus;
                NEXUS_SimpleVideoDecoder_GetStatus(m_hSimpleVideoDecoder, &vdecStatus);
                pVp8->eProfile = BOMX_VP8ProfileFromNexus(vdecStatus.protocolProfile);
                pVp8->eLevel = BOMX_VP8LevelFromNexus(vdecStatus.protocolLevel);
            }
            return OMX_ErrorNone;
        }
    case OMX_IndexParamVideoHevc:
        {
            OMX_VIDEO_PARAM_HEVCTYPE *pHevc = (OMX_VIDEO_PARAM_HEVCTYPE *)pComponentParameterStructure;
            BOMX_MSG(("GetParameter OMX_IndexParamVideoHevc"));
            BOMX_STRUCT_INIT(pHevc);
            pHevc->nPortIndex = m_videoPortBase;
            pHevc->eProfile = OMX_VIDEO_HEVCProfileUnknown;
            pHevc->eLevel = OMX_VIDEO_HEVCLevelUnknown;
            if ( m_hSimpleVideoDecoder )
            {
                NEXUS_VideoDecoderStatus vdecStatus;
                NEXUS_SimpleVideoDecoder_GetStatus(m_hSimpleVideoDecoder, &vdecStatus);
                pHevc->eProfile = BOMX_HevcProfileFromNexus(vdecStatus.protocolProfile);
                pHevc->eLevel = BOMX_HevcLevelFromNexus(vdecStatus.protocolLevel);
            }
            return OMX_ErrorNone;
        }
    case OMX_IndexParamVideoProfileLevelQuerySupported:
        {
            OMX_VIDEO_PARAM_PROFILELEVELTYPE *pProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
            BOMX_MSG(("GetParameter OMX_IndexParamVideoProfileLevelQuerySupported"));
            BOMX_STRUCT_VALIDATE(pProfileLevel);
            if ( pProfileLevel->nPortIndex != m_videoPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            switch ( (int)GetCodec() )
            {
            default:
                // Only certain codecs support this interface
                return BOMX_ERR_TRACE(OMX_ErrorUnsupportedIndex);
            case OMX_VIDEO_CodingMPEG2:
                pProfileLevel->eProfile = (OMX_U32)OMX_VIDEO_MPEG2ProfileMain;   // We support Main Profile
                switch ( pProfileLevel->nProfileIndex )
                {
                case 0:
                    pProfileLevel->eLevel = (OMX_U32)OMX_VIDEO_MPEG2LevelLL;
                    break;
                case 1:
                    pProfileLevel->eLevel = (OMX_U32)OMX_VIDEO_MPEG2LevelML;
                    break;
                case 2:
                    pProfileLevel->eLevel = (OMX_U32)OMX_VIDEO_MPEG2LevelHL;
                    break;
                default:
                    return OMX_ErrorNoMore;
                }
                break;
            case OMX_VIDEO_CodingH263:
                if ( pProfileLevel->nProfileIndex > 0 )
                {
                    return OMX_ErrorNoMore;
                }
                pProfileLevel->eProfile = (OMX_U32)OMX_VIDEO_H263ProfileBaseline;   // We support profile 0
                pProfileLevel->eLevel = (OMX_U32)OMX_VIDEO_H263Level70;             // We support up to 70
                break;
            case OMX_VIDEO_CodingMPEG4:
                pProfileLevel->eLevel = (OMX_U32)OMX_VIDEO_MPEG4Level5;
                switch ( pProfileLevel->nProfileIndex )
                {
                case 0:
                    pProfileLevel->eProfile = (OMX_U32)OMX_VIDEO_MPEG4ProfileSimple; //SP
                    break;
                case 1:
                    pProfileLevel->eProfile = (OMX_U32)OMX_VIDEO_MPEG4ProfileAdvancedSimple; //ASP
                    break;
                default:
                    return OMX_ErrorNoMore;
                }
                break;
            case OMX_VIDEO_CodingAVC:
                switch ( pProfileLevel->nProfileIndex )
                {
                case 0:
                    pProfileLevel->eProfile = (OMX_U32)OMX_VIDEO_AVCProfileMain;
                    pProfileLevel->eLevel = (OMX_U32)OMX_VIDEO_AVCLevel42;
                    break;
                case 1:
                    pProfileLevel->eProfile = (OMX_U32)OMX_VIDEO_AVCProfileHigh;
                    pProfileLevel->eLevel = (OMX_U32)OMX_VIDEO_AVCLevel42;
                    break;
                default:
                    return OMX_ErrorNoMore;
                }
                break;
            case OMX_VIDEO_CodingVP8:
                if ( pProfileLevel->nProfileIndex > 0 )
                {
                    return OMX_ErrorNoMore;
                }
                pProfileLevel->eProfile = (OMX_U32)OMX_VIDEO_VP8ProfileMain;
                pProfileLevel->eLevel = (OMX_U32)OMX_VIDEO_VP8Level_Version3;   // ?
                break;
            case OMX_VIDEO_CodingHEVC:
                if ( pProfileLevel->nProfileIndex > 0 )
                {
                    return OMX_ErrorNoMore;
                }
                pProfileLevel->eProfile = (OMX_U32)OMX_VIDEO_HEVCProfileMain;
                pProfileLevel->eLevel = (OMX_U32)OMX_VIDEO_HEVCMainTierLevel51;
            }
            return OMX_ErrorNone;
        }
    case OMX_IndexParamVideoProfileLevelCurrent:
        {
            OMX_VIDEO_PARAM_PROFILELEVELTYPE *pProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
            BOMX_MSG(("GetParameter OMX_IndexParamVideoProfileLevelCurrent"));
            BOMX_STRUCT_VALIDATE(pProfileLevel);
            if ( pProfileLevel->nPortIndex != m_videoPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            pProfileLevel->eProfile = 0;    // These are invalid or unused for all codecs
            pProfileLevel->eLevel = 0;
            if ( m_hSimpleVideoDecoder )
            {
                NEXUS_VideoDecoderStatus vdecStatus;
                NEXUS_SimpleVideoDecoder_GetStatus(m_hSimpleVideoDecoder, &vdecStatus);
                // Having different enums per-codec is really sadistic...
                switch ( (int)GetCodec() )
                {
                default:
                    // Only certain codecs support this interface
                    break;
                case OMX_VIDEO_CodingMPEG2:
                    pProfileLevel->eProfile = (OMX_U32)BOMX_Mpeg2ProfileFromNexus(vdecStatus.protocolProfile);
                    pProfileLevel->eLevel = (OMX_U32)BOMX_Mpeg2LevelFromNexus(vdecStatus.protocolLevel);
                    break;
                case OMX_VIDEO_CodingH263:
                    pProfileLevel->eProfile = (OMX_U32)BOMX_H263ProfileFromNexus(vdecStatus.protocolProfile);
                    pProfileLevel->eLevel = (OMX_U32)BOMX_H263LevelFromNexus(vdecStatus.protocolLevel);
                    break;
                case OMX_VIDEO_CodingMPEG4:
                    pProfileLevel->eProfile = (OMX_U32)BOMX_Mpeg4ProfileFromNexus(vdecStatus.protocolProfile);
                    pProfileLevel->eLevel = (OMX_U32)BOMX_Mpeg4LevelFromNexus(vdecStatus.protocolLevel);
                    break;
                case OMX_VIDEO_CodingAVC:
                    pProfileLevel->eProfile = (OMX_U32)BOMX_AvcProfileFromNexus(vdecStatus.protocolProfile);
                    pProfileLevel->eLevel = (OMX_U32)BOMX_AvcLevelFromNexus(vdecStatus.protocolLevel);
                    break;
                case OMX_VIDEO_CodingVP8:
                    pProfileLevel->eProfile = (OMX_U32)BOMX_VP8ProfileFromNexus(vdecStatus.protocolProfile);
                    pProfileLevel->eLevel = (OMX_U32)BOMX_VP8LevelFromNexus(vdecStatus.protocolLevel);
                case OMX_VIDEO_CodingHEVC:
                    pProfileLevel->eProfile = (OMX_U32)BOMX_HevcProfileFromNexus(vdecStatus.protocolProfile);
                    pProfileLevel->eLevel = (OMX_U32)BOMX_HevcLevelFromNexus(vdecStatus.protocolLevel);
                    break;
                }
            }
            return OMX_ErrorNone;
        }
    case OMX_IndexParamEnableAndroidNativeGraphicsBuffer:
    {
        EnableAndroidNativeBuffersParams *pEnableParams = (EnableAndroidNativeBuffersParams *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pEnableParams);
        BOMX_MSG(("GetParameter OMX_IndexParamEnableAndroidNativeGraphicsBuffer %u", pEnableParams->enable));
        if ( pEnableParams->nPortIndex != m_videoPortBase+1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        pEnableParams->enable = m_nativeGraphicsEnabled == true ? OMX_TRUE : OMX_FALSE;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamStoreMetaDataInBuffers:
    {
        StoreMetaDataInBuffersParams *pMetadata = (StoreMetaDataInBuffersParams *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pMetadata);
        BOMX_MSG(("GetParameter OMX_IndexParamStoreMetaDataInBuffers %u", pMetadata->bStoreMetaData));
        if ( pMetadata->nPortIndex != m_videoPortBase+1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        pMetadata->bStoreMetaData = m_metadataEnabled == true ? OMX_TRUE : OMX_FALSE;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamGetAndroidNativeBufferUsage:
    {
        GetAndroidNativeBufferUsageParams *pUsage = (GetAndroidNativeBufferUsageParams *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pUsage);
        BOMX_MSG(("GetParameter OMX_IndexParamGetAndroidNativeBufferUsage"));
        if ( pUsage->nPortIndex != m_videoPortBase+1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        // TODO: Revise this to use a vendor private flag w/gralloc.  SW_READ/SW_WRITE should not be used here.
        pUsage->nUsage = GRALLOC_USAGE_EXTERNAL_DISP | GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamDescribeColorFormat:
    {
        DescribeColorFormatParams *pColorFormat = (DescribeColorFormatParams *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pColorFormat);
        BOMX_MSG(("GetParameter OMX_IndexParamDescribeColorFormat"));
        const OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = m_pVideoPorts[1]->GetDefinition();
        pColorFormat->eColorFormat = pPortDef->format.video.eColorFormat;
        pColorFormat->nFrameWidth = pPortDef->format.video.nFrameWidth;
        pColorFormat->nFrameHeight = pPortDef->format.video.nFrameHeight;
        pColorFormat->nStride = pPortDef->format.video.nStride;
        pColorFormat->nSliceHeight = pPortDef->format.video.nSliceHeight;
        /* This is only supported for 4:2:0 planar formats - return unknown */
        pColorFormat->sMediaImage.mType = MediaImage::MEDIA_IMAGE_TYPE_UNKNOWN;
        return OMX_ErrorNone;
    }

    default:
        BOMX_MSG(("GetParameter %#x Deferring to base class", nParamIndex));
        return BOMX_ERR_TRACE(BOMX_Component::GetParameter(nParamIndex, pComponentParameterStructure));
    }
}

OMX_ERRORTYPE BOMX_VideoDecoder::SetParameter(
        OMX_IN  OMX_INDEXTYPE nIndex,
        OMX_IN  OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE err;

    switch ( (int)nIndex )
    {
    case OMX_IndexParamStandardComponentRole:
        {
            OMX_PARAM_COMPONENTROLETYPE *pRole = (OMX_PARAM_COMPONENTROLETYPE *)pComponentParameterStructure;
            unsigned i;
            BOMX_STRUCT_VALIDATE(pRole);
            BOMX_MSG(("SetParameter OMX_IndexParamStandardComponentRole '%s'", pRole->cRole));
            // The spec says this should reset the component to default setings for the role specified.
            // It is technically redundant for this component, changing the input codec has the same
            // effect - but this will also set the input codec accordingly.
            for ( i = 0; i < g_numRoles; i++ )
            {
                if ( !strcmp(g_roles[i], (const char *)pRole->cRole) )
                {
                    // Set input port to matching compression std.
                    OMX_VIDEO_PARAM_PORTFORMATTYPE format;
                    memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
                    BOMX_STRUCT_INIT(&format);
                    format.nPortIndex = m_videoPortBase;
                    format.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)g_roleCodec[i];
                    err = SetParameter(OMX_IndexParamVideoPortFormat, &format);
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
    case OMX_IndexParamVideoPortFormat:
        {
            BOMX_Port *pPort;
            OMX_VIDEO_PARAM_PORTFORMATTYPE *pFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
            BOMX_MSG(("SetParameter OMX_IndexParamVideoPortFormat"));
            BOMX_STRUCT_VALIDATE(pFormat);
            pPort = FindPortByIndex(pFormat->nPortIndex);
            if ( NULL == pPort )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            if ( pPort->GetDir() == OMX_DirInput )
            {
                BOMX_MSG(("Set Input Port Compression Format to %u (%#x)", pFormat->eCompressionFormat, pFormat->eCompressionFormat));
                err = BOMX_VideoDecoder_InitMimeType(pFormat->eCompressionFormat, m_inputMimeType);
                if ( err )
                {
                    return BOMX_ERR_TRACE(err);
                }
                // Per the OMX spec you are supposed to initialize the port defs to defaults when changing format
                // The only thing we change is the format itself though - the MIME type is set above.
                OMX_VIDEO_PORTDEFINITIONTYPE portDefs;
                memset(&portDefs, 0, sizeof(portDefs));
                portDefs.eCompressionFormat = pFormat->eCompressionFormat;
                portDefs.cMIMEType = m_inputMimeType;
                err = m_pVideoPorts[0]->SetPortFormat(pFormat, &portDefs);
                if ( err )
                {
                    return BOMX_ERR_TRACE(err);
                }
            }
            else
            {
                BOMX_MSG(("Set Output Port Color Format to %u (%#x)", pFormat->eColorFormat, pFormat->eColorFormat));
                // Per the OMX spec you are supposed to initialize the port defs to defaults when changing format
                // Leave buffer size parameters alone and update color format/framerate.
                OMX_VIDEO_PORTDEFINITIONTYPE portDefs;
                portDefs = pPort->GetDefinition()->format.video;
                portDefs.eCompressionFormat = pFormat->eCompressionFormat;
                portDefs.xFramerate = pFormat->xFramerate;
                portDefs.eColorFormat = pFormat->eColorFormat;
                /// TODO: Fix this to update entire port definition.
                err = m_pVideoPorts[1]->SetPortFormat(pFormat, &portDefs);
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
        BOMX_MSG(("SetParameter OMX_IndexParamPortDefinition"));
        BOMX_STRUCT_VALIDATE(pDef);
        if ( pDef->nPortIndex == m_videoPortBase && pDef->format.video.cMIMEType != m_inputMimeType )
        {
            BOMX_ERR(("Video input MIME type cannot be changed in the application"));
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        else if ( pDef->nPortIndex == (m_videoPortBase+1) && pDef->format.video.cMIMEType != m_outputMimeType )
        {
            BOMX_ERR(("Video output MIME type cannot be changed in the application"));
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        BOMX_Port *pPort = FindPortByIndex(pDef->nPortIndex);
        if ( NULL == pPort )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if ( pDef->nPortIndex == m_videoPortBase && pDef->format.video.eCompressionFormat != GetCodec() )
        {
            BOMX_ERR(("Video compression format cannot be changed in the port defintion.  Change Port Format instead."));
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        // Handle remainder in base class
        return BOMX_ERR_TRACE(BOMX_Component::SetParameter(nIndex, (OMX_PTR)pDef));
    }
    case OMX_IndexParamEnableAndroidNativeGraphicsBuffer:
    {
        EnableAndroidNativeBuffersParams *pEnableParams = (EnableAndroidNativeBuffersParams *)pComponentParameterStructure;
        bool oldValue;
        BOMX_STRUCT_VALIDATE(pEnableParams);
        BOMX_MSG(("SetParameter OMX_IndexParamEnableAndroidNativeGraphicsBuffer %u", pEnableParams->enable));
        if ( pEnableParams->nPortIndex != m_videoPortBase+1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        oldValue = m_nativeGraphicsEnabled;
        m_nativeGraphicsEnabled = pEnableParams->enable == OMX_TRUE ? true : false;

        // Mode has changed.  Set appropriate output color format.
        OMX_VIDEO_PARAM_PORTFORMATTYPE portFormat;
        BOMX_STRUCT_INIT(&portFormat);
        portFormat.nPortIndex = m_videoPortBase+1;
        portFormat.eCompressionFormat = OMX_VIDEO_CodingUnused;
        if ( m_nativeGraphicsEnabled )
        {
            // In this mode, the output color format should be an android HAL format.  Our gralloc only supports RGB888/565 variants.  Use 565 to save memory and bandwidth.
            portFormat.nIndex = 1;  // The second port format is the native format
            portFormat.eColorFormat = (OMX_COLOR_FORMATTYPE)((int)PIXEL_FORMAT_RGB_565);
        }
        else
        {
            // In this mode, use an OMX color format
            portFormat.nIndex = 0;  // The first port format is the traditional omx buffer format
            portFormat.eColorFormat = OMX_COLOR_Format16bitRGB565;
        }
        // Update port format to appropriate value for native vs. non-native output.
        err = SetParameter(OMX_IndexParamVideoPortFormat, &portFormat);
        if ( OMX_ErrorNone != err )
        {
            m_nativeGraphicsEnabled = oldValue;
            return BOMX_ERR_TRACE(err);
        }
        return OMX_ErrorNone;
    }
    case OMX_IndexParamStoreMetaDataInBuffers:
    {
        StoreMetaDataInBuffersParams *pMetadata = (StoreMetaDataInBuffersParams *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pMetadata);
        BOMX_MSG(("SetParameter OMX_IndexParamStoreMetaDataInBuffers %u", pMetadata->bStoreMetaData));
        if ( pMetadata->nPortIndex != m_videoPortBase+1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        m_metadataEnabled = pMetadata->bStoreMetaData == OMX_TRUE ? true : false;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamUseAndroidNativeBuffer:
    case OMX_IndexParamUseAndroidNativeBuffer2:
    {
        UseAndroidNativeBufferParams *pBufferParams = (UseAndroidNativeBufferParams *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pBufferParams);
        BOMX_MSG(("SetParameter OMX_IndexParamUseAndroidNativeBuffer"));
        if ( pBufferParams->nPortIndex != m_videoPortBase+1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        sp <ANativeWindowBuffer> nativeBuffer = pBufferParams->nativeBuffer;
        private_handle_t *handle = (private_handle_t *)nativeBuffer->handle;
        err = AddOutputPortBuffer(pBufferParams->bufferHeader, pBufferParams->nPortIndex, pBufferParams->pAppPrivate, handle);
        if ( OMX_ErrorNone != err )
        {
            return BOMX_ERR_TRACE(err);
        }
        return OMX_ErrorNone;
    }
    case OMX_IndexParamConfigureVideoTunnelMode:
    {
        ConfigureVideoTunnelModeParams *pTunnel = (ConfigureVideoTunnelModeParams *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pTunnel);
        return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
    }
    default:
        BOMX_MSG(("SetParameter %#x - Defer to base class", nIndex));
        return BOMX_ERR_TRACE(BOMX_Component::SetParameter(nIndex, pComponentParameterStructure));
    }
}

NEXUS_VideoCodec BOMX_VideoDecoder::GetNexusCodec()
{
    switch ( (int)GetCodec() )
    {
    case OMX_VIDEO_CodingMPEG2:
        return NEXUS_VideoCodec_eMpeg2;
    case OMX_VIDEO_CodingH263:
        return NEXUS_VideoCodec_eH263;
    case OMX_VIDEO_CodingMPEG4:
        return NEXUS_VideoCodec_eMpeg4Part2;
    case OMX_VIDEO_CodingAVC:
        return NEXUS_VideoCodec_eH264;
    case OMX_VIDEO_CodingVP8:
        return NEXUS_VideoCodec_eVp8;
    case OMX_VIDEO_CodingHEVC:
        return NEXUS_VideoCodec_eH265;
    default:
        BOMX_WRN(("Unknown video codec %u (%#x)", GetCodec(), GetCodec()));
        return NEXUS_VideoCodec_eNone;
    }
}

NEXUS_Error BOMX_VideoDecoder::SetInputPortState(OMX_STATETYPE newState)
{
    BOMX_MSG(("Setting Input Port State to %s", BOMX_StateName(newState)));
    // Loaded means stop and release all resources
    if ( newState == OMX_StateLoaded )
    {
        // Decoder is shutting down, don't poll for more frames
        if ( m_outputFrameTimerId )
        {
            CancelTimer(m_outputFrameTimerId);
            m_outputFrameTimerId = NULL;
        }
        // Invalidate queue of decoded frames
        InvalidateDecodedFrames();
        // Shutdown and free resources
        if ( m_hSimpleVideoDecoder )
        {
            NEXUS_Playpump_Stop(m_hPlaypump);
            NEXUS_SimpleVideoDecoder_Stop(m_hSimpleVideoDecoder);

            m_submittedDescriptors = 0;
            m_eosPending = false;
            m_eosDelivered = false;
            m_pBufferTracker->Flush();
            m_pIpcClient->disconnectClientResources(m_pNexusClient);
            m_pIpcClient->releaseVideoDecoderHandle(m_hSimpleVideoDecoder);
            m_hSimpleVideoDecoder = NULL;

            NEXUS_Playpump_ClosePidChannel(m_hPlaypump, m_hPidChannel);
            NEXUS_Playpump_Close(m_hPlaypump);
            UnregisterEvent(m_playpumpEventId);
            m_playpumpEventId = NULL;
            m_hPlaypump = NULL;
        }
    }
    else
    {
        NEXUS_VideoDecoderStatus vdecStatus;
        NEXUS_SimpleVideoDecoderStartSettings vdecStartSettings;
        NEXUS_VideoDecoderTrickState vdecTrickState;
        NEXUS_Error errCode;

        // All states other than loaded require a playpump and video decoder handle
        if ( NULL == m_hSimpleVideoDecoder )
        {
            NEXUS_VideoDecoderSettings vdecSettings;
            NEXUS_PlaypumpOpenSettings playpumpOpenSettings;
            NEXUS_PlaypumpSettings playpumpSettings;
            b_refsw_client_connect_resource_settings    connectSettings;
            b_refsw_client_client_info                  clientInfo;
            NEXUS_Error errCode;

            m_hSimpleVideoDecoder = m_pIpcClient->acquireVideoDecoderHandle();
            if ( NULL == m_hSimpleVideoDecoder )
            {
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }

            m_pIpcClient->getClientInfo(m_pNexusClient, &clientInfo);
            m_pIpcClient->getDefaultConnectClientSettings(&connectSettings);
            connectSettings.simpleVideoDecoder[0].id = clientInfo.videoDecoderId;
            connectSettings.simpleVideoDecoder[0].surfaceClientId = m_surfaceClientId;
            connectSettings.simpleVideoDecoder[0].windowId = 0; /* TODO: Hardcode to main for now */
            connectSettings.simpleVideoDecoder[0].windowCaps.type = eVideoWindowType_eMain;
            if ( (int)GetCodec() == OMX_VIDEO_CodingHEVC || (int)GetCodec() == OMX_VIDEO_CodingVP9 )
            {
                // Request 4k decoder for HEVC/VP9 only
                connectSettings.simpleVideoDecoder[0].decoderCaps.maxWidth = 3840;
                connectSettings.simpleVideoDecoder[0].decoderCaps.maxHeight = 2160;
                BOMX_WRN(("Requesting 4k decoder from nexus"));
            }
            if ( ! m_pIpcClient->connectClientResources(m_pNexusClient, &connectSettings) )
            {
                m_pIpcClient->disconnectClientResources(m_pNexusClient);
                m_pIpcClient->releaseVideoDecoderHandle(m_hSimpleVideoDecoder);
                m_hSimpleVideoDecoder = NULL;
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }

            NEXUS_Playpump_GetDefaultOpenSettings(&playpumpOpenSettings);
            playpumpOpenSettings.fifoSize = 0;
            playpumpOpenSettings.dataNotCpuAccessible = true;
            playpumpOpenSettings.numDescriptors = 2+(2*m_pVideoPorts[0]->GetDefinition()->nBufferCountActual);
            m_hPlaypump = NEXUS_Playpump_Open(NEXUS_ANY_ID, &playpumpOpenSettings);
            if ( NULL == m_hPlaypump )
            {
                m_pIpcClient->disconnectClientResources(m_pNexusClient);
                m_pIpcClient->releaseVideoDecoderHandle(m_hSimpleVideoDecoder);
                m_hSimpleVideoDecoder = NULL;
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }
            NEXUS_Playpump_GetSettings(m_hPlaypump, &playpumpSettings);
            playpumpSettings.transportType = NEXUS_TransportType_eMpeg2Pes;
            playpumpSettings.dataNotCpuAccessible = true;
            playpumpSettings.dataCallback.callback = BOMX_VideoDecoder_PlaypumpDataCallback;
            playpumpSettings.dataCallback.context = static_cast <void *> (m_hPlaypumpEvent);
            errCode = NEXUS_Playpump_SetSettings(m_hPlaypump, &playpumpSettings);
            if ( errCode )
            {
                NEXUS_Playpump_Close(m_hPlaypump);
                m_hPlaypump = NULL;
                m_pIpcClient->disconnectClientResources(m_pNexusClient);
                m_pIpcClient->releaseVideoDecoderHandle(m_hSimpleVideoDecoder);
                m_hSimpleVideoDecoder = NULL;
                return BOMX_BERR_TRACE(errCode);
            }

            NEXUS_PlaypumpOpenPidChannelSettings pidSettings;
            NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&pidSettings);
            pidSettings.pidType = NEXUS_PidType_eVideo;
            m_hPidChannel = NEXUS_Playpump_OpenPidChannel(m_hPlaypump, B_STREAM_ID, &pidSettings);
            if ( NULL == m_hPidChannel )
            {
                NEXUS_Playpump_Close(m_hPlaypump);
                m_hPlaypump = NULL;
                m_pIpcClient->disconnectClientResources(m_pNexusClient);
                m_pIpcClient->releaseVideoDecoderHandle(m_hSimpleVideoDecoder);
                m_hSimpleVideoDecoder = NULL;
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }
            m_playpumpEventId = RegisterEvent(m_hPlaypumpEvent, BOMX_VideoDecoder_PlaypumpEvent, static_cast <void *> (this));
            if ( NULL == m_playpumpEventId )
            {
                BOMX_ERR(("Unable to register playpump event"));
                NEXUS_Playpump_ClosePidChannel(m_hPlaypump, m_hPidChannel);
                m_hPidChannel = NULL;
                NEXUS_Playpump_Close(m_hPlaypump);
                m_hPlaypump = NULL;
                m_pIpcClient->disconnectClientResources(m_pNexusClient);
                m_pIpcClient->releaseVideoDecoderHandle(m_hSimpleVideoDecoder);
                m_hSimpleVideoDecoder = NULL;
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }

            // Intended for derived classes to use if additional settings for transport are
            // required
            errCode = ExtraTransportConfig();
            if ( errCode )
            {
                NEXUS_Playpump_ClosePidChannel(m_hPlaypump, m_hPidChannel);
                m_hPidChannel = NULL;
                NEXUS_Playpump_Close(m_hPlaypump);
                m_hPlaypump = NULL;
                m_pIpcClient->disconnectClientResources(m_pNexusClient);
                m_pIpcClient->releaseVideoDecoderHandle(m_hSimpleVideoDecoder);
                m_hSimpleVideoDecoder = NULL;
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }
        }

        (void)NEXUS_SimpleVideoDecoder_GetStatus(m_hSimpleVideoDecoder, &vdecStatus);
        switch ( newState )
        {
        case OMX_StateIdle:
            if ( vdecStatus.started )
            {
                // Executing/Paused -> Idle = Stop
                if ( m_outputFrameTimerId )
                {
                    CancelTimer(m_outputFrameTimerId);
                    m_outputFrameTimerId = NULL;
                }
                // Invalidate queue of decoded frames
                InvalidateDecodedFrames();
                NEXUS_Playpump_Stop(m_hPlaypump);
                NEXUS_SimpleVideoDecoder_Stop(m_hSimpleVideoDecoder);
                m_eosPending = false;
                m_eosDelivered = false;
                m_pBufferTracker->Flush();
                ReturnPortBuffers(m_pVideoPorts[0]);
                m_submittedDescriptors = 0;
            }
            break;
        case OMX_StateExecuting:
            if ( vdecStatus.started )
            {
                // Paused -> Executing = Resume
                NEXUS_SimpleVideoDecoder_GetTrickState(m_hSimpleVideoDecoder, &vdecTrickState);
                vdecTrickState.rate = NEXUS_NORMAL_DECODE_RATE;
                errCode = NEXUS_SimpleVideoDecoder_SetTrickState(m_hSimpleVideoDecoder, &vdecTrickState);
                if ( errCode )
                {
                    return BOMX_BERR_TRACE(errCode);
                }
            }
            else
            {
                // Idle -> Executing = Start
                NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(&vdecStartSettings);
                vdecStartSettings.displayEnabled = (m_outputMode == BOMX_VideoDecoderOutputBufferType_eStandard) ? false : true;  // If we're doing video-as-graphics don't fire up the display path
                vdecStartSettings.settings.appDisplayManagement = true;
                vdecStartSettings.settings.stcChannel = NULL;
                vdecStartSettings.settings.pidChannel = m_hPidChannel;
                vdecStartSettings.settings.codec = GetNexusCodec();
                BOMX_MSG(("Start Decoder display %u appDM %u codec %u", vdecStartSettings.displayEnabled, vdecStartSettings.settings.appDisplayManagement, vdecStartSettings.settings.codec));
                if ( vdecStartSettings.settings.codec == NEXUS_VideoCodec_eH265 )
                {
                    // Request 4k decoder for HEVC/VP9 only
                    vdecStartSettings.maxWidth = 3840;
                    vdecStartSettings.maxHeight = 2160;
                }
                errCode = NEXUS_SimpleVideoDecoder_Start(m_hSimpleVideoDecoder, &vdecStartSettings);
                if ( errCode )
                {
                    return BOMX_BERR_TRACE(errCode);
                }

                m_submittedDescriptors = 0;
                errCode = NEXUS_Playpump_Start(m_hPlaypump);
                if ( errCode )
                {
                    NEXUS_SimpleVideoDecoder_Stop(m_hSimpleVideoDecoder);
                    m_eosPending = false;
                    m_eosDelivered = false;
                    return BOMX_BERR_TRACE(errCode);
                }

                if ( m_pVideoPorts[1]->IsEnabled() )
                {
                    // Kick off the decoder frame queue
                    OutputFrameEvent();
                }
            }
            break;
        case OMX_StatePause:
            if ( vdecStatus.started )
            {
                // Executing -> Paused = Pause
                NEXUS_SimpleVideoDecoder_GetTrickState(m_hSimpleVideoDecoder, &vdecTrickState);
                vdecTrickState.rate = 0;
                errCode = NEXUS_SimpleVideoDecoder_SetTrickState(m_hSimpleVideoDecoder, &vdecTrickState);
                if ( errCode )
                {
                    return BOMX_BERR_TRACE(errCode);
                }
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

NEXUS_Error BOMX_VideoDecoder::SetOutputPortState(OMX_STATETYPE newState)
{
    // The Output port for video decoder is a logical construct and not
    // related to the actual decoder output except through an indirect
    // Queue.  For format changes, we need to be able to control this logical
    // output port independently and leave the input port active during any
    // Resolution Change.
    BOMX_MSG(("Setting Output Port State to %s", BOMX_StateName(newState)));
    if ( newState == OMX_StateLoaded )
    {
        if ( m_outputFrameTimerId )
        {
            CancelTimer(m_outputFrameTimerId);
            m_outputFrameTimerId = NULL;
        }
        // Invalidate queue of decoded frames
        InvalidateDecodedFrames();
        m_formatChangePending = false;
    }
    else
    {
        // For any other state change, kick off the frame check if the decoder is running
        if ( newState == OMX_StateIdle )
        {
            // Return all pending buffers to the client
            ReturnPortBuffers(m_pVideoPorts[1]);
        }
        else if ( m_hSimpleVideoDecoder )
        {
            NEXUS_VideoDecoderStatus vdecStatus;
            // If the video decoder is runnign, generate an output frame event to poll for data
            NEXUS_SimpleVideoDecoder_GetStatus(m_hSimpleVideoDecoder, &vdecStatus);
            if ( vdecStatus.started )
            {
                OutputFrameEvent();
            }
        }
    }
    return NEXUS_SUCCESS;
}

OMX_ERRORTYPE BOMX_VideoDecoder::CommandStateSet(
    OMX_STATETYPE newState,
    OMX_STATETYPE oldState)
{
    OMX_ERRORTYPE err;
    NEXUS_Error errCode;

    BOMX_MSG(("Begin State Change %s->%s", BOMX_StateName(oldState), BOMX_StateName(newState)));

    switch ( newState )
    {
    case OMX_StateLoaded:
    {
        // If we are returning to loaded, we need to first flush all enabled ports and return their buffers
        bool inputPortEnabled, outputPortEnabled;
        BOMX_VideoDecoderFrameBuffer *pFrameBuffer;

        // Save port state
        inputPortEnabled = m_pVideoPorts[0]->IsEnabled();
        outputPortEnabled = m_pVideoPorts[1]->IsEnabled();

        // Disable all ports
        err = CommandPortDisable(OMX_ALL);
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }

        // Now all resources have been released.  Reset port enable state
        if ( inputPortEnabled )
        {
            (void)m_pVideoPorts[0]->Enable();
        }
        if ( outputPortEnabled )
        {
            (void)m_pVideoPorts[1]->Enable();
        }

        // Make sure the output buffer list is cleared.  In Metadata mode, not all buffers will be freed automatically.
        while ( NULL != (pFrameBuffer = BLST_Q_FIRST(&m_frameBufferAllocList)) )
        {
            BLST_Q_REMOVE_HEAD(&m_frameBufferAllocList, node);
            BLST_Q_INSERT_TAIL(&m_frameBufferFreeList, pFrameBuffer, node);
        }

        BOMX_MSG(("End State Change %s->%s", BOMX_StateName(oldState), BOMX_StateName(newState)));
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
            BOMX_MSG(("Waiting for port population..."));
            PortWaitBegin();
            // Now we need to wait for all enabled ports to become populated
            while ( (m_pVideoPorts[0]->IsEnabled() && !m_pVideoPorts[0]->IsPopulated()) ||
                    (m_pVideoPorts[1]->IsEnabled() && !m_pVideoPorts[1]->IsPopulated()) )
            {
                if ( PortWait() != B_ERROR_SUCCESS )
                {
                    BOMX_ERR(("Timeout waiting for ports to be populated"));
                    PortWaitEnd();
                    m_pIpcClient->disconnectClientResources(m_pNexusClient);
                    m_pIpcClient->releaseVideoDecoderHandle(m_hSimpleVideoDecoder);
                    m_hSimpleVideoDecoder = NULL;
                    return BOMX_ERR_TRACE(OMX_ErrorTimeout);
                }
            }
            PortWaitEnd();
            BOMX_MSG(("Done waiting for port population"));

            bool inputEnabled = m_pVideoPorts[0]->IsEnabled();
            bool outputEnabled = m_pVideoPorts[1]->IsEnabled();
            if ( m_pVideoPorts[1]->IsPopulated() && m_pVideoPorts[1]->IsEnabled() )
            {
                errCode = SetOutputPortState(OMX_StateIdle);
                if ( errCode )
                {
                    (void)CommandPortDisable(OMX_ALL);
                    if ( inputEnabled ) {m_pVideoPorts[0]->Enable();}
                    if ( outputEnabled ) {m_pVideoPorts[1]->Enable();}
                    return BOMX_ERR_TRACE(OMX_ErrorUndefined);
                }
            }
            if ( m_pVideoPorts[0]->IsPopulated() && m_pVideoPorts[0]->IsEnabled() )
            {
                errCode = SetInputPortState(OMX_StateIdle);
                if ( errCode )
                {
                    (void)CommandPortDisable(OMX_ALL);
                    if ( inputEnabled ) {m_pVideoPorts[0]->Enable();}
                    if ( outputEnabled ) {m_pVideoPorts[1]->Enable();}
                    return BOMX_ERR_TRACE(OMX_ErrorUndefined);
                }
            }
        }
        else
        {
            // Transitioning to idle from any state except loaded is equivalent to stop
            if ( m_pVideoPorts[0]->IsPopulated() )
            {
                (void)SetInputPortState(OMX_StateIdle);
            }
            if ( m_pVideoPorts[1]->IsPopulated() )
            {
                (void)SetOutputPortState(OMX_StateIdle);
            }
        }
        BOMX_MSG(("End State Change %s->%s", BOMX_StateName(oldState), BOMX_StateName(newState)));
        return OMX_ErrorNone;
    }
    case OMX_StateExecuting:
    case OMX_StatePause:
        if ( m_pVideoPorts[1]->IsPopulated() && m_pVideoPorts[1]->IsEnabled() )
        {
            errCode = SetOutputPortState(newState);
            if ( errCode )
            {
                return BOMX_ERR_TRACE(OMX_ErrorUndefined);
            }
        }
        if ( m_pVideoPorts[0]->IsPopulated() && m_pVideoPorts[0]->IsEnabled() )
        {
            errCode = SetInputPortState(newState);
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

OMX_ERRORTYPE BOMX_VideoDecoder::CommandFlush(
    OMX_U32 portIndex)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    if ( StateChangeInProgress() )
    {
        BOMX_ERR(("Flush should not be called during a state change"));
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
    }

    // Handle case for OMX_ALL by calling flush on each port
    if ( portIndex == OMX_ALL )
    {
        BOMX_MSG(("Flushing all ports"));

        err = CommandFlush(m_videoPortBase);
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }
        err = CommandFlush(m_videoPortBase+1);
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
        if ( portIndex == m_videoPortBase )
        {
            // Input port
            if ( m_hSimpleVideoDecoder )
            {
                (void)SetInputPortState(OMX_StateIdle);
                (void)SetInputPortState(StateGet());
                m_configRequired = true;
            }
        }
        else
        {
            // Output port
            if ( m_pVideoPorts[1]->IsEnabled() && m_pVideoPorts[1]->IsPopulated() )
            {
                (void)SetOutputPortState(OMX_StateIdle);
                (void)SetOutputPortState(StateGet());
            }
        }
        ReturnPortBuffers(pPort);
    }

    return err;
}

OMX_ERRORTYPE BOMX_VideoDecoder::CommandPortEnable(
    OMX_U32 portIndex)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    // Handle case for OMX_ALL by calling enable on each port
    if ( portIndex == OMX_ALL )
    {
        BOMX_MSG(("Enabling all ports"));

        // Enable output first
        if ( !m_pVideoPorts[1]->IsEnabled() )
        {
            err = CommandPortEnable(m_videoPortBase+1);
            if ( err )
            {
                return BOMX_ERR_TRACE(err);
            }
        }
        // Enable input
        if ( !m_pVideoPorts[0]->IsEnabled() )
        {
            err = CommandPortEnable(m_videoPortBase);
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

OMX_ERRORTYPE BOMX_VideoDecoder::CommandPortDisable(
    OMX_U32 portIndex)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    // Handle case for OMX_ALL by calling enable on each port
    if ( portIndex == OMX_ALL )
    {
        BOMX_MSG(("Disabling all ports"));

        if ( m_pVideoPorts[0]->IsEnabled() )
        {
            err = CommandPortDisable(m_videoPortBase);
            if ( err )
            {
                return BOMX_ERR_TRACE(err);
            }
        }
        if ( m_pVideoPorts[1]->IsEnabled() )
        {
            err = CommandPortDisable(m_videoPortBase+1);
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
            NEXUS_Error errCode;
            // Release port resources
            if ( pPort->GetDir() == OMX_DirInput )
            {
                errCode = SetInputPortState(OMX_StateLoaded);
                BOMX_ASSERT(errCode == NEXUS_SUCCESS);
            }
            else
            {
                errCode = SetOutputPortState(OMX_StateLoaded);
                BOMX_ASSERT(errCode == NEXUS_SUCCESS);
            }

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

OMX_ERRORTYPE BOMX_VideoDecoder::AddInputPortBuffer(
        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
        OMX_IN OMX_U32 nPortIndex,
        OMX_IN OMX_PTR pAppPrivate,
        OMX_IN OMX_U32 nSizeBytes,
        OMX_IN OMX_U8* pBuffer,
        bool componentAllocated)
{
    BOMX_Port *pPort;
    BOMX_VideoDecoderInputBufferInfo *pInfo;
    OMX_ERRORTYPE err;
    NEXUS_Error errCode;
    NEXUS_ClientConfiguration               clientConfig;
    NEXUS_MemoryAllocationSettings          memorySettings;

    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    NEXUS_Memory_GetDefaultAllocationSettings(&memorySettings);
    memorySettings.heap = clientConfig.heap[1];

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
        BOMX_ERR(("Cannot add buffers to a tunneled port"));
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }

    if ( pPort->IsEnabled() && StateGet() != OMX_StateLoaded )
    {
        BOMX_ERR(("Can only add buffers to an enabled port while transitioning from loaded->idle (state=%u)", StateGet()));
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
    }

    pInfo = new BOMX_VideoDecoderInputBufferInfo;
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    errCode = NEXUS_Memory_Allocate(B_HEADER_BUFFER_SIZE, &memorySettings, &pInfo->pHeader);
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

OMX_ERRORTYPE BOMX_VideoDecoder::AddOutputPortBuffer(
        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
        OMX_IN OMX_U32 nPortIndex,
        OMX_IN OMX_PTR pAppPrivate,
        OMX_IN OMX_U32 nSizeBytes,
        OMX_IN OMX_U8* pBuffer,
        bool componentAllocated)
{
    BOMX_Port *pPort;
    BOMX_VideoDecoderOutputBufferInfo *pInfo;
    OMX_ERRORTYPE err;
    NEXUS_Error errCode;

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
        BOMX_ERR(("Cannot add buffers to a tunneled port"));
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }

    if ( pPort->IsEnabled() && StateGet() != OMX_StateLoaded )
    {
        BOMX_ERR(("Can only add buffers to an enabled port while transitioning from loaded->idle (state=%u)", StateGet()));
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
    }

    pInfo = new BOMX_VideoDecoderOutputBufferInfo;
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    pInfo->pFrameBuffer = NULL;
    pInfo->type = m_outputMode;
    switch ( m_outputMode )
    {
    case BOMX_VideoDecoderOutputBufferType_eStandard:
        {
            NEXUS_SurfaceCreateSettings surfaceSettings;
            NEXUS_SurfaceMemory surfaceMem;
            const OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;

            /* Check buffer size */
            pPortDef = pPort->GetDefinition();
            if ( nSizeBytes < 2*pPortDef->format.video.nFrameWidth*pPortDef->format.video.nFrameHeight )
            {
                BOMX_ERR(("Outbuffer size is not sufficient - must be at least %u bytes (2*%u*%u) got %u [eColorFormat %#x]", 2*pPortDef->format.video.nFrameWidth*pPortDef->format.video.nFrameHeight,
                    pPortDef->format.video.nFrameWidth, pPortDef->format.video.nFrameHeight, nSizeBytes, pPortDef->format.video.eColorFormat));
                delete pInfo;
                return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
            }
            /* Create Surface (Android has limited 422 support so use this for now) */
            NEXUS_Surface_GetDefaultCreateSettings(&surfaceSettings);
            surfaceSettings.pixelFormat = NEXUS_PixelFormat_eR5_G6_B5;
            surfaceSettings.width = pPortDef->format.video.nFrameWidth;
            surfaceSettings.height = pPortDef->format.video.nFrameHeight;
            surfaceSettings.pitch = pPortDef->format.video.nStride;
            pInfo->typeInfo.standard.hSurface = NEXUS_Surface_Create(&surfaceSettings);
            if ( NULL == pInfo->typeInfo.standard.hSurface )
            {
                delete pInfo;
                return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
            }
            NEXUS_Surface_GetMemory(pInfo->typeInfo.standard.hSurface, &surfaceMem);
            pInfo->typeInfo.standard.pSurfaceMemory = surfaceMem.buffer;
            if ( componentAllocated )
            {
                // Client will directly get a pointer to the nexus surface memory
                pInfo->typeInfo.standard.pClientMemory = NULL;
                pBuffer = (OMX_U8 *)surfaceMem.buffer;
            }
            else
            {
                // Client has its own buffer we must CPU-copy into
                pInfo->typeInfo.standard.pClientMemory = pBuffer;
            }
        }
        break;
    default:
        BOMX_ERR(("Unsupported buffer type"));
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    err = pPort->AddBuffer(ppBufferHdr, pAppPrivate, nSizeBytes, pBuffer, pInfo, componentAllocated);
    if ( OMX_ErrorNone != err )
    {
        switch ( pInfo->type )
        {
        case BOMX_VideoDecoderOutputBufferType_eStandard:
            NEXUS_Surface_Destroy(pInfo->typeInfo.standard.hSurface);
            break;
        default:
            break;
        }
        delete pInfo;
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    PortStatusChanged();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_VideoDecoder::AddOutputPortBuffer(
        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
        OMX_IN OMX_U32 nPortIndex,
        OMX_IN OMX_PTR pAppPrivate,
        private_handle_t *pPrivateHandle)
{
    BOMX_Port *pPort;
    BOMX_VideoDecoderOutputBufferInfo *pInfo;
    OMX_ERRORTYPE err;

   if ( NULL == ppBufferHdr || NULL == pPrivateHandle )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    if ( m_outputMode != BOMX_VideoDecoderOutputBufferType_eNative )
    {
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
    }

    pPort = FindPortByIndex(nPortIndex);
    if ( NULL == pPort || pPort->GetDir() != OMX_DirOutput )
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

    pInfo = new BOMX_VideoDecoderOutputBufferInfo;
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    pInfo->pFrameBuffer = NULL;
    pInfo->type = BOMX_VideoDecoderOutputBufferType_eNative;
    pInfo->typeInfo.native.pPrivateHandle = pPrivateHandle;
    pInfo->typeInfo.native.pSharedData = (SHARED_DATA *)NEXUS_OffsetToCachedAddr(pPrivateHandle->sharedData);
    if ( NULL == pInfo->typeInfo.native.pSharedData )
    {
        delete pInfo;
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    // Setup window parameters for display
    pInfo->typeInfo.native.pSharedData->videoWindow.nexusClientContext = m_pNexusClient;
    android_atomic_release_store(1, &pInfo->typeInfo.native.pSharedData->videoWindow.windowIdPlusOne);

     err = pPort->AddBuffer(ppBufferHdr, pAppPrivate, pPort->GetDefinition()->format.video.nFrameWidth*pPort->GetDefinition()->format.video.nFrameHeight*2, (OMX_U8 *)pPrivateHandle, pInfo, false);
    if ( OMX_ErrorNone != err )
    {
        delete pInfo;
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    PortStatusChanged();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_VideoDecoder::AddOutputPortBuffer(
        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
        OMX_IN OMX_U32 nPortIndex,
        OMX_IN OMX_PTR pAppPrivate,
        VideoDecoderOutputMetaData *pMetadata)
{
    BOMX_Port *pPort;
    BOMX_VideoDecoderOutputBufferInfo *pInfo;
    OMX_ERRORTYPE err;

   if ( NULL == ppBufferHdr || NULL == pMetadata )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    if ( m_outputMode != BOMX_VideoDecoderOutputBufferType_eMetadata )
    {
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
    }

    pPort = FindPortByIndex(nPortIndex);
    if ( NULL == pPort || pPort->GetDir() != OMX_DirOutput )
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

    pInfo = new BOMX_VideoDecoderOutputBufferInfo;
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    pInfo->pFrameBuffer = NULL;
    pInfo->type = BOMX_VideoDecoderOutputBufferType_eMetadata;
    pInfo->typeInfo.metadata.pMetadata = pMetadata;

    err = pPort->AddBuffer(ppBufferHdr, pAppPrivate, sizeof(VideoDecoderOutputMetaData), (OMX_U8 *)pMetadata, pInfo, false);
    if ( OMX_ErrorNone != err )
    {
        delete pInfo;
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    PortStatusChanged();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_VideoDecoder::UseBuffer(
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

    if ( nPortIndex == m_videoPortBase )
    {
        err = AddInputPortBuffer(ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes, pBuffer, false);
        if ( err != OMX_ErrorNone )
        {
            return BOMX_ERR_TRACE(err);
        }
    }
    else
    {
        // Figure out output buffer mode from SetParameter calls.
        // You must do this here because the component case class state checks
        // will ensure this is valid (only port disabled or loaded state)
        // and if you try and do it in the command handler there is a possible
        // race condition where the component thread is scheduled later
        // than the call to EnablePort or CommandStateSet and thsi will
        // come first.  It's always safe to do it here.
        if ( m_metadataEnabled )
        {
            BOMX_MSG(("Selecting metadata output mode for output buffer %#x", pBuffer));
            m_outputMode = BOMX_VideoDecoderOutputBufferType_eMetadata;
        }
        else if ( m_nativeGraphicsEnabled )
        {
            BOMX_MSG(("Selecting native graphics output mode for output buffer %#x", pBuffer));
            m_outputMode = BOMX_VideoDecoderOutputBufferType_eNative;
        }
        else
        {
            BOMX_MSG(("Selecting standard buffer output mode for output buffer %#x", pBuffer));
            m_outputMode = BOMX_VideoDecoderOutputBufferType_eStandard;
        }
        switch ( m_outputMode )
        {
        case BOMX_VideoDecoderOutputBufferType_eStandard:
            err = AddOutputPortBuffer(ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes, pBuffer, false);
            if ( err != OMX_ErrorNone )
            {
                return BOMX_ERR_TRACE(err);
            }
            break;
        case BOMX_VideoDecoderOutputBufferType_eNative:
            {
                err = AddOutputPortBuffer(ppBufferHdr, nPortIndex, pAppPrivate, (private_handle_t *)pBuffer);
                if ( err != OMX_ErrorNone )
                {
                    return BOMX_ERR_TRACE(err);
                }
            }
            break;
        case BOMX_VideoDecoderOutputBufferType_eMetadata:
            {
                err = AddOutputPortBuffer(ppBufferHdr, nPortIndex, pAppPrivate, (VideoDecoderOutputMetaData *)pBuffer);
                if ( err != OMX_ErrorNone )
                {
                    return BOMX_ERR_TRACE(err);
                }
            }
            break;
        default:
            // TODO: Handle metadata
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
    }

    return OMX_ErrorNone;
}


OMX_ERRORTYPE BOMX_VideoDecoder::AllocateBuffer(
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

    if ( nPortIndex == m_videoPortBase )
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
        if ( m_metadataEnabled )
        {
            BOMX_MSG(("Selecting metadata output mode for output buffer %#x", ppBuffer));
            m_outputMode = BOMX_VideoDecoderOutputBufferType_eMetadata;
        }
        else if ( m_nativeGraphicsEnabled )
        {
            BOMX_MSG(("Selecting native graphics output mode for output buffer %#x", ppBuffer));
            m_outputMode = BOMX_VideoDecoderOutputBufferType_eNative;
        }
        else
        {
            BOMX_MSG(("Selecting standard buffer output mode for output buffer %#x", ppBuffer));
            m_outputMode = BOMX_VideoDecoderOutputBufferType_eStandard;
        }

        if ( m_outputMode == BOMX_VideoDecoderOutputBufferType_eStandard )
        {
            err = AddOutputPortBuffer(ppBuffer, nPortIndex, pAppPrivate, nSizeBytes, (OMX_U8 *)NULL, true);
            if ( err != OMX_ErrorNone )
            {
                return BOMX_ERR_TRACE(err);
            }
        }
        else
        {
            BOMX_ERR(("AllocateBuffer is not supported for native or metadata buffer modes"));
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_VideoDecoder::FreeBuffer(
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
        BOMX_VideoDecoderInputBufferInfo *pInfo;
        pInfo = (BOMX_VideoDecoderInputBufferInfo *)pBuffer->GetComponentPrivate();
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
            NEXUS_Memory_Free(pInfo->pPayload);
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
        BOMX_VideoDecoderOutputBufferInfo *pInfo;
        BOMX_VideoDecoderFrameBuffer *pFrameBuffer=NULL;
        pInfo = (BOMX_VideoDecoderOutputBufferInfo *)pBuffer->GetComponentPrivate();
        if ( NULL == pInfo )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        err = pPort->RemoveBuffer(pBufferHeader);
        if ( err != OMX_ErrorNone )
        {
            return BOMX_ERR_TRACE(err);
        }
        switch ( pInfo->type )
        {
        case BOMX_VideoDecoderOutputBufferType_eStandard:
            pFrameBuffer = pInfo->pFrameBuffer;
            NEXUS_Surface_Destroy(pInfo->typeInfo.standard.hSurface);
            break;
        case BOMX_VideoDecoderOutputBufferType_eNative:
            pFrameBuffer = pInfo->pFrameBuffer;
            break;
        case BOMX_VideoDecoderOutputBufferType_eMetadata:
            if ( pInfo->typeInfo.metadata.pMetadata->pHandle )
            {
                pFrameBuffer = FindFrameBuffer((private_handle_t *)pInfo->typeInfo.metadata.pMetadata->pHandle);
            }
            else
            {
                pFrameBuffer = NULL;
            }
            break;
        default:
            break;
        }
        pInfo->pFrameBuffer = NULL;
        if ( pFrameBuffer )
        {
            bool active=false;
            pFrameBuffer->pBufferInfo = NULL;
            pFrameBuffer->pPrivateHandle = NULL;
            if ( m_hSimpleVideoDecoder )
            {
                NEXUS_VideoDecoderStatus vdecStatus;
                NEXUS_SimpleVideoDecoder_GetStatus(m_hSimpleVideoDecoder, &vdecStatus);
                active = vdecStatus.started;
            }
            if ( active )
            {
                // Drop the buffer in the decoder
                pFrameBuffer->state = BOMX_VideoDecoderFrameBufferState_eDropReady;
                ReturnDecodedFrames();
            }
            else
            {
                // Free up the buffer (decoder is already stopped)
                BLST_Q_REMOVE(&m_frameBufferAllocList, pFrameBuffer, node);
                BLST_Q_INSERT_TAIL(&m_frameBufferFreeList, pFrameBuffer, node);
            }
            pFrameBuffer = NULL;
        }

        delete pInfo;
    }
    PortStatusChanged();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_VideoDecoder::EmptyThisBuffer(
    OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    OMX_ERRORTYPE err;
    BOMX_Buffer *pBuffer;
    BOMX_VideoDecoderInputBufferInfo *pInfo;
    size_t payloadLen;
    void *pPayload;
    NEXUS_PlaypumpScatterGatherDescriptor desc[2];
    NEXUS_Error errCode;
    size_t numConsumed, numRequested;
    uint8_t *pCodecHeader = NULL;
    size_t codecHeaderLength = 0;
    uint32_t pts=0, *pPts = NULL;

    if ( NULL == pBufferHeader )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    pBuffer = BOMX_BUFFERHEADER_TO_BUFFER(pBufferHeader);
    if ( NULL == pBuffer || pBuffer->GetDir() != OMX_DirInput )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    if ( pBufferHeader->nInputPortIndex != m_videoPortBase )
    {
        BOMX_ERR(("Invalid buffer"));
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    pInfo = (BOMX_VideoDecoderInputBufferInfo *)pBuffer->GetComponentPrivate();
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    pInfo->numDescriptors = 0;

    ALOGV("%s, comp:%s, buff:%p", __FUNCTION__, GetName(), pBufferHeader->pBuffer);
    if ( pBufferHeader->nFlags & OMX_BUFFERFLAG_CODECCONFIG )
    {
        if ( m_configRequired )
        {
            // If the app re-sends config data after flush invalidate what we have already stored.
            // Otherwise random actions may overflow the config buffer.
            BOMX_MSG(("Invalidating cached config buffer - application is resending after flush."));
            ConfigBufferInit();
        }

        BOMX_MSG(("Accumulating %u bytes of codec config data", pBufferHeader->nFilledLen - pBufferHeader->nOffset));

        err = ConfigBufferAppend(pBufferHeader->pBuffer + pBufferHeader->nOffset, pBufferHeader->nFilledLen - pBufferHeader->nOffset);
        if ( err != OMX_ErrorNone )
        {
            return BOMX_ERR_TRACE(err);
        }
        // If we're actively sending config ignore a previous flush
        m_configRequired = false;
    }

    // Check if there is a previous flush and send codec config if reuqired
    if( m_configRequired )
    {
        err = SendConfigBuffertoHw(pInfo);
        if ( err != OMX_ErrorNone )
                return BOMX_ERR_TRACE(err);

        m_configRequired = false;
        if ( NULL != g_pDebugFile )
        {
            fwrite(m_pConfigBuffer, 1, m_configBufferSize, g_pDebugFile);
        }
    }

    // Add codec-specific header if required
    switch ( (int)GetCodec() )
    {
    case OMX_VIDEO_CodingVP8:
    case OMX_VIDEO_CodingVP9:
    /* Also true for spark and possibly other codecs */
    {
        uint32_t packetLen = pBufferHeader->nFilledLen - pBufferHeader->nOffset;
        if ( packetLen > 0 )
        {
            pCodecHeader = m_pBcmvHeader;
            codecHeaderLength = BOMX_BCMV_HEADER_SIZE;
            packetLen += codecHeaderLength;   // BCMV packet length must include BCMV header
            pCodecHeader[4] = (packetLen>>24)&0xff;
            pCodecHeader[5] = (packetLen>>16)&0xff;
            pCodecHeader[6] = (packetLen>>8)&0xff;
            pCodecHeader[7] = (packetLen>>0)&0xff;
        }
        break;
    }
    default:
        break;
    }

    if ( m_pBufferTracker->Add(pBufferHeader, &pts) )
    {
        pPts = &pts;
    }
    else
    {
        BOMX_WRN(("Unable to track buffer"));
    }

    // Form PES Header
    if ( BOMX_FormPesHeader(pBufferHeader, pPts, pInfo->pHeader, B_HEADER_BUFFER_SIZE, B_STREAM_ID, pCodecHeader, codecHeaderLength, &pInfo->headerLen) )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    if ( NULL != g_pDebugFile )
    {
        fwrite(pInfo->pHeader, 1, pInfo->headerLen, g_pDebugFile);
    }

    // Send data to HW
    err = SendDataBuffertoHw(pInfo, pBufferHeader);
    if ( err != OMX_ErrorNone )
        return BOMX_ERR_TRACE(err);

    if ( pBufferHeader->nFlags & OMX_BUFFERFLAG_EOS )
    {
        numRequested = 1;
        desc[0].addr = m_pEosBuffer;
        desc[0].length = BOMX_VIDEO_EOS_LEN;
        errCode = NEXUS_Playpump_SubmitScatterGatherDescriptor(m_hPlaypump, desc, numRequested, &numConsumed);
        if ( errCode )
        {
            return BOMX_ERR_TRACE(OMX_ErrorUndefined);
        }
        BOMX_ASSERT(numConsumed == numRequested);
        pInfo->numDescriptors += numRequested;
        m_submittedDescriptors += numConsumed;
        m_eosPending = true;
        if ( NULL != g_pDebugFile )
        {
            fwrite(m_pEosBuffer, 1, BOMX_VIDEO_EOS_LEN, g_pDebugFile);
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_VideoDecoder::FillThisBuffer(
    OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    BOMX_Buffer *pBuffer;
    BOMX_VideoDecoderOutputBufferInfo *pInfo;
    BOMX_VideoDecoderFrameBuffer *pFrameBuffer;
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
    pInfo = (BOMX_VideoDecoderOutputBufferInfo *)pBuffer->GetComponentPrivate();
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    if ( pInfo->type == BOMX_VideoDecoderOutputBufferType_eMetadata )
    {
        if ( pInfo->typeInfo.metadata.pMetadata->eType != kMetadataBufferTypeGrallocSource )
        {
            BOMX_ERR(("Only kMetadataBufferTypeGrallocSource buffers are supported in metadata mode."));
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }

        SHARED_DATA *pSharedData;
        private_handle_t *pPrivateHandle = (private_handle_t *)pInfo->typeInfo.metadata.pMetadata->pHandle;
        pSharedData = (SHARED_DATA *)NEXUS_OffsetToCachedAddr(pPrivateHandle->sharedData);
        if ( NULL == pSharedData )
        {
            BOMX_ERR(("Invalid gralloc buffer %#x - sharedDataPhyAddr %#x is invalid", pPrivateHandle, pPrivateHandle->sharedData));
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        BOMX_ASSERT((pInfo->typeInfo.metadata.pMetadata == (void *)pBufferHeader->pBuffer));
        if ( NULL != pInfo->typeInfo.metadata.pMetadata->pHandle )
        {
            pFrameBuffer = FindFrameBuffer((private_handle_t *)pInfo->typeInfo.metadata.pMetadata->pHandle);
        }
        else
        {
            pFrameBuffer = NULL;
        }
    }
    else
    {
        pFrameBuffer = pInfo->pFrameBuffer;
    }
    pInfo->pFrameBuffer = NULL;
    BOMX_MSG(("Fill Buffer, comp:%s ts %u us serial %u pInfo %#x HDR %#x", GetName(), (unsigned int)pBufferHeader->nTimeStamp, pFrameBuffer ? pFrameBuffer->frameStatus.serialNumber : -1, pInfo, pBufferHeader));
    // Determine what to do with the buffer
    if ( pFrameBuffer )
    {
        pFrameBuffer->pBufferInfo = NULL;
        pFrameBuffer->pPrivateHandle = NULL;
        if ( pFrameBuffer->state == BOMX_VideoDecoderFrameBufferState_eInvalid )
        {
            // The frame has been flushed while the app owned it.  Move it back to the free list silently.
            BOMX_MSG(("Invalid FrameBuffer - Return to free list"));
            BLST_Q_REMOVE(&m_frameBufferAllocList, pFrameBuffer, node);
            BLST_Q_INSERT_TAIL(&m_frameBufferFreeList, pFrameBuffer, node);
        }
        else
        {
            // Any state other than delivered and we've done something horribly wrong.
            BOMX_ASSERT(pFrameBuffer->state == BOMX_VideoDecoderFrameBufferState_eDelivered);
            pFrameBuffer->state = BOMX_VideoDecoderFrameBufferState_eDropReady;
            switch ( pInfo->type )
            {
            case BOMX_VideoDecoderOutputBufferType_eStandard:
                BOMX_MSG(("Standard Buffer - Drop"));
                break;
            case BOMX_VideoDecoderOutputBufferType_eNative:
                BOMX_MSG(("Native Buffer %u - Drop", pInfo->typeInfo.native.pSharedData->DisplayFrame.frameStatus.serialNumber));
                break;
            case BOMX_VideoDecoderOutputBufferType_eMetadata:
                BOMX_MSG(("Metadata Buffer %u - Drop", pFrameBuffer->frameStatus.serialNumber));
                break;
            default:
                break;
            }
        }
    }
    else
    {
        BOMX_MSG(("FrameBuffer not set"));
    }
    pBuffer->Reset();
    err = m_pVideoPorts[1]->QueueBuffer(pBufferHeader);
    BOMX_ASSERT(err == OMX_ErrorNone);

    // Return frames to nexus immediately if possible
    ReturnDecodedFrames();

    // Signal thread to look for more
    B_Event_Set(m_hOutputFrameEvent);

    return OMX_ErrorNone;
}

void BOMX_VideoDecoder::PlaypumpEvent()
{
    NEXUS_PlaypumpStatus playpumpStatus;
    unsigned numComplete;

    if ( NULL == m_hPlaypump )
    {
        // This can happen with rapid startup/shutdown sequences such as random action tests
        BOMX_WRN(("Playpump event received, but playpump has been closed."));
        return;
    }

    NEXUS_Playpump_GetStatus(m_hPlaypump, &playpumpStatus);
    if ( playpumpStatus.started )
    {
        numComplete = m_submittedDescriptors - playpumpStatus.descFifoDepth;

        while ( numComplete > 0 )
        {
            BOMX_Buffer *pBuffer;
            BOMX_VideoDecoderInputBufferInfo *pInfo;

            pBuffer = m_pVideoPorts[0]->GetBuffer();
            BOMX_ASSERT(NULL != pBuffer);

            pInfo = (BOMX_VideoDecoderInputBufferInfo *)pBuffer->GetComponentPrivate();
            if ( numComplete >= pInfo->numDescriptors )
            {
                numComplete -= pInfo->numDescriptors;
                BOMX_ASSERT(m_submittedDescriptors >= pInfo->numDescriptors);
                m_submittedDescriptors -= pInfo->numDescriptors;
                BOMX_INPUT_MSG(("Returning input buffer %#x", pBuffer->GetHeader()->pBuffer));
                ReturnPortBuffer(m_pVideoPorts[0], pBuffer);
            }
            else
            {
                // Some descriptors are still pending for this buffer
                break;
            }
        }
    }
}

#if 0  /* Source Changed Event is not required in non-tunneled. */
void BOMX_VideoDecoder::SourceChangedEvent()
{
    NEXUS_VideoDecoderStatus vdecStatus;
    NEXUS_VideoDecoderExtendedStatus vdecExtStatus;
    OMX_PARAM_PORTDEFINITIONTYPE portDef;
    BOMX_Port *pPort = FindPortByIndex(m_videoPortBase+1);

    if ( m_hSimpleVideoDecoder )
    {
        (void)NEXUS_SimpleVideoDecoder_GetStatus(m_hSimpleVideoDecoder, &vdecStatus);
        (void)NEXUS_SimpleVideoDecoder_GetExtendedStatus(m_hSimpleVideoDecoder, &vdecExtStatus);
        if ( vdecStatus.started && vdecStatus.numDecoded > 0 )
        {
            OMX_U32 xFramerate;
            bool formatChanged=false;
            switch ( vdecStatus.frameRate )
            {
            case NEXUS_VideoFrameRate_e23_976:
                xFramerate = (OMX_U32)(65536.0 * 23.976);
                break;
            case NEXUS_VideoFrameRate_e24:
                xFramerate = (OMX_U32)(65536.0 * 24.0);
                break;
            case NEXUS_VideoFrameRate_e25:
                xFramerate = (OMX_U32)(65536.0 * 25.0);
                break;
            case NEXUS_VideoFrameRate_e29_97:
                xFramerate = (OMX_U32)(65536.0 * 29.97);
                break;
            case NEXUS_VideoFrameRate_e30:
                xFramerate = (OMX_U32)(65536.0 * 30.0);
                break;
            case NEXUS_VideoFrameRate_e50:
                xFramerate = (OMX_U32)(65536.0 * 50.0);
                break;
            case NEXUS_VideoFrameRate_e59_94:
                xFramerate = (OMX_U32)(65536.0 * 59.94);
                break;
            case NEXUS_VideoFrameRate_e60:
                xFramerate = (OMX_U32)(65536.0 * 60.0);
                break;
            case NEXUS_VideoFrameRate_e14_985:
                xFramerate = (OMX_U32)(65536.0 * 14.985);
                break;
            case NEXUS_VideoFrameRate_e7_493:
                xFramerate = (OMX_U32)(65536.0 * 7.493);
                break;
            case NEXUS_VideoFrameRate_e10:
                xFramerate = (OMX_U32)(65536.0 * 10.0);
                break;
            case NEXUS_VideoFrameRate_e15:
                xFramerate = (OMX_U32)(65536.0 * 15.0);
                break;
            case NEXUS_VideoFrameRate_e20:
                xFramerate = (OMX_U32)(65536.0 * 20.0);
                break;
            default:
                xFramerate = 0;
                break;
            }
            pPort->GetDefinition(&portDef);
            if ( portDef.format.video.nFrameWidth != vdecStatus.display.width ||
                 portDef.format.video.nFrameHeight != vdecStatus.display.height ||
                 portDef.format.video.xFramerate != xFramerate )
            {
                portDef.format.video.nFrameWidth = vdecStatus.display.width;
                portDef.format.video.nFrameHeight = vdecStatus.display.height;
                formatChanged=true;
            }
            portDef.format.video.xFramerate = xFramerate;
            pPort->SetDefinition(&portDef);
            if ( formatChanged )
            {
                PortFormatChanged(pPort);
            }
            if ( m_eosPending && vdecExtStatus.lastPictureFlag && pPort->IsTunneled() )
            {
                m_eosPending = false;
                HandleEOS();
            }
        }
    }
}
#endif

void BOMX_VideoDecoder::OutputFrameEvent()
{
    // Cancel pending timer
    if ( NULL != m_outputFrameTimerId )
    {
        CancelTimer(m_outputFrameTimerId);
        m_outputFrameTimerId = NULL;
    }
    // Check for new frames - will arm timer if required
    PollDecodedFrames();
}

void BOMX_VideoDecoder::OutputFrameTimer()
{
    // Invalidate timer id
    m_outputFrameTimerId = NULL;
    // Check for new frames - will arm timer if required
    PollDecodedFrames();
}

static const struct {
    const char *pName;
    int index;
} g_extensions[] =
{
    {"OMX.google.android.index.enableAndroidNativeBuffers", (int)OMX_IndexParamEnableAndroidNativeGraphicsBuffer},
    {"OMX.google.android.index.getAndroidNativeBufferUsage", (int)OMX_IndexParamGetAndroidNativeBufferUsage},
    {"OMX.google.android.index.useAndroidNativeBuffer", (int)OMX_IndexParamUseAndroidNativeBuffer},
    {"OMX.google.android.index.useAndroidNativeBuffer2", (int)OMX_IndexParamUseAndroidNativeBuffer2},
    {"OMX.google.android.index.describeColorFormat", (int)OMX_IndexParamDescribeColorFormat},
    {"OMX.google.android.index.storeMetaDataInBuffers", (int)OMX_IndexParamStoreMetaDataInBuffers},
#if 0 /* Not yet supported */
    {"OMX.google.android.index.configureVideoTunnelMode", (int)OMX_IndexParamConfigureVideoTunnelMode},     // TODO: Requires audio HW acceleration
#endif
    {NULL, 0}
};

OMX_ERRORTYPE BOMX_VideoDecoder::GetExtensionIndex(
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
            *pIndexType = (OMX_INDEXTYPE)g_extensions[i].index;
            return OMX_ErrorNone;
        }
    }

    BOMX_WRN(("Extension %s is not supported", cParameterName));
    return OMX_ErrorUnsupportedIndex;
}

OMX_ERRORTYPE BOMX_VideoDecoder::GetConfig(
        OMX_IN  OMX_INDEXTYPE nIndex,
        OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    BSTD_UNUSED(pComponentConfigStructure);
    switch ( (int)nIndex )
    {
    case OMX_IndexConfigCommonOutputCrop:
    {
        OMX_CONFIG_RECTTYPE *pRect = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;
        BOMX_STRUCT_VALIDATE(pRect);
        if ( pRect->nPortIndex != m_videoPortBase+1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        // Output crop is checking to see if we are filling less than the input buffer size
        // This is not relevant since we scale graphics operations to fill and pass other
        // buffers around as metadata
        pRect->nLeft = 0;
        pRect->nTop = 0;
        pRect->nWidth = m_pVideoPorts[1]->GetDefinition()->format.video.nFrameWidth;
        pRect->nHeight = m_pVideoPorts[1]->GetDefinition()->format.video.nFrameHeight;
        return OMX_ErrorNone;
    }
    default:
        BOMX_ERR(("Config index %#x is not supported", nIndex));
        return BOMX_ERR_TRACE(OMX_ErrorUnsupportedIndex);
    }
}

OMX_ERRORTYPE BOMX_VideoDecoder::SetConfig(
        OMX_IN  OMX_INDEXTYPE nIndex,
        OMX_IN  OMX_PTR pComponentConfigStructure)
{
    BSTD_UNUSED(pComponentConfigStructure);
    switch ( (int)nIndex )
    {
    default:
        BOMX_ERR(("Config index %#x is not supported", nIndex));
        return BOMX_ERR_TRACE(OMX_ErrorUnsupportedIndex);
    }
}

void BOMX_VideoDecoder::InvalidateDecodedFrames()
{
    /*
    Invalidating the frame queue does two things.

    1) Frames that have been extracted from nexus and have already
    been delivered to the app are marked as invalid.  This means that
    if they are returned via fillThisBuffer we will simply move them
    back to the free list, they will not really be returned to nexus.

    2) Frames that have been extracted from nexus but not yet
    delivered are simply discarded and moved back to the free list.
    */
    BOMX_VideoDecoderFrameBuffer *pBuffer, *pNext=NULL;

    for ( pBuffer = BLST_Q_FIRST(&m_frameBufferAllocList);
          NULL != pBuffer;
          pBuffer = pNext )
    {
        pNext = BLST_Q_NEXT(pBuffer, node);
        if ( pBuffer->state == BOMX_VideoDecoderFrameBufferState_eDelivered ||
             pBuffer->state == BOMX_VideoDecoderFrameBufferState_eInvalid )
        {
            // If the buffer is still owned by the app, just mark it as invalid and continue
            pBuffer->state = BOMX_VideoDecoderFrameBufferState_eInvalid;
        }
        else
        {
            // Any other state means the buffer is owned by us.  Discard it by moving it back
            // to the free list
            pBuffer->state = BOMX_VideoDecoderFrameBufferState_eInvalid;
            BOMX_MSG(("Invalidating frame buffer %#x (state %u)", pBuffer, pBuffer->state));
            BLST_Q_REMOVE(&m_frameBufferAllocList, pBuffer, node);
            BLST_Q_INSERT_TAIL(&m_frameBufferFreeList, pBuffer, node);
        }
    }
}

void BOMX_VideoDecoder::PollDecodedFrames()
{
    NEXUS_VideoDecoderFrameStatus frameStatus[B_MAX_DECODED_FRAMES], *pFrameStatus;
    BOMX_VideoDecoderFrameBuffer *pBuffer;
    NEXUS_Error errCode;
    unsigned numFrames=0;
    unsigned i;

    if ( NULL == m_hSimpleVideoDecoder )
    {
        return;
    }

    // There should be at least one free buffer in the list or there is no point checking for more from nexus
    if ( NULL != BLST_Q_FIRST(&m_frameBufferFreeList) )
    {
        // Get as many frames as possible from nexus
        errCode = NEXUS_SimpleVideoDecoder_GetDecodedFrames(m_hSimpleVideoDecoder, frameStatus, B_MAX_DECODED_FRAMES, &numFrames);
        BOMX_MSG(("GetDecodedFrames: comp:%s rc=%u numFrames=%u", GetName(), errCode, numFrames));
        if ( NEXUS_SUCCESS == errCode && numFrames > 0 )
        {
            // Ignore invalidated frames in alloc list
            for ( pBuffer = BLST_Q_FIRST(&m_frameBufferAllocList);
                  NULL != pBuffer && pBuffer->state == BOMX_VideoDecoderFrameBufferState_eInvalid;
                  pBuffer = BLST_Q_NEXT(pBuffer, node) );

            // Skip frames already in alloc list
            pFrameStatus = frameStatus;
            for ( ;
                  NULL != pBuffer && numFrames > 0;
                  pBuffer = BLST_Q_NEXT(pBuffer, node) )
            {
                // Sanity Check
                if ( pBuffer->frameStatus.serialNumber != pFrameStatus->serialNumber )
                {
                    BOMX_ERR(("Frame Mismatch - expecting %u got %u (state %u)", pFrameStatus->serialNumber, pBuffer->frameStatus.serialNumber, pBuffer->state));
                    BOMX_ASSERT(pBuffer->frameStatus.serialNumber  == pFrameStatus->serialNumber);
                }
                // Skip This one
                numFrames--;
                pFrameStatus++;
            }
            // Add any new frames if possible
            while ( numFrames > 0 )
            {
                pBuffer = BLST_Q_FIRST(&m_frameBufferFreeList);
                if ( pBuffer )
                {
                    // Move frame to allocated list.  Will be delivered to client afterward if there is a buffer ready.
                    BLST_Q_REMOVE_HEAD(&m_frameBufferFreeList, node);
                    pBuffer->frameStatus = *pFrameStatus;
                    pBuffer->state = BOMX_VideoDecoderFrameBufferState_eReady;
                    BLST_Q_INSERT_TAIL(&m_frameBufferAllocList, pBuffer, node);
                    pFrameStatus++;
                    numFrames--;
                }
                else
                {
                    break;
                }
            }
        }
    }

    // Check if we have client buffers ready to be delivered
    if ( m_pVideoPorts[1]->QueueDepth() > 0 )
    {
        // Skip all frames already delivered
        for ( pBuffer = BLST_Q_FIRST(&m_frameBufferAllocList);
              NULL != pBuffer && pBuffer->state != BOMX_VideoDecoderFrameBufferState_eReady;
              pBuffer = BLST_Q_NEXT(pBuffer, node) )
        {
            //BOMX_MSG(("Skip buffer %u State %u", pBuffer->frameStatus.serialNumber, pBuffer->state));
        }
        // Loop through remaining buffers
        while ( NULL != pBuffer && !m_formatChangePending && !m_eosDelivered )
        {
            BOMX_Buffer *pOmxBuffer;
            pOmxBuffer = m_pVideoPorts[1]->GetBuffer();
            if ( NULL != pOmxBuffer )
            {
                BOMX_VideoDecoderOutputBufferInfo *pInfo;
                OMX_BUFFERHEADERTYPE *pHeader = pOmxBuffer->GetHeader();
                pHeader->nOffset = 0;
                if ( !m_pBufferTracker->Remove(pBuffer->frameStatus.pts, pHeader) )
                {
                    BOMX_WRN(("Unable to find tracker entry for pts %#x", pBuffer->frameStatus.pts));
                    pHeader->nFlags = 0;
                    BOMX_PtsToTick(pBuffer->frameStatus.pts, &pHeader->nTimeStamp);
                }
                pInfo = (BOMX_VideoDecoderOutputBufferInfo *)pOmxBuffer->GetComponentPrivate();
                BDBG_ASSERT(NULL == pInfo->pFrameBuffer);
                if ( pBuffer->frameStatus.lastPicture || (pHeader->nFlags & OMX_BUFFERFLAG_EOS) )
                {
                    BOMX_MSG(("EOS picture received"));
                    if ( m_eosPending )
                    {
                        pHeader->nFlags |= OMX_BUFFERFLAG_EOS;
                        m_eosPending = false;
                        m_eosDelivered = true;
                    }
                    else
                    {
                        // Fatal error - we did something wrong.
                        BOMX_ERR(("Additional frames received after EOS"));
                        BOMX_ASSERT(true == m_eosPending);
                        return;
                    }
                }

                #if 0 // Disable format change for now
                // Check for format change
                if ( (pBuffer->frameStatus.surfaceCreateSettings.imageWidth != m_pVideoPorts[1]->GetDefinition()->format.video.nFrameWidth ||
                      pBuffer->frameStatus.surfaceCreateSettings.imageHeight != m_pVideoPorts[1]->GetDefinition()->format.video.nFrameHeight) )
                {
                    OMX_PARAM_PORTDEFINITIONTYPE portDefs;
                    // TODO: Handle adaptive/metadata mode
                    BOMX_WRN(("Video output format change %ux%u -> %ux%u", m_pVideoPorts[1]->GetDefinition()->format.video.nFrameWidth, m_pVideoPorts[1]->GetDefinition()->format.video.nFrameHeight,
                        pBuffer->frameStatus.surfaceCreateSettings.imageWidth, pBuffer->frameStatus.surfaceCreateSettings.imageHeight));
                    m_formatChangePending = true;
                    m_pVideoPorts[1]->GetDefinition(&portDefs);
                    portDefs.format.video.nFrameWidth = pBuffer->frameStatus.surfaceCreateSettings.imageWidth;
                    portDefs.format.video.nFrameHeight = pBuffer->frameStatus.surfaceCreateSettings.imageHeight;
                    portDefs.nBufferSize = 2*pBuffer->frameStatus.surfaceCreateSettings.imageWidth*pBuffer->frameStatus.surfaceCreateSettings.imageHeight;
                    m_pVideoPorts[1]->SetDefinition(&portDefs);
                    PortFormatChanged(m_pVideoPorts[1]);
                    return;
                }
                #endif
                // If this is a true EOS dummy picture from the decoder, return a 0-length frame.
                // Otherwise there is a valid picture here and we need to handle the frame below as we would any other.
                if ( pBuffer->frameStatus.lastPicture )
                {
                    BOMX_MSG(("EOS-only frame.  Returning length of 0."));
                    pHeader->nFilledLen = 0;
                }
                else
                {
                    BOMX_ASSERT(NULL != pInfo);
                    switch ( pInfo->type )
                    {
                    case BOMX_VideoDecoderOutputBufferType_eStandard:
                        {
                            NEXUS_Error errCode;
                            NEXUS_StripedSurfaceHandle hStripedSurface;
                            hStripedSurface = NEXUS_StripedSurface_Create(&pBuffer->frameStatus.surfaceCreateSettings);
                            if ( NULL == hStripedSurface )
                            {
                                (void)BOMX_BERR_TRACE(BERR_OUT_OF_SYSTEM_MEMORY);
                                // This is not working now because of a nexus bug.  Keep going and return junk...
                                //return;
                            }
                            // Flush the surface in case CPU had read the data
                            NEXUS_Surface_Flush(pInfo->typeInfo.standard.hSurface);
                            if ( hStripedSurface )
                            {
                                errCode = NEXUS_Graphics2D_DestripeToSurface(m_hGraphics2d, hStripedSurface, pInfo->typeInfo.standard.hSurface, NULL);
                                if ( errCode )
                                {
                                    (void)BOMX_BERR_TRACE(errCode);
                                    // Try again later?!?
                                    return;
                                }
                            }
                            pHeader->nFilledLen = 2*m_pVideoPorts[1]->GetDefinition()->format.video.nFrameWidth*m_pVideoPorts[1]->GetDefinition()->format.video.nFrameHeight;
                            // Wait for the blit to complete before delivering in case CPU will access it quickly
                            GraphicsCheckpoint();
                            if ( hStripedSurface )
                            {
                                NEXUS_StripedSurface_Destroy(hStripedSurface);
                            }
                            if ( pInfo->typeInfo.standard.pClientMemory )
                            {
                                CopySurfaceToClient(pInfo);
                            }
                        }
                        break;
                    case BOMX_VideoDecoderOutputBufferType_eNative:
                        {
                            pHeader->nFilledLen = 2*m_pVideoPorts[1]->GetDefinition()->format.video.nFrameWidth*m_pVideoPorts[1]->GetDefinition()->format.video.nFrameHeight;
                            pInfo->typeInfo.native.pSharedData->DisplayFrame.frameStatus = pBuffer->frameStatus;
                        }
                        break;
                    case BOMX_VideoDecoderOutputBufferType_eMetadata:
                        {
                            SHARED_DATA *pSharedData;
                            pBuffer->pPrivateHandle = (private_handle_t *)pInfo->typeInfo.metadata.pMetadata->pHandle;
                            pSharedData = (SHARED_DATA *)NEXUS_OffsetToCachedAddr(pBuffer->pPrivateHandle->sharedData);
                            if ( NULL == pSharedData )
                            {
                                BOMX_ERR(("Unable to convert SHARED_DATA physical address %#x", pBuffer->pPrivateHandle->sharedData));
                                (void)BOMX_ERR_TRACE(OMX_ErrorBadParameter);
                            }
                            else
                            {
                                // Setup window parameters for display and provide buffer status
                                pSharedData->videoWindow.nexusClientContext = m_pNexusClient;
                                android_atomic_release_store(1, &pSharedData->videoWindow.windowIdPlusOne);
                                pSharedData->DisplayFrame.frameStatus = pBuffer->frameStatus;
                            }
                            pHeader->nFilledLen = sizeof(VideoDecoderOutputMetaData);
                        }
                        break;
                    default:
                        break;
                    }
                }

                // Mark buffer as delivered and return to client
                pBuffer->state = BOMX_VideoDecoderFrameBufferState_eDelivered;
                pInfo->pFrameBuffer = pBuffer;
                pBuffer->pBufferInfo = pInfo;
                BOMX_MSG(("Returning Port Buffer ts %u us serial %u pInfo %#x FB %#x HDR %#x flags %#x", (unsigned int)pHeader->nTimeStamp, pBuffer->frameStatus.serialNumber, pInfo, pInfo->pFrameBuffer, pHeader, pHeader->nFlags));
                {
                    unsigned queueDepthBefore = m_pVideoPorts[1]->QueueDepth();
                    ReturnPortBuffer(m_pVideoPorts[1], pOmxBuffer);
                    BOMX_ASSERT(queueDepthBefore == (m_pVideoPorts[1]->QueueDepth()+1));
                }
                // Advance to next frame
                pBuffer = BLST_Q_NEXT(pBuffer, node);
            }
            else
            {
                break;
            }
        }
    }

    // If there are still buffers ready for data, set a timer to check again later
    if ( m_pVideoPorts[1]->QueueDepth() > 0 && !m_eosDelivered )
    {
        BOMX_MSG(("%u output buffers pending, restart timer", m_pVideoPorts[1]->QueueDepth()));
        m_outputFrameTimerId = StartTimer(B_FRAME_TIMER_INTERVAL, BOMX_VideoDecoder_OutputFrameTimer, static_cast<void *>(this));
    }
    else
    {
        if ( m_eosDelivered )
        {
            BOMX_MSG(("EOS already delivered.  Not checking for more frames."));
        }
        else
        {
            BOMX_MSG(("No buffers queued at output port.  Ignore timer."));
        }
    }
}

void BOMX_VideoDecoder::ReturnDecodedFrames()
{
    NEXUS_VideoDecoderReturnFrameSettings returnSettings[B_MAX_DECODED_FRAMES];
    unsigned numFrames=0;
    BOMX_VideoDecoderFrameBuffer *pBuffer, *pStart, *pEnd;

    // Make sure decoder is available and running
    if ( NULL == m_hSimpleVideoDecoder )
    {
        return;
    }
    else
    {
        NEXUS_VideoDecoderStatus status;
        NEXUS_SimpleVideoDecoder_GetStatus(m_hSimpleVideoDecoder, &status);
        if ( !status.started )
        {
            return;
        }
    }

    // Skip pending invalidated frames
    for ( pBuffer = BLST_Q_FIRST(&m_frameBufferAllocList);
          NULL != pBuffer && pBuffer->state == BOMX_VideoDecoderFrameBufferState_eInvalid;
          pBuffer = BLST_Q_NEXT(pBuffer, node) );

    // If we scanned the entire list or the first frame is not yet delivered just bail out
    if ( NULL == pBuffer || pBuffer->state == BOMX_VideoDecoderFrameBufferState_eReady )
    {
        return;
    }

    // Find last actionable frame if there is one
    for ( pStart = pBuffer, pEnd = NULL;
          NULL != pBuffer && pBuffer->state != BOMX_VideoDecoderFrameBufferState_eReady;
          pBuffer = BLST_Q_NEXT(pBuffer, node) )
    {
        switch ( pBuffer->state )
        {
        case BOMX_VideoDecoderFrameBufferState_eDisplayReady:
        case BOMX_VideoDecoderFrameBufferState_eDropReady:
            pEnd = pBuffer;
            break;
        default:
            break;
        }
    }

    // Deal with the frames we found
    if ( NULL != pEnd )
    {
        // Do what was requested
        pEnd = BLST_Q_NEXT(pEnd, node);
        pBuffer = pStart;
        while ( pBuffer != pEnd && numFrames < B_MAX_DECODED_FRAMES )
        {
            BOMX_VideoDecoderFrameBuffer *pNext = BLST_Q_NEXT(pBuffer, node);
            //NEXUS_VideoDecoder_GetDefaultReturnFrameSettings(&returnSettings[numFrames]); Intentionally skipped - there is only one field to set anyway
            if ( pBuffer->state == BOMX_VideoDecoderFrameBufferState_eDelivered )
            {
                BOMX_WRN(("Dropping outstanding frame %u still owned by client - a later frame was returned already", pBuffer->frameStatus.serialNumber));
                pBuffer->state = BOMX_VideoDecoderFrameBufferState_eInvalid;
                returnSettings[numFrames].display = false;
            }
            else
            {
                if ( pNext == pEnd )
                {
                    returnSettings[numFrames].display = (pBuffer->state == BOMX_VideoDecoderFrameBufferState_eDisplayReady) ? true : false;
                }
                else
                {
                    returnSettings[numFrames].display = false;
                }
                pBuffer->pPrivateHandle = NULL;
                pBuffer->pBufferInfo = NULL;
                BLST_Q_REMOVE(&m_frameBufferAllocList, pBuffer, node);
                BLST_Q_INSERT_TAIL(&m_frameBufferFreeList, pBuffer, node);
            }
            numFrames++;
            pBuffer = pNext;
        }

        if ( numFrames > 0 )
        {
            BOMX_MSG(("Returning %u frames to nexus last=%s", numFrames, returnSettings[numFrames-1].display?"display":"drop"));
            NEXUS_Error errCode = NEXUS_SimpleVideoDecoder_ReturnDecodedFrames(m_hSimpleVideoDecoder, returnSettings, numFrames);
            if ( errCode )
            {
                BOMX_BERR_TRACE(errCode);
                // Not much else we can do
            }
        }
    }

    // Do not try to check for more frames here, that will be done by the component thread based on the event or timer
}

void BOMX_VideoDecoder::GraphicsCheckpoint()
{
    NEXUS_Error errCode;

    errCode = NEXUS_Graphics2D_Checkpoint(m_hGraphics2d, NULL);
    if ( errCode == NEXUS_GRAPHICS2D_QUEUED )
    {
        errCode = B_Event_Wait(m_hCheckpointEvent, 1000);
        if ( errCode )
        {
            BOMX_ERR(("Timeout waiting for graphics checkpoint!"));
        }
    }
}

void BOMX_VideoDecoder::CopySurfaceToClient(const BOMX_VideoDecoderOutputBufferInfo *pInfo)
{
    if ( pInfo &&
         pInfo->type == BOMX_VideoDecoderOutputBufferType_eStandard &&
         pInfo->typeInfo.standard.pClientMemory )
    {
        size_t numBytes;
        NEXUS_SurfaceStatus surfaceStatus;

        NEXUS_Surface_GetStatus(pInfo->typeInfo.standard.hSurface, &surfaceStatus);
        numBytes = surfaceStatus.height * surfaceStatus.pitch;  // HW graphics will always scale this to the full buffer size
        BKNI_Memcpy(pInfo->typeInfo.standard.pClientMemory, pInfo->typeInfo.standard.pSurfaceMemory, numBytes);
    }
}

void BOMX_VideoDecoder::InitPESHeader(void *pHeaderBuffer)
{
    uint8_t *pConfig = (uint8_t *)pHeaderBuffer;
    pConfig[0] = 0x00;
    pConfig[1] = 0x00;
    pConfig[2] = 0x01;
    pConfig[3] = (uint8_t)B_STREAM_ID;
    pConfig[4] = 0;                      // Unbounded packet
    pConfig[5] = 0;                      // Unbounded packet
    pConfig[6] = 0x81;                   /* Indicate header with 0x10 in the upper bits, original material */
    pConfig[7] = 0;
    pConfig[8] = 0;
}

OMX_ERRORTYPE BOMX_VideoDecoder::ConfigBufferInit()
{
    NEXUS_Error errCode;

    if (m_pConfigBuffer == NULL)
    {
        errCode = AllocateInputBuffer(BOMX_VIDEO_CODEC_CONFIG_BUFFER_SIZE, m_pConfigBuffer);
        if ( errCode )
        {
            BOMX_ERR(("Unable to allocate codec config buffer"));
            return BOMX_ERR_TRACE(OMX_ErrorUndefined);
        }

        InitPESHeader(m_pConfigBuffer);
        NEXUS_FlushCache(m_pConfigBuffer, m_configBufferSize);
        m_configRequired = false;
    }

    m_configBufferSize = BOMX_VIDEO_CODEC_CONFIG_HEADER_SIZE;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_VideoDecoder::ConfigBufferAppend(const void *pBuffer, size_t length)
{
    BOMX_ASSERT(NULL != pBuffer);
    if ( m_configBufferSize + length > BOMX_VIDEO_CODEC_CONFIG_BUFFER_SIZE )
    {
        return BOMX_ERR_TRACE(OMX_ErrorOverflow);
    }
    BKNI_Memcpy(((uint8_t *)m_pConfigBuffer) + m_configBufferSize, pBuffer, length);
    m_configBufferSize += length;
    NEXUS_FlushCache(m_pConfigBuffer, m_configBufferSize);
    return OMX_ErrorNone;
}

BOMX_VideoDecoderFrameBuffer *BOMX_VideoDecoder::FindFrameBuffer(private_handle_t *pPrivateHandle)
{
    BOMX_VideoDecoderFrameBuffer *pFrameBuffer;
    BOMX_ASSERT(NULL != pPrivateHandle);

    // Scan allocated frame list for matching private handle
    for ( pFrameBuffer = BLST_Q_FIRST(&m_frameBufferAllocList);
          NULL != pFrameBuffer && pFrameBuffer->pPrivateHandle != pPrivateHandle;
          pFrameBuffer = BLST_Q_NEXT(pFrameBuffer, node) );

    return pFrameBuffer;
}

BOMX_VideoDecoderFrameBuffer *BOMX_VideoDecoder::FindFrameBuffer(unsigned serialNumber)
{
    BOMX_VideoDecoderFrameBuffer *pFrameBuffer;

    // Scan allocated frame list for matching private handle
    for ( pFrameBuffer = BLST_Q_FIRST(&m_frameBufferAllocList);
          NULL != pFrameBuffer && pFrameBuffer->frameStatus.serialNumber != serialNumber;
          pFrameBuffer = BLST_Q_NEXT(pFrameBuffer, node) );

    return pFrameBuffer;
}

void BOMX_VideoDecoder::DisplayFrame(unsigned serialNumber)
{
    // This function is only callable by the binder thread and cannot be called internally
    ALOGV("DisplayFrame(%d)", serialNumber);
    Lock();
    DisplayFrame_locked(serialNumber);
    Unlock();
}

void BOMX_VideoDecoder::DisplayFrame_locked(unsigned serialNumber)
{
    BOMX_VideoDecoderFrameBuffer *pFrameBuffer;

    pFrameBuffer = FindFrameBuffer(serialNumber);
    if ( pFrameBuffer )
    {
        if ( pFrameBuffer->state == BOMX_VideoDecoderFrameBufferState_eDelivered )
        {
            // Mark buffer as ready to display
            pFrameBuffer->state = BOMX_VideoDecoderFrameBufferState_eDisplayReady;
            // Break linkage between omx output buffer and frame buffer
            pFrameBuffer->pPrivateHandle = NULL;
            pFrameBuffer->pBufferInfo->pFrameBuffer = NULL;
            pFrameBuffer->pBufferInfo = NULL;
            // Return frames to nexus decoder
            ReturnDecodedFrames();
            // We just returned something to nexus, kick off the output frame thread
            B_Event_Set(m_hOutputFrameEvent);
        }
        else
        {
            ALOGW("DisplayFrame: Ignoring invalid frame %u", serialNumber);
        }
    }
    else
    {
        ALOGW("Request to display buffer %u not found in list (duplicate?)", serialNumber);
    }
}

NEXUS_Error BOMX_VideoDecoder::AllocateInputBuffer(uint32_t nSize, void*& pBuffer)
{
    NEXUS_ClientConfiguration               clientConfig;
    NEXUS_MemoryAllocationSettings          memorySettings;

    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    NEXUS_Memory_GetDefaultAllocationSettings(&memorySettings);
    memorySettings.heap = clientConfig.heap[1];
    return NEXUS_Memory_Allocate(nSize, &memorySettings, &pBuffer);
}

void BOMX_VideoDecoder::FreeInputBuffer(void*& pBuffer)
{
    NEXUS_Memory_Free(pBuffer);
    pBuffer = NULL;
}

OMX_ERRORTYPE BOMX_VideoDecoder::SendConfigBuffertoHw(BOMX_VideoDecoderInputBufferInfo *pInfo)
{
    NEXUS_PlaypumpScatterGatherDescriptor desc[1];
    NEXUS_Error errCode;
    size_t numConsumed, numRequested;

    numRequested = 1;
    desc[0].addr = m_pConfigBuffer;
    desc[0].length = m_configBufferSize;
    BOMX_INPUT_MSG(("Delivering %u bytes of codec config data after flush, buff:%p",
                    (unsigned int)m_configBufferSize, m_pConfigBuffer));
    errCode = NEXUS_Playpump_SubmitScatterGatherDescriptor(m_hPlaypump, desc, numRequested, &numConsumed);
    if ( errCode )
    {
        return BOMX_ERR_TRACE(OMX_ErrorUndefined);
    }
    BOMX_ASSERT(numConsumed == numRequested);
    pInfo->numDescriptors += numRequested;
    m_submittedDescriptors += numConsumed;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_VideoDecoder::SendDataBuffertoHw(BOMX_VideoDecoderInputBufferInfo *pInfo,
                                                    OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    size_t payloadLen;
    void *pPayload;
    NEXUS_PlaypumpScatterGatherDescriptor desc[2];
    NEXUS_Error errCode;
    OMX_ERRORTYPE err;
    size_t numConsumed, numRequested;

    // Flush prior to sending data to HW
    NEXUS_FlushCache(pInfo->pHeader, pInfo->headerLen);
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

    // Give buffers to transport
    numRequested=1;
    desc[0].addr = pInfo->pHeader;
    desc[0].length = pInfo->headerLen;
    if ( payloadLen > 0 )
    {
        // For secure decoders, the payload is in a secure buffer. Don't flush.
        if (!m_secureDecoder)
            NEXUS_FlushCache(pPayload, payloadLen);
        numRequested=2;
        desc[1].addr = pPayload;
        desc[1].length = payloadLen;
    }
    else
    {
        BOMX_WRN(("Discarding empty payload"));
    }

    ALOGV("%s, header:%p, headerLen:%d, payload:%p, payloadLen:%d, numDesc:%u",
          __FUNCTION__, pInfo->pHeader, pInfo->headerLen, pPayload, payloadLen,
          pInfo->numDescriptors);
    errCode = NEXUS_Playpump_SubmitScatterGatherDescriptor(m_hPlaypump, desc, numRequested, &numConsumed);
    if ( errCode )
    {
        return BOMX_ERR_TRACE(OMX_ErrorUndefined);
    }
    BOMX_ASSERT(numConsumed == numRequested);
    pInfo->numDescriptors += numConsumed;
    m_submittedDescriptors += numConsumed;
    err = m_pVideoPorts[0]->QueueBuffer(pBufferHeader);
    BOMX_ASSERT(err == OMX_ErrorNone);

    return OMX_ErrorNone;
}
