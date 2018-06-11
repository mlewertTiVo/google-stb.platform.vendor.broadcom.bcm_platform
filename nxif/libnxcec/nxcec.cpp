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
#define LOG_TAG "nxcec"
//#define LOG_NDEBUG 0

#include <utils/Log.h>
#include <string.h>
#include <cutils/atomic.h>
#include <utils/Errors.h>
#include "cutils/properties.h"
#include <nxcec.h>

extern "C" nxcec_cec_device_type nxcec_to_cec_device_type(const char *device) {
   int type = atoi(device);

   switch (type) {
   case -1: return eCecDeviceType_eInactive; break;
   case  0: return eCecDeviceType_eTv; break;
   case  1: return eCecDeviceType_eRecordingDevice; break;
   case  2: return eCecDeviceType_eReserved; break;
   case  3: return eCecDeviceType_eTuner; break;
   case  4: return eCecDeviceType_ePlaybackDevice; break;
   case  5: return eCecDeviceType_eAudioSystem; break;
   case  6: return eCecDeviceType_ePureCecSwitch; break;
   case  7: return eCecDeviceType_eVideoProcessor; break;
   default: return eCecDeviceType_eInvalid;
   }
}

extern "C" nxcec_cec_device_type nxcec_get_cec_device_type() {
   char value[PROPERTY_VALUE_MAX];
   nxcec_cec_device_type type = eCecDeviceType_eInvalid;

   if (property_get("ro.nx.hdmi.device_type", value, NULL)) {
      type = nxcec_to_cec_device_type(value);
   }
   return type;
}

extern "C" bool nxcec_is_cec_enabled() {
   return property_get_bool(PROPERTY_HDMI_ENABLE_CEC,
                            DEFAULT_PROPERTY_HDMI_ENABLE_CEC);
}

extern "C" bool nxcec_get_cec_xmit_stdby() {
   return property_get_bool(PROPERTY_HDMI_TX_STANDBY_CEC,
                            DEFAULT_PROPERTY_HDMI_TX_STANDBY_CEC);
}

extern "C" bool nxcec_get_cec_xmit_viewon() {
   return property_get_bool(PROPERTY_HDMI_TX_VIEW_ON_CEC,
                            DEFAULT_PROPERTY_HDMI_TX_VIEW_ON_CEC);
}

extern "C" bool nxcec_is_cec_autowake_enabled() {
   return property_get_bool(PROPERTY_HDMI_AUTO_WAKEUP_CEC,
                            DEFAULT_PROPERTY_HDMI_AUTO_WAKEUP_CEC);
}

extern "C" bool nxcec_set_cec_autowake_enabled(bool enabled) {
   char value[PROPERTY_VALUE_MAX];
   snprintf(value, PROPERTY_VALUE_MAX, "%d", enabled);
   if (property_set(PROPERTY_HDMI_AUTO_WAKEUP_CEC, value) != 0) {
      return false;
   } else {
      return true;
   }
}
