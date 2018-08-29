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
#ifndef VENDOR_BCM_PROPS__POWER
#define VENDOR_BCM_PROPS__POWER

/* properties used by the power subsystem. */

#define BCM_RO_POWER_ETH_EN                    "ro.nx.pm.eth_en"
#define BCM_RO_POWER_MOCA_EN                   "ro.nx.pm.moca_en"
#define BCM_RO_POWER_SATA_EN                   "ro.nx.pm.sata_en"
#define BCM_RO_POWER_TP1_EN                    "ro.nx.pm.tp1_en"
#define BCM_RO_POWER_TP2_EN                    "ro.nx.pm.tp2_en"
#define BCM_RO_POWER_TP3_EN                    "ro.nx.pm.tp3_en"
#define BCM_RO_POWER_DDR_PM_EN                 "ro.nx.pm.ddr_pm_en"
#define BCM_RO_POWER_CPU_FREQ_SCALE_EN         "ro.nx.pm.cpufreq_scale_en"
#define BCM_RO_POWER_WOL_EN                    "ro.nx.pm.wol.en"
#define BCM_RO_POWER_WOL_SCREEN_ON_EN          "ro.nx.pm.wol.screen.on.en"
#define BCM_RO_POWER_WOL_OPTS                  "ro.nx.pm.wol.opts"
#define BCM_RO_POWER_WOL_MDNS_EN               "ro.nx.pm.wol.mdns.en"

#define BCM_DYN_POWER_BOOT_WAKEUP              "dyn.nx.boot.wakeup"

#define BCM_PERSIST_POWER_SYS_DOZE_TIMEOUT     "persist.vendor.power.doze.timeout"
#define BCM_PERSIST_POWER_SYS_WAKE_TIMEOUT     "persist.vendor.power.wake.timeout"
#define BCM_PERSIST_POWER_SYS_DOZESTATE        "persist.vendor.power.dozestate"
#define BCM_PERSIST_POWER_SYS_OFFSTATE         "persist.vendor.power.offstate"
#define BCM_PERSIST_POWER_SCREEN_ON            "persist.nx.screen.on"
#define BCM_PERSIST_POWER_KEEP_SCREEN_STATE    "persist.vendor.nx.keep.screen.state"

#endif /* VENDOR_BCM_PROPS__POWER */

