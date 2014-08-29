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
#define LOG_TAG "BrcmAudioDummy"

#if DUMMY_AUDIO_OUT

#define DUMMY_OUT_DEFAULT_SAMPLE_RATE   44100
#define DUMMY_OUT_DEFAULT_CHANNELS      AUDIO_CHANNEL_OUT_STEREO
#define DUMMY_OUT_DEFAULT_FORMAT        AUDIO_FORMAT_PCM_16_BIT
#define DUMMY_OUT_DEFAULT_BUFFER_SIZE   8192
#define DUMMY_OUT_DEFAULT_DEVICE_NAME   "/dev/null"

/*
 * Operation Functions
 */

static int dummy_bout_set_volume(struct brcm_stream_out *bout,
                                 float left, float right)
{
    UNUSED(bout);
    UNUSED(left);
    UNUSED(right);

    return 0;
}

static int dummy_bout_start(struct brcm_stream_out *bout)
{
    int fd = bout->dummy.fd;

    if (fd < 0) {
        LOGE("%s: at %d, device not open\n",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    return 0;
}

static int dummy_bout_stop(struct brcm_stream_out *bout)
{
    UNUSED(bout);

    return 0;
}

static int dummy_bout_write(struct brcm_stream_out *bout,
                            const void *buffer, size_t bytes)
{
    int fd = bout->dummy.fd;

    if (fd < 0) {
        LOGE("%s: at %d, device not open\n",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    return write(fd, buffer, bytes);
}

static int dummy_bout_open(struct brcm_stream_out *bout)
{
    struct audio_config *config = &bout->config;
    int fd;

    /* Only allow default config */
    config->sample_rate = DUMMY_OUT_DEFAULT_SAMPLE_RATE;
    config->channel_mask = DUMMY_OUT_DEFAULT_CHANNELS;
    config->format = DUMMY_OUT_DEFAULT_FORMAT;
    bout->buffer_size = DUMMY_OUT_DEFAULT_BUFFER_SIZE;

    /* Open output device */
    fd = open(DUMMY_OUT_DEFAULT_DEVICE_NAME, O_RDWR);
    if (fd < 0) {
        LOGE("%s: at %d, device open failed\n",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    bout->dummy.fd = fd;
    return 0;
}

static int dummy_bout_close(struct brcm_stream_out *bout)
{
    int fd = bout->dummy.fd;

    dummy_bout_stop(bout);

    if (fd >= 0) {
        close(fd);
    }
    return 0;
}

struct brcm_stream_out_ops dummy_bout_ops = {
    .do_bout_open = dummy_bout_open,
    .do_bout_close = dummy_bout_close,
    .do_bout_start = dummy_bout_start,
    .do_bout_stop = dummy_bout_stop,
    .do_bout_write = dummy_bout_write,
    .do_bout_set_volume = dummy_bout_set_volume,
    .do_bout_dump = NULL
};

#endif /* DUMMY_AUDIO_OUT */


#if DUMMY_AUDIO_IN

#define DUMMY_IN_DEFAULT_SAMPLE_RATE    8000
#define DUMMY_IN_DEFAULT_CHANNELS       AUDIO_CHANNEL_IN_MONO
#define DUMMY_IN_DEFAULT_FORMAT         AUDIO_FORMAT_PCM_16_BIT
#define DUMMY_IN_DEFAULT_DEVICE_NAME    "/dev/zero"

/*
 * Operation Functions
 */

static int dummy_bin_start(struct brcm_stream_in *bin)
{
    int fd = bin->dummy.fd;

    if (fd < 0) {
        LOGE("%s: at %d, device not open\n",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    return 0;
}

static int dummy_bin_stop(struct brcm_stream_in *bin)
{
    UNUSED(bin);
    return 0;
}

static int dummy_bin_read(struct brcm_stream_in *bin,
                          void *buffer, size_t bytes)
{
    int fd = bin->dummy.fd;

    if (fd < 0) {
        LOGE("%s: at %d, device not open\n",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    return read(fd, buffer, bytes);
}

static int dummy_bin_open(struct brcm_stream_in *bin)
{
    struct audio_config *config = &bin->config;
    int fd;

    /* Only allow default config */
    config->sample_rate = DUMMY_IN_DEFAULT_SAMPLE_RATE;
    config->channel_mask = DUMMY_IN_DEFAULT_CHANNELS;
    config->format = DUMMY_IN_DEFAULT_FORMAT;

    /* Open input device */
    fd = open(DUMMY_IN_DEFAULT_DEVICE_NAME, O_RDWR);
    if (fd < 0) {
        LOGE("%s: at %d, device open failed\n",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    bin->dummy.fd = fd;
    return 0;
}

static int dummy_bin_close(struct brcm_stream_in *bin)
{
    int fd = bin->dummy.fd;

    dummy_bin_stop(bin);

    if (fd >= 0) {
        close(fd);
    }
    return 0;
}

struct brcm_stream_in_ops dummy_bin_ops = {
    .do_bin_open = dummy_bin_open,
    .do_bin_close = dummy_bin_close,
    .do_bin_start = dummy_bin_start,
    .do_bin_stop = dummy_bin_stop,
    .do_bin_read = dummy_bin_read,
    .do_bin_dump = NULL
};

#endif /* DUMMY_AUDIO_IN */
