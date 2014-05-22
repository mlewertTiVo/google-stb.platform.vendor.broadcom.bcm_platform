/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "ContainersExtractor"
#define LOG_NDEBUG 0
#include <utils/Log.h>

#ifndef LOGE
#define LOGE(s, ...) ALOGE("[%s] " s, __FUNCTION__, ## __VA_ARGS__)
#define LOGD(s, ...) ALOGD("[%s] " s, __FUNCTION__, ## __VA_ARGS__)
#define LOGV(s, ...) ALOGV("[%s] " s, __FUNCTION__, ## __VA_ARGS__)
#endif

#include "ContainersExtractor.h"
#include "MediaCircularBufferGroup.h"
#include "CodecSpecific.h"

#include "containers/containers.h"
#include "containers/core/containers_io.h"
#include "containers/core/containers_logging.h"
#include "containers/containers_codecs.h"

#include <utils/String8.h>
#include <Utils.h>
#include <DataSource.h>
#include <MediaSource.h>
#include <MediaDefs.h>
#include <MetaData.h>
#include <MediaErrors.h>
#include <MediaBuffer.h>
#include <MediaBufferGroup.h>
#include <ADebug.h>

#include <utils/Errors.h>

// Enable verbose logging
#define ENABLE_EXTRA_LOGGING 0

// Limit for the amount of audio frames packing we can do
#define AUDIO_PACKING_MAX_SIZE 4*1024

///////////////////////////////////////////////////////////////////////////////
// Local prototypes
///////////////////////////////////////////////////////////////////////////////
namespace android {
static const char *MEDIA_MIMETYPE_UNKNOWN = "application/octet-stream";
static const char *codec_to_mime(VC_CONTAINER_FOURCC_T codec,
   VC_CONTAINER_FOURCC_T variant);
static VC_CONTAINER_FOURCC_T mime_to_codec(const char *mime,
   VC_CONTAINER_FOURCC_T *variant);
}

///////////////////////////////////////////////////////////////////////////////
// Client I/O wrapper functions for the container API
///////////////////////////////////////////////////////////////////////////////
typedef struct VC_CONTAINER_IO_MODULE_T {
    android::sp<android::DataSource> source;
    off64_t offset;

} VC_CONTAINER_IO_MODULE_T;

static size_t io_read(VC_CONTAINER_IO_T *io, void *buf, size_t size) {
   uint8_t *buffer = (uint8_t *)buf;
   size_t bytes = 0;

   ssize_t ret = io->module->source->readAt(io->module->offset, buffer, size);
   if (ret <= 0) {
       if (!bytes) {
           if (ret == 0 || ret == android::ERROR_END_OF_STREAM)
               io->status = VC_CONTAINER_ERROR_EOS;
           else
               io->status = VC_CONTAINER_ERROR_FAILED;
       }
       return bytes;
   }
   io->module->offset += ret;
   buffer += ret;
   bytes += ret;
   size -= ret;

   return bytes;
}

static VC_CONTAINER_STATUS_T io_seek(VC_CONTAINER_IO_T *io, int64_t offset) {
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;

   io->module->offset = offset;
   io->status = status;
   return status;
}

static VC_CONTAINER_STATUS_T io_close( VC_CONTAINER_IO_T *ctx ) {
   return VC_CONTAINER_SUCCESS;
}

namespace android {

///////////////////////////////////////////////////////////////////////////////
// ContainerReader Class
//
// The extractor lifetime is short - just long enough to get
// the media sources constructed - so we keep the context shared
// by the media sources into its own class.
///////////////////////////////////////////////////////////////////////////////
class ContainerReader : public RefBase {
public:
    ContainerReader(const sp<DataSource> &source);

    bool open(MetaData *fileMetaData);

    size_t trackCount();
    sp<MetaData> trackGetMetaData(size_t index, uint32_t flags);

    void seek(size_t index, int64_t offset, MediaSource::ReadOptions::SeekMode mode);
    status_t read(MediaBuffer **buffer, size_t index);
    void readPacket(uint32_t track);

    status_t trackStart(size_t index, MetaData *params);
    status_t trackStop(size_t index);

    struct Track {
        Track(VC_CONTAINER_ES_FORMAT_T *format, unsigned int index, unsigned int _id) :
            next(0), mIndex(index), id(_id), meta(new MetaData), mGroup(0),
            mBufferCanBeSent(0), mEnabled(false), mIgnoreSeek(false), codec(format) {}

        Track *next;
        unsigned int mIndex;
        unsigned int id;
        sp<MetaData> meta;

        MediaCircularBufferGroup *mGroup;

        bool mBufferCanBeSent;

        bool mEnabled;
        bool mIgnoreSeek;

        CodecSpecific codec;
    };
    Track *mFirstTrack, *mLastTrack;

    Mutex mLock;

    VC_CONTAINER_IO_MODULE_T mIO;
    VC_CONTAINER_T *mContainer;
    status_t mStatus;

    Condition mReaderCondition;
    Mutex mReaderConditionLock;
    Mutex mReaderLock;
    bool mReaderInUse;

protected:
    Track *trackGet(size_t index);
    virtual ~ContainerReader();
};

ContainerReader::ContainerReader(const sp<DataSource> &source)
 : mFirstTrack(NULL),
   mLastTrack(NULL),
   mContainer(NULL),
   mStatus(NO_ERROR),
   mReaderInUse(false) {
    mIO.source = source;
    mIO.offset = 0;
}

ContainerReader::~ContainerReader() {
    Track *track, *next = mFirstTrack;
    while (next) {
        track = next; 
        next = next->next;
        delete track;
    }

    if (mContainer)
        vc_container_close(mContainer);
}

bool ContainerReader::open(MetaData *fileMetaData) {
    LOGD("URI: %s", mIO.source->getUri().string());

    VC_CONTAINER_IO_T *io;
    VC_CONTAINER_IO_CAPABILITIES_T capabilities = VC_CONTAINER_IO_CAPS_NO_CACHING;
    VC_CONTAINER_STATUS_T status;
    off64_t size = 0;
    unsigned int i, j;

    /* Setup a container io module which points to the data source */
    io = vc_container_io_create( "DataSource", VC_CONTAINER_IO_MODE_READ, capabilities, &status );
    if (!io) {
        LOGE("could not create io (%i)", status);
        return false;
    }

    io->module = &this->mIO;
    io->pf_close = io_close;
    io->pf_read = io_read;
    io->pf_seek = io_seek;

    if (mIO.source->flags() & (DataSource::kIsHTTPBasedSource|DataSource::kIsCachingDataSource))
        capabilities |= VC_CONTAINER_IO_CAPS_SEEK_SLOW;

    if (!(capabilities & (VC_CONTAINER_IO_CAPS_SEEK_SLOW|VC_CONTAINER_IO_CAPS_CANT_SEEK)) &&
        !mIO.source->getSize(&size))
        io->size = size;

    /* Open the media container */
    mContainer = vc_container_open_reader_with_io(io, "DataSource", &status, 0, 0);
    if (!mContainer) {
        LOGE("error opening media (%i)", status);
        vc_container_io_close(io);
        return false;
    }
    LOGD("successfully opened media");

    /* Find all track */
    for ( i = 0, j = 0; i < mContainer->tracks_num; i++) {
       VC_CONTAINER_TRACK_T *container_track = mContainer->tracks[i];
       VC_CONTAINER_ES_FORMAT_T *format = container_track->format;
       Track *track;

       container_track->is_enabled = 0;

       const char *mime = codec_to_mime(format->codec, format->codec_variant);
       if (!mime || mime == MEDIA_MIMETYPE_UNKNOWN)
       {
          LOGD("codec %4.4s (track %i) not recognized", (char *)&format->codec, i);
          continue;
       }
       if (!(format->flags & VC_CONTAINER_ES_FORMAT_FLAG_FRAMED))
       {
          VC_CONTAINER_FOURCC_T codec_variant = format->codec_variant;

          LOGD("codec %4.4s (track %i) is not framed", (char *)&format->codec, i);

          /* The only pcm stagefright understands is signed 16 little endian */
          if (format->codec == VC_CONTAINER_CODEC_PCM_SIGNED_LE ||
              format->codec == VC_CONTAINER_CODEC_PCM_UNSIGNED_LE)
             memcpy(&codec_variant, "s16l", 4);

          if (vc_container_control(mContainer, VC_CONTAINER_CONTROL_TRACK_PACKETIZE, i, codec_variant) != VC_CONTAINER_SUCCESS)
          {
            LOGE("packetization not supported on track: %i, fourcc: %4.4s", i, (char *)&format->codec);
            continue;
          }
          else LOGD("packetizing track %i, fourcc: %4.4s", i, (char *)&format->codec);

       }
       LOGD("adding codec %4.4s (track %i/%i)", (char *)&format->codec, j, i);

       track = new Track(format, j++, i);
       if (!track)
           break;

       track->meta->setCString(kKeyMIMEType, mime);

       track->meta->setInt64(kKeyDuration, mContainer->duration);
       if (format->language[0] || format->language[1] || format->language[2])
       {
           char lang_code[4] = {format->language[0], format->language[1], format->language[2]};
           track->meta->setCString(kKeyMediaLanguage, lang_code);
       }

       switch (format->es_type)
       {
       case VC_CONTAINER_ES_TYPE_VIDEO:
           track->meta->setInt32(kKeyWidth, format->type->video.width);
           track->meta->setInt32(kKeyHeight, format->type->video.height);
           if (format->type->video.frame_rate_num && format->type->video.frame_rate_den)
              track->meta->setInt32(kKeyFrameRate, format->type->video.frame_rate_num /
                                    format->type->video.frame_rate_den);
           if (format->type->video.visible_width)
              track->meta->setInt32(kKeyDisplayWidth, format->type->video.visible_width);
           if (format->type->video.visible_height)
              track->meta->setInt32(kKeyDisplayHeight, format->type->video.visible_height);
           break;
       case VC_CONTAINER_ES_TYPE_AUDIO:
           track->meta->setInt32(kKeyChannelCount, format->type->audio.channels);
           track->meta->setInt32(kKeySampleRate, format->type->audio.sample_rate);
#ifdef  BCG_PORT_FLAG
           track->meta->setInt32(kKeyBitsPerSample, format->type->audio.bits_per_sample);
           track->meta->setInt32(kKeyBlockAlign, format->type->audio.block_align);
#endif
           track->meta->setInt32(kKeyBitRate, format->bitrate);

           // WMA specific option
           if (format->codec == VC_CONTAINER_CODEC_WMA1 ||
               format->codec == VC_CONTAINER_CODEC_WMA2 ||
               format->codec == VC_CONTAINER_CODEC_WMAP) {
               unsigned int offset = format->codec == VC_CONTAINER_CODEC_WMA1 ? 2 : 4;

               if (format->codec == VC_CONTAINER_CODEC_WMAP)
                   offset = 14;

               if (format->extradata_size > offset)
#ifdef  BCG_PORT_FLAG
                   track->meta->setInt32(kKeyEncodeOpt, format->extradata[offset]);
#else
                   ;  //something needs to be added
#endif
               else
                   LOGD("wrong extra data size for %4.4s %d",
                        (const char *)&format->codec, format->extradata_size);
           }

           LOGD("channels=%d sample_rate=%d bit_per_sample=%d block_align=%d bitrate=%d",
                format->type->audio.channels, format->type->audio.sample_rate,
                format->type->audio.bits_per_sample, format->type->audio.block_align,
                format->bitrate);
           break;
       case VC_CONTAINER_ES_TYPE_SUBPICTURE:
           break;
       default:
           break;
       }

       if (track->codec.setCodecSpecificHeaders(track->meta) != NO_ERROR)
       {
          LOGE("invalid codec config for %4.4s", (char *)&format->codec );
          delete track; 
          continue;
       }

       // Add our new track to our list
       if (mLastTrack) {
           mLastTrack->next = track;
       } else {
           mFirstTrack = track;
       }
       mLastTrack = track;
    }

    // FIXME
    fileMetaData->setCString(kKeyMIMEType, MEDIA_MIMETYPE_CONTAINER_MATROSKA);//MEDIA_MIMETYPE_CONTAINER_MPEG4);

    return !!mFirstTrack;
}

size_t ContainerReader::trackCount() {
    size_t n = 0;
    Track *track = mFirstTrack;
    while (track) {
        ++n;
        track = track->next;
    }

    return n;
}

ContainerReader::Track *ContainerReader::trackGet(size_t index) {
    Track *track = mFirstTrack;
    while (index > 0) {
        if (track == NULL) {
            return NULL;
        }

        track = track->next;
        --index;
    }

    return track;
}

sp<MetaData> ContainerReader::trackGetMetaData(size_t index, uint32_t flags) {
    Track *track = trackGet(index);
    int64_t offset;

    if (track == NULL) {
        return NULL;
    }

    // Try and find a suitable thumbnail frame in the first
    // few keyframes of the stream
    if ((flags & MediaExtractor::kIncludeExtensiveMetaData) &&
        (mContainer->capabilities & VC_CONTAINER_CAPS_CAN_SEEK) &&
        !track->meta->findInt64(kKeyThumbnailTime, &offset)) {
        VC_CONTAINER_STATUS_T status;
        VC_CONTAINER_PACKET_T packet;
        int64_t sampleTime = 0;
        unsigned int sampleSize = 0, size, i, j;

        memset(&packet, 0, sizeof(packet));
        mContainer->tracks[track->id]->is_enabled = 1;
        for (i = 0, offset = 0; i < 20; i++) {

            // Seek should always end up on a keyframe
            status = vc_container_seek( mContainer, &offset, VC_CONTAINER_SEEK_MODE_TIME,
                                        VC_CONTAINER_SEEK_FLAG_PRECISE|VC_CONTAINER_SEEK_FLAG_FORWARD);
            if (status != VC_CONTAINER_SUCCESS)
                break;

            status = vc_container_read(mContainer, &packet, VC_CONTAINER_READ_FLAG_INFO);
            if (status != VC_CONTAINER_SUCCESS ||
                packet.track != track->id ||
                packet.pts == VC_CONTAINER_TIME_UNKNOWN) {
                LOGE("thumbnail search failed early (status %i, track %i/%i)",
                     status, packet.track, track->id);
                break;
            }
            size = packet.frame_size;
            if (!size)
                size = packet.size;
            if (size > sampleSize) {
                sampleTime = packet.pts;
                sampleSize = size;
            }
            offset = packet.pts + 1000;
        }
        if (sampleTime)
            track->meta->setInt64(kKeyThumbnailTime, sampleTime);
    }

    return track->meta;
}

void ContainerReader::seek(size_t index, int64_t offset, MediaSource::ReadOptions::SeekMode mode) { 
    Mutex::Autolock autoLock(mLock);
    VC_CONTAINER_SEEK_FLAGS_T seekFlags = 0;
    VC_CONTAINER_STATUS_T status;

    Track *track = trackGet(index);
    if (track == NULL)
        return;

    if (track->mIgnoreSeek)
    {
        LOGD("ignoring seek track %i to %lld", index, offset);
        track->mIgnoreSeek = false;
        return;
    }

    Mutex::Autolock readerAutoLock(mReaderLock);

    // Flush all tracks
    Track *next = mFirstTrack;
    for (Track *next = mFirstTrack; next; next = next->next)
    {
        if (!next->mEnabled)
            continue;

        if (track->id != next->id)
            next->mIgnoreSeek = true;

        next->mGroup->flush_full();
    }

    LOGD("seek track %i to %lld", index, offset);

    switch (mode) {
        case MediaSource::ReadOptions::SEEK_PREVIOUS_SYNC:
            seekFlags = VC_CONTAINER_SEEK_FLAG_PRECISE;
            break;
        case MediaSource::ReadOptions::SEEK_NEXT_SYNC:
            seekFlags = VC_CONTAINER_SEEK_FLAG_PRECISE|VC_CONTAINER_SEEK_FLAG_FORWARD;
            break;
        case MediaSource::ReadOptions::SEEK_CLOSEST_SYNC:
        case MediaSource::ReadOptions::SEEK_CLOSEST:
            break;
        default:
            CHECK(!"Should not be here.");
            break;
    }

    status = vc_container_seek( mContainer, &offset, VC_CONTAINER_SEEK_MODE_TIME, seekFlags );
    if (status != VC_CONTAINER_SUCCESS)
        LOGE("seek to %lld failed (%i)", offset, status);

    mStatus = OK; // FIXME ??
}

status_t ContainerReader::read(MediaBuffer **out, size_t index) {
    Track *track = trackGet(index);
    if (track == NULL)
        return BAD_VALUE;

    *out = NULL;

    if (ENABLE_EXTRA_LOGGING)
    {
        unsigned int i, queues[4] = {0,0,0,0};
        Track *track = mFirstTrack;
        for (i = 0; i < 4 && track; i++, track = track->next)
            queues[i] = track->mEnabled ? track->mGroup->get_full_length() : 0;

        LOGV("read request from track %i (queues: %i,%i,%i,%i)", index,
             queues[0], queues[1], queues[2], queues[3] );
    }

    while (!*out)
    {
        if ((*out = track->mGroup->get_full()) != NULL)
        {
            if (ENABLE_EXTRA_LOGGING)
                LOGV("read buf %i from %i", (*out)->range_length(), index);
            return OK;
        }
        if (mStatus != OK)
            break;

        mReaderConditionLock.lock();
        if (mReaderInUse)
        {
            mReaderCondition.wait(mReaderConditionLock);
            mReaderConditionLock.unlock();
            continue;
        }
        mReaderInUse = true;
        mReaderConditionLock.unlock();

        readPacket(index);
        mReaderConditionLock.lock();
        mReaderInUse = false;
        mReaderCondition.signal();
        mReaderConditionLock.unlock();
    }

    if (mStatus == ERROR_END_OF_STREAM)
        LOGD("EOS for track %i", index);
    else if(mStatus != OK)
        LOGE("read failed for track %i", index);
    return mStatus;
}

void ContainerReader::readPacket(uint32_t index) {
    Mutex::Autolock autoLock(mReaderLock);
    MediaBuffer *buffer;

    VC_CONTAINER_STATUS_T status;
    VC_CONTAINER_PACKET_T packet;
    VC_CONTAINER_READ_FLAGS_T flags = 0;
    size_t offset, size, frame_size, buffer_size;
    bool packet_pushed = false, block = false, can_force;

    can_force = (mContainer->capabilities & VC_CONTAINER_CAPS_FORCE_TRACK) != 0;

    memset(&packet, 0, sizeof(packet));

    // If we can't force reading a given track allow us
    // to block waiting for space to place the track data.
    if (!can_force)
        block = true;

    do
    {
        packet.track = index;
        status = vc_container_read(mContainer, &packet, flags | VC_CONTAINER_READ_FLAG_INFO);
        if ((flags & VC_CONTAINER_READ_FLAG_FORCE_TRACK) && packet.track != index)
            LOGE("incorrect packet index, want %u, got %u", index, packet.track);

        if (status == VC_CONTAINER_ERROR_CONTINUE)
            continue;

        if(status != VC_CONTAINER_SUCCESS) {
            LOGD("EOF (%i)", status);
            break;
        }

        if (ENABLE_EXTRA_LOGGING)
            LOGV("packet info: track %i, size %i/%i, pts %lld%s, dts %lld%s, flags %x%s",
                 packet.track, packet.size, packet.frame_size,
                 packet.pts == VC_CONTAINER_TIME_UNKNOWN ? 0 : packet.pts,
                 packet.pts == VC_CONTAINER_TIME_UNKNOWN ? ":unknown" : "",
                 packet.dts == VC_CONTAINER_TIME_UNKNOWN ? 0 : packet.dts,
                 packet.dts == VC_CONTAINER_TIME_UNKNOWN ? ":unknown" : "",
                 packet.flags, (packet.flags & VC_CONTAINER_PACKET_FLAG_KEYFRAME) ? " (keyframe)" : "");

        // Flush any aggregating buffer which does not belong to this track
        Track *track = mFirstTrack;
        while (track) {
            MediaBuffer *mBuffer;
            if (track->mEnabled && track->id != packet.track &&
                track->mBufferCanBeSent &&
                (mBuffer = track->mGroup->stash_get()) != NULL) {
                track->codec.convert(mBuffer);
                track->mGroup->commit_buffer(mBuffer);
                track->mGroup->put_full(mBuffer);
                packet_pushed = true;
            }
            track = track->next;
        }
        if (packet_pushed)
            break;

        // Find the corresponding track for that packet
        track = mFirstTrack;
        while (track && (track->id != packet.track || !track->mEnabled)) {
            track = track->next;
        }
        if (track == NULL) {
            vc_container_read(mContainer, &packet, flags | VC_CONTAINER_READ_FLAG_SKIP);
            continue;
        }
        VC_CONTAINER_ES_FORMAT_T *format = mContainer->tracks[track->id]->format;

        // Get a buffer big enough to read the new packet
        buffer_size = packet.size + track->codec.getExtraBufferSize();
        if (ENABLE_EXTRA_LOGGING)
            LOGV("acquire buffer track %i", track->mIndex);
        status_t err = track->mGroup->acquire_buffer(&buffer, buffer_size, block, 5000000000ULL);
        if (err == WOULD_BLOCK) {
            if (can_force && track->id != index) {
                block = true;
                flags |= VC_CONTAINER_READ_FLAG_FORCE_TRACK;
                continue;
            }
            return;
        }
        if (err != OK) {
            LOGE("failed to acquire buffer (%s:%i)", err == TIMED_OUT ? "timeout:" : "", err);
            mStatus = err;
            return;
        }
        if (ENABLE_EXTRA_LOGGING)
            LOGV("acquire buffer track %i done (%i)", track->mIndex, buffer->range_length());

        offset = buffer->range_offset() + buffer->range_length();
        packet.data = ((uint8_t *)buffer->data()) + offset;
        packet.buffer_size = packet.size;
        packet.size = 0;
        status = vc_container_read(mContainer, &packet, flags);
        if (status != VC_CONTAINER_SUCCESS) {
            LOGE("EOF (read,%i)", status);
            // Flush any aggregated data
            if (buffer->range_length()) {
                status = VC_CONTAINER_SUCCESS;
                goto send;
            }
            buffer->release();
            break;
        }

        // Set all the relevant metadata if we're beginning a new buffer
        if (!buffer->range_length()) {
            int64_t timestamp = VC_CONTAINER_TIME_UNKNOWN;
            if (packet.pts != VC_CONTAINER_TIME_UNKNOWN)
                timestamp = packet.pts;
            else if (packet.dts != VC_CONTAINER_TIME_UNKNOWN)
                timestamp = packet.dts;

            // Stagefright does not cope with negative timestamps
            if (timestamp < 0) {
               LOGE("invalid timestamp %lld, setting to 0", timestamp);
               timestamp = 0;
            }
            buffer->meta_data()->setInt64(kKeyTime, timestamp);
// FIXME targettime

            if ((packet.flags & VC_CONTAINER_PACKET_FLAG_KEYFRAME) ||
                format->es_type != VC_CONTAINER_ES_TYPE_VIDEO)
                buffer->meta_data()->setInt32(kKeyIsSyncFrame, 1);
        }

        buffer->set_range(buffer->range_offset(), buffer->range_length() + packet.size);

        // Stagefright wants whole frames
        if (!(packet.flags & VC_CONTAINER_PACKET_FLAG_FRAME_END)) {
            track->mGroup->stash_buffer(buffer);
            track->mBufferCanBeSent = false;
            continue;
        }

        // Aggregate audio frames together if we're running out of audio buffers
        if (!can_force && format->es_type == VC_CONTAINER_ES_TYPE_AUDIO &&
            format->codec != VC_CONTAINER_CODEC_VORBIS &&
            track->mGroup->get_full_length() * 4 >
              track->mGroup->get_full_size() * 3 &&
            buffer->range_length() < AUDIO_PACKING_MAX_SIZE)
        {
            LOGD("queue too full (%i/%i). packing frames together (%i bytes)",
                 track->mGroup->get_full_length(), track->mGroup->get_full_size(),
                 buffer->range_length());
            track->mGroup->stash_buffer(buffer);
            track->mBufferCanBeSent = false;
            continue;
        }

        // Aggregate frames without timestamp since stagefright
        // wants valid timestamps for every frame
        if (format->codec != VC_CONTAINER_CODEC_VORBIS &&
            (packet.flags & VC_CONTAINER_PACKET_FLAG_FRAME_END)) {
            if (vc_container_read(mContainer, &packet,
                    flags | VC_CONTAINER_READ_FLAG_INFO) == VC_CONTAINER_SUCCESS &&
                track->id == packet.track &&
                packet.pts == VC_CONTAINER_TIME_UNKNOWN) {
                LOGD("next frame without timestamp, aggregating");
                track->mGroup->stash_buffer(buffer);
                track->mBufferCanBeSent = false;
                continue;
            }
        }

 send:
        /* We've got a buffer ready */
        track->codec.convert(buffer); 
        track->mGroup->commit_buffer(buffer);
        track->mGroup->put_full(buffer);
        break;
    } while (1);

    if (status == VC_CONTAINER_ERROR_EOS)
        mStatus = ERROR_END_OF_STREAM;
    else if (status != VC_CONTAINER_SUCCESS)
        mStatus = ERROR_IO;
}

status_t ContainerReader::trackStart(size_t index, MetaData *params) {
    Mutex::Autolock autoLock(mLock);

    Track *track = trackGet(index);
    if (track == NULL)
        return BAD_VALUE;

    track->mGroup = new MediaCircularBufferGroup;
    if (track->mGroup == NULL)
        return NO_MEMORY;

    int32_t buffer_size = 10*1024;
    int32_t headers_num = 32;

    if (mContainer->tracks[track->id]->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
    {
        buffer_size = 4*1024*1024;
        headers_num = 32;
    }
    else if (mContainer->tracks[track->id]->format->es_type == VC_CONTAINER_ES_TYPE_AUDIO)
    {
        buffer_size = 1024*1024;
        headers_num = 256;
    }

    if (track->mGroup->createBuffers(buffer_size, headers_num))
    {
        delete track->mGroup;
        return NO_MEMORY;
    }

    track->mEnabled = true;
    track->mIgnoreSeek = false;
    mContainer->tracks[track->id]->is_enabled = 1;
    return OK;
}

status_t ContainerReader::trackStop(size_t index) {
    Mutex::Autolock autoLock(mLock);

    Track *track = trackGet(index);
    if (track == NULL)
        return BAD_VALUE;

    track->mEnabled = false;

    // Make sure we're not running the reader
    mReaderLock.lock();
    mReaderLock.unlock();

    delete track->mGroup;
    return OK;
}

///////////////////////////////////////////////////////////////////////////////
// ContainerTrackSource class
///////////////////////////////////////////////////////////////////////////////
class ContainerTrackSource : public MediaSource {
public:
    ContainerTrackSource(ContainerReader *reader,
                         size_t index);

    virtual status_t start(MetaData *params = NULL);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options = NULL);

protected:
    virtual ~ContainerTrackSource();

private:
    Mutex mLock;

    sp<ContainerReader> mReader;
    size_t mIndex;

    bool mStarted;

    ContainerTrackSource(const ContainerTrackSource &);
    ContainerTrackSource &operator=(const ContainerTrackSource &);
};

ContainerTrackSource::ContainerTrackSource(
        ContainerReader *reader,
        size_t index)
    : mReader(reader),
      mIndex(index),
      mStarted(false)
{
}

ContainerTrackSource::~ContainerTrackSource() {
    if (mStarted) {
        stop();
    }
}

status_t ContainerTrackSource::start(MetaData *params) {
    Mutex::Autolock autoLock(mLock);
    LOGD("track (%i)", mIndex);

    CHECK(!mStarted);

    status_t err = mReader->trackStart(mIndex, params);
    if (err == OK)
        mStarted = true;

    return OK;
}

status_t ContainerTrackSource::stop() {
    Mutex::Autolock autoLock(mLock);
    LOGD("track (%i)", mIndex);

    CHECK(mStarted);

    status_t err = mReader->trackStop(mIndex);
    if (err == OK)
        mStarted = false;

    return OK;
}

sp<MetaData> ContainerTrackSource::getFormat() {
    Mutex::Autolock autoLock(mLock);
    LOGD("track (%i)", mIndex);
    return mReader->trackGetMetaData(mIndex, 0);
}

status_t ContainerTrackSource::read(
    MediaBuffer **out, const ReadOptions *options) {
    Mutex::Autolock autoLock(mLock);
    CHECK(mStarted);

    *out = NULL;

    // Take care of seeking first
    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        LOGD("seek track %i to %lld", mIndex, seekTimeUs);
        mReader->seek(mIndex, seekTimeUs, mode);
    }

    return mReader->read(out, mIndex);
}

///////////////////////////////////////////////////////////////////////////////
// ContainersExtractor class
///////////////////////////////////////////////////////////////////////////////
ContainersExtractor::ContainersExtractor(const sp<DataSource> &source)
 : mFileMetaData(new MetaData),
   mReader(new ContainerReader(source)),
   mReaderRef(mReader)
{
}

bool ContainersExtractor::open(){
    bool result = mReader->open(mFileMetaData.get());
    setDrmFlag(false);
    return result;
}

ContainersExtractor::~ContainersExtractor() {
}

size_t ContainersExtractor::countTracks() {
    return mReader->trackCount();
}

sp<MediaSource> ContainersExtractor::getTrack(size_t index) {
    if (index >= mReader->trackCount()) {
        return NULL;
    }

    return new ContainerTrackSource(mReader, index);
}

sp<MetaData> ContainersExtractor::getTrackMetaData(size_t index, uint32_t flags) {
    return mReader->trackGetMetaData(index, flags);
}

sp<MetaData> ContainersExtractor::getMetaData() {
    return mFileMetaData;
}

bool ContainersExtractor::override(const char *mime, DataSource *source) {
    if (
        #ifndef USE_GOOGLE_MATROSKA_PARSER
        !strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_MATROSKA) ||
        #endif
        // MPEG audio sniffing is pretty unreliable so we'll run
        // through our containers anyway and should still use
        // stagefright's extractor if it really is MPEG audio
        !strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_MPEG) ||
        !strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_AVI) ||
        !strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_WVM)) {
        return true;
    }

    // We want to use our reader for quicktime files
    if (!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_MPEG4) && source) {
        uint8_t header[4];
        if (source->readAt(8, header, 4) == 4 &&
            !memcmp(header, "qt  ", 4))
            return true;
    }

    return false;
}

/*****************************************************************************/
static struct {
   VC_CONTAINER_FOURCC_T codec;
   const char *mime;
   VC_CONTAINER_FOURCC_T variant;
} encoding_table[] =
{
   {VC_CONTAINER_CODEC_H263,           MEDIA_MIMETYPE_VIDEO_H263, 0},
   {VC_CONTAINER_CODEC_H264,           MEDIA_MIMETYPE_VIDEO_AVC, 0},
   {VC_CONTAINER_CODEC_MP4V,           MEDIA_MIMETYPE_VIDEO_MPEG4, 0},
   {VC_CONTAINER_CODEC_MP2V,           MEDIA_MIMETYPE_VIDEO_MPEG2, 0},
   {VC_CONTAINER_CODEC_MP1V,           MEDIA_MIMETYPE_VIDEO_MPEG2, 0},

#ifdef  OMX_EXTEND_CODECS_SUPPORT
   {VC_CONTAINER_CODEC_WMV3,           MEDIA_MIMETYPE_VIDEO_WMV3, 0},
   {VC_CONTAINER_CODEC_WMV2,           MEDIA_MIMETYPE_UNKNOWN, 0},
   {VC_CONTAINER_CODEC_WMV1,           MEDIA_MIMETYPE_UNKNOWN, 0},
   {VC_CONTAINER_CODEC_WVC1,           MEDIA_MIMETYPE_VIDEO_WVC1, 0},
#endif

#ifndef __KITKAT__
   {VC_CONTAINER_CODEC_VP8,            MEDIA_MIMETYPE_VIDEO_VPX, 0},
#else
   {VC_CONTAINER_CODEC_VP8,            MEDIA_MIMETYPE_VIDEO_VP8, 0},
#endif

#ifdef  OMX_EXTEND_CODECS_SUPPORT
   {VC_CONTAINER_CODEC_SPARK,          MEDIA_MIMETYPE_VIDEO_SPARK, 0},
   {VC_CONTAINER_CODEC_DIV3,           MEDIA_MIMETYPE_VIDEO_DIVX311, 0},
   {VC_CONTAINER_CODEC_RV40,           MEDIA_MIMETYPE_VIDEO_RV, 0},
   {VC_CONTAINER_CODEC_MJPEG,          MEDIA_MIMETYPE_VIDEO_MJPEG, 0},
#endif

   {VC_CONTAINER_CODEC_AMRNB,          MEDIA_MIMETYPE_AUDIO_AMR_NB, 0},
   {VC_CONTAINER_CODEC_AMRWB,          MEDIA_MIMETYPE_AUDIO_AMR_WB, 0},
   {VC_CONTAINER_CODEC_MPGA,           MEDIA_MIMETYPE_AUDIO_MPEG,
      VC_CONTAINER_VARIANT_MPGA_DEFAULT},
   {VC_CONTAINER_CODEC_MPGA,           MEDIA_MIMETYPE_AUDIO_MPEG,
      VC_CONTAINER_VARIANT_MPGA_L3},
   {VC_CONTAINER_CODEC_MPGA,           MEDIA_MIMETYPE_AUDIO_MPEG_LAYER_II,
      VC_CONTAINER_VARIANT_MPGA_L2},
   {VC_CONTAINER_CODEC_MPGA,           MEDIA_MIMETYPE_AUDIO_MPEG_LAYER_I,
      VC_CONTAINER_VARIANT_MPGA_L1},
   {VC_CONTAINER_CODEC_MP4A,           MEDIA_MIMETYPE_AUDIO_AAC, 0},
   {VC_CONTAINER_CODEC_QCELP,          MEDIA_MIMETYPE_AUDIO_QCELP, 0},
   {VC_CONTAINER_CODEC_VORBIS,         MEDIA_MIMETYPE_AUDIO_VORBIS, 0},
   {VC_CONTAINER_CODEC_ALAW,           MEDIA_MIMETYPE_AUDIO_G711_ALAW, 0},
   {VC_CONTAINER_CODEC_MULAW,          MEDIA_MIMETYPE_AUDIO_G711_MLAW, 0},
   {VC_CONTAINER_CODEC_FLAC,           MEDIA_MIMETYPE_AUDIO_FLAC, 0},
#ifdef  BCG_PORT_FLAG
   {VC_CONTAINER_CODEC_WMA1,           MEDIA_MIMETYPE_AUDIO_WMA, 0},
   {VC_CONTAINER_CODEC_WMA2,           MEDIA_MIMETYPE_AUDIO_WMA, 0},
   {VC_CONTAINER_CODEC_WMAP,           MEDIA_MIMETYPE_AUDIO_WMA, 0},
#endif
   {VC_CONTAINER_CODEC_WMAL,           MEDIA_MIMETYPE_UNKNOWN, 0},
   {VC_CONTAINER_CODEC_PCM_SIGNED,     "audio/raw", 0},
   {VC_CONTAINER_CODEC_PCM_UNSIGNED,   "audio/raw", 0},

   {VC_CONTAINER_CODEC_JPEG,           MEDIA_MIMETYPE_IMAGE_JPEG, 0},

   {VC_CONTAINER_CODEC_UNKNOWN,        MEDIA_MIMETYPE_UNKNOWN, 0}
};

static const char *codec_to_mime(VC_CONTAINER_FOURCC_T codec,
   VC_CONTAINER_FOURCC_T variant)
{
   unsigned int i;
   // Try to find a match with the variant first
   for(i = 0; encoding_table[i].codec != VC_CONTAINER_CODEC_UNKNOWN; i++)
      if(encoding_table[i].codec == codec &&
         encoding_table[i].variant == variant)
         break;
   // If not successful, we just ignore the variant
   if (encoding_table[i].codec == VC_CONTAINER_CODEC_UNKNOWN)
      for(i = 0; encoding_table[i].codec != VC_CONTAINER_CODEC_UNKNOWN; i++)
         if(encoding_table[i].codec == codec)
            break;
   return encoding_table[i].mime;
}

static VC_CONTAINER_FOURCC_T mime_to_codec(const char *mime,
   VC_CONTAINER_FOURCC_T *variant)
{
   unsigned int i;
   for(i = 0; encoding_table[i].codec != VC_CONTAINER_CODEC_UNKNOWN; i++)
      if(!strcmp(encoding_table[i].mime, mime))
         break;
   if (variant) *variant = encoding_table[i].variant;
   return encoding_table[i].codec;
}

} //namespace android

