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
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.media.tv.TvInputHardwareInfo;
import android.media.tv.TvInputInfo;
import android.media.tv.TvInputManager;
import android.media.tv.TvInputService;
import android.media.tv.TvStreamConfig;
import android.os.Handler;
import android.os.HandlerThread;
import android.support.annotation.Nullable;
import android.util.Log;
import android.view.accessibility.CaptioningManager;

import java.util.ArrayList;

public class BcmTvTunerService extends TvInputService {
    protected static final String TAG = "TunerService";

    private HandlerThread mHandlerThread;
    private Handler mDbHandler;
    private Handler mHandler;
    protected CaptioningManager mCaptioningManager;
    protected BcmTvSession tvSession;
    protected static TvInputManager mManager;
    protected TvInputInfo mInfo;
    protected ArrayList<TvStreamConfig> mStreamConfigs;
    private boolean mIsTunerAvailable = false;

    @Override
    public void onCreate() {
        super.onCreate();
        mHandlerThread = new HandlerThread(getClass().getSimpleName());
        mHandlerThread.start();
        Log.d(TAG, "> onCreate() BcmTvTunerService thread started.");

        mDbHandler = new Handler(mHandlerThread.getLooper());
        mHandler = new Handler();
        mCaptioningManager = (CaptioningManager) getSystemService(Context.CAPTIONING_SERVICE);
        mManager = (TvInputManager) getSystemService(Context.TV_INPUT_SERVICE);

        // May need to initiate a scan again as the example nexus live_decode API's
        // are being used. Part of the live_decode cached data may be lost which
        // needs to be recreated.
        if (BcmTunerJniBridge.getInstance().servicesAvailable() == 0) {
            BcmTunerJniBridge.getInstance().servicesScan();
        }

        this.mIsTunerAvailable = true;
        Log.d(TAG, "< onCreate()");
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        this.mIsTunerAvailable = false;
        Log.d(TAG, "OnDestroy() Service closed!");
    }

    @Nullable
    @Override
    public Session onCreateSession(String inputId) {
        Log.d(TAG, "> onCreateSession() inputId: " + inputId);

        if (!this.mIsTunerAvailable) {
            return null;
        }
        BcmTvSession session = new BcmTvSession(this, mInfo);
        this.tvSession = session;
        Log.d(TAG, "< onCreateSession()");
        return session;
    }

    @Override
    public TvInputInfo onHardwareAdded(TvInputHardwareInfo hardwareInfo) {

        Log.d(TAG, "> onHardwareAdded() hardwareInfo: " + hardwareInfo.toString());

        // TODO: Only add this service once.

        if (!this.mIsTunerAvailable && hardwareInfo.getType() != TvInputHardwareInfo.TV_INPUT_TYPE_TUNER) {
            return null;
        }

        TvInputInfo info = null;
        Intent i = new Intent(SERVICE_INTERFACE).setClass(this, this.getClass());
        ResolveInfo resolveInfo = getPackageManager().resolveService(i, PackageManager.GET_SERVICES | PackageManager.GET_META_DATA);
        info = new TvInputInfo.Builder(this, resolveInfo).setLabel("Broadcom TV Tuner").build();

        Log.d(TAG, "< onHardwareAdded() returns: " + info);
        mInfo = info;
        return info;
    }

    @Override
    public String onHardwareRemoved(TvInputHardwareInfo hardwareInfo) {

        Log.d(TAG, "> onHardwareRemoved() hardwareInfo:" + hardwareInfo);

        int deviceId = hardwareInfo.getDeviceId();

        Log.d(TAG, "< onHardwareRemoved() returns: " + deviceId);
        return null;
    }
}
