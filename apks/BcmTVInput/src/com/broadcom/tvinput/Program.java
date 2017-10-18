/*
 * Copyright 2016-2017 The Android Open Source Project
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
package com.broadcom.tvinput;

import android.content.ContentValues;
import android.database.Cursor;
import android.media.tv.TvContract;
import android.text.TextUtils;

import java.util.Objects;

/**
 * A convenience class to create and insert program information into the database.
 */
public final class Program {

    public static final String[] PROJECTION = new String[] {
            TvContract.Programs._ID,
            TvContract.Programs.COLUMN_CHANNEL_ID,
            TvContract.Programs.COLUMN_TITLE,
            TvContract.Programs.COLUMN_SHORT_DESCRIPTION,
            TvContract.Programs.COLUMN_POSTER_ART_URI,
            TvContract.Programs.COLUMN_THUMBNAIL_URI,
            TvContract.Programs.COLUMN_AUDIO_LANGUAGE,
            TvContract.Programs.COLUMN_START_TIME_UTC_MILLIS,
            TvContract.Programs.COLUMN_END_TIME_UTC_MILLIS
    };

    private static final long INVALID_LONG_VALUE = -1;

    private long mId;
    private long mChannelId;
    private String mTitle;
    private long mStartTimeUtcMillis;
    private long mEndTimeUtcMillis;
    private String mDescription;
    private String mPosterArtUri;
    private String mThumbnailUri;
    private String mAudioLanguages;

    private Program() {
        mChannelId = INVALID_LONG_VALUE;
        mId = INVALID_LONG_VALUE;
        mStartTimeUtcMillis = INVALID_LONG_VALUE;
        mEndTimeUtcMillis = INVALID_LONG_VALUE;
    }

    public long getId() {
        return mId;
    }

    public long getChannelId() {
        return mChannelId;
    }

    public String getTitle() {
        return mTitle;
    }

    public long getStartTimeUtcMillis() {
        return mStartTimeUtcMillis;
    }

    public long getEndTimeUtcMillis() {
        return mEndTimeUtcMillis;
    }

    public String getDescription() {
        return mDescription;
    }

    public String getPosterArtUri() {
        return mPosterArtUri;
    }

    public String getThumbnailUri() {
        return mThumbnailUri;
    }

    public String getAudioLanguages() {
        return mAudioLanguages;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mChannelId, mStartTimeUtcMillis, mEndTimeUtcMillis,
                mTitle, mDescription, mPosterArtUri, mThumbnailUri);
    }

    @Override
    public boolean equals(Object other) {
        if (!(other instanceof Program)) {
            return false;
        }
        Program program = (Program) other;
        return mChannelId == program.mChannelId
                && mStartTimeUtcMillis == program.mStartTimeUtcMillis
                && mEndTimeUtcMillis == program.mEndTimeUtcMillis
                && Objects.equals(mTitle, program.mTitle)
                && Objects.equals(mDescription, program.mDescription)
                && Objects.equals(mPosterArtUri, program.mPosterArtUri)
                && Objects.equals(mThumbnailUri, program.mThumbnailUri);
    }

    @Override
    public String toString() {
        return "Program{"
                + "id=" + mId
                + ", channelId=" + mChannelId
                + ", title=" + mTitle
                + ", startTimeUtcSec=" + mStartTimeUtcMillis
                + ", endTimeUtcSec=" + mEndTimeUtcMillis
                + ", posterArtUri=" + mPosterArtUri
                + ", thumbnailUri=" + mThumbnailUri
                + "}";
    }

    private void copyFrom(Program other) {
        if (this == other) {
            return;
        }

        mId = other.mId;
        mChannelId = other.mChannelId;
        mTitle = other.mTitle;
        mStartTimeUtcMillis = other.mStartTimeUtcMillis;
        mEndTimeUtcMillis = other.mEndTimeUtcMillis;
        mDescription = other.mDescription;
        mPosterArtUri = other.mPosterArtUri;
        mThumbnailUri = other.mThumbnailUri;
        mAudioLanguages = other.mAudioLanguages;
    }

    public ContentValues toContentValues() {
        ContentValues values = new ContentValues();
        if (mId != INVALID_LONG_VALUE) {
            values.put(TvContract.Programs._ID, mId);
        }

        if (mChannelId != INVALID_LONG_VALUE) {
            values.put(TvContract.Programs.COLUMN_CHANNEL_ID, mChannelId);
        } else {
            values.putNull(TvContract.Programs.COLUMN_CHANNEL_ID);
        }

        if (!TextUtils.isEmpty(mTitle)) {
            values.put(TvContract.Programs.COLUMN_TITLE, mTitle);
        } else {
            values.putNull(TvContract.Programs.COLUMN_TITLE);
        }

        if (!TextUtils.isEmpty(mDescription)) {
            values.put(TvContract.Programs.COLUMN_SHORT_DESCRIPTION, mDescription);
        } else {
            values.putNull(TvContract.Programs.COLUMN_SHORT_DESCRIPTION);
        }

        if (!TextUtils.isEmpty(mPosterArtUri)) {
            values.put(TvContract.Programs.COLUMN_POSTER_ART_URI, mPosterArtUri);
        } else {
            values.putNull(TvContract.Programs.COLUMN_POSTER_ART_URI);
        }

        if (!TextUtils.isEmpty(mThumbnailUri)) {
            values.put(TvContract.Programs.COLUMN_THUMBNAIL_URI, mThumbnailUri);
        } else {
            values.putNull(TvContract.Programs.COLUMN_THUMBNAIL_URI);
        }

        if (!TextUtils.isEmpty(mAudioLanguages)) {
            values.put(TvContract.Programs.COLUMN_AUDIO_LANGUAGE, mAudioLanguages);
        } else {
            values.putNull(TvContract.Programs.COLUMN_AUDIO_LANGUAGE);
        }

        if (mStartTimeUtcMillis != INVALID_LONG_VALUE) {
            values.put(TvContract.Programs.COLUMN_START_TIME_UTC_MILLIS, mStartTimeUtcMillis);
        } else {
            values.putNull(TvContract.Programs.COLUMN_START_TIME_UTC_MILLIS);
        }

        if (mEndTimeUtcMillis != INVALID_LONG_VALUE) {
            values.put(TvContract.Programs.COLUMN_END_TIME_UTC_MILLIS, mEndTimeUtcMillis);
        } else {
            values.putNull(TvContract.Programs.COLUMN_END_TIME_UTC_MILLIS);
        }

        return values;
    }

    public static Program fromCursor(Cursor cursor) {
        Builder builder = new Builder();
        int index = 0;
        if (!cursor.isNull(index)) {
            builder.setId(cursor.getLong(index));
        }

        if (!cursor.isNull(++index)) {
            builder.setChannelId(cursor.getLong(index));
        }

        if (!cursor.isNull(++index)) {
            builder.setTitle(cursor.getString(index));
        }

        if (!cursor.isNull(++index)) {
            builder.setDescription(cursor.getString(index));
        }

        if (!cursor.isNull(++index)) {
            builder.setPosterArtUri(cursor.getString(index));
        }

        if (!cursor.isNull(++index)) {
            builder.setThumbnailUri(cursor.getString(index));
        }

        if (!cursor.isNull(++index)) {
            builder.setAudioLanguages(cursor.getString(index));
        }

        if (!cursor.isNull(++index)) {
            builder.setStartTimeUtcMillis(cursor.getLong(index));
        }

        if (!cursor.isNull(++index)) {
            builder.setEndTimeUtcMillis(cursor.getLong(index));
        }
        return builder.build();
    }

    public static final class Builder {
        private final Program mProgram;

        public Builder() {
            mProgram = new Program();
        }


        public Builder(Program other) {
            mProgram = new Program();
            mProgram.copyFrom(other);
        }

        public Builder(Channel channel) {
            mProgram = new Program();
            mProgram.mChannelId = channel.getId();
            mProgram.mDescription = channel.getDescription();
            mProgram.mThumbnailUri = channel.getChannelLogo();
            mProgram.mTitle = channel.getDisplayName();
        }


        private Builder setId(long programId) {
            mProgram.mId = programId;
            return this;
        }

        public Builder setChannelId(long channelId) {
            mProgram.mChannelId = channelId;
            return this;
        }

        public Builder setTitle(String title) {
            mProgram.mTitle = title;
            return this;
        }

        public Builder setStartTimeUtcMillis(long startTimeUtcMillis) {
            mProgram.mStartTimeUtcMillis = startTimeUtcMillis;
            return this;
        }

        public Builder setEndTimeUtcMillis(long endTimeUtcMillis) {
            mProgram.mEndTimeUtcMillis = endTimeUtcMillis;
            return this;
        }

        public Builder setDescription(String description) {
            mProgram.mDescription = description;
            return this;
        }

        public Builder setPosterArtUri(String posterArtUri) {
            mProgram.mPosterArtUri = posterArtUri;
            return this;
        }

        public Builder setThumbnailUri(String thumbnailUri) {
            mProgram.mThumbnailUri = thumbnailUri;
            return this;
        }

        public Builder setAudioLanguages(String audioLanguages) {
            mProgram.mAudioLanguages = audioLanguages;
            return this;
        }

        public Program build() {
            Program program = new Program();
            program.copyFrom(mProgram);
            return program;
        }
    }
}
