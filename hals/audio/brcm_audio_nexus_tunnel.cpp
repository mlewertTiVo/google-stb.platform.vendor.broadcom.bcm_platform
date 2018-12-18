/******************************************************************************
 *    (c)2016 Broadcom Corporation
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
#include "brcm_audio_nexus_hdmi.h"
#include "brcm_audio_nexus_parser.h"
#include "OMX_Types.h"
#include "bomx_utils.h"
#include <inttypes.h>
#include "nexus_base_mmap.h"
#include "nexus_platform.h"
#include "nxclient.h"

#define STRINGIFY(x)                    #x
#define TOSTRING(x)                     STRINGIFY(x)

#define NEXUS_OUT_DEFAULT_SAMPLE_RATE   44100
#define NEXUS_OUT_DEFAULT_CHANNELS      AUDIO_CHANNEL_OUT_STEREO
#define NEXUS_OUT_DEFAULT_FORMAT        AUDIO_FORMAT_PCM_16_BIT

/* Supported stream out sample rate */
const static uint32_t nexus_out_sample_rates[] = {
   8000,
   11025,
   12000,
   16000,
   22050,
   24000,
   32000,
   44100,
   48000,
   64000,
   88200,
   96000,
   128000,
   176400,
   192000
};

#define NEXUS_OUT_SAMPLE_RATE_COUNT                     \
    (sizeof(nexus_out_sample_rates) / sizeof(uint32_t))

#define BRCM_AUDIO_STREAM_ID                    (0xC0)

#define BRCM_AUDIO_TUNNEL_DURATION_MS           (5)
#define BRCM_AUDIO_TUNNEL_HALF_DURATION_US      (BRCM_AUDIO_TUNNEL_DURATION_MS * 500)
#define BRCM_AUDIO_TUNNEL_FIFO_DURATION_MS      (200)
#define BRCM_AUDIO_TUNNEL_FIFO_MULTIPLIER       (BRCM_AUDIO_TUNNEL_FIFO_DURATION_MS / BRCM_AUDIO_TUNNEL_DURATION_MS)
#define BRCM_AUDIO_TUNNEL_DEBOUNCE_DURATION_MS  (300)
#define BRCM_AUDIO_TUNNEL_NEXUS_LATENCY_MS      (128)   // Audio latency is 128ms + lipsync offset in standard latency mode

#define BRCM_AUDIO_TUNNEL_COMP_BUFFER_SIZE      (2048)
#define BRCM_AUDIO_TUNNEL_COMP_DELAY_MAX_MS     (200)
#define BRCM_AUDIO_TUNNEL_COMP_EST_PERIOD_MS    (125)
#define BRCM_AUDIO_TUNNEL_COMP_EST_BYTE_MUL     (5)
#define BRCM_AUDIO_TUNNEL_COMP_THRES_MUL        (4)

#define BRCM_AUDIO_TUNNEL_STC_SYNC_INVALID      (0xFFFFFFFF)

#define KBITRATE_TO_BYTES_PER_250MS(kbr)        ((kbr) * 32)    // 1024/8/8*2
#define KBITRATE_TO_BYTES_PER_375MS(kbr)        ((kbr) * 48)    // 1024/8/8*3

/*
 * Function declarations
 */
static int nexus_tunnel_bout_pause(struct brcm_stream_out *bout);
static int nexus_tunnel_bout_resume(struct brcm_stream_out *bout);

const static uint8_t av_sync_marker[] = {0x55, 0x55};
#define HW_AV_SYNC_HDR_VER_LEN  2
#define HW_AV_SYNC_HDR_LEN_V1   16
#define HW_AV_SYNC_HDR_LEN_V2   20
#define HW_AV_SYNC_HDR_LEN_MAX  HW_AV_SYNC_HDR_LEN_V2

struct av_header_t {
    av_header_t() {
        length = 0;
        hdr_length = 0;
        hdr_version = 0;
        offset = 0;
    }
    unsigned version() {
        return hdr_version;
    }
    void set_version(unsigned version) {
        ALOG_ASSERT(version == 1 || version == 2);
        hdr_version = version;
        hdr_length = (version == 1) ? HW_AV_SYNC_HDR_LEN_V1 : HW_AV_SYNC_HDR_LEN_V2;
    }
    uint8_t *buffer() {
        return &hdr_buffer[0];
    }
    size_t len() {
        return length;
    }
    size_t bytes_hdr_needed() {
        return (length < hdr_length) ? hdr_length - length : 0;
    }
    void add_bytes(const void *buff, size_t size) {
        ALOG_ASSERT(hdr_length > 0 && size <= bytes_hdr_needed());
        memcpy(&hdr_buffer[length], buff, size);
        length += size;
        // if we have a full header, look for the offset field
        if (length == hdr_length) {
            offset = hdr_length;
            if (hdr_version == 2) {
                offset = B_MEDIA_LOAD_UINT32_BE(&hdr_buffer[0], 4*sizeof(uint32_t));
            }
        }
    }
    size_t bytes_offset_needed() {
        ALOG_ASSERT(offset >= hdr_length);
        return (offset > length) ? offset - length : 0;
    }
    void add_offset(size_t bytes) {
        ALOG_ASSERT(bytes <= bytes_offset_needed());
        length += bytes;
    }
    bool is_empty() {
        return (length == 0);
    }
    bool is_complete() {
        if (hdr_version == 1)
            return (length == hdr_length);
        else if (hdr_version == 2)
            return (length >= hdr_length) && (length == offset);
        else
            return false;
    }
    void reset() {
        length = 0;
        offset = 0;
    }
private:
    size_t length;
    size_t hdr_length;
    size_t hdr_version;
    size_t offset;
    uint8_t hdr_buffer[HW_AV_SYNC_HDR_LEN_MAX];
} av_header;

struct current_buff_t {
    current_buff_t() {
        bytes_written = 0;
    }
    void set_av_header(struct av_header_t av_hdr) {
        this->av_hdr = av_hdr;
        bytes_written = 0;
    }
    bool is_valid() {
        return av_hdr.is_complete();
    }
    size_t bytes_pending() {
        size_t frameBytes = frame_len();
        return (frameBytes > bytes_written) ? frameBytes - bytes_written : 0;
    }
    size_t frame_len() {
        if (!is_valid()) return 0;
        void *pts_buffer = av_hdr.buffer();
        return B_MEDIA_LOAD_UINT32_BE(pts_buffer, sizeof(uint32_t));
    }
    void add_bytes(size_t bytes) {
        bytes_written += bytes;
        if (bytes_written >= frame_len())
            reset();
    }
    void reset() {
        bytes_written = 0;
        av_hdr.reset();
    }
private:
    struct av_header_t av_hdr;
    size_t bytes_written;
} current_buff;
/*
 * Operation Functions
 */
static int nexus_tunnel_bout_set_volume(struct brcm_stream_out *bout,
                                 float left, float right)
{
    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;

    if (left != right) {
        ALOGV("%s: Left and Right volumes must be equal, cannot change volume", __FUNCTION__);
        return 0;
    }
    else if (left > 1.0) {
        ALOGV("%s: Volume: %f exceeds max volume of 1.0", __FUNCTION__, left);
        return 0;
    }

    if (bout->nexus.tunnel.fadeLevel != (unsigned)(left * 100)) {
        bout->nexus.tunnel.fadeLevel = (unsigned)(left * 100);

        ALOGV("%s: Volume: %d", __FUNCTION__, bout->nexus.tunnel.fadeLevel);
        if (bout->started) {
            if (bout->nexus.tunnel.deferred_volume) {
                struct timespec now;

                clock_gettime(CLOCK_MONOTONIC, &now);
                if (nexus_common_is_paused(audio_decoder)) {
                    // Record deferral amount if volume is set during priming
                    int32_t deferred_ms;
                    deferred_ms = (int32_t)((now.tv_sec - bout->nexus.tunnel.start_ts.tv_sec) * 1000) +
                                  (int32_t)((now.tv_nsec - bout->nexus.tunnel.start_ts.tv_nsec)/1000000);
                    bout->nexus.tunnel.deferred_volume_ms = (deferred_ms > MAX_VOLUME_DEFERRAL) ? MAX_VOLUME_DEFERRAL : deferred_ms;
                } else {
                    if (timespec_greater_than(now, bout->nexus.tunnel.deferred_window)) {
                        // Deferral window over, resume normal operation
                        bout->nexus.tunnel.deferred_volume = false;
                        nexus_common_set_volume(bout->bdev, audio_decoder, bout->nexus.tunnel.fadeLevel, NULL, -1, -1);
                    } else {
                        // Push deferral window based on current time
                        bout->nexus.tunnel.deferred_window = now;
                        timespec_add_ms(bout->nexus.tunnel.deferred_window, MAX_VOLUME_DEFERRAL);

                        nexus_common_set_volume(bout->bdev, audio_decoder, bout->nexus.tunnel.fadeLevel, NULL,
                            bout->nexus.tunnel.deferred_volume_ms, -1);
                    }
                }
            } else if (!nexus_common_is_paused(audio_decoder)) {
                // Apply volume immediately if not paused
                nexus_common_set_volume(bout->bdev, audio_decoder, bout->nexus.tunnel.fadeLevel, NULL, -1, -1);
            }
        }
    }

    return 0;
}

static unsigned nexus_tunnel_bout_get_frame_multipler(struct brcm_stream_out *bout)
{
    unsigned res = 1;

    switch (bout->config.format) {
        case AUDIO_FORMAT_AC3:
        {
            res = NEXUS_PCM_FRAMES_PER_AC3_FRAME;
            break;
        }
        case AUDIO_FORMAT_E_AC3:
        {
            res = (bout->nexus.tunnel.audioblocks_per_frame > 0) ?
                                        (bout->nexus.tunnel.audioblocks_per_frame * NEXUS_PCM_FRAMES_PER_AC3_AUDIO_BLOCK) :
                                        NEXUS_PCM_FRAMES_PER_AC3_FRAME; // Assuming 6 by default
            break;
        }
        case AUDIO_FORMAT_DTS:
        case AUDIO_FORMAT_DTS_HD:
        {
            res = (bout->nexus.tunnel.audioblocks_per_frame > 0) ?
                                        (bout->nexus.tunnel.audioblocks_per_frame * NEXUS_PCM_FRAMES_PER_DTS_SAMPLE_BLOCK) :
                                        NEXUS_PCM_FRAMES_PER_DTS_FRAME; // Assuming 16 by default
            break;
        }
        default:
        {
            // Return the default value of 1
            break;
        }
    }

    return res;
}

static int nexus_tunnel_bout_get_render_position(struct brcm_stream_out *bout, uint32_t *dsp_frames)
{
    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_AudioDecoderStatus status;
    NEXUS_Error ret;

    if (audio_decoder) {
        ret = NEXUS_SimpleAudioDecoder_GetStatus(audio_decoder, &status);
        if (ret != NEXUS_SUCCESS) {
            ALOGE("%s: Get render position failed, ret = %d", __FUNCTION__, ret);
            return -ENOSYS;
        }

        if (!status.started) {
            *dsp_frames = 0;
        }
        else {
            bout->framesPlayed += status.framesDecoded - bout->nexus.tunnel.lastCount;
            bout->nexus.tunnel.lastCount = status.framesDecoded;

            if (bout->nexus.tunnel.pcm_format) {
                uint64_t bytes = status.numBytesDecoded -
                   (bout->framesPlayed * (BMEDIA_WAVEFORMATEX_BASE_SIZE + bmedia_frame_bcma.len + 4));
                *dsp_frames = (uint32_t)(bytes/bout->frameSize);
            }
            else {
                *dsp_frames = (uint32_t)(bout->framesPlayed * bout->nexus.tunnel.frame_multiplier);
            }
        }
    }
    else {
        *dsp_frames = 0;
    }

    return 0;
}

static int nexus_tunnel_bout_get_presentation_position(struct brcm_stream_out *bout, uint64_t *frames)
{
    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_AudioDecoderStatus status;
    NEXUS_Error ret;

    if (audio_decoder) {
        ret = NEXUS_SimpleAudioDecoder_GetStatus(audio_decoder, &status);
        if (ret != NEXUS_SUCCESS) {
            ALOGE("%s: Get presentation position failed, ret = %d", __FUNCTION__, ret);
            return -ENOSYS;
        }

        if (!status.started) {
            *frames = bout->framesPlayedTotal;
        }
        else {
            bout->framesPlayed += status.framesDecoded - bout->nexus.tunnel.lastCount;
            bout->nexus.tunnel.lastCount = status.framesDecoded;

            if (bout->nexus.tunnel.pcm_format) {
                uint64_t bytes = status.numBytesDecoded -
                   (bout->framesPlayed * (BMEDIA_WAVEFORMATEX_BASE_SIZE + bmedia_frame_bcma.len + 4));
                *frames = bout->framesPlayedTotal + bytes/bout->frameSize;
            }
            else {
                *frames = (bout->framesPlayedTotal + bout->framesPlayed) * bout->nexus.tunnel.frame_multiplier;
            }
        }
    }
    else {
        *frames = 0;
    }
    return 0;
}


static uint32_t nexus_tunnel_bout_get_latency(struct brcm_stream_out *bout)
{
    UNUSED(bout);
    return BRCM_AUDIO_TUNNEL_NEXUS_LATENCY_MS;
}

static int32_t nexus_tunnel_bout_get_fifo_depth(struct brcm_stream_out *bout)
{
    NEXUS_AudioDecoderStatus decStatus;
    NEXUS_PlaypumpStatus ppStatus;
    unsigned bitrate = 0;

    ALOG_ASSERT(!bout->nexus.tunnel.pcm_format);

    NEXUS_Error rc = NEXUS_SimpleAudioDecoder_GetStatus(bout->nexus.tunnel.audio_decoder, &decStatus);
    if (rc != NEXUS_SUCCESS) {
        ALOGE("%s: Error retriving audio decoder status %u", __FUNCTION__, rc);
        return -1;
    }
    rc = NEXUS_Playpump_GetStatus(bout->nexus.tunnel.playpump, &ppStatus);
    if (rc != NEXUS_SUCCESS) {
        ALOGE("%s: Error retriving playpump status %u", __FUNCTION__, rc);
        return -1;
    }

    switch (decStatus.codec) {
        case NEXUS_AudioCodec_eAc3:
        case NEXUS_AudioCodec_eAc3Plus: {
            bitrate = decStatus.codecStatus.ac3.bitrate;
        }
        case NEXUS_AudioCodec_eDts:
        case NEXUS_AudioCodec_eDtsHd: {
            bitrate = decStatus.codecStatus.dts.bitRate;
        }
        default: {
        }
    }
    if (bitrate > 0 && bitrate != bout->nexus.tunnel.bitrate) {
        bout->nexus.tunnel.bitrate = bitrate;
        ALOGI("%s: new bitrate detected: %u", __FUNCTION__, bout->nexus.tunnel.bitrate);
    }

    ALOGV("%s: %u(%u/%u) thrs=%u/%u", __FUNCTION__, ppStatus.fifoDepth + decStatus.fifoDepth, ppStatus.fifoDepth, decStatus.fifoDepth, KBITRATE_TO_BYTES_PER_250MS(bout->nexus.tunnel.bitrate), KBITRATE_TO_BYTES_PER_375MS(bout->nexus.tunnel.bitrate));

    return ppStatus.fifoDepth + decStatus.fifoDepth;
}

static bool nexus_tunnel_bout_pause_int(struct brcm_stream_out *bout)
{
    NEXUS_Error res;

    ALOGV("%s", __FUNCTION__);
    ALOGV_FIFO_INFO(bout->nexus.tunnel.audio_decoder, bout->nexus.tunnel.playpump);

    // Stop volume deferral as soon as paused
    bout->nexus.tunnel.deferred_volume = false;

    res = nexus_common_mute_and_pause(bout->bdev, bout->nexus.tunnel.audio_decoder, bout->nexus.tunnel.stc_channel,
                                     bout->nexus.tunnel.soft_muting,
                                     bout->nexus.tunnel.sleep_after_mute);

    if (res != NEXUS_SUCCESS) {
        ALOGE("%s: Error pausing %u", __FUNCTION__, res);
        return false;
    }

    ALOGV_FIFO_INFO(bout->nexus.tunnel.audio_decoder, bout->nexus.tunnel.playpump);
    return true;
}

static bool nexus_tunnel_bout_resume_pcm(struct brcm_stream_out *bout)
{
    NEXUS_Error res;

    ALOGV("%s, %p", __FUNCTION__, bout);

    res = nexus_common_resume_and_unmute(bout->bdev, bout->nexus.tunnel.audio_decoder, bout->nexus.tunnel.stc_channel,
                                         bout->nexus.tunnel.soft_unmuting, bout->nexus.tunnel.fadeLevel);
    if (res != NEXUS_SUCCESS) {
        ALOGE("%s: Error resuming %u", __FUNCTION__, res);

        return false;
    }

    return true;
}

static bool nexus_tunnel_bout_resume_comp(struct brcm_stream_out *bout)
{
    NEXUS_Error res;

    ALOGV("%s, %p", __FUNCTION__, bout);

    ALOG_ASSERT(!bout->nexus.tunnel.pcm_format);

    int32_t fifoDepth = nexus_tunnel_bout_get_fifo_depth(bout);
    if (fifoDepth < 0) {
        return false;
    }

    if (bout->nexus.tunnel.bitrate > 0 && (uint32_t)fifoDepth >= KBITRATE_TO_BYTES_PER_375MS(bout->nexus.tunnel.bitrate)) {
        ALOGV("%s: Resume without priming", __FUNCTION__);

        if (bout->nexus.tunnel.deferred_volume) {
            if (bout->nexus.tunnel.deferred_volume_ms) {
                struct timespec now;
                int32_t deferred_ms;

                clock_gettime(CLOCK_MONOTONIC, &now);

                // Apply existing deferred volume
                res = nexus_common_resume_and_unmute(bout->bdev, bout->nexus.tunnel.audio_decoder, bout->nexus.tunnel.stc_channel,
                                                     bout->nexus.tunnel.deferred_volume_ms, bout->nexus.tunnel.fadeLevel);

                // Record delta between resume and first start as subsequent deferral amount
                deferred_ms = (int32_t)((now.tv_sec - bout->nexus.tunnel.start_ts.tv_sec) * 1000) +
                              (int32_t)((now.tv_nsec - bout->nexus.tunnel.start_ts.tv_nsec)/1000000);
                bout->nexus.tunnel.deferred_volume_ms = (deferred_ms > MAX_VOLUME_DEFERRAL) ? MAX_VOLUME_DEFERRAL : deferred_ms;

                // Set deferral window
                bout->nexus.tunnel.deferred_window = now;
                timespec_add_ms(bout->nexus.tunnel.deferred_window, MAX_VOLUME_DEFERRAL);
            } else {
                // No volume was set during priming, no need to defer
                bout->nexus.tunnel.deferred_volume = false;
            }
        }

        if (!bout->nexus.tunnel.deferred_volume) {
            res = nexus_common_resume_and_unmute(bout->bdev, bout->nexus.tunnel.audio_decoder, bout->nexus.tunnel.stc_channel,
                                                 bout->nexus.tunnel.soft_unmuting, bout->nexus.tunnel.fadeLevel);
        }


        if (res != NEXUS_SUCCESS) {
           ALOGE("%s: Error resuming %u", __FUNCTION__, res);
           return false;
        }
        bout->nexus.tunnel.priming = false;
    }
    else {
        ALOGV("%s: Resume with priming", __FUNCTION__);
        bout->nexus.tunnel.priming = true;
    }

    return true;
}

static void nexus_tunnel_bout_debounce_reset(struct brcm_stream_out *bout)
{
    bout->nexus.tunnel.debounce = false;
    bout->nexus.tunnel.debounce_pausing = false;
    bout->nexus.tunnel.debounce_more = false;
    bout->nexus.tunnel.debounce_expired = false;
    bout->nexus.tunnel.debounce_stopping = false;
    bout->nexus.tunnel.last_pause_time = 0;
}

static int nexus_tunnel_bout_start(struct brcm_stream_out *bout)
{
    NEXUS_Error ret;

    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_SimpleAudioDecoderStartSettings start_settings;
    NEXUS_SimpleAudioDecoderSettings settings;

    if (bout->suspended || !audio_decoder) {
        ALOGE("%s: at %d, device not open\n",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    // defer start until first write request
    if (!bout->nexus.tunnel.first_write)
        return 0;

    clock_gettime(CLOCK_MONOTONIC, &bout->nexus.tunnel.start_ts);

    ret = NEXUS_Playpump_Start(bout->nexus.tunnel.playpump);
    if (ret != NEXUS_SUCCESS) {
        ALOGE("%s: Start playpump failed, ret = %d", __FUNCTION__, ret);
        return -ENOSYS;
    }

    unsigned threshold = bout->buffer_size * BRCM_AUDIO_TUNNEL_FIFO_MULTIPLIER;
    NEXUS_SimpleAudioDecoder_GetSettings(audio_decoder, &settings);
    ALOGV("Primary fifoThreshold %u->%u", settings.primary.fifoThreshold, threshold);
    settings.primary.fifoThreshold = threshold;
    NEXUS_SimpleAudioDecoder_SetSettings(audio_decoder, &settings);

    NEXUS_SimpleAudioDecoder_GetDefaultStartSettings(&start_settings);
    start_settings.primary.codec = brcm_audio_get_codec_from_format(bout->config.format);
    start_settings.primary.pidChannel = bout->nexus.tunnel.pid_channel;

    if (bout->bdev->dolby_ms == 12) {
        start_settings.primary.mixingMode = NEXUS_AudioDecoderMixingMode_eStandalone;

        if ((start_settings.primary.codec == NEXUS_AudioCodec_eAc3) ||
            (start_settings.primary.codec == NEXUS_AudioCodec_eAc3Plus)) {
            NEXUS_AudioDecoderCodecSettings codecSettings;
            NEXUS_AudioDecoderDolbySettings *dolbySettings;

            // Get default settings
            NEXUS_SimpleAudioDecoder_GetCodecSettings(audio_decoder,
                    NEXUS_SimpleAudioDecoderSelector_ePrimary, start_settings.primary.codec, &codecSettings);
            if (codecSettings.codec != start_settings.primary.codec) {
                ALOGE("%s: Codec mismatch %d != %d", __FUNCTION__,
                        codecSettings.codec, start_settings.primary.codec);
                NEXUS_Playpump_Stop(bout->nexus.tunnel.playpump);
                return -ENOSYS;
            }

            dolbySettings = (codecSettings.codec == NEXUS_AudioCodec_eAc3)?
                                &codecSettings.codecSettings.ac3:
                                &codecSettings.codecSettings.ac3Plus;

            dolbySettings->enableAtmosProcessing = true;
            if (property_get_bool(BCM_RO_AUDIO_DISABLE_ATMOS, false) ||
                property_get_bool(BCM_PERSIST_AUDIO_DISABLE_ATMOS, false)) {
                dolbySettings->enableAtmosProcessing = false;
            }

            ALOGI("%s: %s Dolby ATMOS", __FUNCTION__,
                dolbySettings->enableAtmosProcessing?"Enabling":"Disabling");

            ret = NEXUS_SimpleAudioDecoder_SetCodecSettings(audio_decoder,
                    NEXUS_SimpleAudioDecoderSelector_ePrimary, &codecSettings);
            if (ret) {
                ALOGE("%s: Unable to set codec %d settings, ret = %d", __FUNCTION__,
                        start_settings.primary.codec, ret);
                NEXUS_Playpump_Stop(bout->nexus.tunnel.playpump);
                return -ENOSYS;
            }
        }
    }

    ret = NEXUS_SimpleAudioDecoder_Start(audio_decoder, &start_settings);
    if (ret != NEXUS_SUCCESS) {
        ALOGE("%s: Start audio decoder failed, ret = %d", __FUNCTION__, ret);
        NEXUS_Playpump_Stop(bout->nexus.tunnel.playpump);
        return -ENOSYS;
    }
    ALOGV("%s: Audio decoder started (0x%x:%d)", __FUNCTION__, bout->config.format, start_settings.primary.codec);

    bout->nexus.tunnel.priming = false;
    if (!bout->nexus.tunnel.pcm_format) {
        ret = nexus_common_mute_and_pause(bout->bdev, audio_decoder, bout->nexus.tunnel.stc_channel, 0, 0);
        if (ret != NEXUS_SUCCESS) {
            ALOGE("%s: Error pausing audio decoder for priming %u", __FUNCTION__, ret);
        } else {
            bout->nexus.tunnel.priming = true;
        }
    }

    nexus_tunnel_bout_debounce_reset(bout);
    bout->nexus.tunnel.last_write_time = 0;
    bout->nexus.tunnel.last_written_ts = UINT64_MAX;

    bout->nexus.tunnel.lastCount = 0;
    bout->nexus.tunnel.audioblocks_per_frame = 0;
    bout->nexus.tunnel.frame_multiplier = nexus_tunnel_bout_get_frame_multipler(bout);
    bout->nexus.tunnel.bitrate = 0;
    bout->nexus.tunnel.last_bytes_written = 0;
    bout->framesPlayed = 0;

    return 0;
}

static int nexus_tunnel_bout_stop(struct brcm_stream_out *bout)
{
    NEXUS_Error ret;

    if (bout->nexus.tunnel.debounce) {
       // Wait for the debouncing thread to finish
       ALOGV("%s: Waiting for debouncing to finish", __FUNCTION__);
       pthread_t thread = bout->nexus.tunnel.debounce_thread;
       bout->nexus.tunnel.debounce_stopping = true;

       pthread_mutex_unlock(&bout->lock);
       pthread_join(thread, NULL);
       pthread_mutex_lock(&bout->lock);
       ALOGV("%s:      ... done", __FUNCTION__);
    }

    av_header.reset();
    current_buff.reset();
    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.tunnel.playpump;

    {
        NEXUS_AudioDecoderStatus decoderStatus;
        NEXUS_PlaypumpStatus playpumpStatus;

        NEXUS_SimpleAudioDecoder_GetStatus(audio_decoder, &decoderStatus);
        NEXUS_Playpump_GetStatus(playpump, &playpumpStatus);

        ALOGV("%s: AC3 bitrate = %u, decoder = %u/%u, playpump = %u/%u", __FUNCTION__,
            decoderStatus.codecStatus.ac3.bitrate,
            decoderStatus.fifoDepth,
            decoderStatus.fifoSize,
            playpumpStatus.fifoDepth,
            playpumpStatus.fifoSize);
    }

    if (audio_decoder) {
        NEXUS_SimpleAudioDecoder_Stop(audio_decoder);

        NEXUS_AudioDecoderStatus status;
        ret = NEXUS_SimpleAudioDecoder_GetStatus(audio_decoder, &status);
        if (ret != NEXUS_SUCCESS) {
            ALOGE("%s: Update frame played failed, ret=%d", __FUNCTION__, ret);
        }
        else {
            bout->framesPlayed += status.framesDecoded - bout->nexus.tunnel.lastCount;
            bout->nexus.tunnel.lastCount = status.framesDecoded;

            if (bout->nexus.tunnel.pcm_format) {
                uint64_t bytes = status.numBytesDecoded -
                   (bout->framesPlayed * (BMEDIA_WAVEFORMATEX_BASE_SIZE + bmedia_frame_bcma.len + 4));
                bout->framesPlayedTotal += bytes/bout->frameSize;
            }
            else {
                bout->framesPlayedTotal += bout->framesPlayed;
            }
        }
    }

    if (playpump) {
        NEXUS_Playpump_Stop(playpump);
    }

    // In case stc channel was left frozen by the previous output stream (during seeking)
    ret = NEXUS_SimpleStcChannel_Freeze(bout->nexus.tunnel.stc_channel, false);
    if (ret != NEXUS_SUCCESS) {
       ALOGE("%s: Error starting STC %d", __FUNCTION__, ret);
       NEXUS_Playpump_Stop(bout->nexus.tunnel.playpump);
       return -ENOSYS;
    }

    ALOGV("%s: setting framesPlayedTotal to %" PRIu64 "", __FUNCTION__, bout->framesPlayedTotal);
    bout->nexus.tunnel.first_write = false;
    bout->nexus.tunnel.lastCount = 0;
    bout->nexus.tunnel.audioblocks_per_frame = 0;
    bout->nexus.tunnel.frame_multiplier = nexus_tunnel_bout_get_frame_multipler(bout);
    bout->nexus.tunnel.bitrate = 0;
    bout->nexus.tunnel.last_bytes_written = 0;
    bout->framesPlayed = 0;

    return 0;
}

static void *nexus_tunnel_bout_debounce_task(void *argv)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)argv;
    nsecs_t now;
    int delta_ms;

    ALOGV("%s: Debouncing started", __FUNCTION__);

    usleep(BRCM_AUDIO_TUNNEL_DEBOUNCE_DURATION_MS * 1000);

    pthread_mutex_lock(&bout->lock);
    while (!bout->nexus.tunnel.debounce_stopping && bout->nexus.tunnel.debounce_more) {
       bout->nexus.tunnel.debounce_more = false;

       now = systemTime(SYSTEM_TIME_MONOTONIC);
       delta_ms = BRCM_AUDIO_TUNNEL_DEBOUNCE_DURATION_MS - toMillisecondTimeoutDelay(bout->nexus.tunnel.last_pause_time, now);
       if (delta_ms > 0 && delta_ms <= BRCM_AUDIO_TUNNEL_DEBOUNCE_DURATION_MS) {
          ALOGV("%s: Debouncing more for %d ms", __FUNCTION__, delta_ms);
          pthread_mutex_unlock(&bout->lock);
          usleep(delta_ms * 1000);
          pthread_mutex_lock(&bout->lock);
       }
    }

    bout->nexus.tunnel.debounce_expired = true;

    if (!bout->nexus.tunnel.debounce_stopping) {
       if (bout->nexus.tunnel.debounce_pausing) {
          ALOGV("%s: Pause after debouncing", __FUNCTION__);
          nexus_tunnel_bout_pause(bout);
       }
       else {
          ALOGV("%s: Resume after debouncing", __FUNCTION__);
          nexus_tunnel_bout_resume(bout);
       }

       pthread_detach(pthread_self());
    }

    nexus_tunnel_bout_debounce_reset(bout);

    pthread_mutex_unlock(&bout->lock);

    ALOGV("%s: Debouncing stopped", __FUNCTION__);

    return NULL;
}

static int nexus_tunnel_bout_pause(struct brcm_stream_out *bout)
{
    NEXUS_Error res;

    if (!bout->nexus.tunnel.audio_decoder || !bout->nexus.tunnel.stc_channel) {
       return -ENOENT;
    }

    if (!bout->nexus.tunnel.no_debounce) {
        // Audio underruns can happen when the app is not feeding the audio frames fast enough
        // especially at the beginning of the playback or after seeking. We debounce the pause/resume
        // operations in such underruns by deferring the pause for 0.3 seconds.
        if (!bout->nexus.tunnel.debounce) {
           if (pthread_create(&bout->nexus.tunnel.debounce_thread, NULL, nexus_tunnel_bout_debounce_task, bout) != 0) {
              ALOGE("%s: Error starting debouncing", __FUNCTION__);
           }
           else {
              pthread_setname_np(bout->nexus.tunnel.debounce_thread, "baudio_debounce");
              bout->nexus.tunnel.debounce = true;
              bout->nexus.tunnel.debounce_pausing = true;

              return 0;
           }
        }

        // No-op when debouncing
        if (bout->nexus.tunnel.debounce && !bout->nexus.tunnel.debounce_expired) {
           ALOGV("%s: No-op", __FUNCTION__);
           if (!bout->nexus.tunnel.debounce_pausing) {
              bout->nexus.tunnel.debounce_more = true;
              bout->nexus.tunnel.last_pause_time = systemTime(SYSTEM_TIME_MONOTONIC);
           }
           bout->nexus.tunnel.debounce_pausing = true;
           return 0;
        }
    }

    if (bout->nexus.tunnel.pcm_format) {
        if (!nexus_tunnel_bout_pause_int(bout)) {
            return -ENOMEM;
        }
    }
    else {
        if (!bout->nexus.tunnel.priming) {
            if (!nexus_tunnel_bout_pause_int(bout)) {
                return -ENOMEM;
            }
        }
        else {
            ALOGV("%s: Stop priming", __FUNCTION__);
            bout->nexus.tunnel.priming = false;
        }
    }
    ALOGV("%s", __FUNCTION__);

    return 0;
}

static int nexus_tunnel_bout_resume(struct brcm_stream_out *bout)
{
    NEXUS_Error res;

    if (!bout->nexus.tunnel.audio_decoder || !bout->nexus.tunnel.stc_channel) {
       return -ENOENT;
    }

    // No-op when debouncing
    if (bout->nexus.tunnel.debounce && !bout->nexus.tunnel.debounce_expired) {
       ALOGV("%s: No-op", __FUNCTION__);
       bout->nexus.tunnel.debounce_pausing = false;
       bout->nexus.tunnel.debounce_more = false;
       return 0;
    }

    if (bout->nexus.tunnel.pcm_format) {
        if (!nexus_tunnel_bout_resume_pcm(bout)) {
            return -ENOMEM;
        }
    }
    else {
        if (!bout->nexus.tunnel.priming) {
            ALOGV("%s: priming decoder for resume", __FUNCTION__);
            bout->nexus.tunnel.priming = true;
        }
    }
    ALOGV("%s", __FUNCTION__);

    return 0;
}

static int nexus_tunnel_bout_drain(struct brcm_stream_out *bout, int action)
{
    (void)bout;
    (void)action;
    // TBD
    return 0;
}

static int nexus_tunnel_bout_flush(struct brcm_stream_out *bout)
{
    NEXUS_Error res;
    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.tunnel.playpump;

    if (!audio_decoder || !playpump) {
        return -ENOENT;
    }

    if (bout->nexus.tunnel.debounce) {
       // Wait for the debouncing thread to finish
       ALOGV("%s: Waiting for debouncing to finish", __FUNCTION__);
       pthread_t thread = bout->nexus.tunnel.debounce_thread;
       bout->nexus.tunnel.debounce_stopping = true;

       pthread_mutex_unlock(&bout->lock);
       pthread_join(thread, NULL);
       pthread_mutex_lock(&bout->lock);
       ALOGV("%s:      ... done", __FUNCTION__);
    }

    res = NEXUS_Playpump_Flush(playpump);
    if (res != NEXUS_SUCCESS) {
       ALOGE("%s: Error flushing playpump %u", __FUNCTION__, res);
       return -ENOMEM;
    }

    NEXUS_SimpleAudioDecoder_Flush(audio_decoder);

    bout->framesPlayed = 0;
    bout->framesPlayedTotal = 0;
    bout->nexus.tunnel.lastCount = 0;
    bout->nexus.tunnel.first_write = false;
    bout->nexus.tunnel.audioblocks_per_frame = 0;
    bout->nexus.tunnel.frame_multiplier = nexus_tunnel_bout_get_frame_multipler(bout);
    bout->nexus.tunnel.bitrate = 0;
    bout->nexus.tunnel.last_bytes_written = 0;

    return 0;
}

static int nexus_tunnel_bout_write(struct brcm_stream_out *bout,
                            const void* buffer, size_t bytes)
{
    size_t bytes_written = 0;
    int ret = 0;
    void *nexus_buffer, *pts_buffer;
    uint32_t pts=0;
    size_t nexus_space;
    NEXUS_Error rc;
    NEXUS_AudioDecoderStatus decStatus;
    NEXUS_PlaypumpStatus ppStatus;
    NEXUS_AudioDecoderTrickState trickState;
    BKNI_EventHandle event = bout->nexus.event;
    bool init_stc = false;
    uint64_t last_written_ts = UINT64_MAX;

    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.tunnel.playpump;

    if (bout->suspended || !audio_decoder || !playpump) {
        ALOGE("%s: not ready to output audio samples", __FUNCTION__);
        return -ENOSYS;
    }

    if (!bout->nexus.tunnel.first_write && (bytes > 0)) {
        rc = NEXUS_SimpleAudioDecoder_GetStatus(audio_decoder, &decStatus);
        if (rc != NEXUS_SUCCESS) {
            ALOGE("%s: failed to get status, ret:%d", __FUNCTION__, rc);
            return -ENOSYS;
        }
        bout->nexus.tunnel.first_write = true;
        if (!decStatus.started) {
            ret = nexus_tunnel_bout_start(bout);
            if (ret != 0) {
                ALOGE("%s: failed to start, ret:%d", __FUNCTION__, ret);
                bout->nexus.tunnel.first_write = false;
                return -ENOSYS;
            }
        }
        // init stc with the first audio timestamp
        init_stc = true;
    }

    if (!av_header.is_empty() && !av_header.is_complete()) {
        ALOGV("%s: av_header.len:%zu, bytes:%zu", __FUNCTION__, av_header.len(), bytes);
        size_t bytes_added = 0;
        if (av_header.bytes_hdr_needed() > 0) {
            bytes_added = av_header.bytes_hdr_needed() > bytes ?  bytes : av_header.bytes_hdr_needed();
            av_header.add_bytes(buffer, bytes_added);
        }
        // check if av header has offset
        size_t offset_aded = 0;
        if (av_header.bytes_hdr_needed() == 0 && !av_header.is_complete()) {
            offset_aded = av_header.bytes_offset_needed() > bytes ? bytes : av_header.bytes_offset_needed();
            av_header.add_offset(offset_aded);
        }
        bytes_written += bytes_added + offset_aded;
        bytes -= bytes_added + offset_aded;
        buffer = (void *)((uint8_t *)buffer + bytes_added + offset_aded);
    }

    while ( bytes > 0 )
    {
        bool writeHeader = false;
        size_t frameBytes;
        size_t offset;
        bool split_header = false;
        size_t bytes_segment = bytes;

        if (!bout->nexus.tunnel.pcm_format) {
            NEXUS_SimpleAudioDecoder_GetTrickState(audio_decoder, &trickState);
            if (!bout->nexus.tunnel.priming && trickState.rate != 0) {
                /* Limit from accumulating too much data */
                if (bout->nexus.tunnel.bitrate > 0) {
                    unsigned bitrate;
                    int32_t fifoDepth;
                    fifoDepth = nexus_tunnel_bout_get_fifo_depth(bout);
                    if (fifoDepth < 0) {
                        break;
                    }
                    if (bout->nexus.tunnel.bitrate > 0 && (uint32_t)fifoDepth >= KBITRATE_TO_BYTES_PER_250MS(bout->nexus.tunnel.bitrate)) {
                        /* Enough data accumulated in playpump and decoder */
                        BKNI_ResetEvent(event);
                        pthread_mutex_unlock(&bout->lock);
                        ret = BKNI_WaitForEvent(event, 50);
                        pthread_mutex_lock(&bout->lock);
                        if (ret == BERR_TIMEOUT) {
                            ret = 0;
                            goto done;
                        }
                            /* Check again */
                        continue;
                    }
                }
            }
            else if (bout->nexus.tunnel.priming) {
                nexus_tunnel_bout_resume_comp(bout);
            }
        }

        ALOG_ASSERT(av_header.is_empty() || av_header.is_complete());
        // Search for next frame header
        if (av_header.is_complete()) {
            pts_buffer = av_header.buffer();
            split_header = true;
        } else {
            uint8_t *buffer_ptr = (uint8_t*)buffer;
            size_t offset = 0;
            size_t min_bytes_for_sync = HW_AV_SYNC_HDR_VER_LEN + sizeof(av_sync_marker);
            bool header_found = false;

            // if we're in the middle of writing a frame, try to find the next marker immediately
            // after the current frame boundary
            if (current_buff.is_valid() && (current_buff.bytes_pending() > 0) &&
               (current_buff.bytes_pending() + min_bytes_for_sync) <= bytes ) {
                offset = current_buff.bytes_pending();
                ALOGVV("%s: frame writing in progress, bytes_pending:%zu", __FUNCTION__, offset);
                pts_buffer = memmem(buffer_ptr + offset, bytes - offset, av_sync_marker, sizeof(av_sync_marker));

                // if there's a marker at the expected location, check if version matches
                if (pts_buffer == (void*)(buffer_ptr + offset) && (av_header.version() != 0)) {
                    uint8_t *pts_buffer_u8 = (uint8_t*)pts_buffer;
                    unsigned hdr_version = pts_buffer_u8[2] << 8 | pts_buffer_u8[3];
                    if (hdr_version != av_header.version()) {
                        // This pattern looks like an av_sync header but it is not.
                        ALOGW("spurious marker at expected location, hdr_version:0x%02X,0x%02X",
                            pts_buffer_u8[2], pts_buffer_u8[3]);
                    } else {
                        ALOGVV("%s: found header at expected offset:%d, bytes:%zu",
                        __FUNCTION__, ((uint8_t*)pts_buffer - (uint8_t*)buffer), bytes);
                        header_found = true;
                    }
                } else {
                    ALOGW("%s: not header marker found at expected location:%zu", __FUNCTION__, offset);
                }
            }
            if (!header_found) {
                buffer_ptr = (uint8_t*)buffer;
                offset = 0;
            }

            // If no marker found with method above, do a blind search
            while (!header_found) {
                pts_buffer = memmem(buffer_ptr + offset, bytes - offset, av_sync_marker, sizeof(av_sync_marker));
                if ((pts_buffer != NULL) && (av_header.version() != 0)) {
                    ALOGVV("%s: marker at offset:%zu, bytes:%zu",
                        __FUNCTION__, ((uint8_t*)pts_buffer - (uint8_t*)buffer), bytes);
                    if (bytes < min_bytes_for_sync) {
                        // We can't verify if this is a valid header as we can't even see the version.
                        // Stop processing the header and wait for the framework to send us a full one
                        // on the next buffer
                        ALOGV("%s: found apparent header marker at end of buffer", __FUNCTION__);
                        pts_buffer = NULL;
                        bytes_segment = bytes = 0;
                        break;
                    } else {
                        uint8_t *pts_buffer_u8 = (uint8_t*)pts_buffer;
                        unsigned hdr_version = pts_buffer_u8[2] << 8 | pts_buffer_u8[3];
                        if (hdr_version != av_header.version()) {
                            // we found data which looks like an av_sync header but it isn't
                            ALOGV("spurious header, hdr_version:0x%02X,0x%02X", pts_buffer_u8[2], pts_buffer_u8[3]);
                            // Ignore the first byte of the false marker and resume the search
                            offset = pts_buffer_u8 - buffer_ptr + 1;
                        } else {
                            break;
                        }
                    }
                } else {
                    break;
                }
            }
        }

        if ( NULL != pts_buffer )
        {
            if ((buffer == pts_buffer) || split_header)
            {
                if (buffer == pts_buffer)
                {
                    uint8_t *ptru8 = (uint8_t*)pts_buffer;
                    // Try to lock header version
                    if (av_header.version() == 0) {
                        size_t min_bytes_for_sync = HW_AV_SYNC_HDR_VER_LEN + sizeof(av_sync_marker);
                        uint8_t *pts_buffer_u8 = (uint8_t*)pts_buffer;
                        unsigned hdr_version = (bytes >= min_bytes_for_sync) ?
                                                pts_buffer_u8[2] << 8 | pts_buffer_u8[3] : 0;
                        if ((hdr_version != 1) && (hdr_version != 2)) {
                            // Very unlikely scenario but just to be on the safe side, ignore
                            // this marker
                            ALOGI("%s: skipping malformed initial header, bytes:%zu, hdr_version:%u",
                                  __FUNCTION__, bytes, hdr_version);
                            bytes -= sizeof(av_sync_marker);
                            buffer = (uint8_t*)buffer + sizeof(av_sync_marker);
                            pts_buffer = NULL;
                            continue;
                        }
                        // Found valid version
                        av_header.set_version(hdr_version);
                        ALOGI("%s: header version:%u", __FUNCTION__, hdr_version);
                    }

                    size_t bytes_added = av_header.bytes_hdr_needed() > bytes ? bytes: av_header.bytes_hdr_needed();
                    av_header.add_bytes(buffer, bytes_added);
                    // check if av header has offset
                    size_t offset_aded = 0;
                    if (av_header.bytes_hdr_needed() == 0 && !av_header.is_complete()) {
                        offset_aded = av_header.bytes_offset_needed() > bytes ? bytes : av_header.bytes_offset_needed();
                        av_header.add_offset(offset_aded);
                    }
                    if (!av_header.is_complete() || (bytes == av_header.len())) {
                        // Incomplete header or complete header at the end of the buffer (no payload).
                        ALOGVV("hdr_bytes_needed:%zu, av_hdr_len:%zu, bytes:%zu", av_header.bytes_hdr_needed(), av_header.len(), bytes);
                        bytes_written += bytes;
                        bytes = 0;
                        break;
                    }
                }

                // frame header is at payload start, write the header
                current_buff.set_av_header(av_header);
                writeHeader = true;
                frameBytes = B_MEDIA_LOAD_UINT32_BE(pts_buffer, sizeof(uint32_t));
                uint64_t timestamp = B_MEDIA_LOAD_UINT32_BE(pts_buffer, 2*sizeof(uint32_t));
                timestamp <<= 32;
                timestamp |= B_MEDIA_LOAD_UINT32_BE(pts_buffer, 3*sizeof(uint32_t));
                timestamp /= 1000; // Convert ns -> us
                last_written_ts = timestamp;
                pts = BOMX_TickToPts((OMX_TICKS *)&timestamp);
                ALOGV("%s: av-sync header, ts=%" PRIu64 " ver=%u, pts=%" PRIu32 ", size=%zu, av_header.len()=%zu payload=%zu",
                    __FUNCTION__, timestamp, av_header.version(), pts, frameBytes, av_header.len(), bytes);
                if (init_stc) {
                    NEXUS_SimpleStcChannel_SetStc(bout->nexus.tunnel.stc_channel_sync, pts);
                    ALOGV("%s: stc-sync initialized to:%u", __FUNCTION__, pts);
                    init_stc = false;
                }
                if (!split_header) {
                    bytes -= av_header.len();
                    buffer = (void *)((uint8_t *)buffer + av_header.len());

                    // For E-AC3, DTS, and DTS-HD, parse the frame header to determine the number of PCM samples per frame
                    if (bout->nexus.tunnel.audioblocks_per_frame == 0) {
                        if (bout->config.format == AUDIO_FORMAT_AC3 || bout->config.format == AUDIO_FORMAT_E_AC3) {
                            const uint8_t *syncframe = (const uint8_t *)memmem(buffer, bytes, g_nexus_parse_eac3_syncword, sizeof(g_nexus_parse_eac3_syncword));
                            if (syncframe != NULL) {
                                eac3_frame_hdr_info info;
                                if (nexus_parse_eac3_frame_hdr(syncframe, bytes - (syncframe - (const uint8_t *)buffer), &info)) {
                                    if (bout->config.format == AUDIO_FORMAT_E_AC3) {
                                        bout->nexus.tunnel.audioblocks_per_frame = info.num_audio_blks;
                                        bout->nexus.tunnel.frame_multiplier = nexus_tunnel_bout_get_frame_multipler(bout);
                                        bout->nexus.tunnel.bitrate = info.bitrate;
                                    }
                                    else { // AC3
                                        bout->nexus.tunnel.audioblocks_per_frame = NEXUS_PCM_FRAMES_PER_AC3_FRAME / NEXUS_PCM_FRAMES_PER_AC3_AUDIO_BLOCK;
                                        bout->nexus.tunnel.frame_multiplier = NEXUS_PCM_FRAMES_PER_AC3_FRAME;
                                        bout->nexus.tunnel.bitrate = info.bitrate;
                                    }
                                }
                                else {
                                    ALOGV("%s: Error parsing E-/AC3 sync frame", __FUNCTION__);
                                }
                            }
                        }
                        else if (bout->config.format == AUDIO_FORMAT_DTS || bout->config.format == AUDIO_FORMAT_DTS_HD) {
                            NEXUS_AudioDecoderStatus status;
                            if (NEXUS_SimpleAudioDecoder_GetStatus(audio_decoder, &status) == NEXUS_SUCCESS && status.codecStatus.dts.numPcmBlocks >= 5 && status.codecStatus.dts.numPcmBlocks <= 127) {
                                bout->nexus.tunnel.audioblocks_per_frame = status.codecStatus.dts.numPcmBlocks;
                                bout->nexus.tunnel.frame_multiplier = nexus_tunnel_bout_get_frame_multipler(bout);
                            }
                        }
                        ALOGI_IF(bout->nexus.tunnel.audioblocks_per_frame > 0, "%s: %u blocks detected, fmt:%x mpy:%u bkps:%u", __FUNCTION__, bout->nexus.tunnel.audioblocks_per_frame, bout->config.format, bout->nexus.tunnel.frame_multiplier, bout->nexus.tunnel.bitrate);
                    }
                }
            }
            else
            {
                // frame header is later in the payload, write as many bytes as needed to get there
                frameBytes = (uint8_t *)pts_buffer - (uint8_t *)buffer;
                ALOGV("%s: later av-sync header, %zu bytes to write", __FUNCTION__, frameBytes);
            }
        }
        else
        {
            frameBytes = bytes_segment;
            ALOGV("%s: no av-sync header, %zu bytes to write", __FUNCTION__, frameBytes);
        }

        while ( frameBytes > 0 )
        {
            bmedia_pes_info pes;
            size_t used, waveformat_size;
            size_t spaceRequired, length;

            ret = NEXUS_Playpump_GetBuffer(playpump, &nexus_buffer, &nexus_space);
            if (ret) {
                ALOGE("%s: get playpump buffer failed, ret=%d", __FUNCTION__, ret);
                ret = -ENOSYS;
                break;
            }

            spaceRequired = (frameBytes >= bytes) ? bytes : frameBytes;
            if ( writeHeader ) {
                spaceRequired += BMEDIA_PES_HEADER_MAX_SIZE;
                if (bout->nexus.tunnel.pcm_format) {
                    spaceRequired += BMEDIA_WAVEFORMATEX_BASE_SIZE + bmedia_frame_bcma.len + 4;
                }
            }
            if (nexus_space >= spaceRequired) {
                used = 0;
                if ( writeHeader ) {
                    // Format PES
                    BMEDIA_PES_INFO_INIT(&pes, BRCM_AUDIO_STREAM_ID);
                    BMEDIA_PES_SET_PTS(&pes, pts);
                    length = frameBytes;
                    if (bout->nexus.tunnel.pcm_format) {
                        length += BMEDIA_WAVEFORMATEX_BASE_SIZE + bmedia_frame_bcma.len + 4;
                    }
                    used += bmedia_pes_header_init((uint8_t *)nexus_buffer, length, &pes);
                    if (bout->nexus.tunnel.pcm_format) {
                        BKNI_Memcpy((uint8_t *)nexus_buffer + used, bmedia_frame_bcma.base, bmedia_frame_bcma.len);
                        used += bmedia_frame_bcma.len;
                        B_MEDIA_SAVE_UINT32_BE((uint8_t *)nexus_buffer + used, frameBytes);
                        used += sizeof(uint32_t);

                        // Format WAV header
                        waveformat_size = bmedia_write_waveformatex((uint8_t *)nexus_buffer + used, &bout->nexus.tunnel.wave_fmt);
                        ALOG_ASSERT(waveformat_size == BMEDIA_WAVEFORMATEX_BASE_SIZE);
                        used += waveformat_size;
                    }

                    // Header may indicate more data in frame than was passed in write()
                    if ( frameBytes > bytes ) {
                        frameBytes = bytes;
                    }
                }

                BKNI_Memcpy((uint8_t *)nexus_buffer + used, buffer, frameBytes);
                used += frameBytes;
                BDBG_ASSERT(used <= nexus_space);

                if (bout->nexus.tunnel.pes_debug != NULL) {
                    fwrite(nexus_buffer, 1, used, bout->nexus.tunnel.pes_debug);
                }
                ret = NEXUS_Playpump_WriteComplete(playpump, 0, used);
                if (ret) {
                    ALOGE("%s:%d playpump write failed, ret=%d", __FUNCTION__, __LINE__, ret);
                    break;
                }
                else {
                    bytes_written += frameBytes;
                    current_buff.add_bytes(frameBytes);
                    if ( writeHeader ) {
                        writeHeader = false;
                        if (!split_header) {
                            bytes_written += av_header.len();
                        }
                        av_header.reset();
                    }
                    buffer = (void *)((uint8_t *)buffer + frameBytes);
                    ALOG_ASSERT(bytes >= frameBytes);
                    bytes -= frameBytes;
                    frameBytes = 0;
                }
            }
            else if ((uint8_t *)nexus_buffer + nexus_space >= bout->nexus.tunnel.pp_buffer_end) {
                if (nexus_space > 0) {
                    // Look for a complete contiguous buffer
                    ret = NEXUS_Playpump_WriteComplete(playpump, nexus_space, 0);
                    if (ret) {
                        ALOGE("%s:%d playpump write failed, ret=%d", __FUNCTION__, __LINE__, ret);
                        break;
                    }
                }
            }
            else {
                NEXUS_SimpleAudioDecoderHandle prev_audio_decoder = audio_decoder;
                NEXUS_PlaypumpHandle prev_playpump = playpump;

                if (nexus_space > 0) {
                    // Look for a complete contiguous buffer
                    ret = NEXUS_Playpump_WriteComplete(playpump, nexus_space, 0);
                    if (ret) {
                        ALOGE("%s:%d playpump write failed, ret=%d", __FUNCTION__, __LINE__, ret);
                        break;
                    }
                }

                BKNI_ResetEvent(event);
                pthread_mutex_unlock(&bout->lock);
                ret = BKNI_WaitForEvent(event, 500);
                pthread_mutex_lock(&bout->lock);

                // Sanity check when relocking
                audio_decoder = bout->nexus.tunnel.audio_decoder;
                playpump = bout->nexus.tunnel.playpump;
                ALOG_ASSERT(audio_decoder == prev_audio_decoder);
                ALOG_ASSERT(playpump == prev_playpump);
                ALOG_ASSERT(!bout->suspended);

                if (ret != BERR_SUCCESS) {
                    NEXUS_Playpump_GetStatus(playpump, &ppStatus);
                    ALOGE("%s: playpump write timeout, ret=%d, ns:%zu, fifoS:%zu fifoD:%zu",
                            __FUNCTION__, ret, nexus_space, ppStatus.fifoSize, ppStatus.fifoDepth);
                    ret = -ENOSYS;
                    break;
                }
            }
        }

        if ((ret != 0) && writeHeader && !split_header) {
            bytes += av_header.len();
            buffer = (void *)((uint8_t *)buffer - av_header.len());
        }
    }

done:
    /* Return error if no bytes written */
    if (bytes_written == 0) {
        return ret;
    }

    // Throttle the output to prevent audio underruns
    if (bout->nexus.tunnel.pcm_format) {
        nsecs_t delta = systemTime(SYSTEM_TIME_MONOTONIC) - bout->nexus.tunnel.last_write_time;
        int32_t throttle_us = BRCM_AUDIO_TUNNEL_HALF_DURATION_US - (delta / 1000);
        if (throttle_us <= BRCM_AUDIO_TUNNEL_HALF_DURATION_US && throttle_us > 0) {
            ALOGV("%s: throttle %d us", __FUNCTION__, throttle_us);
            usleep(throttle_us);
        }
        bout->nexus.tunnel.last_write_time = systemTime(SYSTEM_TIME_MONOTONIC);
    }
    else {
        bout->nexus.tunnel.last_write_time = systemTime(SYSTEM_TIME_MONOTONIC);
        ALOGV("%s: prime %d rate %d wr %d", __FUNCTION__, bout->nexus.tunnel.priming, trickState.rate, bytes_written);
    }

    return bytes_written;
}

// returns true if nexus audio can enter standby
static bool nexus_tunnel_bout_standby_monitor(void *context)
{
    bool standby = true;
    struct brcm_stream_out *bout = (struct brcm_stream_out *)context;

    if (bout != NULL) {
        pthread_mutex_lock(&bout->lock);
        standby = (bout->started == false);
        pthread_mutex_unlock(&bout->lock);
    }
    ALOGV("%s: standby=%d", __FUNCTION__, standby);
    return standby;
}

static void nexus_tunnel_bout_playpump_callback(void *context, int param)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)context;
    ALOG_ASSERT(bout->nexus.event == (BKNI_EventHandle)(intptr_t)param);
    BKNI_SetEvent((BKNI_EventHandle)(intptr_t)param);
}

static int nexus_tunnel_bout_open(struct brcm_stream_out *bout)
{
    struct audio_config *config = &bout->config;
    NEXUS_Error rc;
    android::status_t status;
    NxClient_AllocSettings allocSettings;
    NEXUS_SurfaceComposition surfSettings;
    NxClient_ConnectSettings connectSettings;
    NEXUS_PlaypumpSettings playpumpSettings;
    NEXUS_PlaypumpOpenPidChannelSettings pidSettings;
    BKNI_EventHandle event;
    uint32_t audioResourceId;
    int i, ret = 0;
    NEXUS_PlaypumpStatus playpumpStatus;

    /* Check if sample rate is supported */
    for (i = 0; i < (int)NEXUS_OUT_SAMPLE_RATE_COUNT; i++) {
        if (config->sample_rate == nexus_out_sample_rates[i]) {
            break;
        }
    }
    if (i == NEXUS_OUT_SAMPLE_RATE_COUNT) {
        config->sample_rate = NEXUS_OUT_DEFAULT_SAMPLE_RATE;
    }

    if (config->channel_mask == 0) {
        config->channel_mask = NEXUS_OUT_DEFAULT_CHANNELS;
    }
    if (config->format == AUDIO_FORMAT_DEFAULT) {
        config->format = NEXUS_OUT_DEFAULT_FORMAT;
    }

    bout->framesPlayedTotal = 0;
    bout->nexus.tunnel.first_write = false;

    size_t bytes_per_sample = audio_bytes_per_sample(config->format);
    bout->nexus.tunnel.pcm_format = (bytes_per_sample == 0) ? false : true;
    if (bout->nexus.tunnel.pcm_format) {
        bout->frameSize = bytes_per_sample * popcount(config->channel_mask);
        bout->buffer_size = get_brcm_audio_buffer_size(config->sample_rate,
                                                       config->format,
                                                       popcount(config->channel_mask),
                                                       BRCM_AUDIO_TUNNEL_DURATION_MS);
    }
    else {
        /* Frame size should be single byte long for compressed audio formats */
        bout->frameSize = 1;
        bout->buffer_size = BRCM_AUDIO_TUNNEL_COMP_BUFFER_SIZE;
    }

    ALOGV("%s: sample_rate=%" PRIu32 " frameSize=%" PRIu32 " buffer_size=%zu",
            __FUNCTION__, config->sample_rate, bout->frameSize, bout->buffer_size);

    /* Allocate simpleAudioPlayback */
    NxClient_GetDefaultAllocSettings(&allocSettings);
    allocSettings.simpleAudioDecoder = 1;
    rc = NxClient_Alloc(&allocSettings, &(bout->nexus.allocResults));
    if (rc) {
        ALOGE("%s: Cannot allocate NxClient resources, rc:%d", __FUNCTION__, rc);
        ret = -ENOMEM;
        goto err_alloc;
    }

    audioResourceId = bout->nexus.allocResults.simpleAudioDecoder.id;
    bout->nexus.tunnel.audio_decoder = NEXUS_SimpleAudioDecoder_Acquire(audioResourceId);
    if (!bout->nexus.tunnel.audio_decoder) {
        ALOGE("%s: at %d, acquire Nexus simple audio decoder handle failed\n",
             __FUNCTION__, __LINE__);
        ret = -ENOMEM;
        goto err_acquire;
    }

    NxClient_GetDefaultConnectSettings(&connectSettings);
    connectSettings.simpleAudioDecoder.id = audioResourceId;

    if (bout->bdev->dolby_ms == 12) {
        connectSettings.simpleAudioDecoder.decoderCapabilities.type = NxClient_AudioDecoderType_ePersistent;
    }

    rc = NxClient_Connect(&connectSettings, &(bout->nexus.connectId));
    if (rc) {
        ALOGE("%s: error calling NxClient_Connect, rc:%d", __FUNCTION__, rc);
        ret = -ENOSYS;
        goto err_acquire;
    }

    ret = BKNI_CreateEvent(&event);
    if (ret) {
        ALOGE("%s: at %d, create event failed, ret = %d\n",
             __FUNCTION__, __LINE__, ret);
        ret = -ENOMEM;
        goto err_event;
    }

    // register standby callback
    bout->standbyCallback = bout->bdev->standbyThread->RegisterCallback(nexus_tunnel_bout_standby_monitor, bout);
    if (bout->standbyCallback < 0) {
        ALOGE("Error registering standby callback");
        ret = -ENOSYS;
        goto err_callback;
    }

    size_t fifoSize;
    NEXUS_PlaypumpOpenSettings playpumpOpenSettings;
    NEXUS_Playpump_GetDefaultOpenSettings(&playpumpOpenSettings);
    fifoSize = bout->buffer_size * BRCM_AUDIO_TUNNEL_FIFO_MULTIPLIER;
    ALOGV("Playpump fifo size %zu->%zu", playpumpOpenSettings.fifoSize, fifoSize);
    playpumpOpenSettings.fifoSize = fifoSize;
    bout->nexus.tunnel.playpump = NEXUS_Playpump_Open(NEXUS_ANY_ID, &playpumpOpenSettings);
    if (!bout->nexus.tunnel.playpump) {
        ALOGE("%s: Error opening playpump", __FUNCTION__);
        ret = -ENOMEM;
        goto err_playpump;
    }

    NEXUS_Playpump_GetSettings(bout->nexus.tunnel.playpump, &playpumpSettings);
    playpumpSettings.transportType = NEXUS_TransportType_eMpeg2Pes;
    playpumpSettings.dataCallback.callback = nexus_tunnel_bout_playpump_callback;
    playpumpSettings.dataCallback.context = bout;
    playpumpSettings.dataCallback.param = (int)(intptr_t)event;
    rc = NEXUS_Playpump_SetSettings(bout->nexus.tunnel.playpump, &playpumpSettings);
    if (rc) {
        ALOGE("%s: Error setting playpump settings, rc:%d", __FUNCTION__, rc);
        ret = -ENOSYS;
        goto err_playpump_set;
    }

    rc = NEXUS_Playpump_GetStatus(bout->nexus.tunnel.playpump, &playpumpStatus);
    if (rc) {
        ALOGE("%s: Error getting playpump status, rc:%d", __FUNCTION__, rc);
        ret = -ENOSYS;
        goto err_playpump_set;
    }
    bout->nexus.tunnel.pp_buffer_end = (uint8_t *)playpumpStatus.bufferBase + playpumpStatus.fifoDepth;

    NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&pidSettings);
    pidSettings.pidType = NEXUS_PidType_eAudio;
    bout->nexus.tunnel.pid_channel = NEXUS_Playpump_OpenPidChannel(bout->nexus.tunnel.playpump, BRCM_AUDIO_STREAM_ID, &pidSettings);
    if (!bout->nexus.tunnel.pid_channel) {
        ALOGE("%s: Error openning pid channel", __FUNCTION__);
        ret = -ENOMEM;
        goto err_pid;
    }

    ALOG_ASSERT(bout->nexus.tunnel.stc_channel != NULL);
    NEXUS_SimpleAudioDecoder_SetStcChannel(bout->nexus.tunnel.audio_decoder, bout->nexus.tunnel.stc_channel);

    if (bout->nexus.tunnel.pcm_format) {
        // Initialize WAV format parameters
        bmedia_waveformatex_header *wave_fmt = &bout->nexus.tunnel.wave_fmt;
        bmedia_init_waveformatex(wave_fmt);
        wave_fmt->wFormatTag = 0x0001;  // PCM
        wave_fmt->nChannels = popcount(bout->config.channel_mask);
        wave_fmt->nSamplesPerSec = bout->config.sample_rate;
        wave_fmt->wBitsPerSample = audio_bytes_per_sample(bout->config.format) * 8;
        wave_fmt->nBlockAlign = (wave_fmt->nChannels * wave_fmt->wBitsPerSample) / 8;
        wave_fmt->cbSize = 0;
        wave_fmt->nAvgBytesPerSec = (wave_fmt->wBitsPerSample * wave_fmt->nSamplesPerSec * wave_fmt->nChannels) / 8;
        ALOGV("%s: wave_fmt channel=%d sample_rate=%" PRId32 " bit_per_sample=%d align=%d avg_bytes_rate=%" PRId32 "",
                __FUNCTION__, wave_fmt->nChannels, wave_fmt->nSamplesPerSec, wave_fmt->wBitsPerSample,
                wave_fmt->nBlockAlign, wave_fmt->nAvgBytesPerSec);
    }

    bout->nexus.event = event;
    bout->nexus.state = BRCM_NEXUS_STATE_CREATED;
    av_header.reset();
    current_buff.reset();

    bout->nexus.tunnel.no_debounce = property_get_bool(BCM_RO_AUDIO_TUNNEL_NO_DEBOUNCE, true);
    bout->nexus.tunnel.soft_muting = property_get_int32(BCM_RO_AUDIO_SOFT_MUTING, 10);
    bout->nexus.tunnel.soft_unmuting = property_get_int32(BCM_RO_AUDIO_SOFT_UNMUTING, 30);
    bout->nexus.tunnel.sleep_after_mute = property_get_int32(BCM_RO_AUDIO_SLEEP_AFTER_MUTE, 30);

    if (property_get_int32(BCM_RO_AUDIO_TUNNEL_PROPERTY_PES_DEBUG, 0)) {
        time_t rawtime;
        struct tm * timeinfo;
        char fname[100];

        // Generate unique filename
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        snprintf(fname, sizeof(fname), "/data/vendor/nxmedia/aout-tunnel-dev_%u-", bout->devices);
        strftime(fname+strlen(fname), sizeof(fname)-strlen(fname), "%F_%H_%M_%S.pes", timeinfo);
        ALOGD("Tunneled audio PES debug output file:%s", fname);
        bout->nexus.tunnel.pes_debug = fopen(fname, "wb+");
        if (bout->nexus.tunnel.pes_debug == NULL) {
            ALOGW("Error creating tunneled audio PES output debug file %s (%s)", fname, strerror(errno));
            // Just keep going without debug
        }
    }
    else {
        bout->nexus.tunnel.pes_debug = NULL;
    }

    // Restore auto mode for MS11
    if ((bout->bdev->dolby_ms == 11) && !bout->nexus.tunnel.pcm_format) {
        NxClient_AudioSettings audioSettings;

        ALOGI("Force auto output");
        NxClient_GetAudioSettings(&audioSettings);
        audioSettings.hdmi.outputMode = NxClient_AudioOutputMode_eAuto;
        audioSettings.spdif.outputMode = NxClient_AudioOutputMode_eAuto;
        ret = NxClient_SetAudioSettings(&audioSettings);
        if (ret) {
            ALOGE("%s: Error setting auto mode, ret = %d", __FUNCTION__, ret);
        }
    }

    bout->nexus.tunnel.fadeLevel = 100;
    if (bout->nexus.tunnel.pcm_format) {
        bout->nexus.tunnel.deferred_volume = false;
        bout->nexus.tunnel.deferred_volume_ms = 0;
    } else {
        bout->nexus.tunnel.deferred_volume = true;
        bout->nexus.tunnel.deferred_volume_ms = 0;
    }

    return 0;

err_pid:
    bout->nexus.tunnel.stc_channel = NULL;
err_playpump_set:
    NEXUS_Playpump_Close(bout->nexus.tunnel.playpump);
    bout->nexus.tunnel.playpump = NULL;
err_playpump:
    bout->bdev->standbyThread->UnregisterCallback(bout->standbyCallback);
err_callback:
    BKNI_DestroyEvent(event);
err_event:
    NxClient_Disconnect(bout->nexus.connectId);
err_acquire:
    NxClient_Free(&(bout->nexus.allocResults));
err_alloc:
    return ret;
}

static int nexus_tunnel_bout_close(struct brcm_stream_out *bout)
{
    BKNI_EventHandle event = bout->nexus.event;

    if (bout->nexus.state == BRCM_NEXUS_STATE_DESTROYED) {
       return 0;
    }

    // Force PCM mode for MS11
    if ((bout->bdev->dolby_ms == 11) && !bout->nexus.tunnel.pcm_format) {
        NxClient_AudioSettings audioSettings;

        ALOGI("Force PCM output");
        NxClient_GetAudioSettings(&audioSettings);
        audioSettings.hdmi.outputMode = NxClient_AudioOutputMode_ePcm;
        audioSettings.spdif.outputMode = NxClient_AudioOutputMode_ePcm;
        NxClient_SetAudioSettings(&audioSettings);
    }

    nexus_tunnel_bout_stop(bout);

    if (bout->nexus.tunnel.pid_channel) {
        ALOG_ASSERT(bout->nexus.tunnel.playpump != NULL);
        NEXUS_Playpump_ClosePidChannel(bout->nexus.tunnel.playpump, bout->nexus.tunnel.pid_channel);
        bout->nexus.tunnel.pid_channel = NULL;
    }

    if (bout->nexus.tunnel.playpump) {
        NEXUS_Playpump_Close(bout->nexus.tunnel.playpump);
        bout->nexus.tunnel.playpump = NULL;
    }

    bout->bdev->standbyThread->UnregisterCallback(bout->standbyCallback);

    if (event) {
        BKNI_DestroyEvent(event);
    }

    if (bout->nexus.tunnel.audio_decoder) {
        NEXUS_SimpleAudioDecoder_Release(bout->nexus.tunnel.audio_decoder);
        bout->nexus.tunnel.audio_decoder = NULL;
    }

    NxClient_Disconnect(bout->nexus.connectId);
    NxClient_Free(&(bout->nexus.allocResults));
    bout->nexus.state = BRCM_NEXUS_STATE_DESTROYED;

    if (bout->nexus.tunnel.pes_debug) {
        fclose(bout->nexus.tunnel.pes_debug);
        bout->nexus.tunnel.pes_debug = NULL;
    }

    return 0;
}

static char *nexus_tunnel_bout_get_parameters (struct brcm_stream_out *bout, const char *keys)
{
    struct str_parms *query = str_parms_create_str(keys);
    struct str_parms *result = str_parms_create();
    String8 rates_str, channels_str, formats_str;
    char* result_str;

    /* Get connected HDMI status */
    nexus_get_hdmi_parameters(rates_str, channels_str, formats_str);

    pthread_mutex_lock(&bout->lock);
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_HW_AV_SYNC)) {
        str_parms_add_int(result, AUDIO_PARAMETER_STREAM_HW_AV_SYNC,
                          (int)(intptr_t)bout->nexus.tunnel.stc_channel);
    }

    /* Supported sample rates */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES,
                          rates_str.isEmpty() ? TOSTRING(NEXUS_OUT_DEFAULT_SAMPLE_RATE) : rates_str.string());
    }

    /* Supported channel counts */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_CHANNELS,
                          channels_str.isEmpty() ? TOSTRING(NEXUS_OUT_DEFAULT_CHANNELS) : channels_str.string());
    }

    /* Supported sample formats */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        if (formats_str.contains("AUDIO_FORMAT_AC3") && !formats_str.contains("AUDIO_FORMAT_E_AC3")) {
            NEXUS_AudioCapabilities audioCaps;
            NEXUS_GetAudioCapabilities(&audioCaps);
            if (audioCaps.dsp.codecs[NEXUS_AudioCodec_eAc3Plus].decode) {
                formats_str.append("|AUDIO_FORMAT_E_AC3");
            }
        }
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_FORMATS,
                          formats_str.isEmpty() ? TOSTRING(NEXUS_OUT_DEFAULT_FORMAT) : formats_str.string());
    }
    pthread_mutex_unlock(&bout->lock);

    result_str = str_parms_to_str(result);
    str_parms_destroy(query);
    str_parms_destroy(result);

    ALOGV("%s: result = %s", __FUNCTION__, result_str);

    return result_str;
}

static int nexus_tunnel_bout_dump(struct brcm_stream_out *bout, int fd)
{
   dprintf(fd, "\nnexus_tunnel_bout_dump:\n"
            "\tstc-channel = %p\n"
            "\tpid-channel = %p\n"
            "\tplaypump    = %p\n"
            "\tdecoder     = %p\n",
            (void *)bout->nexus.tunnel.stc_channel,
            (void *)bout->nexus.tunnel.pid_channel,
            (void *)bout->nexus.tunnel.playpump,
            (void *)bout->nexus.tunnel.audio_decoder);

   return 0;
}

struct brcm_stream_out_ops nexus_tunnel_bout_ops = {
    .do_bout_open = nexus_tunnel_bout_open,
    .do_bout_close = nexus_tunnel_bout_close,
    .do_bout_get_latency = nexus_tunnel_bout_get_latency,
    .do_bout_start = nexus_tunnel_bout_start,
    .do_bout_stop = nexus_tunnel_bout_stop,
    .do_bout_write = nexus_tunnel_bout_write,
    .do_bout_set_volume = nexus_tunnel_bout_set_volume,
    .do_bout_get_render_position = nexus_tunnel_bout_get_render_position,
    .do_bout_get_presentation_position = nexus_tunnel_bout_get_presentation_position,
    .do_bout_dump = nexus_tunnel_bout_dump,
    .do_bout_get_parameters = nexus_tunnel_bout_get_parameters,
    .do_bout_pause = nexus_tunnel_bout_pause,
    .do_bout_resume = nexus_tunnel_bout_resume,
    .do_bout_drain = nexus_tunnel_bout_drain,
    .do_bout_flush = nexus_tunnel_bout_flush,
};


NEXUS_Error nexus_tunnel_alloc_stc_mem_hdl(NEXUS_MemoryBlockHandle *hdl)
{
    NEXUS_Error nx_rc;
    NEXUS_ClientConfiguration client_config;
    NEXUS_HeapHandle global_heap;
    NEXUS_MemoryBlockHandle mem_block_hdl = NULL;
    void *memory_ptr;
    stc_channel_st *channel_st_ptr;
    NEXUS_SimpleStcChannelSettings stc_channel_settings;

    NEXUS_Platform_GetClientConfiguration(&client_config);
    global_heap = client_config.heap[NXCLIENT_FULL_HEAP];
    if (global_heap == NULL) {
        global_heap = client_config.heap[NXCLIENT_DEFAULT_HEAP];
        if (global_heap == NULL) {
           ALOGE("%s: error accessing main heap", __FUNCTION__);
           return NEXUS_OUT_OF_DEVICE_MEMORY;
        }
    }
    mem_block_hdl = NEXUS_MemoryBlock_Allocate(global_heap, sizeof(stc_channel_st), 16, NULL);
    if (mem_block_hdl == NULL) {
        ALOGE("%s: unable to allocate memory block", __FUNCTION__);
        return NEXUS_OUT_OF_DEVICE_MEMORY;
    }
    memory_ptr = NULL;
    nx_rc = NEXUS_MemoryBlock_Lock(mem_block_hdl, &memory_ptr);
    if (nx_rc || memory_ptr == NULL) {
        ALOGE("%s: unable to lock global memory block", __FUNCTION__);
        if (!nx_rc)
            nx_rc = NEXUS_OUT_OF_DEVICE_MEMORY;
        NEXUS_MemoryBlock_Free(mem_block_hdl);
        return nx_rc;
    }

    channel_st_ptr = (stc_channel_st*)memory_ptr;
    memset(channel_st_ptr, 0, sizeof(*channel_st_ptr));

    // create stc channels
    NEXUS_SimpleStcChannel_GetDefaultSettings(&stc_channel_settings);
    stc_channel_settings.modeSettings.Auto.behavior = NEXUS_StcChannelAutoModeBehavior_eAudioMaster;
    stc_channel_settings.sync = NEXUS_SimpleStcChannelSyncMode_eAudioAdjustmentConcealment;
    channel_st_ptr->stc_channel = NEXUS_SimpleStcChannel_Create(&stc_channel_settings);
    NEXUS_Platform_SetSharedHandle(channel_st_ptr->stc_channel, true);

    NEXUS_SimpleStcChannel_GetDefaultSettings(&stc_channel_settings);
    stc_channel_settings.mode = NEXUS_StcChannelMode_eHost;
    channel_st_ptr->stc_channel_sync = NEXUS_SimpleStcChannel_Create(&stc_channel_settings);
    NEXUS_Platform_SetSharedHandle(channel_st_ptr->stc_channel_sync, true);
    NEXUS_SimpleStcChannel_SetStc(channel_st_ptr->stc_channel_sync, BRCM_AUDIO_TUNNEL_STC_SYNC_INVALID);
    ALOGV("%s: stc-channels: [%p %p]", __FUNCTION__,
            channel_st_ptr->stc_channel, channel_st_ptr->stc_channel_sync);

    *hdl = mem_block_hdl;
    NEXUS_MemoryBlock_Unlock(mem_block_hdl);
    return NEXUS_SUCCESS;
}

void nexus_tunnel_release_stc_mem_hdl(NEXUS_MemoryBlockHandle *hdl)
{
    void *memory_ptr;
    NEXUS_Error nx_rc;
    stc_channel_st *channel_st_ptr;

    if (*hdl == NULL)
        return;

    memory_ptr = NULL;
    nx_rc = NEXUS_MemoryBlock_Lock(*hdl, &memory_ptr);
    if (nx_rc || memory_ptr == NULL) {
        ALOGW("%s: unable to lock handle", __FUNCTION__);
        NEXUS_MemoryBlock_Free(*hdl);
        *hdl = NULL;
        return;
    }
    channel_st_ptr = (stc_channel_st*)memory_ptr;
    NEXUS_Platform_SetSharedHandle(channel_st_ptr->stc_channel, false);
    NEXUS_SimpleStcChannel_Destroy(channel_st_ptr->stc_channel);
    NEXUS_Platform_SetSharedHandle(channel_st_ptr->stc_channel_sync, false);
    NEXUS_SimpleStcChannel_Destroy(channel_st_ptr->stc_channel_sync);

    NEXUS_MemoryBlock_Unlock(*hdl);
    NEXUS_MemoryBlock_Free(*hdl);
    *hdl = NULL;
}

void nexus_tunnel_lock_stc_mem_hdl(NEXUS_MemoryBlockHandle hdl, stc_channel_st **stc_st)
{
    void *memory_ptr;
    NEXUS_Error nx_rc;
    stc_channel_st *channel_st_ptr;

    if (hdl == NULL)
        return;

    memory_ptr = NULL;
    *stc_st = NULL;
    nx_rc = NEXUS_MemoryBlock_Lock(hdl, &memory_ptr);
    if (nx_rc || memory_ptr == NULL) {
        ALOGW("%s: unable to lock handle", __FUNCTION__);
        return;
    }
    channel_st_ptr = (stc_channel_st*)memory_ptr;
    *stc_st = channel_st_ptr;
}

void nexus_tunnel_unlock_stc_mem_hdl(NEXUS_MemoryBlockHandle hdl)
{
    NEXUS_MemoryBlock_Unlock(hdl);
}
