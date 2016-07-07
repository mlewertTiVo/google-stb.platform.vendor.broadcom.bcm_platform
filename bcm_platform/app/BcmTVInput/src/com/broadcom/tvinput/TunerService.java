/*
 * Copyright 2014 Google Inc. All rights reserved.
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

import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.ContentUris;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.hardware.hdmi.HdmiDeviceInfo;
import android.media.tv.TvContentRating;
import android.media.tv.TvInputHardwareInfo;
import android.media.tv.TvInputInfo;
import android.media.tv.TvInputManager;
import android.media.tv.TvInputService;
import android.media.tv.TvStreamConfig;
import android.media.tv.TvContract;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Handler;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;
import android.util.Log;
import android.util.SparseArray;
import android.view.KeyEvent;
import android.view.Surface;

import java.io.File;
import java.io.FileInputStream;
import java.util.Collections;
import java.io.IOException;
import java.util.HashMap;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Random;

import android.media.AudioManager;

import org.xmlpull.v1.XmlPullParserException;

/**
 * TV-Input service for Broadcom's Tuner
 * 
 */

public class TunerService extends TvInputService {
    private static final boolean DEBUG = true;
    private static final String TAG = TunerService.class.getSimpleName();
    private static final TvStreamConfig[] EMPTY_STREAM_CONFIGS = {};

    // inputId -> deviceId
    private final Map<String, Integer> mDeviceIdMap = new HashMap<>();

    // deviceId -> inputId
    private final SparseArray<String> mInputIdMap = new SparseArray<String>();

    // inputId -> TvInputInfo
    private final Map<String, TvInputInfo> mInputMap = new HashMap<>();

    private TvInputManager mManager = null;
    private ResolveInfo mResolveInfo;
    private static List<ChannelInfo> sSampleChannels = null;

    private static final String CHANNEL_1_NUMBER = "1-1";
    private static final String CHANNEL_1_NAME = "Tuner_Channel";
    private static final String PROGRAM_1_TITLE = "Sample";
    private static final String PROGRAM_1_DESC = "Sample Video";

    @Override
    public void onCreate() {
        if (DEBUG) 
			Log.d(TAG, "TunerService::onCreate()");

        super.onCreate();

        mResolveInfo = getPackageManager().resolveService(
                new Intent(SERVICE_INTERFACE).setClass(this, getClass()),
                PackageManager.GET_SERVICES | PackageManager.GET_META_DATA);

        mManager = (TvInputManager) getSystemService(Context.TV_INPUT_SERVICE);
    }

    @Override
    public Session onCreateSession(String inputId) {
        if (DEBUG) 
			Log.d(TAG, "TunerService::onCreateSession(), inputId = " +inputId);

        // Lookup TvInputInfo from inputId
        TvInputInfo info = mInputMap.get(inputId);

        return new TunerTvInputSessionImpl(this, info);
    }

    @Override
    public TvInputInfo onHardwareAdded(TvInputHardwareInfo hardwareInfo) {
        if (DEBUG) 
			Log.d(TAG, "TunerService::onHardwareAdded()");
		
		int type = hardwareInfo.getType();

        if (type != TvInputHardwareInfo.TV_INPUT_TYPE_TUNER) {
			Log.d(TAG, "TunerService::onHardwareAdded(), returning as the type is not tuner, type = " +type);
            return null;
        }

        int deviceId = hardwareInfo.getDeviceId();
		Log.d(TAG, "TunerService::onHardwareAdded(), deviceId = " +deviceId);

        TvInputInfo info = null;
        try {
            info = TvInputInfo.createTvInputInfo(this, mResolveInfo, hardwareInfo, "STB-TUNER " + deviceId, null);
        } catch (XmlPullParserException | IOException e) {
            Log.e(TAG, "Error while creating TvInputInfo", e);
            return null;
        }

        // Save TvInputInfo
        mInputMap.put(info.getId(), info);

        // Save mapping between deviceId and inputId
        mInputIdMap.put(deviceId, info.getId());

        // Save mapping between inputId and deviceId
        mDeviceIdMap.put(info.getId(), deviceId);

        if (DEBUG) 
			Log.d(TAG, "onHardwareAdded returns " + info);

        // Create some sample channels
        createSampleChannels(info.getId());

        return info;
    }

    private void createSampleChannels(String inputId) 
    {
        List<ChannelInfo> channels = null;
        Class clazz = TunerService.class;

        sSampleChannels = new ArrayList<ChannelInfo>();
        sSampleChannels.add(
                new ChannelInfo(CHANNEL_1_NUMBER, CHANNEL_1_NAME, null, 0, 0, 1, 640, 480, 2,
                        false,
                new ProgramInfo(PROGRAM_1_TITLE, null, PROGRAM_1_DESC, 0, 3600,
                        null, null, 0)));

        Uri uri = TvContract.buildChannelsUriForInput(inputId);
        String[] projection = { TvContract.Channels._ID };

        Cursor cursor = null;
        try 
        {
            while (true) {
                cursor = getContentResolver().query(uri, projection, null, null, null);
                if (cursor != null && cursor.getCount() > 0) {
                    return;
                }
                if (cursor != null) {
                    cursor.close();
                }
                if (DEBUG) 
                    Log.d(TAG, "Couldn't find the channel list. Inserting new channels...");

                // Insert channels into the database. This needs to be done only for the first time.
                populateChannels(this, inputId, sSampleChannels);
            }
        }
        
        finally 
        {
            if (cursor != null) 
            {
                cursor.close();
            }
        }
    }

    public static void populateChannels(Context context, String inputId, List<ChannelInfo> channels) 
    {
        ContentValues values = new ContentValues();
        values.put(TvContract.Channels.COLUMN_INPUT_ID, inputId);
        Map<Uri, String> logos = new HashMap<Uri, String>();

        for (ChannelInfo channel : channels) 
        {
            values.put(TvContract.Channels.COLUMN_DISPLAY_NUMBER, channel.mNumber);
            values.put(TvContract.Channels.COLUMN_DISPLAY_NAME, channel.mName);
            values.put(TvContract.Channels.COLUMN_ORIGINAL_NETWORK_ID, channel.mOriginalNetworkId);
            values.put(TvContract.Channels.COLUMN_TRANSPORT_STREAM_ID, channel.mTransportStreamId);
            values.put(TvContract.Channels.COLUMN_SERVICE_ID, channel.mServiceId);

            values.put(TvContract.Channels.COLUMN_VIDEO_FORMAT, TvContract.Channels.VIDEO_FORMAT_480P);

            Uri uri = context.getContentResolver().insert(TvContract.Channels.CONTENT_URI, values);
/*
            if (!TextUtils.isEmpty(channel.mLogoUrl)) 
            {
                logos.put(TvContract.buildChannelLogoUri(uri), channel.mLogoUrl);
            }
*/
        }
/*
        if (!logos.isEmpty()) 
        {
            new InsertLogosTask(context).executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, logos);
        }
*/
    }

    private static ContentValues createDummyChannelValues(String inputId) {
        ContentValues values = new ContentValues();
        values.put(TvContract.Channels.COLUMN_INPUT_ID, inputId);
        values.put(TvContract.Channels.COLUMN_TYPE, TvContract.Channels.TYPE_OTHER);
        values.put(TvContract.Channels.COLUMN_SERVICE_TYPE, TvContract.Channels.SERVICE_TYPE_AUDIO_VIDEO);
        values.put(TvContract.Channels.COLUMN_DISPLAY_NUMBER, "1");
        values.put(TvContract.Channels.COLUMN_VIDEO_FORMAT, TvContract.Channels.VIDEO_FORMAT_480P);
        values.put(TvContract.Channels.COLUMN_DISPLAY_NAME, "One dash one");
        values.put(TvContract.Channels.COLUMN_INTERNAL_PROVIDER_DATA, "Broadcom-Tuner".getBytes());

        return values;
    }

    @Override
    public String onHardwareRemoved(TvInputHardwareInfo hardwareInfo) {
        if (DEBUG) 
			Log.d(TAG, "TunerService::onHardwareRemoved()");

        int deviceId = hardwareInfo.getDeviceId();
        String inputId = mInputIdMap.get(deviceId);

        mInputIdMap.remove(deviceId);
        mDeviceIdMap.remove(inputId);
        mInputMap.remove(inputId);

        if (DEBUG) 
			Log.d(TAG, "onHardwareRemoved returns " + deviceId);

        return inputId;
    }

  public static final class ChannelInfo {
        public final String mNumber;
        public final String mName;
        public final String mLogoUrl;
        public final int mOriginalNetworkId;
        public final int mTransportStreamId;
        public final int mServiceId;
        public final int mVideoWidth;
        public final int mVideoHeight;
        public final int mAudioChannel;
        public final boolean mHasClosedCaption;
        public final ProgramInfo mProgram;

        public ChannelInfo(String number, String name, String logoUrl, int originalNetworkId,
                int transportStreamId, int serviceId, int videoWidth, int videoHeight,
                int audioChannel, boolean hasClosedCaption, ProgramInfo program) {
            mNumber = number;
            mName = name;
            mLogoUrl = logoUrl;
            mOriginalNetworkId = originalNetworkId;
            mTransportStreamId = transportStreamId;
            mServiceId = serviceId;
            mVideoWidth = videoWidth;
            mVideoHeight = videoHeight;
            mAudioChannel = audioChannel;
            mHasClosedCaption = hasClosedCaption;
            mProgram = program;
        }
    }

    public static final class ProgramInfo {
        public final String mTitle;
        public final String mPosterArtUri;
        public final String mDescription;
        public final long mStartTimeSec;
        public final long mDurationSec;
        public final String mUrl;
        public final int mResourceId;
        public final TvContentRating[] mContentRatings;

        public ProgramInfo(String title, String posterArtUri, String description, long startTimeSec,
                long durationSec, TvContentRating[] contentRatings, String url, int resourceId) {
            mTitle = title;
            mPosterArtUri = posterArtUri;
            mDescription = description;
            mStartTimeSec = startTimeSec;
            mDurationSec = durationSec;
            mContentRatings = contentRatings;
            mUrl = url;
            mResourceId = resourceId;
        }
    }

    private class TunerTvInputSessionImpl extends Session {
        protected final TvInputInfo mInfo;
        protected final int mDeviceId;
        protected Surface mSurface = null;
        private TvInputManager.Hardware mHardware;
        private TvStreamConfig[] mStreamConfigs = EMPTY_STREAM_CONFIGS;

        TunerTvInputSessionImpl(Context context, TvInputInfo info) {
            super(context);

            mInfo = info;
            mDeviceId = mDeviceIdMap.get(info.getId());
			Log.d(TAG, "TunerTvInputSessionImpl,  mDeviceId = " +mDeviceId);

            acquireHardware();
        }

        private void acquireHardware() {
            if (mHardware != null) {
                return;
            }
            TvInputManager.HardwareCallback callback = new TvInputManager.HardwareCallback() {
                @Override
                public void onReleased() {
                    mHardware = null;
                }

                @Override
                public void onStreamConfigChanged(TvStreamConfig[] configs) {
                    mStreamConfigs = configs;
                }
            };
            mHardware = mManager.acquireTvInputHardware(mDeviceId, callback, mInfo);
        }

        private TvStreamConfig getStreamConfig() {
            for (TvStreamConfig config : mStreamConfigs) {
                if (config.getType() == TvStreamConfig.STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE) {
                      return config;
                }
            }
            return null;
        }

        // Passes surface object to the corresponding hardware for rendering of the content
        // to be visible.
        protected boolean setSurface(Surface surface) {
			Log.d(TAG, "setSurface (local),  Enter...");

            if (surface == null) {
                mStreamConfigs = EMPTY_STREAM_CONFIGS;
            }
            if (mHardware == null) {
                acquireHardware();
                if (mHardware == null) {
                    return false;
                }
            }
            TvStreamConfig config = null;
            if (surface != null) {
                config = getStreamConfig();
                if (config == null) {
                    return false;
                }
            }
            return mHardware.setSurface(surface, config);
        }

        @Override
        public boolean onSetSurface(Surface surface) {
            if (DEBUG) 
				Log.d(TAG, "onSetSurface surface:" + surface);

            return setSurface(surface);
        }

        @Override
        public void onRelease() {
            if (DEBUG) Log.d(TAG, "onRelease()");
            if (mHardware != null) {
                mManager.releaseTvInputHardware(mDeviceId, mHardware);
                mHardware = null;
            }
        }

        @Override
        public void onSetStreamVolume(float volume) {
            // No-op
        }

        @Override
        public boolean onTune(Uri channelUri) {
            return true;
        }

        @Override
        public void onSetCaptionEnabled(boolean enabled) {
            // No-op
        }
    }
}