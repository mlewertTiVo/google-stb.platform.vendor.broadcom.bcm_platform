/******************************************************************************
 *    (c)2010-2015 Broadcom Corporation
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
#define LOG_TAG "bomx_video_encoder"

#include <fcntl.h>
#include <cutils/log.h>

#include "bomx_video_encoder.h"
#include "nexus_platform.h"
#include "OMX_IndexExt.h"
#include "OMX_VideoExt.h"
#include "OMX_AsString.h"
#include "nexus_base_mmap.h"
#include <inttypes.h>
#include "nx_ashmem.h"

#define BOMX_INPUT_MSG(x)

// Runtime Properties (note: set selinux to permissive mode)
#define B_PROPERTY_ITB_DESC_DEBUG ("media.brcm.venc_itb_desc_debug")
#define B_PROPERTY_MEMBLK_ALLOC ("ro.nexus.ashmem.devname")

#define B_NUM_OF_OUT_BUFFERS (10)
#define B_NUM_OF_IN_BUFFERS (5)

#define Q16_SCALE_FACTOR 65536.0
#define B_DEFAULT_INPUT_FRAMERATE (Q16_SCALE_FACTOR * 15.0)
#define B_DEFAULT_INPUT_NEXUS_FRAMERATE (NEXUS_VideoFrameRate_e15)

#define B_DEFAULT_MAX_FRAME_WIDTH   1280
#define B_DEFAULT_MAX_FRAME_HEIGHT  720
#define B_MAX_FRAME_RATE            NEXUS_VideoFrameRate_e30
#define B_MAX_FRAME_RATE_F          30.0f
#define B_MAX_FRAME_RATE_Q16        (Q16_SCALE_FACTOR*30)
#define B_MIN_FRAME_RATE            NEXUS_VideoFrameRate_e15
#define B_MIN_FRAME_RATE_Q16        (Q16_SCALE_FACTOR*15)
#define B_RATE_BUFFER_DELAY_MS      1500

#define NAL_UNIT_TYPE_SPS  7
#define NAL_UNIT_TYPE_PPS  8

#define VIDEO_ENCODE_DEPTH 16
#define VIDEO_ENCODE_IMAGEINPUT_DEPTH 5

#define OMX_IndexParamEnableAndroidNativeGraphicsBuffer      0x7F000001
#define OMX_IndexParamGetAndroidNativeBufferUsage            0x7F000002
#define OMX_IndexParamStoreMetaDataInBuffers                 0x7F000003
#define OMX_IndexParamUseAndroidNativeBuffer                 0x7F000004
#define OMX_IndexParamUseAndroidNativeBuffer2                0x7F000005
#define OMX_IndexParamDescribeColorFormat                    0x7F000006
#define OMX_IndexParamConfigureVideoTunnelMode               0x7F000007

#define GET_NAL_UNIT_TYPE(x)    (x & 0x1F)
#define NXCLIENT_INVALID_ID     (0xffffffff)
#define MIN_BLOCK_SIZE          (960*1080*2)

#define B_CHECKPOINT_TIMEOUT (5000)

using namespace android;

static const char *g_roles[] = {"video_encoder.avc", "video_encoder.mpeg4", "video_encoder.h263", "video_encoder.vp8"};
static const unsigned int g_numRoles = sizeof(g_roles)/sizeof(const char *);
static int g_roleCodec[] = {OMX_VIDEO_CodingAVC, OMX_VIDEO_CodingMPEG4, OMX_VIDEO_CodingH263, OMX_VIDEO_CodingVP8};

enum BOMX_VideoEncoderEventType
{
    BOMX_VideoEncoderEventType_eImageBuffer = 0,
    BOMX_VideoEncoderEventType_eCheckpoint,
    BOMX_VideoEncoderEventType_eDataReady,
    BOMX_VideoEncoderEventType_eMax
};

extern "C" OMX_ERRORTYPE BOMX_VideoEncoder_Create(
    OMX_COMPONENTTYPE *pComponentTpe,
    OMX_IN OMX_STRING pName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallbacks)
{
    unsigned i;
    bool encodeSupported = false;
    NEXUS_VideoEncoderCapabilities caps;

    /* check if encoder is supported by Nexus */
    NEXUS_GetVideoEncoderCapabilities(&caps);
    for ( i = 0; i < NEXUS_MAX_VIDEO_ENCODERS; i++ )
    {
        if ( caps.videoEncoder[i].supported && caps.videoEncoder[i].memory.used )
        {
            encodeSupported = true;
            break;
        }
    }

    if ( !encodeSupported )
    {
        ALOGW("HW encode support is not available");
        return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
    }

    BOMX_VideoEncoder *pVideoEncoder = new BOMX_VideoEncoder(pComponentTpe, pName, pAppData, pCallbacks);
    if ( NULL == pVideoEncoder )
    {
        return BOMX_ERR_TRACE(OMX_ErrorUndefined);
    }
    else
    {
        if ( pVideoEncoder->IsValid() )
        {
            return OMX_ErrorNone;
        }
        else
        {
            delete pVideoEncoder;
            return BOMX_ERR_TRACE(OMX_ErrorUndefined);
        }
    }
}

extern "C" const char *BOMX_VideoEncoder_GetRole(unsigned roleIndex)
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

static int BOMX_VideoEncoder_OpenMemoryInterface(void)
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

static int BOMX_VideoEncoder_AdvertisePresence(void)
{
   int memBlkFd = -1;

   memBlkFd = BOMX_VideoEncoder_OpenMemoryInterface();
   if (memBlkFd >= 0) {
      struct nx_ashmem_alloc ashmem_alloc;
      memset(&ashmem_alloc, 0, sizeof(struct nx_ashmem_alloc));
      ashmem_alloc.marker = NX_ASHMEM_MARKER_VIDEO_ENCODER;
      int ret = ioctl(memBlkFd, NX_ASHMEM_SET_SIZE, &ashmem_alloc);
      if (ret < 0) {
         close(memBlkFd);
         memBlkFd = -1;
      }
   }

   return memBlkFd;
}

static void BOMX_VideoEncoder_EventCallback(void *pParam, int param)
{
    B_EventHandle hEvent;

    static const char *pEventMsg[BOMX_VideoEncoderEventType_eMax] = {
        "ImageBuffer",
        "Graphic2DCheckPoint",
        "DataReady"
    };

    hEvent = static_cast <B_EventHandle> (pParam);
    if ( param < BOMX_VideoEncoderEventType_eMax )
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

static void BOMX_VideoEncoder_ImageInputEvent(void *pParam)
{
    BOMX_VideoEncoder *pEncoder = static_cast <BOMX_VideoEncoder *> (pParam);

    ALOGV("ImageInput");

    pEncoder->ImageBufferProcess();
}

static void BOMX_VideoEncoder_OutputBufferProcess(void *pParam)
{
    BOMX_VideoEncoder *pEncoder = static_cast <BOMX_VideoEncoder *> (pParam);

    ALOGV("OutputBufferProcess");

    pEncoder->OutputBufferProcess();
}

static void BOMX_VideoEncoder_InputBufferProcess(void *pParam)
{
    BOMX_VideoEncoder *pEncoder = static_cast <BOMX_VideoEncoder *> (pParam);

    ALOGV("InputBufferProcess");

    pEncoder->InputBufferProcess();
}

static OMX_ERRORTYPE BOMX_VideoEncoder_InitMimeType(OMX_VIDEO_CODINGTYPE eCompressionFormat, char *pMimeType)
{
    const char *pMimeTypeStr;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    ALOG_ASSERT(NULL != pMimeType);

    switch ( eCompressionFormat )
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
    case OMX_VIDEO_CodingAVC:
        pMimeTypeStr = "video/avc";
        break;
    case OMX_VIDEO_CodingVP8:
        pMimeTypeStr = "video/x-vnd.on2.vp8";
        break;
    default:
        ALOGW("Unable to find MIME type for eCompressionFormat %u", eCompressionFormat);
        pMimeTypeStr = NULL;
        err = OMX_ErrorBadParameter;
        break;
    }
    if (pMimeTypeStr)
    {
        strncpy(pMimeType, pMimeTypeStr, OMX_MAX_STRINGNAME_SIZE);
    }
    return err;
}

static size_t ComputeInputBufferSize(unsigned stride, unsigned height)
{
    return (stride * height * 3) / 2;
}

static size_t ComputeOutputBufferSize(unsigned stride, unsigned height)
{
    return (stride * height) / 2; // minimum compression ratio
}

BOMX_VideoEncoder::BOMX_VideoEncoder(
    OMX_COMPONENTTYPE *pComponentType,
    const OMX_STRING pName,
    const OMX_PTR pAppData,
    const OMX_CALLBACKTYPE *pCallbacks) :
    BOMX_Component(pComponentType, pName, pAppData, pCallbacks, BOMX_VideoEncoder_GetRole),
    m_pIpcClient(NULL),
    m_pNexusClient(NULL),
    m_nxClientId(NXCLIENT_INVALID_ID),
    m_hSimpleVideoDecoder(NULL),
    m_hSimpleEncoder(NULL),
    m_hStcChannel(NULL),
    m_hImageInput(NULL),
    m_nImageSurfaceFreeListLen(0),
    m_nImageSurfacePushedListLen(0),
    m_bSimpleEncoderStarted(false),
    m_EmptyFrListLen(0),
    m_EncodedFrListLen(0),
    m_hImageInputEvent(NULL),
    m_ImageInputEventId(NULL),
    m_hOutputBufferProcessEvent(NULL),
    m_outputBufferProcessEventId(NULL),
    m_hInputBufferProcessEvent(NULL),
    m_inputBufferProcessEventId(NULL),
    m_hCheckpointEvent(NULL),
    m_hGraphics2d(NULL),
    m_pBufferTracker(NULL),
    m_maxFrameWidth(B_DEFAULT_MAX_FRAME_WIDTH),
    m_maxFrameHeight(B_DEFAULT_MAX_FRAME_HEIGHT),
    m_metadataEnabled(false),
    m_nativeGraphicsEnabled(false),
    m_inputMode(BOMX_VideoEncoderInputBufferType_eStandard),
    m_pITBDescDumpFile(NULL),
    m_memTracker(-1)
{
    unsigned i;
    NEXUS_Error errCode;

#define MAX_PORT_FORMATS (4)
#define MAX_INPUT_PORT_FORMATS  BOMX_VideoEncoderInputBufferType_eMax
#define MAX_OUTPUT_PORT_FORMATS (3)

    BDBG_CASSERT(MAX_INPUT_PORT_FORMATS  <= MAX_PORT_FORMATS);
    BDBG_CASSERT(MAX_OUTPUT_PORT_FORMATS <= MAX_PORT_FORMATS);

    OMX_VIDEO_PORTDEFINITIONTYPE portDefs;
    OMX_VIDEO_PARAM_PORTFORMATTYPE portFormats[MAX_PORT_FORMATS];

    BLST_Q_INIT(&m_ImageSurfaceFreeList);
    BLST_Q_INIT(&m_ImageSurfacePushedList);
    BLST_Q_INIT(&m_EmptyFrList);
    BLST_Q_INIT(&m_EncodedFrList);

    m_memTracker = BOMX_VideoEncoder_AdvertisePresence();

    m_numVideoPorts = 0;
    m_videoPortBase = 0;    // Android seems to require this - was: BOMX_COMPONENT_PORT_BASE(BOMX_ComponentId_eVideoEncoder, OMX_PortDomainVideo);

    NEXUS_VideoEncoderCapabilities caps;
    NEXUS_GetVideoEncoderCapabilities(&caps);
    for ( i = 0; i < NEXUS_MAX_VIDEO_ENCODERS; i++ )
    {
        if ( caps.videoEncoder[i].supported && caps.videoEncoder[i].memory.used )
        {
            m_maxFrameWidth = caps.videoEncoder[i].memory.maxWidth;
            m_maxFrameHeight = caps.videoEncoder[i].memory.maxHeight;
            break;
        }
    }

    memset(&portDefs, 0, sizeof(portDefs));
    portDefs.eCompressionFormat = OMX_VIDEO_CodingUnused;
    portDefs.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    strcpy(m_inputMimeType, "video/x-raw");
    portDefs.cMIMEType = m_inputMimeType;
    portDefs.nFrameWidth = m_maxFrameWidth;
    portDefs.nFrameHeight = m_maxFrameHeight;
    portDefs.nStride = portDefs.nFrameWidth;
    portDefs.nSliceHeight = portDefs.nFrameHeight;
    portDefs.xFramerate = (OMX_U32)B_DEFAULT_INPUT_FRAMERATE;
    for ( i = 0; i < MAX_INPUT_PORT_FORMATS; i++ )
    {
        memset(&portFormats[i], 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        BOMX_STRUCT_INIT(&portFormats[i]);
        portFormats[i].nIndex = i;
        portFormats[i].nPortIndex = m_videoPortBase;
        portFormats[i].eCompressionFormat = OMX_VIDEO_CodingUnused;
        switch ( i )
        {
        default:
        case BOMX_VideoEncoderInputBufferType_eStandard:
            portFormats[i].eColorFormat = OMX_COLOR_FormatYUV420Planar;
            break;
        case BOMX_VideoEncoderInputBufferType_eNative:
            portFormats[i].eColorFormat = (OMX_COLOR_FORMATTYPE)((int)HAL_PIXEL_FORMAT_YV12);
            break;
        case BOMX_VideoEncoderInputBufferType_eMetadata:
            portFormats[i].eColorFormat = OMX_COLOR_FormatAndroidOpaque;
            break;
        }
    }
    m_pVideoPorts[0] = new BOMX_VideoPort(m_videoPortBase, OMX_DirInput, B_NUM_OF_IN_BUFFERS, ComputeInputBufferSize(portDefs.nStride, portDefs.nSliceHeight), false, 0, &portDefs, portFormats, MAX_INPUT_PORT_FORMATS);
    if ( NULL == m_pVideoPorts[0] )
    {
        ALOGE("Unable to create video input port");
        this->Invalidate();
        return;
    }
    m_numVideoPorts++;



    memset(&portDefs, 0, sizeof(portDefs));
    portDefs.eCompressionFormat = OMX_VIDEO_CodingAVC;
    portDefs.eColorFormat = OMX_COLOR_FormatUnused;
    portDefs.nFrameWidth = m_maxFrameWidth;
    portDefs.nFrameHeight = m_maxFrameHeight;
    portDefs.nStride = portDefs.nFrameWidth;
    portDefs.nSliceHeight = portDefs.nFrameHeight;
    (void)BOMX_VideoEncoder_InitMimeType(portDefs.eCompressionFormat, m_outputMimeType);
    portDefs.cMIMEType = m_outputMimeType;
    for ( i = 0; i < MAX_OUTPUT_PORT_FORMATS; i++ )
    {
        memset(&portFormats[i], 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        BOMX_STRUCT_INIT(&portFormats[i]);
        portFormats[i].nPortIndex = m_videoPortBase + 1;
        portFormats[i].nIndex = i;
        switch ( i )
        {
        default:
        case 0:
            portFormats[i].eCompressionFormat = OMX_VIDEO_CodingMPEG2;
            break;
        case 1:
            portFormats[i].eCompressionFormat = OMX_VIDEO_CodingAVC;
            break;
        case 2:
            portFormats[i].eCompressionFormat = OMX_VIDEO_CodingVP8;
            break;
        }
    }

    m_pVideoPorts[1] = new BOMX_VideoPort(m_videoPortBase + 1, OMX_DirOutput, B_NUM_OF_OUT_BUFFERS, ComputeOutputBufferSize(portDefs.nStride, portDefs.nSliceHeight), false, 0, &portDefs, portFormats, MAX_OUTPUT_PORT_FORMATS);
    if ( NULL == m_pVideoPorts[1] )
    {
        ALOGE("Unable to create video output port");
        this->Invalidate();
        return;
    }
    m_numVideoPorts++;

    /* set default component role */
    SetRole(g_roles[0]);


    /* create image input event */
    m_hImageInputEvent = B_Event_Create(NULL);
    if ( NULL == m_hImageInputEvent )
    {
        ALOGE("Unable to create image input event");
        this->Invalidate();
        return;
    }

    /* create output buffer process event */
    m_hOutputBufferProcessEvent = B_Event_Create(NULL);
    if ( NULL == m_hOutputBufferProcessEvent )
    {
        ALOGE("Unable to create output process event");
        this->Invalidate();
        return;
    }

    /* register output buffer process event handler */
    m_outputBufferProcessEventId = this->RegisterEvent(m_hOutputBufferProcessEvent, BOMX_VideoEncoder_OutputBufferProcess, static_cast <void *> (this));
    if ( NULL == m_outputBufferProcessEventId )
    {
        ALOGE("Unable to register output frame event");
        this->Invalidate();
        return;
    }

    /* create input buffer process event */
    m_hInputBufferProcessEvent = B_Event_Create(NULL);
    if ( NULL == m_hInputBufferProcessEvent )
    {
        ALOGE("Unable to create input buffer process event");
        this->Invalidate();
        return;
    }

    /* register input buffer process event handler */
    m_inputBufferProcessEventId = this->RegisterEvent(m_hInputBufferProcessEvent, BOMX_VideoEncoder_InputBufferProcess, static_cast <void *> (this));
    if ( NULL == m_inputBufferProcessEventId )
    {
        ALOGE("Unable to register input frame event");
        this->Invalidate();
        return;
    }

    /* create buffer tracker */
    m_pBufferTracker = new BOMX_BufferTracker(this);
    if ( NULL == m_pBufferTracker || !m_pBufferTracker->Valid() )
    {
        ALOGE("Unable to create buffer tracker");
        this->Invalidate();
        return;
    }

    /* create Nexus IPC client */
    m_pIpcClient = NexusIPCClientFactory::getClient(pName);
    if ( NULL == m_pIpcClient )
    {
        ALOGE("Unable to create client factory");
        this->Invalidate();
        return;
    }

    /* create Nexus client */
    m_pNexusClient = m_pIpcClient->createClientContext();
    if (m_pNexusClient == NULL)
    {
        ALOGE("Unable to create nexus client context");
        this->Invalidate();
        return;
    }

    m_hCheckpointEvent = B_Event_Create(NULL);
    m_hGraphics2d = NEXUS_Graphics2D_Open(NEXUS_ANY_ID, NULL);
    if ( NULL == m_hGraphics2d )
    {
        ALOGE("Unable to open graphics 2d");
        this->Invalidate();
        return;
    }
    else
    {
        NEXUS_Graphics2DSettings gfxSettings;
        NEXUS_Graphics2D_GetSettings(m_hGraphics2d, &gfxSettings);
        gfxSettings.checkpointCallback.callback = BOMX_VideoEncoder_EventCallback;
        gfxSettings.checkpointCallback.context = static_cast<void *>(m_hCheckpointEvent);
        gfxSettings.checkpointCallback.param = (int)BOMX_VideoEncoderEventType_eCheckpoint;
        gfxSettings.pollingCheckpoint = false;
        errCode = NEXUS_Graphics2D_SetSettings(m_hGraphics2d, &gfxSettings);
        if ( errCode )
        {
            errCode = BOMX_BERR_TRACE(errCode);
            this->Invalidate();
            return;
        }
    }

    /* set AVC defaults */
    // Much of this structure is not relevant.  Zero everything except for allowed picture types, profile, level.
    memset(&m_sAvcVideoParams, 0, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
    BOMX_STRUCT_INIT(&m_sAvcVideoParams);
    m_sAvcVideoParams.nPortIndex = m_videoPortBase + 1;
    m_sAvcVideoParams.nRefFrames = 16;
    m_sAvcVideoParams.eProfile = OMX_VIDEO_AVCProfileBaseline;
    m_sAvcVideoParams.eLevel = OMX_VIDEO_AVCLevel31;
    m_sAvcVideoParams.nAllowedPictureTypes = (OMX_U32)OMX_VIDEO_PictureTypeI|(OMX_U32)OMX_VIDEO_PictureTypeP|(OMX_U32)OMX_VIDEO_PictureTypeB|(OMX_U32)OMX_VIDEO_PictureTypeEI|(OMX_U32)OMX_VIDEO_PictureTypeEP;

    /* set video bitrate defaults */
    memset(&m_sVideoBitrateParams, 0, sizeof(OMX_VIDEO_PARAM_BITRATETYPE));
    BOMX_STRUCT_INIT(&m_sVideoBitrateParams);
    m_sVideoBitrateParams.eControlRate = OMX_Video_ControlRateConstant;
    m_sVideoBitrateParams.nTargetBitrate = 60000;

    /* allocate encoded frame header list */
    for (unsigned int i=0; i < VIDEO_ENCODE_DEPTH; i++)
    {
        BOMX_NexusEncodedVideoFrame *pEncoFr = new BOMX_NexusEncodedVideoFrame;

        if (!pEncoFr)
        {
            ALOGE("failed to allocate frame header");
            this->Invalidate();
            return;
        }

        pEncoFr->frameData = new Vector <NEXUS_VideoEncoderDescriptor *>();
        BOMX_VIDEO_ENCODER_INIT_ENC_FR(pEncoFr);
        BLST_Q_INSERT_TAIL(&m_EmptyFrList, pEncoFr, link);
        m_EmptyFrListLen++;
    }
}

BOMX_VideoEncoder::~BOMX_VideoEncoder()
{
    unsigned i;
    BOMX_NexusEncodedVideoFrame *pNxVidEncFr;

    ShutdownScheduler();

    Lock();

    /* stop encoder if started */
    if ( IsEncoderStarted() )
    {
        StopEncoder();
    }

    /* release encoder resources if acquired */
    ReleaseEncoderResource();

    /* unregister event handler */
    if ( m_outputBufferProcessEventId )
    {
        UnregisterEvent(m_outputBufferProcessEventId);
    }
    if ( m_inputBufferProcessEventId )
    {
        UnregisterEvent(m_inputBufferProcessEventId);
    }
    if ( m_ImageInputEventId )
    {
        UnregisterEvent(m_ImageInputEventId);
    }
    /* destroy events */
    if ( m_hOutputBufferProcessEvent )
    {
        B_Event_Destroy(m_hOutputBufferProcessEvent);
    }
    if ( m_hInputBufferProcessEvent )
    {
        B_Event_Destroy(m_hInputBufferProcessEvent);
    }
    if ( m_hImageInputEvent )
    {
        B_Event_Destroy(m_hImageInputEvent);
    }
    if ( m_hGraphics2d )
    {
        NEXUS_Graphics2D_Close(m_hGraphics2d);
    }
    if ( m_hCheckpointEvent )
    {
        B_Event_Destroy(m_hCheckpointEvent);
    }

    /* delete all ports */
    for ( i = 0; i < m_numVideoPorts; i++ )
    {
        if ( m_pVideoPorts[i] )
        {
            while ( !m_pVideoPorts[i]->IsEmpty() )
            {
                // Clean up the allocated OMX buffers if they have not been freed for some reason
                BOMX_Buffer *pBuffer = m_pVideoPorts[i]->GetPortBuffer();
                ALOG_ASSERT(NULL != pBuffer);

                OMX_ERRORTYPE err = FreeBuffer((m_pVideoPorts[i]->GetDir() == OMX_DirInput) ? m_videoPortBase : m_videoPortBase+1,  pBuffer->GetHeader());
                ALOGE_IF(err != OMX_ErrorNone, "Failed to free buffer at destructor %d", err);
            }
            delete m_pVideoPorts[i];
        }
    }

    /* delete Nexus IPC client */
    if ( m_pIpcClient )
    {
        if ( m_pNexusClient )
        {
            m_pIpcClient->destroyClientContext(m_pNexusClient);
        }
        delete m_pIpcClient;
    }

    /* delete buffer tracker */
    if ( m_pBufferTracker )
    {
        delete m_pBufferTracker;
    }

    /* release frame buffer list */
    while ( (pNxVidEncFr = BLST_Q_FIRST(&m_EncodedFrList)) )
    {
        BLST_Q_REMOVE_HEAD(&m_EncodedFrList, link);
        delete pNxVidEncFr;
    }
    while ( (pNxVidEncFr = BLST_Q_FIRST(&m_EmptyFrList)) )
    {
        BLST_Q_REMOVE_HEAD(&m_EmptyFrList, link);
        delete pNxVidEncFr;
    }

    if (m_memTracker != -1)
    {
        close(m_memTracker);
        m_memTracker = -1;
    }
    Unlock();
}

OMX_ERRORTYPE BOMX_VideoEncoder::GetParameter(
    OMX_IN  OMX_INDEXTYPE nParamIndex,
    OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    switch ( (int)nParamIndex )
    {
    case OMX_IndexParamVideoAvc:
    {
        OMX_VIDEO_PARAM_AVCTYPE *pAvc = (OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure;
        ALOGV("GetParameter OMX_IndexParamVideoAvc");
        BOMX_STRUCT_VALIDATE(pAvc);
        if ( pAvc->nPortIndex != m_videoPortBase + 1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        // Much of this structure is not relevant.  Zero everything except for allowed picture types, profile, level.
        memset(pAvc, 0, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        BOMX_STRUCT_INIT(pAvc);
        pAvc->nPortIndex = m_videoPortBase + 1;
        pAvc->nRefFrames = 16;
        pAvc->eProfile = m_sAvcVideoParams.eProfile;
        pAvc->eLevel = m_sAvcVideoParams.eLevel;
        pAvc->nAllowedPictureTypes = m_sAvcVideoParams.nAllowedPictureTypes;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamVideoBitrate:
    {
        OMX_VIDEO_PARAM_BITRATETYPE *pBitrate = (OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
        ALOGV("GetParameter OMX_IndexParamVideoBitrate");
        BOMX_STRUCT_VALIDATE(pBitrate);
        if ( pBitrate->nPortIndex != m_videoPortBase + 1 )
        {
            ALOGE("output port only");
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        pBitrate->eControlRate = m_sVideoBitrateParams.eControlRate;
        pBitrate->nTargetBitrate = m_sVideoBitrateParams.nTargetBitrate;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamVideoProfileLevelQuerySupported:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *pProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        ALOGV("GetParameter OMX_IndexParamVideoProfileLevelQuerySupported");
        BOMX_STRUCT_VALIDATE(pProfileLevel);
        if ( pProfileLevel->nPortIndex != m_videoPortBase + 1 )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        switch ( (int)GetCodec() )
        {
        default:
            // Only certain codecs support this interface
            return BOMX_ERR_TRACE(OMX_ErrorUnsupportedIndex);
        case OMX_VIDEO_CodingAVC:
            switch ( pProfileLevel->nProfileIndex )
            {
            case 0:
                pProfileLevel->eProfile = (OMX_U32)OMX_VIDEO_AVCProfileBaseline;
                pProfileLevel->eLevel = (OMX_U32)GetMaxLevelAvc(OMX_VIDEO_AVCProfileBaseline);
                break;
            case 1:
                pProfileLevel->eProfile = (OMX_U32)OMX_VIDEO_AVCProfileMain;
                pProfileLevel->eLevel = (OMX_U32)GetMaxLevelAvc(OMX_VIDEO_AVCProfileMain);
                break;
            case 2:
                pProfileLevel->eProfile = (OMX_U32)OMX_VIDEO_AVCProfileHigh;
                pProfileLevel->eLevel = (OMX_U32)GetMaxLevelAvc(OMX_VIDEO_AVCProfileHigh);
                break;
            default:
                return OMX_ErrorNoMore;
            }
            break;
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
        if ( IsEncoderStarted() )
        {
            switch ( (int)GetCodec() )
            {
            default:
                // Only certain codecs support this interface
                break;
            case OMX_VIDEO_CodingAVC:
                pProfileLevel->eProfile = m_sAvcVideoParams.eProfile;
                pProfileLevel->eLevel = m_sAvcVideoParams.eLevel;
                break;
            }
        }
        return OMX_ErrorNone;
    }
    case OMX_IndexParamEnableAndroidNativeGraphicsBuffer:
    {
        EnableAndroidNativeBuffersParams *pEnableParams = (EnableAndroidNativeBuffersParams *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pEnableParams);
        pEnableParams->enable = OMX_FALSE;
        if ( pEnableParams->nPortIndex == m_videoPortBase )
        {
            pEnableParams->enable = m_nativeGraphicsEnabled == true ? OMX_TRUE : OMX_FALSE;
        }
        ALOGV("GetParameter OMX_IndexParamEnableAndroidNativeGraphicsBuffer %u", pEnableParams->enable);
        return OMX_ErrorNone;
    }
    case OMX_IndexParamStoreMetaDataInBuffers:
    {
        StoreMetaDataInBuffersParams *pMetadata = (StoreMetaDataInBuffersParams *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pMetadata);
        pMetadata->bStoreMetaData = OMX_FALSE;
        if ( pMetadata->nPortIndex == m_videoPortBase )
        {
            pMetadata->bStoreMetaData = m_metadataEnabled == true ? OMX_TRUE : OMX_FALSE;
        }
        ALOGV("GetParameter OMX_IndexParamStoreMetaDataInBuffers %u", pMetadata->bStoreMetaData);
        return OMX_ErrorNone;
    }
    case OMX_IndexParamGetAndroidNativeBufferUsage:
    {
        GetAndroidNativeBufferUsageParams *pUsage = (GetAndroidNativeBufferUsageParams *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pUsage);
        ALOGV("GetParameter OMX_IndexParamGetAndroidNativeBufferUsage");
        if ( pUsage->nPortIndex != m_videoPortBase )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        // Flag this as a HW video decoder allocation to gralloc
        pUsage->nUsage = 0;//GRALLOC_USAGE_PRIVATE_0; -- TODO: This causes Dequeue buffer to fail and playback stops.
        return OMX_ErrorNone;
    }
    case OMX_IndexParamDescribeColorFormat:
    {
        DescribeColorFormatParams *pColorFormat = (DescribeColorFormatParams *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pColorFormat);
        ALOGV("GetParameter OMX_IndexParamDescribeColorFormat");
        switch ( (int)pColorFormat->eColorFormat )
        {
        case HAL_PIXEL_FORMAT_YV12:
        {
            // YV12 is Y/Cr/Cb
            pColorFormat->sMediaImage.mPlane[MediaImage::V].mOffset = pColorFormat->nStride*pColorFormat->nSliceHeight;
            pColorFormat->sMediaImage.mPlane[MediaImage::U].mOffset =
                pColorFormat->sMediaImage.mPlane[MediaImage::V].mOffset +
                (pColorFormat->nStride*pColorFormat->nSliceHeight)/4;
        }
        break;
        case OMX_COLOR_FormatYUV420Planar:
        {
            // 420Planar is Y/Cb/Cr
            pColorFormat->sMediaImage.mPlane[MediaImage::U].mOffset = pColorFormat->nStride*pColorFormat->nSliceHeight;
            pColorFormat->sMediaImage.mPlane[MediaImage::V].mOffset =
                pColorFormat->sMediaImage.mPlane[MediaImage::U].mOffset +
                (pColorFormat->nStride*pColorFormat->nSliceHeight)/4;
        }
        break;
        case OMX_COLOR_FormatAndroidOpaque:
        {
            // AndroidOpaque
            pColorFormat->sMediaImage.mType = MediaImage::MEDIA_IMAGE_TYPE_UNKNOWN;
            return OMX_ErrorNone;
        }
        break;
        default:
        {
            ALOGE("Unknown color format %x", pColorFormat->eColorFormat);
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
        pColorFormat->sMediaImage.mPlane[MediaImage::Y].mRowInc = pColorFormat->nStride;
        pColorFormat->sMediaImage.mPlane[MediaImage::Y].mHorizSubsampling = 1;
        pColorFormat->sMediaImage.mPlane[MediaImage::Y].mVertSubsampling = 1;
        pColorFormat->sMediaImage.mPlane[MediaImage::U].mColInc = 1;
        pColorFormat->sMediaImage.mPlane[MediaImage::U].mRowInc = pColorFormat->nStride/2;
        pColorFormat->sMediaImage.mPlane[MediaImage::U].mHorizSubsampling = 2;
        pColorFormat->sMediaImage.mPlane[MediaImage::U].mVertSubsampling = 2;
        pColorFormat->sMediaImage.mPlane[MediaImage::V].mColInc = 1;
        pColorFormat->sMediaImage.mPlane[MediaImage::V].mRowInc = pColorFormat->nStride/2;
        pColorFormat->sMediaImage.mPlane[MediaImage::V].mHorizSubsampling = 2;
        pColorFormat->sMediaImage.mPlane[MediaImage::V].mVertSubsampling = 2;

        return OMX_ErrorNone;
    }

    default:
        ALOGW("GetParameter %#x Deferring to base class", nParamIndex);
        return BOMX_Component::GetParameter(nParamIndex, pComponentParameterStructure);
    }
}

OMX_ERRORTYPE BOMX_VideoEncoder::SetParameter(
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
        // The spec says this should reset the component to default settings for the role specified.
        // It is technically redundant for this component, changing the output codec has the same
        // effect - but this will also set the output codec accordingly.
        for ( i = 0; i < g_numRoles; i++ )
        {
            if ( !strcmp(g_roles[i], (const char *)pRole->cRole) )
            {
                // Set output port to matching compression std.
                OMX_VIDEO_PARAM_PORTFORMATTYPE format;
                memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
                BOMX_STRUCT_INIT(&format);
                format.nPortIndex = m_videoPortBase + 1;
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
        ALOGV("SetParameter OMX_IndexParamVideoPortFormat");
        BOMX_STRUCT_VALIDATE(pFormat);
        pPort = FindPortByIndex(pFormat->nPortIndex);
        if ( NULL == pPort )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if ( pPort->GetDir() == OMX_DirOutput )
        {
            ALOGV("Set Output Port Compression Format to %u (%#x)", pFormat->eCompressionFormat, pFormat->eCompressionFormat);
            err = BOMX_VideoEncoder_InitMimeType(pFormat->eCompressionFormat, m_outputMimeType);
            if ( err )
            {
                return BOMX_ERR_TRACE(err);
            }
            // Per the OMX spec you are supposed to initialize the port defs to defaults when changing format
            // The only thing we change is the format itself though - the MIME type is set above.
            OMX_VIDEO_PORTDEFINITIONTYPE portDefs;
            memset(&portDefs, 0, sizeof(portDefs));
            portDefs.eCompressionFormat = pFormat->eCompressionFormat;
            portDefs.cMIMEType = m_outputMimeType;
            err = m_pVideoPorts[1]->SetPortFormat(pFormat, &portDefs);
            if ( err )
            {
                return BOMX_ERR_TRACE(err);
            }
            /* set component role */
            SetRole((const char *)m_outputMimeType);
        }
        else
        {
            ALOGV("Set Input Port Color Format to %u (%#x)", pFormat->eColorFormat, pFormat->eColorFormat);
            // Per the OMX spec you are supposed to initialize the port defs to defaults when changing format
            // Leave buffer size parameters alone and update color format/framerate.
            OMX_VIDEO_PORTDEFINITIONTYPE portDefs;
            portDefs = pPort->GetDefinition()->format.video;
            portDefs.eCompressionFormat = pFormat->eCompressionFormat;
            portDefs.xFramerate = pFormat->xFramerate;
            portDefs.eColorFormat = pFormat->eColorFormat;
            /// TODO: Fix this to update entire port definition.
            err = m_pVideoPorts[0]->SetPortFormat(pFormat, &portDefs);
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
            ALOGE("Video input MIME type cannot be changed in the application");
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        else if ( pDef->nPortIndex == (m_videoPortBase+1) && pDef->format.video.cMIMEType != m_outputMimeType )
        {
            ALOGE("Video output MIME type cannot be changed in the application");
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        BOMX_Port *pPort = FindPortByIndex(pDef->nPortIndex);
        if ( NULL == pPort )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if ( pDef->nPortIndex == m_videoPortBase + 1 && pDef->format.video.eCompressionFormat != GetCodec() )
        {
            ALOGE("Video compression format cannot be changed in the port definition.  Change Port Format instead.");
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        /* The value 0x0 is used to indicate the frame rate is unknown, variable, or is not needed. */
        if ( pDef->format.video.xFramerate && (MapOMXFrameRateToNexus(pDef->format.video.xFramerate) == NEXUS_VideoFrameRate_eUnknown ||
                                               pDef->format.video.xFramerate > B_MAX_FRAME_RATE_Q16 ||
                                               pDef->format.video.xFramerate < B_MIN_FRAME_RATE_Q16) )
        {
           ALOGE("Video framerate: %.2fHz is not supported by the encoder", (float)pDef->format.video.xFramerate/(float)Q16_SCALE_FACTOR);
           return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }

        // Handle update in base class
        err = BOMX_Component::SetParameter(nIndex, (OMX_PTR)pDef);
        if ( err != OMX_ErrorNone )
        {
            return BOMX_ERR_TRACE(err);
        }
        else if ( pDef->nPortIndex == (m_videoPortBase) )
        {
            OMX_PARAM_PORTDEFINITIONTYPE portDef;
            m_pVideoPorts[0]->GetDefinition(&portDef);
            // Ensure slice height and stride match frame width/height and update buffer size
            portDef.format.video.nSliceHeight = portDef.format.video.nFrameHeight;
            portDef.format.video.nStride = portDef.format.video.nFrameWidth;
            portDef.nBufferSize = ComputeInputBufferSize(portDef.format.video.nStride, portDef.format.video.nSliceHeight);
            err = m_pVideoPorts[0]->SetDefinition(&portDef);
            if ( err )
            {
                return BOMX_ERR_TRACE(err);
            }
        }
        return OMX_ErrorNone;
    }
    case OMX_IndexParamVideoAvc:
    {
        OMX_VIDEO_PARAM_AVCTYPE *pParam = (OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure;
        ALOGV("SetParameter OMX_IndexParamVideoAvc");
        BOMX_STRUCT_VALIDATE(pParam);
        if ( pParam->nPortIndex != m_videoPortBase + 1 )
        {
            ALOGE("output port only");
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }

        BKNI_Memcpy(&m_sAvcVideoParams, pParam, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        ALOGV("Profile = %d, Level = %d", pParam->eProfile, pParam->eLevel);
        return OMX_ErrorNone;
    }
    case OMX_IndexParamVideoBitrate:
    {
        OMX_VIDEO_PARAM_BITRATETYPE *pParam = (OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
        ALOGV("SetParameter OMX_IndexParamVideoBitrate");
        BOMX_STRUCT_VALIDATE(pParam);
        if ( pParam->nPortIndex != m_videoPortBase + 1 )
        {
            ALOGE("output port only");
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }

        BKNI_Memcpy(&m_sVideoBitrateParams, pParam, sizeof(OMX_VIDEO_PARAM_BITRATETYPE));
        ALOGV("control type = %d, bitrate = %d", pParam->eControlRate, pParam->nTargetBitrate);

        if ( IsEncoderStarted() )
        {
            NEXUS_Error rc = UpdateEncoderSettings();
            if ( rc )
            {
                LOGE("Failed to update encoder settings");
                return BOMX_ERR_TRACE(OMX_ErrorUndefined);
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

        if ( pEnableParams->nPortIndex != m_videoPortBase )
        {
            if (pEnableParams->enable == OMX_FALSE)
            {
                // to make ACodec happy
                return OMX_ErrorNone;
            }
            else
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
        }
        oldValue = m_nativeGraphicsEnabled;
        m_nativeGraphicsEnabled = pEnableParams->enable == OMX_TRUE ? true : false;

        // Mode has changed.  Set appropriate output color format.
        OMX_VIDEO_PARAM_PORTFORMATTYPE portFormat;
        BOMX_STRUCT_INIT(&portFormat);
        portFormat.nPortIndex = m_videoPortBase;
        portFormat.eCompressionFormat = OMX_VIDEO_CodingUnused;
        if ( m_nativeGraphicsEnabled )
        {
            // In this mode, the color format should be an android HAL format.
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

        if ( pMetadata->nPortIndex != m_videoPortBase )
        {
            if ( pMetadata->bStoreMetaData == OMX_FALSE )
            {
                // to make ACodec happy
                return OMX_ErrorNone;
            }
            else
            {
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
        }
        m_metadataEnabled = pMetadata->bStoreMetaData == OMX_TRUE ? true : false;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamUseAndroidNativeBuffer:
    case OMX_IndexParamUseAndroidNativeBuffer2:
    {
        UseAndroidNativeBufferParams *pBufferParams = (UseAndroidNativeBufferParams *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pBufferParams);
        ALOGV("SetParameter OMX_IndexParamUseAndroidNativeBuffer");
        if ( pBufferParams->nPortIndex != m_videoPortBase )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        sp <ANativeWindowBuffer> nativeBuffer = pBufferParams->nativeBuffer;
        private_handle_t *handle = (private_handle_t *)nativeBuffer->handle;
        err = AddInputPortBuffer(pBufferParams->bufferHeader, pBufferParams->nPortIndex, pBufferParams->pAppPrivate, handle);
        if ( OMX_ErrorNone != err )
        {
            return BOMX_ERR_TRACE(err);
        }
        return OMX_ErrorNone;
    }
    default:
        ALOGV("SetParameter %#x - Defer to base class", nIndex);
        return BOMX_ERR_TRACE(BOMX_Component::SetParameter(nIndex, pComponentParameterStructure));
    }
}

NEXUS_VideoCodec BOMX_VideoEncoder::GetNexusCodec()
{
    return GetNexusCodec((OMX_VIDEO_CODINGTYPE)GetCodec());
}

NEXUS_VideoCodec BOMX_VideoEncoder::GetNexusCodec(OMX_VIDEO_CODINGTYPE omxType)
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
    case OMX_VIDEO_CodingHEVC:
        return NEXUS_VideoCodec_eH265;
    default:
        ALOGE("Unknown video codec %u (%#x)", (unsigned int)omxType, (int)omxType);
        return NEXUS_VideoCodec_eNone;
    }
}

NEXUS_Error BOMX_VideoEncoder::SetInputPortState(OMX_STATETYPE newState)
{
    ALOGV("Setting Input Port State to %s", BOMX_StateName(newState));
    // Loaded means stop and release all resources
    if ( newState == OMX_StateLoaded )
    {
        if ( IsEncoderStarted() )
        {
            StopEncoder();
        }
        // Shutdown and free resources
        ReleaseEncoderResource();
    }
    else
    {
        NEXUS_Error errCode;

        // All states other than loaded require encoder/decoder resource allocated
        if ( m_nxClientId == NXCLIENT_INVALID_ID )
        {
            errCode = AllocateEncoderResource();
            if (errCode)
            {
                ReleaseEncoderResource();
                return BOMX_BERR_TRACE(BERR_UNKNOWN);
            }
        }

        switch ( newState )
        {
        case OMX_StateIdle:
            if ( IsEncoderStarted() )
            {
                // Executing/Paused -> Idle = Stop
                StopEncoder();
            }
            break;
        case OMX_StateExecuting:
            if ( IsEncoderStarted() )
            {
                // Paused -> Executing = Resume
                // trigger the event to restart the input buffer process
                B_Event_Set(m_hInputBufferProcessEvent);
            }
            else
            {
                // Idle -> Executing = Start
                errCode = StartEncoder();
                if (errCode)
                {
                    StopEncoder(); // Encoder might be partially started
                    ReleaseEncoderResource();
                    return BOMX_BERR_TRACE(errCode);
                }
            }
            break;
        case OMX_StatePause:
            if ( IsEncoderStarted() )
            {
                // Executing -> Paused = Pause

                // InputBufferProcess will check the state and stop
                // pushing buffer to HW encoder if it is in Pause state.
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

NEXUS_Error BOMX_VideoEncoder::SetOutputPortState(OMX_STATETYPE newState)
{
    // The Output port for video encoder is a logical construct and not
    // related to the actual encoder output except through an indirect
    // Queue.  For format changes, we need to be able to control this logical
    // output port independently and leave the input port active during any
    // Resolution Change.
    ALOGV("Setting Output Port State to %s", BOMX_StateName(newState));
    if ( newState == OMX_StateLoaded || newState == OMX_StateIdle )
    {
        // Return all pending buffers to the client
        ReturnPortBuffers(m_pVideoPorts[1]);
        ResetEncodedFrameList();
    }
    return NEXUS_SUCCESS;
}

OMX_ERRORTYPE BOMX_VideoEncoder::CommandStateSet(
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

        ALOGV("End State Change %s->%s", BOMX_StateName(oldState), BOMX_StateName(newState));
        return OMX_ErrorNone;
    }
    case OMX_StateIdle:
    {
        // Transitioning from Loaded->Idle requires us to allocate all required resources
        if ( oldState == OMX_StateLoaded )
        {
            ALOGV("Waiting for port population...");
            PortWaitBegin();
            // Now we need to wait for all enabled ports to become populated
            while ( (m_pVideoPorts[0]->IsEnabled() && !m_pVideoPorts[0]->IsPopulated()) ||
                    (m_pVideoPorts[1]->IsEnabled() && !m_pVideoPorts[1]->IsPopulated()) )
            {
                if ( PortWait() != B_ERROR_SUCCESS )
                {
                    ALOGE("Timeout waiting for ports to be populated");
                    PortWaitEnd();
                    return BOMX_ERR_TRACE(OMX_ErrorTimeout);
                }
            }
            PortWaitEnd();
            ALOGV("Done waiting for port population");

            bool inputEnabled = m_pVideoPorts[0]->IsEnabled();
            bool outputEnabled = m_pVideoPorts[1]->IsEnabled();
            if ( m_pVideoPorts[1]->IsPopulated() && m_pVideoPorts[1]->IsEnabled() )
            {
                errCode = SetOutputPortState(OMX_StateIdle);
                if ( errCode )
                {
                    (void)CommandPortDisable(OMX_ALL);
                    if ( inputEnabled ) {
                        m_pVideoPorts[0]->Enable();
                    }
                    if ( outputEnabled ) {
                        m_pVideoPorts[1]->Enable();
                    }
                    return BOMX_ERR_TRACE(OMX_ErrorUndefined);
                }
            }
            if ( m_pVideoPorts[0]->IsPopulated() && m_pVideoPorts[0]->IsEnabled() )
            {
                errCode = SetInputPortState(OMX_StateIdle);
                if ( errCode )
                {
                    (void)CommandPortDisable(OMX_ALL);
                    if ( inputEnabled ) {
                        m_pVideoPorts[0]->Enable();
                    }
                    if ( outputEnabled ) {
                        m_pVideoPorts[1]->Enable();
                    }
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

    case OMX_StateWaitForResources:
    case OMX_StateInvalid:
        ALOGV("End State Change %s->%s", BOMX_StateName(oldState), BOMX_StateName(newState));
        return OMX_ErrorNone;
    default:
        ALOGE("Unsupported state %u", newState);
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
    }
}

OMX_ERRORTYPE BOMX_VideoEncoder::CommandFlush(
    OMX_U32 portIndex)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    if ( StateChangeInProgress() )
    {
        ALOGE("Flush should not be called during a state change");
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
            ALOGE("Invalid port %u", portIndex);
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        ALOGV("Flushing %s port", pPort->GetDir() == OMX_DirInput ? "input" : "output");
        if ( portIndex == m_videoPortBase )
        {
            // Input port
            if ( IsEncoderStarted() )
            {
                (void)SetInputPortState(OMX_StateIdle);
                (void)SetInputPortState(StateGet());
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

OMX_ERRORTYPE BOMX_VideoEncoder::CommandPortEnable(
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
            ALOGE("Invalid port %u", portIndex);
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if ( pPort->IsEnabled() )
        {
            ALOGE("Port %u is already enabled", portIndex);
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
                    ALOGE("Timeout waiting for port %u to be populated", portIndex);
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

OMX_ERRORTYPE BOMX_VideoEncoder::CommandPortDisable(
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
            ALOGE("Invalid port %u", portIndex);
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        if ( pPort->IsDisabled() )
        {
            ALOGE("Port %u is already disabled", portIndex);
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
            // Now we need to wait for all enabled ports to become de-populated
            while ( !pPort->IsEmpty() )
            {
                if ( PortWait() != B_ERROR_SUCCESS )
                {
                    ALOGE("Timeout waiting for port %u to be de-populated", portIndex);
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

OMX_ERRORTYPE BOMX_VideoEncoder::AddInputPortBuffer(
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes,
    OMX_IN OMX_U8* pBuffer,
    bool componentAllocated)
{
    BOMX_Port *pPort;
    BOMX_VideoEncoderInputBufferInfo *pInfo;
    OMX_ERRORTYPE err;
    NEXUS_Error errCode;

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
        ALOGE("Cannot add buffers to a tunneled port");
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }

    pInfo = new BOMX_VideoEncoderInputBufferInfo;
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->type = m_inputMode;

    if (pInfo->type != BOMX_VideoEncoderInputBufferType_eStandard)
    {
        ALOGE("Unsupported buffer type");
        delete pInfo;
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    /* Check buffer size */
    const OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = pPort->GetDefinition();
    if ( nSizeBytes < ComputeInputBufferSize(pPortDef->format.video.nStride, pPortDef->format.video.nSliceHeight) )
    {
        ALOGE("Outbuffer size is not sufficient - must be at least %u bytes ((3*%u*%u)/2) got %u [eColorFormat %#x]", (unsigned int)ComputeInputBufferSize(pPortDef->format.video.nStride, pPortDef->format.video.nSliceHeight),
              pPortDef->format.video.nStride, pPortDef->format.video.nSliceHeight, nSizeBytes, pPortDef->format.video.eColorFormat);
        delete pInfo;
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    if ( componentAllocated )
    {
        // allocated buffer
        pInfo->typeInfo.standard.pAllocatedBuffer = pBuffer;
    }

    err = pPort->AddBuffer(ppBufferHdr, pAppPrivate, nSizeBytes, pBuffer, pInfo, componentAllocated);
    if ( OMX_ErrorNone != err )
    {
        delete pInfo;
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    PortStatusChanged();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_VideoEncoder::AddInputPortBuffer(
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    private_handle_t *pPrivateHandle)
{
    BOMX_Port *pPort;
    BOMX_VideoEncoderInputBufferInfo *pInfo;
    OMX_ERRORTYPE err;

    if ( NULL == ppBufferHdr || NULL == pPrivateHandle )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    if ( m_inputMode != BOMX_VideoEncoderInputBufferType_eNative )
    {
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
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

    pInfo = new BOMX_VideoEncoderInputBufferInfo;
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->type = BOMX_VideoEncoderInputBufferType_eNative;
    pInfo->typeInfo.native.pPrivateHandle = pPrivateHandle;

    err = pPort->AddBuffer(ppBufferHdr, pAppPrivate, ComputeInputBufferSize(pPort->GetDefinition()->format.video.nStride, pPort->GetDefinition()->format.video.nSliceHeight), (OMX_U8 *)pPrivateHandle, pInfo, false);
    if ( OMX_ErrorNone != err )
    {
        delete pInfo;
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    PortStatusChanged();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_VideoEncoder::AddInputPortBuffer(
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    VideoDecoderOutputMetaData *pMetadata)
{
    BOMX_Port *pPort;
    BOMX_VideoEncoderInputBufferInfo *pInfo;
    OMX_ERRORTYPE err;
    void *pMemory;

    if ( NULL == ppBufferHdr || NULL == pMetadata )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    if ( m_inputMode != BOMX_VideoEncoderInputBufferType_eMetadata )
    {
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateOperation);
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

    pInfo = new BOMX_VideoEncoderInputBufferInfo;
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->type = BOMX_VideoEncoderInputBufferType_eMetadata;
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


OMX_ERRORTYPE BOMX_VideoEncoder::AddOutputPortBuffer(
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes,
    OMX_IN OMX_U8* pBuffer,
    bool componentAllocated)
{
    BOMX_Port *pPort;
    BOMX_VideoEncoderOutputBufferInfo *pInfo;
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
        ALOGE("Cannot add buffers to a tunneled port");
        return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
    }

    pInfo = new BOMX_VideoEncoderOutputBufferInfo;
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }
    memset(pInfo, 0, sizeof(*pInfo));

    if ( componentAllocated )
    {
        // allocated buffer
        pInfo->type = BOMX_VideoEncoderOutputBufferType_eAllocatedBuffer;
    }

    ALOGD("add output buffer size:%d, buffer:%p, allocated:%d", nSizeBytes, pBuffer, componentAllocated);

    err = pPort->AddBuffer(ppBufferHdr, pAppPrivate, nSizeBytes, pBuffer, pInfo, componentAllocated);
    if ( OMX_ErrorNone != err )
    {
        delete pInfo;
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }

    PortStatusChanged();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_VideoEncoder::UseBuffer(
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
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    else
    {
        err = AddOutputPortBuffer(ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes, pBuffer, false);
        if ( err != OMX_ErrorNone )
        {
            return BOMX_ERR_TRACE(err);
        }
    }

    return OMX_ErrorNone;
}


OMX_ERRORTYPE BOMX_VideoEncoder::AllocateBuffer(
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes)
{
    OMX_ERRORTYPE err;
    NEXUS_Error errCode;
    void *pBuffer;

    if ( NULL == ppBuffer )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    if ( nPortIndex == m_videoPortBase )
    {
        // Figure out input buffer mode from SetParameter calls.
        // You must do this here because the component base class state checks
        // will ensure this is valid (only port disabled or loaded state)
        // and if you try and do it in the command handler there is a possible
        // race condition where the component thread is scheduled later
        // than the call to EnablePort or CommandStateSet and this will
        // come first.  It's always safe to do it here.
        if ( m_metadataEnabled )
        {
            ALOGV("AllocateBuffer: Selecting metadata input mode for input buffer");
            m_inputMode = BOMX_VideoEncoderInputBufferType_eMetadata;
        }
        else if ( m_nativeGraphicsEnabled )
        {
            ALOGV("AllocateBuffer: Selecting native graphics input mode for input buffer");
            m_inputMode = BOMX_VideoEncoderInputBufferType_eNative;
        }
        else
        {
            ALOGV("AllocateBuffer: Selecting standard buffer input mode for input buffer");
            m_inputMode = BOMX_VideoEncoderInputBufferType_eStandard;
        }
        switch ( m_inputMode )
        {
        case BOMX_VideoEncoderInputBufferType_eStandard:
            errCode = AllocateInputBuffer(nSizeBytes, pBuffer);
            ALOGV("AllocateBuffer: allocate input buffer (%p - %u) from Nexus heap", pBuffer, nSizeBytes);
            if ( errCode )
            {
                ALOGE("failed to allocate buffer from Nexus heap");
                return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
            }
            err = AddInputPortBuffer(ppBuffer, nPortIndex, pAppPrivate, nSizeBytes, (OMX_U8 *)pBuffer, true);
            if ( err != OMX_ErrorNone )
            {
                FreeInputBuffer(pBuffer);
                return BOMX_ERR_TRACE(err);
            }
            break;
        case BOMX_VideoEncoderInputBufferType_eNative:
        {
            pBuffer = malloc(nSizeBytes);
            ALOGV("AllocateBuffer: allocate eNatvie input buffer (%p - %u) from system", pBuffer, nSizeBytes);
            if ( pBuffer == NULL )
            {
                ALOGE("malloc failed");
                return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
            }
            err = AddInputPortBuffer(ppBuffer, nPortIndex, pAppPrivate, (private_handle_t *)pBuffer);
            if ( err != OMX_ErrorNone )
            {
                return BOMX_ERR_TRACE(err);
            }
        }
        break;
        case BOMX_VideoEncoderInputBufferType_eMetadata:
        {
            pBuffer = malloc(nSizeBytes);
            ALOGV("AllocateBuffer: allocate eMetadata input buffer (%p - %u) from system", pBuffer, nSizeBytes);
            if ( pBuffer == NULL )
            {
                ALOGE("malloc failed");
                return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
            }
            err = AddInputPortBuffer(ppBuffer, nPortIndex, pAppPrivate, (VideoDecoderOutputMetaData *)pBuffer);
            if ( err != OMX_ErrorNone )
            {
                return BOMX_ERR_TRACE(err);
            }
        }
        break;
        default:
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
    }
    else
    {
        pBuffer = malloc(nSizeBytes);
        ALOGV("AllocateBuffer: allocate output buffer (%p) from system", pBuffer);
        if ( pBuffer == NULL )
        {
            ALOGE("malloc failed");
            return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
        }
        err = AddOutputPortBuffer(ppBuffer, nPortIndex, pAppPrivate, nSizeBytes, (OMX_U8 *)pBuffer, true);
        if ( err != OMX_ErrorNone )
        {
            free(pBuffer);
            return BOMX_ERR_TRACE(err);
        }
    }


    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_VideoEncoder::FreeBuffer(
    OMX_IN  OMX_U32 nPortIndex,
    OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    BOMX_Port *pPort;
    BOMX_Buffer *pBuffer;
    OMX_ERRORTYPE err;
    void* pBuff = NULL;

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
        BOMX_VideoEncoderInputBufferInfo *pInfo;
        pInfo = (BOMX_VideoEncoderInputBufferInfo *)pBuffer->GetComponentPrivate();

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
        case BOMX_VideoEncoderInputBufferType_eStandard:
            pBuff = pInfo->typeInfo.standard.pAllocatedBuffer;
            // free allocated buffer
            if (pBuff)
            {
                FreeInputBuffer(pBuff);
            }
            break;
        case BOMX_VideoEncoderInputBufferType_eNative:
        case BOMX_VideoEncoderInputBufferType_eMetadata:
            pBuff = pBufferHeader->pBuffer;
            // free allocated buffer
            if (pBuff)
            {
                free(pBuff);
            }
            break;
        default:
            break;
        }

        delete pInfo;

    }
    else
    {
        BOMX_VideoEncoderOutputBufferInfo *pInfo;
        pInfo = (BOMX_VideoEncoderOutputBufferInfo *)pBuffer->GetComponentPrivate();
        pBuff = pBufferHeader->pBuffer;
        if ( NULL == pInfo )
        {
            return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
        }
        err = pPort->RemoveBuffer(pBufferHeader);
        if ( err != OMX_ErrorNone )
        {
            return BOMX_ERR_TRACE(err);
        }

        if (pInfo->type == BOMX_VideoEncoderOutputBufferType_eAllocatedBuffer)
        {
            free(pBuff);
        }

        delete pInfo;
    }
    PortStatusChanged();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_VideoEncoder::EmptyThisBuffer(
    OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    OMX_ERRORTYPE err;
    BOMX_Buffer *pBuffer;
    BOMX_VideoEncoderInputBufferInfo *pInfo;

    size_t payloadLen;
    void *pPayload;
    NEXUS_Error errCode;
    size_t numConsumed, numRequested;
    uint8_t *pCodecHeader = NULL;
    size_t codecHeaderLength = 0;

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
        ALOGE("Invalid buffer");
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    pInfo = (BOMX_VideoEncoderInputBufferInfo *)pBuffer->GetComponentPrivate();
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    ALOGV("comp:%s, buff:%p len:%d ts:%llu flags:%x", GetName(), pBufferHeader->pBuffer, pBufferHeader->nFilledLen, pBufferHeader->nTimeStamp, pBufferHeader->nFlags);

    if(pBufferHeader->nFlags & ( OMX_BUFFERFLAG_DATACORRUPT | OMX_BUFFERFLAG_EXTRADATA | OMX_BUFFERFLAG_CODECCONFIG ))
    {
        ALOGE("Flags :%d is not handled ", pBufferHeader->nFlags);
    }

    // Track the buffer
    if ( !m_pBufferTracker->Add(pBufferHeader, &pInfo->pts) )
    {
        ALOGW("Unable to track buffer");
        pInfo->pts = BOMX_TickToPts(&pBufferHeader->nTimeStamp);
    }

    ALOGV("PTS: ts:%llu - pts:%lu", pBufferHeader->nTimeStamp, pInfo->pts);

    /* queue the buffer */
    err = m_pVideoPorts[0]->QueueBuffer(pBufferHeader);
    if ( err )
    {
        return BOMX_ERR_TRACE(err);
    }

    /* notify input buffer process event handler */
    B_Event_Set(m_hInputBufferProcessEvent);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_VideoEncoder::FillThisBuffer(
    OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHeader)
{
    BOMX_Buffer *pBuffer;
    BOMX_VideoEncoderOutputBufferInfo *pInfo;
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
    pInfo = (BOMX_VideoEncoderOutputBufferInfo *)pBuffer->GetComponentPrivate();
    if ( NULL == pInfo )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    pBuffer->Reset();
    err = m_pVideoPorts[1]->QueueBuffer(pBufferHeader);
    ALOG_ASSERT(err == OMX_ErrorNone);

    // Signal thread to look for more
    B_Event_Set(m_hOutputBufferProcessEvent);

    return OMX_ErrorNone;
}

NEXUS_Error BOMX_VideoEncoder::PushInputEosFrame()
{
    NEXUS_VideoImageInputSurfaceSettings surfSettings;

    // NEXUS: EOS mush be pushed with NULL frame
    NEXUS_VideoImageInput_GetDefaultSurfaceSettings(&surfSettings);
    surfSettings.endOfStream = true;
    return NEXUS_VideoImageInput_PushSurface(m_hImageInput, NULL, &surfSettings);
}

OMX_ERRORTYPE BOMX_VideoEncoder::BuildInputFrame(OMX_BUFFERHEADERTYPE *pBufferHeader)
{
    BOMX_Buffer *pBuffer;
    BOMX_VideoEncoderInputBufferInfo *pInfo;
    NEXUS_Error nerr;
    OMX_ERRORTYPE oerr;

    pBuffer = BOMX_BUFFERHEADER_TO_BUFFER(pBufferHeader);
    ALOG_ASSERT(NULL != pBuffer);
    pInfo = (BOMX_VideoEncoderInputBufferInfo *)pBuffer->GetComponentPrivate();
    ALOG_ASSERT(NULL != pInfo);

    if (pBufferHeader->nFilledLen)
    {
        BOMX_ImageSurfaceNode *pNode;
        NEXUS_VideoImageInputSurfaceSettings surfSettings;
        const OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = m_pVideoPorts[0]->GetDefinition();

        /* allocate an image input surface */
        pNode = BLST_Q_FIRST(&m_ImageSurfaceFreeList);
        if (NULL == pNode)
        {
            ALOGV("no image surface is available");
            return OMX_ErrorInsufficientResources;
        }

        NEXUS_VideoImageInput_GetDefaultSurfaceSettings(&surfSettings);

        surfSettings.pts = pInfo->pts;
        surfSettings.ptsValid = true;
        surfSettings.frameRate = MapOMXFrameRateToNexus(pPortDef->format.video.xFramerate);
        if (surfSettings.frameRate == NEXUS_VideoFrameRate_eUnknown)
        {
            surfSettings.frameRate = B_DEFAULT_INPUT_NEXUS_FRAMERATE;
        }

        ALOGV("Push surface setting: pts:%d, frameRate:%g", surfSettings.pts, BOMX_NexusFramerateValue(surfSettings.frameRate));

        /* do color format conversion */
        if (!ConvertOMXPixelFormatToCrYCbY(pBufferHeader, pNode->hSurface))
        {
            ALOGE("Error converting colour format");
            return BOMX_ERR_TRACE(OMX_ErrorUndefined);
        }

        ALOGV("Push surface:%p PTS = %d", pNode->hSurface, pInfo->pts);
        nerr = NEXUS_VideoImageInput_PushSurface(m_hImageInput, pNode->hSurface, &surfSettings);
        if (nerr)
        {
            ALOGE("Error Push Surface. err = %d", nerr);
            return BOMX_ERR_TRACE(OMX_ErrorUndefined);
        }
        // add the image input surface to pushed list
        BLST_Q_REMOVE_HEAD(&m_ImageSurfaceFreeList, node);
        ALOG_ASSERT(m_nImageSurfaceFreeListLen > 0);
        m_nImageSurfaceFreeListLen--;
        BLST_Q_INSERT_TAIL(&m_ImageSurfacePushedList, pNode, node);
        m_nImageSurfacePushedListLen++;
    }

    // handle EOS
    if (pBufferHeader->nFlags & OMX_BUFFERFLAG_EOS)
    {
        ALOGD("input EOS received");
        if (NEXUS_SUCCESS != PushInputEosFrame())
        {
            m_bPushEos = true;
        }
    }

    ReturnPortBuffer(m_pVideoPorts[0], pBuffer);

    return OMX_ErrorNone;
}


void BOMX_VideoEncoder::ReturnInputBuffers(bool returnAll)
{
    BOMX_Buffer *pBuffer;
    uint32_t count = 0;

    uint32_t buffersToReturn =
        returnAll ? m_pVideoPorts[0]->GetDefinition()->nBufferCountActual : 1;

    while ( (pBuffer = m_pVideoPorts[0]->GetBuffer()) != NULL )
    {
        BOMX_VideoEncoderInputBufferInfo *pInfo =
            (BOMX_VideoEncoderInputBufferInfo *)pBuffer->GetComponentPrivate();
        OMX_BUFFERHEADERTYPE *pBufferHeader = pBuffer->GetHeader();

        ALOG_ASSERT(NULL != pInfo);
        ALOG_ASSERT(NULL != pBufferHeader);

        ReturnPortBuffer(m_pVideoPorts[0], pBuffer);
    }

    ALOGV("returned %u buffers, %u attempted", count, buffersToReturn);
}

void BOMX_VideoEncoder::ResetEncodedFrameList()
{
    BOMX_NexusEncodedVideoFrame *pNxVidEncFr, *pNxVidEncFrNext=NULL;

    ALOGV("Reset encoded frame list");

    for ( pNxVidEncFr = BLST_Q_FIRST(&m_EncodedFrList);
            NULL != pNxVidEncFr;
            pNxVidEncFr = pNxVidEncFrNext )
    {
        pNxVidEncFrNext = BLST_Q_NEXT(pNxVidEncFr, link);
        pNxVidEncFr->frameData->clear();
        ALOGV("Invalidating frame buffer %#x ", pNxVidEncFr);
        BLST_Q_REMOVE(&m_EncodedFrList, pNxVidEncFr, link);
        m_EncodedFrListLen--;
        BLST_Q_INSERT_TAIL(&m_EmptyFrList, pNxVidEncFr, link);
        m_EmptyFrListLen++;
    }
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
    {NULL, 0}
};

OMX_ERRORTYPE BOMX_VideoEncoder::GetExtensionIndex(
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

    ALOGW("Extension %s is not supported", cParameterName);
    return OMX_ErrorUnsupportedIndex;
}

OMX_ERRORTYPE BOMX_VideoEncoder::GetConfig(
    OMX_IN  OMX_INDEXTYPE nIndex,
    OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    switch ( (int)nIndex )
    {
    case OMX_IndexConfigVideoBitrate:
    {
        OMX_VIDEO_CONFIG_BITRATETYPE *pConfig = (OMX_VIDEO_CONFIG_BITRATETYPE *)pComponentConfigStructure;
        ALOGV("GetConfig OMX_IndexConfigVideoBitrate");
        BOMX_STRUCT_VALIDATE(pConfig);

        if ( pConfig->nPortIndex != m_videoPortBase + 1 )
        {
            ALOGE("Unable to get bit rate: Can only get the output port's bit rate");
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }

        pConfig->nEncodeBitrate = m_sVideoBitrateParams.nTargetBitrate;
        return OMX_ErrorNone;
    }
    default:
    {
        ALOGE("Config index %#x is not supported", nIndex);
        return BOMX_ERR_TRACE(OMX_ErrorUnsupportedIndex);
    }
    }
}

OMX_ERRORTYPE BOMX_VideoEncoder::SetConfig(
    OMX_IN  OMX_INDEXTYPE nIndex,
    OMX_IN  OMX_PTR pComponentConfigStructure)
{
    switch ( (int)nIndex )
    {
    case OMX_IndexConfigVideoBitrate:
    {
        OMX_VIDEO_CONFIG_BITRATETYPE *pConfig = (OMX_VIDEO_CONFIG_BITRATETYPE *)pComponentConfigStructure;
        ALOGV("SetConfig OMX_IndexConfigVideoBitrate");
        BOMX_STRUCT_VALIDATE(pConfig);

        if ( pConfig->nPortIndex != m_videoPortBase + 1 )
        {
           ALOGE("Unable to change bit rate: Can only change the output port's bit rate");
           return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }

        m_sVideoBitrateParams.nTargetBitrate = pConfig->nEncodeBitrate;
        ALOGV("Set: bitrate = %d", m_sVideoBitrateParams.nTargetBitrate);

        NEXUS_Error rc = UpdateEncoderSettings();
        if ( rc )
        {
            LOGE("Failed to update encoder settings");
            return BOMX_ERR_TRACE(OMX_ErrorUnsupportedSetting);
        }

        return OMX_ErrorNone;
    }
    default:
    {
        ALOGE("Config index %#x is not supported", nIndex);
        return BOMX_ERR_TRACE(OMX_ErrorUnsupportedIndex);
    }
    }
}

void BOMX_VideoEncoder::InputBufferProcess()
{
    BOMX_Buffer *pBuffer;
    OMX_STATETYPE currentState;
    int nPushed = 0;

    /* skip pushing if it is in Pause state */
    GetState(&currentState);
    if ( OMX_StatePause == currentState)
    {
        ALOGV("OMX in Pause state: not push to HW encoder");
        return;
    }

    for (pBuffer = m_pVideoPorts[0]->GetBuffer(); (pBuffer != NULL);
            pBuffer = m_pVideoPorts[0]->GetNextBuffer(pBuffer))
    {
        BOMX_VideoEncoderInputBufferInfo *pInfo =
            (BOMX_VideoEncoderInputBufferInfo *)pBuffer->GetComponentPrivate();
        OMX_BUFFERHEADERTYPE *pBufferHeader = pBuffer->GetHeader();
        OMX_ERRORTYPE err;

        /* ignore the error. In this way, we won't get into busy loop
         * when encoder FIFO is full. Retry will be triggered after
         * surface is recycled.
         */
        err = BuildInputFrame(pBufferHeader);
        if ( OMX_ErrorNone == err  )
        {
            nPushed++;
        }
        else
        {
            break;
        }
    }

    // handle EOS
    if (m_bPushEos)
    {
        if (NEXUS_SUCCESS == PushInputEosFrame())
        {
            m_bPushEos = false;
        }
    }

    ALOGV(" %d buffer(s) pushed", nPushed);
}

void BOMX_VideoEncoder::PrintVideoEncoderStatus()
{
    NEXUS_SimpleEncoderStatus EncSts;
    NEXUS_SimpleEncoder_GetStatus(m_hSimpleEncoder,&EncSts);

    ALOGI("pBaseAddr:%p pMetaDataBuff:%p picturesReceived:%d picturesEncoded:%d picturesDroppedFRC:%d picturesDroppedHRD:%d picturesDroppedErrors:%d pictureIdLastEncoded:%d",
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

void BOMX_VideoEncoder::NotifyOutputPortSettingsChanged()
{
    OMX_PARAM_PORTDEFINITIONTYPE portDef;
    m_pVideoPorts[1]->GetDefinition(&portDef);
    OMX_CONFIG_RECTTYPE cropRect;
    // Send crop update and continue without reset
    BOMX_STRUCT_INIT(&cropRect);
    cropRect.nLeft = 0;
    cropRect.nTop = 0;
    cropRect.nWidth = portDef.format.video.nStride;
    cropRect.nHeight = portDef.format.video.nSliceHeight;

    ALOGV("send OMX_EventPortSettingsChanged event");
    if ( m_callbacks.EventHandler )
    {
        (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate, OMX_EventPortSettingsChanged, m_videoPortBase+1, OMX_IndexConfigCommonOutputCrop, &cropRect);
    }
}

void BOMX_VideoEncoder::OutputBufferProcess()
{
    NEXUS_Error errCode;
    unsigned numFrames=0;
    unsigned i;
    BOMX_NexusEncodedVideoFrame *pNxVidEncFr;

    /* check if encoder is started */
    if (!IsEncoderStarted())
    {
        ALOGE("Encoder Is Not Started");
        return;
    }

    /* check if we have client buffers ready */
    if ( m_pVideoPorts[1]->QueueDepth() == 0 )
    {
        ALOGD("no client buffer available");
        return;
    }

    /* lock video encoder buffer */
    VideoEncoderBufferBlock_Lock();

    /* log encoder status */
    PrintVideoEncoderStatus();

    /* retrieve frames from encoder */
    RetrieveFrameFromHardware();

    pNxVidEncFr = BLST_Q_FIRST(&m_EncodedFrList);

    while (pNxVidEncFr)
    {
        BOMX_Buffer *pOmxBuffer = m_pVideoPorts[1]->GetBuffer();

        if ( NULL == pOmxBuffer )
            break;

        OMX_BUFFERHEADERTYPE *pHeader = pOmxBuffer->GetHeader();
        BLST_Q_REMOVE_HEAD(&m_EncodedFrList, link);
        m_EncodedFrListLen--;

        BDBG_ASSERT(pNxVidEncFr->baseAddr);

        unsigned int NumVectors = pNxVidEncFr->frameData->size();
        unsigned int TotalLen = 0;
        for (unsigned int i = 0; i < NumVectors; i++)
        {
            NEXUS_VideoEncoderDescriptor * pVidEncOut = pNxVidEncFr->frameData->editItemAt(i);
            BDBG_ASSERT(pVidEncOut);
            TotalLen += pVidEncOut->length;
        }
        if (pNxVidEncFr->combinedSz != TotalLen)
        {
            ALOGE("Length mismatch in the frame descriptors %d != %d", pNxVidEncFr->combinedSz, TotalLen);
            VideoEncoderBufferBlock_Unlock();
            ResetEncodedFrameList();
            ReturnEncodedFrameSynchronized(pNxVidEncFr);
            /* notify the client */
            NotifyOutputPortSettingsChanged();
            return;
        }

        if (pNxVidEncFr->combinedSz > pHeader->nAllocLen)
        {
            ALOGE("Provided Buffer Too Small To Hold The Encoded Video Data");
            VideoEncoderBufferBlock_Unlock();
            ResetEncodedFrameList();
            ReturnEncodedFrameSynchronized(pNxVidEncFr);
            /* notify the client */
            NotifyOutputPortSettingsChanged();
            return;
        }

        OMX_U8 *pDest = pHeader->pBuffer;
        for (unsigned int i = 0; i < NumVectors; i++)
        {
            NEXUS_VideoEncoderDescriptor * pVidEncOut = pNxVidEncFr->frameData->editItemAt(i);
            BDBG_ASSERT(pVidEncOut);

            BKNI_Memcpy(pDest,
                        (void *)(pNxVidEncFr->baseAddr + pVidEncOut->offset),
                        pVidEncOut->length);
            pDest += pVidEncOut->length;
        }

        ALOGV("PTS: Fr:%p - origin ts:%llu flags:%x",  pNxVidEncFr, pNxVidEncFr->usTimeStampOriginal, pNxVidEncFr->clientFlags);
        if (!(pNxVidEncFr->clientFlags & OMX_BUFFERFLAG_CODECCONFIG))
        {
            if ( !m_pBufferTracker->Remove(pNxVidEncFr->usTimeStampOriginal, pHeader) )
            {
                ALOGW("Unable to find tracker entry for pts %llu", pNxVidEncFr->usTimeStampOriginal);
                BOMX_PtsToTick(pNxVidEncFr->usTimeStampOriginal, &pHeader->nTimeStamp);
            }
            if ( (pHeader->nFlags & OMX_BUFFERFLAG_EOS) && !m_pBufferTracker->Last(pHeader->nTimeStamp) )
            {
                ALOGE("Received EOS, but timeStamp mis-matched");
            }
        }

        if (pNxVidEncFr->clientFlags & OMX_BUFFERFLAG_EOS)
        {
            ALOGV("EOS: EOS is delivered");
            m_bEosDelieverd = true;
        }

        pHeader->nFlags = pNxVidEncFr->clientFlags;
        pHeader->nFilledLen = pNxVidEncFr->combinedSz;

        /* return frame buffer */
        ReturnEncodedFrameSynchronized(pNxVidEncFr);

        /* return client buffer */
        ALOGV("Returning Port Buffer HDR %#x flags %#x ts %llu us ",
              pHeader, pHeader->nFlags, pHeader->nTimeStamp);
        ReturnPortBuffer(m_pVideoPorts[1], pOmxBuffer);

        /* advance to next frame */
        pNxVidEncFr = BLST_Q_FIRST(&m_EncodedFrList);
    }
    VideoEncoderBufferBlock_Unlock();
}


bool BOMX_VideoEncoder::IsEncoderStarted()
{
    return m_bSimpleEncoderStarted;
}

bool BOMX_VideoEncoder::ConvertOMXPixelFormatToCrYCbY(OMX_BUFFERHEADERTYPE *pInBufHdr, NEXUS_SurfaceHandle hDst)
{
    const OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = m_pVideoPorts[0]->GetDefinition();
    BOMX_Buffer *pBomxBuffer = BOMX_BUFFERHEADER_TO_BUFFER(pInBufHdr);
    BOMX_VideoEncoderInputBufferInfo *pInfo = (BOMX_VideoEncoderInputBufferInfo*)pBomxBuffer->GetComponentPrivate();
    bool bRet = true;

    if (pInfo->type == BOMX_VideoEncoderInputBufferType_eStandard)
    {
        uint8_t *pSrcBuf = (uint8_t *)pInBufHdr->pBuffer + pInBufHdr->nOffset;
        unsigned int width = pPortDef->format.video.nFrameWidth;
        unsigned int height = pPortDef->format.video.nFrameHeight;
        OMX_COLOR_FORMATTYPE colorFormat = pPortDef->format.video.eColorFormat;

        ALOGV("InputBufferType_eStandard: color:%d, width:%d, height:%d", colorFormat, width, height);

        /* only support this colour format */
        BDBG_ASSERT(colorFormat == OMX_COLOR_FormatYUV420Planar);
        if (NEXUS_SUCCESS != ExtractNexusBuffer(pSrcBuf, width, height, hDst))
        {
            bRet = false;
        }
    }
    else if ( pInfo->type == BOMX_VideoEncoderInputBufferType_eNative)
    {
        private_handle_t *pPrivateHandle = pInfo->typeInfo.native.pPrivateHandle;

        ALOGV("InputBufferType_eNative: pPrivateHandle:%p", pPrivateHandle);
        ALOGV("--->phandle:%p magic=%x, flags=%x, pid=%x, stride=%x, format=%x, size=%x, sharedata=%x, usage=%x, paddr=%x, saddr=%x, aligment=%x", pPrivateHandle,
              pPrivateHandle->magic,
              pPrivateHandle->flags,
              pPrivateHandle->pid,
              pPrivateHandle->oglStride,
              pPrivateHandle->oglFormat,
              pPrivateHandle->oglSize,
              pPrivateHandle->sharedData,
              pPrivateHandle->usage,
              pPrivateHandle->nxSurfacePhysicalAddress,
              pPrivateHandle->nxSurfaceAddress,
              pPrivateHandle->alignment);

        if (NEXUS_SUCCESS != ExtractGrallocBuffer(pPrivateHandle, hDst))
        {
            bRet = false;
        }
    }
    else if ( pInfo->type == BOMX_VideoEncoderInputBufferType_eMetadata)
    {
        void *pMemory;
        PSHARED_DATA pSharedData;
        private_handle_t *pPrivateHandle = (private_handle_t *)pInfo->typeInfo.metadata.pMetadata->pHandle;

        ALOGV("InputBufferType_eMetadata: type:%d, handle:%p", pInfo->typeInfo.metadata.pMetadata->eType, pPrivateHandle);
        if ( pInfo->typeInfo.metadata.pMetadata->eType != kMetadataBufferTypeGrallocSource )
        {
            ALOGW("Only kMetadataBufferTypeGrallocSource buffers are supported in metadata mode.");
            return false;
        }

        ALOGV("--->phandle:%p magic=%x, flags=%x, pid=%x, stride=%x, format=%x, size=%x, sharedata=%x, usage=%x, paddr=%x, saddr=%x, aligment=%x", pPrivateHandle,
              pPrivateHandle->magic,
              pPrivateHandle->flags,
              pPrivateHandle->pid,
              pPrivateHandle->oglStride,
              pPrivateHandle->oglFormat,
              pPrivateHandle->oglSize,
              pPrivateHandle->sharedData,
              pPrivateHandle->usage,
              pPrivateHandle->nxSurfacePhysicalAddress,
              pPrivateHandle->nxSurfaceAddress,
              pPrivateHandle->alignment);

        if (NEXUS_SUCCESS != ExtractGrallocBuffer(pPrivateHandle, hDst))
        {
            bRet = false;
        }
    }

    return bRet;
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

NEXUS_VideoCodecProfile BOMX_VideoEncoder::ConvertOMXProfileTypetoNexus(OMX_VIDEO_AVCPROFILETYPE profile)
{
    for (unsigned int i = 0; i < sizeof(ProfileMapTable)/sizeof(ProfileMapTable[0]); i++)
    {
        if (ProfileMapTable[i].omxProfile==profile)
            return ProfileMapTable[i].nexusProfile;
    }

    return NEXUS_VideoCodecProfile_eBaseline;
}

NEXUS_VideoCodecLevel BOMX_VideoEncoder::ConvertOMXLevelTypetoNexus(OMX_VIDEO_AVCLEVELTYPE level)
{
    for(unsigned int i = 0; i < sizeof(LevelMapTable)/sizeof(LevelMapTable[0]); i++)
    {
        if(LevelMapTable[i].omxLevel==level)
            return LevelMapTable[i].nexusLevel;
    }

    return NEXUS_VideoCodecLevel_e31;
}


void BOMX_VideoEncoder::StopEncoder(void)
{
    /* stop image input first */
    if (m_hSimpleVideoDecoder)
    {
        NEXUS_SimpleVideoDecoder_StopImageInput(m_hSimpleVideoDecoder);
    }

    /* then stop simple encoder */
    if (m_hSimpleEncoder)
    {
        NEXUS_SimpleEncoder_Stop(m_hSimpleEncoder);
    }

    /* set encoder stopped flag */
    m_bSimpleEncoderStarted = false;

    /* Return all input buffers */
    ReturnInputBuffers(true);

    /* flush buffer tracker */
    m_pBufferTracker->Flush();

    /* destroy image surfaces */
    DestroyImageSurfaces();

    if (m_ImageInputEventId) {
        UnregisterEvent(m_ImageInputEventId);
        m_ImageInputEventId = NULL;
    }
}

NEXUS_Error BOMX_VideoEncoder::StartOutput(void)
{
    NEXUS_Error rc;
    NEXUS_SimpleEncoderStartSettings encoderStartSettings;
    const OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = m_pVideoPorts[0]->GetDefinition();

    ALOGV("BOMX_VideoEncoder::StartOutput");
    rc = UpdateEncoderSettings();
    if ( rc )
    {
        LOGE("Failed to update encoder settings");
        return BOMX_BERR_TRACE(rc);
    }

    // const OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = m_pVideoPorts[1]->GetDefinition();
    NEXUS_SimpleEncoder_GetDefaultStartSettings(&encoderStartSettings);

    encoderStartSettings.input.video = m_hSimpleVideoDecoder;
    encoderStartSettings.output.video.settings.codec = GetNexusCodec();
    encoderStartSettings.output.transport.type = NEXUS_TransportType_eEs;
    encoderStartSettings.output.video.settings.bypassVideoProcessing = true;

    switch (GetNexusCodec())
    {
    case NEXUS_VideoCodec_eH264:
    {
        encoderStartSettings.output.video.settings.profile = ConvertOMXProfileTypetoNexus(m_sAvcVideoParams.eProfile);
        encoderStartSettings.output.video.settings.level = ConvertOMXLevelTypetoNexus(m_sAvcVideoParams.eLevel);
        encoderStartSettings.output.video.settings.nonRealTime = true;
        encoderStartSettings.output.video.settings.interlaced = false;
        break;
    }
    case NEXUS_VideoCodec_eVp8:
    {
        encoderStartSettings.output.video.settings.nonRealTime = true;
        encoderStartSettings.output.video.settings.interlaced = false;
        break;
    }
    default:
    {
        ALOGE("invalid code: %d", GetNexusCodec());
        return BOMX_BERR_TRACE(OMX_ErrorNotImplemented);
    }
    }

    /* setup encoder bounds to improve latency */
    encoderStartSettings.output.video.settings.bounds.outputFrameRate.min = B_MIN_FRAME_RATE;
    encoderStartSettings.output.video.settings.bounds.outputFrameRate.max = B_MAX_FRAME_RATE;
    encoderStartSettings.output.video.settings.bounds.inputFrameRate.min = B_MIN_FRAME_RATE;
    encoderStartSettings.output.video.settings.bounds.inputDimension.max.width = m_maxFrameWidth;
    encoderStartSettings.output.video.settings.bounds.inputDimension.max.height = m_maxFrameHeight;
    encoderStartSettings.output.video.settings.bounds.streamStructure.max.framesB = 0;
    encoderStartSettings.output.video.settings.rateBufferDelay = B_RATE_BUFFER_DELAY_MS;

    /* Setup output frame ready callback */
    encoderStartSettings.output.video.settings.dataReady.callback = BOMX_VideoEncoder_EventCallback;
    encoderStartSettings.output.video.settings.dataReady.context = m_hOutputBufferProcessEvent;
    encoderStartSettings.output.video.settings.dataReady.param = (int) BOMX_VideoEncoderEventType_eDataReady;

    ALOGV("Profile = %d, Level = %d, width = %d, height = %d",
          encoderStartSettings.output.video.settings.profile,
          encoderStartSettings.output.video.settings.level,
          pPortDef->format.video.nFrameWidth,
          pPortDef->format.video.nFrameHeight);

    rc = NEXUS_SimpleEncoder_Start(m_hSimpleEncoder, &encoderStartSettings);
    if (rc)
    {
        LOGE("Failed to start Nexus simple encoder");
        return BOMX_BERR_TRACE(rc);
    }

    /* reset the codec config flag */
    m_bCodecConfigDone = false;
    m_bEosDelieverd = false;
    m_bEosReceived = false;
    m_bPushEos = false;

    ALOGV("started Nexus encoder");

    return NEXUS_SUCCESS;
}

void BOMX_VideoEncoder::StopOutput()
{
    NEXUS_SimpleEncoder_Stop(m_hSimpleEncoder);
}

static void BOMX_VideoEncoder_NodeDestroy(BOMX_ImageSurfaceNode *pNode)
{
   if (pNode->hSurface != NULL) {
      NEXUS_Surface_Destroy(pNode->hSurface);
   }

   if (pNode->memBlkFd >= 0) {
      close(pNode->memBlkFd);
   }

   return;
}

static NEXUS_MemoryBlockHandle BOMX_VideoEncoder_AllocateMemoryBlk(size_t size, int *pMemBlkFd)
{
   NEXUS_MemoryBlockHandle hMemBlk = NULL;
   int memBlkFd = -1;
   char device[PROPERTY_VALUE_MAX];
   char name[PROPERTY_VALUE_MAX];

   if (pMemBlkFd) {
      memBlkFd = BOMX_VideoEncoder_OpenMemoryInterface();
      if (memBlkFd >= 0) {
         struct nx_ashmem_alloc ashmem_alloc;
         memset(&ashmem_alloc, 0, sizeof(struct nx_ashmem_alloc));
         ashmem_alloc.size  = size;
         ashmem_alloc.align = 4096;
         int ret = ioctl(memBlkFd, NX_ASHMEM_SET_SIZE, &ashmem_alloc);
         if (ret < 0) {
            close(memBlkFd);
            memBlkFd = -1;
         } else {
            hMemBlk = (NEXUS_MemoryBlockHandle)ioctl(memBlkFd, NX_ASHMEM_GETMEM);
            if (hMemBlk == NULL) {
               close(memBlkFd);
               memBlkFd = -1;
            }
         }
      }
      *pMemBlkFd = memBlkFd;
   }

   return hMemBlk;
}

NEXUS_SurfaceHandle BOMX_VideoEncoder::CreateSurface(
        int width, int height, int stride, NEXUS_PixelFormat format,
        unsigned handle, unsigned offset, void *pAddr, int *pMemBlkFd)
{
    NEXUS_SurfaceCreateSettings createSettings;

    NEXUS_Surface_GetDefaultCreateSettings(&createSettings);
    createSettings.pixelFormat   = format;
    createSettings.width         = width;
    createSettings.height        = height;
    createSettings.pitch         = stride;
    createSettings.managedAccess = false;

    if (!handle && !offset && !pAddr)
    {
       NEXUS_Error rc;
       NEXUS_ClientConfiguration clientConfig;
       NEXUS_VideoImageInputStatus imageInputStatus;
       NEXUS_HeapHandle surfaceHeap = NULL;
       bool dynHeap = false;

       NEXUS_Platform_GetClientConfiguration(&clientConfig);

       if (m_hImageInput == NULL)
       {
          return NULL;
       }

       rc = NEXUS_VideoImageInput_GetStatus(m_hImageInput, &imageInputStatus);
       if (rc)
       {
          BOMX_BERR_TRACE(rc);
          return NULL;
       }

       for (int i=0; i<NEXUS_MAX_HEAPS; i++) {
          NEXUS_MemoryStatus s;
          if (!clientConfig.heap[i] || NEXUS_Heap_GetStatus(clientConfig.heap[i], &s)) continue;
          if (!surfaceHeap && s.memcIndex == imageInputStatus.memcIndex && (s.memoryType & NEXUS_MemoryType_eApplication) && s.largestFreeBlock >= MIN_BLOCK_SIZE) {
             surfaceHeap = clientConfig.heap[i];
             ALOGI("found default heap[%d] on MEMC%d for VideoImageInput", i, s.memcIndex);
          }
          if (s.memcIndex == imageInputStatus.memcIndex && (s.memoryType & (NEXUS_MEMORY_TYPE_MANAGED|NEXUS_MEMORY_TYPE_ONDEMAND_MAPPED|NEXUS_MEMORY_TYPE_DYNAMIC))) {
            dynHeap = true;
            ALOGI("prefered dynamic heap[%d] on MEMC%d for VideoImageInput", i, s.memcIndex);
            break;
          }
       }

       if (!dynHeap && !surfaceHeap) {
          ALOGE("no heap found. RTS failure likely.");
          BOMX_BERR_TRACE(BERR_UNKNOWN);
          return NULL;
       }

       if (dynHeap && pMemBlkFd)
       {
          createSettings.pixelMemory = BOMX_VideoEncoder_AllocateMemoryBlk(
              createSettings.pitch * createSettings.height, pMemBlkFd);
          if (createSettings.pixelMemory == NULL)
          {
              ALOGE("Failed to create image input memory block");
              BOMX_BERR_TRACE(BERR_UNKNOWN);
              return NULL;
          }
          createSettings.pixelMemoryOffset = offset;
       }
       else
       {
          createSettings.heap = surfaceHeap;
       }
    }
    else if (handle)
    {
        createSettings.pixelMemory       = (NEXUS_MemoryBlockHandle) handle;
        createSettings.pixelMemoryOffset = offset;
    }
    else if (pAddr)
    {
        createSettings.pMemory = pAddr;
    }

    return NEXUS_Surface_Create(&createSettings);
}

NEXUS_Error BOMX_VideoEncoder::StartInput()
{
    NEXUS_Error rc;
    NEXUS_VideoImageInputSettings imageInputSetting;
    NEXUS_SimpleVideoDecoderStartSettings decoderStartSettings;

    /* create video decoder image input handle */
    NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(&decoderStartSettings);
    decoderStartSettings.lowDelayImageInput = false;    /* Low delay mode bypasses xdm display management */
    m_hImageInput = NEXUS_SimpleVideoDecoder_StartImageInput(m_hSimpleVideoDecoder, &decoderStartSettings);
    if ( NULL == m_hImageInput)
    {
        return BOMX_BERR_TRACE(BERR_UNKNOWN);
    }

    ALOGV("Image Input Started Successfully!");

    /* create surface on specified heap */
    const OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = m_pVideoPorts[0]->GetDefinition();

    ALOGV("creating surfaces: width=%d height=%d pixelFormat=%d",
          pPortDef->format.video.nFrameWidth, pPortDef->format.video.nFrameHeight, NEXUS_PixelFormat_eCr8_Y18_Cb8_Y08);

    BOMX_ImageSurfaceNode *pNode;
    for (unsigned int i = 0; i < VIDEO_ENCODE_IMAGEINPUT_DEPTH; i++)
    {
        pNode = new BOMX_ImageSurfaceNode;
        if ( NULL == pNode )
        {
            ALOGE("Failed to create pNode");
            return BOMX_BERR_TRACE(BERR_OUT_OF_SYSTEM_MEMORY);
        }
        pNode->memBlkFd = -1;
        pNode->hSurface = CreateSurface(
                              pPortDef->format.video.nFrameWidth,
                              pPortDef->format.video.nFrameHeight,
                              2 * pPortDef->format.video.nFrameWidth,
                              NEXUS_PixelFormat_eCr8_Y18_Cb8_Y08,
                              0,
                              0,
                              NULL,
                              &pNode->memBlkFd);
        if (NULL == pNode->hSurface)
        {
            ALOGE("Failed to create image input surface");
            BOMX_VideoEncoder_NodeDestroy(pNode);
            delete pNode;
            return BOMX_BERR_TRACE(BERR_UNKNOWN);
        }

        BLST_Q_INSERT_TAIL(&m_ImageSurfaceFreeList, pNode, node);
        m_nImageSurfaceFreeListLen++;

    }
    ALOGV("%d Nexus surfaces created", m_nImageSurfaceFreeListLen);

    /* add call back function */
    NEXUS_VideoImageInput_GetSettings(m_hImageInput, &imageInputSetting);
    imageInputSetting.imageCallback.callback = BOMX_VideoEncoder_EventCallback;
    imageInputSetting.imageCallback.context  = static_cast <void *>(m_hImageInputEvent);
    imageInputSetting.imageCallback.param = (int) BOMX_VideoEncoderEventType_eImageBuffer;
    rc = NEXUS_VideoImageInput_SetSettings(m_hImageInput, &imageInputSetting);
    if (rc)
    {
        ALOGE("Failed to add VideoImageInput callback function");
        return BOMX_BERR_TRACE(rc);
    }

    /* register image input event to scheduler */
    m_ImageInputEventId = RegisterEvent(m_hImageInputEvent, BOMX_VideoEncoder_ImageInputEvent, static_cast <void *> (this));
    if ( NULL == m_ImageInputEventId )
    {
        ALOGE("Failed to register ImageInput event");
        return BOMX_BERR_TRACE(BERR_UNKNOWN);
    }

    ALOGV("Image input event registered");

    return NEXUS_SUCCESS;
}

NEXUS_Error BOMX_VideoEncoder::AllocateEncoderResource()
{
    NEXUS_SimpleStcChannelSettings stcSettings;
    NEXUS_Error rc;

    /* request transcode (decoder/encoder pair) resource */
    NxClient_AllocSettings nxAllocSettings;
    NxClient_GetDefaultAllocSettings(&nxAllocSettings);
    nxAllocSettings.simpleVideoDecoder = 1;
    nxAllocSettings.simpleEncoder = 1;
    rc = NxClient_Alloc(&nxAllocSettings, &m_allocResults);
    if ( rc )
    {
        ALOGE("NxClient_Alloc failed");
        return BOMX_BERR_TRACE(rc);
    }

    NxClient_ConnectSettings connectSettings;
    NxClient_GetDefaultConnectSettings(&connectSettings);
    connectSettings.simpleVideoDecoder[0].id = m_allocResults.simpleVideoDecoder[0].id;
    connectSettings.simpleVideoDecoder[0].windowCapabilities.encoder = true;
    connectSettings.simpleVideoDecoder[0].decoderCapabilities.maxWidth = 720;
    connectSettings.simpleVideoDecoder[0].decoderCapabilities.maxHeight = 480;
    connectSettings.simpleVideoDecoder[0].decoderCapabilities.maxFormat = NEXUS_VideoFormat_eNtsc;
    connectSettings.simpleEncoder[0].id = m_allocResults.simpleEncoder[0].id;
    connectSettings.simpleEncoder[0].nonRealTime = true;
    connectSettings.simpleEncoder[0].audio.cpuAccessible = true;
    connectSettings.simpleEncoder[0].video.cpuAccessible = true;
    rc = NxClient_Connect(&connectSettings, &m_nxClientId);
    if ( rc )
    {
        ALOGE("NxClient_Connect failed.  Resources may be exhausted.");
        NxClient_Free(&m_allocResults);
        return BOMX_BERR_TRACE(rc);
    }

    /* acquire video decoder */
    m_hSimpleVideoDecoder = NEXUS_SimpleVideoDecoder_Acquire(m_allocResults.simpleVideoDecoder[0].id);
    if ( NULL == m_hSimpleVideoDecoder )
    {
        ALOGE("Failed to allocate Nexus simple video decoder");
        return BOMX_BERR_TRACE(BERR_UNKNOWN);
    }
    ALOGV("acquired video decoder");

    /* acquire video encoder */
    m_hSimpleEncoder = NEXUS_SimpleEncoder_Acquire(m_allocResults.simpleEncoder[0].id);
    if ( NULL == m_hSimpleEncoder )
    {
        ALOGE("Failed to allocate Nexus simple video encoder");
        return BOMX_BERR_TRACE(BERR_UNKNOWN);
    }
    ALOGV("acquired video encoder");

    /* create stc channel */
    m_hStcChannel = NEXUS_SimpleStcChannel_Create(NULL);
    if ( NULL == m_hStcChannel )
    {
        ALOGE("Failed to allocate Nexus stc channel");
        return BOMX_BERR_TRACE(BERR_UNKNOWN);
    }
    ALOGV("stc channel created");

    /* set video decoder stc channel */
    NEXUS_SimpleStcChannel_GetSettings(m_hStcChannel, &stcSettings);
    stcSettings.mode = NEXUS_StcChannelMode_eAuto;
    rc = NEXUS_SimpleStcChannel_SetSettings(m_hStcChannel, &stcSettings);
    if (rc)
    {
        ALOGE("Failed to change STC settings");
        return BOMX_BERR_TRACE(rc);
    }

    rc = NEXUS_SimpleVideoDecoder_SetStcChannel(m_hSimpleVideoDecoder, m_hStcChannel);
    if (rc)
    {
        ALOGE("Failed to set STC channel");
        return BOMX_BERR_TRACE(rc);
    }

    ALOGV("set STC channel");

    // Setup file to debug ITB descriptors if required
    if ( property_get_int32(B_PROPERTY_ITB_DESC_DEBUG, 0) )
    {
        time_t rawtime;
        struct tm * timeinfo;
        char fname[100];

        // Generate unique file name
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(fname, sizeof(fname), "/data/nxmedia/venc-%F_%H_%M_%S.csv", timeinfo);
        ALOGD("ITB descriptors output file:%s", fname);
        m_pITBDescDumpFile = fopen(fname, "w+");
        if ( m_pITBDescDumpFile )
        {
            fprintf(m_pITBDescDumpFile, "offset,length,flags,dts(90Khz),pts(90Khz),origPts(45Khz),escr(27Mhz),tpb,shr,videoFlags,stcSnapshot,dataUnitType\n");
        }
        else
        {
            ALOGW("Error creating ITB descriptors dump file %s (%s)", fname, strerror(errno));
            // Just keep going without debug
        }
    }
    return NEXUS_SUCCESS;
}

void BOMX_VideoEncoder::ReleaseEncoderResource()
{
    /* release Nexus stc channel */
    if (m_hStcChannel) {
        NEXUS_SimpleVideoDecoder_SetStcChannel(m_hSimpleVideoDecoder, NULL);
        NEXUS_SimpleStcChannel_Destroy(m_hStcChannel);
        m_hStcChannel = NULL;
    }

    /*  release simple video decoder */
    if (m_hSimpleVideoDecoder) {
        NEXUS_SimpleVideoDecoder_Release(m_hSimpleVideoDecoder);
        m_hSimpleVideoDecoder = NULL;
    }

    /* release simple video encoder */
    if (m_hSimpleEncoder) {
        NEXUS_SimpleEncoder_Release(m_hSimpleEncoder);
        m_hSimpleEncoder = NULL;
    }

    if (m_nxClientId != NXCLIENT_INVALID_ID)
    {
        NxClient_Disconnect(m_nxClientId);
        NxClient_Free(&m_allocResults);
        m_nxClientId = NXCLIENT_INVALID_ID;
    }

    if ( m_pITBDescDumpFile )
    {
        fclose(m_pITBDescDumpFile);
        m_pITBDescDumpFile = NULL;
    }
}

NEXUS_Error BOMX_VideoEncoder::StartEncoder()
{
    NEXUS_Error rc;

    rc = StartOutput();
    if (NEXUS_SUCCESS != rc)
    {
        ALOGE("Failed to start encoder - Output");
        return BOMX_BERR_TRACE(rc);
    }

    rc = StartInput();
    if (NEXUS_SUCCESS != rc)
    {
        ALOGE("Failed to start encoder - Input");
        return BOMX_BERR_TRACE(rc);
    }

    ALOGV("Started Encoder ");

    m_bSimpleEncoderStarted = true;
    return NEXUS_SUCCESS;
}

void BOMX_VideoEncoder::DestroyImageSurfaces()
{
    BOMX_ImageSurfaceNode *pNode;

    while ( (pNode = BLST_Q_FIRST(&m_ImageSurfaceFreeList)) )
    {
        BLST_Q_REMOVE_HEAD(&m_ImageSurfaceFreeList, node);
        BOMX_VideoEncoder_NodeDestroy(pNode);
        delete pNode;
    }
    m_nImageSurfaceFreeListLen = 0;

    while ( (pNode = BLST_Q_FIRST(&m_ImageSurfacePushedList)) )
    {
        BLST_Q_REMOVE_HEAD(&m_ImageSurfacePushedList, node);
        BOMX_VideoEncoder_NodeDestroy(pNode);
        delete pNode;
    }
    m_nImageSurfacePushedListLen = 0;
}


NEXUS_VideoFrameRate BOMX_VideoEncoder::MapOMXFrameRateToNexus(OMX_U32 omxFR)
{
    NEXUS_VideoFrameRate eFrameRate;

    /* Convert from Q16 fixed point format.
     * If framerate is not supported or doesn't exist it will return unknown.
     * List of supported framerates can be found in refsw.
     */
    switch ( omxFR )
    {
    case (OMX_U32)(Q16_SCALE_FACTOR * 23.976):
        eFrameRate = NEXUS_VideoFrameRate_e23_976;
        break;
    case (OMX_U32)(Q16_SCALE_FACTOR * 24.0):
        eFrameRate = NEXUS_VideoFrameRate_e24;
        break;
    case (OMX_U32)(Q16_SCALE_FACTOR * 25.0):
        eFrameRate = NEXUS_VideoFrameRate_e25;
        break;
    case (OMX_U32)(Q16_SCALE_FACTOR * 29.97):
        eFrameRate = NEXUS_VideoFrameRate_e29_97;
        break;
    case (OMX_U32)(Q16_SCALE_FACTOR * 30.0):
        eFrameRate = NEXUS_VideoFrameRate_e30;
        break;
    case (OMX_U32)(Q16_SCALE_FACTOR * 50.0):
        eFrameRate = NEXUS_VideoFrameRate_e50;
        break;
    case (OMX_U32)(Q16_SCALE_FACTOR * 59.94):
        eFrameRate = NEXUS_VideoFrameRate_e59_94;
        break;
    case (OMX_U32)(Q16_SCALE_FACTOR * 60.0):
        eFrameRate = NEXUS_VideoFrameRate_e60;
        break;
    case (OMX_U32)(Q16_SCALE_FACTOR * 14.985):
        eFrameRate = NEXUS_VideoFrameRate_e14_985;
        break;
    case (OMX_U32)(Q16_SCALE_FACTOR * 15.0):
        eFrameRate = NEXUS_VideoFrameRate_e15;
        break;
    case (OMX_U32)(Q16_SCALE_FACTOR * 20.0):
        eFrameRate = NEXUS_VideoFrameRate_e20;
        break;
    default:
        eFrameRate = NEXUS_VideoFrameRate_eUnknown;
        break;
    }
    return eFrameRate;
}

void BOMX_VideoEncoder::ImageBufferProcess()
{
    unsigned num_entries = 0;
    NEXUS_Error rc;
    const OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = m_pVideoPorts[0]->GetDefinition();
    unsigned count = pPortDef->nBufferCountActual;
    NEXUS_SurfaceHandle hFreeSurface[count];

    if (NULL == m_hImageInput)
    {
        ALOGE("ImageInput event received, but the ImageInput has been closed");
        return;
    }

    rc = NEXUS_VideoImageInput_RecycleSurface(m_hImageInput, hFreeSurface, count, &num_entries);
    if (rc)
    {
        ALOGE("ImageInput recycle surface return error: %d", rc);
        return;
    }

    ALOGV("ImageBufferProcess: %d image surfaces recycled", num_entries);

    for (unsigned i = 0; i < num_entries; i++)
    {
        BOMX_ImageSurfaceNode *pNode;
        pNode = BLST_Q_FIRST(&m_ImageSurfacePushedList);
        ALOG_ASSERT(NULL != pNode);

        ALOGV("pNode=%p, pNode->hSurface = %p, hFreeSurface=%p",  pNode, pNode->hSurface, hFreeSurface[i]);
        ALOG_ASSERT(hFreeSurface[i] == pNode->hSurface);

        // remove recycled image surface from pushed list and put it back to free list
        BLST_Q_REMOVE_HEAD(&m_ImageSurfacePushedList, node);
        ALOG_ASSERT(m_nImageSurfacePushedListLen > 0);
        m_nImageSurfacePushedListLen--;
        BLST_Q_INSERT_TAIL(&m_ImageSurfaceFreeList, pNode, node);
        m_nImageSurfaceFreeListLen++;

    }
    // retry if failed to push EOS, or input queue is not empty.
    if ((m_pVideoPorts[0]->QueueDepth() > 0) || m_bPushEos )
    {
        B_Event_Set(m_hInputBufferProcessEvent);
        ALOGV("trigger input event process for %s", m_bPushEos ? "EOS" : "remaining buffer");
    }

}


bool BOMX_VideoEncoder::ReturnEncodedFrameSynchronized(BOMX_NexusEncodedVideoFrame *pReturnFr)
{
    if (!pReturnFr)
    {
        ALOGE("Invalid Parameters");
        return false;
    }

    if (!IsEncoderStarted())
    {
        ALOGE("Encoder Is Not Started");
        return false;
    }

    unsigned int CntReturnFrame = pReturnFr->frameData->size();
    bool codeConfigFrame = pReturnFr->clientFlags & OMX_BUFFERFLAG_CODECCONFIG;
    BOMX_VIDEO_ENCODER_INIT_ENC_FR(pReturnFr);

    //The Vector should be empty now
    BDBG_ASSERT(0==pReturnFr->frameData->size());

    BLST_Q_INSERT_TAIL(&m_EmptyFrList, pReturnFr, link);
    m_EmptyFrListLen++;

    // We extract and deliver codec config data at the very beginning. This data comes in middle of
    // first encoded frame. We don't want to return codec config descriptors to hardware so that
    // we can read and deliver complete first frame in next cycle.
    if (codeConfigFrame)
    {
        ALOGV("Codec config descriptors. Not returning to hardware.");
        return true;
    }

    if (CntReturnFrame)
    {
        NEXUS_SimpleEncoder_VideoReadComplete(m_hSimpleEncoder,CntReturnFrame);

    } else {
        ALOGW("Returning A Frame With Zero Count [%d]",
              CntReturnFrame);

        BDBG_ASSERT(CntReturnFrame);
    }

    return true;
}

void BOMX_VideoEncoder::VideoEncoderBufferBlock_Lock()
{
    NEXUS_Error rc;
    void *pBufferBase;

    /* get encoder buffer base address */
    NEXUS_SimpleEncoder_GetStatus(m_hSimpleEncoder, &m_videoEncStatus);
    if (m_videoEncStatus.video.bufferBlock)
    {
        rc = NEXUS_MemoryBlock_Lock(m_videoEncStatus.video.bufferBlock, &pBufferBase);
        ALOG_ASSERT(!rc);
        m_videoEncStatus.video.pBufferBase = pBufferBase;
    }
}

void BOMX_VideoEncoder::VideoEncoderBufferBlock_Unlock()
{
    if (m_videoEncStatus.video.bufferBlock)
    {
        NEXUS_MemoryBlock_Unlock(m_videoEncStatus.video.bufferBlock);
        m_videoEncStatus.video.bufferBlock = NULL;
        m_videoEncStatus.video.pBufferBase = NULL;
    }
}

void * BOMX_VideoEncoder::VideoEncoderBufferBaseAddress()
{
    return (void *)m_videoEncStatus.video.pBufferBase;
}


#define ADVANCE_DESC() do { if ( size0 > 1 ) { pDesc0++; size0--; } else { pDesc0=pDesc1; size0=size1; pDesc1=NULL; size1=0; } } while (0)

bool BOMX_VideoEncoder::GetCodecConfig( const NEXUS_VideoEncoderDescriptor *pConstDesc0, size_t size0,
                                        const NEXUS_VideoEncoderDescriptor *pConstDesc1, size_t size1)
{
    unsigned int count;
    void *pBufferBase;
    BOMX_NexusEncodedVideoFrame *pEmptyFr;
    NEXUS_VideoEncoderDescriptor *pDescSps, *pDescPps;
    NEXUS_VideoEncoderDescriptor *pDesc0 = (NEXUS_VideoEncoderDescriptor *) pConstDesc0;
    NEXUS_VideoEncoderDescriptor *pDesc1 = (NEXUS_VideoEncoderDescriptor *) pConstDesc1;

    for (count=0; count < size0; count++)
    {
        ALOGV("Index: %d - Descriptor:%p - flags=%x, videoFlags = %x, length = %d, dataUnitType = %d",
              count, pDesc0, pDesc0->flags, pDesc0->videoFlags, pDesc0->length, pDesc0->dataUnitType);
        if (pDesc0->flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_FRAME_START)
        {
            break;
        }
        pDesc0++;
    }

    if ( count == size0 )
    {
        /* No start flag found, drop pre-start descriptors and retry */
        ALOGV("no frame start flag found. Drop %d descriptors:", count);
        NEXUS_SimpleEncoder_VideoReadComplete(m_hSimpleEncoder, count);
        return false;
    }

    /* now, we found the descriptor with frame start flag */
    /* drop pre-start descriptors */
    if ( 0 != count )
    {
        size0 -= count;
        ALOGV("Drop %d descriptors:", count);
        NEXUS_SimpleEncoder_VideoReadComplete(m_hSimpleEncoder, count);
    }

    /* skip SPS/PPS for VP8 */
    if (GetNexusCodec() == NEXUS_VideoCodec_eVp8)
    {
        ALOGV("Skipping SPS/PPS");
        return true;
    }

    ADVANCE_DESC();
    ALOG_ASSERT(pDesc0);
    ALOGV("Index: %d - Descriptor:%p - flags=%x, videoFlags = %x, length = %d, dataUnitType = %d",
          count+1, pDesc0, pDesc0->flags, pDesc0->videoFlags, pDesc0->length, pDesc0->dataUnitType);

    // SPS supposed to be next to the start descriptor
    if (GET_NAL_UNIT_TYPE(pDesc0->dataUnitType)!=NAL_UNIT_TYPE_SPS)
    {
        /* not valid frame - drop them and retry */
        ALOGW("SPS frame is found. Drop descriptor");
        NEXUS_SimpleEncoder_VideoReadComplete(m_hSimpleEncoder, 1);
        return false;
    }

    pDescSps = pDesc0;
    ADVANCE_DESC();

    ALOGV("Index: %d - Descriptor:%p - flags=%x, videoFlags = %x, length = %d, dataUnitType = %d",
          count+2, pDesc0,
          pDesc0 ? pDesc0->flags : 0,
          pDesc0 ? pDesc0->videoFlags : 0,
          pDesc0 ? pDesc0->length : 0,
          pDesc0 ? pDesc0->dataUnitType : 0);

    if ( (NULL == pDesc0) || (GET_NAL_UNIT_TYPE(pDesc0->dataUnitType) != NAL_UNIT_TYPE_PPS))
    {
        /* miss PPS packet, retry */
        ALOGV("PPS frame is found");
        return false;
    }

    pDescPps = pDesc0;

    /* get buffer base address */
    pBufferBase = VideoEncoderBufferBaseAddress();

    /* get free frame header */
    pEmptyFr = BLST_Q_FIRST(&m_EmptyFrList);
    BLST_Q_REMOVE_HEAD(&m_EmptyFrList, link);
    m_EmptyFrListLen--;
    BOMX_VIDEO_ENCODER_INIT_ENC_FR(pEmptyFr);

    ALOGD("Sending SPS");
    for (unsigned int cnt = 0; cnt < pDescSps->length; cnt+=4)
        ALOGD("SPS:%x %x %x %x", *((char*) pBufferBase+pDescSps->offset+cnt),
              *((char*) pBufferBase+pDescSps->offset+cnt+1),
              *((char*) pBufferBase+pDescSps->offset+cnt+2),
              *((char*) pBufferBase+pDescSps->offset+cnt+3));

    pEmptyFr->clientFlags |= OMX_BUFFERFLAG_CODECCONFIG;
    pEmptyFr->baseAddr = (unsigned int) pBufferBase;
    pEmptyFr->combinedSz += pDescSps->length;
    pEmptyFr->frameData->add((NEXUS_VideoEncoderDescriptor *) pDescSps);

    ALOGD("Sending PPS");
    for (unsigned int cnt = 0; cnt < pDescPps->length; cnt+=4)
        ALOGD("PPS:%x %x %x %x", *((char*) pBufferBase+pDescPps->offset+cnt),
              *((char*) pBufferBase+pDescPps->offset+cnt+1),
              *((char*) pBufferBase+pDescPps->offset+cnt+2),
              *((char*) pBufferBase+pDescPps->offset+cnt+3));

    pEmptyFr->combinedSz += pDescPps->length;
    pEmptyFr->frameData->add((NEXUS_VideoEncoderDescriptor *) pDescPps);

    /* queue the codec config frame for delivery */
    BLST_Q_INSERT_TAIL(&m_EncodedFrList, pEmptyFr, link);
    m_EncodedFrListLen++;
    ALOGV("Codec config frame queued. Encoded List Length:%d Empty List Length:%d",
          m_EncodedFrListLen, m_EmptyFrListLen);

    return true;
}

#define IS_START_OF_UNIT(pDesc) (((pDesc)->flags & (NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_FRAME_START|NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_EOS))?true:false)

bool BOMX_VideoEncoder::HaveCompleteFrame( const NEXUS_VideoEncoderDescriptor *pConstDesc0, size_t size0,
        const NEXUS_VideoEncoderDescriptor *pConstDesc1, size_t size1,
        size_t *pNumDesc)
{
    int count = 0;
    NEXUS_VideoEncoderDescriptor *pDesc0 = (NEXUS_VideoEncoderDescriptor *) pConstDesc0;
    NEXUS_VideoEncoderDescriptor *pDesc1 = (NEXUS_VideoEncoderDescriptor *) pConstDesc1;

    ALOGV("Index: %d - Descriptor:%p - flags=%x, videoFlags = %x, length = %d, dataUnitType = %d",
          count++, pDesc0, pDesc0->flags, pDesc0->videoFlags, pDesc0->length, pDesc0->dataUnitType);

    *pNumDesc=0;
    if ( IS_START_OF_UNIT(pDesc0) )
    {
        /* We have a start of frame or data unit */
        *pNumDesc=*pNumDesc+1;
        pDesc0++;
        size0--;
        /* Look for next one */
        while ( size0 > 0 )
        {
            ALOGV("Index: %d - Descriptor:%p - flags=%x, videoFlags = %x, length = %d, dataUnitType = %d",
                  count++, pDesc0, pDesc0->flags, pDesc0->videoFlags, pDesc0->length, pDesc0->dataUnitType);
            if ( IS_START_OF_UNIT(pDesc0) )
            {
                if (pDesc0->flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_EOS)
                {
                    ALOGI("EOS: received EOS on encoder output");
                    m_bEosReceived = true;
                }
                return true;
            }
            *pNumDesc=*pNumDesc+1;
            pDesc0++;
            size0--;
        }
        while ( size1 > 0 )
        {
            ALOGV("Index: %d - Descriptor:%p - flags=%x, videoFlags = %x, length = %d, dataUnitType = %d",
                  count++, pDesc1, pDesc1->flags, pDesc1->videoFlags, pDesc1->length, pDesc1->dataUnitType);
            if ( IS_START_OF_UNIT(pDesc1) )
            {
                if (pDesc1->flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_EOS)
                {
                    ALOGI("EOS: received EOS on encoder output");
                    m_bEosReceived = true;
                }
                return true;
            }
            *pNumDesc=*pNumDesc+1;
            pDesc1++;
            size1--;
        }
    }
    *pNumDesc=0;
    return false;
}

unsigned int BOMX_VideoEncoder::RetrieveFrameFromHardware()
{
    const NEXUS_VideoEncoderDescriptor *pDesc0=NULL, *pDesc1=NULL;
    size_t  size0=0,size1=0;
    void *pBufferBase;
    unsigned int framesRetrived=0;
    NEXUS_Error NxErrCode = NEXUS_SUCCESS;

    BOMX_NexusEncodedVideoFrame *pEmptyFr, *pEncodedFr;

    /* skip pulling if encoder is not started */
    if (!IsEncoderStarted())
    {
        ALOGW("Skip pulling new frame as encoder is not started");
        return 0;
    }

    /* skip pulling if there are frames queued for delivery */
    pEncodedFr = BLST_Q_FIRST(&m_EncodedFrList);
    if (pEncodedFr)
    {
        ALOGV("Skip pulling new frame as there are frames queued for delivery");
        return 0;
    }

    /* assert if no frame header available
     * This should never happen as no frame is queued
     * if happen, it means we are leaking frame header
     */
    pEmptyFr = BLST_Q_FIRST(&m_EmptyFrList);
    ALOG_ASSERT(pEmptyFr);

    /* get encoder buffer base address */
    /* Nothing we can do if the base address is NULL */
    pBufferBase = VideoEncoderBufferBaseAddress();
    ALOG_ASSERT(pBufferBase);

    NxErrCode = NEXUS_SimpleEncoder_GetVideoBuffer(m_hSimpleEncoder,
                &pDesc0,
                &size0,
                &pDesc1,
                &size1);

    ALOGV("pDesc0 = %p, size0 = %d, pDesc1 = %p, size1 = %d", pDesc0, size0, pDesc1, size1);

    if ( (NxErrCode != NEXUS_SUCCESS) || (0 == size0 + size1) || (0 == size0))
    {
        ALOGV("No Encoded Frames From Encoder Sts:%d Sz0:%d Sz1:%d",
              NxErrCode,size0,size0);
        return 0;
    }

    // get codec config first
    if (!m_bCodecConfigDone)
    {
        if (GetCodecConfig(pDesc0, size0, pDesc1, size1))
        {
            // Codec config flag is expected to be set only once after start of encode.
            m_bCodecConfigDone = true;
            return 1;
        }
        // retry at next round.
        return 0;
    }
#ifdef ADVANCE_DESC
#undef ADVANCE_DESC
#define ADVANCE_DESC() do { if ( size0 > 1 ) { pDesc0++; size0--; } else { pDesc0=pDesc1; size0=size1; pDesc1=NULL; size1=0; } } while (0)
#endif
    while (size0 > 0)
    {
        size_t numToProcess;
        if (!HaveCompleteFrame(pDesc0, size0, pDesc1, size1, &numToProcess))
        {
            ALOGV("in-completed frame, retry");
            break;
        }

        if ( pDesc0->flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_FRAME_START )
        {
            /* queue the frame for delivering to client */

            // get frame header
            pEmptyFr = BLST_Q_FIRST(&m_EmptyFrList);
            if (NULL == pEmptyFr)
            {
                ALOGE("Ran out of frame header. FLL:%d DLL:%d==", m_EmptyFrListLen, m_EncodedFrListLen);
                break;
            }
            BLST_Q_REMOVE_HEAD(&m_EmptyFrList, link);
            m_EmptyFrListLen--;
            BOMX_VIDEO_ENCODER_INIT_ENC_FR(pEmptyFr);

            while ( numToProcess > 0 )
            {
                ALOG_ASSERT(pDesc0);
                ALOGV("FRAME: %p - add descriptor: %p flag: %x PTS: %u", pEmptyFr, pDesc0, pDesc0->flags, pDesc0->originalPts);
                if ( m_pITBDescDumpFile )
                {
                    /* offset,length,flags,dts(90Khz),pts(90Khz),origPts(45Khz),escr(27Mhz),tpb,shr,videoFlags,stcSnapshot,dataUnitType */
                    fprintf(m_pITBDescDumpFile, "%u,%u,0x%08"PRIx32",%"PRIu64",%"PRIu64",%"PRIu32",%"PRIu32",%"PRIu16",%"PRIi16",0x%08"PRIx32",%"PRIu64",%"PRIu8"\n",
                            pDesc0->offset, pDesc0->length, pDesc0->flags, pDesc0->dts, pDesc0->pts, pDesc0->originalPts, pDesc0->escr, pDesc0->ticksPerBit, pDesc0->shr, pDesc0->videoFlags, pDesc0->stcSnapshot, pDesc0->dataUnitType);
                }
                pEmptyFr->combinedSz += pDesc0->length;
                pEmptyFr->frameData->add((NEXUS_VideoEncoderDescriptor *) pDesc0);
                if ( pDesc0->flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_ORIGINALPTS_VALID )
                {
                    pEmptyFr->usTimeStampOriginal = (unsigned long long) pDesc0->originalPts;
                }
                if ( pDesc0->flags & NEXUS_VIDEOENCODERDESCRIPTOR_FLAG_PTS_VALID )
                {
                    pEmptyFr->usTimeStampIntepolated = pDesc0->pts;
                }
                if ( pDesc0->videoFlags & NEXUS_VIDEOENCODERDESCRIPTOR_VIDEOFLAG_RAP )
                {
                    /* frames with random access point (RAP) flag are sync frames */
                    pEmptyFr->clientFlags |= OMX_BUFFERFLAG_SYNCFRAME;
                }
                numToProcess--;
                ADVANCE_DESC();
            }

            pEmptyFr->baseAddr = (unsigned int) pBufferBase;
            if (m_bEosReceived)
            {
                pEmptyFr->clientFlags |= OMX_BUFFERFLAG_EOS;
            }

            ALOGV("FRAME: %p - clientFlags: %x, size:%lu orig TS: %llu, intep TS: %llu",
                  pEmptyFr, pEmptyFr->clientFlags, pEmptyFr->combinedSz, pEmptyFr->usTimeStampOriginal, pEmptyFr->usTimeStampIntepolated);
            BLST_Q_INSERT_TAIL(&m_EncodedFrList, pEmptyFr, link);
            m_EncodedFrListLen++;

            ALOGV("Frame %p queued.  Encoded list length:%d Empty list length:%d",
                  pEmptyFr, m_EncodedFrListLen, m_EmptyFrListLen);
            framesRetrived++;
            pEmptyFr=NULL;
        }
        else
        {
            // should never be here
            ALOGE("something wrong - pDesc:%p: flags:%x, videoFlags = %x, length = %d, dataUnitType = %d",
                  pDesc0, pDesc0->flags, pDesc0->videoFlags, pDesc0->length, pDesc0->dataUnitType);
            break;
        }
    }
    ALOGV("Frames Retrieved :%d", framesRetrived);
    return framesRetrived;
}

NEXUS_Error BOMX_VideoEncoder::AllocateInputBuffer(uint32_t nSize, void*& pBuffer)
{
    NEXUS_ClientConfiguration               clientConfig;
    NEXUS_MemoryAllocationSettings          memorySettings;

    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    NEXUS_Memory_GetDefaultAllocationSettings(&memorySettings);
    memorySettings.heap = clientConfig.heap[1];
    return NEXUS_Memory_Allocate(nSize, &memorySettings, &pBuffer);
}

void BOMX_VideoEncoder::FreeInputBuffer(void*& pBuffer)
{
    NEXUS_Memory_Free(pBuffer);
    pBuffer = NULL;
}

bool BOMX_VideoEncoder::GraphicsCheckpoint()
{
    NEXUS_Error errCode;
    bool ret = true;

    errCode = NEXUS_Graphics2D_Checkpoint(m_hGraphics2d, NULL);
    if ( errCode == NEXUS_GRAPHICS2D_QUEUED )
    {
        errCode = B_Event_Wait(m_hCheckpointEvent, B_CHECKPOINT_TIMEOUT);
        if ( errCode )
        {
            ALOGE("!!! ERROR: Timeout waiting for graphics checkpoint !!!");
            ret = false;
        }
    }

    return ret;
}

NEXUS_Error BOMX_VideoEncoder::ConvertYv12To422p(NEXUS_SurfaceHandle hSrcCb, NEXUS_SurfaceHandle hSrcCr, NEXUS_SurfaceHandle hSrcY, NEXUS_SurfaceHandle hDst, bool isSurfaceBuffer)
{
    NEXUS_Error rc = NEXUS_SUCCESS;

    BM2MC_PACKET_Plane planeY, planeCb, planeCr, planeYCbCr;
    void *buffer, *next;
    size_t size;

    BM2MC_PACKET_Blend combColor = {BM2MC_PACKET_BlendFactor_eSourceColor,
                                    BM2MC_PACKET_BlendFactor_eOne,
                                    false,
                                    BM2MC_PACKET_BlendFactor_eDestinationColor,
                                    BM2MC_PACKET_BlendFactor_eOne,
                                    false,
                                    BM2MC_PACKET_BlendFactor_eZero
                                   };
    BM2MC_PACKET_Blend copyAlpha = {BM2MC_PACKET_BlendFactor_eZero,
                                    BM2MC_PACKET_BlendFactor_eOne,
                                    false,
                                    BM2MC_PACKET_BlendFactor_eZero,
                                    BM2MC_PACKET_BlendFactor_eZero,
                                    false,
                                    BM2MC_PACKET_BlendFactor_eZero
                                   };


    ALOGV("intermediate surfaces: y:%p, cr:%p, cb:%p - output surface:%p", hSrcY, hSrcCr, hSrcCb, hDst);

    rc = NEXUS_Graphics2D_GetPacketBuffer(m_hGraphics2d, &buffer, &size, 1024);
    if ((rc != NEXUS_SUCCESS) || !size) {
        ALOGE("%s: failed getting packet buffer from g2d: (num:%d, id:0x%x)\n", __FUNCTION__,
              NEXUS_GET_ERR_NUM(rc), NEXUS_GET_ERR_ID(rc));
        goto con_out;
    }

    if (isSurfaceBuffer)
    {
        NEXUS_Surface_LockPlaneAndPalette(hSrcY, &planeY, NULL);
        NEXUS_Surface_LockPlaneAndPalette(hSrcCb, &planeCb, NULL);
        NEXUS_Surface_LockPlaneAndPalette(hSrcCr, &planeCr, NULL);
        NEXUS_Surface_LockPlaneAndPalette(hDst, &planeYCbCr, NULL);
    }
    else
    {
        NEXUS_Surface_InitPlaneAndPaletteOffset(hSrcY, &planeY, NULL);
        NEXUS_Surface_InitPlaneAndPaletteOffset(hSrcCb, &planeCb, NULL);
        NEXUS_Surface_InitPlaneAndPaletteOffset(hSrcCr, &planeCr, NULL);
        NEXUS_Surface_InitPlaneAndPaletteOffset(hDst, &planeYCbCr, NULL);
    }

    next = buffer;
    {
        BM2MC_PACKET_PacketFilterEnable *pPacket = (BM2MC_PACKET_PacketFilterEnable *)next;
        BM2MC_PACKET_INIT(pPacket, FilterEnable, false );
        pPacket->enable = 0;
        next = ++pPacket;
    }
    {
        BM2MC_PACKET_PacketSourceFeeders *pPacket = (BM2MC_PACKET_PacketSourceFeeders *)next;
        BM2MC_PACKET_INIT(pPacket, SourceFeeders, false );
        pPacket->plane0          = planeCb;
        pPacket->plane1          = planeCr;
        pPacket->color           = 0;
        next = ++pPacket;
    }
    {
        BM2MC_PACKET_PacketDestinationFeeder *pPacket = (BM2MC_PACKET_PacketDestinationFeeder *)next;
        BM2MC_PACKET_INIT(pPacket, DestinationFeeder, false );
        pPacket->plane           = planeY;
        pPacket->color           = 0;
        next = ++pPacket;
    }
    {
        BM2MC_PACKET_PacketOutputFeeder *pPacket = (BM2MC_PACKET_PacketOutputFeeder *)next;
        BM2MC_PACKET_INIT(pPacket, OutputFeeder, false);
        pPacket->plane           = planeYCbCr;
        next = ++pPacket;
    }
    {
        BM2MC_PACKET_PacketBlend *pPacket = (BM2MC_PACKET_PacketBlend *)next;
        BM2MC_PACKET_INIT( pPacket, Blend, false );
        pPacket->color_blend     = combColor;
        pPacket->alpha_blend     = copyAlpha;
        pPacket->color           = 0;
        next = ++pPacket;
    }
    {
        BM2MC_PACKET_PacketScaleBlendBlit *pPacket = (BM2MC_PACKET_PacketScaleBlendBlit *)next;
        BM2MC_PACKET_INIT(pPacket, ScaleBlendBlit, true);
        pPacket->src_rect.x      = 0;
        pPacket->src_rect.y      = 0;
        pPacket->src_rect.width  = planeCb.width;
        pPacket->src_rect.height = planeCb.height;
        pPacket->out_rect.x      = 0;
        pPacket->out_rect.y      = 0;
        pPacket->out_rect.width  = planeYCbCr.width;
        pPacket->out_rect.height = planeYCbCr.height;
        pPacket->dst_point.x     = 0;
        pPacket->dst_point.y     = 0;
        next = ++pPacket;
    }

    rc = NEXUS_Graphics2D_PacketWriteComplete(m_hGraphics2d, (uint8_t*)next - (uint8_t*)buffer);
    if (rc != NEXUS_SUCCESS) {
        ALOGE("%s: failed writing packet buffer: (num:%d, id:0x%x)\n", __FUNCTION__,
              NEXUS_GET_ERR_NUM(rc), NEXUS_GET_ERR_ID(rc));
        goto con_out_clean;
    }

    // Wait for completion
    if ( !GraphicsCheckpoint() )
    {
        ALOGE("Graphics2D_PacketWrite checkpoint timeout");
        rc = NEXUS_TIMEOUT;
    }
con_out_clean:
    if (isSurfaceBuffer)
    {
        NEXUS_Surface_UnlockPlaneAndPalette(hSrcY);
        NEXUS_Surface_UnlockPlaneAndPalette(hSrcCb);
        NEXUS_Surface_UnlockPlaneAndPalette(hSrcCr);
        NEXUS_Surface_UnlockPlaneAndPalette(hDst);
    }
con_out:
    return rc;
}

NEXUS_Error BOMX_VideoEncoder::ExtractNexusBuffer(uint8_t *pSrcBuf, unsigned int width, unsigned height, NEXUS_SurfaceHandle hDst)
{
    NEXUS_Error rc = NEXUS_SUCCESS;

    int stride, cstride, alignment = 4;
    uint8_t *y_addr, *cr_addr, *cb_addr;
    unsigned cr_offset, cb_offset;
    NEXUS_SurfaceHandle srcCb, srcCr, srcY;
    void *slock;
    size_t size;
    NEXUS_MemoryBlockHandle block_handle = NULL;
    PSHARED_DATA pSharedData;

    stride = (width + (alignment-1)) & ~(alignment-1);
    cstride = (stride/2 + (alignment-1)) & ~(alignment-1),

    y_addr  = pSrcBuf;
    cb_offset = stride * height;
    cr_offset = (height/2) * ((stride/2 + (alignment-1)) & ~(alignment-1));
    cb_addr = (uint8_t *)(y_addr + cb_offset);
    cr_addr = (uint8_t *)(cb_addr + cr_offset);

    ALOGV("%s: yv12 (%d,%d):%d: cr-off:%u, cb-off:%u\n", __FUNCTION__,
          width, height, stride, cr_offset, cb_offset);

    srcY = CreateSurface(width, height, stride, NEXUS_PixelFormat_eY8, 0, 0, y_addr, NULL);
    if (NULL == srcY)
    {
        ALOGE("failed to create intermediate Y surface");
        rc = NEXUS_INVALID_PARAMETER;
        goto en_out;
    }
    NEXUS_Surface_Lock(srcY, &slock);
    NEXUS_Surface_Flush(srcY);

    srcCr = CreateSurface(width/2, height/2, cstride, NEXUS_PixelFormat_eCr8, 0, 0, cr_addr, NULL);
    if (NULL == srcCr)
    {
        ALOGE("failed to create intermediate Cr surface");
        rc = NEXUS_INVALID_PARAMETER;
        NEXUS_Surface_Unlock(srcY);
        NEXUS_Surface_Destroy(srcY);
        goto en_out;
    }
    NEXUS_Surface_Lock(srcCr, &slock);
    NEXUS_Surface_Flush(srcCr);

    srcCb = CreateSurface(width/2, height/2, cstride, NEXUS_PixelFormat_eCb8, 0, 0, cb_addr, NULL);
    if (NULL == srcCb)
    {
        ALOGE("failed to create intermediate Cb surface");
        rc = NEXUS_INVALID_PARAMETER;
        NEXUS_Surface_Unlock(srcCr);
        NEXUS_Surface_Destroy(srcCr);
        NEXUS_Surface_Unlock(srcY);
        NEXUS_Surface_Destroy(srcY);
        goto en_out;
    }
    NEXUS_Surface_Lock(srcCb, &slock);
    NEXUS_Surface_Flush(srcCb);

    NEXUS_Surface_Lock(hDst, &slock);

    ALOGV("%s: intermediate surfaces: y:%p, cr:%p, cb:%p\n", __FUNCTION__, srcY, srcCr, srcCb);

    ConvertYv12To422p(srcCb, srcCr, srcY, hDst, false);

    NEXUS_Surface_Flush(hDst);

    NEXUS_Surface_Unlock(srcCb);
    NEXUS_Surface_Destroy(srcCb);
    NEXUS_Surface_Unlock(srcCr);
    NEXUS_Surface_Destroy(srcCr);
    NEXUS_Surface_Unlock(srcY);
    NEXUS_Surface_Destroy(srcY);
    NEXUS_Surface_Unlock(hDst);
en_out:
    return rc;
}

NEXUS_Error BOMX_VideoEncoder::ExtractGrallocBuffer(private_handle_t *handle, NEXUS_SurfaceHandle hDst)
{
    NEXUS_Error rc = NEXUS_SUCCESS;

    NEXUS_SurfaceHandle hSrc;
    void *pMemory, *pLock;
    uint8_t *pAddr;
    NEXUS_MemoryBlockHandle block_handle = NULL;
    PSHARED_DATA pSharedData;
    unsigned int cFormat, width, height, stride, planeHandle;

    pMemory = NULL;
    block_handle = (NEXUS_MemoryBlockHandle)handle->sharedData;
    rc = NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
    ALOG_ASSERT(!rc);
    pSharedData = (PSHARED_DATA) pMemory;

    if (pSharedData == NULL) {
        ALOGE("%s: unable to locate shared data, abort conversion\n", __FUNCTION__);
        rc = NEXUS_INVALID_PARAMETER;
        goto out;
    }

    planeHandle = pSharedData->container.physAddr;
    pAddr = NULL;

    cFormat = pSharedData->container.format;
    width = pSharedData->container.width;
    height = pSharedData->container.height;
    stride = pSharedData->container.stride;
    ALOGV("InputBufferType_eNative: pSharedData:%p", pSharedData);

    // create source surface with gralloc buffer
    ALOGV("%s: pShareData:%p, width:%u, height:%u, format:%u, stride:%u", __FUNCTION__,
          pSharedData, width, height, cFormat, stride);

    if (HAL_PIXEL_FORMAT_YV12 == cFormat)
    {
        int stride, cstride;
        unsigned cr_offset, cb_offset;
        NEXUS_SurfaceHandle srcCb, srcCr, srcY;
        void *slock;
        size_t size;
        int alignment = handle->alignment;

        stride = (width + (alignment-1)) & ~(alignment-1);
        cstride = (stride/2 + (alignment-1)) & ~(alignment-1),
        cr_offset = stride * height;
        cb_offset = (height/2) * ((stride/2 + (alignment-1)) & ~(alignment-1));

        ALOGV("%s: yv12 (%d,%d):%d: cr-off:%u, cb-off:%u\n", __FUNCTION__,
              width, height, stride, cr_offset, cb_offset);

        srcY = CreateSurface(width, height, stride, NEXUS_PixelFormat_eY8, planeHandle, 0, NULL, NULL);
        if (NULL == srcY)
        {
            ALOGE("failed to create intermediate Y surface");
            rc = NEXUS_INVALID_PARAMETER;
            goto out;
        }
        NEXUS_Surface_Lock(srcY, &slock);
        NEXUS_Surface_Flush(srcY);

        srcCr = CreateSurface(width/2, height/2, cstride, NEXUS_PixelFormat_eCr8, planeHandle, cr_offset, NULL, NULL);
        if (NULL == srcCr)
        {
            ALOGE("failed to create intermediate Cr surface");
            rc = NEXUS_INVALID_PARAMETER;
            NEXUS_Surface_Unlock(srcY);
            NEXUS_Surface_Destroy(srcY);
            goto out;
        }
        NEXUS_Surface_Lock(srcCr, &slock);
        NEXUS_Surface_Flush(srcCr);

        srcCb = CreateSurface(width/2, height/2, cstride, NEXUS_PixelFormat_eCb8, planeHandle, cb_offset, NULL, NULL);
        if (NULL == srcCb)
        {
            ALOGE("failed to create intermediate Cb surface");
            rc = NEXUS_INVALID_PARAMETER;
            NEXUS_Surface_Unlock(srcCr);
            NEXUS_Surface_Destroy(srcCr);
            NEXUS_Surface_Unlock(srcY);
            NEXUS_Surface_Destroy(srcY);
            goto out;
        }
        NEXUS_Surface_Lock(srcCb, &slock);
        NEXUS_Surface_Flush(srcCb);

        NEXUS_Surface_Lock(hDst, &slock);

        ALOGV("%s: intermediate surfaces: y:%p, cr:%p, cb:%p\n", __FUNCTION__, srcY, srcCr, srcCb);

        ConvertYv12To422p(srcCb, srcCr, srcY, hDst, true);

        NEXUS_Surface_Flush(hDst);

        NEXUS_Surface_Unlock(srcCb);
        NEXUS_Surface_Destroy(srcCb);
        NEXUS_Surface_Unlock(srcCr);
        NEXUS_Surface_Destroy(srcCr);
        NEXUS_Surface_Unlock(srcY);
        NEXUS_Surface_Destroy(srcY);
        NEXUS_Surface_Unlock(hDst);
    }
    else
    {
        NEXUS_Graphics2DBlitSettings blitSettings;
        NEXUS_PixelFormat pixelFormat;

        switch (cFormat) {
        case HAL_PIXEL_FORMAT_RGBA_8888:
            pixelFormat = NEXUS_PixelFormat_eA8_B8_G8_R8;
            break;
        case HAL_PIXEL_FORMAT_RGBX_8888:
            pixelFormat = NEXUS_PixelFormat_eX8_R8_G8_B8;
            break;
        case HAL_PIXEL_FORMAT_RGB_888:
            pixelFormat = NEXUS_PixelFormat_eX8_R8_G8_B8;
            break;
        case HAL_PIXEL_FORMAT_RGB_565:
            pixelFormat = NEXUS_PixelFormat_eR5_G6_B5;
            break;
        case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
            pixelFormat = NEXUS_PixelFormat_eX8_R8_G8_B8;
            break;
        default:
            ALOGE("Unable to allocate color format conversion surface");
            rc = NEXUS_NOT_SUPPORTED;
            goto out;
        }

        ALOGV("Nexus pixel format:%d - pAddr=%p, plane handle=%u",
              pixelFormat, pAddr, planeHandle);

        hSrc = CreateSurface(width, height, stride, pixelFormat, planeHandle, 0, NULL, NULL);
        if ( NULL == hSrc )
        {
            ALOGE("Unable to allocate color format conversion surface");
            rc = NEXUS_OUT_OF_DEVICE_MEMORY;
            goto out;
        }
        rc = NEXUS_Surface_Lock(hSrc, &pLock);
        if ( rc )
        {
            ALOGE("Error locking color format conversion surface - %d", rc);
            goto rgb_out_cleanup;
        }
        NEXUS_Surface_Flush(hSrc);
        NEXUS_Surface_Lock(hDst, &pLock);

        // Surface buffer blit
        ALOGV("%s: Graphic2D_Blit: source:%p, output:%p", __FUNCTION__, hSrc, hDst);
        NEXUS_Graphics2D_GetDefaultBlitSettings(&blitSettings);
        blitSettings.source.surface = hSrc;
        blitSettings.output.surface = hDst;
        blitSettings.colorOp = NEXUS_BlitColorOp_eCopySource;
        blitSettings.alphaOp = NEXUS_BlitAlphaOp_eCopySource;
        rc = NEXUS_Graphics2D_Blit(m_hGraphics2d, &blitSettings);
        if ( rc )
        {
            ALOGE("NEXUS_Graphics2D_Blit error - %d", rc);
            goto rgb_out_cleanup_locked;
        }

        // Wait for completion
        if ( !GraphicsCheckpoint() )
        {
            ALOGE("NEXUS_Graphics2D_Blit checkpoint timeout");
            rc = NEXUS_TIMEOUT;
            goto rgb_out_cleanup_locked;
        }

        NEXUS_Surface_Flush(hDst);
rgb_out_cleanup_locked:
        NEXUS_Surface_Unlock(hSrc);
rgb_out_cleanup:
        NEXUS_Surface_Destroy(hSrc);
        NEXUS_Surface_Unlock(hDst);
    }

out:
    if (block_handle) {
        NEXUS_MemoryBlock_Unlock(block_handle);
    }
    return rc;
}

NEXUS_Error BOMX_VideoEncoder::UpdateEncoderSettings(void)
{
    NEXUS_Error rc;
    NEXUS_SimpleEncoderSettings encoderSettings;
    const OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = m_pVideoPorts[0]->GetDefinition();

    NEXUS_SimpleEncoder_GetSettings(m_hSimpleEncoder, &encoderSettings);

    encoderSettings.video.width = pPortDef->format.video.nFrameWidth;;
    encoderSettings.video.height = pPortDef->format.video.nFrameHeight;
    encoderSettings.video.refreshRate = 60000;

    encoderSettings.videoEncoder.bitrateMax = m_sVideoBitrateParams.nTargetBitrate;
    switch ( m_sVideoBitrateParams.eControlRate )
    {
    case OMX_Video_ControlRateVariable:
    {
        encoderSettings.videoEncoder.bitrateTarget = m_sVideoBitrateParams.nTargetBitrate;
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
        encoderSettings.videoEncoder.bitrateTarget = m_sVideoBitrateParams.nTargetBitrate;
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
        ALOGW("%s: No rate control", __FUNCTION__);
        encoderSettings.videoEncoder.bitrateTarget = 0;
        encoderSettings.videoEncoder.variableFrameRate = false;
    }
    break;
    }
    encoderSettings.videoEncoder.frameRate = MapOMXFrameRateToNexus(pPortDef->format.video.xFramerate);
    if ( encoderSettings.videoEncoder.frameRate == NEXUS_VideoFrameRate_eUnknown )
    {
        ALOGW("Unknown encoder frame rate %u. Use variable frame rate...", pPortDef->format.video.xFramerate);

        encoderSettings.videoEncoder.frameRate = B_DEFAULT_INPUT_NEXUS_FRAMERATE;
        encoderSettings.videoEncoder.variableFrameRate = true;
    }

    ALOGV("FrameRate=%g BitRateMax=%d BitRateTarget=%d bVarFrameRate=%d",
          BOMX_NexusFramerateValue(encoderSettings.videoEncoder.frameRate),
          encoderSettings.videoEncoder.bitrateMax,
          encoderSettings.videoEncoder.bitrateTarget,
          encoderSettings.videoEncoder.variableFrameRate);

    rc = NEXUS_SimpleEncoder_SetSettings(m_hSimpleEncoder, &encoderSettings);
    if ( rc )
    {
        return BOMX_BERR_TRACE(BERR_UNKNOWN);
    }
    ALOGV("configured Nexus encoder");

    return NEXUS_SUCCESS;
}

/* https://en.wikipedia.org/wiki/H.264/MPEG-4_AVC#Levels */
static const struct {
    OMX_VIDEO_AVCLEVELTYPE level;
    struct {
        unsigned lumaSamples;
        unsigned macroblocks;
    } maxDecodingSpeed;
    struct {
        unsigned lumaSamples;
        unsigned macroblocks;
    } maxFrameSize;
    struct {
        unsigned baselineExtendedMain;
        unsigned high;
        unsigned high10;
    } maxVideoBitRate; // (VCL) kbit/s
    struct {
        unsigned width;
        unsigned height;
        float frameRate;
        unsigned maxStoredFrames;
    } examples[6];
    int nbExamples;
} MPEG4_AVC_Levels[] = {
    /*      Level         | Max decoding speed  | Max frame size  | Max video bit rate for  | Examples for high resolution
    |                     |                     |                 |  video coding layer     | @ highest frame rate
    |                     |                     |                 |     (VCL) kbit/s        |{{width, height, rate,
    |                     |{Luma samples/s,     |{Luma samples,   |{Baseline/Extended/Main, |      max stored frames},
    |                     |      Macroblocks/s} |    Macroblocks} |          High, High 10} | {width, ...}}, nbExamples} */
    {OMX_VIDEO_AVCLevel1,  {   380160,    1485}, {  25344,    99}, {    64,     80,    192}, {{ 128,   96,  30.9,  8},
                                                                                              { 176,  144,  15.0,  4}}, 2},
    {OMX_VIDEO_AVCLevel1b, {   380160,    1485}, {  25344,    99}, {   128,    160,    384}, {{ 128,   96,  30.9,  8},
                                                                                              { 176,  144,  15.0,  4}}, 2},
    {OMX_VIDEO_AVCLevel11, {   768000,    3000}, { 101376,   396}, {   192,    240,    576}, {{ 176,  144,  30.3,  9},
                                                                                              { 320,  240,  10.0,  3},
                                                                                              { 352,  288,   7.5,  2}}, 3},
    {OMX_VIDEO_AVCLevel12, {  1536000,    6000}, { 101376,   396}, {   384,    480,   1152}, {{ 320,  240,  20.0,  7},
                                                                                              { 352,  288,  15.2,  6}}, 2},
    {OMX_VIDEO_AVCLevel13, {  3041280,   11880}, { 101376,   396}, {   768,    960,   2304}, {{ 320,  240,  36.0,  7},
                                                                                              { 352,  288,  30.0,  6}}, 2},
    {OMX_VIDEO_AVCLevel2,  {  3041280,   11880}, { 101376,   396}, {  2000,   2500,   6000}, {{ 320,  240,  36.0,  7},
                                                                                              { 352,  288,  30.0,  6}}, 2},
    {OMX_VIDEO_AVCLevel21, {  5068800,   19800}, { 202752,   792}, {  4000,   5000,  12000}, {{ 352,  480,  30.0,  7},
                                                                                              { 352,  576,  25.0,  6}}, 2},
    {OMX_VIDEO_AVCLevel22, {  5184000,   20250}, { 414720,  1620}, {  4000,   5000,  12000}, {{ 352,  480,  30.7, 12},
                                                                                              { 352,  576,  25.6, 10},
                                                                                              { 720,  480,  15.0,  6},
                                                                                              { 720,  576,  12.5,  5}}, 4},
    {OMX_VIDEO_AVCLevel3,  { 10368000,   40500}, { 414720,  1620}, { 10000,  12500,  30000}, {{ 352,  480,  61.4, 12},
                                                                                              { 352,  576,  51.1, 10},
                                                                                              { 720,  480,  30.0,  6},
                                                                                              { 720,  576,  25.0,  5}}, 4},
    {OMX_VIDEO_AVCLevel31, { 27648000,  108000}, { 921600,  3600}, { 14000,  17500,  42000}, {{ 720,  480,  80.0, 13},
                                                                                              { 720,  576,  66.7, 11},
                                                                                              {1280,  720,  30.0,  5}}, 3},
    {OMX_VIDEO_AVCLevel32, { 55296000,  216000}, {1310720,  5120}, { 20000,  25000,  60000}, {{1280,  720,  60.0,  5},
                                                                                              {1280, 1024,  42.2,  4}}, 2},
    {OMX_VIDEO_AVCLevel4,  { 62914560,  245760}, {2097152,  8192}, { 20000,  25000,  60000}, {{1280,  720,  68.3,  9},
                                                                                              {1920, 1080,  30.1,  4},
                                                                                              {2048, 1024,  30.0,  4}}, 3},
    {OMX_VIDEO_AVCLevel41, { 62914560,  245760}, {2097152,  8192}, { 50000,  62500, 150000}, {{1280,  720,  68.3,  9},
                                                                                              {1920, 1080,  30.1,  4},
                                                                                              {2048, 1024,  30.0,  4}}, 3},
    {OMX_VIDEO_AVCLevel42, {133693440,  522240}, {2228224,  8704}, { 50000,  62500, 150000}, {{1280,  720, 145.1,  9},
                                                                                              {1920, 1080,  64.0,  4},
                                                                                              {2048, 1080,  60.0,  4}}, 3},
    {OMX_VIDEO_AVCLevel5,  {150994944,  589824}, {5652480, 22080}, {135000, 168750, 405000}, {{1920, 1080,  72.3, 13},
                                                                                              {2048, 1024,  72.0, 13},
                                                                                              {2048, 1080,  67.8, 12},
                                                                                              {2560, 1920,  30.7,  5},
                                                                                              {3672, 1536,  26.7,  5}}, 5},
    {OMX_VIDEO_AVCLevel51, {251658240,  983040}, {9437184, 36864}, {240000, 300000, 720000}, {{1920, 1080, 120.5, 16},
                                                                                              {2560, 1920,  51.2,  9},
                                                                                              {3840, 2160,  31.7,  5},
                                                                                              {4096, 2048,  30.0,  5},
                                                                                              {4096, 2160,  28.5,  5},
                                                                                              {4096, 2304,  26.7,  5}}, 6},
    {OMX_VIDEO_AVCLevel52, {530841600, 2073600}, {9437184, 36864}, {240000, 300000, 720000}, {{1920, 1080, 172.0, 16},
                                                                                              {2560, 1920, 108.0,  9},
                                                                                              {3840, 2160,  66.8,  5},
                                                                                              {4096, 2048,  63.3,  5},
                                                                                              {4096, 2160,  60.0,  5},
                                                                                              {4096, 2304,  56.3,  5}}, 6},
};
static const int nMPEG4_AVC_Levels = sizeof(MPEG4_AVC_Levels)/sizeof(MPEG4_AVC_Levels[0]);

/* Return the maximum level supported for a given profile. */
OMX_VIDEO_AVCLEVELTYPE BOMX_VideoEncoder::GetMaxLevelAvc(OMX_VIDEO_AVCPROFILETYPE profile)
{
    int i, j;

    ALOGV("GetMaxLevelAvc %s %ux%u@%f",
        asString(profile), m_maxFrameWidth, m_maxFrameHeight, B_MAX_FRAME_RATE_F);

    for ( i = nMPEG4_AVC_Levels-1; i >= 0; i-- )
    {
        unsigned maxVideoBitRate;

        switch (profile) {
        case OMX_VIDEO_AVCProfileBaseline:
        case OMX_VIDEO_AVCProfileMain:
        case OMX_VIDEO_AVCProfileExtended:
            maxVideoBitRate = MPEG4_AVC_Levels[i].maxVideoBitRate.baselineExtendedMain;
            break;
        case OMX_VIDEO_AVCProfileHigh:
            maxVideoBitRate = MPEG4_AVC_Levels[i].maxVideoBitRate.high;
            break;
        case OMX_VIDEO_AVCProfileHigh10:
            maxVideoBitRate = MPEG4_AVC_Levels[i].maxVideoBitRate.high10;
            break;
        default:
            /* We don't know */
            maxVideoBitRate = 0;
        }

        ALOGV("AVC trying %s %s maxDecodingSpeed=%u Luma samples/s (%u Macroblocks/s) "
            "maxFrameSize=%u Luma samples (%u Macroblocks) maxVideoBitRate=%u",
            asString(profile),
            asString(MPEG4_AVC_Levels[i].level),
            MPEG4_AVC_Levels[i].maxDecodingSpeed.lumaSamples,
            MPEG4_AVC_Levels[i].maxDecodingSpeed.macroblocks,
            MPEG4_AVC_Levels[i].maxFrameSize.lumaSamples,
            MPEG4_AVC_Levels[i].maxFrameSize.macroblocks,
            maxVideoBitRate);

        for ( j = MPEG4_AVC_Levels[i].nbExamples-1; j >= 0; j-- )
        {
            if (MPEG4_AVC_Levels[i].examples[j].width <= m_maxFrameWidth &&
                MPEG4_AVC_Levels[i].examples[j].height <= m_maxFrameHeight &&
                MPEG4_AVC_Levels[i].examples[j].frameRate <= B_MAX_FRAME_RATE_F /*&&
                MPEG4_AVC_Levels[i].examples[j].maxStoredFrames <= our encoder's max stored frames*/ )
            {
                ALOGV("AVC: %s %s matching example %ux%u@%f (%u)",
                    asString(profile),
                    asString(MPEG4_AVC_Levels[i].level),
                    MPEG4_AVC_Levels[i].examples[j].width,
                    MPEG4_AVC_Levels[i].examples[j].height,
                    MPEG4_AVC_Levels[i].examples[j].frameRate,
                    MPEG4_AVC_Levels[i].examples[j].maxStoredFrames);
                return MPEG4_AVC_Levels[i].level;
            }
        }
    }
    ALOGV("AVC: no matching example found in spec");
    return (OMX_VIDEO_AVCLEVELTYPE)0;
}
