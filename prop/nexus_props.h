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
#ifndef VENDOR_BCM_PROPS__NEXUS
#define VENDOR_BCM_PROPS__NEXUS

/* properties used by the nexus integration (nexus server and hidl). */

#define BCM_RO_NX_GR_G2D                              "ro.nx.g2d"
#define BCM_RO_NX_DEV_ASHMEM                          "ro.nx.ashmem.devname"
#define BCM_RO_NX_IR_INITIAL_TIMEOUT                  "ro.nx.ir_remote.initial_timeout"
#define BCM_RO_NX_IR_TIMEOUT                          "ro.nx.ir_remote.timeout"
#define BCM_RO_NX_BOOT_COMPLETED                      "ro.nx.boot_completed"
#define BCM_RO_NX_WAKEUP_KEY                          "ro.nx.ir_remote.wakeup.button"
#define BCM_RO_NX_IR_NAME                             "ro.nx.ir_remote.name"
#define BCM_RO_NX_IR_MODE                             "ro.nx.ir_remote.mode"
#define BCM_RO_NX_IR_MAP                              "ro.nx.ir_remote.map"
#define BCM_RO_NX_IR_MASK                             "ro.nx.ir_remote.mask"
#define BCM_RO_NX_ENC_RES_WIDTH                       "ro.nx.enc.max.width"
#define BCM_RO_NX_ENC_RES_HEIGHT                      "ro.nx.enc.max.height"
#define BCM_RO_NX_ENC_ENABLE_ALL                      "ro.nx.enc.all"
#define BCM_RO_NX_DHD_SECDMA                          "ro.nx.dhd.secdma"
#define BCM_RO_NX_LMK_BG                              "ro.nx.lmk.bg"
#define BCM_RO_NX_LMK_TA                              "ro.nx.lmk.ta"
#define BCM_RO_NX_ACT_GC                              "ro.nx.act.gc"
#define BCM_RO_NX_ACT_GS                              "ro.nx.act.gs"
#define BCM_RO_NX_ACT_LMK                             "ro.nx.act.lmk"
#define BCM_RO_NX_ACT_WD                              "ro.nx.act.wd"
#define BCM_RO_NX_ACT_EV                              "ro.nx.act.ev"
#define BCM_RO_NX_MMA_SHRINK_THRESHOLD                "ro.nx.heap.shrink"
#define BCM_RO_NX_AUDIO_LOUDNESS                      "ro.nx.audio_loudness"
#define BCM_RO_NX_CAPABLE_COMP_BYPASS                 "ro.nx.capable.cb"
#define BCM_RO_NX_COMP_VIDEO                          "ro.nx.cvbs"
#define BCM_RO_NX_CAPABLE_FRONT_END                   "ro.nx.capable.fe"
#define BCM_RO_NX_NO_OUTPUT_VIDEO                     "ro.nx.output.dis"
#define BCM_RO_NX_WD_TIMEOUT                          "ro.nx.wd.timeout"
#define BCM_RO_NX_CAPABLE_DTU                         "ro.nx.capable.dtu"
#define BCM_RO_NX_ODV                                 "ro.nx.odv"
#define BCM_RO_NX_ODV_ALT_THRESHOLD                   "ro.nx.odv.use.alt"
#define BCM_RO_NX_ODV_ALT_1_USAGE                     "ro.nx.odv.a1.use"
#define BCM_RO_NX_ODV_ALT_2_USAGE                     "ro.nx.odv.a2.use"
#define BCM_RO_NX_HD_OUT_COLOR_DEPTH_10B              "ro.nx.colordepth10b.force"
#define BCM_RO_NX_SVP                                 "ro.nx.svp"
#define BCM_RO_NX_SPLASH                              "ro.nx.splash"
#define BCM_RO_NX_HDCP_MODE                           "ro.nx.hdcp.mode"
#define BCM_RO_NX_HDCP1X_KEY                          "ro.nx.nxserver.hdcp1x_keys"
#define BCM_RO_NX_HDCP2X_KEY                          "ro.nx.nxserver.hdcp2x_keys"
#define BCM_RO_NX_CFG_THERMAL                         "ro.nx.nxserver.thermal"
#define BCM_RO_NX_LOGGER_DISABLED                     "ro.nx.logger_disabled"
#define BCM_RO_NX_LOGGER_SIZE                         "ro.nx.logger_size"
#define BCM_RO_NX_WRN_MODULES                         "ro.nx.wrn_modules"
#define BCM_RO_NX_MSG_MODULES                         "ro.nx.msg_modules"
#define BCM_RO_NX_TRACE_MODULES                       "ro.nx.trace_modules"
#define BCM_RO_NX_AUDIO_LOG                           "ro.nx.audio_log"
#define BCM_RO_NX_HD_OUT_OBR                          "ro.nx.vidout.obr"
#define BCM_RO_NX_AP_NUM                              "ro.nx.audio.pbk"
#define BCM_RO_NX_AP_FIFO_SZ                          "ro.nx.audio.pbkfifosz"
#define BCM_RO_NX_BPCM_NUM                            "ro.nx.audio.bpcm"
#define BCM_RO_NX_PBAND                               "ro.nx.trpt.pband"
#define BCM_RO_NX_PPUMP                               "ro.nx.trpt.ppump"
#define BCM_RO_NX_EV_MAXCLI                           "ro.nx.ev.maxcli"

#define BCM_DYN_NX_STATE                              "dyn.nx.state"
#define BCM_SYS_RECOVERY_NX_STATE                     "sys.recov.nx.state"
#define BCM_DYN_NX_BOOT_KEY_TWO                       "dyn.nx.boot.key2"
#define BCM_DYN_NX_HDCP_FORCE                         "dyn.nx.hdcp.force"
#define BCM_DYN_NX_DISPLAY_SIZE                       "dyn.nx.display-size"
#define BCM_VDR_DISPLAY_SIZE                          "vendor.display-size"

/* nexus 'static' heap size allocations. */
#define BCM_RO_NX_HEAP_MAIN                           "ro.nx.heap.main"
#define BCM_RO_NX_HEAP_GFX                            "ro.nx.heap.gfx"
#define BCM_RO_NX_HEAP_GFX2                           "ro.nx.heap.gfx2"
#define BCM_RO_NX_HEAP_VIDEO_SECURE                   "ro.nx.heap.video_secure"
#define BCM_RO_NX_HEAP_HIGH_MEM                       "ro.nx.heap.highmem"
#define BCM_RO_NX_HEAP_DRV_MANAGED                    "ro.nx.heap.drv_managed"
#define BCM_RO_NX_HEAP_XRR                            "ro.nx.heap.export"
#define BCM_RO_NX_HEAP_GROW                           "ro.nx.heap.grow"

/* nexus dtu heap sizing. */
#define BCM_RO_NX_HEAP_DTU_PBUF0_SET                  "ro.nx.dtu.pbuf0.set"
#define BCM_RO_NX_HEAP_DTU_PBUF0_ADDR                 "ro.nx.dtu.pbuf0.addr"
#define BCM_RO_NX_HEAP_DTU_PBUF0_SIZE                 "ro.nx.dtu.pbuf0.size"
#define BCM_RO_NX_HEAP_DTU_PBUF1_SET                  "ro.nx.dtu.pbuf1.set"
#define BCM_RO_NX_HEAP_DTU_PBUF1_ADDR                 "ro.nx.dtu.pbuf1.addr"
#define BCM_RO_NX_HEAP_DTU_PBUF1_SIZE                 "ro.nx.dtu.pbuf1.size"
#define BCM_RO_NX_HEAP_DTU_SPBUF0_SET                 "ro.nx.dtu.spbuf0.set"
#define BCM_RO_NX_HEAP_DTU_SPBUF0_ADDR                "ro.nx.dtu.spbuf0.addr"
#define BCM_RO_NX_HEAP_DTU_SPBUF0_SIZE                "ro.nx.dtu.spbuf0.size"
#define BCM_RO_NX_HEAP_DTU_SPBUF1_SET                 "ro.nx.dtu.spbuf1.set"
#define BCM_RO_NX_HEAP_DTU_SPBUF1_ADDR                "ro.nx.dtu.spbuf1.addr"
#define BCM_RO_NX_HEAP_DTU_SPBUF1_SIZE                "ro.nx.dtu.spbuf1.size"
#define BCM_RO_NX_HEAP_DTU_USER_SET                   "ro.nx.dtu.user.set"
#define BCM_RO_NX_HEAP_DTU_USER_ADDR                  "ro.nx.dtu.user.addr"
#define BCM_RO_NX_HEAP_DTU_USER_SIZE                  "ro.nx.dtu.user.size"

/* begnine trimming config - not needed for ATV experience - default ENABLED. */
#define BCM_RO_NX_TRIM_VC1                            "ro.nx.trim.vc1"
#define BCM_RO_NX_TRIM_PIP                            "ro.nx.trim.pip"
#define BCM_RO_NX_TRIM_PIP_QR                         "ro.nx.trim.pip.qr"
#define BCM_RO_NX_TRIM_MOSAIC                         "ro.nx.trim.mosaic"
#define BCM_RO_NX_TRIM_STILLS                         "ro.nx.trim.stills"
#define BCM_RO_NX_TRIM_MINFMT                         "ro.nx.trim.minfmt"
#define BCM_RO_NX_TRIM_DISP                           "ro.nx.trim.disp"
#define BCM_RO_NX_TRIM_VIDIN                          "ro.nx.trim.vidin"
#define BCM_RO_NX_TRIM_MTG                            "ro.nx.trim.mtg"
#define BCM_RO_NX_TRIM_HDMIIN                         "ro.nx.trim.hdmiin"
#define BCM_RO_NX_TRIM_3D                             "ro.nx.trim.disp.3d"
/* destructive trimming config - feature set limitation - default DISABLED. */
#define BCM_RO_NX_TRIM_VP9                            "ro.nx.trim.vp9"
#define BCM_RO_NX_TRIM_4KDEC                          "ro.nx.trim.4kdec"
#define BCM_RO_NX_TRIM_10BCOL                         "ro.nx.trim.10bcol"
#define BCM_RO_NX_TRIM_D0HD                           "ro.nx.trim.d0hd"
#define BCM_RO_NX_TRIM_DEINT                          "ro.nx.trim.deint"
#define BCM_RO_NX_TRIM_CAP                            "ro.nx.trim.disp.cap"

#define BCM_DYN_NX_HD_OUT_HWC                         "dyn.nx.vidout.hwc"
#define BCM_DYN_NX_HD_OUT_SET                         "dyn.nx.vidout.set"

/* needs prefixing (persist.|ro.|dyn.). */
#define BCM_XX_NX_HD_OUT_FMT                          "nx.vidout.force"

#endif /* VENDOR_BCM_PROPS__NEXUS */
