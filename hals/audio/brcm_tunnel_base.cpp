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
#include "OMX_Types.h"
#include "bomx_utils.h"
#include <inttypes.h>

#define MAX_TS_DELTA                            10000000

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
    // resets length/offset. Use this function when parsing a stream once the header
    // version has been locked
    void reset() {
        length = 0;
        offset = 0;
    }
    // clears all data, including the header version. Use this function only when
    // opening/closing an output stream.
    void clear() {
        reset();
        hdr_length = 0;
        hdr_version = 0;
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
        size_t frame_bytes = frame_len();
        return (frame_bytes > bytes_written) ? frame_bytes - bytes_written : 0;
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
static int tunnel_base_bout_set_volume(struct brcm_stream_out *bout,
                                 float left, float right)
{
    return bout->tunnel_base.sink.set_volume(bout, left, right);
}

static int tunnel_base_bout_get_render_position(struct brcm_stream_out *bout, uint32_t *dsp_frames)
{
    return bout->tunnel_base.sink.get_render_position(bout, dsp_frames);
}

static int tunnel_base_bout_get_presentation_position(struct brcm_stream_out *bout, uint64_t *frames)
{
    return bout->tunnel_base.sink.get_presentation_position(bout, frames);
}

static uint32_t tunnel_base_bout_get_latency(struct brcm_stream_out *bout)
{
    return bout->tunnel_base.sink.get_latency(bout);
}

static int tunnel_base_bout_start(struct brcm_stream_out *bout)
{
    return bout->tunnel_base.sink.start(bout);
}

static int tunnel_base_bout_stop(struct brcm_stream_out *bout)
{
    av_header.clear();
    current_buff.reset();
    return bout->tunnel_base.sink.stop(bout);
}

static int tunnel_base_bout_pause(struct brcm_stream_out *bout)
{
    return bout->tunnel_base.sink.pause(bout);
}

static int tunnel_base_bout_resume(struct brcm_stream_out *bout)
{
    return bout->tunnel_base.sink.resume(bout);
}

static int tunnel_base_bout_drain(struct brcm_stream_out *bout, int action)
{
    return bout->tunnel_base.sink.drain(bout, action);
}

static int tunnel_base_bout_flush(struct brcm_stream_out *bout)
{
    return bout->tunnel_base.sink.flush(bout);
}

static int tunnel_base_bout_write(struct brcm_stream_out *bout, const void* buffer, size_t bytes)
{
    size_t bytes_written = 0;
    int ret = 0;
    void *sink_buffer, *pts_buffer;
    uint32_t pts=0;
    size_t sink_space;

    if (!bout->tunnel_base.sink.is_ready(bout)) {
        ALOGE("%s: not ready to output audio samples", __FUNCTION__);
        return -ENOSYS;
    }

    if (!av_header.is_empty() && !av_header.is_complete()) {
        BA_LOG(TUN_WRITE, "%s: av_header.len:%zu, bytes:%zu",
                __FUNCTION__, av_header.len(), bytes);
        size_t bytes_added = 0;
        if (av_header.bytes_hdr_needed() > 0) {
            bytes_added = av_header.bytes_hdr_needed() > bytes ?  bytes : av_header.bytes_hdr_needed();
            av_header.add_bytes(buffer, bytes_added);
        }
        // check if av header has offset
        size_t offset_added = 0;
        if (av_header.bytes_hdr_needed() == 0 && !av_header.is_complete()) {
            offset_added = av_header.bytes_offset_needed() > bytes ? bytes : av_header.bytes_offset_needed();
            av_header.add_offset(offset_added);
        }
        bytes_written += bytes_added + offset_added;
        bytes -= bytes_added + offset_added;
        buffer = (void *)((uint8_t *)buffer + bytes_added + offset_added);
    }

    while ( bytes > 0 )
    {
        bool new_frame = false;
        size_t frame_bytes;
        size_t offset;
        bool split_header = false;
        size_t bytes_segment = bytes;

        ret = bout->tunnel_base.sink.pace(bout);
        if (ret == -ETIMEDOUT) {
            ret = 0;
            goto done;
        } else if (ret == -EAGAIN) {
            continue;
        } else if (ret != 0) {
            return ret;
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
                BA_LOG(VERB, "%s: frame writing in progress, bytes_pending:%zu", __FUNCTION__, offset);
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
                        BA_LOG(VERB, "%s: found header at expected offset:%d, bytes:%zu",
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
                    BA_LOG(VERB, "%s: marker at offset:%zu, bytes:%zu",
                           __FUNCTION__, ((uint8_t*)pts_buffer - (uint8_t*)buffer), bytes);
                    if (bytes < min_bytes_for_sync) {
                        // We can't verify if this is a valid header as we can't even see the version.
                        // Stop processing the header and wait for the framework to send us a full one
                        // on the next buffer
                        BA_LOG(TUN_WRITE, "%s: found apparent header marker at end of buffer", __FUNCTION__);
                        pts_buffer = NULL;
                        bytes_segment = bytes = 0;
                        break;
                    } else {
                        uint8_t *pts_buffer_u8 = (uint8_t*)pts_buffer;
                        unsigned hdr_version = pts_buffer_u8[2] << 8 | pts_buffer_u8[3];
                        if (hdr_version != av_header.version()) {
                            // we found data which looks like an av_sync header but it isn't
                            BA_LOG(TUN_WRITE, "spurious header, hdr_version:0x%02X,0x%02X",
                                      pts_buffer_u8[2], pts_buffer_u8[3]);
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
                            ALOGW("%s: skipping malformed initial header, bytes:%zu, hdr_version:%u",
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
                    size_t offset_added = 0;
                    if (av_header.bytes_hdr_needed() == 0 && !av_header.is_complete()) {
                        offset_added = av_header.bytes_offset_needed() > bytes ? bytes : av_header.bytes_offset_needed();
                        av_header.add_offset(offset_added);
                    }
                    if (!av_header.is_complete() || (bytes == av_header.len())) {
                        // Incomplete header or complete header at the end of the buffer (no payload).
                        BA_LOG(VERB, "hdr_bytes_needed:%zu, av_hdr_len:%zu, bytes:%zu",
                                      av_header.bytes_hdr_needed(), av_header.len(), bytes);
                        bytes_written += bytes;
                        bytes = 0;
                        break;
                    }
                }

                // frame header is at payload start, write the header
                current_buff.set_av_header(av_header);
                new_frame = true;
                frame_bytes = B_MEDIA_LOAD_UINT32_BE(pts_buffer, sizeof(uint32_t));
                uint64_t timestamp = B_MEDIA_LOAD_UINT32_BE(pts_buffer, 2*sizeof(uint32_t));
                timestamp <<= 32;
                timestamp |= B_MEDIA_LOAD_UINT32_BE(pts_buffer, 3*sizeof(uint32_t));
                timestamp /= 1000; // Convert ns -> us
                pts = BOMX_TickToPts((OMX_TICKS *)&timestamp);
                BA_LOG(TUN_WRITE, "%s: av-sync header, ts=%" PRIu64 " ver=%u, pts=%" PRIu32
                                    ", size=%zu, av_header.len()=%zu payload=%zu",
                                    __FUNCTION__, timestamp, av_header.version(), pts, frame_bytes, av_header.len(), bytes);
                if (!bout->tunnel_base.started) {
                    if ((bout->tunnel_base.last_written_ts == UINT64_MAX) ||
                        ((int64_t)timestamp - (int64_t)bout->tunnel_base.last_written_ts < 0) ||
                        ((int64_t)timestamp - (int64_t)bout->tunnel_base.last_written_ts > MAX_TS_DELTA)) {
                        // Valid PTS detected.  Start!
                        ALOGI("%s: start, ts=%" PRIu64 " -> %" PRIu64 " ver=%u, pts=%" PRIu32 ", size=%zu, av_header.len()=%zu payload=%zu",
                            __FUNCTION__, bout->tunnel_base.last_written_ts, timestamp, av_header.version(), pts, frame_bytes, av_header.len(), bytes);
                        bout->tunnel_base.last_written_ts = timestamp;
                        ret = bout->tunnel_base.sink.set_start_pts(bout, pts);
                        if (ret != 0) {
                            ALOGE("%s: failed to start, ret:%d", __FUNCTION__, ret);
                            bout->tunnel_base.started = false;
                            return -ENOSYS;
                        }
                    }
                } else {
                    bout->tunnel_base.last_written_ts = timestamp;
                }

                if (!split_header) {
                    bytes -= av_header.len();
                    buffer = (void *)((uint8_t *)buffer + av_header.len());
                }
            }
            else
            {
                // frame header is later in the payload, write as many bytes as needed to get there
                frame_bytes = (uint8_t *)pts_buffer - (uint8_t *)buffer;
                BA_LOG(TUN_WRITE, "%s: later av-sync header, %zu bytes to write", __FUNCTION__, frame_bytes);
            }
        }
        else
        {
            frame_bytes = bytes_segment;
            BA_LOG(TUN_WRITE, "%s: no av-sync header, %zu bytes to write", __FUNCTION__, frame_bytes);
        }

        // Throw away frame before starting
        if (!bout->tunnel_base.started) {
            bytes_written = (frame_bytes > bytes_segment) ? bytes_segment : frame_bytes;
            if ( new_frame ) {
                av_header.reset();
            }
            BA_LOG(TUN_WRITE, "%s: %s av-sync header, %zu bytes before start",
                   __FUNCTION__, new_frame? "cleared":"no", bytes_written);
            ret = 0;
            goto done;
        }
        ALOG_ASSERT(bout->tunnel_base.started);

        // write bytes to sink, add a valid header when sending beginning of a frame
        brcm_frame_header frame_header;
        if (new_frame) {
            frame_header.pts = pts;
            frame_header.frame_length = frame_bytes;
        }
        if (frame_bytes > bytes)
            frame_bytes = bytes;

        ret = bout->tunnel_base.sink.write(bout, buffer, frame_bytes, new_frame ? &frame_header : NULL);
        BA_LOG(TUN_WRITE, "sink write, frame_bytes:%d, ret:%d", frame_bytes, ret);
        if (ret > 0) {
            bytes_written += frame_bytes;
            current_buff.add_bytes(frame_bytes);
            if ( new_frame ) {
                new_frame = false;
                if (!split_header) {
                    bytes_written += av_header.len();
                }
                av_header.reset();
            }
            buffer = (void *)((uint8_t *)buffer + frame_bytes);
            ALOG_ASSERT(bytes >= frame_bytes);
            bytes -= frame_bytes;
            frame_bytes = 0;
        } else if ((ret != 0) && new_frame && !split_header) {
            bytes += av_header.len();
            buffer = (void *)((uint8_t *)buffer - av_header.len());
        }
    }

done:
    /* Return error if no bytes written */
    if (bytes_written == 0) {
        return ret;
    }

    bout->tunnel_base.sink.throttle(bout);

    return bytes_written;
}

static int tunnel_base_bout_open(struct brcm_stream_out *bout)
{
    int ret;

    BA_LOG_INIT();
    // When additional sinks are implemented, the choice must be made
    // based on the selected audio device
    bout->tunnel_base.sink = nexus_tunnel_sink_ops;
    ret = bout->tunnel_base.sink.open(bout);
    if (ret != 0)
        return ret;

    av_header.clear();
    current_buff.reset();
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
        bout->tunnel_base.pes_debug = fopen(fname, "wb+");
        if (bout->tunnel_base.pes_debug == NULL) {
            ALOGW("Error creating tunneled audio PES output debug file %s (%s)", fname, strerror(errno));
            // Just keep going without debug
        }
    }
    else {
        bout->tunnel_base.pes_debug = NULL;
    }

    bout->tunnel_base.last_written_ts = UINT64_MAX;
    ALOGD("Opened tunnel base stream");

    return 0;
}

static int tunnel_base_bout_close(struct brcm_stream_out *bout)
{
    return bout->tunnel_base.sink.close(bout);
}

static char *tunnel_base_bout_get_parameters (struct brcm_stream_out *bout, const char *keys)
{
    return bout->tunnel_base.sink.get_parameters(bout, keys);
}

static int tunnel_base_bout_dump(struct brcm_stream_out *bout, int fd)
{
    return bout->tunnel_base.sink.dump(bout, fd);
}

struct brcm_stream_out_ops brcm_tunnel_bout_ops = {
    .do_bout_open = tunnel_base_bout_open,
    .do_bout_close = tunnel_base_bout_close,
    .do_bout_get_latency = tunnel_base_bout_get_latency,
    .do_bout_start = tunnel_base_bout_start,
    .do_bout_stop = tunnel_base_bout_stop,
    .do_bout_write = tunnel_base_bout_write,
    .do_bout_set_volume = tunnel_base_bout_set_volume,
    .do_bout_get_render_position = tunnel_base_bout_get_render_position,
    .do_bout_get_presentation_position = tunnel_base_bout_get_presentation_position,
    .do_bout_dump = tunnel_base_bout_dump,
    .do_bout_get_parameters = tunnel_base_bout_get_parameters,
    .do_bout_pause = tunnel_base_bout_pause,
    .do_bout_resume = tunnel_base_bout_resume,
    .do_bout_drain = tunnel_base_bout_drain,
    .do_bout_flush = tunnel_base_bout_flush,
};


