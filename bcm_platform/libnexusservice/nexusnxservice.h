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
 * $brcm_Workfile: nexusnxservice.h $
 * $brcm_Revision: $
 * $brcm_Date: $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/libnexusservice/nexusnxservice.h $
 * 
 *****************************************************************************/
#ifndef _NEXUSNXSERVICE_H_
#define _NEXUSNXSERVICE_H_

#include "nexusservice.h"
#include "nexusirhandler.h"

class NexusNxService : public NexusService
{
public:
    static void instantiate();
    virtual ~NexusNxService();

    /* These API's require a Nexus Client Context as they handle per client resources... */
    virtual NexusClientContext * createClientContext(const b_refsw_client_client_configuration *config);
    virtual void destroyClientContext(NexusClientContext * client);
    virtual void getClientInfo(NexusClientContext * client, b_refsw_client_client_info *info);  
    virtual void getClientComposition(NexusClientContext * client, NEXUS_SurfaceComposition *composition);
    virtual void setClientComposition(NexusClientContext * client, NEXUS_SurfaceComposition *composition);
    virtual void getVideoWindowSettings(NexusClientContext * client, uint32_t window_id, b_video_window_settings *settings);
    virtual void setVideoWindowSettings(NexusClientContext * client, uint32_t window_id, b_video_window_settings *settings);
    virtual bool addGraphicsWindow(NexusClientContext * client);
    //virtual bool getFrame(NexusClientContext * client, const uint32_t width, const uint32_t height, 
        //const uint32_t surfacePhyAddress, const NEXUS_PixelFormat surfaceFormat, const uint32_t decoderId);
    virtual bool connectClientResources(NexusClientContext * client, b_refsw_client_connect_resource_settings *pConnectSettings);
    virtual bool disconnectClientResources(NexusClientContext * client);

    /* These API's do NOT require a Nexus Client Context as they handle global resources...*/
    //virtual status_t setHdmiCecMessageEventListener(uint32_t cecId, const sp<INexusHdmiCecMessageEventListener> &listener);
    virtual status_t setHdmiHotplugEventListener(uint32_t portId, const sp<INexusHdmiHotplugEventListener> &listener);

    //virtual void getPictureCtrlCommonSettings(uint32_t window_id, NEXUS_PictureCtrlCommonSettings *settings);
    //virtual void setPictureCtrlCommonSettings(uint32_t window_id, NEXUS_PictureCtrlCommonSettings *settings);
    //virtual void getGraphicsColorSettings(uint32_t display_id, NEXUS_GraphicsColorSettings *settings);
    //virtual void setGraphicsColorSettings(uint32_t display_id, NEXUS_GraphicsColorSettings *settings); 
    //virtual void getDisplaySettings(uint32_t display_id, NEXUS_DisplaySettings *settings);
    //virtual void setDisplaySettings(uint32_t display_id, NEXUS_DisplaySettings *settings);    
    //virtual void setDisplayOutputs(int display); 
    //virtual void setAudioVolume(float leftVol, float rightVol);
    //virtual void setAudioMute(int mute);  
    //virtual bool setPowerState(b_powerState pmState);
    //virtual b_powerState getPowerState();
    //virtual bool setCecPowerState(uint32_t cecId, b_powerState pmState);
    //virtual bool getCecPowerStatus(uint32_t cecId, uint8_t *pPowerStatus);
    //virtual bool getCecStatus(uint32_t cecId, b_cecStatus *pCecStatus);
    //virtual bool sendCecMessage(uint32_t cecId, uint8_t srcAddr, uint8_t destAddr, size_t length, uint8_t *pMessage);
    //virtual bool isCecEnabled(uint32_t cecId);
    //virtual bool setCecLogicalAddress(uint32_t cecId, uint8_t addr);
    virtual bool getHdmiOutputStatus(uint32_t portId, b_hdmiOutputStatus *pHdmiOutputStatus);

protected:
#if NEXUS_HAS_CEC
    struct CecServiceManager;
#endif
    NexusNxService();
    virtual void platformInit();
    virtual void platformUninit();
    //virtual NEXUS_ClientHandle getNexusClient(NexusClientContext * client);

private:
    int platformInitHdmiOutputs(void);
    void platformUninitHdmiOutputs();
    static void hotplugCallback(void *context, int param);
    static void hdcpCallback(void *context, int param);

    bool platformInitIR();
    void platformUninitIR();

    NEXUS_SurfaceCompositorClientId     graphicSurfaceClientId;
    NexusIrHandler irHandler;
};

#endif // _NEXUSNXSERVICE_H_
