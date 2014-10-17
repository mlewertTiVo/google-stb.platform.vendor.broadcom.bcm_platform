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
 * $brcm_Workfile: nexus_ipc_common.h $
 * 
 * Module Description:
 * This header file contains the prototype of the API functions and their params.
 * This file is included by both the client and the server code and provides the 
 * identical interface to them.
 * 
 *****************************************************************************/
#ifndef _NEXUS_IPC_COMMON_H_
#define _NEXUS_IPC_COMMON_H_

#include <cutils/properties.h>
#include <utils/Errors.h>

#include "INexusHdmiCecMessageEventListener.h"
#include "INexusHdmiHotplugEventListener.h"
#include "nexus_surface_compositor_types.h"
#include "nexus_types.h"
#include "nexus_platform.h"
#include "nexus_display.h"
#include "nexus_video_window.h"
#include "nexus_video_types.h"
#include "nexus_picture_ctrl.h"

/* Define the timeout to wait before polling for the NxClient server. */
#define NXCLIENT_SERVER_TIMEOUT_IN_MS (500)

/* Define the timeout to wait for polling the standby status. */
#define NXCLIENT_STANDBY_MONITOR_TIMEOUT_IN_MS  (20)

/* Define the maximum number of client ID's */
#define CLIENT_MAX_IDS 16

/* Define the maximum size of the client name */
#define CLIENT_MAX_NAME 32

#define MIN(a,b)    ((a) < (b)? (a) : (b))

using namespace android;

/* API parameters */
// adopted these from Trellis
typedef struct NexusClientContext NexusClientContext;
typedef struct NexusServerContext NexusServerContext;

typedef struct b_refsw_client_client_name  {
    char string[CLIENT_MAX_NAME];
} b_refsw_client_client_name;

typedef struct b_refsw_client_client_configuration {
    b_refsw_client_client_name name; /* name of the client application */
    unsigned pid;
    struct {
        struct {
            bool required;
            NEXUS_Rect position;
        } screen; /* graphics */
        bool audioDecoder;
        bool audioPlayback;
        bool videoDecoder;
        bool hdmiInput;
        bool encoder;
    } resources;
} b_refsw_client_client_configuration;

typedef struct b_refsw_client_client_info {
    NEXUS_Certificate certificate;
    NEXUS_SurfaceCompositorClientId surfaceClientId;
    unsigned audioDecoderId;
    unsigned videoDecoderId;
    unsigned audioPlaybackId;
    unsigned hdmiInputId;
    unsigned encoderId;
} b_refsw_client_client_info;

/* Subset of NEXUS_VideoWindowSettings */
typedef struct b_video_window_settings
{
    NEXUS_SurfaceRegion virtualDisplay;
    NEXUS_Rect position;
    NEXUS_Rect clipRect;
    bool visible;
    NEXUS_VideoWindowContentMode contentMode; 
    bool autoMaster;
    unsigned zorder;
} b_video_window_settings;

typedef enum b_powerState {
    ePowerState_S0, // Power on
    ePowerState_S1, // Standby S1
    ePowerState_S2, // Standby S2
    ePowerState_S3, // Standby S3 (deep sleep - aka suspend to ram)
    ePowerState_S4, // Suspend to disk (not implemented on our kernels)
    ePowerState_S5, // Poweroff (aka S3 cold boot)
    ePowerState_Max
} b_powerState;

typedef enum b_cecDeviceType
{
    eCecDeviceType_eTv = 0,
    eCecDeviceType_eRecordingDevice,
    eCecDeviceType_eReserved,
    eCecDeviceType_eTuner,
    eCecDeviceType_ePlaybackDevice,
    eCecDeviceType_eAudioSystem,
    eCecDeviceType_ePureCecSwitch,
    eCecDeviceType_eVideoProcessor,
    eCecDeviceType_eMax
} b_cecDeviceType;

typedef struct b_cecStatus {
    bool ready;                 /* device is ready */
    bool messageRx;             /* If true, call NEXUS_Cec_ReceiveMessage to receive a message. */
    bool messageTxPending;      /* If true, you must wait before calling NEXUS_Cec_TransmitMessage again. */
    bool txMessageAck;          /* status from last transmitted message */
    uint8_t physicalAddress[2];
    uint8_t logicalAddress;     /* 0xFF means uninitialized logical address */
    b_cecDeviceType deviceType;
    unsigned cecVersion;        /* Cec Protocol version the platform is running */
} b_cecStatus;

typedef struct b_hdmiOutputStatus
{
    bool connected;             /* True if Rx device is connected; device may be ON or OFF */
                                /* if !connected the remaining values should be ignored */
                                /* HDMI Rx capability information (EDID) can be read with Rx power off */

    bool rxPowered;             /* True if Rx device is powered ON, false OFF */
    bool hdmiDevice;            /* True if Rx supports HDMI, false if supports only DVI */

    uint8_t physicalAddress[2]; /* device physical address: read from attached EDID */
    NEXUS_VideoFormat videoFormat;
    NEXUS_VideoFormat preferredVideoFormat;  /* monitor's preferred video format */
    NEXUS_AspectRatio aspectRatio;
    NEXUS_ColorSpace colorSpace;
    NEXUS_AudioCodec audioFormat;
    unsigned audioSamplingRate; /* in units of Hz */
    unsigned audioSamplingSize;
    unsigned audioChannelCount;
} b_hdmiOutputStatus;

typedef struct b_video_decoder_caps
{
    unsigned fifoSize; /* actual fifo should be at least this size */
    unsigned maxWidth, maxHeight;
    unsigned colorDepth; /* 0 or 8 is 8-bit, 10 is 10-bit */
    bool supportedCodecs[NEXUS_VideoCodec_eMax];
    bool avc51Enabled;
} b_video_decoder_caps;

typedef struct b_audio_decoder_caps
{
    bool encoder; /* if true, prefer decoders which are dedicated for transcode */
} b_audio_decoder_caps;

typedef enum b_video_window_type
{
    eVideoWindowType_eMain, /* full screen capable */
    eVideoWindowType_ePip,  /* reduced size only. typically quarter screen. */
    eVideoWindowType_eNone, /* app will do video as graphics */
    eVideoWindowType_Max
} b_video_window_type;

typedef struct b_video_window_caps
{
    b_video_window_type type;
    unsigned maxWidth, maxHeight; /* TODO: these are unused becaused server API requires global, not per-client, id */
    bool encoder; /* if true, prefer decoders which are dedicated for transcode */
    bool deinterlaced; /* if true, deinterlacing is required. if false, not required. */
} b_video_window_caps;

typedef struct b_refsw_client_connect_resource_settings {

    struct {
        unsigned id; /* id used to acquire SimpleVideoDecoder. 0 is no request. */
        unsigned surfaceClientId; /* id for top-level surface client */
        unsigned windowId;
        b_video_decoder_caps decoderCaps;
        b_video_window_caps windowCaps;
    } simpleVideoDecoder[CLIENT_MAX_IDS];

    struct {
        unsigned id;    /* id used to acquire SimpleAudioDecoder. 0 is no request. */
        b_audio_decoder_caps decoderCaps;
    } simpleAudioDecoder;

    struct {
        unsigned id; /* id used to acquire SimpleAudioPlayback. 0 is no request. */
        struct {
            bool enabled;
            unsigned index;
        } i2s;        
    } simpleAudioPlayback[CLIENT_MAX_IDS];

    struct {
        unsigned id; /* one-based index of HdmiInput. 0 is no request */
        unsigned surfaceClientId; /* id for top-level surface client */
        unsigned windowId;
        b_video_window_caps windowCaps;
        bool hdDvi; /* if set, use hd-dvi interface instead of hdmiInput interface */
    } hdmiInput;

    struct {
        unsigned id; /* id used to acquire SimpleEncoder. 0 is no request. */
        bool display; /* Encode the display and audio for this session. Not for file transcoding. */
        
        struct {
            bool cpuAccessible; /* alloc encoder buffers in a client heap so that functions like NEXUS_SimpleEncoder_GetVideoBuffer
                                       and NEXUS_SimpleEncoder_GetAudioBuffer work. */
        } audio, video;
    } simpleEncoder[CLIENT_MAX_IDS];

} b_refsw_client_connect_resource_settings;

#ifdef __cplusplus
class NexusIPCCommon 
{
public: 
                 NexusIPCCommon()  { }
    virtual      ~NexusIPCCommon() { } // required to avoid build error "...has virtual functions and accessible non-virtual destructor"

    /* These API's require a Nexus Client Context as they handle per client resources... */
    virtual NexusClientContext * createClientContext(const b_refsw_client_client_configuration *config) = 0;
    virtual void destroyClientContext(NexusClientContext * client) = 0;
    virtual void getClientInfo(NexusClientContext * client, b_refsw_client_client_info *info) = 0;
    virtual void getClientComposition(NexusClientContext * client, NEXUS_SurfaceComposition *composition) = 0;
    virtual void setClientComposition(NexusClientContext * client, NEXUS_SurfaceComposition *composition) = 0;    
    virtual void getVideoWindowSettings(NexusClientContext * client, uint32_t window_id, b_video_window_settings *settings) = 0;
    virtual void setVideoWindowSettings(NexusClientContext * client, uint32_t window_id, b_video_window_settings *settings) = 0;
    virtual bool addGraphicsWindow(NexusClientContext * client)=0;
    virtual bool getFrame(NexusClientContext * client, const uint32_t width, const uint32_t height, 
        const uint32_t surfacePhyAddress, const NEXUS_PixelFormat surfaceFormat, const uint32_t decoderId) = 0;
    virtual bool connectClientResources(NexusClientContext * client, b_refsw_client_connect_resource_settings *pConnectSettings)=0;
    virtual bool disconnectClientResources(NexusClientContext * client)=0;

    /* These API's do NOT require a Nexus Client Context as they handle global resources...*/
    virtual status_t setHdmiCecMessageEventListener(uint32_t cecId, const sp<INexusHdmiCecMessageEventListener> &listener) = 0;
    virtual status_t setHdmiHotplugEventListener(uint32_t portId, const sp<INexusHdmiHotplugEventListener> &listener) = 0;

    virtual void getPictureCtrlCommonSettings(uint32_t window_id, NEXUS_PictureCtrlCommonSettings *settings) = 0;
    virtual void setPictureCtrlCommonSettings(uint32_t window_id, NEXUS_PictureCtrlCommonSettings *settings) = 0;
    virtual void getGraphicsColorSettings(uint32_t display_id, NEXUS_GraphicsColorSettings *settings) = 0;
    virtual void setGraphicsColorSettings(uint32_t display_id, NEXUS_GraphicsColorSettings *settings) = 0;
    virtual void getDisplaySettings(uint32_t display_id, NEXUS_DisplaySettings *settings) = 0;
    virtual void setDisplaySettings(uint32_t display_id, NEXUS_DisplaySettings *settings) = 0;
    virtual void setDisplayOutputs(int display) = 0;
    virtual void setAudioVolume(float leftVol, float rightVol) = 0;
    virtual void setAudioMute(int mute) = 0;
    virtual bool setPowerState(b_powerState pmState)=0;
    virtual b_powerState getPowerState()=0;
    virtual bool setCecPowerState(uint32_t cecId, b_powerState pmState)=0;
    virtual bool getCecPowerStatus(uint32_t cecId, uint8_t *pPowerStatus)=0;
    virtual bool getCecStatus(uint32_t cecId, b_cecStatus *pCecStatus)=0;
    virtual bool sendCecMessage(uint32_t cecId, uint8_t srcAddr, uint8_t destAddr, size_t length, uint8_t *pMessage)=0;
    virtual bool isCecEnabled(uint32_t cecId)=0;
    virtual bool setCecLogicalAddress(uint32_t cecId, uint8_t addr)=0;
    virtual bool getHdmiOutputStatus(uint32_t portId, b_hdmiOutputStatus *pHdmiOutputStatus)=0;
};

/* -----------------------------------------------------------------------------
   All utility functions that are common between client and server can be put 
   below as static functions 
   ---------------------------------------------------------------------------*/

/* This function reads ro.hd_output_format and ro.sd_output_format properties and 
   return respective enums for hd and sd formats. These HD and SD output formats
   are the initial configuration with which Android boots up. */
static void get_initial_output_formats_from_property(NEXUS_VideoFormat *hd_format, NEXUS_VideoFormat *sd_format)
{
    char value[PROPERTY_VALUE_MAX];
    NEXUS_VideoFormat fmt;

    // HD output format
    if(property_get("ro.hd_output_format", value, NULL)) 
    {
        if (strncmp((char *) value, "1080p",5)==0)
        {
            LOGW("Set output format to e1080p");
            fmt = NEXUS_VideoFormat_e1080p;
        }
        else if(strncmp((char *) value, "3d720p",6)==0)
        {
            LOGW("Set output format to e720p_3DOU_AS...");
            fmt = NEXUS_VideoFormat_e720p_3DOU_AS;
        }
        else
        {
            LOGW("Set output format to e720p");
            fmt = NEXUS_VideoFormat_e720p;
        }
    }
    else
    {
        LOGW("Set output format to default e720p");
        fmt = NEXUS_VideoFormat_e720p;
    }

    if(hd_format) *hd_format = fmt;

    // SD output format
    if(property_get("ro.sd_output_format", value, NULL)) 
    {
        if (strncmp((char *) value, "PAL",3)==0)
        {
            LOGW("Set SD output format to PAL...");
            fmt = NEXUS_VideoFormat_ePal;
        }
        else
        {
            LOGW("Set SD output format to NTSC");
            fmt = NEXUS_VideoFormat_eNtsc;
        }
    }
    else
    {
        LOGW("Set SD output format to default Ntsc");
        fmt = NEXUS_VideoFormat_eNtsc;
    }
    if(sd_format) *sd_format = fmt;

    return;
}

#endif /* __cplusplus */
#endif /* _NEXUS_IPC_COMMON_H_ */
