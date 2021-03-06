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
import android.os.SystemProperties;
import android.os.UserHandle;
import android.provider.Settings.Global;
import android.util.Log;
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

    private static final String ACTION_LEANBACK_LAUNCH = "com.google.android.leanbacklauncher.action.PARTNER_CUSTOMIZATION";
    private static final String SYSPROP_BOOT_WAKEUP = "dyn.nx.boot.wakeup";

    private static final String TV_SETTING_PACKAGE = "com.android.tv.settings";
    private static final String TV_SETTING_WPS_ACTIVITY = "com.android.tv.settings.connectivity.WpsConnectionActivity";

    private static final String BRCM_SPLASH_PACKAGE = "com.broadcom.BcmSplash";
    private static final String BRCM_SPLASH_ACTIVITY = "com.broadcom.BcmSplash.BcmSplashActivity";
    private static final String BRCM_SPLASH_EXTRA_TEXT = "text";
    private static final String BRCM_SPLASH_EXTRA_BC_INTENT = "broadcast_intent";
    private static final String BRCM_SPLASH_EXTRA_BC_PERMISSION = "broadcast_permission";
    private static final String BRCM_SPLASH_EXTRA_BC_WAIT_BOOTUP = "broadcast_wait_bootup";
    private static final String BRCM_SPLASH_EXTRA_BC_DELAY = "broadcast_delay";
    private static final long BRCM_SPLASH_BC_DELAY_MS = 7000;

    private static final String ACTION_NETFLIX_KEY = "com.netflix.ninja.intent.action.NETFLIX_KEY";
    private static final String NETFLIX_KEY_PERMISSION = "com.netflix.ninja.permission.NETFLIX_KEY";
    private static final String NETFLIX_KEY_POWER_MODE = "power_on";

    private static final String TAG = "BcmKeyInterceptorReceiver";

    private static final boolean DEBUG = true;

    private Intent createNetflixIntent() {
        Intent res = new Intent();
        res.setAction(ACTION_NETFLIX_KEY);
        // all Netflix key presses resulted in device power on (via WAKEUP input event)
        res.putExtra(NETFLIX_KEY_POWER_MODE, true);
        res.addFlags(Intent.FLAG_INCLUDE_STOPPED_PACKAGES);

        return res;
    }

    private void launchNetflix(Context context) {
        Intent localIntent = createNetflixIntent();

        if (DEBUG) Log.d(TAG, "localIntent: " + localIntent);
        context.sendBroadcast(localIntent, NETFLIX_KEY_PERMISSION);
    }

    private void launchNetflixSplash(Context context, boolean waitBootup, long delayMs) {
        Intent localIntent = new Intent();
        localIntent.setComponent(new ComponentName(BRCM_SPLASH_PACKAGE, BRCM_SPLASH_ACTIVITY));
        localIntent.putExtra(BRCM_SPLASH_EXTRA_TEXT, "Starting up Netflix...");
        localIntent.putExtra(BRCM_SPLASH_EXTRA_BC_INTENT, createNetflixIntent());
        localIntent.putExtra(BRCM_SPLASH_EXTRA_BC_PERMISSION, NETFLIX_KEY_PERMISSION);
        localIntent.putExtra(BRCM_SPLASH_EXTRA_BC_WAIT_BOOTUP, waitBootup);
        localIntent.putExtra(BRCM_SPLASH_EXTRA_BC_DELAY, delayMs);
        localIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        if (DEBUG) Log.d(TAG, "waitBootup: " + waitBootup + " localIntent: " + localIntent);
        context.startActivity(localIntent);
    }

    @Override
    public void onReceive(Context context, Intent intent) {

        if (DEBUG) Log.d(TAG, "onReceive");

        if (ACTION_LEANBACK_LAUNCH.equals(intent.getAction())) {
            if (DEBUG) Log.d(TAG, "Got Intent " + intent.getAction());

            if (SystemProperties.getBoolean(SYSPROP_BOOT_WAKEUP, false)) {
                Log.i(TAG, "Launching Netflix from power on");
                launchNetflixSplash(context, true, 0);
            }
        }
        else if (ACTION_GLOBAL_BUTTON.equals(intent.getAction())) {
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

                if (SystemProperties.getBoolean(SYSPROP_BOOT_WAKEUP, false)) {
                    Log.i(TAG, "Launching Netflix from resume");
                    launchNetflixSplash(context, false, BRCM_SPLASH_BC_DELAY_MS);
                }
                else {
                    launchNetflix(context);
                }
            }
        }
    }
}
