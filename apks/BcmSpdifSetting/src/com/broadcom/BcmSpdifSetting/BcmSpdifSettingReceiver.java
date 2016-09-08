/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.broadcom.BcmSpdifSetting;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.ComponentName;
import android.os.SystemProperties;
import android.os.UserHandle;
import android.util.Log;
/**
 */
public class BcmSpdifSettingReceiver extends BroadcastReceiver {

    private static final String ACTION_ENABLE_SPDIF_DOLBY = "com.broadcom.action.ENABLE_SPDIF_DOLBY";
    private static final String INTENT_EXTRA_STATE = "state";

    private static final String NXPROP_ENABLE_SPDIF_DOLBY = "persist.nx.spdif.enable_dolby";

    private static final String ACTION_HDMI_PLUGGED = "android.intent.action.HDMI_PLUGGED";
    private static final String EXTRA_HDMI_PLUGGED_STATE = "state";

    private static final String TAG = "BcmSpdifSettingReceiver";

    private static final boolean DEBUG = true;

    @Override
    public void onReceive(Context context, Intent intent) {

        if (DEBUG) Log.d(TAG, "onReceive");

        if (ACTION_ENABLE_SPDIF_DOLBY.equals(intent.getAction())) {
            boolean state = intent.getBooleanExtra(INTENT_EXTRA_STATE, false);

            if (DEBUG) Log.d(TAG, "Got Intent " + intent.getAction() +
                                  " Extra " + INTENT_EXTRA_STATE + ":" + state);

            boolean oldState = SystemProperties.getBoolean(NXPROP_ENABLE_SPDIF_DOLBY, false);

            if (oldState != state) {
                Log.i(TAG, "Switching state from " + oldState + " to " + state);
                SystemProperties.set(NXPROP_ENABLE_SPDIF_DOLBY, state?"true":"false");
            }
        }
    }
}
