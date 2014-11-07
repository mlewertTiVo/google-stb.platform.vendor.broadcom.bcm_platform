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

        Log.e(TAG, "Calling TunerHAL.initialize!!");
        TunerHAL.initialize();
        ChannelInfo[] civ = TunerHAL.getChannelList();

        Log.e(TAG, "Got channels: " + civ.length);
        updateChannels(this, info.getId(), civ);

        if (DEBUG) 
			Log.d(TAG, "onHardwareAdded returns " + info);

        return info;
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

    public static void populateChannels(Context context, String inputId, ChannelInfo channels[]) 
    {
        ContentValues channel_values = new ContentValues();
        ContentValues prog_values = new ContentValues();

        channel_values.put(TvContract.Channels.COLUMN_INPUT_ID, inputId);

        Log.d(TAG, "populateChannels: start");
        for (ChannelInfo channel : channels) 
        {
            // Initialize the Channels class
            channel_values.put(TvContract.Channels.COLUMN_DISPLAY_NUMBER, channel.number);
            channel_values.put(TvContract.Channels.COLUMN_DISPLAY_NAME, channel.name);
            channel_values.put(TvContract.Channels.COLUMN_ORIGINAL_NETWORK_ID, channel.onid);
            channel_values.put(TvContract.Channels.COLUMN_TRANSPORT_STREAM_ID, channel.tsid);
            channel_values.put(TvContract.Channels.COLUMN_SERVICE_ID, channel.sid);
            channel_values.put(TvContract.Channels.COLUMN_BROWSABLE, 1);
            channel_values.put(TvContract.Channels.COLUMN_VIDEO_FORMAT, TvContract.Channels.VIDEO_FORMAT_1080P);
            byte[] dbid = new byte[1];
            dbid[0] = (byte)channel.id;
            channel_values.put(TvContract.Channels.COLUMN_INTERNAL_PROVIDER_DATA, dbid);

            Uri channelUri = context.getContentResolver().insert(TvContract.Channels.CONTENT_URI, channel_values);
	        Log.d(TAG, "populateChannels: " + channel.number + " " + channel.name + " " + channelUri);

            //long channelId = ContentUris.parseId(channelUri);
            //Log.d(TAG, "channelId = " +channelId);

            // Initialize the Programs class
            //prog_values.put(TvContract.Programs.COLUMN_CHANNEL_ID, channelId);
            //prog_values.put(TvContract.Programs.COLUMN_TITLE, channel.mProgram.mTitle);
            //prog_values.put(TvContract.Programs.COLUMN_SHORT_DESCRIPTION, channel.mProgram.mDescription);
            
            //Uri rowUri = context.getContentResolver().insert(TvContract.Programs.CONTENT_URI, prog_values);
		//	Log.d(TAG, "rowUri = " +rowUri);
        }
        Log.d(TAG, "populateChannels: finish");
    }

    private void updateChannels(Context context, String inputId, ChannelInfo channels[]) {
        Uri uri = TvContract.buildChannelsUriForInput(inputId);
        getContentResolver().delete(uri, null, null);
        getContentResolver().delete(TvContract.Programs.CONTENT_URI, null, null);
        populateChannels(context, inputId, channels);
    }

    private class TunerTvInputSessionImpl extends Session 
    {
        protected final TvInputInfo mInfo;
        protected final int mDeviceId;
        protected Surface mSurface = null;
        protected int mCurrentChannelId = -1;
        private TvInputManager.Hardware mHardware;
        private TvStreamConfig[] mStreamConfigs = EMPTY_STREAM_CONFIGS;

        TunerTvInputSessionImpl(Context context, TvInputInfo info) 
        {
            super(context);

            mInfo = info;
            mDeviceId = mDeviceIdMap.get(info.getId());
			Log.d(TAG, "TunerTvInputSessionImpl,  mDeviceId = " +mDeviceId);

            acquireHardware();
        }

        private void acquireHardware() 
        {
            if (mHardware != null) 
            {
                return;
            }

            TvInputManager.HardwareCallback callback = new TvInputManager.HardwareCallback() 
            {
                @Override
                public void onReleased() 
                {
                    mHardware = null;
                }

                @Override
                public void onStreamConfigChanged(TvStreamConfig[] configs) 
                {
                    for (TvStreamConfig x: configs)
                    {
                        Log.d(TAG, "onStreamConfigChanged,  stream_id = " +x.getStreamId());
                        Log.d(TAG, "onStreamConfigChanged,  width = " +x.getMaxWidth());
                        Log.d(TAG, "onStreamConfigChanged,  height = " +x.getMaxHeight());
                        Log.d(TAG, "onStreamConfigChanged,  type = " +x.getType());
                    }

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
        protected boolean setSurface(Surface surface) 
        {
			Log.d(TAG, "setSurface (local),  Enter...");

            // Inform that we don't have the video yet
            notifyVideoUnavailable(TvInputManager.VIDEO_UNAVAILABLE_REASON_TUNING);

            if (surface == null) {
                mStreamConfigs = EMPTY_STREAM_CONFIGS;
                Log.d(TAG, "setSurface (local): surface is null");
            }

            if (mHardware == null) {
                acquireHardware();
                if (mHardware == null) {
                    Log.d(TAG, "setSurface (local): mHardware is null");
                    return false;
                }
            }

            TvStreamConfig config = null;
            if (surface != null) {
                config = getStreamConfig();
                if (config == null) {
                    Log.d(TAG, "setSurface (local): config is null");
                    return false;
                }

                // Save the surface
                mSurface = surface;
            }

            // Inform that we've got video running
            notifyVideoAvailable();

            Log.d(TAG, "setSurface (local): Calling mHardware.setSurface");
            return mHardware.setSurface(surface, config);
        }

        @Override
        public boolean onSetSurface(Surface surface) 
        {
            if (DEBUG) 
				Log.d(TAG, "onSetSurface surface:" + surface);

            return setSurface(surface);
        }

        @Override
        public void onRelease() 
        {
            if (DEBUG) 
                Log.d(TAG, "onRelease()");

            if (mHardware != null) 
            {
                mManager.releaseTvInputHardware(mDeviceId, mHardware);
                mHardware = null;
            }
        }

        @Override
        public void onSetStreamVolume(float volume) 
        {
			Log.d(TAG, "onSetStreamVolume,  Enter...");
            // No-op
        }

        @Override
        public boolean onTune(Uri channelUri) 
        {

            String[] projection = { TvContract.Channels.COLUMN_INTERNAL_PROVIDER_DATA };
            if (channelUri == null) {
                return false;
            }
            Cursor cursor = getContentResolver().query(
                    channelUri, projection, null, null, null);
            if (cursor == null) {
                return false;
            }
            if (cursor.getCount() < 1) {
                cursor.close();
                return false;
            }
            cursor.moveToNext();
            byte[] dbid = cursor.getBlob(0);
            cursor.close();
            int id = dbid[0];

            Log.d(TAG, "onTune,  channelUri = " + channelUri + ", id = " + id);

            if (mCurrentChannelId == id)
            {
                Log.d(TAG, "onTune: We're already on this channel, not re-tuning...");
                return false;
            }

            else
            {
                // Stop the current playback
                TunerHAL.stop();

                // Inform that we don't have the video yet
                notifyVideoUnavailable(TvInputManager.VIDEO_UNAVAILABLE_REASON_TUNING);

                // Call our TunerHAL to tune on this new channel
                Log.e(TAG, "Calling TunerHAL.tune!!");
                TunerHAL.tune(id);

                // Flag that we're now ready to tune
                notifyVideoAvailable();

                // Update the current id
                mCurrentChannelId = id;
            }
                        
            return true;
        }

        @Override
        public void onSetCaptionEnabled(boolean enabled) 
        {
			Log.d(TAG, "onSetCaptionEnabled,  Enter...");
            // No-op
        }
    }
}