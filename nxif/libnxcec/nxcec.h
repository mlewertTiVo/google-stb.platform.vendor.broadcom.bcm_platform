/******************************************************************************
 * (c) 2017 Broadcom
 *
 * This program is the proprietary software of Broadcom and/or its licensors,
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
#ifndef _NXCEC__H_
#define _NXCEC__H_

#include <utils/threads.h>
#include <utils/Errors.h>
#include "nxclient.h"

typedef enum {
   eCecDeviceType_eInactive = -1,
   eCecDeviceType_eTv = 0,
   eCecDeviceType_eRecordingDevice,
   eCecDeviceType_eReserved,
   eCecDeviceType_eTuner,
   eCecDeviceType_ePlaybackDevice,
   eCecDeviceType_eAudioSystem,
   eCecDeviceType_ePureCecSwitch,
   eCecDeviceType_eVideoProcessor,
   eCecDeviceType_eInvalid,
   eCecDeviceType_eMax = eCecDeviceType_eInvalid
} nxcec_cec_device_type;

#define PROPERTY_HDMI_ENABLE_CEC                "persist.nx.hdmi.enable_cec"
#define DEFAULT_PROPERTY_HDMI_ENABLE_CEC        1
#define PROPERTY_HDMI_TX_STANDBY_CEC            "persist.nx.hdmi.tx_standby_cec"
#define DEFAULT_PROPERTY_HDMI_TX_STANDBY_CEC    0
#define PROPERTY_HDMI_TX_VIEW_ON_CEC            "persist.nx.hdmi.tx_view_on_cec"
#define DEFAULT_PROPERTY_HDMI_TX_VIEW_ON_CEC    0
#define PROPERTY_HDMI_AUTO_WAKEUP_CEC           "persist.nx.hdmi.auto_wake_cec"
#define DEFAULT_PROPERTY_HDMI_AUTO_WAKEUP_CEC   1
#define PROPERTY_HDMI_HOTPLUG_WAKEUP            "ro.nx.hdmi.wake_on_hotplug"
#define DEFAULT_PROPERTY_HDMI_HOTPLUG_WAKEUP    0
#define PROPERTY_HDMI_CEC_VENDOR_ID             "ro.nx.hdmi.cec_vendor_id"
#define DEFAULT_PROPERTY_HDMI_CEC_VENDOR_ID     0x18C086

#define HDMI_CEC_MESSAGE_BODY_MAX_LENGTH        16

extern "C" nxcec_cec_device_type nxcec_get_cec_device_type();
extern "C" bool nxcec_is_cec_enabled();
extern "C" bool nxcec_get_cec_xmit_stdby();
extern "C" bool nxcec_get_cec_xmit_viewon();
extern "C" bool nxcec_is_cec_autowake_enabled();
extern "C" bool nxcec_set_cec_autowake_enabled(bool enabled);

using namespace android;

#endif

