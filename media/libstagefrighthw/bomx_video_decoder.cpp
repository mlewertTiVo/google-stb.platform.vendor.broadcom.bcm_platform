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
#define LOG_TAG "bomx_video_decoder"

#include <fcntl.h>
#include <cutils/log.h>
#include <cutils/sched_policy.h>
#include <cutils/compiler.h>
#include <sys/resource.h>
#include <inttypes.h>

#include "bomx_video_decoder.h"
#include "nexus_platform.h"
#include "nexus_memory.h"
#include "OMX_IndexExt.h"
#include "OMX_VideoExt.h"
#include "nexus_base_mmap.h"
#include "nexus_video_decoder.h"
#include "nexus_core_utils.h"
#include "nx_ashmem.h"
#include "bomx_pes_formatter.h"
#if defined(SECURE_DECODER_ON)
#include "nexus_sage.h"
#endif

// Runtime Properties
#define B_PROPERTY_PES_DEBUG ("media.brcm.vdec_pes_debug")
#define B_PROPERTY_INPUT_DEBUG ("media.brcm.vdec_input_debug")
#define B_PROPERTY_ENABLE_METADATA ("media.brcm.vdec_enable_metadata")
#define B_PROPERTY_TUNNELED_HFRVIDEO ("media.brcm.vdec_hfrvideo_tunnel")
#define B_PROPERTY_MEMBLK_ALLOC ("ro.nexus.ashmem.devname")
#define B_PROPERTY_SVP ("ro.nx.svp")
#define B_PROPERTY_COALESCE ("dyn.nx.netcoal.set")
#define B_PROPERTY_HFRVIDEO ("dyn.nx.hfrvideo.set")
#define B_PROPERTY_HW_SYNC_FAKE ("media.brcm.hw_sync.fake")
#define B_PROPERTY_EARLYDROP_THRESHOLD ("media.brcm.stat.earlydrop_thres")
#define B_PROPERTY_DISABLE_RUNTIME_HEAPS ("ro.nx.rth.disable")
#define B_PROPERTY_DTU ("ro.nx.capable.dtu")

#define B_HEADER_BUFFER_SIZE (32+BOMX_BCMV_HEADER_SIZE)
#define B_DATA_BUFFER_SIZE_DEFAULT (1536*1536)
#define B_DATA_BUFFER_SIZE_HIGHRES (3*1024*1024)
#define B_NUM_BUFFERS (12)
#define B_STREAM_ID 0xe0
#define B_MAX_FRAMES (12)
#define B_MAX_DECODED_FRAMES (16)
#define B_MAX_INPUT_TIMEOUT_RETURN (2)
#define B_MAX_INPUT_COMPLETED_COUNT (5)
#define B_INPUT_RETURN_SPEEDUP_THRES_INTERVAL (20)  // 50fps - frame interval threshold to speed up returning of input buffers
#define B_INPUT_RETURN_SPEEDUP_THRES_HEIGHT (2160)  // Minimum height for 4K
#define B_CHECKPOINT_TIMEOUT (5000)
#define B_SECURE_QUERY_MAX_RETRIES (10)
#define B_SECURE_QUERY_SLEEP_INTERVAL_US (200000)
#define B_STAT_EARLYDROP_THRESHOLD_MS (5000)
#define B_WAIT_FOR_STC_SYNC_TIMEOUT_MS (10000)
#define B_WAIT_FOR_FORMAT_CHANGE_TIMEOUT_MS (500)
#define B_STC_SYNC_INVALID_VALUE (0xFFFFFFFF)
/****************************************************************************
 * The descriptors used per-frame are laid out as follows:
 * [PES Header] [Codec Config Data] [Codec Header] [Payload] = 4 descriptors
 * [PES Header] [Payload] Repeats until we have completed the input frame.
 *                        This requires 2 descriptors and we can fit a max
 *                        of 65535-3 bytes of payload in each based on
 *                        the 16-bit PES length field.
 *
 * For VP9, a BPP packet for end of chunk is added at the end of the chunk
 * payload so the firmware can parse the chunk for super-frames.
*****************************************************************************/
#define B_MAX_DESCRIPTORS_PER_BUFFER(buffer_size) (4+1+(2*((buffer_size)/(B_MAX_PES_PACKET_LENGTH-(B_PES_HEADER_LENGTH_WITHOUT_PTS-B_PES_HEADER_START_BYTES)))))

#define B_TIMESTAMP_INVALID (-1)
#define B_FR_EST_NUM_STABLE_DELTA_NEEDED (8)
#define B_FR_EST_STABLE_DELTA_THRESHOLD (2000)          // in micro-seconds

#define B_DEC_ACTIVE_STATE_INACTIVE                         (0)
#define B_DEC_ACTIVE_STATE_ACTIVE_CLEAR                     (1)
#define B_DEC_ACTIVE_STATE_ACTIVE_SECURE                    (2)

#define OMX_IndexParamEnableAndroidNativeGraphicsBuffer      0x7F000001
#define OMX_IndexParamGetAndroidNativeBufferUsage            0x7F000002
#define OMX_IndexParamStoreMetaDataInBuffers                 0x7F000003
#define OMX_IndexParamUseAndroidNativeBuffer                 0x7F000004
#define OMX_IndexParamUseAndroidNativeBuffer2                0x7F000005
#define OMX_IndexParamDescribeColorFormat                    0x7F000006
#define OMX_IndexParamConfigureVideoTunnelMode               0x7F000007
#define OMX_IndexParamPrepareForAdaptivePlayback             0x7F000008
#define OMX_IndexParamDescribeHdrColorInfo                   0x7F000009
#define OMX_IndexParamDescribeColorAspects                   0x7F00000A

using namespace android;

static volatile int32_t g_decActiveState = B_DEC_ACTIVE_STATE_INACTIVE;
static volatile int32_t g_decInstanceCount = 0;
static Mutex g_decActiveStateLock("bomx_dec_active_state");

// Handling of persistent nxclient
static volatile bool g_persistNxClientOn = false;
Mutex g_persistNxClientLock("bomx_decoder_persist_nxClient");

#if defined(HW_HVD_REVISION_S)
static const BOMX_VideoDecoderRole g_defaultRoles[] = {{"video_decoder.mpeg2", OMX_VIDEO_CodingMPEG2},
                                                       {"video_decoder.avc", OMX_VIDEO_CodingAVC},
                                                       {"video_decoder.hevc", OMX_VIDEO_CodingHEVC}};
#else
static const BOMX_VideoDecoderRole g_defaultRoles[] = {{"video_decoder.mpeg2", OMX_VIDEO_CodingMPEG2},
                                                       {"video_decoder.h263", OMX_VIDEO_CodingH263},
                                                       {"video_decoder.avc", OMX_VIDEO_CodingAVC},
                                                       {"video_decoder.mpeg4", OMX_VIDEO_CodingMPEG4},
                                                       {"video_decoder.hevc", OMX_VIDEO_CodingHEVC},
                                                       {"video_decoder.vp8", OMX_VIDEO_CodingVP8}};
#endif

static const unsigned g_numDefaultRoles = sizeof(g_defaultRoles)/sizeof(BOMX_VideoDecoderRole);

struct BOMX_VideoFrameRateInfo
{
    NEXUS_VideoFrameRate rate;
    const char *str;
    int interval;
};
static const BOMX_VideoFrameRateInfo g_frameRateInfo[] = {{NEXUS_VideoFrameRate_eUnknown,   "0",          17},    // Worst case
                                                          {NEXUS_VideoFrameRate_e23_976,    "23.976",     42},
                                                          {NEXUS_VideoFrameRate_e24,        "24",         42},
                                                          {NEXUS_VideoFrameRate_e25,        "25",         40},
                                                          {NEXUS_VideoFrameRate_e29_97,     "29.97",      34},
                                                          {NEXUS_VideoFrameRate_e30,        "30",         34},
                                                          {NEXUS_VideoFrameRate_e50,        "50",         20},
                                                          {NEXUS_VideoFrameRate_e59_94,     "59.94",      17},
                                                          {NEXUS_VideoFrameRate_e60,        "60",         17},
                                                          {NEXUS_VideoFrameRate_e14_985,    "14.985",     67},
                                                          {NEXUS_VideoFrameRate_e7_493,     "7.493",      134},
                                                          {NEXUS_VideoFrameRate_e10,        "10",         100},
                                                          {NEXUS_VideoFrameRate_e15,        "15",         67},
                                                          {NEXUS_VideoFrameRate_e20,        "20",         50},
                                                          {NEXUS_VideoFrameRate_e12_5,      "12.5",       80},
                                                          {NEXUS_VideoFrameRate_e100,       "100",        10},
                                                          {NEXUS_VideoFrameRate_e119_88,    "119.88",     9},
                                                          {NEXUS_VideoFrameRate_e120,       "120",        9},
                                                          {NEXUS_VideoFrameRate_e19_98,     "19.98",      50},
                                                          {NEXUS_VideoFrameRate_e7_5,       "7.5",        134},
                                                          {NEXUS_VideoFrameRate_e12,        "12",         84},
                                                          {NEXUS_VideoFrameRate_e11_988,    "11.988",     84},
                                                          {NEXUS_VideoFrameRate_e9_99,      "9.99",       100}};
static const unsigned g_numFrameRateInfos = sizeof(g_frameRateInfo)/sizeof(BOMX_VideoFrameRateInfo);

static const NEXUS_VideoFormat g_standardFrVideoFmts[] = {NEXUS_VideoFormat_e1080p24hz,       /* HD 1080 Progressive, 24hz */
                                                          NEXUS_VideoFormat_e1080p25hz,       /* HD 1080 Progressive, 25hz */
                                                          NEXUS_VideoFormat_e1080p30hz,       /* HD 1080 Progressive, 30hz */
                                                          NEXUS_VideoFormat_e720p24hz,        /* HD 720p 24hz */
                                                          NEXUS_VideoFormat_e720p25hz,        /* HD 720p 25hz */
                                                          NEXUS_VideoFormat_e720p30hz,        /* HD 720p 30hz */
                                                          NEXUS_VideoFormat_e3840x2160p24hz,  /* UHD 3840x2160 24Hz */
                                                          NEXUS_VideoFormat_e3840x2160p25hz,  /* UHD 3840x2160 25Hz */
                                                          NEXUS_VideoFormat_e3840x2160p30hz,  /* UHD 3840x2160 30Hz */
                                                          NEXUS_VideoFormat_e4096x2160p24hz,  /* UHD 4096x2160 24Hz */
                                                          NEXUS_VideoFormat_e4096x2160p25hz,  /* UHD 4096x2160 25Hz */
                                                          NEXUS_VideoFormat_e4096x2160p30hz,  /* UHD 4096x2160 30Hz */
                                                          NEXUS_VideoFormat_e720p30hz_3DOU_AS,/* HD 3D 720p30 */
                                                          NEXUS_VideoFormat_e720p24hz_3DOU_AS,/* HD 3D 720p24 */
                                                          NEXUS_VideoFormat_e1080p24hz_3DOU_AS,/* HD 1080p 24Hz, 2750 sample per line, SMPTE 274M-1998 */
                                                          NEXUS_VideoFormat_e1080p30hz_3DOU_AS,/* HD 1080p 30Hz, 2200 sample per line, SMPTE 274M-1998 */
                                                          NEXUS_VideoFormat_eCustom_3D_720p_30hz,
                                                          NEXUS_VideoFormat_eCustom_3D_720p_24hz,
                                                          NEXUS_VideoFormat_eCustom_3D_1080p_24hz,
                                                          NEXUS_VideoFormat_eCustom_3D_1080p_30hz};
static const unsigned g_numStandardFrVideoFmts = sizeof(g_standardFrVideoFmts)/sizeof(NEXUS_VideoFormat);

static const OMX_VIDEO_HEVCLEVELTYPE g_hevcLevels[] = {OMX_VIDEO_HEVCMainTierLevel1,
                                                       OMX_VIDEO_HEVCMainTierLevel2,
                                                       OMX_VIDEO_HEVCMainTierLevel21,
                                                       OMX_VIDEO_HEVCMainTierLevel3,
                                                       OMX_VIDEO_HEVCMainTierLevel31,
                                                       OMX_VIDEO_HEVCMainTierLevel4,
                                                       OMX_VIDEO_HEVCMainTierLevel41,
                                                       OMX_VIDEO_HEVCMainTierLevel5,
                                                       OMX_VIDEO_HEVCMainTierLevel51};
static const unsigned g_numHevcLevels = sizeof(g_hevcLevels)/sizeof(OMX_VIDEO_HEVCLEVELTYPE);

static const OMX_VIDEO_HEVCLEVELTYPE g_hevcStandardLevels[] = {OMX_VIDEO_HEVCMainTierLevel1,
                                                               OMX_VIDEO_HEVCMainTierLevel2,
                                                               OMX_VIDEO_HEVCMainTierLevel21,
                                                               OMX_VIDEO_HEVCMainTierLevel3,
                                                               OMX_VIDEO_HEVCMainTierLevel31,
                                                               OMX_VIDEO_HEVCMainTierLevel4,
                                                               OMX_VIDEO_HEVCMainTierLevel41,
                                                               OMX_VIDEO_HEVCMainTierLevel5};
static const unsigned g_numHevcStandardLevels = sizeof(g_hevcStandardLevels)/sizeof(OMX_VIDEO_HEVCLEVELTYPE);

static const OMX_VIDEO_VP9LEVELTYPE g_vp9Levels[] =   {OMX_VIDEO_VP9Level1,
                                                       OMX_VIDEO_VP9Level11,
                                                       OMX_VIDEO_VP9Level2,
                                                       OMX_VIDEO_VP9Level21,
                                                       OMX_VIDEO_VP9Level3,
                                                       OMX_VIDEO_VP9Level31,
                                                       OMX_VIDEO_VP9Level4,
                                                       OMX_VIDEO_VP9Level41,
                                                       OMX_VIDEO_VP9Level5,
                                                       OMX_VIDEO_VP9Level51};
static const size_t g_numVp9Levels = sizeof(g_vp9Levels)/sizeof(OMX_VIDEO_VP9LEVELTYPE);

static const OMX_VIDEO_VP9LEVELTYPE g_vp9StandardLevels[] =  {OMX_VIDEO_VP9Level1,
                                                              OMX_VIDEO_VP9Level11,
                                                              OMX_VIDEO_VP9Level2,
                                                              OMX_VIDEO_VP9Level21,
                                                              OMX_VIDEO_VP9Level3,
                                                              OMX_VIDEO_VP9Level31,
                                                              OMX_VIDEO_VP9Level4,
                                                              OMX_VIDEO_VP9Level41,
                                                              OMX_VIDEO_VP9Level5};
static const size_t g_numVp9StandardLevels = sizeof(g_vp9StandardLevels)/sizeof(OMX_VIDEO_VP9LEVELTYPE);

enum BOMX_VideoDecoderEventType
{
    BOMX_VideoDecoderEventType_ePlaypump=0,
    BOMX_VideoDecoderEventType_eDataReady,
    BOMX_VideoDecoderEventType_eCheckpoint,
    BOMX_VideoDecoderEventType_eSourceChanged,
    BOMX_VideoDecoderEventType_eStreamChanged,
    BOMX_VideoDecoderEventType_eMax
};

OMX_ERRORTYPE BOMX_VideoDecoder_CreateCommon(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks,
    bool tunnelMode)
{
    BOMX_VideoDecoder *pVideoDecoder = new BOMX_VideoDecoder(pComponentTpe, pName, pAppData, pCallbacks,
                                                             NULL, 0, false, tunnelMode);
    if ( NULL == pVideoDecoder )
    {
        return BOMX_ERR_TRACE(OMX_ErrorUndefined);
    }
    else
    {
        OMX_ERRORTYPE constructorError = pVideoDecoder->IsValid();
        if ( constructorError == OMX_ErrorNone )
        {
            return OMX_ErrorNone;
        }
        else
        {
            delete pVideoDecoder;
            return BOMX_ERR_TRACE(constructorError);
        }
    }
}

extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_CreateTunnel(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    return BOMX_VideoDecoder_CreateCommon(pComponentTpe, pName, pAppData, pCallbacks, true);
}

extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_Create(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    return BOMX_VideoDecoder_CreateCommon(pComponentTpe, pName, pAppData, pCallbacks, false);
}

OMX_ERRORTYPE BOMX_VideoDecoder_CreateVp9Common(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks,
    bool tunnelMode)
{
    static const BOMX_VideoDecoderRole vp9Role = {"video_decoder.vp9", OMX_VIDEO_CodingVP9};
    BOMX_VideoDecoder *pVideoDecoder;
    unsigned i;
    bool vp9Supported = false;
    NEXUS_VideoDecoderCapabilities caps;
    NexusIPCClientBase *pIpcClient = NULL;
    uint64_t nexusClient = 0;

    pIpcClient = NexusIPCClientFactory::getClient(pName);
    if (pIpcClient)
    {
        nexusClient = pIpcClient->createClientContext();
    }
    if (!nexusClient)
    {
        ALOGW("Unable to determine presence of VP9 hardware!");
    }
    else
    {
        // Check if the platform supports VP9
        NEXUS_GetVideoDecoderCapabilities(&caps);
        for ( i = 0; i < caps.numVideoDecoders; i++ )
        {
            if ( caps.memory[i].supportedCodecs[NEXUS_VideoCodec_eVp9] )
            {
                vp9Supported = true;
                break;
            }
        }
    }

    if ( !vp9Supported )
    {
        ALOGW("VP9 hardware support is not available");
        goto error;
    }

    // VP9 can be disabled by this property
    if ( property_get_int32(B_PROPERTY_TRIM_VP9, 0) )
    {
        ALOGW("VP9 hardware support is available but disabled (ro.nx.trim.vp9=1)");
        goto error;
    }

    pVideoDecoder = new BOMX_VideoDecoder(pComponentTpe, pName, pAppData, pCallbacks,
                                          pIpcClient, nexusClient,
                                          false, tunnelMode, 1, &vp9Role, BOMX_VideoDecoder_GetRoleVp9);
    if ( NULL == pVideoDecoder )
    {
        goto error;
    }
    else
    {
        OMX_ERRORTYPE constructorError = pVideoDecoder->IsValid();
        if ( constructorError == OMX_ErrorNone )
        {
            return OMX_ErrorNone;
        }
        else
        {
            delete pVideoDecoder;
            return BOMX_ERR_TRACE(constructorError);
        }
    }

error:
    if (pIpcClient)
    {
        if (nexusClient)
        {
            pIpcClient->destroyClientContext(nexusClient);
        }
        delete pIpcClient;
    }
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_CreateVp9Tunnel(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    return BOMX_VideoDecoder_CreateVp9Common(pComponentTpe, pName, pAppData, pCallbacks, true);
}

extern "C" OMX_ERRORTYPE BOMX_VideoDecoder_CreateVp9(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    return BOMX_VideoDecoder_CreateVp9Common(pComponentTpe, pName, pAppData, pCallbacks, false);
}

extern "C" const char *BOMX_VideoDecoder_GetRole(unsigned roleIndex)
{
    if ( roleIndex >= g_numDefaultRoles )
    {
        return NULL;
    }
    else
    {
        return g_defaultRoles[roleIndex].name;
    }
}

extern "C" const char *BOMX_VideoDecoder_GetRoleVp9(unsigned roleIndex)
{
    if ( roleIndex > 0 )
    {
        return NULL;
    }
    else
    {
        return "video_decoder.vp9";
    }
}

static void BOMX_VideoDecoder_PlaypumpEvent(void *pParam)
{
    BOMX_VideoDecoder *pDecoder = static_cast <BOMX_VideoDecoder *> (pParam);

    ALOGV("PlaypumpEvent");

    pDecoder->PlaypumpEvent();
}

static void BOMX_VideoDecoder_EventCallback(void *pParam, int param)
{
    B_EventHandle hEvent;

    static const char *pEventMsg[BOMX_VideoDecoderEventType_eMax] = {
        "Playpump",
        "Data Ready",
        "Checkpoint",
        "SourceChanged",
        "StreamChanged"

    };

    hEvent = static_cast <B_EventHandle> (pParam);
    if ( param < BOMX_VideoDecoderEventType_eMax )
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

static void BOMX_VideoDecoder_OutputFrameEvent(void *pParam)
{
    BOMX_VideoDecoder *pDecoder = static_cast <BOMX_VideoDecoder *> (pParam);

    ALOGV("OutputFrameEvent");

    pDecoder->OutputFrameEvent();
}

static void BOMX_VideoDecoder_SourceChangedEvent(void *pParam)
{
    BOMX_VideoDecoder *pDecoder = static_cast <BOMX_VideoDecoder *> (pParam);

    ALOGV("SourceChangedEvent");

    pDecoder->SourceChangedEvent();
}

static void BOMX_VideoDecoder_StreamChangedEvent(void *pParam)
{
    BOMX_VideoDecoder *pDecoder = static_cast <BOMX_VideoDecoder *> (pParam);

    ALOGV("StreamChangedEvent");

    pDecoder->StreamChangedEvent();
}

static void BOMX_VideoDecoder_InputBuffersTimer(void *pParam)
{
    BOMX_VideoDecoder *pDecoder = static_cast <BOMX_VideoDecoder *> (pParam);
    ALOGV("%s: Run out of input buffers. Returning all completed buffers...", __FUNCTION__);
    pDecoder->InputBufferTimeoutCallback();
}

static void BOMX_VideoDecoder_FormatChangeTimer(void *pParam)
{
    BOMX_VideoDecoder *pDecoder = static_cast <BOMX_VideoDecoder *> (pParam);
    ALOGI("%s: Format change deferral timeout", __FUNCTION__);
    pDecoder->FormatChangeTimeoutCallback();
}

static OMX_ERRORTYPE BOMX_VideoDecoder_InitMimeType(OMX_VIDEO_CODINGTYPE eCompressionFormat, char *pMimeType)
{
    const char *pMimeTypeStr;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    ALOG_ASSERT(NULL != pMimeType);

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
        ALOGW("Unable to find MIME type for eCompressionFormat %u", eCompressionFormat);
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
    case NEXUS_VideoProtocolProfile_eMain10:   return OMX_VIDEO_HEVCProfileMain10;
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

static OMX_VIDEO_VP9PROFILETYPE BOMX_VP9ProfileFromNexus(NEXUS_VideoProtocolProfile nexusProfile)
{
    switch ( nexusProfile )
    {
    default:
    case NEXUS_VideoProtocolProfile_eSimple:
    case NEXUS_VideoProtocolProfile_eMain:   return OMX_VIDEO_VP9Profile0;
    case NEXUS_VideoProtocolProfile_eHigh:   return OMX_VIDEO_VP9Profile2;
    }
}

static OMX_VIDEO_VP9LEVELTYPE BOMX_VP9LevelFromNexus(NEXUS_VideoProtocolLevel nexusLevel)
{
    switch ( nexusLevel )
    {
    default: return OMX_VIDEO_VP9LevelUnknown;
    case NEXUS_VideoProtocolLevel_e10: return OMX_VIDEO_VP9Level1;
    case NEXUS_VideoProtocolLevel_e11: return OMX_VIDEO_VP9Level11;
    case NEXUS_VideoProtocolLevel_e20: return OMX_VIDEO_VP9Level2;
    case NEXUS_VideoProtocolLevel_e21: return OMX_VIDEO_VP9Level21;
    case NEXUS_VideoProtocolLevel_e30: return OMX_VIDEO_VP9Level3;
    case NEXUS_VideoProtocolLevel_e31: return OMX_VIDEO_VP9Level31;
    case NEXUS_VideoProtocolLevel_e40: return OMX_VIDEO_VP9Level4;
    case NEXUS_VideoProtocolLevel_e41: return OMX_VIDEO_VP9Level41;
    case NEXUS_VideoProtocolLevel_e50: return OMX_VIDEO_VP9Level5;
    case NEXUS_VideoProtocolLevel_e51: return OMX_VIDEO_VP9Level51;
    }
}

static NEXUS_VideoFrameRate BOMX_FrameRateToNexus(OMX_TICKS intervalUs)
{
    if ( intervalUs <= 0 )                  return NEXUS_VideoFrameRate_eUnknown;

    if ( intervalUs <= 9001 )               return NEXUS_VideoFrameRate_e120;
    else if ( intervalUs <= 10001 )         return NEXUS_VideoFrameRate_e119_88;
    else if ( intervalUs <= 17001 )         return NEXUS_VideoFrameRate_e60;
    else if ( intervalUs <= 18001 )         return NEXUS_VideoFrameRate_e59_94;
    else if ( intervalUs <= 21001 )         return NEXUS_VideoFrameRate_e50;
    else if ( intervalUs <= 34001 )         return NEXUS_VideoFrameRate_e30;
    else if ( intervalUs <= 35001 )         return NEXUS_VideoFrameRate_e29_97;
    else if ( intervalUs <= 41001 )         return NEXUS_VideoFrameRate_e25;
    else if ( intervalUs <= 42001 )         return NEXUS_VideoFrameRate_e24;
    else if ( intervalUs <= 43001 )         return NEXUS_VideoFrameRate_e23_976;
    else if ( intervalUs <= 51001 )         return NEXUS_VideoFrameRate_e20;
    else if ( intervalUs <= 52001 )         return NEXUS_VideoFrameRate_e19_98;
    else if ( intervalUs <= 67001 )         return NEXUS_VideoFrameRate_e15;
    else if ( intervalUs <= 68001 )         return NEXUS_VideoFrameRate_e14_985;
    else if ( intervalUs <= 81001 )         return NEXUS_VideoFrameRate_e12_5;
    else if ( intervalUs <= 84001 )         return NEXUS_VideoFrameRate_e12;
    else if ( intervalUs <= 85001 )         return NEXUS_VideoFrameRate_e11_988;
    else if ( intervalUs <= 101001 )        return NEXUS_VideoFrameRate_e10;
    else if ( intervalUs <= 102001 )        return NEXUS_VideoFrameRate_e9_99;
    else if ( intervalUs <= 134001 )        return NEXUS_VideoFrameRate_e7_5;
    else if ( intervalUs <= 135001 )        return NEXUS_VideoFrameRate_e7_493;

    return NEXUS_VideoFrameRate_eUnknown;
}

static int BOMX_VideoDecoder_GetFrameInterval(NEXUS_VideoFrameRate frameRate)
{
    unsigned i;
    for ( i = 0; i < g_numFrameRateInfos; i++ )
    {
        if ( g_frameRateInfo[i].rate == frameRate )
        {
            return g_frameRateInfo[i].interval;
        }
    }

    ALOGW("WARNING: Unknown NEXUS frame rate enum %d", frameRate);

    ALOG_ASSERT(g_frameRateInfo[0].rate == NEXUS_VideoFrameRate_eUnknown);
    return g_frameRateInfo[0].interval;
}

static const char *BOMX_VideoDecoder_GetFrameString(NEXUS_VideoFrameRate frameRate)
{
    unsigned i;
    for ( i = 0; i < g_numFrameRateInfos; i++ )
    {
        if ( g_frameRateInfo[i].rate == frameRate )
        {
            return g_frameRateInfo[i].str;
        }
    }

    ALOGW("WARNING: Unknown NEXUS frame rate enum %d", frameRate);
    return "unknown";
}

void OmxBinder::notify(int msg, struct hwc_notification_info &ntfy)
{
   ALOGV( "%s: notify received: msg=%u", __FUNCTION__, msg);

   if (cb)
      cb(cb_data, msg, ntfy);
}

static void BOMX_OmxBinderNotify(void *cb_data, int msg, struct hwc_notification_info &ntfy)
{
    BOMX_VideoDecoder *pComponent = (BOMX_VideoDecoder *)cb_data;

    ALOG_ASSERT(NULL != pComponent);

    switch (msg)
    {
    case HWC_BINDER_NTFY_DISPLAY:
    {
        pComponent->BinderNotifyDisplay(ntfy);
        break;
    }
    case HWC_BINDER_NTFY_SIDEBAND_SURFACE_GEOMETRY_UPDATE:
    {
       NEXUS_Rect position, clip;
       position.x = ntfy.frame.x;
       position.y = ntfy.frame.y;
       position.width = ntfy.frame.w;
       position.height = ntfy.frame.h;
       clip.x = ntfy.clipped.x;
       clip.y = ntfy.clipped.y;
       clip.width = ntfy.clipped.w;
       clip.height = ntfy.clipped.h;
       pComponent->SetVideoGeometry(&position, &clip, ntfy.frame_id, ntfy.display_width, ntfy.display_height, ntfy.zorder, true);
       break;
    }
    default:
        break;
    }
}

static void *BOMX_DisplayThread(void *pParam)
{
    BOMX_VideoDecoder *pComponent = static_cast <BOMX_VideoDecoder *> (pParam);

    ALOGV("Display Thread Start");
    pComponent->RunDisplayThread();
    ALOGV("Display Thread Exited");

    return NULL;
}

static size_t ComputeBufferSize(unsigned stride, unsigned height)
{
    return (stride * height * 3) / 2;
}

static void BOMX_VideoDecoder_MemLock(private_handle_t *pPrivateHandle, void **addr)
{
   *addr = NULL;
   NEXUS_MemoryBlockHandle block_handle = NULL;
   private_handle_t::get_block_handles(pPrivateHandle, &block_handle, NULL);
   NEXUS_MemoryBlock_Lock(block_handle, addr);
}

static void BOMX_VideoDecoder_MemUnlock(private_handle_t *pPrivateHandle)
{
   NEXUS_MemoryBlockHandle block_handle = NULL;
   private_handle_t::get_block_handles(pPrivateHandle, &block_handle, NULL);
   NEXUS_MemoryBlock_Unlock(block_handle);
}

static int BOMX_VideoDecoder_OpenMemoryInterface(void)
{
   int memBlkFd = -1;
   char device[PROPERTY_VALUE_MAX];
   char name[PROPERTY_VALUE_MAX];

   memset(device, 0, sizeof(device));
   memset(name, 0, sizeof(name));

   property_get(B_PROPERTY_MEMBLK_ALLOC, device, NULL);
   if (strlen(device)) {
      strcpy(name, "/dev/");
      strcat(name, device);
      memBlkFd = open(name, O_RDWR, 0);
   }

   return memBlkFd;
}

static int BOMX_VideoDecoder_AdvertisePresence(bool secure)
{
   int memBlkFd = -1;

   memBlkFd = BOMX_VideoDecoder_OpenMemoryInterface();
   if (memBlkFd >= 0) {
      struct nx_ashmem_alloc ashmem_alloc;
      memset(&ashmem_alloc, 0, sizeof(struct nx_ashmem_alloc));
      ashmem_alloc.marker = secure ? NX_ASHMEM_MARKER_VIDEO_DEC_SECURE : NX_ASHMEM_MARKER_VIDEO_DECODER;
      int ret = ioctl(memBlkFd, NX_ASHMEM_SET_SIZE, &ashmem_alloc);
      if (ret < 0) {
         close(memBlkFd);
         memBlkFd = -1;
      }
   }

   return memBlkFd;
}

static NEXUS_MemoryBlockHandle BOMX_VideoDecoder_AllocatePixelMemoryBlk(const NEXUS_SurfaceCreateSettings *pCreateSettings, int *pMemBlkFd)
{
   NEXUS_MemoryBlockHandle hMemBlk = NULL;
   int memBlkFd = -1;
   char device[PROPERTY_VALUE_MAX];
   char name[PROPERTY_VALUE_MAX];

   if (pCreateSettings && pMemBlkFd) {
      memBlkFd = BOMX_VideoDecoder_OpenMemoryInterface();
      if (memBlkFd >= 0) {
         struct nx_ashmem_alloc ashmem_alloc;
         memset(&ashmem_alloc, 0, sizeof(struct nx_ashmem_alloc));
         ashmem_alloc.size = pCreateSettings->height * pCreateSettings->pitch;
         ashmem_alloc.align = 4096;
         int ret = ioctl(memBlkFd, NX_ASHMEM_SET_SIZE, &ashmem_alloc);
         if (ret < 0) {
            close(memBlkFd);
            memBlkFd = -1;
         } else {
            struct nx_ashmem_getmem ashmem_getmem;
            memset(&ashmem_getmem, 0, sizeof(struct nx_ashmem_getmem));
            ret = ioctl(memBlkFd, NX_ASHMEM_GETMEM, &ashmem_getmem);
            if (ret < 0) {
               close(memBlkFd);
               memBlkFd = -1;
            } else {
               hMemBlk = (NEXUS_MemoryBlockHandle)ashmem_getmem.hdl;
            }
         }
      }
      *pMemBlkFd = memBlkFd;
   }

   return hMemBlk;
}

static void BOMX_VideoDecoder_SurfaceDestroy(int *pMemBlkFd, NEXUS_SurfaceHandle surface)
{
   if (surface != NULL) {
      NEXUS_Surface_Unlock(surface);
      NEXUS_Surface_Destroy(surface);
   }

   if (pMemBlkFd && (*pMemBlkFd >= 0)) {
      close(*pMemBlkFd);
      *pMemBlkFd = -1;
   }

   return;
}

static NEXUS_SurfaceHandle BOMX_VideoDecoder_ToNexusSurface(int width, int height, int stride, NEXUS_PixelFormat format,
                                                            NEXUS_MemoryBlockHandle handle, unsigned offset)
{
    NEXUS_SurfaceHandle shdl = NULL;
    NEXUS_SurfaceCreateSettings createSettings;

    NEXUS_Surface_GetDefaultCreateSettings(&createSettings);
    createSettings.pixelFormat   = format;
    createSettings.width         = width;
    createSettings.height        = height;
    createSettings.pitch         = stride;
    createSettings.managedAccess = false;
    if (handle) {
       createSettings.pixelMemory = (NEXUS_MemoryBlockHandle) handle;
       createSettings.pixelMemoryOffset = offset;
    }

    return NEXUS_Surface_Create(&createSettings);
}

static void BOMX_VideoDecoder_StripedSurfaceDestroy(BOMX_VideoDecoderFrameBuffer *pFrameBuffer)
{
   if (pFrameBuffer->hStripedSurface) {
      NEXUS_StripedSurface_Destroy(pFrameBuffer->hStripedSurface);
      pFrameBuffer->hStripedSurface = NULL;
   }
}

static bool BOMX_VideoDecoder_SetupRuntimeHeaps(bool secureDecoder, bool secureHeap)
{
   unsigned i;
   NEXUS_Error errCode;
   NEXUS_PlatformConfiguration platformConfig;
   NEXUS_PlatformSettings platformSettings;
   NEXUS_MemoryStatus memoryStatus;
   char value[PROPERTY_VALUE_MAX];

   if (property_get_int32(B_PROPERTY_DISABLE_RUNTIME_HEAPS, 0))
   {
      return false;
   }

   memset(value, 0, sizeof(value));
   property_get(B_PROPERTY_SVP, value, "play");
   if (strncmp(value, "none", strlen("none")))
   {
      NEXUS_Platform_GetConfiguration(&platformConfig);
      for (i = 0; i < NEXUS_MAX_HEAPS ; i++)
      {
         if (platformConfig.heap[i] != NULL)
         {
            errCode = NEXUS_Heap_GetStatus(platformConfig.heap[i], &memoryStatus);
            if (!errCode && (memoryStatus.heapType & NEXUS_HEAP_TYPE_PICTURE_BUFFERS))
            {
               NEXUS_HeapRuntimeSettings settings;
               bool origin, wanted;
               NEXUS_Platform_GetHeapRuntimeSettings(platformConfig.heap[i], &settings);
               origin = settings.secure;
               wanted = secureHeap ? true : false;
               if (origin != wanted)
               {
                  settings.secure = wanted;
                  errCode = NEXUS_Platform_SetHeapRuntimeSettings(platformConfig.heap[i], &settings);
                  if (errCode)
                  {
                     ALOGE("NEXUS_Platform_SetHeapRuntimeSettings(%i:%p, %s) on decoder %s -> failed: %d",
                            i, platformConfig.heap[i], settings.secure?"secure":"open", secureDecoder?"secure":"open", errCode);
                     /* Continue anyways, something may still work... */
                     if (origin && !wanted)
                     {
                        return false;
                     }
                  }
                  else
                  {
                     ALOGI("NEXUS_Platform_SetHeapRuntimeSettings(%i:%p, %s) on decoder %s -> success",
                           i, platformConfig.heap[i], settings.secure?"secure":"open", secureDecoder?"secure":"open");
                  }
               }
            }
         }
      }
   }
   return true;
}

BOMX_VideoDecoder::BOMX_VideoDecoder(
    OMX_COMPONENTTYPE *pComponentType,
    const OMX_STRING pName,
    const OMX_PTR pAppData,
    const OMX_CALLBACKTYPE *pCallbacks,
    NexusIPCClientBase *pIpcClient,
    uint64_t nexusClient,
    bool secure,
    bool tunnel,
    unsigned numRoles,
    const BOMX_VideoDecoderRole *pRoles,
    const char *(*pGetRole)(unsigned roleIndex)) :
    BOMX_Component(pComponentType, pName, pAppData, pCallbacks, (pGetRole!=NULL) ? pGetRole : BOMX_VideoDecoder_GetRole,
                   true, PRIORITY_URGENT_DISPLAY + 2*PRIORITY_MORE_FAVORABLE),
    m_hSimpleVideoDecoder(NULL),
    m_hPlaypump(NULL),
    m_hPidChannel(NULL),
    m_hPlaypumpEvent(NULL),
    m_playpumpEventId(NULL),
    m_playpumpTimerId(NULL),
    m_hOutputFrameEvent(NULL),
    m_outputFrameEventId(NULL),
    m_hSourceChangedEvent(NULL),
    m_sourceChangedEventId(NULL),
    m_hStreamChangedEvent(NULL),
    m_streamChangedEventId(NULL),
    m_hDisplayFrameEvent(NULL),
    m_displayFrameEventId(NULL),
    m_hDisplayMutex(NULL),
    m_inputBuffersTimerId(NULL),
    m_hCheckpointEvent(NULL),
    m_hGraphics2d(NULL),
    m_submittedDescriptors(0),
    m_maxDescriptorsPerBuffer(0),
    m_pPlaypumpDescList(NULL),
    m_completedInputBuffers(0),
    m_pBufferTracker(NULL),
    m_AvailInputBuffers(0),
    m_frameRate(NEXUS_VideoFrameRate_eUnknown),
    m_frEstimated(false),
    m_frStableCount(0),
    m_deltaUs(0),
    m_lastTsUs(B_TIMESTAMP_INVALID),
    m_pIpcClient(pIpcClient),
    m_nexusClient(nexusClient),
    m_nxClientId(NXCLIENT_INVALID_ID),
    m_hSurfaceClient(NULL),
    m_hVideoClient(NULL),
    m_hAlphaSurface(NULL),
    m_setSurface(false),
    m_pPesFile(NULL),
    m_pInputFile(NULL),
    m_pPes(NULL),
    m_pEosBuffer(NULL),
    m_eosPending(false),
    m_eosDelivered(false),
    m_eosReceived(false),
    m_formatChangeState(FCState_eNone),
    m_formatChangeTimerId(NULL),
    m_nativeGraphicsEnabled(false),
    m_metadataEnabled(false),
    m_adaptivePlaybackEnabled(false),
    m_secureDecoder(secure),
    m_tunnelMode(tunnel),
    m_tunnelHfr(false),
    m_pTunnelNativeHandle(NULL),
    m_tunnelCurrentPts(0),
    m_waitingForStc(false),
    m_flushTime(0),
    m_stcSyncValue(B_STC_SYNC_INVALID_VALUE),
    m_stcResumePending(false),
    m_outputWidth(1920),
    m_outputHeight(1080),
    m_maxDecoderWidth(1920),
    m_maxDecoderHeight(1080),
    m_pRoles(NULL),
    m_numRoles(0),
    m_pConfigBuffer(NULL),
    m_configBufferState(ConfigBufferState_eAccumulating),
    m_configBufferSize(0),
    m_outputMode(BOMX_VideoDecoderOutputBufferType_eStandard),
    m_omxHwcBinder(NULL),
    m_memTracker(-1),
    m_secureRuntimeHeaps(false),
    m_frameSerial(0),
    m_displayFrameAvailable(false),
    m_displayThreadStop(false),
    m_hDisplayThread(0),
    m_lastReturnedSerial(0),
    m_droppedFrames(0),
    m_consecDroppedFrames(0),
    m_maxConsecDroppedFrames(0),
    m_earlyDroppedFrames(0),
    m_earlyDropThresholdMs(0),
    m_startTime(-1),
    m_tunnelStcChannel(NULL),
    m_tunnelStcChannelSync(NULL),
    m_inputFlushing(false),
    m_outputFlushing(false),
    m_ptsReceived(false),
    m_hdrStaticInfoSet(false),
    m_colorAspectsSet(false)
{
    unsigned i;
    NEXUS_Error errCode;
    NEXUS_ClientConfiguration clientConfig;
    char value[PROPERTY_VALUE_MAX];

    BLST_Q_INIT(&m_frameBufferFreeList);
    BLST_Q_INIT(&m_frameBufferAllocList);

    #define MAX_PORT_FORMATS (6)
    #define MAX_OUTPUT_PORT_FORMATS (2)

    BDBG_CASSERT(MAX_OUTPUT_PORT_FORMATS <= MAX_PORT_FORMATS);

    OMX_VIDEO_PORTDEFINITIONTYPE portDefs;
    OMX_VIDEO_PARAM_PORTFORMATTYPE portFormats[MAX_PORT_FORMATS];

    m_memTracker = BOMX_VideoDecoder_AdvertisePresence(m_secureDecoder);

    m_videoPortBase = 0;    // Android seems to require this - was: BOMX_COMPONENT_PORT_BASE(BOMX_ComponentId_eVideoDecoder, OMX_PortDomainVideo);

    if ( numRoles==0 || pRoles==NULL )
    {
        pRoles = g_defaultRoles;
        numRoles = g_numDefaultRoles;
    }
    if ( numRoles > MAX_PORT_FORMATS )
    {
        ALOGW("Warning, exceeded max number of video decoder input port formats");
        numRoles = MAX_PORT_FORMATS;
    }

    m_numRoles = numRoles;
    m_pRoles = new BOMX_VideoDecoderRole[numRoles];
    if ( NULL == m_pRoles )
    {
        ALOGE("Unable to allocate role memory");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }
    BKNI_Memcpy(m_pRoles, pRoles, numRoles*sizeof(BOMX_VideoDecoderRole));

    memset(&portDefs, 0, sizeof(portDefs));
    portDefs.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)pRoles[0].omxCodec;
    portDefs.cMIMEType = m_inputMimeType;
    (void)BOMX_VideoDecoder_InitMimeType(portDefs.eCompressionFormat, m_inputMimeType);
    for ( i = 0; i < numRoles; i++ )
    {
        memset(&portFormats[i], 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        BOMX_STRUCT_INIT(&portFormats[i]);
        portFormats[i].nPortIndex = m_videoPortBase;
        portFormats[i].nIndex = i;
        portFormats[i].eCompressionFormat = (OMX_VIDEO_CODINGTYPE)pRoles[i].omxCodec;
    }

    SetRole(m_pRoles[0].name);
    m_maxDescriptorsPerBuffer = B_MAX_DESCRIPTORS_PER_BUFFER(B_DATA_BUFFER_SIZE_DEFAULT);
    m_numVideoPorts = 0;
    m_pVideoPorts[0] = new BOMX_VideoPort(m_videoPortBase, OMX_DirInput, B_NUM_BUFFERS, B_DATA_BUFFER_SIZE_DEFAULT, false, 0, &portDefs, portFormats, numRoles);
    if ( NULL == m_pVideoPorts[0] )
    {
        ALOGW("Unable to create video input port");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }
    m_numVideoPorts = 1;
    memset(&portDefs, 0, sizeof(portDefs));
    portDefs.eCompressionFormat = OMX_VIDEO_CodingUnused;
    portDefs.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    strcpy(m_outputMimeType, "video/x-raw");
    portDefs.cMIMEType = m_outputMimeType;
    portDefs.nFrameWidth = m_outputWidth;
    portDefs.nFrameHeight = m_outputHeight;
    portDefs.nStride = portDefs.nFrameWidth;
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
            portFormats[i].eColorFormat = OMX_COLOR_FormatYUV420Planar;
            break;
        case 1: // Native grpahics format
            portFormats[i].eColorFormat = (OMX_COLOR_FORMATTYPE)((int)HAL_PIXEL_FORMAT_YV12);
            break;
        }
    }
    m_pVideoPorts[1] = new BOMX_VideoPort(m_videoPortBase+1, OMX_DirOutput, B_MAX_FRAMES, ComputeBufferSize(portDefs.nStride, portDefs.nSliceHeight), false, 0, &portDefs, portFormats, MAX_OUTPUT_PORT_FORMATS);
    if ( NULL == m_pVideoPorts[1] )
    {
        ALOGW("Unable to create video output port");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }
    m_numVideoPorts = 2;

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

    m_outputFrameEventId = this->RegisterEvent(m_hOutputFrameEvent, BOMX_VideoDecoder_OutputFrameEvent, static_cast <void *> (this));
    if ( NULL == m_outputFrameEventId )
    {
        ALOGW("Unable to register output frame event");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    m_hSourceChangedEvent = B_Event_Create(NULL);
    if ( NULL == m_hSourceChangedEvent )
    {
        ALOGW("Unable to create source changed event");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    m_sourceChangedEventId = this->RegisterEvent(m_hSourceChangedEvent, BOMX_VideoDecoder_SourceChangedEvent, static_cast <void *> (this));
    if ( NULL == m_sourceChangedEventId )
    {
        ALOGW("Unable to register source changed event");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    m_hStreamChangedEvent = B_Event_Create(NULL);
    if ( NULL == m_hStreamChangedEvent )
    {
        ALOGW("Unable to create stream changed event");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    m_streamChangedEventId = this->RegisterEvent(m_hStreamChangedEvent, BOMX_VideoDecoder_StreamChangedEvent, static_cast <void *> (this));
    if ( NULL == m_streamChangedEventId )
    {
        ALOGW("Unable to register stream changed event");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    if ( BKNI_CreateEvent(&m_hDisplayFrameEvent) != BERR_SUCCESS )
    {
        ALOGW("Unable to create display frame event");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    m_hDisplayMutex = B_Mutex_Create(NULL);
    if ( NULL == m_hDisplayMutex )
    {
        ALOGW("Unable to create display mutex");
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

    if (m_pIpcClient == NULL)
    {
        m_pIpcClient = NexusIPCClientFactory::getClient(pName);
        if ( NULL == m_pIpcClient )
        {
            ALOGW("Unable to create client factory");
            this->Invalidate(OMX_ErrorUndefined);
            return;
        }
    }


    if (!m_nexusClient)
    {
        m_nexusClient = m_pIpcClient->createClientContext();
        if (!m_nexusClient)
        {
            ALOGW("Unable to create nexus client context");
            this->Invalidate(OMX_ErrorUndefined);
            return;
        }
    }

    // create a persistent nxclient connection on the process this component runs,
    // in order to help speeding up subsequent initialization of other components running
    // in this same process.
    g_persistNxClientLock.lock();
    if ( !g_persistNxClientOn )
    {
        NEXUS_Error rc = NEXUS_SUCCESS;
        NxClient_JoinSettings joinSettings;

        NxClient_GetDefaultJoinSettings(&joinSettings);
        joinSettings.ignoreStandbyRequest = true;
        do {
            rc = NxClient_Join(&joinSettings);
            if (rc != NEXUS_SUCCESS) {
                usleep(1 * 1000);
            }
        } while (rc != NEXUS_SUCCESS);
        g_persistNxClientOn = true;
    }
    g_persistNxClientLock.unlock();

    if (property_get_int32(B_PROPERTY_DTU, 0))
    {
       // guaranteed by the dtu.
       m_secureRuntimeHeaps = m_secureDecoder;
    }
    else
    {
       g_decActiveStateLock.lock();
       g_decInstanceCount++;
       // sanity checking
       if ( (g_decInstanceCount == 1) && (g_decActiveState != B_DEC_ACTIVE_STATE_INACTIVE) )
       {
           ALOGW("Active state must be inactive for the first decoder instance!");
           g_decActiveStateLock.unlock();
           this->Invalidate(OMX_ErrorUndefined);
           return;
       }

       if ( (m_secureDecoder && g_decActiveState == B_DEC_ACTIVE_STATE_ACTIVE_CLEAR) ||
            (!m_secureDecoder && g_decActiveState == B_DEC_ACTIVE_STATE_ACTIVE_SECURE) )
       {
           ALOGW("Unable to set up runtime heap while %s decoder still active", m_secureDecoder ? "non-secure" : "secure");
           g_decActiveStateLock.unlock();
           this->Invalidate(OMX_ErrorInsufficientResources);
           return;
       }

       m_secureRuntimeHeaps = m_secureDecoder;
       if ( (g_decInstanceCount == 1) && !BOMX_VideoDecoder_SetupRuntimeHeaps(m_secureDecoder, m_secureDecoder) )
       {
           BOMX_VideoDecoder_SetupRuntimeHeaps(m_secureDecoder, true);
           // failed, so forced to assume picture buffer heaps are secured.
           m_secureRuntimeHeaps = true;
       }

       g_decActiveState = m_secureRuntimeHeaps ? B_DEC_ACTIVE_STATE_ACTIVE_SECURE : B_DEC_ACTIVE_STATE_ACTIVE_CLEAR;
       g_decActiveStateLock.unlock();
    }

    NxClient_AllocSettings nxAllocSettings;
    NxClient_GetDefaultAllocSettings(&nxAllocSettings);
    nxAllocSettings.simpleVideoDecoder = 1;
    nxAllocSettings.surfaceClient = 1;
    errCode = NxClient_Alloc(&nxAllocSettings, &m_allocResults);
    if ( errCode )
    {
        ALOGW("NxClient_Alloc failed");
        this->Invalidate(OMX_ErrorInsufficientResources);
        return;
    }

    NEXUS_VideoFormatInfo videoInfo;
    NEXUS_VideoDecoderCapabilities caps;

    // Check the decoder capabilities for the highest resolution.
    NEXUS_GetVideoDecoderCapabilities(&caps);
    for ( i = 0; i < caps.numVideoDecoders; i++ )
    {
        NEXUS_VideoFormat_GetInfo(caps.memory[i].maxFormat, &videoInfo);
        if ( videoInfo.width > m_maxDecoderWidth ) {
            m_maxDecoderWidth = videoInfo.width;
        }
        if ( videoInfo.height > m_maxDecoderHeight ) {
            m_maxDecoderHeight = videoInfo.height;
        }
    }

    ALOGV("max decoder height:%u width:%u", m_maxDecoderHeight, m_maxDecoderWidth);
    NxClient_ConnectSettings connectSettings;
    NxClient_GetDefaultConnectSettings(&connectSettings);
    connectSettings.simpleVideoDecoder[0].id = m_allocResults.simpleVideoDecoder[0].id;
    connectSettings.simpleVideoDecoder[0].surfaceClientId = m_allocResults.surfaceClient[0].id;
    connectSettings.simpleVideoDecoder[0].windowId = 0;
    connectSettings.simpleVideoDecoder[0].windowCapabilities.type = NxClient_VideoWindowType_eMain; // TODO: Support Main/Pip
    for ( i = 0; i < m_numRoles; i++ )
    {
        connectSettings.simpleVideoDecoder[0].decoderCapabilities.supportedCodecs[GetNexusCodec((OMX_VIDEO_CODINGTYPE)m_pRoles[i].omxCodec)] = true;
    }
    connectSettings.simpleVideoDecoder[0].decoderCapabilities.maxWidth = m_maxDecoderWidth;
    connectSettings.simpleVideoDecoder[0].decoderCapabilities.maxHeight = m_maxDecoderHeight;

    memset(value, 0, sizeof(value));
    property_get(B_PROPERTY_SVP, value, "play");
    if (strncmp(value, "none", strlen("none")))
    {
       connectSettings.simpleVideoDecoder[0].decoderCapabilities.secureVideo = m_secureDecoder ? true : false;
    }

    errCode = NxClient_Connect(&connectSettings, &m_nxClientId);
    if ( errCode )
    {
        ALOGW("NxClient_Connect failed.  Resources may be exhausted.");
        (void)BOMX_BERR_TRACE(errCode);
        NxClient_Free(&m_allocResults);
        this->Invalidate(OMX_ErrorInsufficientResources);
        return;
    }

    m_hSurfaceClient = NEXUS_SurfaceClient_Acquire(m_allocResults.surfaceClient[0].id);
    if ( NULL == m_hSurfaceClient )
    {
        ALOGW("Unable to acquire surface client");
        NxClient_Disconnect(m_nxClientId);
        NxClient_Free(&m_allocResults);
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }
    m_hVideoClient = NEXUS_SurfaceClient_AcquireVideoWindow(m_hSurfaceClient, 0);
    if ( NULL == m_hVideoClient )
    {
        ALOGW("Unable to acquire video client");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    // Initialize video window to full screen
    NEXUS_SurfaceClientSettings videoClientSettings;
    NEXUS_SurfaceClient_GetSettings(m_hVideoClient, &videoClientSettings);
    videoClientSettings.composition.virtualDisplay.width = m_outputWidth;
    videoClientSettings.composition.virtualDisplay.height = m_outputHeight;
    videoClientSettings.composition.position.x = 0;
    videoClientSettings.composition.position.y = 0;
    videoClientSettings.composition.position.width = m_outputWidth;
    videoClientSettings.composition.position.height = m_outputHeight;
    videoClientSettings.composition.clipRect = videoClientSettings.composition.position;    // No clipping
    videoClientSettings.composition.contentMode = NEXUS_VideoWindowContentMode_eFull;
    errCode = NEXUS_SurfaceClient_SetSettings(m_hVideoClient, &videoClientSettings);
    if ( errCode )
    {
        ALOGW("Unable to setup video initial size");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    NEXUS_SurfaceCreateSettings surfaceCreateSettings;
    NEXUS_Surface_GetDefaultCreateSettings(&surfaceCreateSettings);
    surfaceCreateSettings.height = surfaceCreateSettings.width = 10;
    surfaceCreateSettings.pixelFormat = NEXUS_PixelFormat_eA8;
    m_hAlphaSurface = NEXUS_Surface_Create(&surfaceCreateSettings);
    if ( NULL == m_hAlphaSurface )
    {
        ALOGW("Unable to create alpha surface");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    NEXUS_SurfaceMemory surfaceMemory;
    errCode = NEXUS_Surface_GetMemory(m_hAlphaSurface, &surfaceMemory);
    if ( errCode )
    {
        ALOGW("Unable to fill surface");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }
    BKNI_Memset(surfaceMemory.buffer, 0, surfaceCreateSettings.height * surfaceMemory.pitch);
    NEXUS_Surface_Flush(m_hAlphaSurface);

    NEXUS_MemoryAllocationSettings          memorySettings;

    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    NEXUS_Memory_GetDefaultAllocationSettings(&memorySettings);
    memorySettings.heap = clientConfig.heap[1];
    errCode = NEXUS_Memory_Allocate(BOMX_VIDEO_EOS_LEN, &memorySettings, &m_pEosBuffer);
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

    /* Populate EOS buffer */
    char *pBuffer = (char *)m_pEosBuffer;
    m_pPes->FormBppPacket(pBuffer, 0xa); /* Inline flush / TPD */
    m_pPes->FormBppPacket(pBuffer+B_BPP_PACKET_LEN, 0x82); /* LAST */
    m_pPes->FormBppPacket(pBuffer+(2*B_BPP_PACKET_LEN), 0xa); /* Inline flush / TPD */
    NEXUS_FlushCache(pBuffer, BOMX_VIDEO_EOS_LEN);

    // Allocate and fill end-of-chunk buffer for vp9
    errCode = NEXUS_Memory_Allocate(B_BPP_PACKET_LEN, &memorySettings, &m_pEndOfChunkBuffer);
    pBuffer = (char *)m_pEndOfChunkBuffer;
    m_pPes->FormBppPacket(pBuffer, 0x85);
    NEXUS_FlushCache(pBuffer, B_BPP_PACKET_LEN);

    m_hCheckpointEvent = B_Event_Create(NULL);
    if ( NULL == m_hCheckpointEvent )
    {
        ALOGW("Unable to create checkpoint event");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }

    NEXUS_Graphics2DOpenSettings g2dOpenSettings;
    NEXUS_Graphics2D_GetDefaultOpenSettings(&g2dOpenSettings);
    g2dOpenSettings.compatibleWithSurfaceCompaction = false;
    m_hGraphics2d = NEXUS_Graphics2D_Open(NEXUS_ANY_ID, &g2dOpenSettings);
    if ( NULL == m_hGraphics2d )
    {
        ALOGW("Unable to open graphics 2d");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }
    else
    {
        NEXUS_Graphics2DSettings gfxSettings;
        NEXUS_Graphics2D_GetSettings(m_hGraphics2d, &gfxSettings);
        gfxSettings.checkpointCallback.callback = BOMX_VideoDecoder_EventCallback;
        gfxSettings.checkpointCallback.context = static_cast<void *>(m_hCheckpointEvent);
        gfxSettings.checkpointCallback.param = (int)BOMX_VideoDecoderEventType_eCheckpoint;
        gfxSettings.pollingCheckpoint = false;
        errCode = NEXUS_Graphics2D_SetSettings(m_hGraphics2d, &gfxSettings);
        if ( errCode )
        {
            errCode = BOMX_BERR_TRACE(errCode);
            this->Invalidate(OMX_ErrorUndefined);
            return;
        }
    }

    for ( i = 0; i < B_MAX_DECODED_FRAMES; i++ )
    {
        BOMX_VideoDecoderFrameBuffer *pBuffer = new BOMX_VideoDecoderFrameBuffer;
        if ( NULL == pBuffer )
        {
            errCode = BOMX_BERR_TRACE(BERR_OUT_OF_SYSTEM_MEMORY);
            this->Invalidate(OMX_ErrorUndefined);
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

    int32_t pesDebug, inputDebug;
    pesDebug = property_get_int32(B_PROPERTY_PES_DEBUG, 0);
    inputDebug = property_get_int32(B_PROPERTY_INPUT_DEBUG, 0);
    // Setup file to debug input or PES data packing if required
    if ( pesDebug || inputDebug )
    {
        time_t rawtime;
        struct tm * timeinfo;
        char fname[100];

        // Generate unique file name
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        if ( pesDebug )
        {
            strftime(fname, sizeof(fname), "/data/nxmedia/vdec-%F_%H_%M_%S.pes", timeinfo);
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
            strftime(fname, sizeof(fname), "/data/nxmedia/vdec-%F_%H_%M_%S.input", timeinfo);
            ALOGD("Input debug output file:%s", fname);
            m_pInputFile = fopen(fname, "wb+");
            if ( NULL == m_pInputFile )
            {
                ALOGW("Error creating input debug file %s (%s)", fname, strerror(errno));
                // Just keep going without debug
            }
        }
    }

    // connect to the HWC binder.
    m_omxHwcBinder = new OmxBinder_wrap;
    if ( NULL == m_omxHwcBinder )
    {
        ALOGW("Unable to connect to HwcBinder");
        this->Invalidate(OMX_ErrorUndefined);
        return;
    }
    m_omxHwcBinder->get()->register_notify(&BOMX_OmxBinderNotify, this);
    if (m_tunnelMode)
    {
       int sidebandId;
       m_omxHwcBinder->getsideband(0, sidebandId);
       m_pTunnelNativeHandle = native_handle_create(0, 2);
       if (!m_pTunnelNativeHandle)
       {
          ALOGE("Unable to create native handle for tunnel mode");
          this->Invalidate(OMX_ErrorUndefined);
          return;
       }
       m_pTunnelNativeHandle->data[0] = 2;
       m_pTunnelNativeHandle->data[1] = sidebandId;

       m_outputMode = BOMX_VideoDecoderOutputBufferType_eNone;

       m_tunnelHfr = ( property_get_int32(B_PROPERTY_TUNNELED_HFRVIDEO, 0) != 0 );
    }
    else
    {
        int surfaceClientId;
        m_omxHwcBinder->getvideo(0, surfaceClientId);
    }

    m_videoStreamInfo.valid = false;
    m_hdrStaticInfoSet = false;
    m_colorAspectsSet = false;
    // Threshold that we consider a drop as early for stat tracking purpose.
    m_earlyDropThresholdMs = property_get_int32(B_PROPERTY_EARLYDROP_THRESHOLD, B_STAT_EARLYDROP_THRESHOLD_MS);

    // Create the internal display thread
    if ( pthread_create(&m_hDisplayThread, NULL, BOMX_DisplayThread, static_cast <void *> (this)) != 0 )
    {
        ALOGW("Unable to create bomx_display");
        this->Invalidate(OMX_ErrorInsufficientResources);
        return;
    }
    pthread_setname_np(m_hDisplayThread, "bomx_display");
}

BOMX_VideoDecoder::~BOMX_VideoDecoder()
{
    unsigned i;
    BOMX_VideoDecoderFrameBuffer *pBuffer;

    if ( m_hDisplayThread )
    {
        m_displayThreadStop = true;
        BKNI_SetEvent(m_hDisplayFrameEvent);
        pthread_join(m_hDisplayThread, NULL);
        m_hDisplayThread = 0;
    }

    // Stop listening to HWC. Note that HWC binder does need to be protected given
    // it's not updated during the decoder's lifetime.
    if (m_omxHwcBinder)
    {
        delete m_omxHwcBinder;
        m_omxHwcBinder = NULL;
    }

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

    if ( m_pPes )
    {
        delete m_pPes;
        m_pPes = NULL;
    }

    if ( m_hSimpleVideoDecoder )
    {
        NEXUS_SimpleVideoDecoder_Release(m_hSimpleVideoDecoder);
        property_set(B_PROPERTY_COALESCE, "default");
        if ( m_tunnelMode )
        {
            if ( m_tunnelHfr )
            {
                property_set(B_PROPERTY_HFRVIDEO, "default-t");
            }
        }
        else
        {
            property_set(B_PROPERTY_HFRVIDEO, "default-v");
        }
    }
    if ( m_hPlaypump )
    {
        ClosePidChannel();
        NEXUS_Playpump_Close(m_hPlaypump);
    }
    if ( m_hVideoClient )
    {
        NEXUS_SurfaceClient_ReleaseVideoWindow(m_hVideoClient);
    }
    if ( m_hSurfaceClient )
    {
        NEXUS_SurfaceClient_Release(m_hSurfaceClient);
        NxClient_Disconnect(m_nxClientId);
        NxClient_Free(&m_allocResults);
    }
    if ( m_hAlphaSurface )
    {
        NEXUS_Surface_Destroy(m_hAlphaSurface);
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
    if ( m_sourceChangedEventId )
    {
        UnregisterEvent(m_sourceChangedEventId);
    }
    if ( m_streamChangedEventId )
    {
        UnregisterEvent(m_streamChangedEventId);
    }
    if ( m_displayFrameEventId )
    {
        UnregisterEvent(m_displayFrameEventId);
    }

    CancelTimerId(m_inputBuffersTimerId);
    CancelTimerId(m_formatChangeTimerId);

    if ( m_hOutputFrameEvent )
    {
        B_Event_Destroy(m_hOutputFrameEvent);
    }
    if ( m_hSourceChangedEvent )
    {
        B_Event_Destroy(m_hSourceChangedEvent);
    }
    if ( m_hStreamChangedEvent )
    {
        B_Event_Destroy(m_hStreamChangedEvent);
    }
    if ( m_hDisplayFrameEvent )
    {
        BKNI_DestroyEvent(m_hDisplayFrameEvent);
        m_hDisplayFrameEvent = NULL;
    }
    if ( m_hDisplayMutex )
    {
        B_Mutex_Destroy(m_hDisplayMutex);
    }
    if ( m_hGraphics2d )
    {
        NEXUS_Graphics2D_Close(m_hGraphics2d);
    }
    if ( m_hCheckpointEvent )
    {
        B_Event_Destroy(m_hCheckpointEvent);
    }
    if (m_pPlaypumpDescList)
    {
        delete [] m_pPlaypumpDescList;
    }
    for ( i = 0; i < m_numVideoPorts; i++ )
    {
        // Clean up the allocated OMX buffers if they have not been freed for some reason
        if ( m_pVideoPorts[i] )
        {
            CleanupPortBuffers(i);
            delete m_pVideoPorts[i];
        }
    }
    if ( m_pEosBuffer )
    {
        NEXUS_Memory_Free(m_pEosBuffer);
    }
    if ( m_pConfigBuffer )
    {
        FreeConfigBuffer(m_pConfigBuffer);
    }
    if ( m_pIpcClient )
    {
        if ( m_nexusClient )
        {
            m_pIpcClient->destroyClientContext(m_nexusClient);
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
        BOMX_VideoDecoder_StripedSurfaceDestroy(pBuffer);
        delete pBuffer;
    }
    while ( (pBuffer = BLST_Q_FIRST(&m_frameBufferFreeList)) )
    {
        BLST_Q_REMOVE_HEAD(&m_frameBufferFreeList, node);
        delete pBuffer;
    }
    if ( m_pRoles )
    {
        delete m_pRoles;
    }

    if (m_memTracker != -1)
    {
       close(m_memTracker);
       m_memTracker = -1;
    }

    if (m_pTunnelNativeHandle)
    {
        native_handle_delete(m_pTunnelNativeHandle);
        m_pTunnelNativeHandle = NULL;
    }

    g_decActiveStateLock.lock();
    if ( --g_decInstanceCount == 0 )
    {
        g_decActiveState = B_DEC_ACTIVE_STATE_INACTIVE;
    }
    g_decActiveStateLock.unlock();

    BOMX_VIDEO_STATS_RESET;
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
            ALOGV("GetParameter OMX_IndexParamVideoH263");
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
            ALOGV("GetParameter OMX_IndexParamVideoAvc");
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
            ALOGV("GetParameter OMX_IndexParamVideoMpeg4");
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
            ALOGV("GetParameter OMX_IndexParamVideoVp8");
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
            ALOGV("GetParameter OMX_IndexParamVideoHevc");
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
            ALOGV("GetParameter OMX_IndexParamVideoProfileLevelQuerySupported");
            BOMX_STRUCT_VALIDATE(pProfileLevel);
            if ( pProfileLevel->nPortIndex != m_videoPortBase )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }

            // Query the connected display refresh rate info
            bool bStandardFrameRate = false;
            NxClient_DisplaySettings settings;
            NxClient_GetDisplaySettings(&settings);

            for ( unsigned i = 0; i < g_numStandardFrVideoFmts; i++ )
            {
                if ( settings.format == g_standardFrVideoFmts[i] )
                {
                    bStandardFrameRate = true;
                    break;
                }
            }

            switch ( (int)GetCodec() )
            {
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
                    pProfileLevel->eProfile = (OMX_U32)OMX_VIDEO_AVCProfileBaseline;
                    pProfileLevel->eLevel = bStandardFrameRate ? (OMX_U32)OMX_VIDEO_AVCLevel41 : (OMX_U32)OMX_VIDEO_AVCLevel42;
                    break;
                case 1:
                    pProfileLevel->eProfile = (OMX_U32)OMX_VIDEO_AVCProfileMain;
                    pProfileLevel->eLevel = bStandardFrameRate ? (OMX_U32)OMX_VIDEO_AVCLevel41 : (OMX_U32)OMX_VIDEO_AVCLevel42;
                    break;
                case 2:
                    pProfileLevel->eProfile = (OMX_U32)OMX_VIDEO_AVCProfileHigh;
                    pProfileLevel->eLevel = bStandardFrameRate ? (OMX_U32)OMX_VIDEO_AVCLevel41 : (OMX_U32)OMX_VIDEO_AVCLevel42;
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

            case OMX_VIDEO_CodingVP9:
            {
                const OMX_VIDEO_VP9LEVELTYPE *levels = bStandardFrameRate ? g_vp9StandardLevels : g_vp9Levels;
                const size_t countLevels = bStandardFrameRate ? g_numVp9StandardLevels : g_numVp9Levels;
                bool supportsProfile2 = false;
#ifdef HW_HVD_REVISION_S
                supportsProfile2 = true;
#endif
                size_t numProfiles = !supportsProfile2 ? 1 : m_tunnelMode? 3 : 2;
                if (pProfileLevel->nProfileIndex >= numProfiles*countLevels)
                    return OMX_ErrorNoMore;
                const OMX_VIDEO_VP9PROFILETYPE supportedVp9Profiles[] = {OMX_VIDEO_VP9Profile0,
                                                OMX_VIDEO_VP9Profile2, OMX_VIDEO_VP9Profile2HDR};
                pProfileLevel->eProfile = (OMX_U32) supportedVp9Profiles[pProfileLevel->nProfileIndex/countLevels];
                pProfileLevel->eLevel = (OMX_U32) levels[pProfileLevel->nProfileIndex % countLevels];
                break;
            }

            case OMX_VIDEO_CodingHEVC:
            {
                // Return all combinations of profiles/levels supported
                const OMX_VIDEO_HEVCLEVELTYPE *levels = bStandardFrameRate ? g_hevcStandardLevels : g_hevcLevels;
                const size_t countLevels = bStandardFrameRate ? g_numHevcStandardLevels : g_numHevcLevels;
                unsigned numProfiles = (m_tunnelMode) ? 3 : 2;
                if (pProfileLevel->nProfileIndex >= numProfiles*countLevels)
                    return OMX_ErrorNoMore;
                const OMX_VIDEO_HEVCPROFILETYPE supportedHdrProfiles[] = {OMX_VIDEO_HEVCProfileMain,
                                                OMX_VIDEO_HEVCProfileMain10, OMX_VIDEO_HEVCProfileMain10HDR10};
                pProfileLevel->eProfile = (OMX_U32) supportedHdrProfiles[pProfileLevel->nProfileIndex/countLevels];
                pProfileLevel->eLevel = (OMX_U32) levels[pProfileLevel->nProfileIndex % countLevels];
                break;
            }
            default:
                // Only certain codecs support this interface
                return OMX_ErrorNoMore;
            }
            return OMX_ErrorNone;
        }
    case OMX_IndexParamVideoProfileLevelCurrent:
        {
            OMX_VIDEO_PARAM_PROFILELEVELTYPE *pProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
            ALOGV("GetParameter OMX_IndexParamVideoProfileLevelCurrent");
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
                    break;
                case OMX_VIDEO_CodingVP9:
                    pProfileLevel->eProfile = (OMX_U32)BOMX_VP9ProfileFromNexus(vdecStatus.protocolProfile);
                    pProfileLevel->eLevel = (OMX_U32)BOMX_VP9LevelFromNexus(vdecStatus.protocolLevel);
                    break;
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
        ALOGV("GetParameter OMX_IndexParamEnableAndroidNativeGraphicsBuffer %u", pEnableParams->enable);
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
        ALOGV("GetParameter OMX_IndexParamStoreMetaDataInBuffers %u", pMetadata->bStoreMetaData);
        if ( pMetadata->nPortIndex != m_videoPortBase+1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        pMetadata->bStoreMetaData = m_metadataEnabled == true ? OMX_TRUE : OMX_FALSE;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamPrepareForAdaptivePlayback:
    {
        PrepareForAdaptivePlaybackParams *pAdaptive = (PrepareForAdaptivePlaybackParams *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pAdaptive);
        ALOGV("GetParameter OMX_IndexParamPrepareForAdaptivePlayback %u", pAdaptive->bEnable);
        if ( pAdaptive->nPortIndex != m_videoPortBase+1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        pAdaptive->bEnable = m_adaptivePlaybackEnabled == true ? OMX_TRUE : OMX_FALSE;
        pAdaptive->nMaxFrameWidth = m_pVideoPorts[1]->GetDefinition()->format.video.nFrameWidth;
        pAdaptive->nMaxFrameHeight = m_pVideoPorts[1]->GetDefinition()->format.video.nFrameHeight;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamGetAndroidNativeBufferUsage:
    {
        GetAndroidNativeBufferUsageParams *pUsage = (GetAndroidNativeBufferUsageParams *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pUsage);
        ALOGV("GetParameter OMX_IndexParamGetAndroidNativeBufferUsage");
        if ( pUsage->nPortIndex != m_videoPortBase+1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        // Flag this as a HW video decoder allocation to gralloc
        pUsage->nUsage = GRALLOC_USAGE_PRIVATE_0;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamDescribeColorFormat:
    {
        DescribeColorFormatParams *pColorFormat = (DescribeColorFormatParams *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pColorFormat);
        ALOGV("GetParameter OMX_IndexParamDescribeColorFormat eColorFormat=%d", (int)pColorFormat->eColorFormat);
        switch ( (int)pColorFormat->eColorFormat )
        {
            case HAL_PIXEL_FORMAT_YV12:
            {
                // YV12 is Y/Cr/Cb
                size_t yAlign, cAlign;

                yAlign = (pColorFormat->nStride + (16-1)) & ~(16-1);
                cAlign = ((pColorFormat->nStride/2) + (16-1)) & ~(16-1);
                pColorFormat->sMediaImage.mPlane[MediaImage::Y].mRowInc = yAlign;
                pColorFormat->sMediaImage.mPlane[MediaImage::V].mRowInc = cAlign;
                pColorFormat->sMediaImage.mPlane[MediaImage::U].mRowInc = cAlign;

                pColorFormat->sMediaImage.mPlane[MediaImage::V].mOffset = yAlign*pColorFormat->nSliceHeight;
                pColorFormat->sMediaImage.mPlane[MediaImage::U].mOffset =
                                        pColorFormat->sMediaImage.mPlane[MediaImage::V].mOffset +
                                        (cAlign*pColorFormat->nSliceHeight)/2;
            }
            break;
            case OMX_COLOR_FormatYUV420Planar:
            case OMX_COLOR_FormatYUV420SemiPlanar:
            {
                // 420Planar is Y/Cb/Cr
                pColorFormat->sMediaImage.mPlane[MediaImage::Y].mRowInc = pColorFormat->nStride;
                pColorFormat->sMediaImage.mPlane[MediaImage::U].mRowInc = pColorFormat->nStride/2;
                pColorFormat->sMediaImage.mPlane[MediaImage::V].mRowInc = pColorFormat->nStride/2;

                pColorFormat->sMediaImage.mPlane[MediaImage::U].mOffset = pColorFormat->nStride*pColorFormat->nSliceHeight;
                pColorFormat->sMediaImage.mPlane[MediaImage::V].mOffset =
                                        pColorFormat->sMediaImage.mPlane[MediaImage::U].mOffset +
                                        (pColorFormat->nStride*pColorFormat->nSliceHeight)/4;
            }
            break;
            default:
            {
                ALOGW("Unknown color format 0x%x", pColorFormat->eColorFormat);
                return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
            }
            break;
        }
        pColorFormat->sMediaImage.mType = MediaImage::MEDIA_IMAGE_TYPE_YUV;
        pColorFormat->sMediaImage.mNumPlanes = 3;
        pColorFormat->sMediaImage.mWidth = pColorFormat->nFrameWidth;
        pColorFormat->sMediaImage.mHeight = pColorFormat->nFrameHeight;
        pColorFormat->sMediaImage.mBitDepth = 8;
        pColorFormat->sMediaImage.mPlane[MediaImage::Y].mOffset = 0;
        pColorFormat->sMediaImage.mPlane[MediaImage::Y].mColInc = 1;
        pColorFormat->sMediaImage.mPlane[MediaImage::Y].mHorizSubsampling = 1;
        pColorFormat->sMediaImage.mPlane[MediaImage::Y].mVertSubsampling = 1;
        pColorFormat->sMediaImage.mPlane[MediaImage::U].mColInc = 1;
        pColorFormat->sMediaImage.mPlane[MediaImage::U].mHorizSubsampling = 2;
        pColorFormat->sMediaImage.mPlane[MediaImage::U].mVertSubsampling = 2;
        pColorFormat->sMediaImage.mPlane[MediaImage::V].mColInc = 1;
        pColorFormat->sMediaImage.mPlane[MediaImage::V].mHorizSubsampling = 2;
        pColorFormat->sMediaImage.mPlane[MediaImage::V].mVertSubsampling = 2;

        return OMX_ErrorNone;
    }
    case OMX_IndexParamConfigureVideoTunnelMode:
    {
        ConfigureVideoTunnelModeParams *pTunnel = (ConfigureVideoTunnelModeParams *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pTunnel);
        ALOGV("GetParameter OMX_IndexParamConfigureVideoTunnelMode");
        if ( pTunnel->nPortIndex != m_videoPortBase+1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if ( !m_tunnelMode )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }

        pTunnel->bTunneled = OMX_TRUE;
        pTunnel->pSidebandWindow = (OMX_PTR)m_pTunnelNativeHandle;

        return OMX_ErrorNone;
    }

    default:
        ALOGV("GetParameter %#x Deferring to base class", nParamIndex);
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
            ALOGV("SetParameter OMX_IndexParamStandardComponentRole '%s'", pRole->cRole);
            // The spec says this should reset the component to default setings for the role specified.
            // It is technically redundant for this component, changing the input codec has the same
            // effect - but this will also set the input codec accordingly.
            for ( i = 0; i < m_numRoles; i++ )
            {
                if ( !strcmp(m_pRoles[i].name, (const char *)pRole->cRole) )
                {
                    // Set input port to matching compression std.
                    OMX_VIDEO_PARAM_PORTFORMATTYPE format;
                    memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
                    BOMX_STRUCT_INIT(&format);
                    format.nPortIndex = m_videoPortBase;
                    format.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)m_pRoles[i].omxCodec;
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
            ALOGV("SetParameter OMX_IndexParamVideoPortFormat");
            BOMX_STRUCT_VALIDATE(pFormat);
            pPort = FindPortByIndex(pFormat->nPortIndex);
            if ( NULL == pPort )
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            if ( pPort->GetDir() == OMX_DirInput )
            {
                ALOGV("Set Input Port Compression Format to %u (%#x)", pFormat->eCompressionFormat, pFormat->eCompressionFormat);
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

                NEXUS_VideoCodec currentCodec = GetNexusCodec();
                if ((currentCodec == NEXUS_VideoCodec_eH265) || (currentCodec == NEXUS_VideoCodec_eVp9))
                {
                    // Increase input buffer size for hevc/vp9 decoding to handle 4K streams
                    OMX_PARAM_PORTDEFINITIONTYPE portDef;

                    m_pVideoPorts[0]->GetDefinition(&portDef);
                    portDef.nBufferSize = B_DATA_BUFFER_SIZE_HIGHRES;
                    err = m_pVideoPorts[0]->SetDefinition(&portDef);
                    if (err )
                    {
                        return BOMX_ERR_TRACE(err);
                    }
                    err = VerifyInputPortBuffers();
                    if ( err )
                    {
                        return BOMX_ERR_TRACE(err);
                    }
                }
            }
            else
            {
                ALOGV("Set Output Port Color Format to %u (%#x)", pFormat->eColorFormat, pFormat->eColorFormat);
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
        ALOGV("SetParameter OMX_IndexParamPortDefinition");
        BOMX_STRUCT_VALIDATE(pDef);
        if ( pDef->nPortIndex == m_videoPortBase && pDef->format.video.cMIMEType != m_inputMimeType )
        {
            ALOGW("Video input MIME type cannot be changed in the application");
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        else if ( pDef->nPortIndex == (m_videoPortBase+1) && pDef->format.video.cMIMEType != m_outputMimeType )
        {
            ALOGW("Video output MIME type cannot be changed in the application");
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        BOMX_Port *pPort = FindPortByIndex(pDef->nPortIndex);
        if ( NULL == pPort )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if ( pDef->nPortIndex == m_videoPortBase && pDef->format.video.eCompressionFormat != GetCodec() )
        {
            ALOGW("Video compression format cannot be changed in the port defintion.  Change Port Format instead.");
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        // Handle update in base class
        err = BOMX_Component::SetParameter(nIndex, (OMX_PTR)pDef);
        if ( err != OMX_ErrorNone )
        {
            return BOMX_ERR_TRACE(err);
        }
        else if ( pDef->nPortIndex == (m_videoPortBase+1) )
        {
            OMX_PARAM_PORTDEFINITIONTYPE portDef;
            m_pVideoPorts[1]->GetDefinition(&portDef);
            // Validate portdef width/height against their counterpart platform maximum values
            if ((portDef.format.video.nFrameWidth > m_maxDecoderWidth)
                    || (portDef.format.video.nFrameHeight > m_maxDecoderHeight))
            {
                ALOGE("Video stream exceeds decoder capabilities, w:%u,h:%u,maxW:%u,maxH:%u",
                        portDef.format.video.nFrameWidth, portDef.format.video.nFrameHeight,
                        m_maxDecoderWidth, m_maxDecoderHeight);
                return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
            }

            // Ensure crop reports buffer width/height
            m_outputWidth = portDef.format.video.nFrameWidth;
            m_outputHeight = portDef.format.video.nFrameHeight;
            // Ensure slice height and stride match frame width/height and update buffer size
            portDef.format.video.nSliceHeight = portDef.format.video.nFrameHeight;
            portDef.format.video.nStride = portDef.format.video.nFrameWidth;
            portDef.nBufferSize = ComputeBufferSize(portDef.format.video.nStride, portDef.format.video.nSliceHeight);
            err = m_pVideoPorts[1]->SetDefinition(&portDef);
            if ( err )
            {
                return BOMX_ERR_TRACE(err);
            }
        }
        else if ( pDef->nPortIndex == m_videoPortBase )
        {
            err = VerifyInputPortBuffers();
            if (err)
            {
                return BOMX_ERR_TRACE(err);
            }
        }
        return OMX_ErrorNone;
    }
    case OMX_IndexParamEnableAndroidNativeGraphicsBuffer:
    {
        EnableAndroidNativeBuffersParams *pEnableParams = (EnableAndroidNativeBuffersParams *)pComponentParameterStructure;
        bool oldValue;
        BOMX_STRUCT_VALIDATE(pEnableParams);
        ALOGV("SetParameter OMX_IndexParamEnableAndroidNativeGraphicsBuffer %u", pEnableParams->enable);
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
            // In this mode, the output color format should be an android HAL format.
            portFormat.nIndex = 1;  // The second port format is the native format
            portFormat.eColorFormat = (OMX_COLOR_FORMATTYPE)((int)HAL_PIXEL_FORMAT_YV12);
        }
        else
        {
            // In this mode, use an OMX color format
            portFormat.nIndex = 0;  // The first port format is the traditional omx buffer format
            portFormat.eColorFormat = OMX_COLOR_FormatYUV420Planar;
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
        ALOGV("SetParameter OMX_IndexParamStoreMetaDataInBuffers %u", pMetadata->bStoreMetaData);
        if ( pMetadata->nPortIndex != m_videoPortBase+1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        m_metadataEnabled = pMetadata->bStoreMetaData == OMX_TRUE ? true : false;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamPrepareForAdaptivePlayback:
    {
        PrepareForAdaptivePlaybackParams *pAdaptive = (PrepareForAdaptivePlaybackParams *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pAdaptive);
        ALOGV("SetParameter OMX_IndexParamPrepareForAdaptivePlayback %u %ux%u", pAdaptive->bEnable, pAdaptive->nMaxFrameWidth, pAdaptive->nMaxFrameHeight);
        if ( pAdaptive->nPortIndex != m_videoPortBase+1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        m_adaptivePlaybackEnabled = pAdaptive->bEnable == OMX_TRUE ? true : false;
        // Update crop and port format
        OMX_PARAM_PORTDEFINITIONTYPE portDef;
        m_pVideoPorts[1]->GetDefinition(&portDef);
        // Ensure crop reports buffer width/height
        m_outputWidth = pAdaptive->nMaxFrameWidth;
        m_outputHeight = pAdaptive->nMaxFrameHeight;
        // Ensure slice height and stride match frame width/height and update buffer size
        portDef.format.video.nFrameWidth = m_outputWidth;
        portDef.format.video.nFrameHeight = m_outputHeight;
        portDef.format.video.nSliceHeight = m_outputHeight;
        portDef.format.video.nStride = m_outputWidth;
        portDef.nBufferSize = ComputeBufferSize(portDef.format.video.nStride, portDef.format.video.nSliceHeight);
        err = m_pVideoPorts[1]->SetDefinition(&portDef);
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }
        PortFormatChanged(m_pVideoPorts[1]);
        return OMX_ErrorNone;
    }
    case OMX_IndexParamUseAndroidNativeBuffer:
    case OMX_IndexParamUseAndroidNativeBuffer2:
    {
        UseAndroidNativeBufferParams *pBufferParams = (UseAndroidNativeBufferParams *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pBufferParams);
        ALOGV("SetParameter OMX_IndexParamUseAndroidNativeBuffer");
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
        ALOGV("SetParameter OMX_IndexParamConfigureVideoTunnelMode");
        if ( pTunnel->nPortIndex != m_videoPortBase+1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if ( !m_tunnelMode )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        if ( pTunnel->bTunneled && pTunnel->nAudioHwSync > 0)
        {
            // A pair of stc channel handles are wrapped by the audio hal in a global memory block
            typedef struct stc_channel_st {
               NEXUS_SimpleStcChannelHandle stc_channel;
               NEXUS_SimpleStcChannelHandle stc_channel_sync;
            } stc_channel_st;
            NEXUS_MemoryBlockHandle hdl = (NEXUS_MemoryBlockHandle)(intptr_t)pTunnel->nAudioHwSync;
            void *pMemory = NULL;
            stc_channel_st *stcChannelSt = NULL;
            if ((NEXUS_MemoryBlock_Lock(hdl, &pMemory) == NEXUS_SUCCESS) && (pMemory != NULL)) {
                stcChannelSt = (stc_channel_st*)pMemory;
                m_tunnelStcChannel = stcChannelSt->stc_channel;
                m_tunnelStcChannelSync = stcChannelSt->stc_channel_sync;
                NEXUS_SimpleStcChannel_SetStc(m_tunnelStcChannelSync, B_STC_SYNC_INVALID_VALUE);
                NEXUS_MemoryBlock_Unlock(hdl);
            }
            ALOGV("OMX_IndexParamConfigureVideoTunnelMode - stc-channels %p %p",
                    m_tunnelStcChannel, m_tunnelStcChannelSync);
        }

        return OMX_ErrorNone;
    }
    default:
        ALOGV("SetParameter %#x - Defer to base class", nIndex);
        return BOMX_ERR_TRACE(BOMX_Component::SetParameter(nIndex, pComponentParameterStructure));
    }
}

NEXUS_VideoCodec BOMX_VideoDecoder::GetNexusCodec()
{
    return GetNexusCodec((OMX_VIDEO_CODINGTYPE)GetCodec());
}

NEXUS_VideoCodec BOMX_VideoDecoder::GetNexusCodec(OMX_VIDEO_CODINGTYPE omxType)
{
    switch ( (int)omxType )
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
    case OMX_VIDEO_CodingVP9:
        return NEXUS_VideoCodec_eVp9;
    case OMX_VIDEO_CodingHEVC:
        return NEXUS_VideoCodec_eH265;
    default:
        ALOGW("Unknown video codec %u (%#x)", GetCodec(), GetCodec());
        return NEXUS_VideoCodec_eNone;
    }
}

NEXUS_Error BOMX_VideoDecoder::SetInputPortState(OMX_STATETYPE newState)
{
    ALOGV("Setting Input Port State to %s", BOMX_StateName(newState));
    // Loaded means stop and release all resources
    if ( newState == OMX_StateLoaded )
    {
        // Invalidate queue of decoded frames
        InvalidateDecodedFrames();
        // Shutdown and free resources
        if ( m_hSimpleVideoDecoder )
        {
            if ( m_tunnelMode )
            {
                NEXUS_VideoDecoderStatus status;
                if ( NEXUS_SimpleVideoDecoder_GetStatus(m_hSimpleVideoDecoder, &status) == NEXUS_SUCCESS )
                {
                    ALOGD("stats decErr:%u decDrop:%u dispDrop:%u dispUnder:%u ptsErr:%u",
                        status.numDecodeErrors, status.numDecodeDrops, status.numDisplayDrops, status.numDisplayUnderflows, status.ptsErrorCount);
                }
            }
            else
            {
                // Exclude early frame drops from total tally
                ALOGD("stats df:%zu edf:%zu mcdf:%zu",
                   m_droppedFrames - m_earlyDroppedFrames, m_earlyDroppedFrames, m_maxConsecDroppedFrames);
            }

            NEXUS_Playpump_Stop(m_hPlaypump);
            NEXUS_SimpleVideoDecoder_Stop(m_hSimpleVideoDecoder);

            m_submittedDescriptors = 0;
            m_completedInputBuffers = 0;
            m_eosPending = false;
            m_eosDelivered = false;
            m_eosReceived = false;
            m_ptsReceived = false;
            B_Mutex_Lock(m_hDisplayMutex);
            m_displayFrameAvailable = false;
            B_Mutex_Unlock(m_hDisplayMutex);
            m_pBufferTracker->Flush();
            InputBufferCounterReset();
            NEXUS_SimpleVideoDecoder_Release(m_hSimpleVideoDecoder);
            m_hSimpleVideoDecoder = NULL;

            ClosePidChannel();
            NEXUS_Playpump_Close(m_hPlaypump);
            UnregisterEvent(m_playpumpEventId);
            m_playpumpEventId = NULL;
            m_hPlaypump = NULL;
            CancelTimerId(m_playpumpTimerId);
            BOMX_VIDEO_STATS_PRINT_BASIC;
            BOMX_VIDEO_STATS_PRINT_DETAILED;
            BOMX_VIDEO_STATS_RESET;
            m_droppedFrames = m_earlyDroppedFrames = m_consecDroppedFrames = m_maxConsecDroppedFrames = 0;
            m_lastReturnedSerial = 0;
            m_formatChangeState = FCState_eNone;
            CancelTimerId(m_formatChangeTimerId);
            m_waitingForStc = false;
            if (m_stcResumePending) {
                // resume stc if we left it paused
                m_stcResumePending = false;
                ALOGV("Restoring stc channel");
                NEXUS_SimpleStcChannel_SetRate(m_tunnelStcChannel, 1, 0);
            }
            m_videoStreamInfo.valid = false;
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
            NEXUS_VideoDecoderExtendedSettings extSettings;
            NEXUS_PlaypumpOpenSettings playpumpOpenSettings;
            NEXUS_PlaypumpSettings playpumpSettings;
            NEXUS_Error errCode;

            m_hSimpleVideoDecoder = NEXUS_SimpleVideoDecoder_Acquire(m_allocResults.simpleVideoDecoder[0].id);
            if ( NULL == m_hSimpleVideoDecoder )
            {
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }

            if (m_tunnelMode) {
                NEXUS_SimpleVideoDecoder_GetSettings(m_hSimpleVideoDecoder, &vdecSettings);
                vdecSettings.sourceChanged.callback = BOMX_VideoDecoder_EventCallback;
                vdecSettings.sourceChanged.context = (void *)m_hSourceChangedEvent;
                vdecSettings.sourceChanged.param = (int)BOMX_VideoDecoderEventType_eSourceChanged;
                vdecSettings.streamChanged.callback = BOMX_VideoDecoder_EventCallback;
                vdecSettings.streamChanged.context = (void *)m_hStreamChangedEvent;
                vdecSettings.streamChanged.param = (int)BOMX_VideoDecoderEventType_eStreamChanged;
                NEXUS_SimpleVideoDecoder_SetSettings(m_hSimpleVideoDecoder, &vdecSettings);
            }

            NEXUS_SimpleVideoDecoder_GetExtendedSettings(m_hSimpleVideoDecoder, &extSettings);
            extSettings.dataReadyCallback.callback = BOMX_VideoDecoder_EventCallback;
            extSettings.dataReadyCallback.context = (void *)m_hOutputFrameEvent;
            extSettings.dataReadyCallback.param = (int)BOMX_VideoDecoderEventType_eDataReady;
            errCode = NEXUS_SimpleVideoDecoder_SetExtendedSettings(m_hSimpleVideoDecoder, &extSettings);
            if ( errCode )
            {
                NEXUS_SimpleVideoDecoder_Release(m_hSimpleVideoDecoder);
                m_hSimpleVideoDecoder = NULL;
                return BOMX_BERR_TRACE(errCode);
            }

            NEXUS_Playpump_GetDefaultOpenSettings(&playpumpOpenSettings);
            playpumpOpenSettings.fifoSize = 0;
            playpumpOpenSettings.dataNotCpuAccessible = true;
            playpumpOpenSettings.numDescriptors = 1+(m_maxDescriptorsPerBuffer*m_pVideoPorts[0]->GetDefinition()->nBufferCountActual);   // Extra 1 is for EOS
            m_hPlaypump = NEXUS_Playpump_Open(NEXUS_ANY_ID, &playpumpOpenSettings);
            if ( NULL == m_hPlaypump )
            {
                NEXUS_SimpleVideoDecoder_Release(m_hSimpleVideoDecoder);
                m_hSimpleVideoDecoder = NULL;
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }
            NEXUS_Playpump_GetSettings(m_hPlaypump, &playpumpSettings);
            playpumpSettings.transportType = NEXUS_TransportType_eMpeg2Pes;
            playpumpSettings.dataNotCpuAccessible = true;
            playpumpSettings.dataCallback.callback = BOMX_VideoDecoder_EventCallback;
            playpumpSettings.dataCallback.context = static_cast <void *> (m_hPlaypumpEvent);
            playpumpSettings.dataCallback.param = (int)BOMX_VideoDecoderEventType_ePlaypump;
            errCode = NEXUS_Playpump_SetSettings(m_hPlaypump, &playpumpSettings);
            if ( errCode )
            {
                NEXUS_Playpump_Close(m_hPlaypump);
                m_hPlaypump = NULL;
                NEXUS_SimpleVideoDecoder_Release(m_hSimpleVideoDecoder);
                m_hSimpleVideoDecoder = NULL;
                return BOMX_BERR_TRACE(errCode);
            }

            errCode = OpenPidChannel(B_STREAM_ID);
            if ( errCode )
            {
                NEXUS_Playpump_Close(m_hPlaypump);
                m_hPlaypump = NULL;
                NEXUS_SimpleVideoDecoder_Release(m_hSimpleVideoDecoder);
                m_hSimpleVideoDecoder = NULL;
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }
            m_playpumpEventId = RegisterEvent(m_hPlaypumpEvent, BOMX_VideoDecoder_PlaypumpEvent, static_cast <void *> (this));
            if ( NULL == m_playpumpEventId )
            {
                ALOGW("Unable to register playpump event");
                ClosePidChannel();
                NEXUS_Playpump_Close(m_hPlaypump);
                m_hPlaypump = NULL;
                NEXUS_SimpleVideoDecoder_Release(m_hSimpleVideoDecoder);
                m_hSimpleVideoDecoder = NULL;
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }
            // Safe to initialize here the number of input buffers available
            m_AvailInputBuffers = m_pVideoPorts[0]->GetDefinition()->nBufferCountActual;
        }

        (void)NEXUS_SimpleVideoDecoder_GetStatus(m_hSimpleVideoDecoder, &vdecStatus);
        switch ( newState )
        {
        case OMX_StateIdle:
            if ( vdecStatus.started )
            {
                // Executing/Paused -> Idle = Stop

                // Invalidate queue of decoded frames
                InvalidateDecodedFrames();
                NEXUS_Playpump_Stop(m_hPlaypump);
                NEXUS_SimpleVideoDecoder_Stop(m_hSimpleVideoDecoder);
                m_eosPending = false;
                m_eosDelivered = false;
                m_eosReceived = false;
                m_ptsReceived = false;
                m_pBufferTracker->Flush();
                InputBufferCounterReset();
                ReturnPortBuffers(m_pVideoPorts[0]);
                m_submittedDescriptors = 0;
                m_completedInputBuffers = 0;
                B_Mutex_Lock(m_hDisplayMutex);
                m_displayFrameAvailable = false;
                B_Mutex_Unlock(m_hDisplayMutex);
                property_set(B_PROPERTY_COALESCE, "default");
                if ( m_tunnelMode )
                {
                    if ( m_tunnelHfr )
                    {
                        property_set(B_PROPERTY_HFRVIDEO, "default-t");
                    }
                }
                else
                {
                    property_set(B_PROPERTY_HFRVIDEO, "default-v");
                }
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
                vdecStartSettings.displayEnabled = m_tunnelMode ? true : ((m_outputMode == BOMX_VideoDecoderOutputBufferType_eStandard) ? false : true);
                vdecStartSettings.settings.appDisplayManagement = m_tunnelMode ? false : true;
                vdecStartSettings.settings.stcChannel = NULL;
                vdecStartSettings.settings.pidChannel = m_hPidChannel;
                vdecStartSettings.settings.codec = GetNexusCodec();
                vdecStartSettings.maxWidth = m_maxDecoderWidth;     // Always request the max dimension for allowing decoder not waiting for output buffer
                vdecStartSettings.maxHeight = m_maxDecoderHeight;
                ALOGV("Start Decoder display %u appDM %u codec %u", vdecStartSettings.displayEnabled, vdecStartSettings.settings.appDisplayManagement, vdecStartSettings.settings.codec);
                property_set(B_PROPERTY_COALESCE, "vmode");
                if ( m_tunnelMode )
                {
                    if ( m_tunnelHfr )
                    {
                        property_set(B_PROPERTY_HFRVIDEO, "tunneled");
                    }
                }
                else
                {
                    property_set(B_PROPERTY_HFRVIDEO, "vmode");
                }
                m_startTime = systemTime(CLOCK_MONOTONIC); // Track start time
                if (m_tunnelMode)
                {
                    if (!property_get_int32(B_PROPERTY_HW_SYNC_FAKE, 0))
                    {
                        errCode = NEXUS_SimpleVideoDecoder_SetStcChannel(m_hSimpleVideoDecoder, m_tunnelStcChannel);
                        if ( errCode )
                        {
                            return BOMX_BERR_TRACE(errCode);
                        }
                    }
                }
                errCode = NEXUS_SimpleVideoDecoder_Start(m_hSimpleVideoDecoder, &vdecStartSettings);
                if ( errCode )
                {
                    return BOMX_BERR_TRACE(errCode);
                }
                m_submittedDescriptors = 0;
                m_completedInputBuffers = 0;
                errCode = NEXUS_Playpump_Start(m_hPlaypump);
                if ( errCode )
                {
                    NEXUS_SimpleVideoDecoder_Stop(m_hSimpleVideoDecoder);
                    m_eosPending = false;
                    m_eosDelivered = false;
                    m_eosReceived = false;
                    m_ptsReceived = false;
                    property_set(B_PROPERTY_COALESCE, "default");
                    if ( m_tunnelMode )
                    {
                        if ( m_tunnelHfr )
                        {
                            property_set(B_PROPERTY_HFRVIDEO, "default-t");
                        }
                    }
                    else
                    {
                        property_set(B_PROPERTY_HFRVIDEO, "default-v");
                    }
                    return BOMX_BERR_TRACE(errCode);
                }

                if ( !m_tunnelMode && m_pVideoPorts[1]->IsEnabled() )
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
    ALOGV("Setting Output Port State to %s", BOMX_StateName(newState));
    if ( newState == OMX_StateLoaded )
    {
        CancelTimerId(m_inputBuffersTimerId);
        CancelTimerId(m_formatChangeTimerId);

        // Return all pending buffers to the client
        ReturnPortBuffers(m_pVideoPorts[1]);
        if ( m_formatChangeState == FCState_eWaitForPortReconfig )
        {
            m_formatChangeState = FCState_eNone;
        }
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

    ALOGV("Begin State Change %s->%s", BOMX_StateName(oldState), BOMX_StateName(newState));

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
            BOMX_VideoDecoder_StripedSurfaceDestroy(pFrameBuffer);
            BLST_Q_INSERT_TAIL(&m_frameBufferFreeList, pFrameBuffer, node);
        }

        // TODO: Hide video window?

        ALOGV("End State Change %s->%s", BOMX_StateName(oldState), BOMX_StateName(newState));
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
            ALOGV("Waiting for port population...");
            PortWaitBegin();
            // Now we need to wait for all enabled ports to become populated
            while ( (m_pVideoPorts[0]->IsEnabled() && !m_pVideoPorts[0]->IsPopulated()) ||
                    (m_pVideoPorts[1]->IsEnabled() && (!m_tunnelMode && !m_pVideoPorts[1]->IsPopulated())) )
            {
                if ( PortWait() != B_ERROR_SUCCESS )
                {
                    ALOGW("Timeout waiting for ports to be populated");
                    PortWaitEnd();
                    return BOMX_ERR_TRACE(OMX_ErrorTimeout);
                }
            }
            PortWaitEnd();
            ALOGV("Done waiting for port population");

            bool inputEnabled = m_pVideoPorts[0]->IsEnabled();
            bool outputEnabled = m_pVideoPorts[1]->IsEnabled();
            if ( ((!m_tunnelMode && m_pVideoPorts[1]->IsPopulated()) ||
                  (m_tunnelMode && !m_pVideoPorts[1]->IsPopulated()))
                 && m_pVideoPorts[1]->IsEnabled())
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
        ALOGV("End State Change %s->%s", BOMX_StateName(oldState), BOMX_StateName(newState));
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
        ALOGV("End State Change %s->%s", BOMX_StateName(oldState), BOMX_StateName(newState));
        return OMX_ErrorNone;
    default:
        ALOGW("Unsupported state %u", newState);
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
    }
}

OMX_ERRORTYPE BOMX_VideoDecoder::CommandFlush(
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
        ALOGV("Flushing all ports");

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
            ALOGW("Invalid port %u", portIndex);
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        ALOGV("Flushing %s port", pPort->GetDir() == OMX_DirInput ? "input" : "output");
        if ( portIndex == m_videoPortBase )
        {
            // Input port
            if ( m_pVideoPorts[0]->IsEnabled() && m_pVideoPorts[0]->IsPopulated() && m_hPlaypump != NULL )
            {
                m_inputFlushing = true;

                NEXUS_Playpump_Flush(m_hPlaypump);
                m_eosPending = false;
                m_ptsReceived = false;
                PlaypumpEvent();
                ALOG_ASSERT(m_submittedDescriptors == 0);
                ReturnInputBuffers(0, InputReturnMode_eAll);
                m_configBufferState = ConfigBufferState_eFlushed;

                m_inputFlushing = false;
            }
            else
            {
                ReturnPortBuffers(m_pVideoPorts[0]);
            }
        }
        else
        {
            // Output port
            if ( (m_tunnelMode || (m_pVideoPorts[1]->IsEnabled() && m_pVideoPorts[1]->IsPopulated())) && m_hSimpleVideoDecoder != NULL )
            {
                m_outputFlushing = true;

                NEXUS_SimpleVideoDecoder_Flush(m_hSimpleVideoDecoder);
                ReturnDecodedFrames();
                m_pBufferTracker->Flush();
                m_eosDelivered = false;
                m_eosReceived = false;
                B_Mutex_Lock(m_hDisplayMutex);
                m_displayFrameAvailable = false;
                m_frameSerial = 0;
                B_Mutex_Unlock(m_hDisplayMutex);

                m_outputFlushing = false;
                // Handle possible timestamp discontinuity (seeking) after a flush command
                if ( m_tunnelMode && (m_tunnelStcChannel != NULL ) && !m_waitingForStc)
                {
                    NEXUS_VideoDecoderTrickState vdecTrickState;
                    NEXUS_Error errCode;

                    m_waitingForStc = true;
                    m_flushTime = systemTime(SYSTEM_TIME_MONOTONIC);
                    m_stcResumePending = false;

                    // Pause decoder until a valid stc is available
                    NEXUS_SimpleVideoDecoder_GetTrickState(m_hSimpleVideoDecoder, &vdecTrickState);
                    vdecTrickState.rate = 0;
                    errCode = NEXUS_SimpleVideoDecoder_SetTrickState(m_hSimpleVideoDecoder, &vdecTrickState);
                    if (errCode != NEXUS_SUCCESS)
                        return BOMX_ERR_TRACE(OMX_ErrorUndefined);

                    // Reset stc sync only if it hasn't changed its last value.
                    uint32_t stcSync;
                    NEXUS_SimpleStcChannel_GetStc(m_tunnelStcChannelSync, &stcSync);
                    ALOGV("Flush request, stcSync read:%u value:%u, ", stcSync, m_stcSyncValue);
                    if (stcSync == m_stcSyncValue) {
                        NEXUS_SimpleStcChannel_SetStc(m_tunnelStcChannelSync, B_STC_SYNC_INVALID_VALUE);
                        ALOGV("Setting sync stc to invalid value");
                    }
                }
            }
            ReturnPortBuffers(m_pVideoPorts[1]);
        }
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
        ALOGV("Enabling all ports");

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
            ALOGW("Invalid port %u", portIndex);
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if ( pPort->IsEnabled() )
        {
            ALOGW("Port %u is already enabled", portIndex);
            return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
        }
        ALOGV("Enabling %s port", pPort->GetDir() == OMX_DirInput ? "input" : "output");
        err = pPort->Enable();
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }
        if ( StateGet() != OMX_StateLoaded )
        {
            // Wait for port to become populated
            ALOGV("Waiting for port to populate");
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
            ALOGV("Done waiting for port to populate");

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
        ALOGV("Disabling all ports");

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
            ALOGW("Invalid port %u", portIndex);
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if ( pPort->IsDisabled() )
        {
            ALOGW("Port %u is already disabled", portIndex);
            return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
        }
        ALOGV("Disabling %s port", pPort->GetDir() == OMX_DirInput ? "input" : "output");
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
            ALOGV("Waiting for port to de-populate");
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
            ALOGV("Done waiting for port to de-populate");
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
    size_t headerBufferSize;
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
        ALOGW("Cannot add buffers to a tunneled port");
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }

    pInfo = new BOMX_VideoDecoderInputBufferInfo;
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    headerBufferSize = B_PES_HEADER_LENGTH_WITH_PTS;  // Basic PES header w/PTS.
    switch ( GetCodec() )
    {
    case OMX_VIDEO_CodingVP9:
    case OMX_VIDEO_CodingVP8:
        headerBufferSize += BOMX_BCMV_HEADER_SIZE;  // VP8/VP9 require space for a BCMV header on each frame.
    default:
        break;
    }
    // To avoid unbounded PES, create enough headers to fit each input buffer into individual PES packets.  Subsequent PES packets will not have a timestamp so they are only 9 bytes.
    headerBufferSize += B_PES_HEADER_LENGTH_WITHOUT_PTS * ((m_maxDescriptorsPerBuffer-4)/2);
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
        ALOGW("Cannot add buffers to a tunneled port");
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }

    if (m_tunnelMode)
    {
        ALOGW("Do not expect output port population in tunnel mode");
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }

    pInfo = new BOMX_VideoDecoderOutputBufferInfo;
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }
    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->type = m_outputMode;
    switch ( m_outputMode )
    {
    case BOMX_VideoDecoderOutputBufferType_eStandard:
        {
            NEXUS_ClientConfiguration clientConfig;
            NEXUS_SurfaceCreateSettings surfaceSettings;
            const OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
            void *pMemory;

            /* Check buffer size */
            pPortDef = pPort->GetDefinition();
            if ( nSizeBytes < ComputeBufferSize(pPortDef->format.video.nStride, pPortDef->format.video.nSliceHeight) )
            {
                ALOGE("Outbuffer size is not sufficient - must be at least %u bytes ((3*%u*%u)/2) got %u [eColorFormat %#x]", (unsigned int)ComputeBufferSize(pPortDef->format.video.nStride, pPortDef->format.video.nSliceHeight),
                    pPortDef->format.video.nStride, pPortDef->format.video.nSliceHeight, nSizeBytes, pPortDef->format.video.eColorFormat);
                delete pInfo;
                return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
            }
            if ( componentAllocated )
            {
                // Not supported for now.  Will add if required.
                delete pInfo;
                return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
            }
            else
            {
                // Client has its own buffer we must CPU-copy into
                pInfo->typeInfo.standard.pClientMemory = pBuffer;
                BKNI_Memset(pBuffer, 0xff, nSizeBytes);
            }

            /* Create output surfaces */
            NEXUS_Platform_GetClientConfiguration(&clientConfig);

#if BOMX_VIDEO_DECODER_DESTRIPE_PLANAR
            pInfo->typeInfo.standard.yMemBlkFd = -1;
            pInfo->typeInfo.standard.crMemBlkFd = -1;
            pInfo->typeInfo.standard.cbMemBlkFd = -1;

            NEXUS_Surface_GetDefaultCreateSettings(&surfaceSettings);
            surfaceSettings.pixelFormat = NEXUS_PixelFormat_eY8;
            surfaceSettings.width = pPortDef->format.video.nFrameWidth;
            surfaceSettings.height = pPortDef->format.video.nFrameHeight;
            surfaceSettings.pitch = pPortDef->format.video.nStride;
            surfaceSettings.pixelMemory = BOMX_VideoDecoder_AllocatePixelMemoryBlk(&surfaceSettings, &pInfo->typeInfo.standard.yMemBlkFd);
            if (surfaceSettings.pixelMemory != NULL) {
               pInfo->typeInfo.standard.hSurfaceY = NEXUS_Surface_Create(&surfaceSettings);
            } else {
               pInfo->typeInfo.standard.hSurfaceY = NULL;
            }
            if ( NULL == pInfo->typeInfo.standard.hSurfaceY )
            {
                BOMX_VideoDecoder_SurfaceDestroy(&pInfo->typeInfo.standard.yMemBlkFd, NULL);
                delete pInfo;
                return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
            }
            NEXUS_Surface_Lock(pInfo->typeInfo.standard.hSurfaceY, &pMemory);    // Pin the surface in memory so we can flush at the correct time without extra locks

            NEXUS_Surface_GetDefaultCreateSettings(&surfaceSettings);
            surfaceSettings.pixelFormat = NEXUS_PixelFormat_eCb8;
            surfaceSettings.width = pPortDef->format.video.nFrameWidth/2;
            surfaceSettings.height = pPortDef->format.video.nFrameHeight/2;
            surfaceSettings.pitch = pPortDef->format.video.nStride/2;
            surfaceSettings.pixelMemory = BOMX_VideoDecoder_AllocatePixelMemoryBlk(&surfaceSettings, &pInfo->typeInfo.standard.cbMemBlkFd);
            if (surfaceSettings.pixelMemory != NULL) {
               pInfo->typeInfo.standard.hSurfaceCb = NEXUS_Surface_Create(&surfaceSettings);
            } else {
               pInfo->typeInfo.standard.hSurfaceCb = NULL;
            }
            if ( NULL == pInfo->typeInfo.standard.hSurfaceCb )
            {
                BOMX_VideoDecoder_SurfaceDestroy(&pInfo->typeInfo.standard.cbMemBlkFd, NULL);
                BOMX_VideoDecoder_SurfaceDestroy(&pInfo->typeInfo.standard.yMemBlkFd, pInfo->typeInfo.standard.hSurfaceY);
                delete pInfo;
                return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
            }
            NEXUS_Surface_Lock(pInfo->typeInfo.standard.hSurfaceCb, &pMemory);    // Pin the surface in memory so we can flush at the correct time without extra locks
            surfaceSettings.pixelFormat = NEXUS_PixelFormat_eCr8;
            surfaceSettings.pixelMemory = BOMX_VideoDecoder_AllocatePixelMemoryBlk(&surfaceSettings, &pInfo->typeInfo.standard.crMemBlkFd);
            if (surfaceSettings.pixelMemory != NULL) {
               pInfo->typeInfo.standard.hSurfaceCr = NEXUS_Surface_Create(&surfaceSettings);
            } else {
               pInfo->typeInfo.standard.hSurfaceCr = NULL;
            }
            if ( NULL == pInfo->typeInfo.standard.hSurfaceCr )
            {
                BOMX_VideoDecoder_SurfaceDestroy(&pInfo->typeInfo.standard.crMemBlkFd, NULL);
                BOMX_VideoDecoder_SurfaceDestroy(&pInfo->typeInfo.standard.cbMemBlkFd, pInfo->typeInfo.standard.hSurfaceCb);
                BOMX_VideoDecoder_SurfaceDestroy(&pInfo->typeInfo.standard.yMemBlkFd, pInfo->typeInfo.standard.hSurfaceY);
                delete pInfo;
                return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
            }
            NEXUS_Surface_Lock(pInfo->typeInfo.standard.hSurfaceCr, &pMemory);    // Pin the surface in memory so we can flush at the correct time without extra locks
#else
            pInfo->typeInfo.standard.destripeMemBlkFd = -1;

            NEXUS_Surface_GetDefaultCreateSettings(&surfaceSettings);
            surfaceSettings.pixelFormat = NEXUS_PixelFormat_eCr8_Y18_Cb8_Y08;
            surfaceSettings.width = pPortDef->format.video.nFrameWidth;
            surfaceSettings.height = pPortDef->format.video.nFrameHeight;
            surfaceSettings.pitch = 2*surfaceSettings.width;
            surfaceSettings.pixelMemory = BOMX_VideoDecoder_AllocatePixelMemoryBlk(&surfaceSettings, &pInfo->typeInfo.standard.destripeMemBlkFd);
            if (surfaceSettings.pixelMemory != NULL) {
               pInfo->typeInfo.standard.hDestripeSurface = NEXUS_Surface_Create(&surfaceSettings);
            } else {
               pInfo->typeInfo.standard.hDestripeSurface = NULL;
            }
            if ( NULL == pInfo->typeInfo.standard.hDestripeSurface )
            {
                BOMX_VideoDecoder_SurfaceDestroy(&pInfo->typeInfo.standard.destripeMemBlkFd, NULL);
                delete pInfo;
                return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
            }
            NEXUS_Surface_Lock(pInfo->typeInfo.standard.hDestripeSurface, &pMemory);    // Pin the surface in memory so we can flush at the correct time without extra locks
#endif
        }
        break;
    default:
        ALOGW("Unsupported buffer type");
        delete pInfo;
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    err = pPort->AddBuffer(ppBufferHdr, pAppPrivate, nSizeBytes, pBuffer, pInfo, componentAllocated);
    if ( OMX_ErrorNone != err )
    {
        switch ( pInfo->type )
        {
        case BOMX_VideoDecoderOutputBufferType_eStandard:
#if BOMX_VIDEO_DECODER_DESTRIPE_PLANAR
            BOMX_VideoDecoder_SurfaceDestroy(&pInfo->typeInfo.standard.crMemBlkFd, pInfo->typeInfo.standard.hSurfaceCr);
            BOMX_VideoDecoder_SurfaceDestroy(&pInfo->typeInfo.standard.cbMemBlkFd, pInfo->typeInfo.standard.hSurfaceCb);
            BOMX_VideoDecoder_SurfaceDestroy(&pInfo->typeInfo.standard.yMemBlkFd, pInfo->typeInfo.standard.hSurfaceY);
#else
            BOMX_VideoDecoder_SurfaceDestroy(&pInfo->typeInfo.standard.destripeMemBlkFd, pInfo->typeInfo.standard.hDestripeSurface);
#endif
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
    void *pMemory;

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
        ALOGW("Cannot add buffers to a tunneled port");
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }

    pInfo = new BOMX_VideoDecoderOutputBufferInfo;
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->type = BOMX_VideoDecoderOutputBufferType_eNative;
    pInfo->typeInfo.native.pPrivateHandle = pPrivateHandle;
    BOMX_VideoDecoder_MemLock(pPrivateHandle, &pMemory);
    if ( NULL == pMemory )
    {
        delete pInfo;
        BOMX_VideoDecoder_MemUnlock(pPrivateHandle);
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    else
    {
       pInfo->typeInfo.native.pSharedData = (PSHARED_DATA)pMemory;
    }
    // Setup window parameters for display
    pInfo->typeInfo.native.pSharedData->videoWindow.nexusClientContext = m_nexusClient;
    android_atomic_release_store(1, &pInfo->typeInfo.native.pSharedData->videoWindow.windowIdPlusOne);

    err = pPort->AddBuffer(ppBufferHdr, pAppPrivate, ComputeBufferSize(pPort->GetDefinition()->format.video.nStride, pPort->GetDefinition()->format.video.nSliceHeight), (OMX_U8 *)pPrivateHandle, pInfo, false);
    if ( OMX_ErrorNone != err )
    {
        delete pInfo;
        BOMX_VideoDecoder_MemUnlock(pPrivateHandle);
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    // BOMX_VideoDecoder_MemUnlock - happens in FreeBuffer to preserve mapping as long as possible.
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
        ALOGW("Cannot add buffers to a tunneled port");
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }

    pInfo = new BOMX_VideoDecoderOutputBufferInfo;
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    memset(pInfo, 0, sizeof(*pInfo));
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
        // than the call to EnablePort or CommandStateSet and this will
        // come first.  It's always safe to do it here.
        if ( m_tunnelMode )
        {
            ALOGV("Selecting no output mode for output buffer %p", (void*)pBuffer);
            m_outputMode = BOMX_VideoDecoderOutputBufferType_eNone;
        }
        else if ( m_metadataEnabled )
        {
            ALOGV("Selecting metadata output mode for output buffer %p", (void*)pBuffer);
            m_outputMode = BOMX_VideoDecoderOutputBufferType_eMetadata;
        }
        else if ( m_nativeGraphicsEnabled )
        {
            ALOGV("Selecting native graphics output mode for output buffer %p", (void*)pBuffer);
            m_outputMode = BOMX_VideoDecoderOutputBufferType_eNative;
        }
        else
        {
            ALOGV("Selecting standard buffer output mode for output buffer %p", (void*)pBuffer);
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
        case BOMX_VideoDecoderOutputBufferType_eNone:
            {
                /* Cannot return error to the frameworks even though no output buffer should be needed in
                 * tunneled playback. This is to satisfy the frameworks without causing an assertion by still
                 * returning the buffer header.
                 */
                OMX_BUFFERHEADERTYPE *pHeader = new OMX_BUFFERHEADERTYPE;

                memset(pHeader, 0, sizeof(OMX_BUFFERHEADERTYPE));
                BOMX_STRUCT_INIT(pHeader);
                pHeader->pBuffer = pBuffer;
                pHeader->nAllocLen = nSizeBytes;
                pHeader->pAppPrivate = pAppPrivate;
                pHeader->pPlatformPrivate = static_cast <OMX_PTR> (this);
                *ppBufferHdr = pHeader;

                return OMX_ErrorNone;
            }
            break;
        default:
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
        // TODO: Implement if required
        ALOGW("AllocateBuffer is not supported for output ports");
        return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
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
        if ( m_outputMode == BOMX_VideoDecoderOutputBufferType_eNone )
        {
            delete pBufferHeader;
            return OMX_ErrorNone;
        }

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
#if BOMX_VIDEO_DECODER_DESTRIPE_PLANAR
            BOMX_VideoDecoder_SurfaceDestroy(&pInfo->typeInfo.standard.crMemBlkFd, pInfo->typeInfo.standard.hSurfaceCr);
            BOMX_VideoDecoder_SurfaceDestroy(&pInfo->typeInfo.standard.cbMemBlkFd, pInfo->typeInfo.standard.hSurfaceCb);
            BOMX_VideoDecoder_SurfaceDestroy(&pInfo->typeInfo.standard.yMemBlkFd, pInfo->typeInfo.standard.hSurfaceY);
#else
            BOMX_VideoDecoder_SurfaceDestroy(&pInfo->typeInfo.standard.destripeMemBlkFd, pInfo->typeInfo.standard.hDestripeSurface);
#endif
            break;
        case BOMX_VideoDecoderOutputBufferType_eNative:
            pFrameBuffer = pInfo->pFrameBuffer;
            BOMX_VideoDecoder_MemUnlock((private_handle_t *)pInfo->typeInfo.native.pPrivateHandle);
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
            if ( active && pFrameBuffer->state == BOMX_VideoDecoderFrameBufferState_eDelivered )
            {
                // Drop the buffer in the decoder
                ALOGV("Dropping frame %u on FreeBuffer", pFrameBuffer->frameStatus.serialNumber);
                pFrameBuffer->state = BOMX_VideoDecoderFrameBufferState_eDropReady;
                ReturnDecodedFrames();
            }
            else if ( pFrameBuffer->state == BOMX_VideoDecoderFrameBufferState_eDelivered ||
                      pFrameBuffer->state == BOMX_VideoDecoderFrameBufferState_eInvalid )
            {
                // Free up the buffer (decoder is already stopped)
                ALOGV("Discarding frame %u on FreeBuffer (%s)", pFrameBuffer->frameStatus.serialNumber, active?"invalid":"stopped");
                BOMX_VIDEO_STATS_ADD_EVENT(BOMX_VD_Stats::DISPLAY_FRAME, 0, 0, pFrameBuffer->frameStatus.serialNumber);
                BLST_Q_REMOVE(&m_frameBufferAllocList, pFrameBuffer, node);
                BOMX_VideoDecoder_StripedSurfaceDestroy(pFrameBuffer);
                BLST_Q_INSERT_TAIL(&m_frameBufferFreeList, pFrameBuffer, node);
            }
            else
            {
                ALOGV("Not discarding frame %u, state is already %u", pFrameBuffer->frameStatus.serialNumber, pFrameBuffer->state);
            }
        }

        delete pInfo;
    }
    PortStatusChanged();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_VideoDecoder::BuildInputFrame(
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
    BOMX_VideoDecoderInputBufferInfo *pInfo;
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
    * [PES Header w/PTS] | [Cached Codec Config Data] | [Codec Frame Header (e.g. BCMV)] | [First Frame Payload Chunk] |
    *   [PES Header(s) w/o PTS] | [Next Frame Payload Chunk] (repeats until frame is complete)
    *****************************************************************/

    pBuffer = BOMX_BUFFERHEADER_TO_BUFFER(pBufferHeader);
    ALOG_ASSERT(NULL != pBuffer);
    pInfo = (BOMX_VideoDecoderInputBufferInfo *)pBuffer->GetComponentPrivate();
    ALOG_ASSERT(NULL != pInfo);

    bufferBytesRemaining = chunkLength;

    ALOGV("Input Frame Offset %u, Length %u, PTS %#x first=%d", pBufferHeader->nOffset, chunkLength, pts, first ? 1 : 0);

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
    OMX_VIDEO_CODINGTYPE codec = GetCodec();
    if ( pts == 0 && m_ptsReceived &&
         (codec == OMX_VIDEO_CodingMPEG2 ||
          codec == OMX_VIDEO_CodingAVC ||
          codec == OMX_VIDEO_CodingH263 ||
          codec == OMX_VIDEO_CodingMPEG4 ||
          codec == OMX_VIDEO_CodingHEVC) )
    {
        // Fragment from previous input frame. No need to specify PTS.
        headerBytes = m_pPes->InitHeader(pPesHeader, pInfo->maxHeaderLen-pInfo->headerLen);
    }
    else
    {
        m_ptsReceived = true;
        headerBytes = m_pPes->InitHeader(pPesHeader, pInfo->maxHeaderLen-pInfo->headerLen, pts);
    }

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

void BOMX_VideoDecoder::DumpInputBuffer(OMX_BUFFERHEADERTYPE *pHeader)
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

OMX_ERRORTYPE BOMX_VideoDecoder::EmptyThisBuffer(
    OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    OMX_ERRORTYPE err;
    BOMX_Buffer *pBuffer;
    BOMX_VideoDecoderInputBufferInfo *pInfo;
    size_t payloadLen;
    void *pPayload;
    NEXUS_PlaypumpScatterGatherDescriptor *desc;
    NEXUS_Error errCode;
    size_t numConsumed, numRequested;
    uint8_t *pCodecHeader = NULL;
    size_t codecHeaderLength = 0;
    uint32_t frameLength = pBufferHeader->nFilledLen;

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
    if ( pBufferHeader->nInputPortIndex != m_videoPortBase )
    {
        ALOGW("Invalid buffer");
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    pInfo = (BOMX_VideoDecoderInputBufferInfo *)pBuffer->GetComponentPrivate();
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    if (m_pPlaypumpDescList == NULL)
    {
       m_pPlaypumpDescList = new NEXUS_PlaypumpScatterGatherDescriptor[m_maxDescriptorsPerBuffer];
       if (m_pPlaypumpDescList == NULL)
           return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }
    desc = m_pPlaypumpDescList;
    pInfo->numDescriptors = 0;
    pInfo->complete = false;

    ALOGV("%s, comp:%s, buff:%p len:%d ts:%lld flags:0x%x avail:%d", __FUNCTION__, GetName(), pBufferHeader->pBuffer, pBufferHeader->nFilledLen, pBufferHeader->nTimeStamp, pBufferHeader->nFlags, m_AvailInputBuffers-1);
    BOMX_VIDEO_STATS_ADD_EVENT(BOMX_VD_Stats::INPUT_FRAME, pBufferHeader->nTimeStamp, pBufferHeader->nFlags, pBufferHeader->nFilledLen, m_AvailInputBuffers);

    if ( m_pInputFile )
    {
        DumpInputBuffer(pBufferHeader);
    }

    if ( pBufferHeader->nFlags & OMX_BUFFERFLAG_CODECCONFIG )
    {
        if ( m_configBufferState != ConfigBufferState_eAccumulating )
        {
            // If the app re-sends config data after we have delivered it to the decoder we
            // may be receiving a dynamic resolution change.  Overwrite old config data with
            // the new data
            ALOGV("Invalidating cached config buffer ");
            ConfigBufferInit();
        }

        ALOGV("Accumulating %u bytes of codec config data", pBufferHeader->nFilledLen);

        err = ConfigBufferAppend(pBufferHeader->pBuffer + pBufferHeader->nOffset, pBufferHeader->nFilledLen);
        if ( err != OMX_ErrorNone )
        {
            return BOMX_ERR_TRACE(err);
        }

        // Config buffers aren't sent individually, they are just appended above and sent with the next frame.  Queue this buffer with no descriptors so it will be returned immediately.
        InputBufferNew();
        err = m_pVideoPorts[0]->QueueBuffer(pBufferHeader);
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }
        return OMX_ErrorNone;
    }

    // If we get here, we have an actual frame to send.

    if ( pBufferHeader->nFilledLen > 0 )
    {
        OMX_VIDEO_CODINGTYPE codec = GetCodec();
        if ( pBufferHeader->nTimeStamp == 0 && m_ptsReceived &&
             (codec == OMX_VIDEO_CodingMPEG2 ||
              codec == OMX_VIDEO_CodingAVC ||
              codec == OMX_VIDEO_CodingH263 ||
              codec == OMX_VIDEO_CodingMPEG4 ||
              codec == OMX_VIDEO_CodingHEVC) )
        {
            // Fragment from previous input frame. No need to add this to tracker.
            pInfo->pts = 0;
        }
        else
        {
            // Track the buffer
            if ( !m_pBufferTracker->Add(pBufferHeader, &pInfo->pts) )
            {
                ALOGW("Unable to track buffer");
                pInfo->pts = BOMX_TickToPts(&pBufferHeader->nTimeStamp);
            }
        }

        switch ( codec )
        {
        case OMX_VIDEO_CodingVP8:
        case OMX_VIDEO_CodingVP9:
        /* Also true for spark and possibly other codecs */
        {
            uint32_t packetLen = frameLength;
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

        // Build up descriptor list
        err = BuildInputFrame(pBufferHeader, true, frameLength, pInfo->pts, pCodecHeader, codecHeaderLength, desc, m_maxDescriptorsPerBuffer-1, &numRequested);
        if ( err != OMX_ErrorNone )
        {
            return BOMX_ERR_TRACE(err);
        }
        ALOG_ASSERT(numRequested <= (m_maxDescriptorsPerBuffer-1));

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

            // For VP9 insert an end-of-chunk BPP packet
            if ( OMX_VIDEO_CodingVP9 == (int)GetCodec() )
            {
                desc[numRequested].addr = m_pEndOfChunkBuffer;
                desc[numRequested].length = B_BPP_PACKET_LEN;
                numRequested++;
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
        InputBufferNew();
        err = m_pVideoPorts[0]->QueueBuffer(pBufferHeader);
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }
    }
    else
    {
        // Mark the empty frame as queued.
        InputBufferNew();
        err = m_pVideoPorts[0]->QueueBuffer(pBufferHeader);
        if ( err )
        {
            return BOMX_ERR_TRACE(err);
        }
    }

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
        ALOG_ASSERT(numConsumed == numRequested);
        pInfo->numDescriptors += numRequested;
        m_submittedDescriptors += numConsumed;
        m_eosPending = true;
        if ( NULL != m_pPesFile )
        {
            fwrite(m_pEosBuffer, 1, BOMX_VIDEO_EOS_LEN, m_pPesFile);
        }
    }

    if ( !m_tunnelMode && (m_pVideoPorts[1]->QueueDepth() > 0) )
    {
        // Force a check for new output frames each time a new input frame arrives if we have output buffers ready to fill
        B_Event_Set(m_hOutputFrameEvent);
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
            ALOGW("Only kMetadataBufferTypeGrallocSource buffers are supported in metadata mode.");
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }

        ALOG_ASSERT((pInfo->typeInfo.metadata.pMetadata == (void *)pBufferHeader->pBuffer));
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
    ALOGV("Fill Buffer, comp:%s ts %lld us serial %u pInfo %p HDR %p", GetName(), pBufferHeader->nTimeStamp, pFrameBuffer ? pFrameBuffer->frameStatus.serialNumber : -1, pInfo, pBufferHeader);
    // Determine what to do with the buffer
    if ( pFrameBuffer )
    {
        bool dropFrame = true;
        if ( pFrameBuffer->state == BOMX_VideoDecoderFrameBufferState_eInvalid )
        {
            // The frame has been flushed while the app owned it.  Move it back to the free list silently.
            ALOGV("Invalid FrameBuffer (%u) - Return to free list", pFrameBuffer->frameStatus.serialNumber);
            BLST_Q_REMOVE(&m_frameBufferAllocList, pFrameBuffer, node);
            BOMX_VideoDecoder_StripedSurfaceDestroy(pFrameBuffer);
            BLST_Q_INSERT_TAIL(&m_frameBufferFreeList, pFrameBuffer, node);
        }
        else
        {
            // Any state other than delivered and we've done something horribly wrong.
            ALOG_ASSERT(pFrameBuffer->state == BOMX_VideoDecoderFrameBufferState_eDelivered);
            // Need to check if this buffer is the same as the frame waiting to be displayed
            B_Mutex_Lock(m_hDisplayMutex);
            if (m_displayFrameAvailable && pFrameBuffer->frameStatus.serialNumber == m_frameSerial) {
                ALOGV("%s: Don't drop frame ready to be displayed!, serial:%d", __FUNCTION__, m_frameSerial);
                dropFrame = false;
            }
            B_Mutex_Unlock(m_hDisplayMutex);
            if (dropFrame) {
                pFrameBuffer->state = BOMX_VideoDecoderFrameBufferState_eDropReady;
                switch ( pInfo->type )
                {
                case BOMX_VideoDecoderOutputBufferType_eStandard:
                    ALOGV("Standard Buffer - Drop");
                    break;
                case BOMX_VideoDecoderOutputBufferType_eNative:
                    ALOGV("Native Buffer %u - Drop", pInfo->typeInfo.native.pSharedData->videoFrame.status.serialNumber);
                    break;
                case BOMX_VideoDecoderOutputBufferType_eMetadata:
                    ALOGV("Metadata Buffer %u - Drop", pFrameBuffer->frameStatus.serialNumber);
                    break;
                default:
                    break;
                }
            }
        }
        if (dropFrame) {
            pFrameBuffer->pBufferInfo = NULL;
            pFrameBuffer->pPrivateHandle = NULL;
        }
    }
    else
    {
        ALOGV("FrameBuffer not set");
    }
    pBuffer->Reset();
    err = m_pVideoPorts[1]->QueueBuffer(pBufferHeader);
    ALOG_ASSERT(err == OMX_ErrorNone);

    // Return frames to nexus immediately if possible
    ReturnDecodedFrames();

    // Signal thread to look for more
    if (!m_tunnelMode)
    {
       B_Event_Set(m_hOutputFrameEvent);
    }

    return OMX_ErrorNone;
}

static bool FindBufferFromPts(BOMX_Buffer *pBuffer, void *pData)
{
    BOMX_VideoDecoderInputBufferInfo *pInfo;
    uint32_t *pPts = (uint32_t *)pData;

    ALOG_ASSERT(NULL != pBuffer);
    ALOG_ASSERT(NULL != pPts);

    pInfo = (BOMX_VideoDecoderInputBufferInfo *)pBuffer->GetComponentPrivate();
    ALOG_ASSERT(NULL != pInfo);

    return (pInfo->pts == *pPts) ? true : false;
}

void BOMX_VideoDecoder::CancelTimerId(B_SchedulerTimerId& timerId)
{
    if ( timerId )
    {
        CancelTimer(timerId);
        timerId = NULL;
    }
}

OMX_ERRORTYPE BOMX_VideoDecoder::VerifyInputPortBuffers()
{
    OMX_PARAM_PORTDEFINITIONTYPE portDef;
    bool updatePortDef = false;
    OMX_ERRORTYPE err = OMX_ErrorNone;

    m_pVideoPorts[0]->GetDefinition(&portDef);
    // Check if we have to recalculate the number of input buffers
    if (portDef.nBufferSize > B_DATA_BUFFER_SIZE_HIGHRES) {
        size_t maxInputBuffMem = B_DATA_BUFFER_SIZE_HIGHRES * B_NUM_BUFFERS;
        size_t newBufferCount = maxInputBuffMem / portDef.nBufferSize;
        ALOGV("%s: setting input buffer count to:%zu", __FUNCTION__, newBufferCount);
        if (newBufferCount < 2)
        {
            ALOGE("%s: input buffer size:%u is too large", __FUNCTION__, portDef.nBufferSize);
            return OMX_ErrorInsufficientResources;
        }
        portDef.nBufferCountActual = newBufferCount;
        updatePortDef = true;
    }

    if (portDef.nBufferSize < B_DATA_BUFFER_SIZE_DEFAULT) {
        portDef.nBufferSize = B_DATA_BUFFER_SIZE_DEFAULT;
        updatePortDef = true;
    }

    if (updatePortDef) {
        err = m_pVideoPorts[0]->SetDefinition(&portDef);
        if (err)
            return err;
    }

    // Update variables dependent on input buffer size
    m_maxDescriptorsPerBuffer = B_MAX_DESCRIPTORS_PER_BUFFER(portDef.nBufferSize);
    if (m_pPlaypumpDescList != NULL)
    {
       delete [] m_pPlaypumpDescList;
       m_pPlaypumpDescList = NULL;
    }

    return OMX_ErrorNone;
}

void BOMX_VideoDecoder:: CleanupPortBuffers(OMX_U32 nPortIndex)
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

unsigned BOMX_VideoDecoder::PlaypumpEvent(bool bTimeout /* = false */)
{
    NEXUS_PlaypumpStatus playpumpStatus;
    unsigned numComplete, res = 0;

    CancelTimerId(m_playpumpTimerId);
    if ( NULL == m_hPlaypump )
    {
        // This can happen with rapid startup/shutdown sequences such as random action tests
        ALOGW("Playpump event received, but playpump has been closed.");
        return 0;
    }

    NEXUS_Playpump_GetStatus(m_hPlaypump, &playpumpStatus);
    if ( playpumpStatus.started )
    {
        BOMX_Buffer *pBuffer, *pFifoHead=NULL;

        if ( m_hSimpleVideoDecoder && !m_inputFlushing )
        {
            NEXUS_VideoDecoderFifoStatus fifoStatus;
            NEXUS_Error errCode = NEXUS_SimpleVideoDecoder_GetFifoStatus(m_hSimpleVideoDecoder, &fifoStatus);
            if ( NEXUS_SUCCESS == errCode && fifoStatus.pts.valid )
            {
                pFifoHead = m_pVideoPorts[0]->FindBuffer(FindBufferFromPts, (void *)&fifoStatus.pts.leastRecent);
            }
        }

        numComplete = m_submittedDescriptors - playpumpStatus.descFifoDepth;

        for (pBuffer = m_pVideoPorts[0]->GetBuffer();
             (pBuffer != NULL) && (bTimeout || (pBuffer != pFifoHead));
             pBuffer = m_pVideoPorts[0]->GetNextBuffer(pBuffer))
        {
            BOMX_VideoDecoderInputBufferInfo *pInfo =
            (BOMX_VideoDecoderInputBufferInfo *)pBuffer->GetComponentPrivate();
            ALOG_ASSERT(NULL != pInfo);
            if (pInfo->complete)
                continue;
            if ( numComplete >= pInfo->numDescriptors )
            {
                numComplete -= pInfo->numDescriptors;
                ALOG_ASSERT(m_submittedDescriptors >= pInfo->numDescriptors);
                m_submittedDescriptors -= pInfo->numDescriptors;
                pInfo->complete = true;
                ++m_completedInputBuffers;
                ++res;
            }
            else
            {
                // Some descriptors are still pending for this buffer
                break;
            }
        }

        // If there are still input buffers waiting in rave, set the timer to try again later.
        if ( pFifoHead )
        {
            ALOGV("Data still pending in RAVE.  Starting Timer.");
            m_playpumpTimerId = StartTimer(BOMX_VideoDecoder_GetFrameInterval(m_frameRate), BOMX_VideoDecoder_PlaypumpEvent, static_cast<void *>(this));
        }
    }

    return res;
}

void BOMX_VideoDecoder::InputBufferNew()
{
    ALOG_ASSERT(m_AvailInputBuffers > 0);
    --m_AvailInputBuffers;
    if (m_AvailInputBuffers == 0) {
        ALOGV("%s: reached zero input buffers, m_inputBuffersTimerId:%p rate:%s",
              __FUNCTION__, m_inputBuffersTimerId, BOMX_VideoDecoder_GetFrameString(m_frameRate));
        CancelTimerId(m_inputBuffersTimerId);
        m_inputBuffersTimerId = StartTimer(BOMX_VideoDecoder_GetFrameInterval(m_frameRate)*2, BOMX_VideoDecoder_InputBuffersTimer, static_cast<void *>(this));
    }
}

void BOMX_VideoDecoder::InputBufferReturned()
{
    unsigned totalBuffers = m_pVideoPorts[0]->GetDefinition()->nBufferCountActual;
    CancelTimerId(m_inputBuffersTimerId);
    ++m_AvailInputBuffers;
    --m_completedInputBuffers;
    ALOG_ASSERT(m_AvailInputBuffers <= totalBuffers && m_completedInputBuffers <= totalBuffers);
}

void BOMX_VideoDecoder::InputBufferCounterReset()
{
    CancelTimerId(m_inputBuffersTimerId);
    m_AvailInputBuffers = m_pVideoPorts[0]->GetDefinition()->nBufferCountActual;
}

uint32_t BOMX_VideoDecoder::ReturnInputBuffers(OMX_TICKS decodeTs, InputReturnMode mode)
{
    uint32_t count = 0, timeoutCount = 0;
    BOMX_VideoDecoderInputBufferInfo *pInfo;
    BOMX_Buffer *pNextBuffer, *pReturnBuffer = NULL, *pBuffer = m_pVideoPorts[0]->GetBuffer();
    while ( pBuffer != NULL )
    {
        pInfo = (BOMX_VideoDecoderInputBufferInfo *)pBuffer->GetComponentPrivate();
        ALOG_ASSERT(NULL != pInfo);
        if ( !pInfo->complete )
        {
            ALOGV("Buffer:%p not completed yet", pBuffer);
            break;
        }

        if ( BOMX_VideoDecoder_GetFrameInterval(m_frameRate) <= B_INPUT_RETURN_SPEEDUP_THRES_INTERVAL &&
             m_outputHeight >= B_INPUT_RETURN_SPEEDUP_THRES_HEIGHT &&
             m_completedInputBuffers > B_MAX_INPUT_COMPLETED_COUNT &&
             !m_tunnelMode )
        {
            pReturnBuffer = pBuffer;
        }
        else if ( mode != InputReturnMode_eTimestamp || pReturnBuffer == NULL )
        {
            if ( mode == InputReturnMode_eTimeout && timeoutCount++ >= B_MAX_INPUT_TIMEOUT_RETURN )
            {
                break;
            }

            pReturnBuffer = pBuffer;
        }
        else if ( *pBuffer->GetTimestamp() == decodeTs )
        {
            pReturnBuffer = pBuffer;
            break;
        }

        pBuffer = m_pVideoPorts[0]->GetNextBuffer(pBuffer);
    }

    if ( pReturnBuffer != NULL )
    {
        pBuffer = m_pVideoPorts[0]->GetBuffer();
        while ( pBuffer != pReturnBuffer )
        {
            pNextBuffer = m_pVideoPorts[0]->GetNextBuffer(pBuffer);

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

    ALOGV("%s: returned %u buffers, %u available, mode %u", __FUNCTION__, count, m_AvailInputBuffers, mode);
    return count;
}

bool BOMX_VideoDecoder::ReturnInputPortBuffer(BOMX_Buffer *pBuffer)
{
    ReturnPortBuffer(m_pVideoPorts[0], pBuffer);
    InputBufferReturned();

    // If the buffer has codec config flags it shouldn't be added
    // to count. These buffers don't have a corresponding output frame
    // so it's ok to return them as soon as possible.
    OMX_BUFFERHEADERTYPE *pHeader = pBuffer->GetHeader();
    ALOG_ASSERT(pHeader != NULL);
    return (!(pHeader->nFlags & OMX_BUFFERFLAG_CODECCONFIG ));
}

void BOMX_VideoDecoder::InputBufferTimeoutCallback()
{
    CancelTimerId(m_inputBuffersTimerId);
    if ( ReturnInputBuffers(0, InputReturnMode_eTimeout) == 0 )
    {
        // Retry returning the input buffers again by marking all the
        // input buffers in RAVE as completed.
        if ( PlaypumpEvent(true) == 0 )
        {
            ALOGV("Keep retrying to return input buffers after timeout");

            CancelTimerId(m_inputBuffersTimerId);
            m_inputBuffersTimerId = StartTimer(BOMX_VideoDecoder_GetFrameInterval(m_frameRate), BOMX_VideoDecoder_InputBuffersTimer, static_cast<void *>(this));
        }
    }
}

void BOMX_VideoDecoder::FormatChangeTimeoutCallback()
{
    // Timed out waiting for the output buffers to come back. Send format change notification immediately.
    CancelTimerId(m_formatChangeTimerId);
    m_formatChangeState = FCState_eProcessCallback;
    OutputFrameEvent();
}

void BOMX_VideoDecoder::SourceChangedEvent()
{
    NEXUS_VideoDecoderStatus vdecStatus;
    OMX_PARAM_PORTDEFINITIONTYPE portDef;
    BOMX_Port *pPort = FindPortByIndex(m_videoPortBase+1);
    OMX_CONFIG_RECTTYPE cropRect;

    if ( m_hSimpleVideoDecoder )
    {
        (void)NEXUS_SimpleVideoDecoder_GetStatus(m_hSimpleVideoDecoder, &vdecStatus);
        if ( vdecStatus.started && vdecStatus.numDecoded > 0 )
        {
            bool formatChanged = false;
            pPort->GetDefinition(&portDef);
            if ( portDef.format.video.nFrameWidth != vdecStatus.display.width ||
                 portDef.format.video.nFrameHeight != vdecStatus.display.height )
            {
                portDef.format.video.nFrameWidth = vdecStatus.display.width;
                portDef.format.video.nFrameHeight = vdecStatus.display.height;
                m_outputWidth = portDef.format.video.nFrameWidth;
                m_outputHeight = portDef.format.video.nFrameHeight;
                formatChanged=true;
            }

            if (formatChanged) {
                pPort->SetDefinition(&portDef);
                BOMX_STRUCT_INIT(&cropRect);
                cropRect.nLeft = 0;
                cropRect.nTop = 0;
                cropRect.nWidth = m_outputWidth;
                cropRect.nHeight = m_outputHeight;
                if ( m_callbacks.EventHandler )
                {
                    (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate, OMX_EventPortSettingsChanged, m_videoPortBase+1, OMX_IndexConfigCommonOutputCrop, &cropRect);
                }
            }
        }
    }
}

void BOMX_VideoDecoder::StreamChangedEvent()
{
    NEXUS_VideoDecoderStreamInformation streamInfo;

    if ( m_hSimpleVideoDecoder )
    {
        NEXUS_SimpleVideoDecoder_GetStreamInformation(m_hSimpleVideoDecoder, &streamInfo);
        if (!streamInfo.valid)
        {
            ALOGV("%s: invalid stream information", __FUNCTION__);
            return;
        }

        ALOGV("%s: stream information", __FUNCTION__);
        ALOGV("eotf:%u", streamInfo.eotf);
        ALOGV("transferCharacteristics:%u", streamInfo.transferCharacteristics);
        ALOGV("color depth:%u", streamInfo.colorDepth);
        ALOGV("input content light level: (%u, %u)",
                streamInfo.contentLightLevel.max,
                streamInfo.contentLightLevel.maxFrameAverage);

        ALOGV("red (%u, %u)",
                streamInfo.masteringDisplayColorVolume.redPrimary.x,
                streamInfo.masteringDisplayColorVolume.redPrimary.y);
        ALOGV("green (%u, %u)",
                streamInfo.masteringDisplayColorVolume.greenPrimary.x,
                streamInfo.masteringDisplayColorVolume.greenPrimary.y);
        ALOGV("blue (%u, %u)",
                streamInfo.masteringDisplayColorVolume.bluePrimary.x,
                streamInfo.masteringDisplayColorVolume.bluePrimary.y);
        ALOGV("white (%u, %u)",
                streamInfo.masteringDisplayColorVolume.whitePoint.x,
                streamInfo.masteringDisplayColorVolume.whitePoint.y);
        ALOGV("luma (%u, %u)",
                streamInfo.masteringDisplayColorVolume.luminance.max,
                streamInfo.masteringDisplayColorVolume.luminance.min);
       m_videoStreamInfo = streamInfo;

       // Trigger event for framework to update both HDR and ColorAspects information
       if ( m_callbacks.EventHandler )
       {
           (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate, OMX_EventPortSettingsChanged, m_videoPortBase+1, OMX_IndexParamDescribeHdrColorInfo, NULL);
       }
    }
}

void BOMX_VideoDecoder::ColorAspectsFromNexusStreamInfo(ColorAspects *colorAspects)
{
    // TODO: Range needs to come from hdmi output, which we don't have at this time
    colorAspects->mRange = ColorAspects::RangeUnspecified;

    // Primaries
    switch (m_videoStreamInfo.colorPrimaries)
    {
    case NEXUS_ColorPrimaries_eItu_R_BT_709:
        colorAspects->mPrimaries = ColorAspects::PrimariesBT709_5;
        break;
    case NEXUS_ColorPrimaries_eUnknown:
        colorAspects->mPrimaries = ColorAspects::PrimariesUnspecified;
        break;
    case NEXUS_ColorPrimaries_eItu_R_BT_470_2_M:
    case NEXUS_ColorPrimaries_eItu_R_BT_470_2_BG:
    case NEXUS_ColorPrimaries_eSmpte_170M:
    case NEXUS_ColorPrimaries_eSmpte_240M:
        colorAspects->mPrimaries = ColorAspects::PrimariesOther;
        break;
    case NEXUS_ColorPrimaries_eGenericFilm:
        colorAspects->mPrimaries = ColorAspects::PrimariesGenericFilm;
        break;
    case NEXUS_ColorPrimaries_eItu_R_BT_2020:
        colorAspects->mPrimaries = ColorAspects::PrimariesBT2020;
        break;
    default:
        colorAspects->mPrimaries = ColorAspects::PrimariesUnspecified;
        break;
    }

    // Transfer
    switch (m_videoStreamInfo.transferCharacteristics)
    {
    case NEXUS_TransferCharacteristics_eUnknown:
        colorAspects->mTransfer = ColorAspects::TransferUnspecified;
        break;
    case NEXUS_TransferCharacteristics_eItu_R_BT_470_2_M:
        colorAspects->mTransfer = ColorAspects::TransferGamma22;
        break;
    case NEXUS_TransferCharacteristics_eItu_R_BT_470_2_BG:
        colorAspects->mTransfer = ColorAspects::TransferGamma28;
        break;
    case NEXUS_TransferCharacteristics_eItu_R_BT_709:
    case NEXUS_TransferCharacteristics_eSmpte_170M:
        colorAspects->mTransfer = ColorAspects::TransferSMPTE170M;
        break;
    case NEXUS_TransferCharacteristics_eLinear:
        colorAspects->mTransfer = ColorAspects::TransferLinear;
        break;
    case NEXUS_TransferCharacteristics_eSmpte_240M:
    case NEXUS_TransferCharacteristics_eIec_61966_2_4:
    case NEXUS_TransferCharacteristics_eItu_R_BT_2020_10bit:
    case NEXUS_TransferCharacteristics_eItu_R_BT_2020_12bit:
        colorAspects->mTransfer = ColorAspects::TransferOther;
        break;
    case NEXUS_TransferCharacteristics_eSmpte_ST_2084:
        colorAspects->mTransfer = ColorAspects::TransferST2084;
        break;
    case NEXUS_TransferCharacteristics_eArib_STD_B67:
        colorAspects->mTransfer = ColorAspects::TransferHLG;
        break;
    case NEXUS_TransferCharacteristics_eMax:
    default:
        colorAspects->mTransfer = ColorAspects::TransferUnspecified;
        break;
    }

    // Matrix coefficients
    switch (m_videoStreamInfo.matrixCoefficients)
    {
    case NEXUS_MatrixCoefficients_eItu_R_BT_709:
        colorAspects->mMatrixCoeffs = ColorAspects::MatrixBT709_5;
        break;
    case NEXUS_MatrixCoefficients_eUnknown:
        colorAspects->mMatrixCoeffs = ColorAspects::MatrixUnspecified;
        break;
    case NEXUS_MatrixCoefficients_eSmpte_240M:
        colorAspects->mMatrixCoeffs = ColorAspects::MatrixSMPTE240M;
        break;
    case NEXUS_MatrixCoefficients_eItu_R_BT_2020_NCL:
        colorAspects->mMatrixCoeffs = ColorAspects::MatrixBT2020;
        break;
    case NEXUS_MatrixCoefficients_eItu_R_BT_2020_CL:
        colorAspects->mMatrixCoeffs = ColorAspects::MatrixBT2020Constant;
        break;
    case NEXUS_MatrixCoefficients_eHdmi_RGB:
    case NEXUS_MatrixCoefficients_eDvi_Full_Range_RGB:
    case NEXUS_MatrixCoefficients_eFCC:
    case NEXUS_MatrixCoefficients_eItu_R_BT_470_2_BG:
    case NEXUS_MatrixCoefficients_eSmpte_170M:
    case NEXUS_MatrixCoefficients_eXvYCC_709:
    case NEXUS_MatrixCoefficients_eXvYCC_601:
    case NEXUS_MatrixCoefficients_eHdmi_Full_Range_YCbCr:
        colorAspects->mMatrixCoeffs = ColorAspects::MatrixOther;
        break;
    case NEXUS_MatrixCoefficients_eMax:
    default:
        colorAspects->mMatrixCoeffs = ColorAspects::MatrixUnspecified;
        break;
    }
}


void BOMX_VideoDecoder::OutputFrameEvent()
{
    // Check for new frames
    PollDecodedFrames();
}

void BOMX_VideoDecoder::DisplayFrameEvent()
{
    NEXUS_Rect framePosition, frameClip;
    unsigned frameSerial, frameWidth, frameHeight, framezOrder;

    B_Mutex_Lock(m_hDisplayMutex);
    if (!m_displayFrameAvailable) {
      // It may happen when component is being stopped or when a
      // new display frame arrives and m_displayFrameAvailable is true
      ALOGV("%s: No display frame available!", __FUNCTION__);
      B_Mutex_Unlock(m_hDisplayMutex);
      return;
    }
    memcpy(&framePosition, &m_framePosition, sizeof(m_framePosition));
    memcpy(&frameClip, &m_frameClip, sizeof(m_frameClip));
    frameSerial = m_frameSerial;
    frameWidth = m_frameWidth;
    frameHeight = m_frameHeight;
    framezOrder = m_framezOrder;
    m_displayFrameAvailable = false;
    B_Mutex_Unlock(m_hDisplayMutex);
    Lock();
    SetVideoGeometry_locked(&framePosition, &frameClip, frameSerial, frameWidth, frameHeight, framezOrder, true);
    DisplayFrame_locked(frameSerial);
    Unlock();
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
    {"OMX.google.android.index.prepareForAdaptivePlayback", (int)OMX_IndexParamPrepareForAdaptivePlayback},
    {"OMX.google.android.index.configureVideoTunnelMode", (int)OMX_IndexParamConfigureVideoTunnelMode},
    {"OMX.google.android.index.describeHDRStaticInfo", (int)OMX_IndexParamDescribeHdrColorInfo},
    {"OMX.google.android.index.describeColorAspects", (int)OMX_IndexParamDescribeColorAspects},
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
            if ( (g_extensions[i].index == OMX_IndexParamStoreMetaDataInBuffers) && (0 == property_get_int32(B_PROPERTY_ENABLE_METADATA, 1)) )
            {
                ALOGD("Metadata output disabled");
                // Drop out here to reduce spam in logcat
                return OMX_ErrorUnsupportedIndex;
            }
            else if ( !m_tunnelMode && ((g_extensions[i].index == OMX_IndexParamDescribeHdrColorInfo)
                                    ||  (g_extensions[i].index == OMX_IndexParamDescribeColorAspects)) )
            {
                ALOGD("Interface %s not supported in non-tunneled mode", g_extensions[i].pName);
                return OMX_ErrorUnsupportedIndex;

            }
            else
            {
                *pIndexType = (OMX_INDEXTYPE)g_extensions[i].index;
                return OMX_ErrorNone;
            }
        }
    }

    ALOGW("Extension %s is not supported", cParameterName);
    return OMX_ErrorUnsupportedIndex;
}

OMX_ERRORTYPE BOMX_VideoDecoder::GetConfig(
        OMX_IN  OMX_INDEXTYPE nIndex,
        OMX_INOUT OMX_PTR pComponentConfigStructure)
{
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
        pRect->nWidth = m_outputWidth;
        pRect->nHeight = m_outputHeight;
        ALOGV("Returning crop %ux%u @ %u,%u", pRect->nWidth, pRect->nHeight, pRect->nLeft, pRect->nTop);
        return OMX_ErrorNone;
    }
    case OMX_IndexParamDescribeHdrColorInfo:
    {
        DescribeHDRStaticInfoParams *pHDRStaticInfo = (DescribeHDRStaticInfoParams *)pComponentConfigStructure;
        BOMX_STRUCT_VALIDATE(pHDRStaticInfo);
        ALOGV("GetConfig OMX_IndexParamDescribeHdrColorInfo");
        if ( pHDRStaticInfo->nPortIndex != m_videoPortBase+1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }

        if (!m_tunnelMode)
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        // Initialize with values provided by the framework (if any)
        if (m_hdrStaticInfoSet)
        {
            pHDRStaticInfo->sInfo = m_hdrStaticInfo;
        }
        // Overwrite parameters with existing stream information
        if (m_videoStreamInfo.valid)
        {
            HDRStaticInfo *sInfo = &pHDRStaticInfo->sInfo;
            sInfo->sType1.mR.x = m_videoStreamInfo.masteringDisplayColorVolume.redPrimary.x;
            sInfo->sType1.mR.y = m_videoStreamInfo.masteringDisplayColorVolume.redPrimary.y;
            sInfo->sType1.mG.x = m_videoStreamInfo.masteringDisplayColorVolume.greenPrimary.x;
            sInfo->sType1.mG.y = m_videoStreamInfo.masteringDisplayColorVolume.greenPrimary.y;
            sInfo->sType1.mB.x = m_videoStreamInfo.masteringDisplayColorVolume.bluePrimary.x;
            sInfo->sType1.mB.y = m_videoStreamInfo.masteringDisplayColorVolume.bluePrimary.y;
            sInfo->sType1.mW.x = m_videoStreamInfo.masteringDisplayColorVolume.whitePoint.x;
            sInfo->sType1.mW.y = m_videoStreamInfo.masteringDisplayColorVolume.whitePoint.x;
            sInfo->sType1.mMaxContentLightLevel =  m_videoStreamInfo.contentLightLevel.max;
            sInfo->sType1.mMaxFrameAverageLightLevel = m_videoStreamInfo.contentLightLevel.maxFrameAverage;
            // TODO: Currently not exposed, 'zero' is a valid value.
            sInfo->sType1.mMaxDisplayLuminance = 0;
            sInfo->sType1.mMinDisplayLuminance = 0;
        }

        return OMX_ErrorNone;
    }
    case OMX_IndexParamDescribeColorAspects:
    {
        DescribeColorAspectsParams *pColorAspects = (DescribeColorAspectsParams *)pComponentConfigStructure;
        BOMX_STRUCT_VALIDATE(pColorAspects);
        ALOGV("GetConfig OMX_IndexParamDescribeColorAspects");
        if ( pColorAspects->nPortIndex != m_videoPortBase+1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if (!m_tunnelMode)
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }

        // TODO: Look into implementing this request
        if (pColorAspects->bRequestingDataSpace || pColorAspects->bDataSpaceChanged)
        {
            ALOGW("%s: requestingDataSpace:%u, dataSpaceChanged:%u",
                    __FUNCTION__, pColorAspects->bRequestingDataSpace, pColorAspects->bDataSpaceChanged);
            return OMX_ErrorUnsupportedSetting;
        }

        if (m_colorAspectsSet)
        {
            pColorAspects->sAspects = m_colorAspects;
        }

        if (m_videoStreamInfo.valid)
        {
            // overwrite fields obtained from the stream
            ColorAspectsFromNexusStreamInfo(&pColorAspects->sAspects);
            ColorAspects *pAspects = &pColorAspects->sAspects;
            ALOGV("ColorAspects from stream [(R:%u, P:%u, M:%u, T:%u]",
                pAspects->mRange, pAspects->mPrimaries, pAspects->mMatrixCoeffs, pAspects->mTransfer);
        }

        return OMX_ErrorNone;
    }
    default:
        ALOGW("Config index %#x is not supported", nIndex);
        return BOMX_ERR_TRACE(OMX_ErrorUnsupportedIndex);
    }
}

OMX_ERRORTYPE BOMX_VideoDecoder::SetConfig(
        OMX_IN  OMX_INDEXTYPE nIndex,
        OMX_IN  OMX_PTR pComponentConfigStructure)
{
    switch ( (int)nIndex )
    {
    case OMX_IndexParamDescribeHdrColorInfo:
    {
        DescribeHDRStaticInfoParams *pHDRStaticInfo = (DescribeHDRStaticInfoParams *)pComponentConfigStructure;
        BOMX_STRUCT_VALIDATE(pHDRStaticInfo);
        ALOGD("SetConfig OMX_IndexParamDescribeHdrColorInfo");
        if ( pHDRStaticInfo->nPortIndex != m_videoPortBase+1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }

        if (!m_tunnelMode)
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        m_hdrStaticInfo = pHDRStaticInfo->sInfo;
        m_hdrStaticInfoSet = true;
        HDRStaticInfo *sInfo = &m_hdrStaticInfo;
        ALOGD("HDR params from framework [r:(%u %u),g:(%u %u),b:(%u %u),w:(%u %u),mcll:%u,mall:%u,madl:%u,midl:%u",
            sInfo->sType1.mR.x, sInfo->sType1.mR.y, sInfo->sType1.mG.x, sInfo->sType1.mG.y, sInfo->sType1.mB.x,
            sInfo->sType1.mB.y, sInfo->sType1.mW.x, sInfo->sType1.mW.y, sInfo->sType1.mMaxContentLightLevel,
            sInfo->sType1.mMaxFrameAverageLightLevel, sInfo->sType1.mMaxDisplayLuminance, sInfo->sType1.mMinDisplayLuminance);

        return OMX_ErrorNone;
    }
    case OMX_IndexParamDescribeColorAspects:
    {
        DescribeColorAspectsParams *pColorAspects = (DescribeColorAspectsParams *)pComponentConfigStructure;
        BOMX_STRUCT_VALIDATE(pColorAspects);
        ALOGD("SetConfig OMX_IndexParamDescribeColorAspects");
        if ( pColorAspects->nPortIndex != m_videoPortBase+1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }

        if (!m_tunnelMode)
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        m_colorAspectsSet = true;
        m_colorAspects = pColorAspects->sAspects;
        ALOGD("ColorAspects from framework [(R:%u, P:%u, M:%u, T:%u]",
            m_colorAspects.mRange, m_colorAspects.mPrimaries, m_colorAspects.mMatrixCoeffs, m_colorAspects.mTransfer);
        return OMX_ErrorNone;
    }
    default:
        ALOGW("Config index %#x is not supported", nIndex);
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

    if (m_tunnelMode)
    {
        return;
    }

    ALOGV("Invalidating decoded frame list");

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
            ALOGV("Invalidating frame buffer %p (state %u)", pBuffer, pBuffer->state);
            BLST_Q_REMOVE(&m_frameBufferAllocList, pBuffer, node);
            BOMX_VideoDecoder_StripedSurfaceDestroy(pBuffer);
            BLST_Q_INSERT_TAIL(&m_frameBufferFreeList, pBuffer, node);
        }
    }
}

void BOMX_VideoDecoder::ResetEstimation()
{
    m_deltaUs = 0;
    m_frStableCount = 0;
}

void BOMX_VideoDecoder::EstimateFrameRate(OMX_TICKS tsUs)
{
    // Estimate frame rate when the video decoder cannot provide a known frame rate.
    // If the frame rate has been estimated, continue the estimation for possible VFR streams
    // until the video decoder can provide a known frame rate.
    if ( m_frameRate == NEXUS_VideoFrameRate_eUnknown || m_frEstimated )
    {
        if ( m_lastTsUs != B_TIMESTAMP_INVALID && tsUs > m_lastTsUs )
        {
            OMX_TICKS deltaUs = tsUs - m_lastTsUs;
            if ( m_frStableCount == 0 )
            {
                m_deltaUs = deltaUs;
                m_frStableCount++;
            }
            else
            {
                if ( llabs(m_deltaUs - deltaUs) > B_FR_EST_STABLE_DELTA_THRESHOLD )
                {
                    ResetEstimation();
                }
                else
                {
                    m_frStableCount++;
                }

                if ( m_frStableCount == B_FR_EST_NUM_STABLE_DELTA_NEEDED )
                {
                    NEXUS_VideoFrameRate rate = BOMX_FrameRateToNexus(m_deltaUs);
                    if ( m_frameRate != rate )
                    {
                        ALOGI("Estimated frame rate %s -> %s", BOMX_VideoDecoder_GetFrameString(m_frameRate), BOMX_VideoDecoder_GetFrameString(rate));

                        m_frameRate = rate;
                        m_frEstimated = true;
                    }
                }
            }
        }
        else
        {
            ResetEstimation();
        }

        m_lastTsUs = tsUs;
    }
}

void BOMX_VideoDecoder::PollDecodedFrames()
{
    NEXUS_VideoDecoderFrameStatus frameStatus[B_MAX_DECODED_FRAMES], *pFrameStatus;
    BOMX_VideoDecoderFrameBuffer *pBuffer;
    NEXUS_VideoDecoderStatus status;
    NEXUS_Error errCode;
    unsigned numFrames=0;
    unsigned i;

    if ( NULL == m_hSimpleVideoDecoder )
    {
        return;
    }

    errCode = NEXUS_SimpleVideoDecoder_GetStatus(m_hSimpleVideoDecoder, &status);
    if ( errCode == NEXUS_SUCCESS && status.started && status.frameRate != NEXUS_VideoFrameRate_eUnknown && m_frameRate != status.frameRate )
    {
        ALOGI("Detected frame rate %s -> %s", BOMX_VideoDecoder_GetFrameString(m_frameRate), BOMX_VideoDecoder_GetFrameString(status.frameRate));
        m_frameRate = status.frameRate;
        m_frEstimated = false;
        m_lastTsUs = B_TIMESTAMP_INVALID;
        ResetEstimation();
    }

    // In tunnel mode, there is no output port populated on the omx component and we do not expect
    // any frame back from the decoder either, so skip all of this...
    if (m_tunnelMode)
    {
        // Report the current rendering timestamp back to the framework
        if ( m_callbacks.EventHandler != NULL && errCode == NEXUS_SUCCESS )
        {
            OMX_BUFFERHEADERTYPE omxHeader;
            nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
            uint32_t stcSync;

            if (m_waitingForStc) {
                bool resumeDecoder = false;

                NEXUS_SimpleStcChannel_GetStc(m_tunnelStcChannelSync, &stcSync);
                if (stcSync != B_STC_SYNC_INVALID_VALUE) {
                    resumeDecoder = true;
                    m_stcSyncValue = stcSync;
                    ALOGV("%s: Resume decoder on stcSync:%u",  __FUNCTION__, stcSync);
                } else if (toMillisecondTimeoutDelay(m_flushTime, now) > B_WAIT_FOR_STC_SYNC_TIMEOUT_MS) {
                    ALOGW("%s: timeout waiting for stc", __FUNCTION__);
                    resumeDecoder = true;
                    m_stcSyncValue = B_STC_SYNC_INVALID_VALUE;
                }

                if (resumeDecoder) {
                   m_waitingForStc = false;
                   m_stcResumePending = true;
                   NEXUS_VideoDecoderTrickState vdecTrickState;

                   errCode = NEXUS_SimpleStcChannel_SetRate(m_tunnelStcChannel, 0, 1);
                   if (errCode != NEXUS_SUCCESS)
                       ALOGE("%s: error setting stc rate to 0", __FUNCTION__);
                   if (stcSync != B_STC_SYNC_INVALID_VALUE) {
                       errCode = NEXUS_SimpleVideoDecoder_SetStartPts(m_hSimpleVideoDecoder, stcSync);
                       ALOGV("%s: setting startPts:%u", __FUNCTION__, stcSync);
                       if (errCode != NEXUS_SUCCESS)
                           ALOGE("%s: error setting start pts", __FUNCTION__);
                   }

                   NEXUS_SimpleVideoDecoder_GetTrickState(m_hSimpleVideoDecoder, &vdecTrickState);
                   vdecTrickState.rate = NEXUS_NORMAL_DECODE_RATE;
                   errCode = NEXUS_SimpleVideoDecoder_SetTrickState(m_hSimpleVideoDecoder, &vdecTrickState);
                   if (errCode != NEXUS_SUCCESS)
                       ALOGE("%s: error setting trick state", __FUNCTION__);
                }
            }

            if (m_stcResumePending) {
               NEXUS_SimpleStcChannel_GetStc(m_tunnelStcChannelSync, &stcSync);
               ALOGV("Trying to resume, pts:%u, stc:%u", status.pts, stcSync);
               if (status.pts >= stcSync) {
                   errCode = NEXUS_SimpleStcChannel_SetRate(m_tunnelStcChannel, 1, 0);
                   if (errCode != NEXUS_SUCCESS)
                       ALOGE("%s: error setting stc rate to 1", __FUNCTION__);
                   else {
                       m_stcResumePending = false;
                   }
               }
            }

            bool reportRenderedFrame = false;
            if (status.pts != 0 && m_tunnelCurrentPts != status.pts) {
                if ( !m_pBufferTracker->Remove(status.pts, &omxHeader) )
                {
                    ALOGI("Unable to find tracker entry for pts %#x", status.pts);
                    BOMX_PtsToTick(status.pts, &omxHeader.nTimeStamp);
                }
                m_tunnelCurrentPts = status.pts;
                reportRenderedFrame = true;
            } else {
                ALOGV("%s: status.pts:%u, m_tunnelCurrentPts:%u", __FUNCTION__, status.pts, m_tunnelCurrentPts);
            }

            if (reportRenderedFrame) {
                OMX_VIDEO_RENDEREVENTTYPE renderEvent;
                renderEvent.nSystemTimeNs = now;
                renderEvent.nMediaTimeUs = omxHeader.nTimeStamp;
                (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate, OMX_EventOutputRendered, 1, 0, &renderEvent);
                ALOGV("Rendering ts=%lld pts=%u now=%" PRIu64 "",
                        omxHeader.nTimeStamp, status.pts, now);

                EstimateFrameRate(omxHeader.nTimeStamp);

                // Use this event to return input buffers
                ReturnInputBuffers(omxHeader.nTimeStamp, InputReturnMode_eTimestamp);
            } else if ((m_waitingForStc || m_stcResumePending) && (m_AvailInputBuffers == 0)) {
                // when we're in the process of dropping frames to reach the start pts, return as many
                // input buffers as possible to speed up the task
                ReturnInputBuffers(0, InputReturnMode_eAll);
            }

            if (!m_waitingForStc && !m_stcResumePending && (m_stcSyncValue == B_STC_SYNC_INVALID_VALUE)) {
                NEXUS_SimpleStcChannel_GetStc(m_tunnelStcChannelSync, &stcSync);
                m_stcSyncValue = stcSync;
                ALOGV("%s: initializing stcSync:%u",  __FUNCTION__, stcSync);
            }
        }

        return;
    }

    // There should be at least one free buffer in the list or there is no point checking for more from nexus
    if ( NULL != BLST_Q_FIRST(&m_frameBufferFreeList) )
    {
        // Get as many frames as possible from nexus
        errCode = NEXUS_SimpleVideoDecoder_GetDecodedFrames(m_hSimpleVideoDecoder, frameStatus, B_MAX_DECODED_FRAMES, &numFrames);
        ALOGV("GetDecodedFrames: comp:%s rc=%u numFrames=%u", GetName(), errCode, numFrames);
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
                    ALOGW("Frame Mismatch - expecting %u got %u (state %u)", pFrameStatus->serialNumber, pBuffer->frameStatus.serialNumber, pBuffer->state);
                    ALOG_ASSERT(pBuffer->frameStatus.serialNumber  == pFrameStatus->serialNumber);
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
            //ALOGV("Skip buffer %u State %u", pBuffer->frameStatus.serialNumber, pBuffer->state);
        }
        // Loop through remaining buffers
        while ( NULL != pBuffer && !m_eosDelivered &&
                (m_formatChangeState == FCState_eNone || m_formatChangeState == FCState_eProcessCallback) )
        {
            BOMX_Buffer *pOmxBuffer;
            pOmxBuffer = m_pVideoPorts[1]->GetBuffer();
            if ( NULL != pOmxBuffer )
            {
                BOMX_VideoDecoderOutputBufferInfo *pInfo;
                OMX_BUFFERHEADERTYPE *pHeader = pOmxBuffer->GetHeader();
                bool prevEosPending = m_eosPending;
                bool prevEosDelivered = m_eosDelivered;
                bool destripedSuccess = true;
                pHeader->nOffset = 0;
                if ( !m_pBufferTracker->Remove(pBuffer->frameStatus.pts, pHeader) )
                {
                    ALOGW("Unable to find tracker entry for pts %#x", pBuffer->frameStatus.pts);
                    pHeader->nFlags = 0;
                    BOMX_PtsToTick(pBuffer->frameStatus.pts, &pHeader->nTimeStamp);
                }
                if ( (pHeader->nFlags & OMX_BUFFERFLAG_EOS) && !m_pBufferTracker->Last(pHeader->nTimeStamp) )
                {
                    // Defer EOS until the last output frame (with the latest timestamp) is returned
                    m_eosReceived = true;
                    pHeader->nFlags &= ~OMX_BUFFERFLAG_EOS;
                }
                else if ( m_eosReceived && m_pBufferTracker->Last(pHeader->nTimeStamp) )
                {
                    m_eosReceived = false;
                    pHeader->nFlags |= OMX_BUFFERFLAG_EOS;
                }

                pInfo = (BOMX_VideoDecoderOutputBufferInfo *)pOmxBuffer->GetComponentPrivate();
                ALOG_ASSERT(NULL != pInfo && NULL == pInfo->pFrameBuffer);
                if ( pBuffer->frameStatus.lastPicture || (pHeader->nFlags & OMX_BUFFERFLAG_EOS) )
                {
                    ALOGV("EOS picture received");
                    if ( m_eosPending )
                    {
                        pHeader->nFlags |= OMX_BUFFERFLAG_EOS;
                        m_eosPending = false;
                        m_eosDelivered = true;
                    }
                    else
                    {
                        // Fatal error - we did something wrong.
                        ALOGW("Additional frames received after EOS");
                        ALOG_ASSERT(true == m_eosPending);
                        return;
                    }
                }

                // If this is a true EOS dummy picture from the decoder, return a 0-length frame.
                // Otherwise there is a valid picture here and we need to handle the frame below as we would any other.
                if ( pBuffer->frameStatus.lastPicture )
                {
                    ALOGV("EOS-only frame.  Returning length of 0.");
                    pHeader->nFilledLen = 0;
                    // For metadata mode, the private handle still needs to be set since it's required to correlate
                    // omx buffers to framebuffers in function FillThisBuffer. Without this, EOS messages (zero length) could
                    // end up permanently in the allocated list if the application is playing video in a loop.
                    if (pInfo->type == BOMX_VideoDecoderOutputBufferType_eMetadata){
                        pBuffer->pPrivateHandle = (private_handle_t *)pInfo->typeInfo.metadata.pMetadata->pHandle;
                    }
                }
                else
                {
                    // Check for format change
                    if ( pBuffer->frameStatus.surfaceCreateSettings.imageWidth != m_outputWidth || pBuffer->frameStatus.surfaceCreateSettings.imageHeight != m_outputHeight )
                    {
                        OMX_PARAM_PORTDEFINITIONTYPE portDefs;
                        bool portReset;

                        if (m_formatChangeState == FCState_eNone)
                        {
                            ALOGI("Video output format change %ux%u -> %lux%lu [max %ux%u] on frame %u", m_outputWidth, m_outputHeight,
                                pBuffer->frameStatus.surfaceCreateSettings.imageWidth, pBuffer->frameStatus.surfaceCreateSettings.imageHeight,
                                m_pVideoPorts[1]->GetDefinition()->format.video.nFrameWidth, m_pVideoPorts[1]->GetDefinition()->format.video.nFrameHeight,
                                pBuffer->frameStatus.serialNumber);
                        }

                        if ( m_outputMode == BOMX_VideoDecoderOutputBufferType_eMetadata || m_adaptivePlaybackEnabled )
                        {
                            // For metadata output, treat port width/height as max values.  Crop can never be > port values.
                            portReset = (pBuffer->frameStatus.surfaceCreateSettings.imageWidth > m_pVideoPorts[1]->GetDefinition()->format.video.nFrameWidth ||
                                         pBuffer->frameStatus.surfaceCreateSettings.imageHeight > m_pVideoPorts[1]->GetDefinition()->format.video.nFrameHeight);
                        }
                        else
                        {
                            // Not metadata or adaptive, port must reset.
                            portReset = true;
                        }

                        if ( portReset )
                        {
                            // Push this entry back into the PTS tracker and reset any changes to EOS state
                            uint32_t temp;
                            m_pBufferTracker->Add(pHeader, &temp);
                            m_eosPending = prevEosPending;
                            m_eosDelivered = prevEosDelivered;

                            // Send port format change and stop processing frames until reset
                            if ((m_formatChangeState == FCState_eNone) && (m_lastReturnedSerial > 0)
                                    && (pBuffer->frameStatus.serialNumber > (m_lastReturnedSerial + 1)))
                            {
                                ALOGI("Output port will reset");
                                m_formatChangeState = FCState_eWaitForSerial;
                                m_formatChangeSerial = pBuffer->frameStatus.serialNumber;
                                CancelTimerId(m_formatChangeTimerId);
                                m_formatChangeTimerId = StartTimer(B_WAIT_FOR_FORMAT_CHANGE_TIMEOUT_MS, BOMX_VideoDecoder_FormatChangeTimer, static_cast<void *>(this));
                                ALOGV("Defer port format change, formatChangeSerial:%u, lastReturnedSerial:%u",
                                        m_formatChangeSerial, m_lastReturnedSerial);
                            } else {
                                ALOGV("Process port format change, formatChangeSerial:%u, lastReturnedSerial:%u",
                                        m_formatChangeSerial, m_lastReturnedSerial);
                                m_pVideoPorts[1]->GetDefinition(&portDefs);
                                portDefs.format.video.nFrameWidth = pBuffer->frameStatus.surfaceCreateSettings.imageWidth;
                                portDefs.format.video.nFrameHeight = pBuffer->frameStatus.surfaceCreateSettings.imageHeight;
                                portDefs.format.video.nStride = pBuffer->frameStatus.surfaceCreateSettings.imageWidth;
                                portDefs.format.video.nSliceHeight = pBuffer->frameStatus.surfaceCreateSettings.imageHeight;
                                portDefs.nBufferSize = ComputeBufferSize(portDefs.format.video.nStride, portDefs.format.video.nSliceHeight);
                                m_pVideoPorts[1]->SetDefinition(&portDefs);
                                m_outputWidth = portDefs.format.video.nFrameWidth;
                                m_outputHeight = portDefs.format.video.nFrameHeight;
                                PortFormatChanged(m_pVideoPorts[1]);
                                m_formatChangeState = FCState_eWaitForPortReconfig;
                            }
                            return;
                        }
                        else
                        {
                            // Only update crop and last output format if we're not resetting.  It will reset automatically
                            // on restart, and because ACodec queries the port format first and then crop to check crop <= port
                            // format you can get a race condition assertion if you update both here.  ACodec queries old port
                            // format, we update here, then they query new crop and fail erroneously.
                            m_outputWidth = pBuffer->frameStatus.surfaceCreateSettings.imageWidth;
                            m_outputHeight = pBuffer->frameStatus.surfaceCreateSettings.imageHeight;

                            // Send crop update and continue without reset
                            OMX_CONFIG_RECTTYPE cropRect;
                            BOMX_STRUCT_INIT(&cropRect);
                            cropRect.nLeft = 0;
                            cropRect.nTop = 0;
                            cropRect.nWidth = m_outputWidth;
                            cropRect.nHeight = m_outputHeight;
                            if ( m_callbacks.EventHandler )
                            {
                                (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate, OMX_EventPortSettingsChanged, m_videoPortBase+1, OMX_IndexConfigCommonOutputCrop, &cropRect);
                            }
                        }
                    }

                    // Setting private handle for meta mode ahead of time
                    if (pInfo->type == BOMX_VideoDecoderOutputBufferType_eMetadata)
                    {
                        pBuffer->pPrivateHandle = (private_handle_t *)pInfo->typeInfo.metadata.pMetadata->pHandle;
                    }

                    // Don't try to create a striped surface for secure video
                    if ( !m_secureDecoder && !m_secureRuntimeHeaps &&
                         ( pInfo->type != BOMX_VideoDecoderOutputBufferType_eMetadata ||
                           ((pBuffer->pPrivateHandle->fmt_set & GR_HWTEX) == GR_HWTEX) ) )
                    {
                        pBuffer->hStripedSurface = NEXUS_StripedSurface_Create(&(pBuffer->frameStatus.surfaceCreateSettings));
                        if ( NULL == pBuffer->hStripedSurface )
                        {
                            (void)BOMX_BERR_TRACE(BERR_UNKNOWN);
                        }
                    }

                    switch ( pInfo->type )
                    {
                    case BOMX_VideoDecoderOutputBufferType_eStandard:
                        {
                            NEXUS_Error errCode;
                            NEXUS_Graphics2DDestripeBlitSettings destripeSettings;
                            NEXUS_Graphics2D_GetDefaultDestripeBlitSettings(&destripeSettings);
                            // Turn off any filtering to perserve the original decoded content as-is
                            destripeSettings.horizontalFilter = NEXUS_Graphics2DFilterCoeffs_ePointSample;
                            destripeSettings.verticalFilter = NEXUS_Graphics2DFilterCoeffs_ePointSample;
                            destripeSettings.chromaFilter = false;  // The data will be internally upconverted to 4:4:4 but we convert back to 4:2:0 so don't filter and just repeat samples
                            destripeSettings.source.stripedSurface = pBuffer->hStripedSurface;
                            if ( pBuffer->hStripedSurface )
                            {
#if BOMX_VIDEO_DECODER_DESTRIPE_PLANAR
                                NEXUS_Surface_Flush(pInfo->typeInfo.standard.hSurfaceY);
                                destripeSettings.output.surface = pInfo->typeInfo.standard.hSurfaceY;
                                errCode = NEXUS_Graphics2D_DestripeBlit(m_hGraphics2d, &destripeSettings);
                                if ( errCode )
                                {
                                    (void)BOMX_BERR_TRACE(errCode);
                                }
                                NEXUS_Surface_Flush(pInfo->typeInfo.standard.hSurfaceCb);
                                destripeSettings.output.surface = pInfo->typeInfo.standard.hSurfaceCb;
                                errCode = NEXUS_Graphics2D_DestripeBlit(m_hGraphics2d, &destripeSettings);
                                if ( errCode )
                                {
                                    (void)BOMX_BERR_TRACE(errCode);
                                }
                                NEXUS_Surface_Flush(pInfo->typeInfo.standard.hSurfaceCr);
                                destripeSettings.output.surface = pInfo->typeInfo.standard.hSurfaceCr;
                                errCode = NEXUS_Graphics2D_DestripeBlit(m_hGraphics2d, &destripeSettings);
                                if ( errCode )
                                {
                                    (void)BOMX_BERR_TRACE(errCode);
                                }
#else
                                NEXUS_Surface_Flush(pInfo->typeInfo.standard.hDestripeSurface);
                                destripeSettings.output.surface = pInfo->typeInfo.standard.hDestripeSurface;
                                errCode = NEXUS_Graphics2D_DestripeBlit(m_hGraphics2d, &destripeSettings);
                                if ( errCode )
                                {
                                    (void)BOMX_BERR_TRACE(errCode);
                                    destripedSuccess = false;
                                }
#endif
                            }
                            pHeader->nFilledLen = ComputeBufferSize(m_pVideoPorts[1]->GetDefinition()->format.video.nStride, m_pVideoPorts[1]->GetDefinition()->format.video.nSliceHeight);
                            // Wait for the blit to complete before delivering in case CPU will access it quickly
                            if ( destripedSuccess )
                            {
                                Unlock();
                                if ( GraphicsCheckpoint() )
                                {
#if BOMX_VIDEO_DECODER_DESTRIPE_PLANAR
                                    NEXUS_Surface_Flush(pInfo->typeInfo.standard.hSurfaceY);
                                    NEXUS_Surface_Flush(pInfo->typeInfo.standard.hSurfaceCb);
                                    NEXUS_Surface_Flush(pInfo->typeInfo.standard.hSurfaceCr);
#else
                                    NEXUS_Surface_Flush(pInfo->typeInfo.standard.hDestripeSurface);
#endif
                                    if ( pInfo->typeInfo.standard.pClientMemory )
                                    {
                                        CopySurfaceToClient(pInfo);
                                    }
                                }
                                else
                                {
                                    ALOGE("Error destriping to buffer");
                                    destripedSuccess = false;
                                }
                                Lock();
                            }
                        }
                        break;
                    case BOMX_VideoDecoderOutputBufferType_eNative:
                        {
                            int rc;
                            pHeader->nFilledLen = ComputeBufferSize(m_pVideoPorts[1]->GetDefinition()->format.video.nStride, m_pVideoPorts[1]->GetDefinition()->format.video.nSliceHeight);
                            pInfo->typeInfo.native.pSharedData->videoFrame.status = pBuffer->frameStatus;
                            pBuffer->pPrivateHandle = pInfo->typeInfo.native.pPrivateHandle;
                            rc = private_handle_t::lock_video_frame(pBuffer->pPrivateHandle, 100);
                            if ( 0 == rc )
                            {
                                pInfo->typeInfo.native.pSharedData->videoFrame.hStripedSurface = pBuffer->hStripedSurface;
                                pInfo->typeInfo.native.pSharedData->videoFrame.destripeComplete = 0;
                                private_handle_t::unlock_video_frame(pBuffer->pPrivateHandle);
                            }
                            else
                            {
                                ALOGW("Timeout locking video frame");
                            }
                        }
                        break;
                    case BOMX_VideoDecoderOutputBufferType_eMetadata:
                        {
                            void *pMemory;
                            PSHARED_DATA pSharedData;
                            BOMX_VideoDecoder_MemLock(pBuffer->pPrivateHandle, &pMemory);
                            if ( NULL == pMemory )
                            {
                                ALOGW("Unable to convert SHARED_DATA physical address %p", pBuffer->pPrivateHandle);
                                (void)BOMX_ERR_TRACE(OMX_ErrorBadParameter);
                            }
                            else
                            {
                                pSharedData = (PSHARED_DATA)pMemory;

                                // Setup window parameters for display and provide buffer status
                                pSharedData->videoWindow.nexusClientContext = m_nexusClient;
                                android_atomic_release_store(1, &pSharedData->videoWindow.windowIdPlusOne);
                                pSharedData->videoFrame.status = pBuffer->frameStatus;
                                // Don't allow gralloc_lock to destripe in metadata mode.  We won't know when it's safe to destroy the striped surface.
                                pSharedData->videoFrame.hStripedSurface = NULL;

                                if ( pBuffer->hStripedSurface )
                                {
                                    int err = private_handle_t::lock_video_frame(pBuffer->pPrivateHandle, 100);
                                    if ( err )
                                    {
                                        ALOGW("Timeout locking video frame in metadata mode - %d", err);
                                    }
                                    else
                                    {
                                        Unlock();
                                        OMX_ERRORTYPE res = DestripeToYV12(pSharedData, pBuffer->hStripedSurface);
                                        Lock();
                                        if ( res != OMX_ErrorNone )
                                        {
                                            ALOGE("Unable to destripe to YV12 - %d", res);
                                            (void)BOMX_ERR_TRACE(res);
                                            destripedSuccess = false;
                                            pSharedData->videoFrame.destripeComplete = true;
                                        }

                                        private_handle_t::unlock_video_frame(pBuffer->pPrivateHandle);
                                    }
                                }
                            }
                            pHeader->nFilledLen = sizeof(VideoDecoderOutputMetaData);
                            BOMX_VideoDecoder_MemUnlock(pBuffer->pPrivateHandle);
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
                ALOGV("Returning Port Buffer ts %lld us serial %u pInfo %p FB %p HDR %p flags %#x", pHeader->nTimeStamp, pBuffer->frameStatus.serialNumber, pInfo, pInfo->pFrameBuffer, pHeader, pHeader->nFlags);

                EstimateFrameRate(pHeader->nTimeStamp);
                {
                    unsigned queueDepthBefore = m_pVideoPorts[1]->QueueDepth();
                    BOMX_VIDEO_STATS_ADD_EVENT(BOMX_VD_Stats::OUTPUT_FRAME, pHeader->nTimeStamp, pBuffer->frameStatus.serialNumber);
                    ReturnPortBuffer(m_pVideoPorts[1], pOmxBuffer);
                    // Try to return processed input buffers
                    ReturnInputBuffers(pHeader->nTimeStamp, InputReturnMode_eTimestamp);
                    ALOG_ASSERT(queueDepthBefore == (m_pVideoPorts[1]->QueueDepth()+1));
                }
                if ( destripedSuccess )
                {
                    // Advance to next frame
                    pBuffer = BLST_Q_NEXT(pBuffer, node);
                }
                else
                {
                    // Bail out early to let the decoded frame to be returned and try destriping
                    // the rest of the frames again on the next timer
                    pBuffer = NULL;
                }
            }
            else
            {
                break;
            }
        }
    }
}

void BOMX_VideoDecoder::ReturnDecodedFrames()
{
    NEXUS_VideoDecoderReturnFrameSettings returnSettings[B_MAX_DECODED_FRAMES];
    unsigned numFrames=0;
    BOMX_VideoDecoderFrameBuffer *pBuffer, *pStart, *pEnd, *pLast;
    NEXUS_VideoDecoderStatus status;

    // Make sure decoder is available and running
    if ( NULL == m_hSimpleVideoDecoder )
    {
        return;
    }

    if (m_tunnelMode)
    {
       // in tunnel mode, there is no decoded frame processing out of the component, so
       // we can skip this.
       return;
    }

    // Skip pending invalidated frames
    for ( pBuffer = BLST_Q_FIRST(&m_frameBufferAllocList);
          NULL != pBuffer && pBuffer->state == BOMX_VideoDecoderFrameBufferState_eInvalid;
          pBuffer = BLST_Q_NEXT(pBuffer, node) );

    // If we scanned the entire list or the first frame is not yet delivered just bail out
    if ( NULL == pBuffer || (!m_outputFlushing && pBuffer->state == BOMX_VideoDecoderFrameBufferState_eReady ) )
    {
        return;
    }

    // Find last actionable frame if there is one
    if ( m_outputFlushing )
    {
        for ( pStart = pBuffer, pEnd = NULL;
              NULL != pBuffer;
              pBuffer = BLST_Q_NEXT(pBuffer, node) )
        {
            if ( pBuffer->state != BOMX_VideoDecoderFrameBufferState_eInvalid )
            {
                pEnd = pBuffer;
            }
        }
    }
    else
    {
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
    }

    // Deal with the frames we found
    if ( NULL != pEnd )
    {
        // Do what was requested
        pLast = pEnd;
        pEnd = BLST_Q_NEXT(pEnd, node);
        pBuffer = pStart;
        while ( pBuffer != pEnd && numFrames < B_MAX_DECODED_FRAMES )
        {
            BOMX_VideoDecoderFrameBuffer *pNext = BLST_Q_NEXT(pBuffer, node);
            //NEXUS_VideoDecoder_GetDefaultReturnFrameSettings(&returnSettings[numFrames]); Intentionally skipped - there is only one field to set anyway
            if ( pBuffer->state == BOMX_VideoDecoderFrameBufferState_eDelivered )
            {
                if ( m_outputFlushing )
                {
                    ALOGW("Dropping outstanding frame %u still owned by client - flushing", pBuffer->frameStatus.serialNumber);
                }
                else
                {
                    ALOGW("Dropping outstanding frame %u still owned by client - a later frame (%u) was returned already", pBuffer->frameStatus.serialNumber, pLast->frameStatus.serialNumber);
                }
                pBuffer->state = BOMX_VideoDecoderFrameBufferState_eInvalid;
                returnSettings[numFrames].display = false;
            }
            else
            {
                PSHARED_DATA pSharedData = NULL;
                void *pMemory;
                // Display only the last frame we're returning if the app is falling behind.
                if ( pNext == pEnd )
                {
                    returnSettings[numFrames].display = (pBuffer->state == BOMX_VideoDecoderFrameBufferState_eDisplayReady) ? true : false;
                    if (!returnSettings[numFrames].display)
                    {
                        if ( m_outputFlushing )
                        {
                            ALOGW("Dropping outstanding frame %u - flushing", pBuffer->frameStatus.serialNumber);
                        }
                        else
                        {
                            ALOGW("Dropping outstanding frame %u - state is [%d] %s", pBuffer->frameStatus.serialNumber, pBuffer->state,
                                  pBuffer->state == BOMX_VideoDecoderFrameBufferState_eDropReady ? "eDropReady" : "???");
                        }
                    }
                }
                else
                {
                    returnSettings[numFrames].display = false;
                    if ( m_outputFlushing )
                    {
                        ALOGW("Dropping outstanding frame %u - flushing", pBuffer->frameStatus.serialNumber);
                    }
                    else
                    {
                        ALOGW("Dropping outstanding frame %u - falling behind", pBuffer->frameStatus.serialNumber);
                    }
                }
                if ( m_outputMode != BOMX_VideoDecoderOutputBufferType_eMetadata && pBuffer->pPrivateHandle )
                {
                    BOMX_VideoDecoder_MemLock(pBuffer->pPrivateHandle, &pMemory);
                    if ( pMemory )
                    {
                        pSharedData = (PSHARED_DATA)pMemory;
                    }
                    if ( pSharedData && pSharedData->videoFrame.hStripedSurface )
                    {
                        NEXUS_StripedSurfaceHandle hStripedSurface;
                        int rc;

                        // Lock the video frame to try and avoid problems with gralloc still using this handle.
                        // If we timeout, we still have to return the frame to nexus so too bad...

                        rc = private_handle_t::lock_video_frame(pBuffer->pPrivateHandle, 250);
                        hStripedSurface = pSharedData->videoFrame.hStripedSurface;
                        pSharedData->videoFrame.hStripedSurface = NULL;
                        if ( 0 == rc )
                        {
                            private_handle_t::unlock_video_frame(pBuffer->pPrivateHandle);
                        }
                    }
                    BOMX_VideoDecoder_MemUnlock(pBuffer->pPrivateHandle);
                }
                pBuffer->pPrivateHandle = NULL;
                pBuffer->pBufferInfo = NULL;
                BLST_Q_REMOVE(&m_frameBufferAllocList, pBuffer, node);
                BOMX_VideoDecoder_StripedSurfaceDestroy(pBuffer);
                BLST_Q_INSERT_TAIL(&m_frameBufferFreeList, pBuffer, node);
            }
            BOMX_VIDEO_STATS_ADD_EVENT(BOMX_VD_Stats::DISPLAY_FRAME, 0, returnSettings[numFrames].display ? 1: 0, pBuffer->frameStatus.serialNumber);

            numFrames++;
            pBuffer = pNext;
        }

        if ( numFrames > 0 )
        {
            int currentPlayTime = 0;
            if (m_startTime != -1) {
                currentPlayTime = toMillisecondTimeoutDelay(m_startTime, systemTime(CLOCK_MONOTONIC));
            }
            for (uint32_t j=0; j < numFrames; ++j) {
                if (!returnSettings[j].display) {
                    ++m_droppedFrames;
                    if (m_startTime != -1) {
                        if (currentPlayTime < m_earlyDropThresholdMs) {
                            ++m_earlyDroppedFrames;
                        } else {
                            m_startTime = -1; // Stop tracking once past the mark
                        }
                    }
                    ++m_consecDroppedFrames;
                    if (m_consecDroppedFrames > m_maxConsecDroppedFrames)
                        m_maxConsecDroppedFrames = m_consecDroppedFrames;
                } else {
                    m_consecDroppedFrames = 0;
                }
            }

            ALOGV("Returning %u frames to nexus last=%u - %s", numFrames, pLast->frameStatus.serialNumber, returnSettings[numFrames-1].display?"display":"drop");
            NEXUS_Error errCode = NEXUS_SimpleVideoDecoder_ReturnDecodedFrames(m_hSimpleVideoDecoder, returnSettings, numFrames);
            if ( errCode )
            {
                BOMX_BERR_TRACE(errCode);
                // Not much else we can do
            }

            m_lastReturnedSerial = pLast->frameStatus.serialNumber;
            if ((m_formatChangeState == FCState_eWaitForSerial) && (m_formatChangeSerial == (m_lastReturnedSerial + 1))) {
                ALOGV("%s: processing format change, m_formatChangeSerial:%u", __FUNCTION__, m_formatChangeSerial);
                CancelTimerId(m_formatChangeTimerId);
                m_formatChangeState = FCState_eProcessCallback;
                OutputFrameEvent();
            }
        }
    }

    // Do not try to check for more frames here, that will be done by the component thread based on the event or timer
}

bool BOMX_VideoDecoder::GraphicsCheckpoint()
{
    NEXUS_Error errCode;
    bool ret = true;

    errCode = NEXUS_Graphics2D_Checkpoint(m_hGraphics2d, NULL);
    if ( errCode == NEXUS_GRAPHICS2D_QUEUED )
    {
        errCode = B_Event_Wait(m_hCheckpointEvent, B_CHECKPOINT_TIMEOUT);
        if ( errCode )
        {
            ALOGW("!!! ERROR: Timeout waiting for graphics checkpoint !!!");
            ret = false;
        }
    }

    return ret;
}

void BOMX_VideoDecoder::CopySurfaceToClient(const BOMX_VideoDecoderOutputBufferInfo *pInfo)
{
    if ( pInfo &&
         pInfo->type == BOMX_VideoDecoderOutputBufferType_eStandard &&
         pInfo->typeInfo.standard.pClientMemory )
    {
#if BOMX_VIDEO_DECODER_DESTRIPE_PLANAR
        size_t numBytes;
        NEXUS_Error errCode;
        NEXUS_SurfaceStatus surfaceStatus;
        uint8_t *pClientMemory;
        void *pSurfaceMemory;
        size_t cbOffset = m_pVideoPorts[1]->GetDefinition()->format.video.nSliceHeight * m_pVideoPorts[1]->GetDefinition()->format.video.nStride;

        // Luma Plane
        pClientMemory = (uint8_t *)pInfo->typeInfo.standard.pClientMemory;
        NEXUS_Surface_GetStatus(pInfo->typeInfo.standard.hSurfaceY, &surfaceStatus);
        numBytes = surfaceStatus.height * surfaceStatus.pitch;
        errCode = NEXUS_Surface_Lock(pInfo->typeInfo.standard.hSurfaceY, &pSurfaceMemory);
        if ( errCode )
        {
            // Fill with black
            BKNI_Memset(pClientMemory, 0, numBytes);
        }
        else
        {
            BKNI_Memcpy(pClientMemory, pSurfaceMemory, numBytes);
            NEXUS_Surface_Unlock(pInfo->typeInfo.standard.hSurfaceY);
        }
        // Cb Plane
        pClientMemory += cbOffset;
        NEXUS_Surface_GetStatus(pInfo->typeInfo.standard.hSurfaceCb, &surfaceStatus);
        numBytes = surfaceStatus.height * surfaceStatus.pitch;
        errCode = NEXUS_Surface_Lock(pInfo->typeInfo.standard.hSurfaceCb, &pSurfaceMemory);
        if ( errCode )
        {
            // Fill with black
            BKNI_Memset(pClientMemory, 0x80, numBytes);
        }
        else
        {
            BKNI_Memcpy(pClientMemory, pSurfaceMemory, numBytes);
            NEXUS_Surface_Unlock(pInfo->typeInfo.standard.hSurfaceCb);
        }
        // Cr Plane
        pClientMemory += cbOffset/4;    // 4:2:0 Cb plane size is 1/4 Y plane size
        NEXUS_Surface_GetStatus(pInfo->typeInfo.standard.hSurfaceCr, &surfaceStatus);
        numBytes = surfaceStatus.height * surfaceStatus.pitch;
        errCode = NEXUS_Surface_Lock(pInfo->typeInfo.standard.hSurfaceCr, &pSurfaceMemory);
        if ( errCode )
        {
            // Fill with black
            BKNI_Memset(pClientMemory, 0x80, numBytes);
        }
        else
        {
            BKNI_Memcpy(pClientMemory, pSurfaceMemory, numBytes);
            NEXUS_Surface_Unlock(pInfo->typeInfo.standard.hSurfaceCr);
        }
#else
        /* For now, we destripe to 422 and convert to 420 with the CPU.  This is less memory efficent and more complex than 420, but planar
           destriping does not seem to work properly */
        unsigned x, y;
        uint8_t *pClientY, *pClientCb, *pClientCr;
        uint8_t *pPackedData;
        void *pSurfaceMemory;
        NEXUS_Error errCode;
        pClientY = (uint8_t *)pInfo->typeInfo.standard.pClientMemory;
        pClientCb = pClientY + (m_pVideoPorts[1]->GetDefinition()->format.video.nSliceHeight * m_pVideoPorts[1]->GetDefinition()->format.video.nStride);
        pClientCr = pClientCb + ((m_pVideoPorts[1]->GetDefinition()->format.video.nSliceHeight * m_pVideoPorts[1]->GetDefinition()->format.video.nStride)/4);
        errCode = NEXUS_Surface_Lock(pInfo->typeInfo.standard.hDestripeSurface, &pSurfaceMemory);
        if ( NEXUS_SUCCESS == errCode )
        {
            pPackedData = (uint8_t *)pSurfaceMemory;
            // Convert 422 YCbCr to 420 Planar
            for ( y = 0; y < m_pVideoPorts[1]->GetDefinition()->format.video.nFrameHeight; ++y )
            {
                for ( x = 0; x < m_pVideoPorts[1]->GetDefinition()->format.video.nFrameWidth; x+=2 )
                {
                    uint8_t y0, cb, y1, cr;
                    y0 = *pPackedData++;
                    cb = *pPackedData++;
                    y1 = *pPackedData++;
                    cr = *pPackedData++;

                    *pClientY++ = y0;
                    *pClientY++ = y1;

                    if ( (y & 1) == 0 )
                    {
                        *pClientCb++ = cb;
                        *pClientCr++ = cr;
                    }
                }
            }
            NEXUS_Surface_Unlock(pInfo->typeInfo.standard.hDestripeSurface);
        }
        else
        {
            BOMX_BERR_TRACE(errCode);
        }
#endif
    }
}

OMX_ERRORTYPE BOMX_VideoDecoder::ConfigBufferInit()
{
    NEXUS_Error errCode;

    if (m_pConfigBuffer == NULL)
    {
        errCode = AllocateConfigBuffer(BOMX_VIDEO_CODEC_CONFIG_BUFFER_SIZE, m_pConfigBuffer);
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

OMX_ERRORTYPE BOMX_VideoDecoder::ConfigBufferAppend(const void *pBuffer, size_t length)
{
    ALOG_ASSERT(NULL != pBuffer);
    if ( m_configBufferSize + length > BOMX_VIDEO_CODEC_CONFIG_BUFFER_SIZE )
    {
        ALOGE("Config buffer not big enough! Size: %zu", m_configBufferSize + length);
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
    ALOG_ASSERT(NULL != pPrivateHandle);

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

void BOMX_VideoDecoder::RunDisplayThread()
{
    BERR_Code rc;
    int priority;

    if ( getSchedPriority(priority) )
    {
        setpriority(PRIO_PROCESS, 0, priority + 2*PRIORITY_MORE_FAVORABLE);
        set_sched_policy(0, SP_FOREGROUND);
    }

    for (;;)
    {
        rc = BKNI_WaitForEvent(m_hDisplayFrameEvent, BKNI_INFINITE);
        if ( m_displayThreadStop )
        {
            break;
        }

        if ( CC_LIKELY(rc == BERR_SUCCESS) )
        {
            DisplayFrameEvent();
        }
        else
        {
            ALOGW("Display frame event failure (%u)", rc);
            (void)BOMX_BERR_TRACE(rc);
        }
    }
}

void BOMX_VideoDecoder::BinderNotifyDisplay(struct hwc_notification_info &ntfy)
{
    B_Mutex_Lock(m_hDisplayMutex);
    if (m_displayFrameAvailable) {
        ALOGW("%s: New frame (%u) arrived but previous frame (%u) hasn't been displayed yet!",
                __FUNCTION__, (unsigned)ntfy.frame_id, m_frameSerial);
    }
    m_displayFrameAvailable = true;
    m_framePosition.x = ntfy.frame.x;
    m_framePosition.y = ntfy.frame.y;
    m_framePosition.width = ntfy.frame.w;
    m_framePosition.height = ntfy.frame.h;
    m_frameClip.x = ntfy.clipped.x;
    m_frameClip.y = ntfy.clipped.y;
    m_frameClip.width = ntfy.clipped.w;
    m_frameClip.height = ntfy.clipped.h;
    m_frameSerial = (unsigned)ntfy.frame_id;
    m_frameWidth = ntfy.display_width;
    m_frameHeight = ntfy.display_height;
    m_framezOrder = ntfy.zorder;
    B_Mutex_Unlock(m_hDisplayMutex);
    BKNI_SetEvent(m_hDisplayFrameEvent);
}

void BOMX_VideoDecoder::SetVideoGeometry(NEXUS_Rect *pPosition, NEXUS_Rect *pClipRect, unsigned serialNumber, unsigned gfxWidth, unsigned gfxHeight, unsigned zorder, bool visible)
{
    Lock();
    SetVideoGeometry_locked(pPosition, pClipRect, serialNumber, gfxWidth, gfxHeight, zorder, visible);
    Unlock();
}

void BOMX_VideoDecoder::DisplayFrame_locked(unsigned serialNumber)
{
    BOMX_VideoDecoderFrameBuffer *pFrameBuffer;

    ALOGV("DisplayFrame(%d)", serialNumber);

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

#define NEXUS_RECT_IS_EQUAL(r1,r2) (((r1)->x == (r2)->x) && ((r1)->y == (r2)->y) && ((r1)->width == (r2)->width) && ((r1)->height == (r2)->height))

void BOMX_VideoDecoder::SetVideoGeometry_locked(NEXUS_Rect *pPosition, NEXUS_Rect *pClipRect, unsigned serialNumber, unsigned gfxWidth, unsigned gfxHeight, unsigned zorder, bool visible)
{
    if ( m_hSurfaceClient )
    {
        NEXUS_SurfaceComposition composition;
        NEXUS_SurfaceClientSettings settings;
        NEXUS_Error errCode;

        NxClient_GetSurfaceClientComposition(m_allocResults.surfaceClient[0].id, &composition);
        if ( composition.virtualDisplay.width != gfxWidth ||
             composition.virtualDisplay.height != gfxHeight ||
             !NEXUS_RECT_IS_EQUAL(&composition.position, pPosition) ||
             composition.zorder != zorder ||
             composition.visible != visible ||
             composition.contentMode != NEXUS_VideoWindowContentMode_eFull )
        {
            composition.virtualDisplay.width = gfxWidth;
            composition.virtualDisplay.height = gfxHeight;
            composition.position = *pPosition;
            composition.zorder = zorder;
            composition.visible = visible;
            composition.contentMode = NEXUS_VideoWindowContentMode_eFull;
            ALOGI("Alpha hole position change to %ux%u @ %u,%u [fb %ux%u] zorder %u visible %u", pPosition->width, pPosition->height, pPosition->x, pPosition->y, gfxWidth, gfxHeight, zorder, (unsigned)visible);
            errCode = NxClient_SetSurfaceClientComposition(m_allocResults.surfaceClient[0].id, &composition);
            if ( errCode )
            {
                (void)BOMX_BERR_TRACE(errCode);
                return;
            }
        }

        BOMX_VideoDecoderFrameBuffer *pFrameBuffer = FindFrameBuffer(serialNumber);
        if ( pFrameBuffer )
        {
            NEXUS_Rect videoPosition;
            unsigned aspectOld, aspectNew;
            unsigned oldW, newW, oldH, newH;
            unsigned oldX, newX, oldY, newY;

            // Sanity check.  Video clipping can not be larger than the video surface (e.g. zoom out), but on format changes the display
            // requests can get out of sync briefly with the clip rectangle change relayed to HWC.
            if ( (pClipRect->x + pClipRect->width) > (int)pFrameBuffer->frameStatus.surfaceCreateSettings.imageWidth ||
                 (pClipRect->y + pClipRect->height) > (int)pFrameBuffer->frameStatus.surfaceCreateSettings.imageHeight )
            {
                ALOGW("Invalid clip region %ux%u @ %u,%u - Buffer is %lux%lu", pClipRect->width, pClipRect->height, pClipRect->x, pClipRect->y,
                    pFrameBuffer->frameStatus.surfaceCreateSettings.imageWidth, pFrameBuffer->frameStatus.surfaceCreateSettings.imageHeight);
            }
            else
            {
                // Adjusting the video window parameters can cause a black flash while the BVN reconfigures.
                // Because the hardware and nexus are intended to isolate applications from source format changes and
                // per-frame interaction, we configure clipping in relative values (virtualDisplay vs. clipRect).
                // So, to avoid black flashes we need to convert the physical values given from hwc into relative values
                // and then only update if the effective values are changing (e.g. aspect ratio changes or relative clipping changes).

                videoPosition.x = videoPosition.y = 0;
                videoPosition.width = pFrameBuffer->frameStatus.surfaceCreateSettings.imageWidth;
                videoPosition.height = pFrameBuffer->frameStatus.surfaceCreateSettings.imageHeight;
                NEXUS_SurfaceClient_GetSettings(m_hVideoClient, &settings);

                aspectOld = (4096*settings.composition.virtualDisplay.width) / settings.composition.virtualDisplay.height;
                aspectNew = (4096*videoPosition.width) / videoPosition.height;
                oldW = (4096*settings.composition.clipRect.width) / settings.composition.virtualDisplay.width;
                oldH = (4096*settings.composition.clipRect.height) / settings.composition.virtualDisplay.height;
                newW = (4096*pClipRect->width) / videoPosition.width;
                newH = (4096*pClipRect->height) / videoPosition.height;
                oldX = (4096*settings.composition.clipRect.x) / settings.composition.virtualDisplay.width;
                oldY = (4096*settings.composition.clipRect.y) / settings.composition.virtualDisplay.height;
                newX = (4096*pClipRect->x) / videoPosition.width;
                newY = (4096*pClipRect->y) / videoPosition.height;

                if ( aspectOld != aspectNew || oldW != newW || oldH != newH || oldX != newX || oldY != newY )
                {
                    settings.composition.virtualDisplay.width = videoPosition.width;
                    settings.composition.virtualDisplay.height = videoPosition.height;
                    settings.composition.position = videoPosition;
                    settings.composition.clipRect = *pClipRect;
                    settings.composition.contentMode = NEXUS_VideoWindowContentMode_eFull;
                    ALOGI("Video clipping change to %ux%u @ %u,%u [source %ux%u]", pClipRect->width, pClipRect->height, pClipRect->x, pClipRect->y, settings.composition.virtualDisplay.width, settings.composition.virtualDisplay.height);
                    errCode = NEXUS_SurfaceClient_SetSettings(m_hVideoClient, &settings);
                    if ( errCode )
                    {
                        (void)BOMX_BERR_TRACE(errCode);
                        return;
                    }
                }
            }
        }

        if ( !m_setSurface )
        {
            errCode = NEXUS_SurfaceClient_SetSurface(m_hSurfaceClient, m_hAlphaSurface);
            if ( errCode )
            {
                (void)BOMX_BERR_TRACE(errCode);
                return;
            }
            m_setSurface = true;
        }
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

NEXUS_Error BOMX_VideoDecoder::AllocateConfigBuffer(uint32_t nSize, void*& pBuffer)
{
    return AllocateInputBuffer(nSize, pBuffer);
}

void BOMX_VideoDecoder::FreeConfigBuffer(void*& pBuffer)
{
    FreeInputBuffer(pBuffer);
}

OMX_ERRORTYPE BOMX_VideoDecoder::DestripeToYV12(SHARED_DATA *pSharedData, NEXUS_StripedSurfaceHandle hStripedSurface)
{
   OMX_ERRORTYPE errCode = OMX_ErrorUndefined;
   NEXUS_SurfaceHandle hSurfaceY = NULL, hSurfaceCb = NULL, hSurfaceCr = NULL;
   NEXUS_StripedSurfaceCreateSettings surfaceSettings;
   NEXUS_Graphics2DDestripeBlitSettings destripeSettings;
   NEXUS_MemoryBlockHandle block_handle = NULL;
   void *slock;
   NEXUS_Error nxCode;
   int yv12_alignment = 16;

   if (m_hGraphics2d == NULL) {
      ALOGE("DestripeToYV12: no gfx2d.");
      errCode = OMX_ErrorInsufficientResources;
      goto err_gfx2d;
   }

   if (pSharedData == NULL) {
      ALOGE("DestripeToYV12: invalid buffer?");
      errCode = OMX_ErrorInsufficientResources;
      goto err_shared_data;
   }

   hSurfaceY = BOMX_VideoDecoder_ToNexusSurface(
                             pSharedData->container.width,
                             pSharedData->container.height,
                             pSharedData->container.stride,
                             NEXUS_PixelFormat_eY8,
                             pSharedData->container.block,
                             0);
   if (hSurfaceY == NULL) {
      ALOGE("DestripeToYV12: invalid plane Y");
      errCode = OMX_ErrorInsufficientResources;
      goto err_surfaces;
   }
   NEXUS_Surface_Lock(hSurfaceY, &slock);
   NEXUS_Surface_Flush(hSurfaceY);

   hSurfaceCr = BOMX_VideoDecoder_ToNexusSurface(
                              pSharedData->container.width/2,
                              pSharedData->container.height/2,
                              (pSharedData->container.stride/2 + (yv12_alignment-1)) & ~(yv12_alignment-1),
                              NEXUS_PixelFormat_eCr8,
                              pSharedData->container.block,
                              pSharedData->container.stride * pSharedData->container.height);
   if (hSurfaceCr == NULL) {
      ALOGE("DestripeToYV12: invalid plane Cr");
      errCode = OMX_ErrorInsufficientResources;
      goto err_surfaces;
   }
   NEXUS_Surface_Lock(hSurfaceCr, &slock);
   NEXUS_Surface_Flush(hSurfaceCr);

   hSurfaceCb = BOMX_VideoDecoder_ToNexusSurface(
                              pSharedData->container.width/2,
                              pSharedData->container.height/2,
                              (pSharedData->container.stride/2 + (yv12_alignment-1)) & ~(yv12_alignment-1),
                              NEXUS_PixelFormat_eCb8,
                              pSharedData->container.block,
                              (pSharedData->container.stride * pSharedData->container.height) +
                              ((pSharedData->container.height/2) * ((pSharedData->container.stride/2 + (yv12_alignment-1)) & ~(yv12_alignment-1))));
   if (hSurfaceCb == NULL) {
      ALOGE("DestripeToYV12: invalid plane Cb");
      errCode = OMX_ErrorInsufficientResources;
      goto err_surfaces;
   }
   NEXUS_Surface_Lock(hSurfaceCb, &slock);
   NEXUS_Surface_Flush(hSurfaceCb);

   NEXUS_Graphics2D_GetDefaultDestripeBlitSettings(&destripeSettings);
   // Turn off any filtering to perserve the original decoded content as-is
   destripeSettings.horizontalFilter = NEXUS_Graphics2DFilterCoeffs_ePointSample;
   destripeSettings.verticalFilter = NEXUS_Graphics2DFilterCoeffs_ePointSample;
   destripeSettings.chromaFilter = false;  // Maintain pixel accuracy as if it came directly from the decoder
   destripeSettings.source.stripedSurface = hStripedSurface;
   destripeSettings.output.surface = hSurfaceY;
   NEXUS_StripedSurface_GetCreateSettings(hStripedSurface, &surfaceSettings);
   destripeSettings.output.rect.x = 0;
   destripeSettings.output.rect.y = 0;
   destripeSettings.output.rect.width = surfaceSettings.imageWidth;
   destripeSettings.output.rect.height = surfaceSettings.imageHeight;

   nxCode = NEXUS_Graphics2D_DestripeBlit(m_hGraphics2d, &destripeSettings);
   if (nxCode) {
      ALOGE("DestripeToYV12: failed destripe to Y surface");
      errCode = OMX_ErrorUndefined;
      goto err_destripe;
   }
   destripeSettings.output.surface = hSurfaceCb;
   destripeSettings.output.rect.x = 0;
   destripeSettings.output.rect.y = 0;
   destripeSettings.output.rect.width = surfaceSettings.imageWidth/2;
   destripeSettings.output.rect.height = surfaceSettings.imageHeight/2;
   nxCode = NEXUS_Graphics2D_DestripeBlit(m_hGraphics2d, &destripeSettings);
   if (nxCode) {
      ALOGE("DestripeToYV12: failed destripe to Cb surface");
      errCode = OMX_ErrorUndefined;
      goto err_destripe;
   }
   destripeSettings.output.surface = hSurfaceCr;
   destripeSettings.output.rect.x = 0;
   destripeSettings.output.rect.y = 0;
   destripeSettings.output.rect.width = surfaceSettings.imageWidth/2;
   destripeSettings.output.rect.height = surfaceSettings.imageHeight/2;
   nxCode = NEXUS_Graphics2D_DestripeBlit(m_hGraphics2d, &destripeSettings);
   if (nxCode) {
      ALOGE("DestripeToYV12: failed destripe to Cr surface");
      errCode = OMX_ErrorUndefined;
      goto err_destripe;
   }

   if (!GraphicsCheckpoint())
   {
       ALOGE("DestripeToYV12: failed!");
       errCode = OMX_ErrorUndefined;
       goto err_checkpoint;
   }

   NEXUS_Surface_Flush(hSurfaceY);
   NEXUS_Surface_Flush(hSurfaceCr);
   NEXUS_Surface_Flush(hSurfaceCb);

   errCode = OMX_ErrorNone;

err_checkpoint:
err_destripe:
err_surfaces:
   if (hSurfaceY) {
      NEXUS_Surface_Unlock(hSurfaceY);
      NEXUS_Surface_Destroy(hSurfaceY);
   }
   if (hSurfaceCr) {
      NEXUS_Surface_Unlock(hSurfaceCr);
      NEXUS_Surface_Destroy(hSurfaceCr);
   }
   if (hSurfaceCb) {
      NEXUS_Surface_Unlock(hSurfaceCb);
      NEXUS_Surface_Destroy(hSurfaceCb);
   }
err_shared_data:
err_gfx2d:
   return errCode;
}

NEXUS_Error BOMX_VideoDecoder::OpenPidChannel(uint32_t pid)
{
    if ( m_hPlaypump )
    {
        ALOG_ASSERT(NULL == m_hPidChannel);

        NEXUS_PlaypumpOpenPidChannelSettings pidSettings;
        NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&pidSettings);
        pidSettings.pidType = NEXUS_PidType_eVideo;
        m_hPidChannel = NEXUS_Playpump_OpenPidChannel(m_hPlaypump, pid, &pidSettings);
        if ( m_hPidChannel )
        {
            return NEXUS_SUCCESS;
        }
    }
    return BOMX_BERR_TRACE(BERR_UNKNOWN);
}

void BOMX_VideoDecoder::ClosePidChannel()
{
    if ( m_hPidChannel )
    {
        ALOG_ASSERT(NULL != m_hPlaypump);

        NEXUS_Playpump_ClosePidChannel(m_hPlaypump, m_hPidChannel);
        m_hPidChannel = NULL;
    }
}
