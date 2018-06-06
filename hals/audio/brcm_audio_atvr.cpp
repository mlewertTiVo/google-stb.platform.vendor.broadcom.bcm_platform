/******************************************************************************
 *    (c)2014-2017 Broadcom Corporation
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
#include "AudioHardwareInput.h"
#include "AudioStreamIn.h"
#include <media/AudioParameter.h>

#undef LOG_TAG
#define LOG_TAG "BrcmAudioAtvr"

// Constants for voice input from fugu-compatible remote controls 
#define ATVR_IN_DEFAULT_SAMPLE_RATE    8000
#define ATVR_IN_DEFAULT_CHANNELS       AUDIO_CHANNEL_IN_MONO
#define ATVR_IN_DEFAULT_FORMAT         AUDIO_FORMAT_PCM_16_BIT
#define ATVR_IN_DEFAULT_DURATION       100 /* ms */

using namespace android;

/*
 * Operation Functions
 */

static int atvr_bin_start(struct brcm_stream_in *bin)
{
    UNUSED(bin);
    return 0;
}

static int atvr_bin_stop(struct brcm_stream_in *bin)
{
    UNUSED(bin);
    return 0;
}

static int atvr_bin_read(struct brcm_stream_in *bin,
                          void *buffer, size_t bytes)
{
    return bin->atvr.impl->read(buffer, bytes);
}

static int atvr_bin_open(struct brcm_stream_in *bin)
{
    struct audio_config *config = &bin->config;
    status_t ret;

    /* Only allow default config */
    config->sample_rate = ATVR_IN_DEFAULT_SAMPLE_RATE;
    config->channel_mask = ATVR_IN_DEFAULT_CHANNELS;
    config->format = ATVR_IN_DEFAULT_FORMAT;

    bin->buffer_size = get_brcm_audio_buffer_size(config->sample_rate,
                                                  config->format,
                                                  popcount(config->channel_mask),
                                                  ATVR_IN_DEFAULT_DURATION);

    bin->atvr.impl = bin->bdev->input->openInputStream(bin->devices,
                                                       &config->format,
                                                       &config->channel_mask,
                                                       &config->sample_rate,
                                                       &ret);
    if (bin->atvr.impl == NULL) {
        ret = -ENOMEM;
    } else if (ret != 0) {
        delete bin->atvr.impl;
    } else {
        /* Configure fixed input source parameter */
        AudioParameter param;
        param.addInt(String8(AudioParameter::keyInputSource), AUDIO_SOURCE_VOICE_RECOGNITION);
        bin->atvr.impl->setParameters(&bin->ain.common, param.toString());
    }

    return (int) ret;
}

static int atvr_bin_close(struct brcm_stream_in *bin)
{
    atvr_bin_stop(bin);

    bin->bdev->input->closeInputStream(bin->atvr.impl);
    return 0;
}

struct brcm_stream_in_ops atvr_bin_ops = {
    .do_bin_open = atvr_bin_open,
    .do_bin_close = atvr_bin_close,
    .do_bin_start = atvr_bin_start,
    .do_bin_stop = atvr_bin_stop,
    .do_bin_read = atvr_bin_read,
    .do_bin_dump = NULL
};

