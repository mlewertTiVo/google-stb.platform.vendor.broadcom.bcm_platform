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
 * $brcm_Workfile: nexusservice.cpp $
 * $brcm_Revision: 23 $
 * $brcm_Date: 12/3/12 3:24p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/libnexusservice/nexusservice.cpp $
 * 
 * 24   1/24/13 5:44p mnelis
 * SWANDROID-301: Add Zorder support
 * 
 * 23   12/3/12 3:24p saranya
 * SWANDROID-266: Removed Non-IPC Standalone Mode
 * 
 * 22   9/20/12 6:28p kagrawal
 * SWANDROID-218: Give framebuffer_heap(1) for the SD display while
 *  setting up the NSC server
 * 
 * 21   9/13/12 11:32a kagrawal
 * SWANDROID-104: Added support for dynamic display resolution change,
 *  1080p and screen resizing
 * 
 * 20   9/12/12 3:59p nitinb
 * SWANDROID-197: Implement volume control at audio output level
 * 
 * 19   9/4/12 6:54p nitinb
 * SWANDROID-197:Implement volume control functionality on nexus server
 *  side
 * 
 * 18   7/30/12 4:08p kagrawal
 * SWANDROID-104: Support for composite output
 * 
 * 17   7/6/12 9:12p ajitabhp
 * SWANDROID-128: FIXED Graphics Window Resource Leakage in SBS and NSC
 *  mode.
 * 
 * 16   6/24/12 10:53p alexpan
 * SWANDROID-108: Fix build errors for platforms without hdmi-in after
 *  changes for SimpleDecoder
 * 
 * 15   6/22/12 3:01p kagrawal
 * SWANDROID-118: Fix for 3D display format in NSC client-server mode
 * 
 * 14   6/22/12 2:35a ajitabhp
 * SWANDROID-121: Overscan fix enabled for NSC client server mode.
 * 
 * 13   6/20/12 6:00p kagrawal
 * SWANDROID-118: Extended get_output_format() to return width and height
 * 
 * 12   6/20/12 11:22a kagrawal
 * SWANDROID-108: Add support for HDMI-Input with SimpleDecoder and w/ or
 *  w/o nexus client server mode
 * 
 * 11   6/5/12 2:38p kagrawal
 * SWANDROID-108:Added support to use simple decoder APIs
 * 
 * 10   5/29/12 6:58p ajitabhp
 * SWANDROID-96: Fixed the problem with environment variable
 * 
 * 9   5/28/12 5:12p kagrawal
 * SWANDROID-101: Calling authenticated_join only for untrusted mode
 * 
 * 8   4/13/12 1:15p ajitabhp
 * SWANDROID-65: Memory Owner ship problem resolved in multi-process mode.
 * 
 * 7   4/3/12 5:00p kagrawal
 * SWANDROID-56: Added support for VideoWindow configuration in NSC mode
 * 
 * 6   3/27/12 4:06p mzhuang
 * SW7425-2633: audio mixer errors after audio flinger restart
 * 
 * 5   3/15/12 4:52p mzhuang
 * SW7425-2633: audio mixer errors after audio flinger restart
 * 
 * 4   3/1/12 1:49p franktcc
 * SW7425-2196: Adding display format for svc/mvc
 * 
 * 3   2/24/12 4:09p kagrawal
 * SWANDROID-12: Dynamic client creation using IPC over binder
 * 
 * 2   2/8/12 2:52p kagrawal
 * SWANDROID-12: Initial support for Nexus client-server mode
 * 
 * 3   9/19/11 5:23p fzhang
 * SW7425-1307: Add libaudio support on 7425 Honeycomb
 * 
 * 2   8/25/11 7:30p franktcc
 * SW7420-2020: Enable PIP/Dual Decode
 * 
 * 
 *****************************************************************************/
  
#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "NexusService"
//#define LOG_NDEBUG 0

#include <utils/Log.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include <string.h>
#include <cutils/atomic.h>
#include <utils/Errors.h>
#include <binder/IServiceManager.h>
#include <utils/String16.h>
#include "cutils/properties.h"

#include "nexusservice.h"
#if NEXUS_HAS_CEC
#include "nexuscecservice.h"
#endif

#include "blst_list.h"
#include "nexus_audio_mixer.h"
#include "nexus_audio_decoder.h"
#include "nexus_audio_playback.h"
#include "nexus_video_decoder_extra.h"
#include "nexus_base_mmap.h"

#include "nexus_ipc_priv.h"

#define UINT32_C(x)  (x ## U)

/* We don't need to perform client reference counting for URSR14.1 or later */
#if defined(NEXUS_COMMON_PLATFORM_VERSION) && defined(NEXUS_PLATFORM_VERSION) && (NEXUS_COMMON_PLATFORM_VERSION < NEXUS_PLATFORM_VERSION(14,1)) || \
   !defined(NEXUS_COMMON_PLATFORM_VERSION) || !defined(NEXUS_PLATFORM_VERSION)
#define MAX_CLIENTS 32
#define MAX_OBJECTS MAX_CLIENTS
#endif

typedef struct NexusClientContext {
    BDBG_OBJECT(NexusClientContext)
    BLST_D_ENTRY(NexusClientContext) link;
    struct {
        NEXUS_ClientHandle nexusClient;
        unsigned connectId;
    } ipc;
    struct {
        NEXUS_SurfaceClientHandle       graphicsSurface;
        NEXUS_SurfaceClientHandle       videoSurface;
        struct {
            bool                            connected;
#if NEXUS_NUM_HDMI_INPUTS
            NEXUS_HdmiInputHandle           handle;
#endif
#if NEXUS_NUM_AUDIO_INPUT_CAPTURES
            NEXUS_AudioInputCaptureHandle   captureInput;
#endif
            unsigned                        windowId;
        } hdmiInput;
    } resources;
    b_refsw_client_client_info info;
    b_refsw_client_client_configuration createConfig;
} NexusClientContext;


BDBG_OBJECT_ID(NexusClientContext);

typedef struct NexusServerContext {
    NexusServerContext();
    ~NexusServerContext() {};

    Mutex mLock;
    unsigned mJoinRefCount;
    BLST_D_HEAD(b_refsw_client_list, NexusClientContext) clients;
    struct {
        unsigned client;
        NEXUS_SurfaceCompositorClientId surfaceClientId;
    } lastId;
} NexusServerContext;

NexusServerContext::NexusServerContext() : mLock(Mutex::SHARED), mJoinRefCount(0)
{
    BLST_D_INIT(&clients);
    lastId.client = 0;
    lastId.surfaceClientId = 0;
}

const String16 INexusService::descriptor(NEXUS_INTERFACE_NAME);

String16 INexusService::getInterfaceDescriptor() {
        return INexusService::descriptor;
}

NEXUS_ClientHandle NexusService::getNexusClient(NexusClientContext * client)
{
    NEXUS_ClientHandle nexusClient = NULL;
    NEXUS_InterfaceName interfaceName;
    unsigned num;
    unsigned i;
    int rc;

    if (client == NULL) {
        LOGE("%s: client context is NULL!!!", __FUNCTION__);
    }
    else {
        NEXUS_Platform_GetDefaultInterfaceName(&interfaceName);
        strcpy(interfaceName.name, "NEXUS_Client");
#ifndef MAX_OBJECTS
        rc = NEXUS_Platform_GetObjects(&interfaceName, NULL, 0, &num);
        BDBG_ASSERT(!rc);
        NEXUS_PlatformObjectInstance objects[num];
#else
        NEXUS_PlatformObjectInstance objects[MAX_OBJECTS];
        num = MAX_OBJECTS;
#endif
        rc = NEXUS_Platform_GetObjects(&interfaceName, objects, num, &num);
        BDBG_ASSERT(!rc);

        for (i=0; i < num; i++) {
            NEXUS_ClientStatus status;
            unsigned j;
            rc = NEXUS_Platform_GetClientStatus(reinterpret_cast<NEXUS_ClientHandle>(objects[i].object), &status);
            if (rc) continue;

            if (status.pid == client->createConfig.pid) {
                nexusClient = reinterpret_cast<NEXUS_ClientHandle>(objects[i].object);
                LOGV("%s: Found client \"%s\" with PID %d nexus_client %p.", __FUNCTION__,
                    client->createConfig.name.string, client->createConfig.pid, (void *)nexusClient);
                break;
            }
        }
    }
    return nexusClient;
}

void NexusService::platformInitAudio(void)
{
    int i;
    NEXUS_Error rc;
    NEXUS_PlatformConfiguration             platformConfig;
    NEXUS_SimpleAudioDecoderServerSettings  simpleAudioDecoderSettings;
    NEXUS_SimpleAudioPlaybackServerSettings simpleAudioPlaybackSettings;
    NEXUS_AudioDecoderOpenSettings          audioDecoderOpenSettings;

    NEXUS_Platform_GetConfiguration(&platformConfig);

    /* create audio decoders */
    for (i=0; i<MAX_AUDIO_DECODERS; i++) 
    {
        NEXUS_AudioDecoder_GetDefaultOpenSettings(&audioDecoderOpenSettings);
        audioDecoderOpenSettings.fifoSize = AUDIO_DECODER_FIFO_SIZE;
        audioDecoderOpenSettings.type     = NEXUS_AudioDecoderType_eDecode;
        audioDecoder[i] = NEXUS_AudioDecoder_Open(i, &audioDecoderOpenSettings);
    }
    
    /* open audio mixer */
    mixer = NEXUS_AudioMixer_Open(NULL);

    /* open the audio playback */
    for (i=0; i<MAX_AUDIO_DECODERS; i++) 
    {
        audioPlayback[i] = NEXUS_AudioPlayback_Open(i, NULL);
    }


    /* Connect audio inputs to the mixer and mixer to the outputs */
    rc = NEXUS_AudioMixer_AddInput(mixer, NEXUS_AudioDecoder_GetConnector(audioDecoder[0], NEXUS_AudioDecoderConnectorType_eStereo));
    BDBG_ASSERT(!rc);
    rc = NEXUS_AudioMixer_AddInput(mixer, NEXUS_AudioDecoder_GetConnector(audioDecoder[1], NEXUS_AudioDecoderConnectorType_eStereo));
    BDBG_ASSERT(!rc);
    
    rc = NEXUS_AudioMixer_AddInput(mixer, NEXUS_AudioPlayback_GetConnector(audioPlayback[0]));
    BDBG_ASSERT(!rc);
    rc = NEXUS_AudioMixer_AddInput(mixer, NEXUS_AudioPlayback_GetConnector(audioPlayback[1]));
    BDBG_ASSERT(!rc);

#if NEXUS_NUM_AUDIO_DACS
    rc = NEXUS_AudioOutput_AddInput(NEXUS_AudioDac_GetConnector(platformConfig.outputs.audioDacs[0]), NEXUS_AudioMixer_GetConnector(mixer));
    BDBG_ASSERT(!rc);
#endif
#if NEXUS_NUM_HDMI_OUTPUTS
    rc = NEXUS_AudioOutput_AddInput(NEXUS_HdmiOutput_GetAudioConnector(platformConfig.outputs.hdmi[0]), NEXUS_AudioMixer_GetConnector(mixer));
    BDBG_ASSERT(!rc);
#endif
#if NEXUS_NUM_SPDIF_OUTPUTS 
    rc = NEXUS_AudioOutput_AddInput(NEXUS_SpdifOutput_GetConnector(platformConfig.outputs.spdif[0]), NEXUS_AudioMixer_GetConnector(mixer));
    BDBG_ASSERT(!rc);
#endif

    /* create simple audio decoder */
    for (i=0; i<MAX_AUDIO_DECODERS; i++) 
    {
        NEXUS_SimpleAudioDecoder_GetDefaultServerSettings(&simpleAudioDecoderSettings);
        simpleAudioDecoderSettings.primary = audioDecoder[i];
        simpleAudioDecoder[i] = NEXUS_SimpleAudioDecoder_Create(i, &simpleAudioDecoderSettings);
    }

    /* create simple audio player */
    for (i=0; i<MAX_AUDIO_DECODERS; i++) 
    {
        NEXUS_SimpleAudioPlayback_GetDefaultServerSettings(&simpleAudioPlaybackSettings);
        simpleAudioPlaybackSettings.decoder = simpleAudioDecoder[i]; /* linked to the audio decoder for StcChannel */
        simpleAudioPlaybackSettings.playback = audioPlayback[i];
        simpleAudioPlayback[i] = NEXUS_SimpleAudioPlayback_Create(i, &simpleAudioPlaybackSettings);
    }
}

void NexusService::setAudioState(bool enable)
{
    int i;
    NEXUS_SimpleAudioDecoderServerSettings simpleAudioDecoderSettings;
    NEXUS_SimpleAudioPlaybackServerSettings simpleAudioPlaybackSettings;
    
    /* Enable/Disable simple audio player */
    for (i=0; i<MAX_AUDIO_DECODERS; i++) {
        NEXUS_SimpleAudioPlayback_GetServerSettings(simpleAudioPlayback[i], &simpleAudioPlaybackSettings);
        if (enable) {
            simpleAudioPlaybackSettings.decoder = simpleAudioDecoder[i]; /* linked to the audio decoder for StcChannel */
            simpleAudioPlaybackSettings.playback = audioPlayback[i];
        } else {
            simpleAudioPlaybackSettings.decoder = NULL;
            simpleAudioPlaybackSettings.playback = NULL;
        }
        NEXUS_SimpleAudioPlayback_SetServerSettings(simpleAudioPlayback[i], &simpleAudioPlaybackSettings);
    }

    /* Enable/Disable simple audio decoder */
    for (i=0; i<MAX_AUDIO_DECODERS; i++) {
        NEXUS_SimpleAudioDecoder_GetServerSettings(simpleAudioDecoder[i], &simpleAudioDecoderSettings);
        simpleAudioDecoderSettings.enabled = enable;
        NEXUS_SimpleAudioDecoder_SetServerSettings(simpleAudioDecoder[i], &simpleAudioDecoderSettings);
    }
}

void NexusService::platformInitVideo(void)
{
    NEXUS_DisplaySettings           displaySettings;        
    NEXUS_PlatformConfiguration     platformConfig;
    NEXUS_VideoDecoderOpenSettings  videoDecoderOpenSettings;
    int rc;
    
    NEXUS_Platform_GetConfiguration(&platformConfig);
    
    /* Init displays */
    for(int j=0; j<MAX_NUM_DISPLAYS; j++)
    {
        NEXUS_Display_GetDefaultSettings(&displaySettings);
        displaySettings.displayType = NEXUS_DisplayType_eAuto;
        displaySettings.format = (j == HD_DISPLAY) ? initial_hd_format : initial_sd_format;       
        displayState[j].display = NEXUS_Display_Open(j, &displaySettings);
        if(!displayState[j].display) while(1){LOGE("NEXUS_Display_Open(%d) failed!!",j);}
        displayState[j].hNexusDisplay = reinterpret_cast<int>(displayState[j].display);
    }
    
#if NEXUS_NUM_COMPONENT_OUTPUTS
    /* Add Component Output to the HD-Display */
    rc = NEXUS_Display_AddOutput(displayState[HD_DISPLAY].display, NEXUS_ComponentOutput_GetConnector(platformConfig.outputs.component[0]));
    if(rc!=NEXUS_SUCCESS) while(1){LOGE("NEXUS_Display_AddOutput(component) failed!!");}
#endif

#if NEXUS_NUM_HDMI_OUTPUTS
    /* Add HDMI Output to the HD-Display */
    rc = NEXUS_Display_AddOutput(displayState[HD_DISPLAY].display, NEXUS_HdmiOutput_GetVideoConnector(platformConfig.outputs.hdmi[0]));
    if(rc!=NEXUS_SUCCESS) while(1){LOGE("NEXUS_Display_AddOutput(hdmi) failed!!");}
#endif

#if NEXUS_NUM_COMPOSITE_OUTPUTS
    if(MAX_NUM_DISPLAYS > 1) {
        /* Add Composite Output to the SD-Display */
        rc = NEXUS_Display_AddOutput(displayState[SD_DISPLAY].display, NEXUS_CompositeOutput_GetConnector(platformConfig.outputs.composite[0]));
        if(rc!=NEXUS_SUCCESS) while(1){LOGE("NEXUS_Display_AddOutput(composite) failed!!");}
    }
#endif
    
    // Both HD and SD displays have 2 video windows each 
    for(int j=0; j<MAX_NUM_DISPLAYS; j++)
    {
        for(int i=0; i<MAX_VIDEO_WINDOWS_PER_DISPLAY; i++)
        {
            displayState[j].video_window[i] = NEXUS_VideoWindow_Open(displayState[j].display, i);
            if(!displayState[j].video_window[i]) while(1){LOGE("NEXUS_VideoWindow_Open(%d) failed!!",i);}
            displayState[j].hNexusVideoWindow[i] = reinterpret_cast<int>(displayState[j].video_window[i]);
        }
    }
    
    for(int i=0; i<2; i++)
    {
        NEXUS_SimpleVideoDecoderServerSettings settings;

        // open video decoder
        NEXUS_VideoDecoder_GetDefaultOpenSettings(&videoDecoderOpenSettings);
        videoDecoderOpenSettings.fifoSize = VIDEO_DECODER_FIFO_SIZE;
        if (i == 0)
        {
            videoDecoderOpenSettings.svc3dSupported = true;
        }

        videoDecoder[i] = NEXUS_VideoDecoder_Open(i, &videoDecoderOpenSettings); 
        if(!videoDecoder[i]) while(1){LOGE("NEXUS_VideoDecoder_Open(%d) failed!!",i);}    

        NEXUS_VideoDecoderSettings videoDecoderSettings;
        NEXUS_PlatformSettings     platformSettings;
        NEXUS_Platform_GetSettings(&platformSettings);
        if ((i==0) && (platformSettings.videoDecoderModuleSettings.supportedCodecs[NEXUS_VideoCodec_eH265] == NEXUS_VideoCodec_eH265))
        {
            NEXUS_VideoDecoder_GetSettings(videoDecoder[i], &videoDecoderSettings);
            videoDecoderSettings.supportedCodecs[NEXUS_VideoCodec_eH265] = NEXUS_VideoCodec_eH265;
            videoDecoderSettings.maxWidth  = 3840; 
            videoDecoderSettings.maxHeight = 2160;
            NEXUS_VideoDecoder_SetSettings(videoDecoder[i], &videoDecoderSettings);
        }

        // create simple video decoder 
        NEXUS_SimpleVideoDecoder_GetDefaultServerSettings(&settings);
        settings.videoDecoder = videoDecoder[i];
        settings.window[0] = NULL;
        simpleVideoDecoder[i] = NEXUS_SimpleVideoDecoder_Create(i, &settings);
        if(!simpleVideoDecoder[i]) while(1){LOGE("NEXUS_SimpleVideoDecoder_Open(%d) failed!!",i);}        

#ifndef ENABLE_BCM_OMX_PROTOTYPE
        //
        // by default give video_window to the simplevideodecoder
        // Note: simplevideodecoder_start will connect videodecoder to the videowindow
        // later if we want to disconnect and connect other videoinput (say, HDMI-input)
        // stop simplevideodecoder, set below to NULL and add new videoinput explicitly 
        // to the video window.
        //
        NEXUS_SimpleVideoDecoder_GetServerSettings(simpleVideoDecoder[i], &settings);
        for(int j=0; j<MAX_NUM_DISPLAYS; j++) 
        {
            settings.window[j] = displayState[j].video_window[i];
        }
        NEXUS_SimpleVideoDecoder_SetServerSettings(simpleVideoDecoder[i], &settings);
#endif
    }

#ifdef ENABLE_BCM_OMX_PROTOTYPE
    /* 
     * Tell Display module to connect to the VideoDecoder module and supply the
     * L1 INT id's from BVDC_Display_GetInterrupt. Display will not register for the data ready ISR callback. 
     * This is just to implement the getFrame API.
     */
    NEXUS_Display_ConnectVideoInput(displayState[0].display, NEXUS_VideoDecoder_GetConnector(videoDecoder[0]));
#endif
    return;
}

void NexusService::setVideoState(bool enable)
{
    for(int i=0; i<2; i++)
    {
        NEXUS_SimpleVideoDecoderServerSettings settings;

        NEXUS_SimpleVideoDecoder_GetServerSettings(simpleVideoDecoder[i], &settings);
        settings.enabled = enable;
        NEXUS_SimpleVideoDecoder_SetServerSettings(simpleVideoDecoder[i], &settings);
    }
    return;
}

void framebuffer_callback(void *context, int param)
{
    BSTD_UNUSED(context);
    BSTD_UNUSED(param);
}

static BKNI_EventHandle inactiveEvent;
static void inactive_callback(void *context, int param)
{    
    BSTD_UNUSED(param);    
    BKNI_SetEvent((BKNI_EventHandle)context);
}

/* Event callback that will be called when a gfx op is complete */
static void complete(void *data, int unused)
{
    BSTD_UNUSED(unused);
    BKNI_SetEvent((BKNI_EventHandle)data);
}

void NexusService::platformInitSurfaceCompositor(void)
{
    NEXUS_SurfaceCreateSettings createSettings;
    int rc=0; unsigned i;
    NEXUS_SurfaceCompositorSettings *p_surface_compositor_settings = NULL;
    NEXUS_VideoFormatInfo formatInfo;
    NEXUS_VideoFormat_GetInfo(initial_hd_format, &formatInfo);
            
    BKNI_CreateEvent(&inactiveEvent);
    p_surface_compositor_settings = (NEXUS_SurfaceCompositorSettings *)BKNI_Malloc(sizeof(NEXUS_SurfaceCompositorSettings));
    if(NULL == p_surface_compositor_settings) 
    {
        while(1) LOGE("%s:%d BKNI_Malloc failed",__FUNCTION__,__LINE__);
        return;
    }

    /* create surface compositor server */    
    surface_compositor = NEXUS_SurfaceCompositor_Create(0);
    NEXUS_SurfaceCompositor_GetSettings(surface_compositor, p_surface_compositor_settings);
    NEXUS_Display_GetGraphicsSettings(displayState[HD_DISPLAY].display, &p_surface_compositor_settings->display[HD_DISPLAY].graphicsSettings);
    // Give space for videowindows to change zorder without using same one twice
    p_surface_compositor_settings->display[HD_DISPLAY].graphicsSettings.zorder = MAX_VIDEO_WINDOWS_PER_DISPLAY + 1;
    p_surface_compositor_settings->display[HD_DISPLAY].graphicsSettings.enabled = true;
    p_surface_compositor_settings->display[HD_DISPLAY].display = displayState[HD_DISPLAY].display;
    p_surface_compositor_settings->display[HD_DISPLAY].framebuffer.number = 2;
    p_surface_compositor_settings->display[HD_DISPLAY].framebuffer.width = formatInfo.width;
    p_surface_compositor_settings->display[HD_DISPLAY].framebuffer.height = formatInfo.height;
    p_surface_compositor_settings->display[HD_DISPLAY].framebuffer.backgroundColor = 0; /* black background */
    p_surface_compositor_settings->display[HD_DISPLAY].framebuffer.heap = NEXUS_Platform_GetFramebufferHeap(0);
#if 0
    // give HD video windows to NSC
    for(i=0; i<MAX_VIDEO_WINDOWS_PER_DISPLAY; i++)
    {
        p_surface_compositor_settings->display[HD_DISPLAY].window[i].window = displayState[HD_DISPLAY].video_window[i];
        if(displayState[HD_DISPLAY].video_window[i])
            NEXUS_VideoWindow_GetSettings(displayState[HD_DISPLAY].video_window[i], &p_surface_compositor_settings->display[HD_DISPLAY].window[i].settings);
    }
#endif

    if((NEXUS_VideoFormat_e720p_3DOU_AS == initial_hd_format) || (NEXUS_VideoFormat_e1080p24hz_3DOU_AS == initial_hd_format))
    {
        p_surface_compositor_settings->display[HD_DISPLAY].display3DSettings.overrideOrientation = true;
        p_surface_compositor_settings->display[HD_DISPLAY].display3DSettings.orientation = NEXUS_VideoOrientation_e2D;
    }
    if(MAX_NUM_DISPLAYS > 1)
    {
        NEXUS_Display_GetGraphicsSettings(displayState[SD_DISPLAY].display, &p_surface_compositor_settings->display[SD_DISPLAY].graphicsSettings);
        p_surface_compositor_settings->display[SD_DISPLAY].graphicsSettings.enabled = true;
        p_surface_compositor_settings->display[SD_DISPLAY].display = displayState[SD_DISPLAY].display;
        NEXUS_VideoFormat_GetInfo(initial_sd_format, &formatInfo);
        p_surface_compositor_settings->display[SD_DISPLAY].framebuffer.width = formatInfo.width;
        p_surface_compositor_settings->display[SD_DISPLAY].framebuffer.height = formatInfo.height;
        p_surface_compositor_settings->display[SD_DISPLAY].framebuffer.backgroundColor = 0; /* black background */
        p_surface_compositor_settings->display[SD_DISPLAY].framebuffer.heap = NEXUS_Platform_GetFramebufferHeap(1);
#if 0
        // give SD video windows to NSC
        for(i=0; i<MAX_VIDEO_WINDOWS_PER_DISPLAY; i++)
        {
            p_surface_compositor_settings->display[SD_DISPLAY].window[i].window = displayState[SD_DISPLAY].video_window[i];
            if(displayState[SD_DISPLAY].video_window[i])
                NEXUS_VideoWindow_GetSettings(displayState[SD_DISPLAY].video_window[i], &p_surface_compositor_settings->display[SD_DISPLAY].window[i].settings);
        }
#endif
    }
    p_surface_compositor_settings->frameBufferCallback.callback = framebuffer_callback;
    p_surface_compositor_settings->frameBufferCallback.context = surface_compositor;
    p_surface_compositor_settings->inactiveCallback.callback = inactive_callback;
    p_surface_compositor_settings->inactiveCallback.context = inactiveEvent;
    rc = NEXUS_SurfaceCompositor_SetSettings(surface_compositor, p_surface_compositor_settings);
    if(rc) while(1) LOGE("%s:%d NSC_SetSettings failed",__FUNCTION__,__LINE__);
    BDBG_ASSERT(!rc);

    if(p_surface_compositor_settings)
        BKNI_Free(p_surface_compositor_settings);
}

void NexusService::platformInit()
{
    NEXUS_Error rc;
    int i=0;
    char value[PROPERTY_VALUE_MAX];
    NEXUS_Graphics2DSettings        gfxSettings;
    NEXUS_PlatformConfiguration platformConfig;
    NEXUS_PlatformStartServerSettings serverSettings;

    NEXUS_Platform_GetConfiguration(&platformConfig);
    NEXUS_Platform_GetDefaultStartServerSettings(&serverSettings);

    serverSettings.allowUnprotectedClientsToCrash = true;
    serverSettings.allowUnauthenticatedClients = true;
    serverSettings.unauthenticatedConfiguration.mode = (NEXUS_ClientMode) ANDROID_CLIENT_SECURITY_MODE;
    LOGI("***************************\n\tStarting server in %d mode \n*************************",ANDROID_CLIENT_SECURITY_MODE);
    serverSettings.unauthenticatedConfiguration.heap[0] = NEXUS_Platform_GetFramebufferHeap(NEXUS_OFFSCREEN_SURFACE);
    serverSettings.unauthenticatedConfiguration.heap[1] = platformConfig.heap[NEXUS_MEMC0_MAIN_HEAP];
#if (BCHP_CHIP == 7425) || (BCHP_CHIP == 7435)
    serverSettings.unauthenticatedConfiguration.heap[2] = platformConfig.heap[NEXUS_MEMC1_MAIN_HEAP];
    serverSettings.unauthenticatedConfiguration.heap[3] = platformConfig.heap[NEXUS_MEMC0_DRIVER_HEAP];   
#endif
    rc = NEXUS_Platform_StartServer(&serverSettings);
    if(rc!=NEXUS_SUCCESS) 
    {
        LOGE("%s:NEXUS_Platform_StartServer Failed (rc=%d)!\n", __PRETTY_FUNCTION__, rc);
        BDBG_ASSERT(rc == NEXUS_SUCCESS);
    }

    for(i=0; i<MAX_NUM_DISPLAYS; i++)
    {
        BKNI_Memset(&displayState[i], 0, sizeof(DisplayState));
    }
    for(i=0; i<MAX_AUDIO_DECODERS; i++)
    {
        audioDecoder[i] = NULL;
        audioPlayback[i] = NULL;
        simpleAudioDecoder[i] = NULL;
        simpleAudioPlayback[i] = NULL;
    }
    for(i=0; i<MAX_VIDEO_DECODERS; i++)
    {
        videoDecoder[i] = NULL;
        simpleVideoDecoder[i] = NULL;
    }
    for(i=0; i<MAX_ENCODERS; i++)
    {
        simpleEncoder[i] = NULL;
    }

    get_initial_output_formats_from_property(&initial_hd_format, &initial_sd_format);

    gfx2D = NEXUS_Graphics2D_Open(NEXUS_ANY_ID,NULL);
        
    BKNI_CreateEvent(&gfxDone);
    NEXUS_Graphics2D_GetSettings(gfx2D, &gfxSettings);
    gfxSettings.checkpointCallback.callback = complete;
    gfxSettings.checkpointCallback.context = gfxDone;
    NEXUS_Graphics2D_SetSettings(gfx2D, &gfxSettings);
    platformInitAudio();
    platformInitVideo();
    platformInitSurfaceCompositor();

#if NEXUS_HAS_CEC
    i = NEXUS_NUM_CEC;
    while (i--) {
        if (isCecEnabled(i)) {
            mCecServiceManager[i] = CecServiceManager::instantiate(i);

            if (mCecServiceManager[i] != NULL) {
                if (mCecServiceManager[i]->platformInit() != OK) {
                    LOGE("%s: ERROR initialising CecServiceManager platform for CEC%d!", __FUNCTION__, i);
                    mCecServiceManager[i] = NULL;
                }
            }
            else {
                LOGE("%s: ERROR instantiating CecServiceManager for CEC%d!", __FUNCTION__, i);
            }
        }
    }
#endif

    // special handling for 1080p HD display
    if(property_get("ro.hd_output_format", value, NULL)) 
    {
        if (strncmp((char *) value, "1080p",5)==0)
        {
            LOGW("Set HD output format to 1080p...");
            NEXUS_DisplaySettings settings;
            getDisplaySettings(HD_DISPLAY, &settings);
            settings.format = NEXUS_VideoFormat_e1080p60hz;
            setDisplaySettings(HD_DISPLAY, &settings);
        }
    }
}

void NexusService::platformUninit()
{
#if NEXUS_HAS_CEC
    for (unsigned i = 0; i < NEXUS_NUM_CEC; i++) {
        if (mCecServiceManager[i] != NULL) {
            mCecServiceManager[i]->platformUninit();
            mCecServiceManager[i] = NULL;
        }
    }
#endif

    if(gfx2D) {
        NEXUS_Graphics2D_Close(gfx2D);
        gfx2D = NULL;
    }
}

void NexusService::instantiate() {
    NexusService *nexusservice = new NexusService();

    nexusservice->platformInit();

    defaultServiceManager()->addService(
                INexusService::descriptor, nexusservice);
}

NexusService::NexusService() : powerState(ePowerState_S0)
{
    server = new NexusServerContext();

    if (server == NULL) {
        LOGE("%s: FATAL: Could not allocate memory for NexusServerContext!", __PRETTY_FUNCTION__);
        BDBG_ASSERT(server != NULL);
    }
    LOGI("NexusService Created");
    surface_compositor = NULL;
    surfaceclient = NULL;
    gfx2D = NULL;
}

NexusService::~NexusService()
{
    LOGI("NexusService Destroyed");

    platformUninit();

    delete server;
    server = NULL;
}

NEXUS_ClientHandle NexusService::clientJoin(const b_refsw_client_client_name *pClientName, NEXUS_ClientAuthenticationSettings *pClientAuthenticationSettings)
{
    NEXUS_ClientHandle nexusClient;

    Mutex::Autolock autoLock(server->mLock);

    nexusClient = NULL;

#ifdef MAX_CLIENTS
    if (server->mJoinRefCount < MAX_CLIENTS) {
#else
    if (true) {
#endif
        pClientAuthenticationSettings->certificate.length =
            BKNI_Snprintf((char *)pClientAuthenticationSettings->certificate.data,
                          sizeof(pClientAuthenticationSettings->certificate.data),
                          "%u,%#x%#x,%s", server->lastId.client, lrand48(), lrand48(), pClientName->string);

        if (pClientAuthenticationSettings->certificate.length >= sizeof(pClientAuthenticationSettings->certificate.data)-1) {
            LOGE("%s: Invalid certificate length %d for client \"%s\"!!!", __FUNCTION__, pClientAuthenticationSettings->certificate.length, pClientName->string);
            (void)BERR_TRACE(BERR_NOT_SUPPORTED);
        }
        else {
            NEXUS_PlatformConfiguration platformConfig;
            NEXUS_ClientSettings        clientSettings;

            LOGI("client %s registering as '%s'", pClientName->string, (char *)pClientAuthenticationSettings->certificate.data);

            NEXUS_Platform_GetDefaultClientSettings(&clientSettings);
            clientSettings.authentication.certificate = pClientAuthenticationSettings->certificate;
            NEXUS_Platform_GetConfiguration(&platformConfig);
            clientSettings.configuration.heap[0] = platformConfig.heap[0];
            clientSettings.configuration.mode = (NEXUS_ClientMode)ANDROID_CLIENT_SECURITY_MODE;
            nexusClient = NEXUS_Platform_RegisterClient(&clientSettings);
            if (nexusClient) {
                LOGI("%s: Successfully registered client \"%s\".", __FUNCTION__, pClientName->string);
                server->lastId.client++;
                server->mJoinRefCount++;
            }
            else {
                LOGE("%s: Could not register client \"%s\"!!!", __FUNCTION__, pClientName->string);
                (void)BERR_TRACE(BERR_NOT_SUPPORTED);
            }
        }
    }
    else {
        LOGE("%s: FATAL: too many clients already joined!!!", __FUNCTION__);
    }
    return nexusClient;
}

NEXUS_Error NexusService::clientUninit(NEXUS_ClientHandle nexusClient)
{
    NEXUS_Error rc;

    Mutex::Autolock autoLock(server->mLock);

#ifdef MAX_CLIENTS
    if (server->mJoinRefCount > 0) {
#else
    if (true) {
#endif
        server->mJoinRefCount--;

        if (nexusClient == NULL) {
            LOGE("%s: Nexus client handle is NULL!!!", __FUNCTION__);
            rc = NEXUS_INVALID_PARAMETER;
        }
        else {
            NEXUS_Platform_UnregisterClient(nexusClient);
            rc = NEXUS_SUCCESS;
        }
    }
    else {
        LOGE("%s: No clients have joined the service!", __FUNCTION__);
        rc = NEXUS_NOT_INITIALIZED;
    }

    return rc;
}

NexusClientContext * NexusService::createClientContext(const b_refsw_client_client_configuration *config)
{
    NexusClientContext * client;
    NEXUS_ClientSettings clientSettings;
    NEXUS_Error rc;

    NEXUS_PlatformConfiguration platformConfig;

    client = (NexusClientContext *)BKNI_Malloc(sizeof(NexusClientContext));
    if (client==NULL) {
        (void)BERR_TRACE(BERR_OUT_OF_SYSTEM_MEMORY);
        return NULL;
    }

    BKNI_Memset(client, 0, sizeof(*client));
    BDBG_OBJECT_SET(client, NexusClientContext);

    client->createConfig = *config;

    BLST_D_INSERT_HEAD(&server->clients, client, link);

    client->ipc.nexusClient = getNexusClient(client);

    // This is used to indicate whether client has requested HDMI input or not...
    if (config->resources.hdmiInput > 0) {
        client->info.hdmiInputId = 1;
    }

    if (powerState != ePowerState_S0) {
        NEXUS_PlatformStandbySettings nexusStandbySettings;

        LOGI("We need to set Nexus Power State S0 first...");
        NEXUS_Platform_GetStandbySettings(&nexusStandbySettings);
        nexusStandbySettings.mode = NEXUS_PlatformStandbyMode_eOn;    
        rc = NEXUS_Platform_SetStandbySettings(&nexusStandbySettings);
        if (rc != NEXUS_SUCCESS) {
            LOGE("Oops we couldn't set Nexus Power State to S0!");
            goto err_client;
        }
        else {
            LOGI("Successfully set Nexus Power State S0");
        }
    }
    LOGI("%s: Exiting with client=%p", __FUNCTION__, (void *)client);
    return client;

err_client:
    /* todo fix cleanup */
    return NULL;
}

void NexusService::destroyClientContext(NexusClientContext * client)
{
    BDBG_OBJECT_ASSERT(client, NexusClientContext);
    if(client->resources.videoSurface) {
        NEXUS_SurfaceCompositor_DestroyClient(client->resources.videoSurface);
        client->resources.videoSurface = NULL;
    }

    if(client->resources.graphicsSurface) {
        NEXUS_SurfaceCompositor_DestroyClient(client->resources.graphicsSurface);
        client->resources.graphicsSurface = NULL;
    }
    if(client->ipc.nexusClient) {
        client->ipc.nexusClient = NULL;
    }
    BLST_D_REMOVE(&server->clients, client, link);
    BDBG_OBJECT_DESTROY(client, NexusClientContext);
    BKNI_Free(client);
}

bool NexusService::addGraphicsWindow(NexusClientContext * client)
{
    char value[PROPERTY_VALUE_MAX];
    bool enable_offset = false;
    int rc;
    int xoff=0, yoff=0;
    unsigned width=0, height=0;
    b_refsw_client_client_configuration *config = &client->createConfig;

    NEXUS_SurfaceCompositorClientSettings *p_client_settings = NULL;

    LOGD("%s[%d]: >>>>>>>>>>>>>>>>>>>>>>>>>> addGraphicsWindow Called Creating NSCClient[IPC] <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<",__FUNCTION__,__LINE__);
    /* See if we need to tweak the graphics to fit on the screen */
    if(property_get("ro.screenresize.x", value, NULL))
    {
        xoff = atoi(value);
        if(property_get("ro.screenresize.y", value, NULL))
        {
            yoff = atoi(value);
            if(property_get("ro.screenresize.w", value, NULL))
            {
                width = atoi(value);
                if(property_get("ro.screenresize.h", value, NULL))
                {
                    height = atoi(value);
                    enable_offset = true;
                }
            }
        }
    }

    /* If user has not set gfx window size, read them from HD output's properties */
    if((config->resources.screen.position.width == 0) && (config->resources.screen.position.height == 0))
    {
        NEXUS_VideoFormatInfo fmt_info;
        NEXUS_VideoFormat hd_fmt, sd_fmt;
        
        get_initial_output_formats_from_property(&hd_fmt, NULL);
        NEXUS_VideoFormat_GetInfo(hd_fmt, &fmt_info);

        config->resources.screen.position.width = fmt_info.width;
        config->resources.screen.position.height = fmt_info.height;
    }

    client->resources.graphicsSurface = NEXUS_SurfaceCompositor_CreateClient(surface_compositor, server->lastId.surfaceClientId);
    if(!client->resources.graphicsSurface)
    {
        (void)BERR_TRACE(BERR_NOT_SUPPORTED);
        goto err_screen;
    }
    // save the surface compositor client
    surfaceclient = client->resources.graphicsSurface;
    
    client->info.surfaceClientId = server->lastId.surfaceClientId;
    p_client_settings = (NEXUS_SurfaceCompositorClientSettings *)BKNI_Malloc(sizeof(NEXUS_SurfaceCompositorClientSettings));
    if(p_client_settings)
    {
        NEXUS_SurfaceCompositor_GetClientSettings(surface_compositor, client->resources.graphicsSurface, p_client_settings);
        p_client_settings->composition.position = config->resources.screen.position;
        p_client_settings->composition.zorder = client->info.surfaceClientId;
        p_client_settings->composition.virtualDisplay.width = config->resources.screen.position.width;
        p_client_settings->composition.virtualDisplay.height = config->resources.screen.position.height;

        if(enable_offset)
        {
            LOGD("######### REPOSITIONING REQUIRED %d %d %d %d ###############\n",xoff,yoff,width,height);
            p_client_settings->composition.clipRect.width = p_client_settings->composition.virtualDisplay.width;
            p_client_settings->composition.clipRect.height = p_client_settings->composition.virtualDisplay.height;
            p_client_settings->composition.position.x = xoff;
            p_client_settings->composition.position.y = yoff;
            p_client_settings->composition.position.width = width;
            p_client_settings->composition.position.height = height;
        }

        rc = NEXUS_SurfaceCompositor_SetClientSettings(surface_compositor, client->resources.graphicsSurface, p_client_settings);
        BKNI_Free(p_client_settings);
        if(rc!=NEXUS_SUCCESS)
        {
            (void)BERR_TRACE(BERR_NOT_SUPPORTED);
            goto err_screen_settings;
        }
        server->lastId.surfaceClientId++;
        return true;
    }

err_screen_settings:
    NEXUS_SurfaceCompositor_DestroyClient(client->resources.graphicsSurface);
err_screen:
    return false;
}

void NexusService::getClientInfo(NexusClientContext * client, b_refsw_client_client_info *info)
{
    BDBG_OBJECT_ASSERT(client, NexusClientContext);
    *info = client->info;
}


void NexusService::getClientComposition(NexusClientContext * client, NEXUS_SurfaceComposition *pComposition)
{
    NEXUS_SurfaceCompositorClientSettings *p_client_settings = NULL;

    p_client_settings = (NEXUS_SurfaceCompositorClientSettings *)BKNI_Malloc(sizeof(NEXUS_SurfaceCompositorClientSettings));
    if(p_client_settings)
    {
        NEXUS_SurfaceCompositor_GetClientSettings(surface_compositor, surfaceclient, p_client_settings);
        *pComposition = p_client_settings->composition;
        BKNI_Free(p_client_settings);
    }
}

void NexusService::setClientComposition(NexusClientContext * client, NEXUS_SurfaceComposition *pComposition)
{
    NEXUS_SurfaceCompositorClientSettings *p_client_settings = NULL;
    NEXUS_Error rc;

    p_client_settings = (NEXUS_SurfaceCompositorClientSettings *)BKNI_Malloc(sizeof(NEXUS_SurfaceCompositorClientSettings));
    if(p_client_settings)
    {
        NEXUS_SurfaceCompositor_GetClientSettings(surface_compositor, surfaceclient, p_client_settings);    
        p_client_settings->composition = *pComposition;
        LOGD("%s: setting client composition [%d,%d,%d,%d]",__FUNCTION__, pComposition->position.x,
        pComposition->position.y, pComposition->position.width, pComposition->position.height);
    
        rc = NEXUS_SurfaceCompositor_SetClientSettings(surface_compositor, surfaceclient, p_client_settings);
        if(rc != NEXUS_SUCCESS)
            LOGE("%s:%d NEXUS_SurfaceCompositor_SetClientSettings() returned error, rc=%d",__FUNCTION__,__LINE__,rc);

        BKNI_Free(p_client_settings);
    }
    return;
}

void NexusService::getVideoWindowSettings(NexusClientContext * client, uint32_t window_id, b_video_window_settings *settings)
{
    NEXUS_VideoWindowSettings nSettings;

    if (window_id >= MAX_VIDEO_WINDOWS_PER_DISPLAY) {
        LOGE("%s: window_id(%d) cannot be >= %d!", __FUNCTION__, window_id, MAX_VIDEO_WINDOWS_PER_DISPLAY);
        return;
    }

    // Always return settings for primary display (HD_DISPLAY) as client code works only on HD_DISPLAY
    if(displayState[HD_DISPLAY].video_window[window_id])
    {
        NEXUS_DisplaySettings disp_settings;
        NEXUS_VideoFormatInfo fmt_info;
        
        /* Get actual display resolution to populate virtual display settings... */
        NEXUS_Display_GetSettings(displayState[HD_DISPLAY].display, &disp_settings);
        NEXUS_VideoFormat_GetInfo(disp_settings.format, &fmt_info);
        settings->virtualDisplay.width  = fmt_info.width;
        settings->virtualDisplay.height = fmt_info.height;

        NEXUS_VideoWindow_GetSettings(displayState[HD_DISPLAY].video_window[window_id], &nSettings);
        settings->position      = nSettings.position;
        settings->clipRect      = nSettings.clipRect;
        settings->visible       = nSettings.visible;
        settings->contentMode   = nSettings.contentMode;
        settings->autoMaster    = nSettings.autoMaster;
        settings->zorder        = nSettings.zorder;
    }
    return;
}

void NexusService::setVideoWindowSettings(NexusClientContext * client, uint32_t window_id, b_video_window_settings *settings)
{
    NEXUS_VideoWindowSettings nSettings;

    if (window_id >= MAX_VIDEO_WINDOWS_PER_DISPLAY) {
        LOGE("%s: window_id(%d) cannot be >= %d!", __FUNCTION__, window_id, MAX_VIDEO_WINDOWS_PER_DISPLAY);
        return;
    }

    for(int display_id = HD_DISPLAY; display_id < MAX_NUM_DISPLAYS; display_id++)
    {
        if(displayState[display_id].video_window[window_id])
        {
            NEXUS_VideoWindow_GetSettings(displayState[display_id].video_window[window_id], &nSettings);
            if(display_id == HD_DISPLAY)
            {
                /* NOTE: virtual display settings are ignored. */
                nSettings.position        = settings->position;
                nSettings.clipRect        = settings->clipRect;
                nSettings.visible         = settings->visible;
                nSettings.contentMode     = settings->contentMode;
                nSettings.autoMaster      = settings->autoMaster;
                nSettings.zorder          = settings->zorder;
            }
            else
            {                
                // User has passed the settings for HD_DISPLAY, so do the necessary HD to SD translation (wherever applicable) before passing it to nexus
                uint32_t sd_width, sd_height;
                uint32_t hd_width, hd_height;
                NEXUS_VideoWindowSettings hd_win_settings;

                /* settings might be set based on the old output format, we should use current output format and window setting */
                NEXUS_DisplaySettings disp_settings;
                NEXUS_VideoFormatInfo fmt_info;
                
                /* get hd display setting and window settings first */
                NEXUS_Display_GetSettings(displayState[HD_DISPLAY].display, &disp_settings);
                NEXUS_VideoWindow_GetSettings(displayState[HD_DISPLAY].video_window[window_id], &hd_win_settings);
                NEXUS_VideoFormat_GetInfo(disp_settings.format, &fmt_info);
                hd_width = fmt_info.width;
                hd_height = fmt_info.height;

                /* then get sd display setting */
                NEXUS_Display_GetSettings(displayState[display_id].display, &disp_settings);
                NEXUS_VideoFormat_GetInfo(disp_settings.format, &fmt_info); 
                sd_width = fmt_info.width;
                sd_height = fmt_info.height;
                
                nSettings.position.x      = (hd_win_settings.position.x * sd_width) / hd_width;
                nSettings.position.y      = (hd_win_settings.position.y * sd_height) / hd_height;
                nSettings.position.width  = (hd_win_settings.position.width * sd_width) / hd_width;
                nSettings.position.height = (hd_win_settings.position.height * sd_height) / hd_height;
                nSettings.clipRect.x      = (hd_win_settings.clipRect.x * sd_width) / hd_width;
                nSettings.clipRect.y      = (hd_win_settings.clipRect.y * sd_width) / hd_width;
                nSettings.clipRect.width  = (hd_win_settings.clipRect.width * sd_width) / hd_width;
                nSettings.clipRect.height = (hd_win_settings.clipRect.height * sd_width) / hd_width;

                //Below are needed or else aspect ratio changes will not come into effect
                nSettings.visible       = settings->visible;
                nSettings.contentMode   = settings->contentMode;
                nSettings.autoMaster    = settings->autoMaster;
            }

            LOGV ("position.width %d position.height %d %s %d", nSettings.position.width, nSettings.position.height, __FUNCTION__, __LINE__);
            if ((nSettings.position.width < 2) || (nSettings.position.height < 1))
            {
              LOGE ("window width %d x height %d is too small", nSettings.position.width, nSettings.position.height);
              // interlaced content min is 2x2
              nSettings.position.width = 2;
              nSettings.position.height = 2;
            }

            NEXUS_VideoWindow_SetSettings(displayState[display_id].video_window[window_id], &nSettings);            
        }
    }//for
    return;
}

void NexusService::getDisplaySettings(uint32_t display_id, NEXUS_DisplaySettings *settings)
{
    if (display_id >= MAX_NUM_DISPLAYS)
    {
        LOGE("display_id(%d) cannot be more than 1!",display_id);
        return;
    }
    if(displayState[display_id].display)
    {
        NEXUS_Display_GetSettings(displayState[display_id].display, settings);
    }
    else
        LOGE("displayHandle[%d] is NULL",display_id);
    
    return;
}

void NexusService::setDisplayState(bool enable)
{
    NEXUS_SurfaceCompositorSettings *p_surface_compositor_settings = NULL;
    int rc;
    
    p_surface_compositor_settings = (NEXUS_SurfaceCompositorSettings *)BKNI_Malloc(sizeof(NEXUS_SurfaceCompositorSettings));
    if(NULL == p_surface_compositor_settings) 
    {
        while(1) LOGE("%s:%d BKNI_Malloc failed",__FUNCTION__,__LINE__);
        return;
    }
    
    NEXUS_SurfaceCompositor_GetSettings(surface_compositor, p_surface_compositor_settings);

    if (!enable) {
        BKNI_ResetEvent(inactiveEvent);
        
        /* disable surface compositor */        
        p_surface_compositor_settings->enabled = false;
        NEXUS_SurfaceCompositor_SetSettings(surface_compositor, p_surface_compositor_settings);
        
        rc = BKNI_WaitForEvent(inactiveEvent, 5000);        
        if (rc)            
        {   
            LOGE("Did not receive NSC inactive event!");
            if(p_surface_compositor_settings) 
                BKNI_Free(p_surface_compositor_settings);
            return;
        }
    }
    else {
        /* reenable surface compositor, framebuffer size should be changed */
        p_surface_compositor_settings->enabled = true;
        NEXUS_SurfaceCompositor_SetSettings(surface_compositor, p_surface_compositor_settings);
    }
    
    if(p_surface_compositor_settings) 
        BKNI_Free(p_surface_compositor_settings);
    return;
}

void NexusService::setDisplaySettings(uint32_t display_id, NEXUS_DisplaySettings *settings)
{
    if (display_id >= MAX_NUM_DISPLAYS)
    {
        LOGE("display_id(%d) cannot be more than 1!",display_id);
        return;
    }

    if(displayState[display_id].display)    
    {        
        /* set display setting, now we only support format change. sd display format should be changed based on hd display format */        
        NEXUS_SurfaceCompositorSettings *p_surface_compositor_settings = NULL;
        NEXUS_DisplaySettings disp_settings;
        NEXUS_VideoFormatInfo formatInfo;
        int rc;
        
        NEXUS_Display_GetSettings(displayState[display_id].display, &disp_settings);
        if (disp_settings.format == settings->format)
        {  
            LOGE("display_id(%d) no format change ",display_id);
            return;
        }

        p_surface_compositor_settings = (NEXUS_SurfaceCompositorSettings *)BKNI_Malloc(sizeof(NEXUS_SurfaceCompositorSettings));
        if(NULL == p_surface_compositor_settings) 
        {
            while(1) LOGE("%s:%d BKNI_Malloc failed",__FUNCTION__,__LINE__);
            return;
        }

        BKNI_ResetEvent(inactiveEvent);
        
        /* disable surface compositor */        
        NEXUS_SurfaceCompositor_GetSettings(surface_compositor, p_surface_compositor_settings);
        p_surface_compositor_settings->enabled = false;
        NEXUS_SurfaceCompositor_SetSettings(surface_compositor, p_surface_compositor_settings);
        
        rc = BKNI_WaitForEvent(inactiveEvent, 5000);        
        if (rc)            
        {   
            LOGE("Did not receive NSC inactive event - not changing the display resolution for display=%d",display_id);
            if(p_surface_compositor_settings) 
                BKNI_Free(p_surface_compositor_settings);
            return;
        }
        
        NEXUS_Display_SetSettings(displayState[display_id].display, settings);

        NEXUS_VideoFormat_GetInfo(settings->format, &formatInfo);

        LOGD("--- display_id(%d) setDisplaySettings %d, w %d, h %d",display_id, settings->format, formatInfo.width, formatInfo.height);
        /* reenable surface compositor, framebuffer size should be changed */
        NEXUS_SurfaceCompositor_GetSettings(surface_compositor, p_surface_compositor_settings);
        p_surface_compositor_settings->enabled = true;
        p_surface_compositor_settings->display[display_id].framebuffer.width = formatInfo.width;
        p_surface_compositor_settings->display[display_id].framebuffer.height = formatInfo.height;

        /* NSC settings when transitioning to/from 3D display format */
        if(HD_DISPLAY == display_id)
        {    
            if ((NEXUS_VideoFormat_e720p_3DOU_AS == settings->format) || (NEXUS_VideoFormat_e1080p24hz_3DOU_AS == settings->format))
            {
                // when transitioning to 3D display format, following are needed or else vertical display resolution will be half
                p_surface_compositor_settings->display[HD_DISPLAY].display3DSettings.overrideOrientation = true;
                p_surface_compositor_settings->display[HD_DISPLAY].display3DSettings.orientation = NEXUS_VideoOrientation_e2D;
            }
            else
            {
                // when transitioning from 3D display format, disable overrideOrientation
                p_surface_compositor_settings->display[HD_DISPLAY].display3DSettings.overrideOrientation = false;
                p_surface_compositor_settings->display[HD_DISPLAY].display3DSettings.orientation = NEXUS_VideoOrientation_e2D;
            }
        }
        
        if (display_id == SD_DISPLAY) /* no scaler in gfd1, we should use the whole sd framebuffer */
            p_surface_compositor_settings->display[display_id].graphicsSettings.clip.width = 0;
        
        NEXUS_SurfaceCompositor_SetSettings(surface_compositor, p_surface_compositor_settings);
        
        if(p_surface_compositor_settings) 
            BKNI_Free(p_surface_compositor_settings);
    }
    else
        LOGE("displayHandle[%d] is NULL",display_id);        
    return;
}

void NexusService::getPictureCtrlCommonSettings(uint32_t window_id, NEXUS_PictureCtrlCommonSettings *settings)
{
    if (window_id >= MAX_VIDEO_WINDOWS_PER_DISPLAY) {
        LOGE("%s: window_id(%d) cannot be >= %d!", __FUNCTION__, window_id, MAX_VIDEO_WINDOWS_PER_DISPLAY);
        return;
    }

    // Always return settings for primary display (HD_DISPLAY) as client code works only on HD_DISPLAY
    if (displayState[HD_DISPLAY].video_window[window_id]) {
        NEXUS_PictureCtrl_GetCommonSettings(displayState[HD_DISPLAY].video_window[window_id],settings);
    }
    return;
}

void NexusService::setPictureCtrlCommonSettings(uint32_t window_id, NEXUS_PictureCtrlCommonSettings *settings)
{
    if (window_id >= MAX_VIDEO_WINDOWS_PER_DISPLAY) {
        LOGE("%s: window_id(%d) cannot be >= %d!", __FUNCTION__, window_id, MAX_VIDEO_WINDOWS_PER_DISPLAY);
        return;
    }

    for (int display_id = HD_DISPLAY; display_id < MAX_NUM_DISPLAYS; display_id++) {
        if (displayState[display_id].video_window[window_id]) {
            NEXUS_PictureCtrl_SetCommonSettings(displayState[display_id].video_window[window_id],settings);
        }
    }
    return;
}

void NexusService::getGraphicsColorSettings(uint32_t display_id, NEXUS_GraphicsColorSettings *settings)
{
    
    if (display_id >= MAX_NUM_DISPLAYS) {
        LOGE("%s: display_id(%d) cannot be >= %d!", __FUNCTION__, display_id, MAX_NUM_DISPLAYS);
        return;
    }

    if (displayState[display_id].display) {
        NEXUS_Display_GetGraphicsColorSettings(displayState[display_id].display, settings);
    }
    else {
        LOGE("%s: displayHandle[%d] is NULL!", __FUNCTION__, display_id);        
    }
    return;
}

void NexusService::setGraphicsColorSettings(uint32_t display_id, NEXUS_GraphicsColorSettings *settings)
{
    if (display_id >= MAX_NUM_DISPLAYS) {
        LOGE("%s: display_id(%d) cannot be >= %d!", __FUNCTION__, display_id, MAX_NUM_DISPLAYS);
        return;
    }

    if (displayState[display_id].display) {        
        NEXUS_Display_SetGraphicsColorSettings(displayState[display_id].display, settings);        
    }
    else {
        LOGE("%s: displayHandle[%d] is NULL!", __FUNCTION__, display_id);        
    }
    return;
}

void NexusService::setDisplayOutputs(int display)
{
    NEXUS_PlatformConfiguration platformConfig;
        
    NEXUS_Platform_GetConfiguration(&platformConfig);
    LOGI("NexusService::setDisplayOutputs %d",display);
    
    if (display) {
#if NEXUS_NUM_COMPONENT_OUTPUTS
        NEXUS_Display_AddOutput(displayState[HD_DISPLAY].display, NEXUS_ComponentOutput_GetConnector(platformConfig.outputs.component[0]));
#endif

#if NEXUS_NUM_HDMI_OUTPUTS
        /* Add HDMI Output to the HD-Display */
        NEXUS_Display_AddOutput(displayState[HD_DISPLAY].display, NEXUS_HdmiOutput_GetVideoConnector(platformConfig.outputs.hdmi[0]));
#endif

#if NEXUS_NUM_COMPOSITE_OUTPUTS
        if (MAX_NUM_DISPLAYS > 1) {
            /* Add Composite Output to the SD-Display */
            NEXUS_Display_AddOutput(displayState[SD_DISPLAY].display, NEXUS_CompositeOutput_GetConnector(platformConfig.outputs.composite[0]));        
        }
#endif
    } 
    else {
        NEXUS_Display_RemoveAllOutputs(displayState[HD_DISPLAY].display); 
        
        if (MAX_NUM_DISPLAYS > 1) {
            /* Add Composite Output to the SD-Display */
            NEXUS_Display_RemoveAllOutputs(displayState[SD_DISPLAY].display);        
        }      
    } 
    return;
}

void NexusService::setAudioMute(int mute)
{
    NEXUS_PlatformConfiguration platformConfig;
    NEXUS_AudioOutputSettings settings;
    NEXUS_AudioOutput output;
    NEXUS_Error aud_err;
    int32_t leftVolume;
    int32_t rightVolume;

    NEXUS_Platform_GetConfiguration(&platformConfig);

#if NEXUS_NUM_AUDIO_DACS
    /* DAC out params config */
    output = NEXUS_AudioDac_GetConnector(platformConfig.outputs.audioDacs[0]);
    NEXUS_AudioOutput_GetSettings(output, &settings);
    settings.muted = mute;
    
    /* Set Dac Volume */
    aud_err = NEXUS_AudioOutput_SetSettings(output, &settings);
    BDBG_ASSERT(aud_err==BERR_SUCCESS);
#endif

    /* Spdif out params config */
#if NEXUS_NUM_SPDIF_OUTPUTS 
    output = NEXUS_SpdifOutput_GetConnector(platformConfig.outputs.spdif[0]);
    NEXUS_AudioOutput_GetSettings(output, &settings);
    settings.muted = mute;
    
    /* Set Spdif Volume */
    aud_err = NEXUS_AudioOutput_SetSettings(output, &settings);
    BDBG_ASSERT(aud_err==BERR_SUCCESS);
#endif

    /* Set HDMI volume */
#if NEXUS_NUM_HDMI_OUTPUTS
    output = NEXUS_HdmiOutput_GetAudioConnector(platformConfig.outputs.hdmi[0]);
    NEXUS_AudioOutput_GetSettings(output, &settings);
    settings.muted = false;
    aud_err = NEXUS_AudioOutput_SetSettings(output, &settings);
    BDBG_ASSERT(aud_err==BERR_SUCCESS);
#endif
}

#define AUDIO_VOLUME_SETTING_MIN (0)
#define AUDIO_VOLUME_SETTING_MAX (99)

/****************************************************
* Volume Table in dB, Mapping as linear attenuation *
****************************************************/
static uint32_t Gemini_VolTable[AUDIO_VOLUME_SETTING_MAX+1] =
{
    0,      9,      17,     26,     35,     
    45,     54,     63,     72,     82,
    92,     101,    111,    121,    131,
    141,    151,    162,    172,    183,
    194,    205,    216,    227,    239,
    250,    262,    273,    285,    297,
    310,    322,    335,    348,    361,
    374,    388,    401,    415,    429,
    444,    458,    473,    488,    504,
    519,    535,    551,    568,    585,
    602,    620,    638,    656,    674,
    694,    713,    733,    754,    774,
    796,    818,    840,    864,    887,
    912,    937,    963,    990,    1017,
    1046,   1075,   1106,   1137,   1170,   
    1204,   1240,   1277,   1315,   1356,
    1398,   1442,   1489,   1539,   1592,   
    1648,   1708,   1772,   1842,   1917,
    2000,   2092,   2194,   2310,   2444,
    2602,   2796,   3046,   3398,   9000    
};

void NexusService::setAudioVolume(float left, float right)
{
    int32_t leftVolume;
    int32_t rightVolume;

    LOGV("AudioStreamOutNEXUS %s: at %d left=%f right=%f\n",__FUNCTION__,__LINE__,left,right);
    leftVolume = left*AUDIO_VOLUME_SETTING_MAX;
    rightVolume = right*AUDIO_VOLUME_SETTING_MAX;

    /* Check for boundary */ 
    if (leftVolume > AUDIO_VOLUME_SETTING_MAX)
        leftVolume = AUDIO_VOLUME_SETTING_MAX;
    if (leftVolume < AUDIO_VOLUME_SETTING_MIN)
        leftVolume = AUDIO_VOLUME_SETTING_MIN;
    if (rightVolume > AUDIO_VOLUME_SETTING_MAX)
        rightVolume = AUDIO_VOLUME_SETTING_MAX;
    if (rightVolume < AUDIO_VOLUME_SETTING_MIN)
        rightVolume = AUDIO_VOLUME_SETTING_MIN;

    NEXUS_PlatformConfiguration platformConfig;
    NEXUS_AudioOutputSettings settings;
    NEXUS_AudioOutput output;
    NEXUS_Error aud_err;

    NEXUS_Platform_GetConfiguration(&platformConfig);

#if NEXUS_NUM_AUDIO_DACS
    /* DAC out params config */
    output = NEXUS_AudioDac_GetConnector(platformConfig.outputs.audioDacs[0]);
    NEXUS_AudioOutput_GetSettings(output, &settings);
    settings.volumeType = NEXUS_AudioVolumeType_eDecibel;
    settings.muted = false;
    settings.leftVolume = -Gemini_VolTable[AUDIO_VOLUME_SETTING_MAX-leftVolume];
    settings.rightVolume = -Gemini_VolTable[AUDIO_VOLUME_SETTING_MAX-rightVolume];

    /* Set Dac Volume */
    aud_err = NEXUS_AudioOutput_SetSettings(output, &settings);
    BDBG_ASSERT(aud_err==BERR_SUCCESS);
#endif

    /* Spdif out params config */
#if NEXUS_NUM_SPDIF_OUTPUTS 
    output = NEXUS_SpdifOutput_GetConnector(platformConfig.outputs.spdif[0]);
    NEXUS_AudioOutput_GetSettings(output, &settings);
    settings.volumeType = NEXUS_AudioVolumeType_eDecibel;
    settings.muted = false;
    settings.leftVolume = -Gemini_VolTable[AUDIO_VOLUME_SETTING_MAX-leftVolume];
    settings.rightVolume = -Gemini_VolTable[AUDIO_VOLUME_SETTING_MAX-rightVolume];

    /* Set Spdif Volume */
    aud_err = NEXUS_AudioOutput_SetSettings(output, &settings);
    BDBG_ASSERT(aud_err==BERR_SUCCESS);
#endif

    /* Set HDMI volume */
#if NEXUS_NUM_HDMI_OUTPUTS
    output = NEXUS_HdmiOutput_GetAudioConnector(platformConfig.outputs.hdmi[0]);
    NEXUS_AudioOutput_GetSettings(output, &settings);
    settings.volumeType = NEXUS_AudioVolumeType_eDecibel;
    settings.muted = false;
    settings.leftVolume = -Gemini_VolTable[AUDIO_VOLUME_SETTING_MAX-leftVolume];
    settings.rightVolume = -Gemini_VolTable[AUDIO_VOLUME_SETTING_MAX-rightVolume];
    
    aud_err = NEXUS_AudioOutput_SetSettings(output, &settings);
    BDBG_ASSERT(aud_err==BERR_SUCCESS);
#endif
}


#if NEXUS_NUM_HDMI_INPUTS

static uint8_t SampleEDID[] = 
{
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x08, 0x6D, 0x74, 0x22, 0x05, 0x01, 0x11, 0x20,
    0x00, 0x14, 0x01, 0x03, 0x80, 0x00, 0x00, 0x78, 0x0A, 0xDA, 0xFF, 0xA3, 0x58, 0x4A, 0xA2, 0x29,
    0x17, 0x49, 0x4B, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40, 0x58, 0x2C,
    0x45, 0x00, 0xBA, 0x88, 0x21, 0x00, 0x00, 0x1E, 0x01, 0x1D, 0x80, 0x18, 0x71, 0x1C, 0x16, 0x20,
    0x58, 0x2C, 0x25, 0x00, 0xBA, 0x88, 0x21, 0x00, 0x00, 0x9E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x42,
    0x43, 0x4D, 0x37, 0x34, 0x32, 0x32, 0x2F, 0x37, 0x34, 0x32, 0x35, 0x0A, 0x00, 0x00, 0x00, 0xFD,
    0x00, 0x17, 0x3D, 0x0F, 0x44, 0x0F, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x89,

    0x02, 0x03, 0x3C, 0x71, 0x7F, 0x03, 0x0C, 0x00, 0x40, 0x00, 0xB8, 0x2D, 0x2F, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xE3, 0x05, 0x1F, 0x01, 0x49, 0x90, 0x05, 0x20, 0x04, 0x03, 0x02, 0x07,
    0x06, 0x01, 0x29, 0x09, 0x07, 0x01, 0x11, 0x07, 0x00, 0x15, 0x07, 0x00, 0x01, 0x1D, 0x00, 0x72,
    0x51, 0xD0, 0x1E, 0x20, 0x6E, 0x28, 0x55, 0x00, 0xBA, 0x88, 0x21, 0x00, 0x00, 0x1E, 0x8C, 0x0A,
    0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3E, 0x96, 0x00, 0xBA, 0x88, 0x21, 0x00, 0x00, 0x18,
    0x8C, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3E, 0x96, 0x00, 0x0B, 0x88, 0x21, 0x00,
    0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9D
};

static void source_changed(void *context, int param)
{
    BSTD_UNUSED(context);
    BSTD_UNUSED(param);

    LOGV("source changed\n");
}

static void avmute_changed(void *context, int param)
{
    NexusClientContext *client;
    NEXUS_HdmiInputStatus hdmiInputStatus ;
    BSTD_UNUSED(param);

    client = (NexusClientContext *) context ;
    NEXUS_HdmiInput_GetStatus(client->resources.hdmiInput.handle, &hdmiInputStatus) ;

    if (!hdmiInputStatus.validHdmiStatus) {
        LOGV("avmute_changed callback: Unable to get hdmiInput status\n") ;
    }
    else {
        LOGV("avmute_changed callback: %s\n", 
            hdmiInputStatus.avMute ? "Set_AvMute" : "Clear_AvMute") ;
    }
}
#endif // NEXUS_NUM_HDMI_INPUTS

bool NexusService::connectHdmiInput(NexusClientContext * client, b_refsw_client_connect_resource_settings *pConnectSettings)
{
    NEXUS_Error rc = NEXUS_SUCCESS;
#if NEXUS_NUM_HDMI_INPUTS && NEXUS_NUM_HDMI_OUTPUTS
    unsigned timeout = 4;
    unsigned hdmiInputIndex = 0;

    NEXUS_PlatformConfiguration platformConfig;
    NEXUS_HdmiInputSettings hdmiInputSettings;
    NEXUS_HdmiOutputStatus hdmiOutputStatus;
    NEXUS_AudioOutput hdmiAudioOutput;

    NEXUS_HdmiInput_GetDefaultSettings(&hdmiInputSettings);
    hdmiInputSettings.timebase = NEXUS_Timebase_e0;
    hdmiInputSettings.sourceChanged.callback = source_changed;
    
    /* set hpdDisconnected to true if a HDMI switch is in front of the Broadcom HDMI Rx.  
         -- The NEXUS_HdmiInput_ConfigureAfterHotPlug should be called to inform the hw of 
         -- the current state,  the Broadcom SV reference boards have no switch so 
         -- the value should always be false 
         */
    hdmiInputSettings.frontend.hpdDisconnected = false ;
    
    /* use NEXUS_HdmiInput_OpenWithEdid ()
          if EDID PROM (U1304 and U1305) is NOT installed; 
            reference boards usually have the PROMs installed.
            this example assumes Port1 EDID has been removed 
        */

    /* all HDMI Tx/Rx combo chips have EDID RAM */
    hdmiInputSettings.useInternalEdid = true ;

    NEXUS_Platform_GetConfiguration(&platformConfig);

    do {
        /* check for connected downstream device */
        rc = NEXUS_HdmiOutput_GetStatus(platformConfig.outputs.hdmi[0], &hdmiOutputStatus);
        if (rc) BERR_TRACE(rc);
        if ( !hdmiOutputStatus.connected ) {
            LOGW("Waiting for HDMI Tx Device");
            BKNI_Sleep(250);
        }
        else {
            break;
        }
    } while (timeout--);

    if (hdmiOutputStatus.connected) {
        NEXUS_HdmiOutputBasicEdidData hdmiOutputBasicEdidData;
        unsigned char *attachedRxEdid;
        NEXUS_HdmiOutputEdidBlock edidBlock;
        unsigned i, j;

        /* Get EDID of attached receiver*/
        NEXUS_HdmiOutput_GetBasicEdidData(platformConfig.outputs.hdmi[0], &hdmiOutputBasicEdidData);

        /* allocate space to hold the EDID blocks */
        attachedRxEdid = (unsigned char*) BKNI_Malloc((hdmiOutputBasicEdidData.extensions+1)* sizeof(edidBlock.data));
        for (i=0; i<= hdmiOutputBasicEdidData.extensions; i++) {
            rc = NEXUS_HdmiOutput_GetEdidBlock(platformConfig.outputs.hdmi[0], i, &edidBlock);
            if (rc)
                LOGE("Error retrieve EDID from attached receiver");

            for (j=0; j < sizeof(edidBlock.data); j++) {
                attachedRxEdid[i*sizeof(edidBlock.data)+j] = edidBlock.data[j];
            }
        }
        
        /* Load hdmiInput EDID RAM with the EDID from the Rx attached to the hdmiOutput (Tx) . */
        client->resources.hdmiInput.handle = NEXUS_HdmiInput_OpenWithEdid(hdmiInputIndex, &hdmiInputSettings, 
            attachedRxEdid, (uint16_t) (sizeof(NEXUS_HdmiOutputEdidBlock) * (hdmiOutputBasicEdidData.extensions+1)));
        LOGD("Use Edid from attached HDMI Rx device");

        /* release memory resources */
        BKNI_Free(attachedRxEdid);
    }
    else {
        client->resources.hdmiInput.handle = NEXUS_HdmiInput_OpenWithEdid(hdmiInputIndex, &hdmiInputSettings, 
            &SampleEDID[0], (uint16_t) sizeof(SampleEDID));
        LOGD("Use Edid from hardcode BCM sample");
    }

    if (client->resources.hdmiInput.handle == NULL) {
        LOGE("%s: Can't get hdmi input!", __FUNCTION__);
        goto err_hdmi_input;
    }
    else {
        client->resources.hdmiInput.connected = true;

        NEXUS_HdmiInput_GetSettings(client->resources.hdmiInput.handle, &hdmiInputSettings) ;
        hdmiInputSettings.avMuteChanged.callback = avmute_changed;
        hdmiInputSettings.avMuteChanged.context = client;
        NEXUS_HdmiInput_SetSettings(client->resources.hdmiInput.handle, &hdmiInputSettings) ;
        
#if NEXUS_NUM_AUDIO_INPUT_CAPTURES
        client->resources.hdmiInput.captureInput = NEXUS_AudioInputCapture_Open(0, NULL);
        if (client->resources.hdmiInput.captureInput == NULL) {
            LOGE("%s: Can't get audio input capture!", __FUNCTION__);
            goto err_hdmi_input;
        }
        else {
            NEXUS_AudioInputCaptureStartSettings captureInputStartSettings;

            NEXUS_AudioInputCapture_GetDefaultStartSettings(&captureInputStartSettings);
            captureInputStartSettings.input = NEXUS_HdmiInput_GetAudioConnector(client->resources.hdmiInput.handle);

            NEXUS_AudioInputCapture_Start(client->resources.hdmiInput.captureInput, &captureInputStartSettings);

            /* Shutdown audio decoders/playback channels as they are not required for HDMI audio playback
               and result in issues when the Audio Mixer is re-connected to the HDMI output later on.
             */
            setAudioState(false);

            hdmiAudioOutput = NEXUS_HdmiOutput_GetAudioConnector(platformConfig.outputs.hdmi[0]);

            rc = NEXUS_AudioOutput_RemoveAllInputs(hdmiAudioOutput);
            if (rc != NEXUS_SUCCESS) {
                LOGE("%s: Could not remove all inputs from HDMI Audio output!", __FUNCTION__);
                goto err_hdmi_input;
            }
            else {
                rc = NEXUS_AudioOutput_AddInput(hdmiAudioOutput, NEXUS_AudioInputCapture_GetConnector(client->resources.hdmiInput.captureInput));
                if (rc != NEXUS_SUCCESS) {
                    LOGE("%s: Could not add audio input capture to HDMI audio output!", __FUNCTION__);
                    goto err_hdmi_input;
                }
            }
        }
#endif
        if (rc == NEXUS_SUCCESS) {
            NEXUS_SimpleVideoDecoderServerSettings settings;
            unsigned window_id = pConnectSettings->hdmiInput.windowId;

            // Save window id for disconnectHdmiInput()
            client->resources.hdmiInput.windowId = window_id;

            // Remove video window from simpledecoder, so that we can add hdmiIn
            // There is 1-1 mapping between simpleVideoDecoder and videoWindow
            NEXUS_SimpleVideoDecoder_GetServerSettings(simpleVideoDecoder[window_id], &settings);
            settings.window[window_id] = NULL;
            rc = NEXUS_SimpleVideoDecoder_SetServerSettings(simpleVideoDecoder[window_id], &settings);
            if (rc != NEXUS_SUCCESS) {
                LOGE("%s: Could not set simple video decoder server settings for video window %d!", __FUNCTION__, window_id);
                goto err_hdmi_input;
            }
            else {
                // Add Hdmi-In to the video window        
                for(int display_id = HD_DISPLAY; display_id < MAX_NUM_DISPLAYS; display_id++) {
                    rc = NEXUS_VideoWindow_AddInput(displayState[display_id].video_window[window_id],
                         NEXUS_HdmiInput_GetVideoConnector(client->resources.hdmiInput.handle));

                    if (rc != NEXUS_SUCCESS) {
                        LOGE("%s: Could not add HDMI input to video window %d of display %d!", __FUNCTION__, window_id, display_id);
                        goto err_hdmi_input;
                    }
                }
            }
        }
    }
#endif
    return (rc == NEXUS_SUCCESS);

#if NEXUS_NUM_HDMI_INPUTS
err_hdmi_input:
#if NEXUS_NUM_AUDIO_INPUT_CAPTURES
    if (client->resources.hdmiInput.captureInput != NULL) {
        NEXUS_AudioInput_Shutdown(NEXUS_AudioInputCapture_GetConnector(client->resources.hdmiInput.captureInput));
        NEXUS_AudioInputCapture_Close(client->resources.hdmiInput.captureInput);
        client->resources.hdmiInput.captureInput = NULL;
    }
#endif

    if (client->resources.hdmiInput.handle != NULL) {
        NEXUS_VideoInput_Shutdown(NEXUS_HdmiInput_GetVideoConnector(client->resources.hdmiInput.handle));
        NEXUS_AudioInput_Shutdown(NEXUS_HdmiInput_GetAudioConnector(client->resources.hdmiInput.handle));
        NEXUS_HdmiInput_Close(client->resources.hdmiInput.handle);  
        client->resources.hdmiInput.handle = NULL;
    }
    return false;
#endif
}

bool NexusService::disconnectHdmiInput(NexusClientContext * client)
{
    NEXUS_Error rc = NEXUS_SUCCESS;
#if NEXUS_NUM_HDMI_INPUTS && NEXUS_NUM_HDMI_OUTPUTS
#if NEXUS_NUM_AUDIO_INPUT_CAPTURES
    NEXUS_PlatformConfiguration platformConfig;
    NEXUS_AudioOutput           hdmiAudioOutput;
#endif

    if (client->resources.hdmiInput.handle)
    {
        NEXUS_StopCallbacks(client->resources.hdmiInput.handle);

#if NEXUS_NUM_AUDIO_INPUT_CAPTURES
        if (client->resources.hdmiInput.captureInput) {
            NEXUS_AudioInputCapture_Stop(client->resources.hdmiInput.captureInput);
        }

        NEXUS_Platform_GetConfiguration(&platformConfig);
        hdmiAudioOutput = NEXUS_HdmiOutput_GetAudioConnector(platformConfig.outputs.hdmi[0]);

        rc = NEXUS_AudioOutput_RemoveAllInputs(hdmiAudioOutput);
        if (rc != NEXUS_SUCCESS) {
            LOGE("%s: Could not remove all audio inputs from HDMI audio output!", __FUNCTION__);
        }
        else {
            rc = NEXUS_AudioOutput_AddInput(hdmiAudioOutput, NEXUS_AudioMixer_GetConnector(mixer));
            if (rc != NEXUS_SUCCESS) {
                LOGE("%s: Could not add audio mixer to HDMI output!", __FUNCTION__);
            }
            else {
                /* Re-instate the Audio Decoders and playback channels */
                setAudioState(true);
            }
        }
#endif
    }

    if (rc == NEXUS_SUCCESS) {
        NEXUS_SimpleVideoDecoderServerSettings settings;
        unsigned window_id = client->resources.hdmiInput.windowId;

        // Read video window from simpledecoder, so that we can add hdmiIn
        NEXUS_SimpleVideoDecoder_GetServerSettings(simpleVideoDecoder[window_id], &settings);
        
        // Remove all video inputs (this will remove hdmi-input earlier added to the video window)
        // There is 1-1 mapping between simpleVideoDecoder and videoWindow
        for(int display_id = HD_DISPLAY; display_id < MAX_NUM_DISPLAYS; display_id++)
        {
            NEXUS_VideoWindow_RemoveAllInputs(displayState[display_id].video_window[window_id]);
            settings.window[display_id] = displayState[display_id].video_window[window_id];
        }
        rc = NEXUS_SimpleVideoDecoder_SetServerSettings(simpleVideoDecoder[window_id], &settings);
        if (rc != NEXUS_SUCCESS) {
            LOGE("%s: Could not set simple video decoder server settings for video window %d!", __FUNCTION__, window_id);
        }
    }
    
#if NEXUS_NUM_AUDIO_INPUT_CAPTURES
    if (client->resources.hdmiInput.captureInput) {
        NEXUS_AudioInput_Shutdown(NEXUS_AudioInputCapture_GetConnector(client->resources.hdmiInput.captureInput));
        NEXUS_AudioInputCapture_Close(client->resources.hdmiInput.captureInput);
        client->resources.hdmiInput.captureInput = NULL;
    }
#endif
    if (client->resources.hdmiInput.handle) {
        NEXUS_VideoInput_Shutdown(NEXUS_HdmiInput_GetVideoConnector(client->resources.hdmiInput.handle));
        NEXUS_AudioInput_Shutdown(NEXUS_HdmiInput_GetAudioConnector(client->resources.hdmiInput.handle));
        NEXUS_HdmiInput_Close(client->resources.hdmiInput.handle);  
        client->resources.hdmiInput.handle = NULL;
    }
#endif
    return (rc == NEXUS_SUCCESS);
}

bool NexusService::isCecEnabled(uint32_t cecId)
{
    bool enabled = false;
#if NEXUS_HAS_CEC
    char value[PROPERTY_VALUE_MAX];

    if (property_get("ro.enable_cec", value, NULL) && strcmp(value,"1")==0) {
        enabled = true;
    }
#endif
    return enabled;
}

bool NexusService::setCecPowerState(unsigned cecId, b_powerState pmState)
{
    bool success = false;
#if NEXUS_HAS_CEC
    if (mCecServiceManager[cecId] != NULL && mCecServiceManager[cecId]->isPlatformInitialised()) {
        success = mCecServiceManager[cecId]->setPowerState(pmState);
    }
#endif
    if (!success) {
        LOGE("%s: Could not set CEC%d power state %d!", __FUNCTION__, cecId, pmState);
    }
    return success;
}

bool NexusService::getCecPowerStatus(uint32_t cecId, uint8_t *pPowerStatus)
{
    bool success = false;
#if NEXUS_HAS_CEC
    if (mCecServiceManager[cecId] != NULL && mCecServiceManager[cecId]->isPlatformInitialised()) {
        success = mCecServiceManager[cecId]->getPowerStatus(pPowerStatus);
    }
#endif
    if (!success) {
        LOGE("%s: Could not get CEC%d TV power status!", __FUNCTION__, cecId);
    }
    return success;
}

bool NexusService::sendCecMessage(unsigned cecId, uint8_t destAddr, size_t length, uint8_t *pMessage)
{
    bool success = false;
#if NEXUS_HAS_CEC
    if (mCecServiceManager[cecId] != NULL && mCecServiceManager[cecId]->isPlatformInitialised()) {
        success = (mCecServiceManager[cecId]->sendCecMessage(destAddr, length, pMessage) == OK) ? true : false;
    }
#endif
    if (!success) {
        LOGE("%s: Could not send CEC%d message opcode: 0x%02X!", __FUNCTION__, cecId, *pMessage);
    }
    return success;
}

bool NexusService::setPowerState(b_powerState pmState)
{
    NEXUS_Error rc = NEXUS_SUCCESS;
    NEXUS_PlatformStandbySettings nexusStandbySettings;

    if (pmState != powerState)
    {
        NEXUS_Platform_GetStandbySettings(&nexusStandbySettings);

        switch (pmState)
        {
            case ePowerState_S0:
            {
                LOGD("%s: About to set power state S0...", __FUNCTION__);
                nexusStandbySettings.mode = NEXUS_PlatformStandbyMode_eOn;    
                rc = NEXUS_Platform_SetStandbySettings(&nexusStandbySettings);
                if (rc == NEXUS_SUCCESS) {
                    setDisplayState(1);
                    setVideoState(1);
                    setAudioState(1);
                }
                break;
            }

            case ePowerState_S1:
            {
                LOGD("%s: About to set power state S1...", __FUNCTION__);
                setDisplayState(0);
                setVideoState(0);
                setAudioState(0);
                nexusStandbySettings.mode = NEXUS_PlatformStandbyMode_eActive;    
                rc = NEXUS_Platform_SetStandbySettings(&nexusStandbySettings);
                break;
            }

            case ePowerState_S2:
            {
                LOGD("%s: About to set power state S2...", __FUNCTION__);
                setDisplayState(0);
                setVideoState(0);
                setAudioState(0);
                nexusStandbySettings.mode = NEXUS_PlatformStandbyMode_ePassive;
                nexusStandbySettings.wakeupSettings.ir = 1;
                nexusStandbySettings.wakeupSettings.uhf = 1;
                nexusStandbySettings.wakeupSettings.transport = 1;
#if NEXUS_HAS_CEC
		if (isCecEnabled(0)) {
                    nexusStandbySettings.wakeupSettings.cec = 1;
                }
#endif
                nexusStandbySettings.wakeupSettings.gpio = 1;
                nexusStandbySettings.wakeupSettings.timeout = 0;
                rc = NEXUS_Platform_SetStandbySettings(&nexusStandbySettings);
                break;
            }

            case ePowerState_S3:
            {
                LOGD("%s: About to set power state S3...", __FUNCTION__);
                setDisplayState(0);
                setVideoState(0);
                setAudioState(0);
                nexusStandbySettings.mode = NEXUS_PlatformStandbyMode_eDeepSleep;  
                nexusStandbySettings.wakeupSettings.ir = 1;
                nexusStandbySettings.wakeupSettings.uhf = 1;
#if NEXUS_HAS_CEC
		if (isCecEnabled(0)) {
                    nexusStandbySettings.wakeupSettings.cec = 1;
                }
#endif
                nexusStandbySettings.wakeupSettings.gpio = 1;
                nexusStandbySettings.wakeupSettings.timeout = 0;
                rc = NEXUS_Platform_SetStandbySettings(&nexusStandbySettings);
                break;
            }

            default:
            {
                LOGE("%s: invalid power state %d!", __FUNCTION__, pmState);
                rc = NEXUS_INVALID_PARAMETER;
                break;
            }
        }
    }

    if (rc == NEXUS_SUCCESS) {
        powerState = pmState;
        return true;
    } else {
        LOGE("%s: ERROR setting power state to S%d!", __FUNCTION__, pmState);
        return false;
    }
}

b_powerState NexusService::getPowerState()
{
    return powerState;
}


/************************************************/
/* ADDED CODE TO CAPTURE THE FRAME              */
/************************************************/




//1 - success
//0 - failure
bool NexusService::getFrame(NexusClientContext * client,
                                    const uint32_t width,
                                    const uint32_t height,
                                    const uint32_t surfacePhyAddress,
                                    const NEXUS_PixelFormat surfaceFormat,
                                    const uint32_t decoderId) 
{
        NEXUS_Error                     rc = NEXUS_SUCCESS;
        NEXUS_SurfaceCreateSettings     createSettings;
        NEXUS_StripedSurfaceHandle      stripedSurface=NULL; 
        NEXUS_SurfaceHandle             dstSurface;
        NEXUS_MemoryStatus              status;
        uint32_t                        actualOffset;
        NEXUS_VideoDecoderHandle        decoder; 

        //ALOGD("%s %d"ENTER ************",__FUNCTION__,__LINE__);

        decoder = videoDecoder[decoderId];
        if (!decoder) {
            LOGE("Failed TO Acquire Decoder Handle To Start Capture ....");
        }
        
        //ALOGD("%s %d"************",__FUNCTION__,__LINE__);
        void * pTemp = NEXUS_OffsetToCachedAddr(surfacePhyAddress);
        memset(pTemp, 0, 100);
        //ALOGD("%s %d"************",__FUNCTION__,__LINE__);

        NEXUS_Surface_GetDefaultCreateSettings(&createSettings);
        createSettings.pixelFormat = surfaceFormat;
        createSettings.width = width;
        createSettings.height = height;
        createSettings.pMemory = (void*) pTemp;   
        dstSurface = NEXUS_Surface_Create(&createSettings);
        if(!dstSurface)
        {
            while(1) LOGE("Creating Surface Failed!!");
        }

        //ALOGD("%s %d"************",__FUNCTION__,__LINE__);
        unsigned int reTryCnt=0;
        do 
        {
            stripedSurface = NEXUS_VideoDecoder_CreateStripedSurface(decoder);
            if(stripedSurface == NULL)
            {
                BKNI_Sleep(12);
                reTryCnt++;
            }
        }while((stripedSurface == NULL) && (reTryCnt <  10));

        if( stripedSurface == NULL )
        {
            LOGE("Creating Striped Surface Failed!!");
            NEXUS_Surface_Destroy(dstSurface);
            ALOGD("%s:%d EXITING",__FUNCTION__,__LINE__);
            return false;
        }

        //ALOGD("%s %d"************",__FUNCTION__,__LINE__);
        rc = NEXUS_Graphics2D_DestripeToSurface( gfx2D, stripedSurface, dstSurface, NULL );
        if(rc) 
        {
            while(1) LOGE("%s:De-Stripe To surface Failed\n",__FUNCTION__);
            NEXUS_Surface_Destroy(dstSurface);
            NEXUS_VideoDecoder_DestroyStripedSurface(decoder,stripedSurface);
            ALOGD("%s:%d EXITING",__FUNCTION__,__LINE__);
            return false;
        }

        //ALOGD("%s %d"************",__FUNCTION__,__LINE__);
        // Wait for the De-Stripe to finish....
        //rc = NEXUS_Graphics2D_Checkpoint( gfx2D, NULL );

        do
        {
            rc = NEXUS_Graphics2D_Checkpoint(gfx2D, NULL);
            if (rc == (NEXUS_Error) NEXUS_GRAPHICS2D_QUEUED)
                rc = BKNI_WaitForEvent(gfxDone, 1000);
        } 
        while (rc == (NEXUS_Error) NEXUS_GRAPHICS2D_QUEUE_FULL) ;

        //ALOGD("%s %d"************",__FUNCTION__,__LINE__);
        NEXUS_Surface_Destroy(dstSurface);
        NEXUS_VideoDecoder_DestroyStripedSurface(decoder,stripedSurface);
        //ALOGD("%s %d"EXITING",__FUNCTION__,__LINE__);
        return true;
}

bool NexusService::connectClientResources(NexusClientContext * client, b_refsw_client_connect_resource_settings *pConnectSettings)
{
    bool ok = true;

    if (pConnectSettings->hdmiInput.id != 0) {
        ok = connectHdmiInput(client, pConnectSettings);
    }
    return ok;
}

bool NexusService::disconnectClientResources(NexusClientContext * client)
{
    bool ok = true;

    if (client->resources.hdmiInput.connected) {
        ok = disconnectHdmiInput(client);
        client->resources.hdmiInput.connected = false;
    }
    return ok;
}

#undef CHECK_INTERFACE
#define CHECK_INTERFACE(interface, data, reply) \
        do { if (!data.enforceInterface(interface::getInterfaceDescriptor())) { \
            LOGE("Call incorrectly routed to " #interface); \
            return PERMISSION_DENIED;              \
        } } while (0)

status_t NexusService::onTransact(uint32_t code,
                                            const Parcel &data,
                                            Parcel *reply,
                                            uint32_t flags)
{
    CHECK_INTERFACE(INexusService, data, reply);
    
    switch(code) {
    case API_OVER_BINDER: { /* braces are necessary when declaring variables within case: */
        int rc = 0;
        api_data cmd;
        data.read(&cmd, sizeof(cmd));

        switch(cmd.api)
        {
            case api_clientJoin:
            {
                cmd.param.clientJoin.out.clientHandle = 
                    clientJoin(&cmd.param.clientJoin.in.clientName, &cmd.param.clientJoin.in.clientAuthenticationSettings);
                break;
            }
            case api_clientUninit:
            {
                cmd.param.clientUninit.out.status = clientUninit(cmd.param.clientUninit.in.clientHandle);
                break;
            }
            case api_createClientContext:
            {
                cmd.param.createClientContext.out.client = createClientContext(&cmd.param.createClientContext.in.createClientConfig);
                break;
            }
            case api_destroyClientContext:
            {
                destroyClientContext(cmd.param.destroyClientContext.in.client);
                break;
            }
            case api_getClientInfo:
            {
                getClientInfo(cmd.param.getClientInfo.in.client, &cmd.param.getClientInfo.out.info);
                break;
            }
            case api_getClientComposition:
            {
                getClientComposition(cmd.param.getClientComposition.in.client, &cmd.param.getClientComposition.out.composition);
                break;
            }
            case api_setClientComposition:
            {
                setClientComposition(cmd.param.setClientComposition.in.client, &cmd.param.setClientComposition.in.composition);
                break;
            }
            
            case api_getVideoWindowSettings:
            {
                getVideoWindowSettings(cmd.param.getVideoWindowSettings.in.client, cmd.param.getVideoWindowSettings.in.window_id, &cmd.param.getVideoWindowSettings.out.settings);
                break;
            }
            case api_setVideoWindowSettings:
            {
                setVideoWindowSettings(cmd.param.setVideoWindowSettings.in.client, cmd.param.setVideoWindowSettings.in.window_id, &cmd.param.setVideoWindowSettings.in.settings);
                break;
            }
            case api_getDisplaySettings:
            {
                getDisplaySettings(cmd.param.getDisplaySettings.in.display_id, &cmd.param.getDisplaySettings.out.settings);
                break;
            }
            case api_setDisplaySettings:            
            {
                setDisplaySettings(cmd.param.getDisplaySettings.in.display_id, &cmd.param.getDisplaySettings.out.settings);
                break;
            } 

            case api_addGraphicsWindow:
            {
                cmd.param.addGraphicsWindow.out.status = addGraphicsWindow(cmd.param.addGraphicsWindow.in.client);
                break;
            }
            case api_setAudioVolume:
            {
                setAudioVolume(cmd.param.setAudioVolume.in.leftVolume, cmd.param.setAudioVolume.in.rightVolume);
                break;
            }
            case api_getFrame:
            {
                cmd.param.getFrame.out.status = getFrame(cmd.param.getFrame.in.client, cmd.param.getFrame.in.width, cmd.param.getFrame.in.height, 
                    cmd.param.getFrame.in.surfacePhyAddress, cmd.param.getFrame.in.surfaceFormat, cmd.param.getFrame.in.decoderId);
                break;
            }
            case api_setPowerState:
            {
                cmd.param.setPowerState.out.status = setPowerState(cmd.param.setPowerState.in.pmState);
                break;
            }
            case api_getPowerState:
            {
                cmd.param.getPowerState.out.pmState = getPowerState();
                break;
            }
            case api_setCecPowerState:
            {
                cmd.param.setCecPowerState.out.status = setCecPowerState(cmd.param.setCecPowerState.in.cecId,
                                                                         cmd.param.setCecPowerState.in.pmState);
                break;
            }
            case api_getCecPowerStatus:
            {
                cmd.param.getCecPowerStatus.out.status = getCecPowerStatus(cmd.param.getCecPowerStatus.in.cecId,
                                                                          &cmd.param.getCecPowerStatus.out.powerStatus);
                break;
            }
            case api_sendCecMessage:
            {
                cmd.param.sendCecMessage.out.status = sendCecMessage(cmd.param.sendCecMessage.in.cecId,
                                                                     cmd.param.sendCecMessage.in.destAddr,
                                                                     cmd.param.sendCecMessage.in.length,
                                                                     cmd.param.sendCecMessage.in.message);
                break;
            }
            case api_connectClientResources:
            {
                cmd.param.connectClientResources.out.status = connectClientResources(cmd.param.connectClientResources.in.client,
                                                                                     &cmd.param.connectClientResources.in.connectSettings);
                break;
            }
            case api_disconnectClientResources:
            {
                cmd.param.disconnectClientResources.out.status = disconnectClientResources(cmd.param.disconnectClientResources.in.client);
                break;
            }
            case api_getPictureCtrlCommonSettings:
            {
                getPictureCtrlCommonSettings(cmd.param.getPictureCtrlCommonSettings.in.window_id, &cmd.param.getPictureCtrlCommonSettings.out.settings);
                break;
            }
            case api_setPictureCtrlCommonSettings:            
            {
                setPictureCtrlCommonSettings(cmd.param.setPictureCtrlCommonSettings.in.window_id, &cmd.param.setPictureCtrlCommonSettings.in.settings);
                break;
            } 
            case api_getGraphicsColorSettings:
            {
                getGraphicsColorSettings(cmd.param.getGraphicsColorSettings.in.display_id, &cmd.param.getGraphicsColorSettings.out.settings);
                break;
            }
            case api_setGraphicsColorSettings:            
            {
                setGraphicsColorSettings(cmd.param.setGraphicsColorSettings.in.display_id, &cmd.param.setGraphicsColorSettings.in.settings);
                break;
            } 
            case api_setDisplayOutputs:            
            {
                LOGI("api_setDisplayOutputs: display=%d",cmd.param.setDisplayOutputs.in.display);
                setDisplayOutputs(cmd.param.setDisplayOutputs.in.display);
                break;
            }
            case api_setAudioMute:
            {
                setAudioMute(cmd.param.setAudioMute.in.mute);
                break;
            }
            default:
            {
                LOGE("%s: Unhandled cmd = %d",__FUNCTION__,cmd.api);
                return INVALID_OPERATION;
            }
        } /* switch(cmd.api) */

        if(!rc)
        {
            reply->write(&cmd.param, sizeof(cmd.param));
            return NO_ERROR;
        }
        else
        {
            return FAILED_TRANSACTION;
        }
    } break; 

    default:
        LOGE("ERROR! No such transaction(%d) in nexus service", code);
        return BBinder::onTransact(code, data, reply, flags);
        break;
    }/* switch(code) */

    return NO_ERROR;
}

