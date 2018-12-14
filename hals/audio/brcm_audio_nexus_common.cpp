/******************************************************************************
 *    (c)2015-2018 Broadcom Corporation
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

#include "brcm_audio.h"

using namespace android;

#define MAX_TRICKRATE_TRIES 10
#define MIN_FADE_DURATION 5

bool nexus_common_is_paused(NEXUS_SimpleAudioDecoderHandle simple_decoder) {
    NEXUS_AudioDecoderTrickState trickState;
    NEXUS_Error rc = NEXUS_SUCCESS;

    NEXUS_SimpleAudioDecoder_GetTrickState(simple_decoder, &trickState);
    return trickState.rate == 0;
}

NEXUS_Error nexus_common_set_volume(struct brcm_device *bdev,
                                    NEXUS_SimpleAudioDecoderHandle simple_decoder,
                                    unsigned level,
                                    unsigned *old_level,
                                    int duration,
                                    int sleep_after)
{
    NEXUS_AudioProcessorStatus processorStatus;
    NEXUS_Error rc = NEXUS_SUCCESS;

    if (bdev->dolby_ms == 12) {
        NEXUS_SimpleAudioDecoderSettings settings;
        NEXUS_SimpleAudioDecoder_GetSettings(simple_decoder, &settings);
        if (old_level)
            *old_level = settings.processorSettings[NEXUS_SimpleAudioDecoderSelector_ePrimary].fade.settings.level;
        settings.processorSettings[NEXUS_SimpleAudioDecoderSelector_ePrimary].fade.connected = true;
        settings.processorSettings[NEXUS_SimpleAudioDecoderSelector_ePrimary].fade.settings.level = level;
        settings.processorSettings[NEXUS_SimpleAudioDecoderSelector_ePrimary].fade.settings.duration =
            (duration < MIN_FADE_DURATION) ? MIN_FADE_DURATION : duration;
        ALOGV("%s: Setting fade level to: %d (%d ms)", __FUNCTION__, level,
            settings.processorSettings[NEXUS_SimpleAudioDecoderSelector_ePrimary].fade.settings.duration);
        rc = NEXUS_SimpleAudioDecoder_SetSettings(simple_decoder, &settings);

        if (rc != NEXUS_SUCCESS) {
            ALOGE("%s: Error setting volume %u", __FUNCTION__, rc);
            return rc;
        }

        if (sleep_after >= 0) {
            rc = NEXUS_SimpleAudioDecoder_GetProcessorStatus(simple_decoder,
                     NEXUS_SimpleAudioDecoderSelector_ePrimary,
                     NEXUS_AudioPostProcessing_eFade, &processorStatus);
            for (int i = ((duration / 10) + 1); i > 0; i--) {
                if (rc != NEXUS_SUCCESS)
                    break;
                if (processorStatus.status.fade.level == 0)
                    break;

                ALOGVV("%s: %d, active %lu, remain %lu, lvl %d%%", __FUNCTION__, i,
                           (unsigned long)processorStatus.status.fade.active,
                           (unsigned long)processorStatus.status.fade.remaining,
                           (int)processorStatus.status.fade.level);
                usleep(10 * 1000);

                rc = NEXUS_SimpleAudioDecoder_GetProcessorStatus(simple_decoder,
                         NEXUS_SimpleAudioDecoderSelector_ePrimary,
                         NEXUS_AudioPostProcessing_eFade, &processorStatus);
            }
            if (sleep_after > 0)
                usleep(sleep_after * 1000);
            ALOGV("%s fade level %d%%", __FUNCTION__, processorStatus.status.fade.level);
        }
    } else {
        float vol = (float)level / 100.0;

        ALOGV("%s: No dolby MS support, changing master volume", __FUNCTION__);
        if (old_level) {
            float old_vol = brcm_audio_get_master_volume();
            *old_level = (unsigned)(old_vol * 100);
        }
        brcm_audio_set_master_volume(vol);
        // Netflix requirement: mute passthrough when volume is 0
        brcm_audio_set_mute_state(level == 0.0);

        if (sleep_after > 0)
            usleep(sleep_after * 1000);
        return rc;
    }
    return rc;
}

NEXUS_Error nexus_common_mute_and_pause(struct brcm_device *bdev,
                                        NEXUS_SimpleAudioDecoderHandle simple_decoder,
                                        NEXUS_SimpleStcChannelHandle stc_channel,
                                        int mute_duration,
                                        int sleep_after_mute)
{
    NEXUS_AudioDecoderTrickState trickState;
    NEXUS_Error rc = NEXUS_SUCCESS;
    unsigned old_level = 0;
    int tries = MAX_TRICKRATE_TRIES;

    NEXUS_SimpleAudioDecoder_GetTrickState(simple_decoder, &trickState);
    if (trickState.rate == 0) {
        ALOGV("%s, %p already paused", __FUNCTION__, simple_decoder);
        return NEXUS_SUCCESS;
    }

    if (mute_duration >= 0) {
        nexus_common_set_volume(bdev, simple_decoder, 0, &old_level, mute_duration, sleep_after_mute);
    }

    trickState.rate = 0;
    ALOGV("%s, %p pause decoder", __FUNCTION__, simple_decoder);
    do {
        rc = NEXUS_SimpleAudioDecoder_SetTrickState(simple_decoder, &trickState);
        if (rc != NEXUS_SUCCESS) {
            ALOGE("%s: Error pausing audio decoder %u", __FUNCTION__, rc);
            if (rc == NEXUS_TIMEOUT) {
                tries--;
                if (tries == 0) {
                    ALOGE("%s: Giving up after %d tries", __FUNCTION__, MAX_TRICKRATE_TRIES);

                    ALOGV("%s: Restoring volume to: %d", __FUNCTION__, old_level);
                    nexus_common_set_volume(bdev, simple_decoder, old_level, NULL, -1, -1);
                }
            }
        }
    } while (rc == NEXUS_TIMEOUT);
    if (stc_channel) {
        rc = NEXUS_SimpleStcChannel_Freeze(stc_channel, true);
        if (rc != NEXUS_SUCCESS) {
            ALOGE("%s: Error pausing STC %u", __FUNCTION__, rc);
            trickState.rate = NEXUS_NORMAL_DECODE_RATE;
            NEXUS_SimpleAudioDecoder_SetTrickState(simple_decoder, &trickState);

            if (bdev->dolby_ms == 12) {
                ALOGV("%s: Restoring fade level to: %d", __FUNCTION__, old_level);
                nexus_common_set_volume(bdev, simple_decoder, old_level, NULL, -1, -1);
            }
        }
    }
    return rc;
}

NEXUS_Error nexus_common_resume_and_unmute(struct brcm_device *bdev,
                                           NEXUS_SimpleAudioDecoderHandle simple_decoder,
                                           NEXUS_SimpleStcChannelHandle stc_channel,
                                           int unmute_duration,
                                           unsigned level)
{
    NEXUS_AudioDecoderTrickState trickState;
    NEXUS_Error rc = NEXUS_SUCCESS;
    int tries = MAX_TRICKRATE_TRIES;

    NEXUS_SimpleAudioDecoder_GetTrickState(simple_decoder, &trickState);
    if (trickState.rate != 0) {
        ALOGV("%s, %p already resumed", __FUNCTION__, simple_decoder);
        return NEXUS_SUCCESS;
    }

    do {
        trickState.rate = NEXUS_NORMAL_DECODE_RATE;
        rc = NEXUS_SimpleAudioDecoder_SetTrickState(simple_decoder, &trickState);
        if (rc == NEXUS_TIMEOUT) {
            ALOGE("%s: Error resuming audio decoder %u", __FUNCTION__, rc);
            tries--;
            if (tries == 0) {
                ALOGE("%s: Giving up after %d tries", __FUNCTION__, MAX_TRICKRATE_TRIES);
                return rc;
            }
        }
    } while (rc == NEXUS_TIMEOUT);

    if (rc != NEXUS_SUCCESS) {
        ALOGE("%s: Error resuming audio decoder %u", __FUNCTION__, rc);
        return rc;
    }

    if (stc_channel) {
        rc = NEXUS_SimpleStcChannel_Freeze(stc_channel, false);
        if (rc != NEXUS_SUCCESS) {
            ALOGE("%s: Error resuming STC %u", __FUNCTION__, rc);

            trickState.rate = 0;
            NEXUS_SimpleAudioDecoder_SetTrickState(simple_decoder, &trickState);
            return rc;
        }
    }

    if (unmute_duration >= 0) {
        nexus_common_set_volume(bdev, simple_decoder, level, NULL, unmute_duration, -1);
        ALOGV("%s unmuted %d", __FUNCTION__, rc);
    }
    return NEXUS_SUCCESS;
}

