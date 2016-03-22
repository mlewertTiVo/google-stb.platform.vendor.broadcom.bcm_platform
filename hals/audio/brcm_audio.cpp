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
 *****************************************************************************/

#define LOG_TAG "BrcmAudio"

#include "brcm_audio.h"

#define BRCM_BUFFER_SIZE_MS 10

/*
 * Utility Functions
 */

size_t get_brcm_audio_buffer_size(unsigned int sample_rate,
                                  audio_format_t format,
                                  unsigned int channel_count)
{
    size_t size;

    if (sample_rate < 8000 ||
        sample_rate > 192000) {
        ALOGW("%s: at %d, bad sampling rate %d\n",
             __FUNCTION__, __LINE__, sample_rate);
        return 0;
    }
    if (format != AUDIO_FORMAT_PCM_16_BIT &&
        format != AUDIO_FORMAT_PCM_8_BIT) {
        ALOGW("%s: %d, bad format %d\n",
             __FUNCTION__, __LINE__, format);
        return 0;
    }
    if (channel_count != 1 &&
        channel_count != 2) {
        ALOGW("%s: %d, bad channel count %d",
             __FUNCTION__, __LINE__, channel_count);
        return 0;
    }

    size = (sample_rate * BRCM_BUFFER_SIZE_MS + 999) / 1000;

    return size * channel_count * audio_bytes_per_sample(format);
}

static brcm_devices_out_t get_brcm_devices_out(audio_devices_t devices)
{
    switch (devices) {
    case AUDIO_DEVICE_OUT_SPEAKER:
        return BRCM_DEVICE_OUT_NEXUS;
    case AUDIO_DEVICE_OUT_AUX_DIGITAL:
        return BRCM_DEVICE_OUT_NEXUS_DIRECT;
    default:
        return BRCM_DEVICE_OUT_MAX;
    }
}

static brcm_devices_in_t get_brcm_devices_in(audio_devices_t devices)
{
    switch (devices) {
    case AUDIO_DEVICE_IN_BUILTIN_MIC:
        return BRCM_DEVICE_IN_BUILTIN;
    default:
        return BRCM_DEVICE_IN_MAX;
    }
}

/*
 * Stream Out Functions
 */

static uint32_t bout_get_sample_rate(const struct audio_stream *stream)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)stream;

    ALOGV("%s: at %d, stream = 0x%X, sample_rate = %d\n",
         __FUNCTION__, __LINE__, (uint32_t)stream, bout->config.sample_rate);

    return bout->config.sample_rate;
}

static int bout_set_sample_rate(struct audio_stream *stream,
                                uint32_t sample_rate)
{
    UNUSED(stream);

    ALOGV("%s: at %d, stream = 0x%X, sample_rate = %d\n",
         __FUNCTION__, __LINE__, (uint32_t)stream, sample_rate);

    return -ENOSYS;
}

static size_t bout_get_buffer_size(const struct audio_stream *stream)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)stream;

    ALOGV("%s: at %d, stream = 0x%X, buffer_size = %d\n",
         __FUNCTION__, __LINE__, (uint32_t)stream, bout->buffer_size);

    return bout->buffer_size;
}

static audio_channel_mask_t bout_get_channels(const struct audio_stream *stream)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)stream;

    ALOGV("%s: at %d, stream = 0x%X, channels = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)stream, bout->config.channel_mask);

    return bout->config.channel_mask;
}

static audio_format_t bout_get_format(const struct audio_stream *stream)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)stream;

    ALOGV("%s: at %d, stream = 0x%X, format = %d\n",
         __FUNCTION__, __LINE__, (uint32_t)stream, bout->config.format);

    return bout->config.format;
}

static int bout_set_format(struct audio_stream *stream,
                           audio_format_t format)
{
    UNUSED(stream);

    ALOGV("%s: at %d, stream = 0x%X, format = %d\n",
         __FUNCTION__, __LINE__, (uint32_t)stream, format);

    return -ENOSYS;
}

static int bout_standby_l(struct audio_stream *stream)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)stream;
    int ret = 0;

    ALOGV("%s: at %d, stream = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)stream);

    if (bout->started) {
        ret = bout->ops.do_bout_stop(bout);
        bout->started = false;
    }

    return ret;
}

static int bout_standby(struct audio_stream *stream)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)stream;
    int ret;

    pthread_mutex_lock(&bout->lock);
    ret = bout_standby_l(stream);
    pthread_mutex_unlock(&bout->lock);

    return ret;
}

static int bout_dump(const struct audio_stream *stream, int fd)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)stream;

    dprintf(fd, "\nbout_dump:\n"
            "\tdevices = 0x%X\n"
            "\tsample_rate = %d\n"
            "\tchannel_mask = 0x%X\n"
            "\tformat = %d\n"
            "\tbuffer_size = %d\n"
            "\tstarted: %s\n"
            "\tsuspended: %s\n",
            bout->devices,
            bout->config.sample_rate,
            bout->config.channel_mask,
            bout->config.format,
            bout->buffer_size,
            bout->started ? "true": "false",
            bout->suspended ? "true": "false");

    if (bout->ops.do_bout_dump) {
        bout->ops.do_bout_dump(bout, fd);
    }

    return 0;
}

static int bout_set_parameters(struct audio_stream *stream,
                               const char *kvpairs)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)stream;
    struct str_parms *parms;
    int ret = 0;

    pthread_mutex_lock(&bout->lock);

    ALOGV("%s: at %d, stream = 0x%X, kvpairs=\"%s\"\n",
         __FUNCTION__, __LINE__, (uint32_t)stream, kvpairs);

    parms = str_parms_create_str(kvpairs);

    /* Set parameters here !!! */
    char value[8];
    const char STANDBY_KEY[] = "screen_state";

    if (str_parms_has_key(parms, STANDBY_KEY)) {
        ret = str_parms_get_str(parms, STANDBY_KEY, value, sizeof(value)/sizeof(value[0]));
        if (ret > 0) {
            if (strcmp(value, "off") == 0) {
                ALOGV("%s: Need to enter power saving mode...", __FUNCTION__);
                ret = bout_standby_l(stream);
                bout->suspended = true;
            }
            else if (strcmp(value, "on") == 0) {
                ALOGV("%s: Need to exit power saving mode...", __FUNCTION__);
                bout->suspended = false;
            }
        }
    }

    str_parms_destroy(parms);
    pthread_mutex_unlock(&bout->lock);
    return ret;
}

static char *bout_get_parameters(const struct audio_stream *stream,
                                 const char *keys)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)stream;

    ALOGV("%s: at %d, stream = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)stream);

    if (bout->ops.do_bout_get_parameters) {
        return bout->ops.do_bout_get_parameters(bout, keys);
    }

    return strdup("");
}

static int bout_add_audio_effect(const struct audio_stream *stream,
                                 effect_handle_t effect)
{
    UNUSED(stream);
    UNUSED(effect);

    ALOGV("%s: at %d, stream = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)stream);

    return 0;
}

static int bout_remove_audio_effect(const struct audio_stream *stream,
                                    effect_handle_t effect)
{
    UNUSED(stream);
    UNUSED(effect);

    ALOGV("%s: at %d, stream = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)stream);

    return 0;
}

static uint32_t bout_get_latency(const struct audio_stream_out *aout)
{
    UNUSED(aout);

    ALOGV("%s: at %d, stream = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)aout);

    return 0;
}

static int bout_set_volume(struct audio_stream_out *aout,
                           float left, float right)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)aout;
    int ret = 0;

    ALOGV("%s: at %d, stream = 0x%X, left = %f, right = %f\n",
         __FUNCTION__, __LINE__, (uint32_t)aout, left, right);

    pthread_mutex_lock(&bout->lock);
    ret = bout->ops.do_bout_set_volume(bout, left, right);
    pthread_mutex_unlock(&bout->lock);

    return ret;
}

static ssize_t bout_write(struct audio_stream_out *aout,
                          const void *buffer, size_t bytes)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)aout;
    size_t bytes_written;
    int ret = 0;

    pthread_mutex_lock(&bout->lock);

     if (!bout->started) {
        if (!bout->suspended) {
            ret = bout->ops.do_bout_start(bout);
            if (ret) {
                ALOGE("%s: at %d, start failed\n",
                     __FUNCTION__, __LINE__);
                pthread_mutex_unlock(&bout->lock); // have to unlock, otherwise deadlock
                return ret;
            }
            bout->started = true;
        }
        else {
            pthread_mutex_unlock(&bout->lock);
            return ret;
        }
    }

    bytes_written = bout->ops.do_bout_write(bout, buffer, bytes);

    pthread_mutex_unlock(&bout->lock);

    ALOGVV("%s: at %d, stream = 0x%X, bytes = %d, bytes_written = %d",
          __FUNCTION__, __LINE__, (uint32_t)aout, bytes, bytes_written);

    return bytes_written;
}

static int bout_get_render_position(const struct audio_stream_out *aout,
                                    uint32_t *dsp_frames)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)aout;
    int ret;

    pthread_mutex_lock(&bout->lock);
    ret = bout->ops.do_bout_get_render_position(bout, dsp_frames);
    ALOGV("%s: stream:%p, frames:%u",__FUNCTION__, aout, *dsp_frames);
    pthread_mutex_unlock(&bout->lock);
    return ret;
}

static int bout_get_next_write_timestamp(const struct audio_stream_out *aout,
                                         int64_t *timestamp)
{
    UNUSED(aout);

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    *timestamp = ts.tv_sec * 1000000ll + (ts.tv_nsec)/1000ll;

    ALOGV("%s: at %d, stream  =  0x%X, Next timestamp..(%lld)",
         __FUNCTION__, __LINE__, (uint32_t)aout, *timestamp);

    return 0;
}

static int bout_get_presentation_position(const struct audio_stream_out *aout,
                                         uint64_t *frames, struct timespec *timestamp)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)aout;
    int ret = 0;

    pthread_mutex_lock(&bout->lock);
    ret = bout->ops.do_bout_get_presentation_position(bout, frames);
    if (ret) {
        ALOGE("%s: at %d, get position failed",
             __FUNCTION__, __LINE__);
        pthread_mutex_unlock(&bout->lock);
        return ret;
    }

    pthread_mutex_unlock(&bout->lock);

    clock_gettime(CLOCK_MONOTONIC, timestamp);
    ALOGV("%s: frames:%lld, timestamp(%lld)",__FUNCTION__, *frames,
                                (timestamp->tv_sec* 1000000ll) + (timestamp->tv_nsec)/1000ll);

    return ret;
}



/*
 * Stream In Functions
 */

static uint32_t bin_get_sample_rate(const struct audio_stream *stream)
{
    struct brcm_stream_in *bin = (struct brcm_stream_in *)stream;

    ALOGV("%s: at %d, stream = 0x%X, sample_rate = %d\n",
         __FUNCTION__, __LINE__, (uint32_t)stream, bin->config.sample_rate);

    return bin->config.sample_rate;
}

static int bin_set_sample_rate(struct audio_stream *stream,
                               uint32_t sample_rate)
{
    UNUSED(stream);

    ALOGV("%s: at %d, stream = 0x%X, sample_rate = %d\n",
         __FUNCTION__, __LINE__, (uint32_t)stream, sample_rate);

    return -ENOSYS;
}

static size_t bin_get_buffer_size(const struct audio_stream *stream)
{
    struct brcm_stream_in *bin = (struct brcm_stream_in *)stream;

    ALOGV("%s: at %d, stream = 0x%X, buffer_size = %d\n",
         __FUNCTION__, __LINE__, (uint32_t)stream, bin->buffer_size);

    return bin->buffer_size;
}

static audio_channel_mask_t bin_get_channels(const struct audio_stream *stream)
{
    struct brcm_stream_in *bin = (struct brcm_stream_in *)stream;

    ALOGV("%s: at %d, stream = 0x%X, chennels = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)stream, bin->config.channel_mask);

    return bin->config.channel_mask;
}

static audio_format_t bin_get_format(const struct audio_stream *stream)
{
    struct brcm_stream_in *bin = (struct brcm_stream_in *)stream;

    ALOGV("%s: at %d, stream = 0x%X, format = %d\n",
         __FUNCTION__, __LINE__, (uint32_t)stream, bin->config.format);

    return bin->config.format;
}

static int bin_set_format(struct audio_stream *stream,
                          audio_format_t format)
{
    UNUSED(stream);

    ALOGV("%s: at %d, stream = 0x%X, format = %d\n",
         __FUNCTION__, __LINE__, (uint32_t)stream, format);

    return -ENOSYS;
}

static int bin_standby(struct audio_stream *stream)
{
    struct brcm_stream_in *bin = (struct brcm_stream_in *)stream;
    int ret = 0;

    ALOGV("%s: at %d, stream = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)stream);

    pthread_mutex_lock(&bin->lock);

    if (bin->started) {
        ret = bin->ops.do_bin_stop(bin);
        bin->started = false;
    }

    pthread_mutex_unlock(&bin->lock);

    return ret;
}

static int bin_dump(const struct audio_stream *stream, int fd)
{
    struct brcm_stream_in *bin = (struct brcm_stream_in *)stream;

    dprintf(fd, "\nbin_dump:\n"
            "\tdevices = 0x%X\n"
            "\tsample_rate = %d\n"
            "\tchannel_mask = 0x%X\n"
            "\tformat = %d\n"
            "\tbuffer_size = %d\n"
            "\tstarted: %s\n"
            "\tsuspended: %s\n",
            bin->devices,
            bin->config.sample_rate,
            bin->config.channel_mask,
            bin->config.format,
            bin->buffer_size,
            bin->started ? "true": "false",
            bin->suspended ? "true": "false");

    if (bin->ops.do_bin_dump) {
        bin->ops.do_bin_dump(bin, fd);
    }

    return 0;
}

static int bin_set_parameters(struct audio_stream *stream,
                              const char *kvpairs)
{
    struct brcm_stream_in *bin = (struct brcm_stream_in *)stream;
    struct str_parms *parms;
    int ret = 0;

    ALOGV("%s: at %d, stream = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)stream);

    parms = str_parms_create_str(kvpairs);

    /* Set parameters here !!! */

    str_parms_destroy(parms);
    return ret;
}

static char *bin_get_parameters(const struct audio_stream *stream,
                                const char *keys)
{
    struct brcm_stream_in *bin = (struct brcm_stream_in *)stream;
    struct str_parms *query;
    struct str_parms *reply;
    char *str;

    ALOGV("%s: at %d, stream = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)stream);

    query = str_parms_create_str(keys);
    reply = str_parms_create();

    /* Get parameter here !!! */

    str = str_parms_to_str(reply);

    str_parms_destroy(query);
    str_parms_destroy(reply);
    return str;
}

static int bin_add_audio_effect(const struct audio_stream *stream,
                                effect_handle_t effect)
{
    UNUSED(stream);
    UNUSED(effect);

    ALOGV("%s: at %d, stream = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)stream);

    return 0;
}

static int bin_remove_audio_effect(const struct audio_stream *stream,
                                   effect_handle_t effect)
{
    UNUSED(stream);
    UNUSED(effect);

    ALOGV("%s: at %d, stream = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)stream);

    return 0;
}

static int bin_set_gain(struct audio_stream_in *ain, float gain)
{
    UNUSED(ain);

    ALOGV("%s: at %d, stream = 0x%X, gain = %f\n",
         __FUNCTION__, __LINE__, (uint32_t)ain, gain);

    return 0;
}

static ssize_t bin_read(struct audio_stream_in *ain,
                        void *buffer, size_t bytes)
{
    struct brcm_stream_in *bin = (struct brcm_stream_in *)ain;
    size_t bytes_read;
    int ret = 0;

    pthread_mutex_lock(&bin->lock);

    if (!bin->started) {
        ret = bin->ops.do_bin_start(bin);
        if (ret) {
            ALOGE("%s: at %d, start failed\n",
                 __FUNCTION__, __LINE__);
            return ret;
        }
        bin->started = true;
    }

    bytes_read = bin->ops.do_bin_read(bin, buffer, bytes);

    pthread_mutex_unlock(&bin->lock);

    ALOGVV("%s: at %d, stream = 0x%X, bytes = %d, bytes_read = %d\n",
          __FUNCTION__, __LINE__, (uint32_t)ain, bytes, bytes_read);

    return bytes_read;
}

static uint32_t bin_get_input_frames_lost(struct audio_stream_in *ain)
{
    UNUSED(ain);

    ALOGV("%s: at %d, stream = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)ain);

    return 0;
}

/*
 * Device Functions
 */

static int bdev_init_check(const struct audio_hw_device *adev)
{
    UNUSED(adev);

    ALOGV("%s: at %d, dev = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)adev);

    return 0;
}

static int bdev_set_voice_volume(struct audio_hw_device *adev,
                                 float volume)
{
    UNUSED(adev);
    UNUSED(volume);

    ALOGV("%s: at %d, dev = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)adev);

    /* set_voice_volume is a no op (simulates phones) */
    return 0;
}

static int bdev_set_master_volume(struct audio_hw_device *adev,
                                  float volume)
{
    UNUSED(adev);

    ALOGV("%s: at %d, dev = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)adev);

    set_master_volume(volume);

    return 0;
}

static int bdev_get_master_volume(struct audio_hw_device *adev,
                                  float *volume)
{
    UNUSED(adev);
    UNUSED(volume);

    ALOGV("%s: at %d, dev = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)adev);

    *volume = get_master_volume();

    return 0;
}

static int bdev_set_master_mute(struct audio_hw_device *adev,
                                bool muted)
{
    UNUSED(adev);

    ALOGV("%s: at %d, dev = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)adev);

    set_mute_state(muted);

    return 0;
}

static int bdev_get_master_mute(struct audio_hw_device *adev,
                                bool *muted)
{
    UNUSED(adev);

    ALOGV("%s: at %d, dev = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)adev);

    *muted = get_mute_state();

    return 0;
}

static int bdev_set_mode(struct audio_hw_device *adev,
                         audio_mode_t mode)
{
    UNUSED(adev);
    UNUSED(mode);

    ALOGV("%s: at %d, dev = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)adev);

    /* set_mode is a no op (simulates phones) */
    return 0;
}

static int bdev_set_mic_mute(struct audio_hw_device *adev,
                             bool muted)
{
    struct brcm_device *bdev = (struct brcm_device *)adev;

    ALOGV("%s: at %d, dev = 0x%X, muted = %d\n",
         __FUNCTION__, __LINE__, (uint32_t)adev, muted);

    pthread_mutex_lock(&bdev->lock);
    bdev->mic_mute = muted;
    pthread_mutex_unlock(&bdev->lock);

    return 0;
}

static int bdev_get_mic_mute(const struct audio_hw_device *adev,
                             bool *muted)
{
    struct brcm_device *bdev = (struct brcm_device *)adev;

    ALOGV("%s: at %d, dev = 0x%X, muted = %d\n",
         __FUNCTION__, __LINE__, (uint32_t)adev, *muted);

    pthread_mutex_lock(&bdev->lock);
    *muted = bdev->mic_mute;
    pthread_mutex_unlock(&bdev->lock);

    return 0;
}

static int bdev_set_parameters(struct audio_hw_device *adev,
                               const char *kvpairs)
{
    struct brcm_device *bdev = (struct brcm_device *)adev;
    int i;

    ALOGV("%s: at %d, dev = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)adev);

    for (i = 0; i < BRCM_DEVICE_OUT_MAX; i++) {
        struct brcm_stream_out *bout = bdev->bouts[i];
        if (bout) {
            bout_set_parameters(&bout->aout.common, kvpairs);
        }
    }

    for (i = 0; i < BRCM_DEVICE_IN_MAX; i++) {
        struct brcm_stream_in *bin = bdev->bins[i];
        if (bin) {
            bin_set_parameters(&bin->ain.common, kvpairs);
        }
    }

    return 0;
}

static char *bdev_get_parameters(const struct audio_hw_device *adev,
                                 const char *keys)
{
    UNUSED(adev);
    UNUSED(keys);

    ALOGV("%s: at %d, dev = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)adev);

    return strdup("");
}

static size_t bdev_get_input_buffer_size(const struct audio_hw_device *adev,
                                         const struct audio_config *config)
{
    size_t buffer_size;

    UNUSED(adev);

    buffer_size =
        get_brcm_audio_buffer_size(config->sample_rate,
                                   config->format,
                                   popcount(config->channel_mask));

    ALOGV("%s: at %d, dev = 0x%X, buffer_size = %d\n",
         __FUNCTION__, __LINE__, (uint32_t)adev, buffer_size);

    return buffer_size;
}

static int bdev_open_output_stream(struct audio_hw_device *adev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **aout,
                                   const char *address)
{
    struct brcm_device *bdev = (struct brcm_device *)adev;
    struct brcm_stream_out *bout;
    brcm_devices_out_t bdevices;
    int ret = 0;

    UNUSED(handle);
    UNUSED(flags);
    UNUSED(address);

    ALOGV("%s: at %d, dev = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)adev);

    ALOGI("Open audio output stream:\n"
         "\tdevices = 0x%X\n"
         "\tflags = 0x%X\n"
         "\tsample_rate = %d\n"
         "\tchannel_mask = 0x%X\n"
         "\tformat = %d\n",
         devices,
         flags,
         config->sample_rate,
         config->channel_mask,
         config->format);

    *aout = NULL;

    bout = (struct brcm_stream_out *)calloc(1, sizeof(struct brcm_stream_out));
    if (!bout) {
        ALOGE("%s: at %d, alloc failed\n",
             __FUNCTION__, __LINE__);
        return -ENOMEM;
    }

    pthread_mutex_init(&bout->lock, NULL);

    bout->aout.common.get_sample_rate = bout_get_sample_rate;
    bout->aout.common.set_sample_rate = bout_set_sample_rate;
    bout->aout.common.get_buffer_size = bout_get_buffer_size;
    bout->aout.common.get_channels = bout_get_channels;
    bout->aout.common.get_format = bout_get_format;
    bout->aout.common.set_format = bout_set_format;
    bout->aout.common.standby = bout_standby;
    bout->aout.common.dump = bout_dump;
    bout->aout.common.set_parameters = bout_set_parameters;
    bout->aout.common.get_parameters = bout_get_parameters;
    bout->aout.common.add_audio_effect = bout_add_audio_effect;
    bout->aout.common.remove_audio_effect = bout_remove_audio_effect;
    bout->aout.get_latency = bout_get_latency;
    bout->aout.set_volume = bout_set_volume;
    bout->aout.write = bout_write;
    bout->aout.get_render_position = bout_get_render_position;
    bout->aout.get_next_write_timestamp = bout_get_next_write_timestamp;
    bout->aout.get_presentation_position = bout_get_presentation_position;

    bdevices = get_brcm_devices_out(devices);
    switch (bdevices) {
    case BRCM_DEVICE_OUT_NEXUS:
        bout->ops = nexus_bout_ops;
        break;
    case BRCM_DEVICE_OUT_NEXUS_DIRECT:
        bout->ops = nexus_direct_bout_ops;
        break;
    case BRCM_DEVICE_OUT_USB:
    default:
        ALOGE("%s: at %d, invalid devices %d\n",
             __FUNCTION__, __LINE__, devices);
        ret = -EINVAL;
        goto err_alloc;
    }

#if DUMMY_AUDIO_OUT
    bout->ops = dummy_bout_ops;
#endif

    bout->devices = devices;
    bout->config = *config;
    bout->bdev = bdev;

    pthread_mutex_lock(&bdev->lock);

    ret = bout->ops.do_bout_open(bout);
    if (ret) {
        ALOGE("%s: at %d, Nexus stream out open failed, ret = %d\n",
             __FUNCTION__, __LINE__, ret);
        ret = -EBUSY;
        goto err_lock;
    }

    if (bdev->bouts[bdevices]) {
        ALOGE("%s: at %d, output is already opened %d\n",
             __FUNCTION__, __LINE__, devices);
        ret = -EBUSY;
        goto err_lock;
    }
    bdev->bouts[bdevices] = bout;

    pthread_mutex_unlock(&bdev->lock);

    config->format = bout_get_format(&bout->aout.common);
    config->channel_mask = bout_get_channels(&bout->aout.common);
    config->sample_rate = bout_get_sample_rate(&bout->aout.common);

    *aout = &bout->aout;

    ALOGI("Audio output stream open, stream = 0x%X\n", (uint32_t)*aout);
    return 0;

 err_lock:
    pthread_mutex_unlock(&bdev->lock);

 err_alloc:
    pthread_mutex_destroy(&bout->lock);
    free(bout);
    return ret;
}

static void bdev_close_output_stream(struct audio_hw_device *adev,
                                     struct audio_stream_out *aout)
{
    struct brcm_device *bdev = (struct brcm_device *)adev;
    struct brcm_stream_out *bout = (struct brcm_stream_out *)aout;
    int i;

    ALOGV("%s: at %d, dev = 0x%X\n, stream = 0x%X",
         __FUNCTION__, __LINE__, (uint32_t)adev, (uint32_t)aout);

    pthread_mutex_lock(&bdev->lock);
    pthread_mutex_lock(&bout->lock);

    bout->ops.do_bout_close(bout);

    for (i = 0; i < BRCM_DEVICE_OUT_MAX; i++) {
        if (bdev->bouts[i] == bout) {
            bdev->bouts[i] = NULL;
            break;
        }
    }

    pthread_mutex_unlock(&bout->lock);
    pthread_mutex_unlock(&bdev->lock);

    pthread_mutex_destroy(&bout->lock);

    ALOGI("Audio output stream closed, stream = 0x%X\n", (uint32_t)aout);
    free(bout);
}

static int bdev_open_input_stream(struct audio_hw_device *adev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **ain,
                                  audio_input_flags_t flags,
                                  const char *address,
                                  audio_source_t source)
{
    struct brcm_device *bdev = (struct brcm_device *)adev;
    struct brcm_stream_in *bin;
    brcm_devices_in_t bdevices;
    int ret = 0;

    UNUSED(handle);
    UNUSED(flags);
    UNUSED(address);
    UNUSED(source);

    ALOGV("%s: at %d, dev = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)adev);

    ALOGI("Open audio input stream:\n"
         "\tdevices = 0x%X\n"
         "\tflags = 0x%X\n"
         "\tsample_rate = %d\n"
         "\tchannel_mask = 0x%X\n"
         "\tformat = %d\n",
         devices,
         flags,
         config->sample_rate,
         config->channel_mask,
         config->format);

    *ain = NULL;

    bin = (struct brcm_stream_in *)calloc(1, sizeof(struct brcm_stream_in));
    if (!bin) {
        ALOGE("%s: at %d, alloc failed\n",
             __FUNCTION__, __LINE__);
        return -ENOMEM;
    }

    pthread_mutex_init(&bin->lock, NULL);

    bin->ain.common.get_sample_rate = bin_get_sample_rate;
    bin->ain.common.set_sample_rate = bin_set_sample_rate;
    bin->ain.common.get_buffer_size = bin_get_buffer_size;
    bin->ain.common.get_channels = bin_get_channels;
    bin->ain.common.get_format = bin_get_format;
    bin->ain.common.set_format = bin_set_format;
    bin->ain.common.standby = bin_standby;
    bin->ain.common.dump = bin_dump;
    bin->ain.common.set_parameters = bin_set_parameters;
    bin->ain.common.get_parameters = bin_get_parameters;
    bin->ain.common.add_audio_effect = bin_add_audio_effect;
    bin->ain.common.remove_audio_effect = bin_remove_audio_effect;
    bin->ain.set_gain = bin_set_gain;
    bin->ain.read = bin_read;
    bin->ain.get_input_frames_lost = bin_get_input_frames_lost;

    bdevices = get_brcm_devices_in(devices);
    switch (bdevices) {
    case BRCM_DEVICE_IN_BUILTIN:
        bin->ops = builtin_bin_ops;
        break;
    case BRCM_DEVICE_IN_USB:
    default:
        ALOGE("%s: at %d, invalid devices %d\n",
             __FUNCTION__, __LINE__, devices);
        ret = -EINVAL;
        goto err_alloc;
    }

#if DUMMY_AUDIO_IN
    bin->ops = dummy_bin_ops;
#endif

    bin->devices = devices;
    bin->config = *config;
    bin->bdev = bdev;

    pthread_mutex_lock(&bdev->lock);

    ret = bin->ops.do_bin_open(bin);
    if (ret) {
        ALOGE("%s: at %d, Nexus stream in open failed, ret = %d\n",
             __FUNCTION__, __LINE__, ret);
        ret = -EBUSY;
        goto err_lock;
    }

    if (bdev->bins[bdevices]) {
        ALOGE("%s: at %d, input is already opened %d\n",
             __FUNCTION__, __LINE__, devices);
        ret = -EBUSY;
        goto err_lock;
    }
    bdev->bins[bdevices] = bin;

    pthread_mutex_unlock(&bdev->lock);

    config->format = bin_get_format(&bin->ain.common);
    config->channel_mask = bin_get_channels(&bin->ain.common);
    config->sample_rate = bin_get_sample_rate(&bin->ain.common);

    *ain = &bin->ain;

    ALOGI("Audio input stream open, stream = 0x%X\n", (uint32_t)*ain);
    return 0;

 err_lock:
    pthread_mutex_unlock(&bdev->lock);

 err_alloc:
    pthread_mutex_destroy(&bin->lock);
    free(bin);
    return ret;
}

static void bdev_close_input_stream(struct audio_hw_device *adev,
                                    struct audio_stream_in *ain)
{
    struct brcm_device *bdev = (struct brcm_device *)adev;
    struct brcm_stream_in *bin = (struct brcm_stream_in *)ain;
    int i;

    ALOGV("%s: at %d, dev = 0x%X, stream = 0x%X\n",
         __FUNCTION__, __LINE__, (uint32_t)adev, (uint32_t)ain);

    pthread_mutex_lock(&bdev->lock);
    pthread_mutex_lock(&bin->lock);

    bin->ops.do_bin_close(bin);

    for (i = 0; i < BRCM_DEVICE_IN_MAX; i++) {
        if (bdev->bins[i] == bin) {
            bdev->bins[i] = NULL;
            break;
        }
    }

    pthread_mutex_unlock(&bin->lock);
    pthread_mutex_unlock(&bdev->lock);

    pthread_mutex_destroy(&bin->lock);

    ALOGI("Audio input stream closed, stream = 0x%X\n", (uint32_t)ain);
    free(bin);
}

static int bdev_dump(const audio_hw_device_t *adev, int fd)
{
    struct brcm_device *bdev = (struct brcm_device *)adev;
    int i;

    dprintf(fd, "\nbdev_dump:\n"
            "\tmic_mute: %s\n\n",
            bdev->mic_mute ? "true": "false");

    for (i = 0; i < BRCM_DEVICE_OUT_MAX; i++) {
        struct brcm_stream_out *bout = bdev->bouts[i];
        if (bout) {
            bout_dump(&bout->aout.common, fd);
        }
    }

    for (i = 0; i < BRCM_DEVICE_IN_MAX; i++) {
        struct brcm_stream_in *bin = bdev->bins[i];
        if (bin) {
            bin_dump(&bin->ain.common, fd);
        }
    }

    return 0;
}

static int bdev_close(hw_device_t *dev)
{
    struct brcm_device *bdev = (struct brcm_device *)dev;
    int i;

    ALOGV("%s: at %d\n", __FUNCTION__, __LINE__);

    for (i = 0; i < BRCM_DEVICE_OUT_MAX; i++) {
        struct brcm_stream_out *bout = bdev->bouts[i];
        if (bout) {
            bdev_close_output_stream(&bdev->adev, &bout->aout);
        }
    }

    for (i = 0; i < BRCM_DEVICE_IN_MAX; i++) {
        struct brcm_stream_in *bin = bdev->bins[i];
        if (bin) {
            bdev_close_input_stream(&bdev->adev, &bin->ain);
        }
    }

    pthread_mutex_destroy(&bdev->lock);

    ALOGI("Audio device closed, dev = 0x%X\n", (uint32_t)dev);
    free(bdev);
    return 0;
}

static int bdev_open(const hw_module_t *module, const char *name,
                     hw_device_t **dev)
{
    struct brcm_device *bdev;
    int ret = 0;

    ALOGV("%s: at %d\n", __FUNCTION__, __LINE__);

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0) {
        ALOGE("%s: at %d, invalid module name %s\n",
             __FUNCTION__, __LINE__, name);
        return -EINVAL;
    }

    bdev = (struct brcm_device *)calloc(1, sizeof(struct brcm_device));
    if (!bdev) {
        ALOGE("%s: at %d, alloc failed\n",
             __FUNCTION__, __LINE__);
        return -ENOMEM;
    }

    pthread_mutex_init(&bdev->lock, NULL);

    bdev->adev.common.tag = HARDWARE_DEVICE_TAG;
    bdev->adev.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    bdev->adev.common.module = (struct hw_module_t *)module;
    bdev->adev.common.close = bdev_close;

    bdev->adev.init_check = bdev_init_check;
    bdev->adev.set_voice_volume = bdev_set_voice_volume;
    bdev->adev.set_master_volume = bdev_set_master_volume;
    bdev->adev.get_master_volume = bdev_get_master_volume;
    bdev->adev.set_master_mute = bdev_set_master_mute;
    bdev->adev.get_master_mute = bdev_get_master_mute;
    bdev->adev.set_mode = bdev_set_mode;
    bdev->adev.set_mic_mute = bdev_set_mic_mute;
    bdev->adev.get_mic_mute = bdev_get_mic_mute;
    bdev->adev.set_parameters = bdev_set_parameters;
    bdev->adev.get_parameters = bdev_get_parameters;
    bdev->adev.get_input_buffer_size = bdev_get_input_buffer_size;
    bdev->adev.open_output_stream = bdev_open_output_stream;
    bdev->adev.close_output_stream = bdev_close_output_stream;
    bdev->adev.open_input_stream = bdev_open_input_stream;
    bdev->adev.close_input_stream = bdev_close_input_stream;
    bdev->adev.dump = bdev_dump;

    *dev = &bdev->adev.common;

    bdev->standbyThread = new StandbyMonitorThread();

    ALOGI("Audio device open, dev = 0x%X\n", (uint32_t)*dev);
    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = bdev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "Brcm NEXUS audio HW HAL",
        .author = "Broadcom Corp",
        .methods = &hal_module_methods
    },
};
