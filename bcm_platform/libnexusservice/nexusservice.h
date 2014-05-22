/******************************************************************************
 *    (c)2011-2013 Broadcom Corporation
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
 * $brcm_Workfile: nexusservice.h $
 * $brcm_Revision: 19 $
 * $brcm_Date: 12/3/12 3:24p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/libnexusservice/nexusservice.h $
 * 
 * 19   12/3/12 3:24p saranya
 * SWANDROID-266: Removed Non-IPC Standalone Mode
 * 
 * 18   9/13/12 11:32a kagrawal
 * SWANDROID-104: Added support for dynamic display resolution change,
 *  1080p and screen resizing
 * 
 * 17   9/4/12 6:54p nitinb
 * SWANDROID-197:Implement volume control functionality on nexus server
 *  side
 * 
 * 16   7/30/12 4:08p kagrawal
 * SWANDROID-104: Support for composite output
 * 
 * 15   7/6/12 9:13p ajitabhp
 * SWANDROID-128: FIXED Graphics Window Resource Leakage in SBS and NSC
 *  mode.
 * 
 * 14   6/24/12 10:51p alexpan
 * SWANDROID-108: Fix build errors for platforms without hdmi-in after
 *  changes for SimpleDecoder
 * 
 * 13   6/20/12 6:00p kagrawal
 * SWANDROID-118: Extended get_output_format() to return width and height
 * 
 * 12   6/20/12 11:16a kagrawal
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
 * 8   5/7/12 3:45p ajitabhp
 * SWANDROID-96: Initial checkin for android side by side implementation.
 * 
 * 7   4/13/12 1:15p ajitabhp
 * SWANDROID-65: Memory Owner ship problem resolved in multi-process mode.
 * 
 * 6   4/3/12 5:00p kagrawal
 * SWANDROID-56: Added support for VideoWindow configuration in NSC mode
 * 
 * 5   3/27/12 4:05p mzhuang
 * SW7425-2633: audio mixer errors after audio flinger restart
 * 
 * 4   3/15/12 4:51p mzhuang
 * SW7425-2633: audio mixer errors after audio flinger restart
 * 
 * 3   2/24/12 4:10p kagrawal
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
 *****************************************************************************/
#ifndef _NEXUSSERVICE_H_
#define _NEXUSSERVICE_H_

#include "nexusservice_base.h"

#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <utils/KeyedVector.h>
#include <string.h>

#include "nexus_interface.h"

#include "nexus_types.h"
#include "nexus_platform.h"
#include "nexus_display.h"
#if NEXUS_NUM_COMPOSITE_OUTPUTS
#include "nexus_composite_output.h"
#endif
#if NEXUS_NUM_COMPONENT_OUTPUTS
#include "nexus_component_output.h"
#endif
#if NEXUS_NUM_HDMI_OUTPUTS
#include "nexus_hdmi_output.h"
#include "nexus_hdmi_output_hdcp.h"
#if NEXUS_HAS_CEC
#include "nexus_cec.h"
#endif
#endif
#if NEXUS_NUM_HDMI_INPUTS
#include "nexus_hdmi_input.h"
#endif
#include "nexus_video_input.h"
#include "nexus_surface.h"
#include "nexus_graphics2d.h"
#include "nexus_core_utils.h"
#include "bkni.h"
#include "bkni_multi.h"
#include "nexus_video_decoder.h"
#include "nexus_audio_decoder.h"
#include "nexus_audio_input.h"
#include "nexus_audio_mixer.h"
#if NEXUS_NUM_AUDIO_INPUT_CAPTURES
#include "nexus_audio_input_capture.h"
#endif
#include "nexus_simple_video_decoder_server.h"
#include "nexus_simple_audio_decoder_server.h"
#include "nexus_simple_encoder_server.h"
#include "nexus_stc_channel.h"
#include "nexus_surface_compositor.h"
#include "nexus_picture_ctrl.h"

/*If the security mode is not eUntrusted then we do Join else we do Authenticated Join*/
#if (ANDROID_CLIENT_SECURITY_MODE != 2)
#define NEXUS_ABSTRACTED_JOIN(auth) NEXUS_Platform_Join()
#else
#define NEXUS_ABSTRACTED_JOIN(auth) NEXUS_Platform_AuthenticatedJoin(auth)
#endif

#if (ANDROID_SUPPORTS_SD_DISPLAY && (NEXUS_NUM_DISPLAYS >= 2))
#define MAX_NUM_DISPLAYS (2)
#else
#define MAX_NUM_DISPLAYS (1)
#endif /* #if ANDROID_SUPPORTS_SD_DISPLAY */

#if (NEXUS_NUM_VIDEO_WINDOWS >= 2)
#define MAX_VIDEO_WINDOWS_PER_DISPLAY (2)
#else
#define MAX_VIDEO_WINDOWS_PER_DISPLAY (NEXUS_NUM_VIDEO_WINDOWS)
#endif

#define HD_DISPLAY (0)
#define SD_DISPLAY (1)
#define MAX_AUDIO_DECODERS (2)
#define MAX_VIDEO_DECODERS (2)
#define MAX_ENCODERS (2)
#define AUDIO_DECODER_FIFO_SIZE  2*1024*1024
#define VIDEO_DECODER_FIFO_SIZE 10*1024*1024

#ifndef NEXUS_CEC_MESSAGE_DATA_SIZE
#define NEXUS_CEC_MESSAGE_DATA_SIZE 16
#endif

using namespace android;

class INexusService: public IInterface {
public:
    android_DECLARE_META_INTERFACE(NexusService)
};

class BnNexusService : public BnInterface<INexusService>
{
};

typedef struct DisplayState
{
    NEXUS_DisplayHandle display;
    NEXUS_VideoWindowHandle video_window[MAX_VIDEO_WINDOWS_PER_DISPLAY]; // video window per display
    /* Below are only for standalone mode, should be removed when standalone mode is gone */
    int hNexusDisplay;
    int hNexusVideoWindow[MAX_VIDEO_WINDOWS_PER_DISPLAY];
} DisplayState;

class NexusService : public NexusServiceBase, public BnNexusService
{
public:
    static void instantiate();
    virtual ~NexusService();
    status_t onTransact(uint32_t code,
                                 const Parcel &data,
                                 Parcel *reply,
                                 uint32_t flags);

    /* These API's require a Nexus Client Context as they handle per client resources... */
    virtual NexusClientContext * createClientContext(const b_refsw_client_client_configuration *config);
    virtual void destroyClientContext(NexusClientContext * client);
    virtual void getClientInfo(NexusClientContext * client, b_refsw_client_client_info *info);  
    virtual void getClientComposition(NexusClientContext * client, NEXUS_SurfaceComposition *composition);
    virtual void setClientComposition(NexusClientContext * client, NEXUS_SurfaceComposition *composition);
    virtual void getVideoWindowSettings(NexusClientContext * client, uint32_t window_id, b_video_window_settings *settings);
    virtual void setVideoWindowSettings(NexusClientContext * client, uint32_t window_id, b_video_window_settings *settings);
    virtual bool addGraphicsWindow(NexusClientContext * client);
    virtual bool getFrame(NexusClientContext * client, const uint32_t width, const uint32_t height, 
        const uint32_t surfacePhyAddress, const NEXUS_PixelFormat surfaceFormat, const uint32_t decoderId);
    virtual bool connectClientResources(NexusClientContext * client, b_refsw_client_connect_resource_settings *pConnectSettings);
    virtual bool disconnectClientResources(NexusClientContext * client);

    /* These API's do NOT require a Nexus Client Context as they handle global resources...*/
    virtual void getPictureCtrlCommonSettings(uint32_t window_id, NEXUS_PictureCtrlCommonSettings *settings);
    virtual void setPictureCtrlCommonSettings(uint32_t window_id, NEXUS_PictureCtrlCommonSettings *settings);
    virtual void getGraphicsColorSettings(uint32_t display_id, NEXUS_GraphicsColorSettings *settings);
    virtual void setGraphicsColorSettings(uint32_t display_id, NEXUS_GraphicsColorSettings *settings); 
    virtual void getDisplaySettings(uint32_t display_id, NEXUS_DisplaySettings *settings);
    virtual void setDisplaySettings(uint32_t display_id, NEXUS_DisplaySettings *settings);    
    virtual void setDisplayOutputs(int display); 
    virtual void setAudioVolume(float leftVol, float rightVol);
    virtual void setAudioMute(int mute);  
    virtual bool setPowerState(b_powerState pmState);
    virtual b_powerState getPowerState();
    virtual bool setCecPowerState(uint32_t cecId, b_powerState pmState);
    virtual bool getCecPowerStatus(uint32_t cecId, uint8_t *pPowerStatus);
    virtual bool sendCecMessage(uint32_t cecId, uint8_t destAddr, size_t length, uint8_t *pMessage);
    virtual bool isCecEnabled(uint32_t cecId);

protected:
#if NEXUS_HAS_CEC
    struct CecServiceManager;
#endif

    NexusService();
    virtual void platformInit();
    virtual void platformUninit();
    virtual NEXUS_ClientHandle getNexusClient(NexusClientContext * client);

    NexusServerContext                  *server;
    b_powerState                        powerState;
#if NEXUS_HAS_CEC
    sp<CecServiceManager>               mCecServiceManager[NEXUS_NUM_CEC];
#endif

private:
    /* These API's are private helper functions... */
    NEXUS_ClientHandle clientJoin(const b_refsw_client_client_name *pClientName, NEXUS_ClientAuthenticationSettings *pClientAuthenticationSettings);
    NEXUS_Error clientUninit(NEXUS_ClientHandle clientHandle);
    void platformInitSurfaceCompositor(void);
    void platformInitVideo(void);    
    void platformInitAudio(void);    
    void setDisplayState(bool enable);
    void setVideoState(bool enable);
    void setAudioState(bool enable);
    bool connectHdmiInput(NexusClientContext * client, b_refsw_client_connect_resource_settings *pConnectSettings);
    bool disconnectHdmiInput(NexusClientContext * client);

    NEXUS_SurfaceCompositorHandle       surface_compositor;
    DisplayState                        displayState[MAX_NUM_DISPLAYS];
    NEXUS_VideoDecoderHandle            videoDecoder[MAX_VIDEO_DECODERS];
    NEXUS_SimpleVideoDecoderHandle      simpleVideoDecoder[MAX_VIDEO_DECODERS];
    NEXUS_AudioDecoderHandle            audioDecoder[MAX_AUDIO_DECODERS];
    NEXUS_AudioPlaybackHandle           audioPlayback[MAX_AUDIO_DECODERS];
    NEXUS_SimpleAudioDecoderHandle      simpleAudioDecoder[MAX_AUDIO_DECODERS];
    NEXUS_SimpleAudioPlaybackHandle     simpleAudioPlayback[MAX_AUDIO_DECODERS];
    NEXUS_SimpleEncoderHandle           simpleEncoder[MAX_ENCODERS];
    NEXUS_AudioMixerHandle              mixer;
    NEXUS_SurfaceClientHandle           surfaceclient;
    NEXUS_Graphics2DHandle              gfx2D;
    BKNI_EventHandle                    gfxDone;
    NEXUS_VideoFormat                   initial_hd_format;
    NEXUS_VideoFormat                   initial_sd_format;    
};

#endif // _NEXUSSERVICE_H_
