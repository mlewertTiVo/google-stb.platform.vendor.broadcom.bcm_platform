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
#ifndef VENDOR_BCM_PROPS__MEDIA
#define VENDOR_BCM_PROPS__MEDIA

/* properties used by the media (codecs) subsystem. */

#define BCM_RO_MEDIA_ITB_DESC_DEBUG                      "ro.nx.media.venc_itb_desc_debug"
#define BCM_RO_MEDIA_INPUT_DEBUG                         "ro.nx.media.venc_input_debug"
#define BCM_RO_MEDIA_VDEC_PES_DEBUG                      "ro.nx.media.vdec_pes_debug"
#define BCM_RO_MEDIA_VDEC_INPUT_DEBUG                    "ro.nx.media.vdec_input_debug"
#define BCM_RO_MEDIA_VDEC_OUTPUT_DEBUG                   "ro.nx.media.vdec_output_debug"
#define BCM_RO_MEDIA_ENABLE_METADATA                     "ro.nx.media.vdec_enable_metadata"
#define BCM_RO_MEDIA_TUNNELED_HFRVIDEO                   "ro.nx.media.vdec_hfrvideo_tunnel"
#define BCM_RO_MEDIA_ENABLE_HDR_FOR_NON_VP9              "ro.nx.media.vdec_enable_hdr_for_non_vp9"
#define BCM_RO_MEDIA_EARLYDROP_THRESHOLD                 "ro.nx.media.stat.earlydrop_thres"
#define BCM_RO_MEDIA_DISABLE_RUNTIME_HEAPS               "ro.nx.rth.disable"
#define BCM_RO_MEDIA_VDEC_STATS_PROPERTY                 "ro.nx.media.vdec_stats.level"

#define BCM_RO_MEDIA_ADEC_PES_DEBUG                      "ro.nx.media.adec_pes_debug"
#define BCM_RO_MEDIA_ADEC_INPUT_DEBUG                    "ro.nx.media.adec_input_debug"
#define BCM_RO_MEDIA_ADEC_OUTPUT_DEBUG                   "ro.nx.media.adec_output_debug"
#define BCM_RO_MEDIA_MP3_DELAY                           "ro.nx.media.adec_mp3_delay"
#define BCM_RO_MEDIA_AAC_DELAY                           "ro.nx.media.adec_aac_delay"
#define BCM_RO_MEDIA_ADEC_ENABLED                        "ro.nx.media.adec_enabled"
#define BCM_RO_MEDIA_ADEC_DRC_MODE                       "ro.nx.media.adec_drc_mode"
#define BCM_RO_MEDIA_ADEC_DRC_MODE_DD                    "ro.nx.media.adec_drc_mode_dd"
#define BCM_RO_MEDIA_ADEC_DRC_REF_LEVEL                  "ro.nx.media.adec_drc_ref_level"
#define BCM_RO_MEDIA_ADEC_DRC_CUT                        "ro.nx.media.adec_drc_cut"
#define BCM_RO_MEDIA_ADEC_DRC_BOOST                      "ro.nx.media.adec_drc_boost"
#define BCM_RO_MEDIA_ADEC_DRC_ENC_LEVEL                  "ro.nx.media.adec_drc_enc_level"

#define BCM_DYN_MEDIA_VDEV_MAIN_VIRT                     "dyn.nx.media.vdec.main.virt"
#define BCM_DYN_MEDIA_PORT_RESET_ON_HWTEX                "dyn.nx.media.hwtex.reset_port"
#define BCM_DYN_MEDIA_LOG_MASK                           "dyn.nx.media.log.mask"

#endif /* VENDOR_BCM_PROPS__MEDIA */
