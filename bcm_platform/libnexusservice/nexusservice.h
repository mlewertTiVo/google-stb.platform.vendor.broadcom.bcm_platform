/******************************************************************************
 *    (c)2011-2014 Broadcom Corporation
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
#if NEXUS_HAS_HDMI_OUTPUT
#include "nexus_hdmi_output.h"
#include "nexus_hdmi_output_hdcp.h"
#if NEXUS_HAS_CEC
#include "nexus_cec.h"
#endif
#endif
#if NEXUS_HAS_HDMI_INPUT
#include "nexus_hdmi_input.h"
#endif
#include "nexus_video_input.h"
#include "nexus_surface.h"
#include "nexus_graphics2d.h"
#include "nexus_core_utils.h"
#include "bkni.h"
#include "bkni_multi.h"
#include "blst_list.h"
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
#define NEXUS_ABSTRACTED_JOIN(auth) NEXUS_Platform_AuthenticatedJoin(auth)

#define MAX_NUM_DISPLAYS (1)

#if (NEXUS_NUM_VIDEO_WINDOWS >= 2)
#define MAX_VIDEO_WINDOWS_PER_DISPLAY (2)
#else
#define MAX_VIDEO_WINDOWS_PER_DISPLAY (NEXUS_NUM_VIDEO_WINDOWS)
#endif

#ifndef NEXUS_NUM_VIDEO_ENCODERS
#define NEXUS_NUM_VIDEO_ENCODERS 0
#endif
#define HD_DISPLAY (0)
#define MAX_AUDIO_DECODERS  ((NEXUS_NUM_AUDIO_DECODERS  < 2) ? NEXUS_NUM_AUDIO_DECODERS  : 2)
#define MAX_AUDIO_PLAYBACKS ((NEXUS_NUM_AUDIO_PLAYBACKS < 2) ? NEXUS_NUM_AUDIO_PLAYBACKS : 2)
#define MAX_VIDEO_DECODERS  ((NEXUS_NUM_VIDEO_DECODERS  < 2) ? NEXUS_NUM_VIDEO_DECODERS  : 2)
#define MAX_ENCODERS        ((NEXUS_NUM_VIDEO_ENCODERS  < 2) ? NEXUS_NUM_VIDEO_ENCODERS  : 2)
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

typedef struct NexusServerContext
{
    NexusServerContext();
    ~NexusServerContext() { ALOGV("%s: called", __PRETTY_FUNCTION__); }

    Mutex mLock;
    unsigned mJoinRefCount;
    BLST_D_HEAD(b_refsw_client_list, NexusClientContext) clients;
#if NEXUS_HAS_HDMI_OUTPUT
    Vector<sp<INexusHdmiHotplugEventListener> > mHdmiHotplugEventListenerList[NEXUS_NUM_HDMI_OUTPUTS];
#endif

    struct {
        unsigned client;
        NEXUS_SurfaceCompositorClientId surfaceClientId;
    } lastId;
} NexusServerContext;

class NexusService : public NexusServiceBase, public BnNexusService, public IBinder::DeathRecipient
{
public:
    static void instantiate();
    virtual ~NexusService();
    status_t onTransact(uint32_t code,
                                 const Parcel &data,
                                 Parcel *reply,
                                 uint32_t flags);

    virtual void binderDied(const wp<IBinder>& who);

    /* These API's require a Nexus Client Context as they handle per client resources.
       Each of these methods *MUST* be implemented for each class that derives from it
       because NexusClientContext cannot be referenced between implementations. */
    virtual NexusClientContext * createClientContext(const b_refsw_client_client_configuration *config);
    virtual void destroyClientContext(NexusClientContext * client);
    virtual void getClientInfo(NexusClientContext * client, b_refsw_client_client_info *info);  
    virtual void getClientComposition(NexusClientContext * client, NEXUS_SurfaceComposition *composition);
    virtual void setClientComposition(NexusClientContext * client, NEXUS_SurfaceComposition *composition);
    virtual bool addGraphicsWindow(NexusClientContext * client);
    virtual bool getFrame(NexusClientContext * client, const uint32_t width, const uint32_t height, 
        const uint32_t surfacePhyAddress, const NEXUS_PixelFormat surfaceFormat, const uint32_t decoderId);
    virtual bool connectClientResources(NexusClientContext * client, b_refsw_client_connect_resource_settings *pConnectSettings);
    virtual bool disconnectClientResources(NexusClientContext * client);

    /* These API's do NOT require a Nexus Client Context as they handle global resources...*/
    virtual status_t setHdmiCecMessageEventListener(uint32_t cecId, const sp<INexusHdmiCecMessageEventListener> &listener);
    virtual status_t addHdmiHotplugEventListener(uint32_t portId, const sp<INexusHdmiHotplugEventListener> &listener);
    virtual status_t removeHdmiHotplugEventListener(uint32_t portId, const sp<INexusHdmiHotplugEventListener> &listener);

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
    virtual bool getCecStatus(uint32_t cecId, b_cecStatus *pCecStatus);
    virtual bool sendCecMessage(uint32_t cecId, uint8_t srcAddr, uint8_t destAddr, size_t length, uint8_t *pMessage);
    virtual bool setCecEnabled(uint32_t cecId, bool enabled);
    virtual bool isCecEnabled(uint32_t cecId);
    virtual bool setCecAutoWakeupEnabled(uint32_t cecId, bool enabled);
    virtual bool isCecAutoWakeupEnabled(uint32_t cecId);
    virtual bool setCecLogicalAddress(uint32_t cecId, uint8_t addr);
    virtual bool getHdmiOutputStatus(uint32_t portId, b_hdmiOutputStatus *pHdmiOutputStatus);

protected:
#if NEXUS_HAS_CEC
    struct CecServiceManager;
#endif

    NexusService();
    virtual void platformInit();
    virtual void platformUninit();
    virtual NEXUS_ClientHandle getNexusClient(unsigned pid, const char * name);

    NexusServerContext                  *server;
    b_powerState                        powerState;
#if NEXUS_HAS_CEC
    sp<CecServiceManager>               mCecServiceManager[NEXUS_NUM_CEC];
#endif

private:
    /* These API's are private helper functions... */
    NEXUS_ClientHandle clientJoin(const b_refsw_client_client_name *pClientName, NEXUS_ClientAuthenticationSettings *pClientAuthenticationSettings);
    NEXUS_Error clientUninit(NEXUS_ClientHandle clientHandle);
    int platformInitSurfaceCompositor(void);
    int platformInitVideo(void);    
    int platformInitAudio(void);    
    int platformInitHdmiOutputs(void);
    void platformUninitHdmiOutputs();
    static void hdmiOutputHotplugCallback(void *context, int param);
    static void hdmiOutputHdcpStateChangedCallback(void *pContext, int param);
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
    NEXUS_AudioPlaybackHandle           audioPlayback[MAX_AUDIO_PLAYBACKS];
    NEXUS_SimpleAudioDecoderHandle      simpleAudioDecoder[MAX_AUDIO_DECODERS];
    NEXUS_SimpleAudioPlaybackHandle     simpleAudioPlayback[MAX_AUDIO_PLAYBACKS];
    NEXUS_SimpleEncoderHandle           simpleEncoder[MAX_ENCODERS];
    NEXUS_AudioMixerHandle              mixer;
    NEXUS_SurfaceClientHandle           surfaceclient;
    NEXUS_Graphics2DHandle              gfx2D;
    BKNI_EventHandle                    gfxDone;
    NEXUS_VideoFormat                   initial_hd_format;
    NEXUS_VideoFormat                   initial_sd_format;    
};

#endif // _NEXUSSERVICE_H_
