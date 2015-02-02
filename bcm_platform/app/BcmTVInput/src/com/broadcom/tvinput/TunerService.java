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

import android.app.AlarmManager;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.database.Cursor;
import android.media.tv.TvContract;
import android.media.tv.TvInputHardwareInfo;
import android.media.tv.TvInputInfo;
import android.media.tv.TvInputManager;
import android.media.tv.TvInputService;
import android.media.tv.TvStreamConfig;
import android.media.tv.TvTrackInfo;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.SystemClock;
import android.util.Log;
import android.util.SparseArray;
import android.view.Surface;

import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.Long;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Queue;
import java.util.Set;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/**
 * TV-Input service for Broadcom's Tuner
 * 
 */

public class TunerService extends TvInputService {
    private static final boolean DEBUG = true;
    private static final String TAG = TunerService.class.getSimpleName();
    private static final TvStreamConfig[] EMPTY_STREAM_CONFIGS = {};
    private static boolean streamerMode = false;

    // inputId -> deviceId
    private final Map<String, Integer> mDeviceIdMap = new HashMap<>();

    // deviceId -> inputId
    private final SparseArray<String> mInputIdMap = new SparseArray<String>();

    // inputId -> TvInputInfo
    private final Map<String, TvInputInfo> mInputMap = new HashMap<>();

    private TvInputManager mManager = null;
    private ResolveInfo mResolveInfo;
    private Handler mMainLoopHandler;

    private static ContentValues buildProgramValues(long channelId, ProgramInfo program, boolean insert) {
        ContentValues prog_values = new ContentValues();
        if (insert) {
            prog_values.put(TvContract.Programs.COLUMN_CHANNEL_ID, channelId);
            prog_values.put(TvContract.Programs.COLUMN_INTERNAL_PROVIDER_DATA, program.id.getBytes());
        }
        prog_values.put(TvContract.Programs.COLUMN_TITLE, program.title);
        prog_values.put(TvContract.Programs.COLUMN_START_TIME_UTC_MILLIS, program.start_time_utc_millis);
        prog_values.put(TvContract.Programs.COLUMN_END_TIME_UTC_MILLIS, program.end_time_utc_millis);
        prog_values.put(TvContract.Programs.COLUMN_SHORT_DESCRIPTION, program.short_description);
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
        if (!api.short_description.equals(pi.short_description)) {
            Log.d(TAG, "DatabaseSyncTask: different short_description '" + api.short_description + "' '" + pi.short_description + "'");
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

    private boolean isAtleastOneSessionTuned() {
        for(TunerTvInputSessionImpl session : sessionSet) {
            if (!session.mCurrentChannelId.equals("")) {
                return true;
            }
        }
        return false;
    }

    private void forgeTime() {
        if (streamerMode && isAtleastOneSessionTuned()) {
            long t = TunerHAL.getUtcTime();
            if (t != 0) {
                Log.e(TAG, "DatabaseSyncTask::Got time " + t);
                t *= 1000;
                if (Math.abs(t - System.currentTimeMillis()) >= 10000) {
                    Log.e(TAG, "DatabaseSyncTask::Calling setTime with " + t); 
                    AlarmManager am = (AlarmManager)TunerService.this.getSystemService(Context.ALARM_SERVICE); 
                    am.setTime(t);
                }
            }
        }
    }

    private class LogoJob {
        public String urlString;
        public Uri contentUri;
        public LogoJob(String s, Uri u) {
            urlString = s;
            contentUri = u;
        }
    }

    private class LogoLoader {
        private Thread thread;
        private LogoLoaderTask task;

        private final Lock lock = new ReentrantLock();
        private final Condition syncRequired = lock.newCondition();

        private Queue<LogoJob> queue=new LinkedList<LogoJob>();

        private void start() {
            if (task == null) {
            }
        }

        public void add(LogoJob j) {
            lock.lock();
            try {
                queue.offer(j);
                start();
                syncRequired.signal();
            } finally {
                lock.unlock();
            }
        }

        /*
        public void reset() {
            lock.lock();
            try {
                queue.clear();
                syncRequired.signal();
            } finally {
                lock.unlock();
            }
        }
        */

        private class LogoLoaderTask implements Runnable {

            void insertStream(Uri contentUri, InputStream is) {
                if (DEBUG) {
                    Log.d(TAG, "Inserting to " + contentUri);
                }
                try (OutputStream os = getContentResolver().openOutputStream(contentUri)) {
                    copy(is, os);
                } catch (IOException ioe) {
                    Log.e(TAG, "Failed to write to " + contentUri, ioe);
                }
            }

            void copy(InputStream is, OutputStream os) throws IOException {
                byte[] buffer = new byte[1024];
                int len;
                while ((len = is.read(buffer)) != -1) {
                    os.write(buffer, 0, len);
                }
            }

            private void download(LogoJob job) {
                try {
                    URL url = new URL(job.urlString); 
                    HttpURLConnection urlConnection = (HttpURLConnection)url.openConnection(); 
                    insertStream(job.contentUri, urlConnection.getInputStream());
                }
                catch (IOException e) {
                    e.printStackTrace();
                }
            }

            @Override
            public void run() {
                SystemClock.sleep(30000);
                lock.lock(); 
                while (true) {
                    LogoJob job = queue.poll();
                    if (job != null) {
                        lock.unlock();
                        // process download
                        Log.d(TAG, "LogoLoaderTask::processing " + job.urlString + " to " + job.contentUri.toString());
                        download(job); 
                        lock.lock();
                    }
                    else {
                        Log.d(TAG, "LogoLoaderTask::waiting");
                        try {
                            syncRequired.await(); 
                        } catch (InterruptedException e) {
                            Log.d(TAG, "LogoLoaderTask::exiting");
                            return;
                        }
                    }
                }
            }

            LogoLoaderTask() {
            }
        }

        LogoLoader() {
            task = new LogoLoaderTask();
            thread = new Thread(task);
            thread.start();
        }
    }

    private LogoLoader logoLoader;

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
                    TvContract.Programs.COLUMN_SHORT_DESCRIPTION,
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
                        oldprograms[op].short_description = cursor.getString(5);
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
                forgeTime();
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
    private Set<TunerTvInputSessionImpl> sessionSet = new HashSet<TunerTvInputSessionImpl>();

    private static void reflectedNotifySessionEvent(TunerTvInputSessionImpl s, String event, Bundle args)
    {
        Class<?> c;
        try {
            c = Class.forName("com.broadcom.tvinput.TunerService$TunerTvInputSessionImpl");
        } catch (ClassNotFoundException e) {
            Log.d(TAG, "reflectedNotifySessionEvent: did not find class");
            return;
        }
        Method m;
        try {
            m = c.getMethod("notifySessionEvent", new Class[] { String.class, Bundle.class }); 
        } catch (NoSuchMethodException e) {
            Log.d(TAG, "reflectedNotifySessionEvent: did not find method");
            return;
        }
        try {
            m.invoke(s, new Object[] { event, args }); 
        } catch (IllegalAccessException e) {
            Log.d(TAG, "reflectedNotifySessionEvent: IllegalAccessException");
        } catch (InvocationTargetException e) {
            Log.d(TAG, "reflectedNotifySessionEvent: InvocationTargetException");
        }
    }

    public static final int BROADCAST_EVENT_CHANNEL_LIST_CHANGED = 0;
    public static final int BROADCAST_EVENT_PROGRAM_LIST_CHANGED = 1;
    public static final int BROADCAST_EVENT_VIDEO_TRACK_LIST_CHANGED = 2;
    public static final int BROADCAST_EVENT_TRACK_SELECTED = 3;
    public static final int BROADCAST_EVENT_VIDEO_AVAILABLE = 4;
    public static final int BROADCAST_EVENT_SCANNING_START = 99;
    public static final int BROADCAST_EVENT_SCANNING_PROGRESS = 100; // 100 - 0% progress 200 100%
    public static final int BROADCAST_EVENT_SCANNING_COMPLETE = 201;

    private void sendScanStatusToAllSessions()
    {
        for(TunerTvInputSessionImpl session : sessionSet) {
            Bundle b = new Bundle();
            ScanInfo si = TunerHAL.getScanInfo();
            b.setClassLoader(ScanInfo.class.getClassLoader());
            b.putParcelable("scaninfo", si);
            //mCurrentSession.notifySessionEvent("scanstate", b);
            reflectedNotifySessionEvent(session, "scanstatus", b);
        }
    }

    private void sendTrackInfoToAllSessions()
    {
        TrackInfo vtia[] = TunerHAL.getTrackInfoList();
        if (vtia.length > 0) {
            List<TvTrackInfo> tracks = new ArrayList<>(); 
            for (TrackInfo vti : vtia) {
                TvTrackInfo info;
                if (vti.type == TvTrackInfo.TYPE_VIDEO) {
                    info = new TvTrackInfo.Builder(
                        TvTrackInfo.TYPE_VIDEO, vti.id)
                        .setVideoWidth(vti.squarePixelWidth)
                        .setVideoHeight(vti.squarePixelHeight)
                        .setVideoFrameRate(vti.frameRate)
                        .build();
                    tracks.add(info);
                }
                else if (vti.type == TvTrackInfo.TYPE_AUDIO) {
                    info = new TvTrackInfo.Builder(
                        TvTrackInfo.TYPE_AUDIO, vti.id)
                        .setLanguage(vti.lang)
                        .setAudioChannelCount(vti.channels)
                        .setAudioSampleRate(vti.sampleRate)
                        .build();
                    tracks.add(info);
                }
            }

            for(TunerTvInputSessionImpl session : sessionSet) {
                session.notifyTracksChanged(tracks);
            }
        }
    }

    private void sendTrackSelectedToAllSessions(int type, String trackId)
    {
        for(TunerTvInputSessionImpl session : sessionSet) {
            session.notifyTrackSelected(type, trackId);
        }
    }

    private void sendVideoAvailabilityToAllSessions(int param)
    {
        for(TunerTvInputSessionImpl session : sessionSet) {
            if (param != 0) {
                session.notifyVideoAvailable();
            }
            else {
                session.notifyVideoUnavailable(TvInputManager.VIDEO_UNAVAILABLE_REASON_TUNING);
            }
        }
    }

    public void onBroadcastEvent(int e, final int param, final String s) {
        Log.e(TAG, "Broadcast event: " + e + " " + param + " " + s);
        if (e == BROADCAST_EVENT_CHANNEL_LIST_CHANGED) {
            dbsync.setChannelListChanged();
        }
        else if (e == BROADCAST_EVENT_PROGRAM_LIST_CHANGED) {
            dbsync.setProgramListChanged();
        }
        else if (e == BROADCAST_EVENT_SCANNING_PROGRESS || e == BROADCAST_EVENT_SCANNING_COMPLETE) {
            mMainLoopHandler.post(new Runnable() {
                @Override
                public void run() {
                    sendScanStatusToAllSessions();
                }
            });
        }
        else if (e == BROADCAST_EVENT_VIDEO_TRACK_LIST_CHANGED) {
            mMainLoopHandler.post(new Runnable() {
                @Override
                public void run() {
                    sendTrackInfoToAllSessions();
                }
            });
        }
        else if (e == BROADCAST_EVENT_TRACK_SELECTED) {
            mMainLoopHandler.post(new Runnable() {
                @Override
                public void run() {
                    sendTrackSelectedToAllSessions(param, s);
                }
            });
        }
        else if (e == BROADCAST_EVENT_VIDEO_AVAILABLE) {
            mMainLoopHandler.post(new Runnable() {
                @Override
                public void run() {
                    sendVideoAvailabilityToAllSessions(param);
                }
            });
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
        mMainLoopHandler = new Handler(getMainLooper());
    }

    @Override
    public Session onCreateSession(String inputId) {
        if (DEBUG) 
			Log.d(TAG, "TunerService::onCreateSession(), inputId = " +inputId);

        // Lookup TvInputInfo from inputId
        TvInputInfo info = mInputMap.get(inputId);

        TunerTvInputSessionImpl newSession = new TunerTvInputSessionImpl(this, info);
        if (newSession != null) {
            sessionSet.add(newSession);
            if (DEBUG) 
                Log.d(TAG, "TunerService::onCreateSession(), sessions = " + sessionSet.size());
        }
        return newSession;
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
        logoLoader = new LogoLoader();
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

    public void addChannel(Context context, String inputId, ChannelInfo channel) 
    {
        ContentValues channel_values = new ContentValues();

        channel_values.put(TvContract.Channels.COLUMN_INPUT_ID, inputId);

        // Initialize the Channels class
        channel_values.put(TvContract.Channels.COLUMN_DISPLAY_NUMBER, channel.number);
        channel_values.put(TvContract.Channels.COLUMN_DISPLAY_NAME, channel.name);
        channel_values.put(TvContract.Channels.COLUMN_ORIGINAL_NETWORK_ID, channel.onid);
        channel_values.put(TvContract.Channels.COLUMN_TRANSPORT_STREAM_ID, channel.tsid);
        channel_values.put(TvContract.Channels.COLUMN_SERVICE_ID, channel.sid);
        channel_values.put(TvContract.Channels.COLUMN_BROWSABLE, 1);
        channel_values.put(TvContract.Channels.COLUMN_VIDEO_FORMAT, TvContract.Channels.VIDEO_FORMAT_1080P);
        channel_values.put(TvContract.Channels.COLUMN_INTERNAL_PROVIDER_DATA, channel.id.getBytes());
        channel_values.put(TvContract.Channels.COLUMN_TYPE, channel.type);

        Uri channelUri = context.getContentResolver().insert(TvContract.Channels.CONTENT_URI, channel_values);
        Log.d(TAG, "addChannel: " + channel.number + " " + channel.name + " " + channelUri);

        if (!channel.logoUrl.equals("")) {
            logoLoader.add(new LogoJob(channel.logoUrl, TvContract.buildChannelLogoUri(channelUri)));
        }

        long channelId = ContentUris.parseId(channelUri);
        Log.d(TAG, "channelId = " +channelId);

        // Initialize the Programs class
        ProgramInfo programs[] = TunerHAL.getProgramList(channel.id);
        for (ProgramInfo program : programs) 
        {
            insertProgram(context, channelId, program);
        }
    }

    public boolean sameChannelInfo(ChannelInfo a, ChannelInfo b) 
    {
        if (!a.id.equals(b.id)) {
            return false;
        }
        if (!a.name.equals(b.name)) {
            return false;
        }
        if (!a.number.equals(b.number)) {
            return false;
        }
        if (a.onid != b.onid) {
            return false;
        }
        if (a.tsid != b.tsid) {
            return false;
        }
        if (a.sid != b.sid) {
            return false;
        }
        return true; 
    }

    private void updateChannels(Context context, String inputId, ChannelInfo channels[]) {
        Uri uri = TvContract.buildChannelsUriForInput(inputId);
        // Get list of channelIds and bids
        String[] channelprojection = {
            TvContract.Channels._ID,
            TvContract.Channels.COLUMN_INTERNAL_PROVIDER_DATA,
            TvContract.Channels.COLUMN_DISPLAY_NAME,
            TvContract.Channels.COLUMN_DISPLAY_NUMBER,
            TvContract.Channels.COLUMN_ORIGINAL_NETWORK_ID,
            TvContract.Channels.COLUMN_TRANSPORT_STREAM_ID,
            TvContract.Channels.COLUMN_SERVICE_ID
        };

        List<String> skipList = new ArrayList<>();

        if (channels.length > 0) {
            Cursor channelcursor = getContentResolver().query(uri, channelprojection, null, null, null); 
            if (channelcursor != null) {
                List<Long> zapList = new ArrayList<>();
                boolean databaseEmpty = true;

                if (channelcursor.getCount() >= 1) {
                    databaseEmpty = false;
                    ChannelInfo dbci = new ChannelInfo();
                    while (channelcursor.moveToNext()) {
                        long channelId = channelcursor.getLong(0);
                        int i;
                        dbci.id = new String(channelcursor.getBlob(1));
                        dbci.name = channelcursor.getString(2);
                        dbci.number = channelcursor.getString(3);
                        dbci.onid = channelcursor.getInt(4);
                        dbci.tsid = channelcursor.getInt(5);
                        dbci.sid = channelcursor.getInt(6);
                        for (i = 0; i < channels.length; i++) {
                            if (sameChannelInfo(dbci, channels[i])) {
                                break;
                            }
                        }
                        if (i < channels.length) {
                            skipList.add(dbci.id);
                        }
                        else {
                            zapList.add(new Long(channelId));
                        }
                    }
                }
                channelcursor.close();
                if (skipList.size() == 0) {
                    if (!databaseEmpty) {
                        getContentResolver().delete(uri, null, null);
                    }
                }
                else {
                    for (Long channelIdWrapper : zapList) {
                        long channelId = channelIdWrapper.longValue();
                        Log.d(TAG, "updateChannels: deleting channelId " + channelId);
                        getContentResolver().delete(TvContract.buildChannelUri(channelId), null, null); 
                    }
                }
            }
        }
        // TODO: Next deletes everyone's programs, not just per channel
        getContentResolver().delete(TvContract.Programs.CONTENT_URI, null, null);

        for (ChannelInfo channel : channels) 
        {
            if (!skipList.contains(channel.id)) {
                Log.d(TAG, "updateChannels: adding " + channel.id + " (" + channel.name + ")");
                addChannel(context, inputId, channel); 
            }
            else {
                Log.d(TAG, "updateChannels: preserving " + channel.id + " (" + channel.name + ")");
            }
        }
    }

    private class TunerTvInputSessionImpl extends Session 
    {
        protected final TvInputInfo mInfo;
        protected final int mDeviceId;
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
        protected boolean setSurfaceLocal(Surface surface)
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
            }

            // Allow HAL to talk to HWC
            TunerHAL.setSurface();

            // Inform that we've got video running
            //notifyVideoAvailable();

            Log.d(TAG, "setSurface (local): Calling mHardware.setSurface");
            return mHardware.setSurface(surface, config);
        }

        @Override
        public boolean onSetSurface(Surface surface) 
        {
            if (DEBUG) 
				Log.d(TAG, "onSetSurface surface:" + surface);

            return setSurfaceLocal(surface);
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
            sessionSet.remove(this);

            if (DEBUG) 
                Log.d(TAG, "onRelease(), sessions = " + sessionSet.size());
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
                //notifyVideoAvailable();

                // Update the current id
                mCurrentChannelId = id;
                forgeTime();
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
            if (action.equals("startBlindScan")) {
                TunerHAL.startBlindScan();
            }
            else if (action.equals("scanStatus")) {
                sendScanStatusToAllSessions(); 
            }
            else if (action.equals("stopScan")) {
                TunerHAL.stopScan();
            }
            else if (action.equals("setStreamerMode")) {
                streamerMode = true;
                forgeTime();
            }

        }

        @Override
        public boolean onSelectTrack(int type, String trackId) {
            Log.d(TAG, "onSelectTrack: " + type + " " + trackId);
            return TunerHAL.selectTrack(type, trackId) == 0;
        }

    }
}
