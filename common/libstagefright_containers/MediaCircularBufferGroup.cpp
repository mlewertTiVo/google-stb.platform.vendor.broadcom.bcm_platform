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
#include <utils/Log.h>

#include "MediaCircularBufferGroup.h"
#include <ADebug.h>

namespace android {

///////////////////////////////////////////////////////////////////////////////
// ContainerMediaBuffer
///////////////////////////////////////////////////////////////////////////////
class ContainerMediaBuffer : public MediaBuffer {
public:
    ContainerMediaBuffer(void *data, size_t size)
        : MediaBuffer(data, size), mNext(NULL), mLength(0) {}
    ContainerMediaBuffer *mNext;
    size_t mLength;
};

///////////////////////////////////////////////////////////////////////////////
// ContainerMediaBufferQueue
///////////////////////////////////////////////////////////////////////////////
ContainerMediaBufferQueue::ContainerMediaBufferQueue()
    : mFirst(NULL), mLast(&mFirst), mLength(0) {}

void ContainerMediaBufferQueue::put(ContainerMediaBuffer *buffer) {
    Mutex::Autolock autoLock(mLock);
    buffer->mNext = NULL;
    *mLast = buffer;
    mLast = &buffer->mNext;
    mLength++;
    mCondition.signal();
}

ContainerMediaBuffer *ContainerMediaBufferQueue::get() {
    Mutex::Autolock autoLock(mLock);
    ContainerMediaBuffer *buffer = mFirst;
    if (buffer == NULL)
        return NULL;
    mFirst = buffer->mNext;
    if (mFirst == NULL)
        mLast = &mFirst;
    mLength--;
    return buffer;
}

ContainerMediaBuffer *ContainerMediaBufferQueue::wait(nsecs_t timeout) {
    Mutex::Autolock autoLock(mLock);
    while (mFirst == NULL) {
        status_t err = timeout ?
            mCondition.waitRelative(mLock, timeout) :
            mCondition.wait(mLock);
        if (err != NO_ERROR)
            return NULL;
    }

    ContainerMediaBuffer *buffer = mFirst;
    mFirst = buffer->mNext;
    if (mFirst == NULL)
        mLast = &mFirst;
    mLength--;
    return buffer;
}

ContainerMediaBuffer *ContainerMediaBufferQueue::wait() {
    return wait(0);
}

///////////////////////////////////////////////////////////////////////////////
// MediaCircularBufferGroup
///////////////////////////////////////////////////////////////////////////////
MediaCircularBufferGroup::MediaCircularBufferGroup()
    : mBuffer(NULL), mSize(0), mNum(0), mReadIndex(0), mWriteIndex(0),
      mWrapIndex(0), mCurrentBuffer(NULL) {
}

MediaCircularBufferGroup::~MediaCircularBufferGroup() {
    flush_full();

    if (mBuffer)
        free(mBuffer);
}

bool MediaCircularBufferGroup::createBuffers(unsigned int size, unsigned int num) {
    mSize = size;
    mNum = num;
    mWrapIndex = mSize;
    mBuffer = (uint8_t *)malloc(size);
    if (!mBuffer)
        return true;
    for (unsigned int i = 0; i < num; i++) {
        ContainerMediaBuffer *buffer = new ContainerMediaBuffer(mBuffer, mSize);
        if (!buffer)
            return true;
        add_buffer(buffer);
        mEmpty.put(buffer);
    }
    return false;
}

void MediaCircularBufferGroup::flush_full() {
    ContainerMediaBuffer *buffer;

    while ((buffer = mFull.get()) != NULL)
        buffer->release();

    if (mCurrentBuffer)
        mCurrentBuffer->release();
    mCurrentBuffer = NULL;
}

MediaBuffer *MediaCircularBufferGroup::get_full() {
    return mFull.get();
}

unsigned int MediaCircularBufferGroup::get_full_length() {
    return mFull.length();
}

unsigned int MediaCircularBufferGroup::get_full_size() {
    return mNum;
}

void MediaCircularBufferGroup::put_full(MediaBuffer *buffer) {
    mFull.put(static_cast <ContainerMediaBuffer *> (buffer));
}

void MediaCircularBufferGroup::release_buffer(ContainerMediaBuffer *buf) {
    ContainerMediaBuffer *buffer = static_cast <ContainerMediaBuffer *> (buf);
    mLock.lock();
    CHECK(mReadIndex <= mWriteIndex || mReadIndex <= mWrapIndex);
    if (mReadIndex > mWriteIndex && mReadIndex == mWrapIndex)
        mReadIndex = 0;
    mReadIndex += buffer->mLength;
    mCondition.signal();
    mLock.unlock();
    buffer->mLength = 0;
}

status_t MediaCircularBufferGroup::acquire_buffer(MediaBuffer **out,
    unsigned int size, bool block, nsecs_t timeout) {
    ContainerMediaBuffer *buffer = mCurrentBuffer;
    status_t err;

    if (buffer)
        size += buffer->range_length();

    CHECK(size < mSize);
    if (size >= mSize)
        return NO_MEMORY;

    mCurrentBuffer = NULL;
    if (!buffer && !block) {
        buffer = mEmpty.get();
        if (!buffer)
            return WOULD_BLOCK;
        buffer->add_ref();
        buffer->set_range(0, 0);
    }

    if (!buffer) {
        buffer = mEmpty.wait(timeout);
        if (!buffer)
            return TIMED_OUT;
        buffer->add_ref();
        buffer->set_range(0, 0);
    }

    // Wait until we have enough space available
    mLock.lock();
    while (getFreeSize() < size) {
        if (!block) {
            mCurrentBuffer = buffer;
            mLock.unlock();
            return WOULD_BLOCK;
        }

        err = timeout ?
            mCondition.waitRelative(mLock, timeout) :
            mCondition.wait(mLock);
        if (err != NO_ERROR) {
            mCurrentBuffer = buffer;
            mLock.unlock();
            return err;
        }
    }

    if (getFreeContiguousSize() < size) {
        unsigned int writeIndex = mWriteIndex;
        mWriteIndex = 0;

        if (!block && getFreeSize() < size) {
            mWriteIndex = writeIndex;
            mCurrentBuffer = buffer;
            mLock.unlock();
            return WOULD_BLOCK;
        }

        while (getFreeSize() < size) {
            err = timeout ?
                mCondition.waitRelative(mLock, timeout) :
                mCondition.wait(mLock);
            if (err != NO_ERROR) {
                mWriteIndex = writeIndex;
                mCurrentBuffer = buffer;
                mLock.unlock();
                return err;
            }
        }
        mWrapIndex = writeIndex;

        // Copy any previous data
        if (buffer->range_length()) {
            uint8_t *p = (uint8_t *)buffer->data();
            memcpy(p + mWriteIndex, p + mWrapIndex, buffer->range_length());
        }
    }
    mLock.unlock();
    buffer->set_range(mWriteIndex, buffer->range_length());
    *out = buffer;
    return OK;
}

void MediaCircularBufferGroup::commit_buffer(MediaBuffer *in) {
    ContainerMediaBuffer *buffer = static_cast <ContainerMediaBuffer *>(in);
    mLock.lock();
    CHECK(buffer->range_length() < getFreeContiguousSize());
    mWriteIndex += buffer->range_length();
    buffer->mLength = buffer->range_length();
    mLock.unlock();
}

void MediaCircularBufferGroup::stash_buffer(MediaBuffer *in) {
    ContainerMediaBuffer *buffer = static_cast <ContainerMediaBuffer *>(in);
    mCurrentBuffer = buffer;
}

MediaBuffer *MediaCircularBufferGroup::stash_get() {
    ContainerMediaBuffer *buffer = mCurrentBuffer;
    mCurrentBuffer = NULL;
    return buffer;
}

void MediaCircularBufferGroup::signalBufferReturned(MediaBuffer *in) {
    ContainerMediaBuffer *buffer = static_cast <ContainerMediaBuffer *>(in);
    release_buffer(buffer);
    mEmpty.put(buffer);
}

unsigned int MediaCircularBufferGroup::getFreeSize() {
    if (mWriteIndex >= mReadIndex)
        return mSize - (mWriteIndex - mReadIndex) - 1;
    else
        return mReadIndex - mWriteIndex - 1;
}

unsigned int MediaCircularBufferGroup::getFreeContiguousSize() {
    if (mWriteIndex >= mReadIndex)
        return mSize - mWriteIndex - 1;
    else
        return mReadIndex - mWriteIndex - 1;
}

} //namespace android
