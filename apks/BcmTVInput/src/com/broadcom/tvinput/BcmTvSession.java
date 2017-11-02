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

    private int mSelectedSubtitleTrackIndex;
    private boolean mCaptionEnabled;
    private String mInputId;
    private Context mContext;
    private TvInputManager.Hardware mHardware;
    private int mDeviceId;
    public BcmTvTunerService serviceContext;
    private boolean hasCurrentSurface = false;

    BcmTvSession(BcmTvTunerService context, TvInputInfo info) {
        super(context);
        if (BcmTvTunerService.DEBUG) Log.d(BcmTvTunerService.TAG, "BcmTvSession -- created");
        this.serviceContext = context;
        mCaptionEnabled = this.serviceContext.mCaptioningManager.isEnabled();
        mContext = context;
        this.serviceContext.mStreamConfigs = new ArrayList<>();

        if (BcmTvTunerService.DEBUG) Log.d(BcmTvTunerService.TAG, "BcmTvSession,  mDeviceId = " +mDeviceId);

        acquireHardware();
    }

    private void acquireHardware()
    {
        if (BcmTvTunerService.DEBUG) Log.d(BcmTvTunerService.TAG, "acquireHardware");

        if (mHardware != null) {
            if (BcmTvTunerService.DEBUG) Log.d(BcmTvTunerService.TAG, "acquireHardware is not null");
            return;
        }

        TvInputManager.HardwareCallback callback = new TvInputManager.HardwareCallback()
        {
            @Override
            public void onReleased() {
                mHardware = null;
            }

            @Override
            public void onStreamConfigChanged(TvStreamConfig[] configs) {
                if (BcmTvTunerService.DEBUG) Log.d(BcmTvTunerService.TAG, "onStreamConfigChanged "+configs.length);
                for (TvStreamConfig x: configs) {
                    Log.d(BcmTvTunerService.TAG, "onStreamConfigChanged,  stream_id = " + x.getStreamId() +
                            "width = " + x.getMaxWidth() +
                            "height = " + x.getMaxHeight() +
                            "type = " + x.getType());
                    serviceContext.mStreamConfigs.add(x);
                }
            }
        };

        mHardware = BcmTvTunerService.mManager.acquireTvInputHardware(mDeviceId, this.serviceContext.mInfo, callback);

        if (BcmTvTunerService.DEBUG) Log.d(BcmTvTunerService.TAG, "acquireHardware() mHardware=" + mHardware);
    }

    private TvStreamConfig getStreamConfig() {

        for (TvStreamConfig config : this.serviceContext.mStreamConfigs) {
            if (config.getType() == TvStreamConfig.STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE) {
                return config;
            }
        }
        return null;
    }

    @Override
    public void onRelease() {
        if (BcmTvTunerService.DEBUG) Log.d(BcmTvTunerService.TAG, "BcmTvSession -- onRelease()");
        BcmTunerJniBridge.getInstance().uninitGUI();
        BcmTvTunerService.mManager.releaseTvInputHardware(mDeviceId, mHardware);
    }

    @Override
    public void onSetCaptionEnabled(boolean enabled) {
        if (BcmTvTunerService.DEBUG) Log.d(BcmTvTunerService.TAG, "BcmTvSession -- onSetCaptionEnabled() " + enabled);
    }

    @Override
    public void onSetStreamVolume(@FloatRange(from = 0.0, to = 1.0) float volume) {
        if (BcmTvTunerService.DEBUG) Log.d(BcmTvTunerService.TAG, "BcmTvSession -- onSetStreamVolume() " + volume);
    }

    @Override
    public boolean onSetSurface(@Nullable Surface surface) {
        if (BcmTvTunerService.DEBUG) Log.d(BcmTvTunerService.TAG, "BcmTvSession -- onSetSurface()");

        if (surface == null) {
            this.serviceContext.mStreamConfigs = null;
            if (this.hasCurrentSurface) {
                this.hasCurrentSurface = false;
                BcmTunerJniBridge.getInstance().stop();
            }
            Log.e(BcmTvTunerService.TAG, "BcmTvSession -- onSetSurface(): surface is null");
            return false;
        }

        if (mHardware == null) {
            if (mHardware == null) {
                Log.e(BcmTvTunerService.TAG, "BcmTvSession -- onSetSurface(): mHardware is null");
                return false;
            }
        }

        TvStreamConfig config = null;
        if (surface != null) {
            config = getStreamConfig();
            if (config == null) {
                Log.d(BcmTvTunerService.TAG, "BcmTvSession -- onSetSurface(): config is null");
                return false;
            }
        }

        if (BcmTvTunerService.DEBUG) Log.d(BcmTvTunerService.TAG, "BcmTvSession -- onSetSurface(): Calling mHardware.setSurface surfaceNull:" + (surface == null)  + " configNull:" + (config == null));
        mHardware.setSurface(surface, config);

        //Should occur here according to documentation
        //Order of onTune and onSetSurface are reversed
        /*this.hasCurrentSurface = true;
        BcmTunerJniBridge.getInstance().initGUI();*/

        return true;
    }

    @Override
    public boolean onTune(Uri channelUri) {
        if (BcmTvTunerService.DEBUG) Log.d(BcmTvTunerService.TAG, "BcmTvSession -- onTune() uri=" + channelUri);

        //Notify video unavailable
        notifyVideoUnavailable(TvInputManager.VIDEO_UNAVAILABLE_REASON_TUNING);

        //This should be done in onSetSurface according to documentation
        //But onTune is called before onSetSurface which is reversed of what should happen
        if (!this.hasCurrentSurface) {
            this.hasCurrentSurface = true;
            BcmTunerJniBridge.getInstance().initGUI();
        }

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
                    return true;
                }
            }
            Log.e(BcmTvTunerService.TAG, "BcmTvSession -- onTune() cannot find channel from DB");

        } catch (Exception e) {
            Log.e(BcmTvTunerService.TAG, "Content provider query: " + Arrays.toString(e.getStackTrace()));
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }

        return false;
    }
}
