/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.broadcom.customizer;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.ComponentName;
import android.content.pm.PackageManager;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.KeyEvent;

/**
 * This class posts notifications that are used to populate the Partner Row of the Leanback Launcher
 * It also allows the system/launcher to find the correct partner customization
 * package.
 * This class also posts notifications that are used by BT pairing and Wifi WPS hardware button
 * It also allows the system/launcher to start the correct component when input event are
 * detected.
 *
 * Packages using this broadcast receiver must also be a system app to be used for
 * partner customization.
 * Packages using this broadcast receiver should also have the corresponding input event
 * defined in the key layout file.
 */
public class BcmCustomizerReceiver extends BroadcastReceiver {
    private static final String TAG = "BcmCustomizerReceiver";
    private static final String ACTION_PARTNER_CUSTOMIZATION =
            "com.google.android.leanbacklauncher.action.PARTNER_CUSTOMIZATION";

    private static final String EXTRA_ROW_WRAPPING_CUTOFF =
            "com.google.android.leanbacklauncher.extra.ROW_WRAPPING_CUTOFF";

    private static final String PARTNER_GROUP = "partner_row_entry";
    private static final String BLACKLIST_PACKAGE = "com.google.android.leanbacklauncher.replacespackage";

    private static final String BCM_ADJUST_SCREEN_OFFSET_PKG_NAME = "com.broadcom.BcmAdjustScreenOffset";
    private static final String BCM_OTA_UPDATER_PKG_NAME = "com.broadcom.BcmOtaUpdater";
    private static final String BCM_SIDEBAND_VIEWER_PKG_NAME = "com.broadcom.sideband";
    private static final String BCM_URI_PLAYER_PKG_NAME = "com.broadcom.BcmUriPlayer";

    private Context mContext;
    private NotificationManager mNotifMan;
    private PackageManager mPkgMan;

    // Cutoff value for when the Launcher displays the Partner row as a single
    // row, or a two row grid. Can be used for correctly positioning the partner
    // app entries.
    private int mRowCutoff = 0;

    private static final String ACTION_GLOBAL_BUTTON = "android.intent.action.GLOBAL_BUTTON";
    private static final String ACTION_CONNECT_INPUT = "com.google.android.intent.action.CONNECT_INPUT";
    private static final String INTENT_EXTRA_NO_INPUT_MODE = "no_input_mode";

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

    private static final boolean DEBUG = false;

    @Override
    public void onReceive(Context context, Intent intent) {
        if (DEBUG) Log.d(TAG, "onReceive intent=" + intent);
        if (mContext == null) {
            mContext = context;
            mNotifMan = (NotificationManager)
                    mContext.getSystemService(Context.NOTIFICATION_SERVICE);
            mPkgMan = mContext.getPackageManager();
        }

        String action = intent.getAction();
        if (Intent.ACTION_PACKAGE_ADDED.equals(action) ||
                Intent.ACTION_PACKAGE_REMOVED.equals(action)) {
            if (launchNetflixAutomatically()) {
                Log.i(TAG, "Launching Netflix from power on");
                launchNetflixSplash(context, true, 0);
            }
            postNotification(getPackageName(intent));
        } else if (ACTION_PARTNER_CUSTOMIZATION.equals(action)) {
            mRowCutoff = intent.getIntExtra(EXTRA_ROW_WRAPPING_CUTOFF, 0);
            postNotification(BCM_SIDEBAND_VIEWER_PKG_NAME);
            postNotification(BCM_OTA_UPDATER_PKG_NAME);
            postNotification(BCM_ADJUST_SCREEN_OFFSET_PKG_NAME);
            postNotification(BCM_URI_PLAYER_PKG_NAME);
        } else if (ACTION_GLOBAL_BUTTON.equals(action)) {
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
                if (DEBUG) Log.d(TAG, "localIntent: " + localIntent);
                context.startActivity(localIntent);
            } else if (localKeyCode == KeyEvent.KEYCODE_TV_NETWORK &&
                    localAction == KeyEvent.ACTION_UP) {
                /* Start WPS activity */
                if (DEBUG) Log.d(TAG, "Got Wifi WPS key");

                localIntent = new Intent();
                localIntent.setComponent(new ComponentName(TV_SETTING_PACKAGE, TV_SETTING_WPS_ACTIVITY));
                localIntent.setFlags(intent.FLAG_ACTIVITY_NEW_TASK);
                if (DEBUG) Log.d(TAG, "localIntent: " + localIntent);
                context.startActivity(localIntent);
            } else if (localKeyCode == KeyEvent.KEYCODE_BUTTON_16 &&
                    localAction == KeyEvent.ACTION_UP) {
                /* Start Netflix activity */
                if (DEBUG) Log.d(TAG, "Got Netflix key");
                if (launchNetflixAutomatically()) {
                    Log.i(TAG, "Launching Netflix from resume");
                    launchNetflixSplash(context, false, BRCM_SPLASH_BC_DELAY_MS);
                } else {
                    launchNetflix(context);
                }
            }
        }
    }

    private void postNotification(String pkgName) {
        int sort;
        int resId;
        int backupResId;
        int titleId;
        int backupTitleId;

        switch (pkgName) {
            case BCM_ADJUST_SCREEN_OFFSET_PKG_NAME:
                sort = 1;
                resId = R.drawable.ic_adjust_screen_offset_banner;
                backupResId = R.drawable.ic_adjust_screen_offset_backup_banner;
                titleId = R.string.bcm_adjust_screen_offset;
                backupTitleId = R.string.bcm_adjust_screen_offset_not_installed;
                break;
            case BCM_OTA_UPDATER_PKG_NAME:
                sort = 2;
                resId = R.drawable.ic_ota_updater_banner;
                backupResId = R.drawable.ic_ota_updater_backup_banner;
                titleId = R.string.bcm_ota_updater;
                backupTitleId = R.string.bcm_ota_updater_not_installed;
                break;
            case BCM_SIDEBAND_VIEWER_PKG_NAME:
                sort = 3;
                resId = R.drawable.ic_sideband_viewer_banner;
                backupResId = R.drawable.ic_sideband_viewer_backup_banner;
                titleId = R.string.bcm_sideband_viewer;
                backupTitleId = R.string.bcm_sideband_viewer_not_installed;
                break;
            case BCM_URI_PLAYER_PKG_NAME:
                sort = 4;
                resId = R.drawable.ic_uri_player_banner;
                backupResId = R.drawable.ic_uri_player_backup_banner;
                titleId = R.string.bcm_uri_player;
                backupTitleId = R.string.bcm_uri_player_not_installed;
                break;
            default:
                return;
        }

        postNotification(sort, resId, backupResId, titleId, backupTitleId, pkgName);
    }

    private void postNotification(int sort, int resId, int backupResId,
            int titleId, int backupTitleId, String pkgName) {
        int id = resId;
        Intent intent = mPkgMan.getLeanbackLaunchIntentForPackage(pkgName);

        if (intent == null) {
            titleId = backupTitleId;
            resId = backupResId;
            intent = getBackupIntent(pkgName);
        }

        Notification.Builder bob = new Notification.Builder(mContext);
        Bundle extras = new Bundle();
        extras.putString(BLACKLIST_PACKAGE, pkgName);

        bob.setContentTitle(mContext.getString(titleId))
                .setSmallIcon(R.drawable.ic_launcher)
                .setLargeIcon(BitmapFactory.decodeResource(mContext.getResources(), resId))
                .setContentIntent(PendingIntent.getActivity(mContext, 0, intent, 0))
                .setCategory(Notification.CATEGORY_RECOMMENDATION)
                .setGroup(PARTNER_GROUP)
                .setSortKey(sort+"")
                .setColor(mContext.getResources().getColor(R.color.partner_color))
                .setExtras(extras);

        mNotifMan.notify(id, bob.build());
    }

    private Intent getBackupIntent(String pkgName) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse("market://details?id=" + pkgName));

        return intent;
    }

    private String getPackageName(Intent intent) {
        Uri uri = intent.getData();
        String pkg = uri != null ? uri.getSchemeSpecificPart() : null;
        return pkg;
    }

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

    private boolean launchNetflixAutomatically() {
        return Boolean.parseBoolean(System.getProperty(SYSPROP_BOOT_WAKEUP, "false"));
    }
}
