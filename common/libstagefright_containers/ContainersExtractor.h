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

#ifndef CONTAINERS_EXTRACTOR_H_
#define CONTAINERS_EXTRACTOR_H_

#include <MediaExtractor.h>
#include <utils/Errors.h>

namespace android {

class DataSource;
class ContainerReader;

class ContainersExtractor : public MediaExtractor {
public:
    ContainersExtractor(const sp<DataSource> &source);
    bool open();

    virtual size_t countTracks();
    virtual sp<MediaSource> getTrack(size_t index);
    virtual sp<MetaData> getTrackMetaData(size_t index, uint32_t flags);
    virtual sp<MetaData> getMetaData();

    static bool override(const char *mime, DataSource *source = NULL);

protected:
    virtual ~ContainersExtractor();

private:
    sp<MetaData> mFileMetaData;
    ContainerReader *mReader;
    sp<RefBase> mReaderRef;
};

}  // namespace android

#endif  // CONTAINERS_EXTRACTOR_H_

