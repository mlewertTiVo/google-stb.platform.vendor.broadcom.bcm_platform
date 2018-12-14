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
#ifndef VENDOR_BCM_PROPS__AUDIO
#define VENDOR_BCM_PROPS__AUDIO

/* properties used by the audio subsystem */

#define BCM_RO_AUDIO_TUNNEL_PROPERTY_PES_DEBUG             "ro.nx.media.aout_t_pes_debug"
#define BCM_RO_AUDIO_OUTPUT_DEBUG                          "ro.nx.media.aout_debug"
#define BCM_RO_AUDIO_DISABLE_ATMOS                         "ro.nx.media.disable_atmos"
#define BCM_RO_AUDIO_OUTPUT_EAC3_TRANS_LATENCY             "ro.nx.eac3.trans_latency"
#define BCM_RO_AUDIO_OUTPUT_CLOCK_ACCURACY                 "ro.nx.audio.clock_acc"
#define BCM_RO_AUDIO_DIRECT_FORCE_PCM                      "ro.nx.media.direct_force_pcm"
#define BCM_RO_AUDIO_DOLBY_MS                              "ro.nx.dolby.ms"
#define BCM_RO_AUDIO_DISABLE_ATMOS                         "ro.nx.media.disable_atmos"
#define BCM_RO_AUDIO_OUTPUT_MIXER_LATENCY                  "ro.nx.audio.mixer_latency"
#define BCM_RO_AUDIO_DIRECT_DISABLE_AC3_PASSTHROUGH        "ro.nx.media.disable_ac3_passthru"
#define BCM_RO_AUDIO_DIRECT_DOLBY_DRC_MODE                 "ro.nx.media.direct_drc_mode"
#define BCM_RO_AUDIO_DIRECT_DOLBY_STEREO_DOWNMIX_MODE      "ro.nx.media.direct_stereo_mode"
#define BCM_RO_AUDIO_TUNNEL_NO_DEBOUNCE                    "ro.nx.media.no_debounce"
#define BCM_RO_AUDIO_SOFT_MUTING                           "ro.nx.media.soft_muting"
#define BCM_RO_AUDIO_SOFT_UNMUTING                         "ro.nx.media.soft_unmuting"
#define BCM_RO_AUDIO_SLEEP_AFTER_MUTE                      "ro.nx.media.sleep_after_mute"

#define BCM_PERSIST_AUDIO_DISABLE_ATMOS                    "persist.nx.disable_atmos"
#define BCM_PERSIST_AUDIO_DIRECT_FORCE_PCM                 "persist.nx.direct_force_pcm"

#endif /* VENDOR_BCM_PROPS__AUDIO */

