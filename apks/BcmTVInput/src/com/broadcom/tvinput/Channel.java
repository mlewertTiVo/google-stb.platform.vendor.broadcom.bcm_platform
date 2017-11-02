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

/**
 * A convenience class to create and insert channel entries into the database.
 */
public final class Channel {
    public static final String[] PROJECTION = new String[]{
            TvContract.Channels._ID,
            TvContract.Channels.COLUMN_DESCRIPTION,
            TvContract.Channels.COLUMN_DISPLAY_NAME,
            TvContract.Channels.COLUMN_DISPLAY_NUMBER,
            TvContract.Channels.COLUMN_INPUT_ID,
            TvContract.Channels.COLUMN_ORIGINAL_NETWORK_ID,
            TvContract.Channels.COLUMN_SEARCHABLE,
            TvContract.Channels.COLUMN_SERVICE_ID,
            TvContract.Channels.COLUMN_SERVICE_TYPE,
            TvContract.Channels.COLUMN_TRANSPORT_STREAM_ID,
            TvContract.Channels.COLUMN_TYPE
    };

    private static final long INVALID_CHANNEL_ID = -1;
    private static final int INVALID_INTEGER_VALUE = -1;

    private long mId;
    private String mInputId;
    private String mType;
    private String mDisplayNumber;
    private String mDisplayName;
    private String mDescription;
    private String mChannelLogo;
    private int mOriginalNetworkId;
    private int mTransportStreamId;
    private boolean mSearchable;
    private int mServiceId;
    private String mServiceType;
    private int mVideoPid;

    private Channel() {
        mId = INVALID_CHANNEL_ID;
        mOriginalNetworkId = INVALID_INTEGER_VALUE;
        mServiceType = TvContract.Channels.SERVICE_TYPE_AUDIO_VIDEO;
        mSearchable = false;
    }


    public long getId() {
        return mId;
    }

    public String getInputId() {
        return mInputId;
    }

    public String getType() {
        return mType;
    }

    public String getDisplayNumber() {
        return mDisplayNumber;
    }

    public String getDisplayName() {
        return mDisplayName;
    }

    public String getDescription() {
        return mDescription;
    }

    public int getOriginalNetworkId() {
        return mOriginalNetworkId;
    }

    public int getTransportStreamId() {
        return mTransportStreamId;
    }

    public int getServiceId() {
        return mServiceId;
    }

    public String getChannelLogo() {
        return mChannelLogo;
    }

    public String getServiceType() {
        return mServiceType;
    }

    public boolean isSearchable() {
        return mSearchable;
    }

    public int getVideoPid() {
        return mVideoPid;
    }

    @Override
    public String toString() {
        return "Channel{"
                + "id=" + mId
                + ", inputId=" + mInputId
                + ", originalNetworkId=" + mOriginalNetworkId
                + ", serviceId=" + mServiceId
                + ", transportStreamId=" + mTransportStreamId
                + ", type=" + mType
                + ", displayNumber=" + mDisplayNumber
                + ", displayName=" + mDisplayName
                + ", description=" + mDescription
                + ", channelLogo=" + mChannelLogo
                + ", searchable=" + mSearchable + "}";
    }

    public ContentValues toContentValues() {
        ContentValues values = new ContentValues();

        if (mId != INVALID_CHANNEL_ID) {
            values.put(TvContract.Channels._ID, mId);
        }

        if (!TextUtils.isEmpty(mInputId)) {
            values.put(TvContract.Channels.COLUMN_INPUT_ID, mInputId);
        } else {
            values.putNull(TvContract.Channels.COLUMN_INPUT_ID);
        }

        if (!TextUtils.isEmpty(mType)) {
            values.put(TvContract.Channels.COLUMN_TYPE, mType);
        } else {
            values.putNull(TvContract.Channels.COLUMN_TYPE);
        }

        if (!TextUtils.isEmpty(mDisplayNumber)) {
            values.put(TvContract.Channels.COLUMN_DISPLAY_NUMBER, mDisplayNumber);
        } else {
            values.putNull(TvContract.Channels.COLUMN_DISPLAY_NUMBER);
        }

        if (!TextUtils.isEmpty(mDisplayName)) {
            values.put(TvContract.Channels.COLUMN_DISPLAY_NAME, mDisplayName);
        } else {
            values.putNull(TvContract.Channels.COLUMN_DISPLAY_NAME);
        }

        if (!TextUtils.isEmpty(mDescription)) {
            values.put(TvContract.Channels.COLUMN_DESCRIPTION, mDescription);
        } else {
            values.putNull(TvContract.Channels.COLUMN_DESCRIPTION);
        }

        values.put(TvContract.Channels.COLUMN_ORIGINAL_NETWORK_ID, mOriginalNetworkId);
        values.put(TvContract.Channels.COLUMN_TRANSPORT_STREAM_ID, mTransportStreamId);
        values.put(TvContract.Channels.COLUMN_SERVICE_ID, mServiceId);
        values.put(TvContract.Channels.COLUMN_SEARCHABLE, mSearchable);
        values.put(TvContract.Channels.COLUMN_SERVICE_TYPE, mServiceType);

        return values;
    }

    private void copyFrom(Channel other) {
        if (this == other) {
            return;
        }
        mId = other.mId;
        mInputId = other.mInputId;
        mType = other.mType;
        mDisplayNumber = other.mDisplayNumber;
        mDisplayName = other.mDisplayName;
        mDescription = other.mDescription;
        mOriginalNetworkId = other.mOriginalNetworkId;
        mTransportStreamId = other.mTransportStreamId;
        mServiceId = other.mServiceId;
        mVideoPid = other.mVideoPid;
        mChannelLogo = other.mChannelLogo;
        mSearchable = other.mSearchable;
        mServiceType = other.mServiceType;
    }

    public static Channel fromCursor(Cursor cursor) {
        Builder builder = new Builder();
        int index = 0;

        if (!cursor.isNull(index)) {
            builder.setId(cursor.getLong(index));
        }

        if (!cursor.isNull(++index)) {
            builder.setDescription(cursor.getString(index));
        }

        if (!cursor.isNull(++index)) {
            builder.setDisplayName(cursor.getString(index));
        }

        if (!cursor.isNull(++index)) {
            builder.setDisplayNumber(cursor.getString(index));
        }

        if (!cursor.isNull(++index)) {
            builder.setInputId(cursor.getString(index));
        }

        if (!cursor.isNull(++index)) {
            builder.setOriginalNetworkId(cursor.getInt(index));
        }

        if (!cursor.isNull(++index)) {
            builder.setSearchable(cursor.getInt(index) == 1);
        }

        if (!cursor.isNull(++index)) {
            builder.setServiceId(cursor.getInt(index));
        }

        if (!cursor.isNull(++index)) {
            builder.setServiceType(cursor.getString(index));
        }

        if (!cursor.isNull(++index)) {
            builder.setTransportStreamId(cursor.getInt(index));
        }

        if (!cursor.isNull(++index)) {
            builder.setType(cursor.getString(index));
        }
        return builder.build();
    }

    /**
     * The builder class that makes it easy to chain setters to create a {@link Channel} object.
     */
    public static final class Builder {
        private final Channel mChannel;

        public Builder() {
            mChannel = new Channel();
        }

        public Builder(Channel other) {
            mChannel = new Channel();
            mChannel.copyFrom(other);
        }

        private Builder setId(long id) {
            mChannel.mId = id;
            return this;
        }

        public Builder setInputId(String inputId) {
            mChannel.mInputId = inputId;
            return this;
        }

        public Builder setType(String type) {
            mChannel.mType = type;
            return this;
        }

        public Builder setDisplayNumber(String displayNumber) {
            mChannel.mDisplayNumber = displayNumber;
            return this;
        }

        public Builder setDisplayName(String displayName) {
            mChannel.mDisplayName = displayName;
            return this;
        }

        public Builder setDescription(String description) {
            mChannel.mDescription = description;
            return this;
        }

        public Builder setChannelLogo(String channelLogo) {
            mChannel.mChannelLogo = channelLogo;
            return this;
        }

        public Builder setOriginalNetworkId(int originalNetworkId) {
            mChannel.mOriginalNetworkId = originalNetworkId;
            return this;
        }

        public Builder setTransportStreamId(int transportStreamId) {
            mChannel.mTransportStreamId = transportStreamId;
            return this;
        }

        public Builder setSearchable(boolean searchable) {
            mChannel.mSearchable = searchable;
            return this;
        }

        public Builder setServiceId(int serviceId) {
            mChannel.mServiceId = serviceId;
            return this;
        }

        public Builder setVideoPid(int vpid) {
            mChannel.mVideoPid = vpid;
            return this;
        }

        public Builder setServiceType(String serviceType) {
            mChannel.mServiceType = serviceType;
            return this;
        }

        public Channel build() {
            Channel channel = new Channel();
            channel.copyFrom(mChannel);
            return channel;
        }
    }
}
