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

package com.broadcom.BcmKeyInterceptor;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.ComponentName;
import android.provider.Settings.Global;
import android.util.Log;
import android.os.UserHandle;
import android.view.KeyEvent;

/**
 * This class posts notifications that are used by BT pairing and Wifi WPS hardware button
 * It also allows the system/launcher to start the correct component when input event are
 * detected.
 *
 * Packages using this broadcast receiver should also have the corresponding input event
 * defined in the key layout file.
 */
public class BcmKeyInterceptorReceiver extends BroadcastReceiver {

    private static final String ACTION_GLOBAL_BUTTON = "android.intent.action.GLOBAL_BUTTON";
    private static final String ACTION_CONNECT_INPUT = "com.google.android.intent.action.CONNECT_INPUT";
    private static final String INTENT_EXTRA_NO_INPUT_MODE = "no_input_mode";

    private static final String TV_SETTING_PACKAGE = "com.android.tv.settings";
    private static final String TV_SETTING_WPS_ACTIVITY = "com.android.tv.settings.connectivity.WpsConnectionActivity";

    private static final String NETFLIX_APP = "com.netflix.ninja";

    private static final String TAG = "BcmKeyInterceptorReceiver";

    private static final boolean DEBUG = true;

    @Override
    public void onReceive(Context context, Intent intent) {

        if (DEBUG) Log.d(TAG, "onReceive");

        if (ACTION_GLOBAL_BUTTON.equals(intent.getAction())) {
            if (DEBUG) Log.d(TAG, "Got Intent " + intent.getAction());

            KeyEvent localKeyEvent = intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
            int localKeyCode = localKeyEvent.getKeyCode();
            int localAction = localKeyEvent.getAction();

            Intent localIntent = null;

            if (DEBUG) Log.d(TAG, "KeyCode is " + localKeyCode);

            if (localKeyCode == KeyEvent.KEYCODE_PAIRING) {
                /* Start BT pairing mode, assuming no input */
                if (DEBUG) Log.d(TAG, "Got BT Pairing key");

                localIntent = new Intent(ACTION_CONNECT_INPUT);
                localIntent.putExtra(Intent.EXTRA_KEY_EVENT, localKeyEvent);
                localIntent.putExtra(INTENT_EXTRA_NO_INPUT_MODE, true);
                localIntent.setFlags(intent.FLAG_RECEIVER_FOREGROUND);

                context.startActivity(localIntent);

            } else if (localKeyCode == KeyEvent.KEYCODE_TV_NETWORK && localAction == KeyEvent.ACTION_UP ) {
                /* Start WPS activity */
                if (DEBUG) Log.d(TAG, "Got Wifi WPS key");

                localIntent = new Intent();
                localIntent.setComponent(new ComponentName(TV_SETTING_PACKAGE, TV_SETTING_WPS_ACTIVITY));
                localIntent.setFlags(intent.FLAG_ACTIVITY_NEW_TASK);

                if (DEBUG) Log.d(TAG, "localIntent: " + localIntent);
                context.startActivity(localIntent);

            } else if (localKeyCode == KeyEvent.KEYCODE_BUTTON_16 && localAction == KeyEvent.ACTION_UP) {
                /* Start Netflix activity */
                if (DEBUG) Log.d(TAG, "Got Netflix key");

                localIntent = context.getPackageManager().getLeanbackLaunchIntentForPackage(NETFLIX_APP);
                if (localIntent != null) {
                    if (DEBUG) Log.d(TAG, "localIntent: " + localIntent);
                    context.startActivity(localIntent);
                } else {
                    if (DEBUG) Log.d(TAG, "LeanbackLaunchIntent for (" + NETFLIX_APP + ") is null" );
                }
            }
        }
    }
}
