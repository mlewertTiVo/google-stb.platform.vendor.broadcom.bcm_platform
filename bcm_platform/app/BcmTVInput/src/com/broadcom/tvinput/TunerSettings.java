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
import android.widget.RatingBar;
import android.widget.TextView;
import android.widget.ToggleButton;


/**
 * TV-Input activity for Broadcom's Tuner
 * 
 */
public class TunerSettings extends Activity {
    private final Handler mHandler = new Handler();
    
    private static final String TAG = TunerSettings.class.getSimpleName();

    private TvInputManager mTvInputManager;

    private String getInputIdFromComponentName(ComponentName name) {
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
            if (eventType.equals("scanStatus")) {
                eventArgs.setClassLoader(ScanInfo.class.getClassLoader());
                ScanInfo si = eventArgs.getParcelable("scanInfo");
                ToggleButton start_stop = (ToggleButton) findViewById(R.id.toggle_scan);
                ProgressBar progress = (ProgressBar) findViewById(R.id.progressbar_scan);
                RatingBar strength = (RatingBar) findViewById(R.id.rating_strength);
                RatingBar quality = (RatingBar) findViewById(R.id.rating_quality);
                TextView all = (TextView) findViewById(R.id.text_all_channels);
                TextView tv = (TextView) findViewById(R.id.text_tv_channels);
                TextView radio = (TextView) findViewById(R.id.text_radio_channels);
                TextView data = (TextView) findViewById(R.id.text_data_channels);
                if (si.valid) {
                    all.setText(String.valueOf(si.channels));
                    tv.setText(String.valueOf(si.TVChannels));
                    radio.setText(String.valueOf(si.radioChannels));
                    data.setText(String.valueOf(si.dataChannels));
                    if (si.busy) {
                        progress.setProgress(si.progress);
                        strength.setProgress(si.signalStrengthPercent);
                        quality.setProgress(si.signalStrengthPercent);
                    }
                    else {
                        start_stop.setChecked(false);
                        progress.setProgress(0);
                        strength.setProgress(0);
                        quality.setProgress(0);
                    }
                }
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

        inputId = getInputIdFromComponentName(
                new ComponentName(this, TunerService.class));
        Log.e(TAG, "onCreate - mInputId = " + inputId);

        mSessionCallback = new MySessionCallback();
        mTvInputManager.createSession(inputId, mSessionCallback, mHandler);
    }

    public void startStopScan(View v) {
        ToggleButton start_stop = (ToggleButton)v;
        if (start_stop.isChecked()) {
            ScanParams sp = new ScanParams();
            sp.deliverySystem = ScanParams.DeliverySystem.Dvbt;
            sp.mode = ScanParams.ScanMode.Blind;
            Bundle b = new Bundle();
            b.setClassLoader(ScanParams.class.getClassLoader());
            b.putParcelable("scanParams", sp);
            mSession.sendAppPrivateCommand("startScan", b);
        }
        else {
            mSession.sendAppPrivateCommand("stopScan", null);
        }
    }

    public void setClockFromStream(View v) {
        mSession.sendAppPrivateCommand("setClockFromStream", null);
    }

    @Override
    public void onDestroy() {
        if (mSession != null) {
            mSession.release(); 
        }
        super.onDestroy();
    }
}
