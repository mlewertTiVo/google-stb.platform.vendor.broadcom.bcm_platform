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

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import org.xmlpull.v1.XmlPullParserException;

import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.hardware.hdmi.HdmiDeviceInfo;
import android.media.AudioManager;
import android.media.tv.TvContentRating;
import android.media.tv.TvContract;
import android.media.tv.TvInputHardwareInfo;
import android.media.tv.TvInputInfo;
import android.media.tv.TvInputManager;
import android.media.tv.TvInputService;
import android.media.tv.TvStreamConfig;
import android.net.Uri;
import android.os.Handler;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;
import android.util.Log;
import android.util.SparseArray;
import android.view.KeyEvent;
import android.view.Surface;

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

    private static ContentValues buildProgramValues(long channelId, ProgramInfo program, boolean insert) {
        ContentValues prog_values = new ContentValues();
        if (insert) {
            prog_values.put(TvContract.Programs.COLUMN_CHANNEL_ID, channelId);
            prog_values.put(TvContract.Programs.COLUMN_INTERNAL_PROVIDER_DATA, program.id.getBytes());
        }
        prog_values.put(TvContract.Programs.COLUMN_TITLE, program.title);
        prog_values.put(TvContract.Programs.COLUMN_START_TIME_UTC_MILLIS, program.start_time_utc_millis);
        prog_values.put(TvContract.Programs.COLUMN_END_TIME_UTC_MILLIS, program.end_time_utc_millis);
        return prog_values;
    }

    private class AndroidProgramInfo extends ProgramInfo {
        long programId;
    };

    private static boolean differentInfo(AndroidProgramInfo api, ProgramInfo pi) {
        if (!api.title.equals(pi.title)) {
            Log.d(TAG, "DatabaseSyncTask: different title '" + api.title + "' '" + pi.title + "'");
            return true;
        }
        if (api.start_time_utc_millis != pi.start_time_utc_millis) {
            Log.d(TAG, "DatabaseSyncTask: different start " + api.start_time_utc_millis + " " + pi.start_time_utc_millis);
            return true;
        }
        if (api.end_time_utc_millis != pi.end_time_utc_millis) {
            Log.d(TAG, "DatabaseSyncTask: different end " + api.end_time_utc_millis + " " + pi.end_time_utc_millis);
            return true;
        }
        return false;
    }

    private static void insertProgram(Context context, long channelId, ProgramInfo program) {
        ContentValues prog_values = buildProgramValues(channelId, program, true);
        context.getContentResolver().insert(TvContract.Programs.CONTENT_URI, prog_values);
    }

    private static void updateProgram(Context context, long programId, ProgramInfo program) {
        ContentValues prog_values = buildProgramValues(0, program, false);
        context.getContentResolver().update(TvContract.buildProgramUri(programId), prog_values, null, null);
    }

    private class DatabaseSync {
        private Thread thread;
        private DatabaseSyncTask task;

        private final Lock lock = new ReentrantLock();
        private final Condition syncRequired = lock.newCondition();
        private boolean channelListChanged = false;
        private boolean programListChanged = false;

        public void setChannelListChanged() {
            lock.lock();
            try {
                channelListChanged = true;
                syncRequired.signal();
            } finally {
                lock.unlock();
            }
        }

        public void setProgramListChanged() {
            lock.lock();
            try {
                programListChanged = true;
                syncRequired.signal();
            } finally {
                lock.unlock();
            }
        }

        private class DatabaseSyncTask implements Runnable {

            private final String inputId;

            private void updateProgramsForChannel(long channelId, String id)
            {
                ProgramInfo newprograms[] = TunerHAL.getProgramList(id);

                Uri uri = TvContract.buildProgramsUriForChannel(channelId);
                String[] projection = {
                    TvContract.Channels.COLUMN_INTERNAL_PROVIDER_DATA,
                    TvContract.Programs.COLUMN_TITLE,
                    TvContract.Programs.COLUMN_START_TIME_UTC_MILLIS,
                    TvContract.Programs.COLUMN_END_TIME_UTC_MILLIS,
                    TvContract.Programs._ID,
                };
                Cursor cursor = getContentResolver().query(uri, projection, null, null, null);
                if (cursor == null) {
                    return;
                }
                AndroidProgramInfo oldprograms[] = new AndroidProgramInfo[cursor.getCount()];
                if (cursor.getCount() > 0) {
                    int op = 0;
                    while (cursor.moveToNext()) {
                        oldprograms[op] = new AndroidProgramInfo();
                        oldprograms[op].id = new String(cursor.getBlob(0));
                        oldprograms[op].title = cursor.getString(1);
                        oldprograms[op].start_time_utc_millis = cursor.getLong(2);
                        oldprograms[op].end_time_utc_millis = cursor.getLong(3);
                        oldprograms[op].programId = cursor.getLong(4);
                        op++;
                    }
                }
                // Add new ids
                for (ProgramInfo npi : newprograms) {
                    boolean inoldlist = false;
                    for (AndroidProgramInfo opi : oldprograms) {
                        if (npi.id.equals(opi.id)) {
                            inoldlist = true;
                            break;
                        }
                    }
                    if (!inoldlist) {
                        Log.d(TAG, "DatabaseSyncTask.updateProgramsForChannel(" + id + "): + " + npi.id + " " + npi.title);
                        TunerService.insertProgram(TunerService.this, channelId, npi);
                    }
                }
                // Delete/update old ids
                for (AndroidProgramInfo opi : oldprograms) {
                    boolean innewlist = false;
                    for (ProgramInfo npi : newprograms) {
                        if (npi.id.equals(opi.id)) {
                            innewlist = true;
                            if (differentInfo(opi, npi)) {
                                Log.d(TAG, "DatabaseSyncTask.updateProgramsForChannel(" + id + "): ! " + opi.id);
                                TunerService.updateProgram(TunerService.this, opi.programId, npi);
                            }
                            break;
                        }
                    }
                    if (!innewlist) {
                        Log.d(TAG, "DatabaseSyncTask.updateProgramsForChannel(" + id + "): - " + opi.id);
                        Uri puri = TvContract.buildProgramUri(opi.programId);
                        getContentResolver().delete(puri, null, null);
                    }
                }
                cursor.close(); 
            }

            private void updatePrograms()
            {
                // Get list of channelIds and bids
                String[] channelprojection = { TvContract.Channels._ID, TvContract.Channels.COLUMN_INTERNAL_PROVIDER_DATA };
                Uri uri = TvContract.buildChannelsUriForInput(inputId);
                Cursor channelcursor = getContentResolver().query(uri, channelprojection, null, null, null);
                if (channelcursor == null) {
                    return;
                }
                if (channelcursor.getCount() < 1) {
                    channelcursor.close();
                    return;
                }

                while (channelcursor.moveToNext()) {
                    long channelId = channelcursor.getLong(0);
                    String id = new String(channelcursor.getBlob(1));
                    updateProgramsForChannel(channelId, id);
                }
                channelcursor.close();

            }

            @Override
            public void run() {
                lock.lock();
                while (true) {
                    if (channelListChanged) {
                        //Context context = (Context) objs[0];
                        channelListChanged = false;
                        lock.unlock();
                        ChannelInfo[] civ = TunerHAL.getChannelList();

                        Log.e(TAG, "DatabaseSyncTask::Got channels " + civ.length);
                        updateChannels(TunerService.this, inputId, civ);
                        lock.lock();
                    }
                    else if (programListChanged) {
                        //Context context = (Context) objs[0];
                        programListChanged = false;
                        lock.unlock();
                        updatePrograms();
                        lock.lock();
                    }
                    else {
                        Log.d(TAG, "DatabaseSyncTask::waiting");
                        try {
                            syncRequired.await(); 
                        } catch (InterruptedException e) {
                            Log.d(TAG, "DatabaseSyncTask::exiting");
                            return;
                        }
                    }
                }
            }

            DatabaseSyncTask(String id) {
                inputId = id;
            }
        }

        DatabaseSync(String id) {
            task = new DatabaseSyncTask(id);
            thread = new Thread(task);
            thread.start();
        }
    }

    private DatabaseSync dbsync;

    public static final int BROADCAST_EVENT_CHANNEL_LIST_CHANGED = 0;
    public static final int BROADCAST_EVENT_PROGRAM_LIST_CHANGED = 1;

    public void onBroadcastEvent(int e) {
        Log.e(TAG, "Broadcast event: " + e);
        if (e == BROADCAST_EVENT_CHANNEL_LIST_CHANGED) {
            dbsync.setChannelListChanged();
        }
        else if (e == BROADCAST_EVENT_PROGRAM_LIST_CHANGED) {
            dbsync.setProgramListChanged();
        }
    }

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

        if (mInputIdMap.indexOfKey(deviceId) >= 0) {
            Log.e(TAG, "Already created TvInputInfo for deviceId=" + deviceId);
            return null;
        }

        Log.e(TAG, "Calling TunerHAL.initialize!!");
        if (TunerHAL.initialize(this) < 0) {
            Log.e(TAG, "TunerHAL.initialize failed!!");
            return null;
        }

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
        Log.d(TAG, "kicking off sync task");
        dbsync = new DatabaseSync(info.getId());
        dbsync.setChannelListChanged();
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
            channel_values.put(TvContract.Channels.COLUMN_INTERNAL_PROVIDER_DATA, channel.id.getBytes());

            Uri channelUri = context.getContentResolver().insert(TvContract.Channels.CONTENT_URI, channel_values);
	    Log.d(TAG, "populateChannels: " + channel.number + " " + channel.name + " " + channelUri);

            long channelId = ContentUris.parseId(channelUri);
            Log.d(TAG, "channelId = " +channelId);

            // Initialize the Programs class
            ProgramInfo programs[] = TunerHAL.getProgramList(channel.id);
            for (ProgramInfo program : programs) 
            {
                insertProgram(context, channelId, program);
            }
            
        }
        Log.d(TAG, "populateChannels: finish");
    }

    private void updateChannels(Context context, String inputId, ChannelInfo channels[]) {
        Uri uri = TvContract.buildChannelsUriForInput(inputId);
        getContentResolver().delete(uri, null, null);
        // TODO: Next deletes everyone's programs, not just per channel
        getContentResolver().delete(TvContract.Programs.CONTENT_URI, null, null);
        populateChannels(context, inputId, channels);
    }

    private class TunerTvInputSessionImpl extends Session 
    {
        protected final TvInputInfo mInfo;
        protected final int mDeviceId;
        protected Surface mSurface = null;
        protected String mCurrentChannelId = "";
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
            TunerHAL.release();
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
            String id = new String(cursor.getBlob(0));
            cursor.close();

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
                // Should really come from underlying broadcast stack
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

        @Override
        public void onAppPrivateCommand(String action, Bundle data) 
        {
            Log.d(TAG, "onAppPrivateCommand: " + action);
            if (action.equals("scan")) {
                TunerHAL.scan();
            }
        }
    }
}