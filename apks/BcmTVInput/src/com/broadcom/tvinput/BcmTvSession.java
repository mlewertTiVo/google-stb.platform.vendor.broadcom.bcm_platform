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

import android.content.Context;
import android.database.Cursor;
import android.media.tv.TvInputInfo;
import android.media.tv.TvInputManager;
import android.media.tv.TvInputService;
import android.media.tv.TvStreamConfig;
import android.net.Uri;
import android.support.annotation.FloatRange;
import android.support.annotation.Nullable;
import android.util.Log;
import android.view.Surface;

import java.util.ArrayList;
import java.util.Arrays;

public class BcmTvSession extends TvInputService.Session {
    private String TAG = "TvSession";

    private int mSelectedSubtitleTrackIndex;
    private boolean mCaptionEnabled;
    private String mInputId;
    private Context mContext;
    private TvInputManager.Hardware mHardware;
    private int mDeviceId;
    private Surface mSurface;
    private boolean mHasCurrentSurface = false;

    public BcmTvTunerService serviceContext;

    BcmTvSession(BcmTvTunerService context, TvInputInfo info) {
        super(context);
        Log.d(TAG, "> BcmTvSession() instantiated.");
        this.serviceContext = context;
        mCaptionEnabled = this.serviceContext.mCaptioningManager.isEnabled();
        mContext = context;
        this.serviceContext.mStreamConfigs = new ArrayList<>();

        acquireHardware();
        Log.d(TAG, "<  BcmTvSession() mDeviceId = " +mDeviceId);
    }

    private void acquireHardware()
    {
        Log.d(TAG, "> acquireHardware()");

        if (mHardware != null) {
            Log.d(TAG, "acquireHardware is not null");
            return;
        }

        TvInputManager.HardwareCallback callback = new TvInputManager.HardwareCallback()
        {
            @Override
            public void onReleased() {
                Log.d(TAG, "<> onReleased()");
                mHardware = null;
            }

            @Override
            public void onStreamConfigChanged(TvStreamConfig[] configs) {
                Log.d(TAG, "onStreamConfigChanged "+configs.length);
                for (TvStreamConfig x: configs) {
                    Log.d(TAG, "onStreamConfigChanged,  stream_id = " + x.getStreamId() +
                            " width = " + x.getMaxWidth() +
                            " height = " + x.getMaxHeight() +
                            " type = " + x.getType());
                    serviceContext.mStreamConfigs.add(x);
                }
            }
        };

        mHardware = BcmTvTunerService.mManager.acquireTvInputHardware(mDeviceId, this.serviceContext.mInfo, callback);

        Log.d(TAG, "< acquireHardware() mHardware=" + mHardware);
    }

    private TvStreamConfig getStreamConfig() {

        for (TvStreamConfig config : this.serviceContext.mStreamConfigs) {
            if (config.getType() == TvStreamConfig.STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE) {
                Log.d(TAG, "getStreamConfig() found video source.");
                return config;
            }
        }
        return null;
    }

    @Override
    public void onRelease() {
        Log.d(TAG, "> onRelease()");

        //Notify video unavailable
        notifyVideoUnavailable(TvInputManager.INPUT_STATE_DISCONNECTED);

        BcmTunerJniBridge.getInstance().closeSession();
        if( mHardware != null )
        {
            // Following code is only needed for a system.
            // mHardware.setSurface(null, null);
            BcmTvTunerService.mManager.releaseTvInputHardware(mDeviceId, mHardware);
        }

        if(!mHasCurrentSurface)
            BcmTunerJniBridge.getInstance().releaseSdb();
        Log.d(TAG, "< onRelease()");
    }

    @Override
    public void onSetCaptionEnabled(boolean enabled) {
        Log.d(TAG, "<> onSetCaptionEnabled() " + enabled);
    }

    @Override
    public void onSetStreamVolume(@FloatRange(from = 0.0, to = 1.0) float volume) {
        Log.d(TAG, "<> onSetStreamVolume() " + volume);
    }

    @Override
    public boolean onSetSurface(@Nullable Surface surface) {
        Log.d(TAG, "> onSetSurface()");

        if (surface == null) {
            this.serviceContext.mStreamConfigs = null;
            if (mHasCurrentSurface) {
                mHasCurrentSurface = false;
                BcmTunerJniBridge.getInstance().stop();
            }
            Log.d(TAG, "< onSetSurface(): surface is null");
            return false;
        }

        if (mHardware == null) {
            Log.d(TAG, " onSetSurface(): mHardware is null");
            return false;
        }

        Log.d(TAG, "<> onSetSurface: has a valid surface.");
        BcmTunerJniBridge.getInstance().initialiseSdb(surface);

        mHasCurrentSurface = true;
        mSurface = surface;

        Log.d(TAG, "< onSetSurface()");
        return true;
    }

    @Override
    public boolean onTune(Uri channelUri) {

        boolean rtc = false;

        // TODO: run this method as AsyncTask

        Log.d(TAG, "> onTune() uri=" + channelUri);

        Cursor cursor = null;
        try {
            cursor = this.serviceContext.getContentResolver().query(channelUri, Channel.PROJECTION, null, null, null);

            if (cursor != null && cursor.getCount() > 0) {
                cursor.moveToFirst();
                Channel channel = Channel.fromCursor(cursor);

                //Tune to channel
                if (BcmTunerJniBridge.getInstance().tune(channel.getTransportStreamId(), channel.getServiceId()) == 0){
                    //Notify video available
                    notifyVideoAvailable();

                    Log.d(TAG, "< onTune() notifyVideoAvailable.");
                    return true;
                }
            }
            Log.e(TAG, " onTune() cannot find channel from DB");

        } catch (Exception e) {
            Log.e(TAG, "Content provider query: " + Arrays.toString(e.getStackTrace()));
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }

        Log.d(TAG, "< onTune() uri=" + channelUri);
        return rtc;
    }
}
