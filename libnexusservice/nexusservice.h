/******************************************************************************
 *    (c)2011-2015 Broadcom Corporation
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
#include "nexus_stc_channel.h"
#include "nexus_surface_compositor.h"
#include "nexus_picture_ctrl.h"

#include <linux/brcmstb/hdmi_hpd_switch.h>

#define HD_DISPLAY (0)

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

typedef struct NexusServerContext
{
    NexusServerContext();
    ~NexusServerContext() { ALOGV("%s: called", __PRETTY_FUNCTION__); }

    Mutex mLock;
#if NEXUS_HAS_HDMI_OUTPUT
    Vector<sp<INexusHdmiHotplugEventListener> > mHdmiHotplugEventListenerList[NEXUS_NUM_HDMI_OUTPUTS];
#endif

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
    virtual NexusClientContext * createClientContext(const b_refsw_client_client_name *pClientName, unsigned clientPid);
    virtual void destroyClientContext(NexusClientContext * client);

    /* These API's do NOT require a Nexus Client Context as they handle global resources...*/
    virtual status_t setHdmiCecMessageEventListener(uint32_t cecId, const sp<INexusHdmiCecMessageEventListener> &listener);
    virtual status_t addHdmiHotplugEventListener(uint32_t portId, const sp<INexusHdmiHotplugEventListener> &listener);
    virtual status_t removeHdmiHotplugEventListener(uint32_t portId, const sp<INexusHdmiHotplugEventListener> &listener);

    virtual bool setPowerState(b_powerState pmState);
    virtual b_powerState getPowerState();
    virtual bool setCecPowerState(uint32_t cecId, b_powerState pmState);
    virtual bool getCecPowerStatus(uint32_t cecId, uint8_t *pPowerStatus);
    virtual bool getCecStatus(uint32_t cecId, b_cecStatus *pCecStatus);
    virtual bool sendCecMessage(uint32_t cecId, uint8_t srcAddr, uint8_t destAddr, size_t length, uint8_t *pMessage, uint8_t maxRetries);
    virtual bool isCecEnabled(uint32_t cecId);
    virtual bool setCecAutoWakeupEnabled(uint32_t cecId, bool enabled);
    virtual bool isCecAutoWakeupEnabled(uint32_t cecId);
    virtual b_cecDeviceType getCecDeviceType(uint32_t cecId);
    virtual bool setCecLogicalAddress(uint32_t cecId, uint8_t addr);
    virtual bool setCecPhysicalAddress(unsigned cecId, uint16_t addr);
    virtual bool getHdmiOutputStatus(uint32_t portId, b_hdmiOutputStatus *pHdmiOutputStatus);

protected:
    struct CecServiceManager;

    NexusService();
    virtual void platformInit();
    virtual void platformUninit();
    virtual NEXUS_ClientHandle getNexusClient(unsigned pid);
    static NEXUS_VideoFormat getForcedOutputFormat(void);

    static const char *getPowerString(b_powerState pmState);

    NexusServerContext                  *server;
    b_powerState                        powerState;
    hdmi_state                          mHotplugConnectedState;
    sp<CecServiceManager>               mCecServiceManager[NexusIPCCommon::MAX_NUM_CEC_PORTS];

private:
    /* These API's are private helper functions... */
    int platformSetupHdmiOutputs(void);
    void platformCleanHdmiOutputs();
    static void hdmiOutputHotplugCallback(void *context, int param);
    static void hdmiOutputHdcpStateChangedCallback(void *pContext, int param);
    static b_cecDeviceType toCecDeviceType(char *string);
};

#endif // _NEXUSSERVICE_H_
