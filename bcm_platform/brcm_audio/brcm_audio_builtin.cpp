/******************************************************************************
 *    (c)2014 Broadcom Corporation
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
 * $brcm_Workfile: AudioHardware.cpp $
 * $brcm_Revision:  $
 * $brcm_Date: 08/08/14 12:05p $
 * $brcm_Author: zhang@broadcom.com
 *
 * Module Description:
 *
 * Revision History:
 *
 * $brcm_Log:  $
 *
 *****************************************************************************/

#include "brcm_audio.h"

#undef LOG_TAG
#define LOG_TAG "BrcmAudioBuiltin"

#define BUILTIN_IN_DEFAULT_SAMPLE_RATE  8000
#define BUILTIN_IN_DEFAULT_CHANNELS     AUDIO_CHANNEL_IN_MONO
#define BUILTIN_IN_DEFAULT_FORMAT       AUDIO_FORMAT_PCM_16_BIT
#define BUILTIN_IN_DEFAULT_FRAGMENT     (0x7fff0006) /* no limitation, 2^6 buffer size*/
#define BUILTIN_IN_DEFAULT_DEVICE_NAME  "/dev/snd/dsp"

/* Supported stream in sample rate */
const static uint32_t builtin_in_sample_rates[] = {
    8000,
    11025,
    12000,
    16000,
    22050,
    24000,
    32000,
    44100,
    48000
};
#define BUILTIN_IN_SAMPLE_RATE_COUNT                        \
    (sizeof(builtin_in_sample_rates) / sizeof(uint32_t))

/*
 * Operation Functions
 */

static int builtin_bin_start(struct brcm_stream_in *bin)
{
    int fd = bin->builtin.fd;
    int fmt, channels, speed, fragment;
    int ret = 0;

    if (fd < 0) {
        LOGE("%s: at %d, device not open\n",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    ret = ioctl(fd, SNDCTL_DSP_RESET, &fmt);
    if (ret) {
        LOGE("%s: at %d, device reset failed, ret = %d\n",
             __FUNCTION__, __LINE__, ret);
        return -ENOSYS;
    }

    fmt = (bin->config.format == AUDIO_FORMAT_PCM_16_BIT) ?
        AFMT_S16_LE : AFMT_S8;
    ret = ioctl(fd, SNDCTL_DSP_SETFMT, &fmt);
    if (ret) {
        LOGE("%s: at %d, set format failed, ret = %d\n",
             __FUNCTION__, __LINE__, ret);
        return -ENOSYS;
    }

    channels = (bin->config.channel_mask == AUDIO_CHANNEL_IN_STEREO) ?
        1 : 0;
    ret = ioctl(fd, SNDCTL_DSP_CHANNELS, &channels);
    if (ret) {
        LOGE("%s: at %d, set channels failed, ret = %d\n",
             __FUNCTION__, __LINE__, ret);
        return -ENOSYS;
    }

    speed = bin->config.sample_rate;
    ret = ioctl(fd, SNDCTL_DSP_SPEED, &speed);
    if (ret) {
        LOGE("%s: at %d, set speed failed, ret = %d\n",
             __FUNCTION__, __LINE__, ret);
        return -ENOSYS;
    }

    fragment = bin->builtin.fragment;
    ret = ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &fragment);
    if (ret) {
        LOGE("%s: at %d, set fragment failed, ret = %d\n",
             __FUNCTION__, __LINE__, ret);
        return -ENOSYS;
    }

    return 0;
}

static int builtin_bin_stop(struct brcm_stream_in *bin)
{
    UNUSED(bin);
    return 0;
}

static int builtin_bin_read(struct brcm_stream_in *bin,
                            void *buffer, size_t bytes)
{
    int fd = bin->builtin.fd;
    int bytes_read = 0;
    int ret = 0;

    if (fd < 0) {
        LOGE("%s: at %d, device not open\n",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    while (bytes > 0) {
        ret = read(fd, (void *)((int)buffer + bytes_read), bytes);
        if (ret < 0) {
            if (errno != EAGAIN) {
                LOGE("%s: at %d, device read failed, ret = %d\n",
                     __FUNCTION__, __LINE__, ret);
                break;
            }
            else {
                BKNI_Sleep(10);
            }
        }

        bytes -= ret;
        bytes_read += ret;
    }

    /* Return error if no bytes read */
    if (bytes_read == 0) {
        return ret;
    }
    return bytes_read;
}

static int builtin_bin_open(struct brcm_stream_in *bin)
{
    struct audio_config *config = &bin->config;
    int fd;
    int i;

    /* Check if config is supported */
    for (i = 0; i < (int)BUILTIN_IN_SAMPLE_RATE_COUNT; i++) {
        if (config->sample_rate == builtin_in_sample_rates[i]) {
            break;
        }
    }
    if (i == BUILTIN_IN_SAMPLE_RATE_COUNT) {
        config->sample_rate = BUILTIN_IN_DEFAULT_SAMPLE_RATE;
    }
    if (config->channel_mask != AUDIO_CHANNEL_IN_STEREO &&
        config->channel_mask != AUDIO_CHANNEL_IN_MONO) {
        config->channel_mask = BUILTIN_IN_DEFAULT_CHANNELS;
    }
    if (config->format != AUDIO_FORMAT_PCM_16_BIT &&
        config->format != AUDIO_FORMAT_PCM_8_BIT) {
        config->format = BUILTIN_IN_DEFAULT_FORMAT;
    }

    bin->buffer_size =
        brcm_audio_input_buffer_size(config->sample_rate,
                                     config->format,
                                     popcount(config->channel_mask));

    /* Open input device */
    fd = open(BUILTIN_IN_DEFAULT_DEVICE_NAME, O_RDWR);
    if (fd < 0) {
        LOGE("%s: at %d, device open failed\n",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    bin->builtin.fd = fd;
    bin->builtin.fragment = BUILTIN_IN_DEFAULT_FRAGMENT;

    return 0;
}

static int builtin_bin_close(struct brcm_stream_in *bin)
{
    int fd = bin->builtin.fd;

    builtin_bin_stop(bin);

    if (fd >= 0) {
        close(fd);
    }
    return 0;
}

static int builtin_bin_dump(struct brcm_stream_in *bin, int fd)
{
    dprintf(fd, "\nbuiltin_bin_dump:\n"
            "\tfragment = %d\n"
            "\tfd = %d\n",
            bin->builtin.fragment,
            bin->builtin.fd);

    return 0;
}

struct brcm_stream_in_ops builtin_bin_ops = {
    .do_bin_open = builtin_bin_open,
    .do_bin_close = builtin_bin_close,
    .do_bin_start = builtin_bin_start,
    .do_bin_stop = builtin_bin_stop,
    .do_bin_read = builtin_bin_read,
    .do_bin_dump = builtin_bin_dump
};
