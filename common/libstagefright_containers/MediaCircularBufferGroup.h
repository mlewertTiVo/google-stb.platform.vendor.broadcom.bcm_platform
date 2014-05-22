/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef MEDIA_CIRCULAR_BUFFER_GROUP_H_

#define MEDIA_CIRCULAR_BUFFER_GROUP_H_

#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaBufferGroup.h>
#include <utils/Errors.h>
#include <utils/threads.h>

namespace android {

class ContainerMediaBuffer;

class ContainerMediaBufferQueue {
public:
    ContainerMediaBufferQueue();
    void put(ContainerMediaBuffer *buffer);
    ContainerMediaBuffer *get();
    ContainerMediaBuffer *wait();
    ContainerMediaBuffer *wait(nsecs_t);
    unsigned int length() { return mLength; }

private:
    Mutex mLock;
    Condition mCondition;

    ContainerMediaBuffer *mFirst;
    ContainerMediaBuffer **mLast;
    unsigned int mLength;
};

class MediaCircularBufferGroup : public MediaBufferGroup {
public:
    MediaCircularBufferGroup();
    ~MediaCircularBufferGroup();

    bool createBuffers(unsigned int size, unsigned int num);
    void flush_full();
    MediaBuffer *get_full();
    unsigned int get_full_length();
    unsigned int get_full_size();
    void put_full(MediaBuffer *buffer);

    status_t acquire_buffer(MediaBuffer **out, unsigned int size, bool block, nsecs_t timeout);
    void commit_buffer(MediaBuffer *in);
    void stash_buffer(MediaBuffer *in);
    MediaBuffer *stash_get();

protected:
    virtual void signalBufferReturned(MediaBuffer *in);

private:
    void release_buffer(ContainerMediaBuffer *buf);
    unsigned int getFreeSize();
    unsigned int getFreeContiguousSize();

    uint8_t *mBuffer;
    unsigned int mSize;
    unsigned int mNum;
    unsigned int mReadIndex;
    unsigned int mWriteIndex;
    unsigned int mWrapIndex;

    Mutex mLock;
    Condition mCondition;

    ContainerMediaBuffer *mCurrentBuffer;
    ContainerMediaBufferQueue mEmpty;
    ContainerMediaBufferQueue mFull;
};

}  // namespace android

#endif  // MEDIA_CIRCULAR_BUFFER_GROUP_H_

