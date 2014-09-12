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
 * $brcm_Workfile: nexus_ipc_priv.h $
 * 
 * Module Description:
 * This header file defines the command that is used to encapsulate an API call
 * from the client in a parcel and is sent over binder to the server.
 * 
 *****************************************************************************/
#ifndef _NEXUS_IPC_PRIV_H_
#define _NEXUS_IPC_PRIV_H_

// api's supported over ipc (binder)
enum api_name{
    api_clientJoin,
    api_clientUninit,
    api_createClientContext,
    api_destroyClientContext,
    api_getClientInfo,
    api_getClientComposition,
    api_setClientComposition,
    api_getVideoWindowSettings,
    api_setVideoWindowSettings,    
    api_getDisplaySettings,
    api_setDisplaySettings,    
    api_addGraphicsWindow,
    api_setAudioVolume,
    api_setPowerState,
    api_getPowerState,
    api_setCecPowerState,
    api_getCecPowerStatus,
    api_getCecStatus,
    api_sendCecMessage,
    api_setCecLogicalAddress,
    api_getHdmiOutputStatus,
    api_getFrame,
    api_connectClientResources,
    api_disconnectClientResources,
    api_getPictureCtrlCommonSettings,
    api_setPictureCtrlCommonSettings,
    api_getGraphicsColorSettings,
    api_setGraphicsColorSettings,
    api_setDisplayOutputs,
    api_setAudioMute
};

// in and out parameters for each of these api's
typedef union api_param
{
    struct {
        struct {
            b_refsw_client_client_name         clientName;
            NEXUS_ClientAuthenticationSettings clientAuthenticationSettings;
        } in;

        struct {
            NEXUS_ClientHandle clientHandle;
        } out;
    } clientJoin;

    struct {
        struct {
            NEXUS_ClientHandle clientHandle;
        } in;

        struct {
            NEXUS_Error status;
        } out;
    } clientUninit;

    struct {
        struct {
            b_refsw_client_client_configuration createClientConfig;
        } in;

        struct {
            NexusClientContext *client;
        } out;
    } createClientContext;

    struct {
        struct {
            NexusClientContext *client;
        } in;

        struct {
        } out;
    } destroyClientContext;

    struct {
        struct {
            NexusClientContext *client;
        } in;

        struct {
            b_refsw_client_client_info info;
        } out;
    } getClientInfo;

    struct {
        struct {
            NexusClientContext *client;
        } in;

        struct {
            NEXUS_SurfaceComposition composition;
        } out;
    } getClientComposition; 

    struct {
        struct {
            NexusClientContext *client;
            NEXUS_SurfaceComposition composition;
        } in;

        struct {
        } out;
    } setClientComposition; 

    struct {
        struct {
            NexusClientContext *client;
            uint32_t window_id;
        } in;

        struct {
            b_video_window_settings settings;
        } out;
    } getVideoWindowSettings;

    struct {
        struct {
            NexusClientContext *client;
            uint32_t window_id;
            b_video_window_settings settings;
        } in;

        struct {
        } out;
    } setVideoWindowSettings;

    struct {
        struct {
            uint32_t display_id;
        } in;

        struct {
            NEXUS_DisplaySettings settings;                
        } out;
    } getDisplaySettings;

    struct {
        struct {
            uint32_t display_id;
            NEXUS_DisplaySettings settings;       
        } in;

        struct {
        } out;
    } setDisplaySettings;

    struct {
        struct {
            NexusClientContext *client;
        } in;

        struct {
            bool status;
        } out;
    } addGraphicsWindow;

    struct {
        struct {
            float leftVolume;
            float rightVolume;
        } in;        

        struct {
        } out;
    } setAudioVolume;

    struct {
        struct {
            NexusClientContext * client;
            uint32_t width;
            uint32_t height;
            uint32_t surfacePhyAddress;
            NEXUS_PixelFormat surfaceFormat; 
            uint32_t decoderId;
        } in;

        struct {
            bool status;
        } out;
    } getFrame;

    struct {
        struct {
            b_powerState pmState;
        } in;

        struct {
            bool status;
        } out;
    } setPowerState;

    struct {
        struct {
            b_powerState pmState;
        } out;
    } getPowerState;

    struct {
        struct {
            uint32_t     cecId;
            b_powerState pmState;
        } in;

        struct {
            bool status;
        } out;
    } setCecPowerState;

    struct {
        struct {
            uint32_t cecId;
        } in;

        struct {
            uint8_t powerStatus;
            bool status;
        } out;
    } getCecPowerStatus;

    struct {
        struct {
            uint32_t cecId;
        } in;

        struct {
            b_cecStatus cecStatus;
            bool status;
        } out;
    } getCecStatus;

    struct {
        struct {
            uint32_t cecId;
            uint8_t  srcAddr;
            uint8_t  destAddr;
            size_t   length;
            uint8_t  message[NEXUS_CEC_MESSAGE_DATA_SIZE];
        } in;

        struct {
            bool status;
        } out;
    } sendCecMessage;

    struct {
        struct {
            uint32_t cecId;
            uint8_t  addr;
        } in;

        struct {
            bool status;
        } out;
    } setCecLogicalAddress;

    struct {
        struct {
            uint32_t portId;
        } in;

        struct {
            b_hdmiOutputStatus hdmiOutputStatus;
            bool status;
        } out;
    } getHdmiOutputStatus;

    struct {
        struct {
            NexusClientContext *client;
            b_refsw_client_connect_resource_settings connectSettings;
        } in;

        struct {
            bool status;
        } out;
    } connectClientResources;

    struct {
        struct {
            NexusClientContext *client;
        } in;

        struct {
            bool status;
        } out;
    } disconnectClientResources;

    struct {
        struct {
            uint32_t window_id;
        } in;

        struct {
            NEXUS_PictureCtrlCommonSettings settings;                
        } out;
    } getPictureCtrlCommonSettings;

    struct {
        struct {
            uint32_t window_id;
            NEXUS_PictureCtrlCommonSettings settings;       
        } in;

        struct {
        } out;
    } setPictureCtrlCommonSettings;

    struct {
        struct {
            uint32_t display_id;
        } in;

        struct {
            NEXUS_GraphicsColorSettings settings;                
        } out;
    } getGraphicsColorSettings;

    struct {
        struct {
            uint32_t display_id;
            NEXUS_GraphicsColorSettings settings;       
        } in;

        struct {
        } out;
    } setGraphicsColorSettings;

    struct {
        struct {
            int display;            
        } in;

        struct {
        } out;
    } setDisplayOutputs;

    struct {
        struct {
            int mute;             
        } in;

        struct {
        } out;
    } setAudioMute;

} api_param;

// api data that is actually transferred over ipc (binder)
typedef struct api_data {
    api_name api;
    api_param param;
} api_data;

#endif
