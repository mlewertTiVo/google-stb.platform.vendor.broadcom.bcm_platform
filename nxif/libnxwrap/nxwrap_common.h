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
#ifndef _NXWRAP_COMMON__H_
#define _NXWRAP_COMMON__H_

#define NXWRAP_NAME_MAX                         (32)
#define NXCLIENT_SERVER_TIMEOUT_IN_MS           (500)
#define NXCLIENT_PM_TIMEOUT_IN_MS               (5000)
#define NXCLIENT_PM_TIMEOUT_COUNT               (20)
#define NXCLIENT_STANDBY_MONITOR_TIMEOUT_IN_MS  (1000)

typedef enum {
   ePowerState_S0, // Power on
   ePowerState_S05,// Standby 0.5 = power on but with HDMI output disconnected
   ePowerState_S1, // Standby S1
   ePowerState_S2, // Standby S2
   ePowerState_S3, // Standby S3 (deep sleep - aka suspend to ram)
   ePowerState_S4, // Suspend to disk (not implemented on our kernels)
   ePowerState_S5, // Poweroff (aka S3 cold boot)
   ePowerState_Max
} nxwrap_pwr_state;

typedef struct {
   bool ir;
   bool uhf;
   bool keypad;
   bool gpio;
   bool nmi;
   bool cec;
   bool transport;
   bool timeout;
} nxwrap_wake_status;


extern "C" const char *nxwrap_get_power_string(nxwrap_pwr_state state);
extern "C" bool nxwrap_get_pwr_info(nxwrap_pwr_state *state, nxwrap_wake_status *wake);
extern "C" bool nxwrap_set_power_state(nxwrap_pwr_state state, bool cec_wake);

#endif
