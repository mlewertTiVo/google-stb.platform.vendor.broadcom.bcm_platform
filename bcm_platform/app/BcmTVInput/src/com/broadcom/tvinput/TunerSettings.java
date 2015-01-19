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

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.ServiceInfo;
import android.media.tv.TvInputInfo;
import android.media.tv.TvInputManager;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.View;
import android.widget.ProgressBar;
import android.widget.TextView;


/**
 * TV-Input activity for Broadcom's Tuner
 * 
 */
public class TunerSettings extends Activity {
    private final Handler mHandler = new Handler();
    
    private static final String TAG = TunerSettings.class.getSimpleName();

    private TvInputManager mTvInputManager;

    private String getInputIdFromComponentName(Context context, ComponentName name) {
        for (TvInputInfo info : mTvInputManager.getTvInputList()) {
            ServiceInfo si = info.getServiceInfo();
            if (new ComponentName(si.packageName, si.name).equals(name)) {
                return info.getId();
            }
        }
        return null;
    }

    private TvInputManager.Session mSession = null;

    private class MySessionCallback extends TvInputManager.SessionCallback {
        @Override
        public void onSessionCreated(TvInputManager.Session session) {
            mSession = session;
            mSession.sendAppPrivateCommand("scanStatus", null);
        }
        @Override
        public void onSessionEvent(TvInputManager.Session session, String eventType, Bundle eventArgs) {
            if (eventType.equals("scanstatus")) {
                eventArgs.setClassLoader(ScanInfo.class.getClassLoader());
                ScanInfo si = eventArgs.getParcelable("scaninfo");
                TextView tv = (TextView) findViewById(R.id.textview_tv);
                if (si.busy) {
                    if (si.valid) {
                        tv.setText("Scanning: "
                                    + si.progress
                                    + "% "
                                    + si.channels
                                    + " found ("
                                    + si.TVChannels
                                    + "/"
                                    + si.radioChannels
                                    + "/"
                                    + si.dataChannels
                                    + ") qual "
                                    + si.signalQualityPercent
                                    + "% snr "
                                    + si.signalStrengthPercent
                                    + "%"); 
                    }
                    else {
                        tv.setText("Scanning");
                    }
                }
                else {
                    tv.setText("No scan in progress");
                }
                ProgressBar progress = (ProgressBar) findViewById(R.id.progressbar_scan);
                progress.setProgress(si.valid ? si.progress : 0);
            }
        }
    }

    private MySessionCallback mSessionCallback;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.tuner_settings);
		Log.e(TAG, "onCreate - Enter...");

        String inputId;

        mTvInputManager = (TvInputManager) this.getSystemService(Context.TV_INPUT_SERVICE);

        inputId = getInputIdFromComponentName(this,
                new ComponentName(this, TunerService.class));
        Log.e(TAG, "onCreate - mInputId = " + inputId);

        mSessionCallback = new MySessionCallback();
        mTvInputManager.createSession(inputId, mSessionCallback, mHandler);
    }

    public void startBlindScan(View v) {
        mSession.sendAppPrivateCommand("startBlindScan", null);
    }

    public void stopScan(View v) {
        mSession.sendAppPrivateCommand("stopScan", null);
    }

    public void setStreamerMode(View v) {
        mSession.sendAppPrivateCommand("setStreamerMode", null);
    }

    @Override
    public void onDestroy() {
        if (mSession != null) {
            mSession.release(); 
        }
        super.onDestroy();
    }
}
