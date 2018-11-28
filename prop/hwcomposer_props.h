/******************************************************************************
 *    (c)2018 Broadcom Corporation
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
 *****************************************************************************/
#ifndef VENDOR_BCM_PROPS__HWCOMPOSER
#define VENDOR_BCM_PROPS__HWCOMPOSER

/* properties used by the hwcomposer subsytem. */

#define BCM_RO_HWC2_EXT_GLES                          "ro.nx.hwc2.ext.gles"
#define BCM_RO_HWC2_EXT_AFB_W                         "ro.nx.hwc2.afb.w"
#define BCM_RO_HWC2_EXT_AFB_H                         "ro.nx.hwc2.afb.h"
#define BCM_RO_HWC2_EXT_NFB_W                         "ro.nx.hwc2.nfb.w"
#define BCM_RO_HWC2_EXT_NFB_H                         "ro.nx.hwc2.nfb.h"
#define BCM_RO_HWC2_GFB_MAX_W                         "ro.nx.hwc2.gfb.w"
#define BCM_RO_HWC2_GFB_MAX_H                         "ro.nx.hwc2.gfb.h"
#define BCM_RO_HWC2_SF_LCD_DENSITY                    "ro.nx.sf.lcd_density"

#define BCM_DYN_HWC2_DUMP_SET                         "dyn.nx.hwc2.dump.data"
#define BCM_DYN_HWC2_DUMP_NOW                         "dyn.nx.hwc2.dump.this"
#define BCM_DYN_HWC2_VIDOUT_FMT                       "dyn.nx.vidout.hwc"

/* logging inside hwc.
 *
 * set the property mask (setprop <name> <value>), then issue "dumpsys SurfaceFlinger"
 * to apply changes.  logs will start to show under "logcat -s bcm-hwc:v".
 *
 * log masks bitset are defined in hwc2.h.
 */
#define BCM_DYN_HWC2_LOG_GLB                           "dyn.nx.hwc2.log.mask"
#define BCM_DYN_HWC2_LOG_EXT                           "dyn.nx.hwc2.log.ext.mask"
#define BCM_DYN_HWC2_LOG_VD                            "dyn.nx.hwc2.log.vd.mask"


/* tweaks for hwc behavior. */

#define BCM_RO_HWC2_TWEAK_FBCOMP                       "ro.nx.hwc2.tweak.fbcomp"
#define BCM_RO_HWC2_TWEAK_NOCB                         "ro.nx.hwc2.tweak.nocb"
#define BCM_RO_HWC2_TWEAK_FEOTF                        "ro.nx.hwc2.tweak.force_eotf"
#define BCM_RO_HWC2_TWEAK_HPD0                         "ro.nx.hwc2.tweak.hpd0"

#define BCM_DYN_HWC2_TWEAK_EOTF                        "dyn.nx.hwc2.tweak.eotf"
#define BCM_DYN_HWC2_TWEAK_PLMOFF                      "dyn.nx.hwc2.tweak.plmoff"
#define BCM_DYN_HWC2_TWEAK_SGLES                       "dyn.nx.hwc2.tweak.sgles"
#define BCM_DYN_HWC2_TWEAK_ONE_CFG                     "dyn.nx.hwc2.tweak.onecfg"

#endif /* VENDOR_BCM_PROPS__HWCOMPOSER */

