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

import android.app.Activity;
import android.os.Bundle;
import android.widget.Toast;
import android.util.Log;


public class TunerSetupActivity extends Activity {

    private BcmTvTunerService tunerService;
    protected static final String TAG = "EpgController";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, "onCreate()");
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_tuner_setup);
        updateEpgData();

        //Show toast
        // TODO: Causes NullPointerException
        //Toast.makeText(this, "Channels added", Toast.LENGTH_LONG).show();
        this.finish();
    }

    private void updateEpgData() {
        Log.d(TAG, "updateEpgData()");
        EpgController epg = new EpgController(this.getApplicationContext());
        epg.updateEpgData(BcmTunerJniBridge.getInstance().getScanResults());
    }
}
