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

import android.content.ContentProviderOperation;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.OperationApplicationException;
import android.database.Cursor;
import android.media.tv.TvContract;
import android.net.Uri;
import android.os.RemoteException;
import android.support.annotation.NonNull;
import android.text.TextUtils;
import android.util.Log;
import android.util.LongSparseArray;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class EpgDataUtil {

    private static final int BATCH_OPERATION_COUNT = 100;
    private static final boolean forceDeleteChannels = false;

    public static List<Channel> getChannelsFromEpgSource(String tunerChannelData) {
        List<Channel> channelList = new ArrayList<>();

        //Get channels info from some EPG source
        ArrayList<EpgSourceChannelInfo> epgChannelList = EpgDataService.getEpgChannelData();

        //Only use the channels that match with that from what the tuner found while scanning
        ArrayList<BroadcastChannelInfo> broadcastChannelList = parseBroadcastChannelData(tunerChannelData);

        //TODO async task this
        for (EpgSourceChannelInfo epgChannel : epgChannelList) {
            for (int i = 0; i < broadcastChannelList.size(); i++) {
                if (broadcastChannelList.get(i).transportId == epgChannel.tsid &&
                        broadcastChannelList.get(i).programNumber == epgChannel.sid &&
                        broadcastChannelList.get(i).video.pid == epgChannel.vpid) {
                    Channel tvChannel = new Channel.Builder()
                            .setDisplayName(epgChannel.name)
                            .setDisplayNumber(epgChannel.number)
                            .setChannelLogo(epgChannel.logoUrl)
                            .setOriginalNetworkId(epgChannel.onid)
                            .setServiceId(epgChannel.sid)
                            .setTransportStreamId(epgChannel.tsid)
                            .setVideoPid(epgChannel.vpid)
                            .setType(epgChannel.type.toString())
                            .build();
                    channelList.add(tvChannel);
                }
            }
        }
        return channelList;
    }

    public static ArrayList<BroadcastChannelInfo> parseBroadcastChannelData(String tunerChannelData){
        ArrayList<BroadcastChannelInfo> channelList = new ArrayList<BroadcastChannelInfo>();

        try {
            JSONArray jsonBroadcastData = new JSONArray(tunerChannelData);
            int bSize = jsonBroadcastData.length();

            for (int i = 0; i < bSize; i++) {
                JSONObject channelInfo = jsonBroadcastData.getJSONObject(i);
                JSONArray programList = channelInfo.getJSONArray("program_list");
                int pSize = programList.length();

                for (int p = 0; p < pSize; p++) {
                    JSONObject program = programList.getJSONObject(p);
                    BroadcastChannelVideoInfo video = null;
                    JSONArray videoInfo = program.getJSONArray("video_pids");

                    //Only add channels with video
                    if (videoInfo.length() > 0) {
                        video = new BroadcastChannelVideoInfo(videoInfo.getJSONObject(0).getString("codec"),
                                videoInfo.getJSONObject(0).getInt("pid"));
                        ArrayList<BroadcastChannelAudioInfo> audioList = new ArrayList<>();
                        JSONArray audioInfo = program.getJSONArray("audio_pids");
                        int aSize = audioInfo.length();

                        for (int a = 0; a < aSize; a++) {
                            audioList.add(new BroadcastChannelAudioInfo(audioInfo.getJSONObject(a).getString("codec"),
                                    audioInfo.getJSONObject(a).getInt("pid")));
                        }

                        BroadcastChannelInfo info = new BroadcastChannelInfo(channelInfo.getString("name"),
                                channelInfo.getLong("freq"),
                                channelInfo.getInt("tsid"),
                                program.getInt("channel_number"),
                                program.getInt("program_number"),
                                program.getInt("pcr_pid"),
                                program.getInt("program_pid"),
                                video,
                                audioList);
                        channelList.add(info);
                    }
                }
            }

        } catch (JSONException e) {
            e.printStackTrace();
        }

        return channelList;
    }

    public static void updateChannels(Context context, String inputId, List<Channel> channels) {
        //Create a map from tsid and serviceID to channel row ID for existing channels.
        HashMap<String, Long> channelMap = new HashMap<>();

        Uri channelsUri = TvContract.buildChannelsUriForInput(inputId);
        String[] projection = {TvContract.Channels._ID, TvContract.Channels.COLUMN_SERVICE_ID, TvContract.Channels.COLUMN_TRANSPORT_STREAM_ID};
        ContentResolver resolver = context.getContentResolver();
        Cursor cursor = null;

        if (forceDeleteChannels) {
            resolver.delete(channelsUri, null, null);
        }

        try {
            cursor = resolver.query(channelsUri, projection, null, null, null);
            while (cursor != null && cursor.moveToNext()) {
                long rowId = cursor.getLong(0);
                int serviceId = cursor.getInt(1);
                int tsid = cursor.getInt(2);
                channelMap.put(tsid + "_" + serviceId, rowId);
            }
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }

        //If a channel exists, update it. If not, insert a new one.
        Map<Uri, String> logos = new HashMap<>();

        for (Channel channel : channels) {
            ContentValues values = new ContentValues();
            values.put(TvContract.Channels.COLUMN_INPUT_ID, inputId);
            values.putAll(channel.toContentValues());

            //Add defaults
            if (channel.getInputId() == null) {
                values.put(TvContract.Channels.COLUMN_INPUT_ID, inputId);
            }

            if (channel.getType() == null) {
                values.put(TvContract.Channels.COLUMN_TYPE, TvContract.Channels.TYPE_OTHER);
            }

            String key = channel.getTransportStreamId() + "_" + channel.getServiceId();
            Long rowId = channelMap.get(key);
            Uri uri;

            if (rowId == null) {
                uri = resolver.insert(TvContract.Channels.CONTENT_URI, values);
                if (BcmTvTunerService.DEBUG) Log.d(BcmTvTunerService.TAG, "Adding channel " + channel.toString()+ " at " + uri);
            } else {
                values.put(TvContract.Channels._ID, rowId);
                uri = TvContract.buildChannelUri(rowId);
                if (BcmTvTunerService.DEBUG) Log.d(BcmTvTunerService.TAG, "Updating channel " + channel.toString() + " at " + uri);
                resolver.update(uri, values, null, null);
                channelMap.remove(key);
            }

            if (channel.getChannelLogo() != null && !TextUtils.isEmpty(channel.getChannelLogo())) {
                logos.put(TvContract.buildChannelLogoUri(uri), channel.getChannelLogo());
            }
        }

        //Deletes channels which don't exist in the new feed.
        int size = channelMap.size();
        for (Long rowId : channelMap.values()) {
            if (BcmTvTunerService.DEBUG) Log.d(BcmTvTunerService.TAG, "Deleting channel " + rowId);
            resolver.delete(TvContract.buildChannelUri(rowId), null, null);
        }

        dumpChanneAndProgramlDb(resolver);
    }

    public static LongSparseArray<Channel> buildChannelMap(@NonNull ContentResolver resolver,
                                                           @NonNull String inputId) {
        Uri uri = TvContract.buildChannelsUriForInput(inputId);
        LongSparseArray<Channel> channelMap = new LongSparseArray<>();
        Cursor cursor = null;
        try {
            cursor = resolver.query(uri, Channel.PROJECTION, null, null, null);
            if (cursor == null || cursor.getCount() == 0) {
                if (BcmTvTunerService.DEBUG) Log.d(BcmTvTunerService.TAG, "Cursor is null or found no results");
                return null;
            }

            while (cursor.moveToNext()) {
                Channel nextChannel = Channel.fromCursor(cursor);
                channelMap.put(nextChannel.getId(), nextChannel);
            }
        } catch (Exception e) {
            if (BcmTvTunerService.DEBUG) Log.d(BcmTvTunerService.TAG, "Content provider query: " + Arrays.toString(e.getStackTrace()));
            return null;
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
        return channelMap;
    }

    public static void updatePrograms(ContentResolver contentResolver, Uri channelUri, List<Program> newPrograms) {
        final int fetchedProgramsCount = newPrograms.size();
        if (fetchedProgramsCount == 0) {
            return;
        }
        List<Program> oldPrograms = EpgDataUtil.getPrograms(contentResolver, channelUri);
        Program firstNewProgram = newPrograms.get(0);
        int oldProgramsIndex = 0;
        int newProgramsIndex = 0;
        // Skip the past programs. They will be automatically removed by the system.
        for (Program program : oldPrograms) {
            if (program.getEndTimeUtcMillis() < System.currentTimeMillis() ||
                    program.getEndTimeUtcMillis() < firstNewProgram.getStartTimeUtcMillis()) {
                oldProgramsIndex++;
            } else {
                break;
            }
        }
        // Compare the new programs with old programs one by one and update/delete the old one
        // or insert new program if there is no matching program in the database.
        ArrayList<ContentProviderOperation> ops = new ArrayList<>();

        while (newProgramsIndex < fetchedProgramsCount) {
            Program oldProgram = oldProgramsIndex < oldPrograms.size()
                    ? oldPrograms.get(oldProgramsIndex) : null;
            Program newProgram = newPrograms.get(newProgramsIndex);
            boolean addNewProgram = false;
            if (oldProgram != null) {
                if (oldProgram.equals(newProgram)) {
                    // Exact match. No need to update. Move on to the next programs.
                    oldProgramsIndex++;
                    newProgramsIndex++;
                } else if (oldProgram.getTitle().equals(newProgram.getTitle())
                        && oldProgram.getStartTimeUtcMillis() <= newProgram.getEndTimeUtcMillis()
                        && newProgram.getStartTimeUtcMillis() <= oldProgram.getEndTimeUtcMillis()) {

                    // Partial match. Update the old program with the new one.
                    // NOTE: Use 'update' in this case instead of 'insert' and 'delete'. There
                    // could be application specific settings which belong to the old program.
                    ops.add(ContentProviderOperation.newUpdate(
                            TvContract.buildProgramUri(oldProgram.getId()))
                            .withValues(newProgram.toContentValues())
                            .build());
                    oldProgramsIndex++;
                    newProgramsIndex++;
                } else if (oldProgram.getEndTimeUtcMillis()
                        < newProgram.getEndTimeUtcMillis()) {
                    // No match. Remove the old program first to see if the next program in
                    // {@code oldPrograms} partially matches the new program.
                    ops.add(ContentProviderOperation.newDelete(
                            TvContract.buildProgramUri(oldProgram.getId()))
                            .build());
                    oldProgramsIndex++;
                } else {
                    // No match. The new program does not match any of the old programs. Insert
                    // it as a new program.
                    addNewProgram = true;
                    newProgramsIndex++;
                }
            } else {
                // No old programs. Just insert new programs.
                addNewProgram = true;
                newProgramsIndex++;
            }
            if (addNewProgram) {
                ops.add(ContentProviderOperation
                        .newInsert(TvContract.Programs.CONTENT_URI)
                        .withValues(newProgram.toContentValues())
                        .build());
            }
            // Throttle the batch operation not to cause TransactionTooLargeException.
            if (ops.size() > BATCH_OPERATION_COUNT
                    || newProgramsIndex >= fetchedProgramsCount) {
                try {
                    contentResolver.applyBatch(TvContract.AUTHORITY, ops);
                } catch (RemoteException | OperationApplicationException e) {
                    Log.e(BcmTvTunerService.TAG, "Failed to insert programs.", e);
                    return;
                }
                ops.clear();
            }
        }
    }

    public static List<Program> getPrograms(ContentResolver resolver, Uri channelUri) {
        if (channelUri == null) {
            return null;
        }
        Uri uri = TvContract.buildProgramsUriForChannel(channelUri);
        List<Program> programs = new ArrayList<>();
        // TvProvider returns programs in chronological order by default.
        Cursor cursor = null;
        try {
            cursor = resolver.query(uri, Program.PROJECTION, null, null, null);
            if (cursor == null || cursor.getCount() == 0) {
                return programs;
            }
            while (cursor.moveToNext()) {
                programs.add(Program.fromCursor(cursor));
            }
        } catch (Exception e) {
            Log.w(BcmTvTunerService.TAG, "Unable to get programs for " + channelUri, e);
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
        return programs;
    }

    public static List<Program> getPrograms(Channel channel, List<Program> programs,
                                            long startTimeMs, long endTimeMs) {
        if (startTimeMs > endTimeMs) {
            throw new IllegalArgumentException("Start time must be before end time");
        }
        List<Program> programForGivenTime = new ArrayList<>();

        //Fake some repeat programs
        long totalDurationMs = 0;
        for (Program program : programs) {
            totalDurationMs +=
                    (program.getEndTimeUtcMillis() - program.getStartTimeUtcMillis());
        }
        if (totalDurationMs <= 0) {
            throw new IllegalArgumentException("The duration of all programs must be greater than 0ms.");
        }

        long programStartTimeMs = startTimeMs - startTimeMs % totalDurationMs;
        int i = 0;
        final int programCount = programs.size();

        while (programStartTimeMs < endTimeMs) {
            Program programInfo = programs.get(i++ % programCount);
            long programEndTimeMs = programStartTimeMs + totalDurationMs;

            if (programInfo.getEndTimeUtcMillis() > -1
                    && programInfo.getStartTimeUtcMillis() > -1) {
                programEndTimeMs = programStartTimeMs +
                        (programInfo.getEndTimeUtcMillis()
                                - programInfo.getStartTimeUtcMillis());
            }

            if (programEndTimeMs < startTimeMs) {
                programStartTimeMs = programEndTimeMs;
                continue;
            }
            programForGivenTime.add(new Program.Builder(programInfo)
                    .setChannelId(channel.getId())
                    .setStartTimeUtcMillis(programStartTimeMs)
                    .setEndTimeUtcMillis(programEndTimeMs)
                    .build()
            );
            programStartTimeMs = programEndTimeMs;
        }
        return programForGivenTime;
    }

    public static void dumpChanneAndProgramlDb(ContentResolver contentResolver){
        Uri allChannelsUri = TvContract.buildChannelsUriForInput(EpgController.BCM_TVINPUT_ID);
        Cursor channelCursor = contentResolver.query(allChannelsUri, Channel.PROJECTION, null, null, null);
        try {
            while (channelCursor.moveToNext()) {
                StringBuilder channelString = new StringBuilder();

                for (int i = 0; i < Channel.PROJECTION.length; i++) {
                    channelString.append(Channel.PROJECTION[i] + ":" + channelCursor.getString(i) + ",");
                }
                channelString.append("\n");

                //get program
                Uri program = TvContract.buildProgramsUriForChannel(channelCursor.getLong(0));
                Cursor programCursor = contentResolver.query(program, Program.PROJECTION, null, null, null);

                while (programCursor.moveToNext()) {
                    channelString.append("-->");
                    for (int i = 0; i < Program.PROJECTION.length; i++) {
                        channelString.append(programCursor.getString(i) + ":");
                    }
                    channelString.append("\n");
                }
                Log.e(BcmTvTunerService.TAG, channelString.toString());
                programCursor.close();
            }
        } finally {
            channelCursor.close();
        }
    }
}
