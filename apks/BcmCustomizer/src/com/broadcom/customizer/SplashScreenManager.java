/******************************************************************************
 *  Copyright (C) 2018 Broadcom.
 *  The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 *
 *  This program is the proprietary software of Broadcom and/or its licensors,
 *  and may only be used, duplicated, modified or distributed pursuant to
 *  the terms and conditions of a separate, written license agreement executed
 *  between you and Broadcom (an "Authorized License").  Except as set forth in
 *  an Authorized License, Broadcom grants no license (express or implied),
 *  right to use, or waiver of any kind with respect to the Software, and
 *  Broadcom expressly reserves all rights in and to the Software and all
 *  intellectual property rights therein. IF YOU HAVE NO AUTHORIZED LICENSE,
 *  THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD
 *  IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
 *
 *  Except as expressly set forth in the Authorized License,
 *
 *  1.     This program, including its structure, sequence and organization,
 *  constitutes the valuable trade secrets of Broadcom, and you shall use all
 *  reasonable efforts to protect the confidentiality thereof, and to use this
 *  information only in connection with your use of Broadcom integrated circuit
 *  products.
 *
 *  2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 *  "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS
 *  OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
 *  RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL
 *  IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR
 *  A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET
 *  ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME
 *  THE ENTIRE RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 *  3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM
 *  OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL,
 *  INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY
 *  RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM
 *  HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN
 *  EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1,
 *  WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY
 *  FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 ******************************************************************************/
package com.broadcom.customizer;

import android.content.Context;
import android.content.Intent;
import android.content.ComponentName;
import android.os.SystemProperties;
import android.util.Log;

import static com.broadcom.customizer.Constants.DEBUG;
import static com.broadcom.customizer.Constants.TAG_BCM_CUSTOMIZER;
import static com.broadcom.customizer.Constants.TAG_WITH_CLASS_NAME;

/**
 * Singleton to manage splash screen state.
 *
 * The forced OTA activity uses it to launch Netflix from cold boot
 * The global key receiver uses it to launch Netflix from the netflix button
 * The boot completed receiver uses it to advertise the system booted.
 * The splash activity uses it to query for boot completed and advertise when
 * splash is completed.
 */
final class SplashScreenManager {
    private static final String TAG = TAG_WITH_CLASS_NAME ?
            "SplashScreenManager" : TAG_BCM_CUSTOMIZER;

    private static final String SYSPROP_BOOT_KEY_TWO = "dyn.nx.boot.key2";
    private static final String SYSPROP_BOOT_WAKEUP = "dyn.nx.boot.wakeup";

    private static final String BRCM_SPLASH_PACKAGE = "com.broadcom.customizer";
    private static final String BRCM_SPLASH_ACTIVITY = "com.broadcom.customizer.SplashActivity";
    private static final String BRCM_SPLASH_EXTRA_TEXT = "text";
    private static final String BRCM_SPLASH_EXTRA_BC_INTENT = "broadcast_intent";
    private static final String BRCM_SPLASH_EXTRA_BC_PERMISSION = "broadcast_permission";
    private static final String BRCM_SPLASH_EXTRA_BC_WAIT_BOOTUP = "broadcast_wait_bootup";
    private static final String BRCM_SPLASH_EXTRA_BC_DELAY = "broadcast_delay";
    private static final long BRCM_SPLASH_BC_DELAY_MS = 7000;

    private static final String NETFLIX_PACKAGE = "com.netflix.ninja";
    private static final String ACTION_NETFLIX_KEY = "com.netflix.ninja.intent.action.NETFLIX_KEY";
    private static final String NETFLIX_KEY_PERMISSION = "com.netflix.ninja.permission.NETFLIX_KEY";
    private static final String NETFLIX_KEY_POWER_MODE = "power_on";

    private boolean mBootCompleted = false;
    private boolean mWaitForSplashCompleted = false;
    private SplashActivity mSplashActivity = null;

    private static final Object mLock = new Object();
    private static SplashScreenManager mInstance;

    static SplashScreenManager getInstance() {
        synchronized (mLock) {
            if (mInstance == null) {
                mInstance = new SplashScreenManager();
            }
            return mInstance;
        }
    }

    private SplashScreenManager() {
        if (DEBUG) Log.d(TAG, "New SplashScreenManager");
    }

    boolean checkStartSplashColdBoot(Context context) {
        if (DEBUG) Log.d(TAG, "checkStartSplashColdBoot");
        if (SystemProperties.getBoolean(SYSPROP_BOOT_KEY_TWO, false)) {
            Log.i(TAG, "Launching Netflix from power on");
            launchNetflixSplash(context, true, 0);
            return true;
        }
        return false;
    }

    void checkStartSplashResume(Context context) {
        if (DEBUG) Log.d(TAG, "checkStartSplashResume");
        if (SystemProperties.getBoolean(SYSPROP_BOOT_WAKEUP, false)) {
            if (mWaitForSplashCompleted) {
                Log.i(TAG, "Waiting for splash to complete.");
            } else {
                Log.i(TAG, "Launching Netflix from resume");
                launchNetflixSplash(context, false, BRCM_SPLASH_BC_DELAY_MS);
            }
        } else {
            if (mWaitForSplashCompleted) {
                Log.i(TAG, "Waiting for splash to complete.");
            } else {
                launchNetflix(context);
            }
        }
    }

    void splashCompleted() {
        Log.i(TAG, "Splash completed");
        mWaitForSplashCompleted = false;
    }

    void setSplashActivity(SplashActivity splashActivity) {
        mSplashActivity = splashActivity;
    }

    void bootCompleted() {
        if (mSplashActivity != null) {
            mSplashActivity.onBootComplete();
        }
    }

    boolean receivedBootupIntent() {
        return mBootCompleted;
    }

    private Intent createNetflixIntent(boolean powerOn) {
        Intent res = new Intent();
        res.setAction(ACTION_NETFLIX_KEY);
        if (powerOn) {
            // Netflix key press resulted in device power on (via WAKEUP input event)
            res.putExtra(NETFLIX_KEY_POWER_MODE, true);
        }
        res.setPackage(NETFLIX_PACKAGE); // Makes the intent explicit
        res.addFlags(Intent.FLAG_INCLUDE_STOPPED_PACKAGES);

        return res;
    }

    private void launchNetflix(Context context) {
        Intent localIntent = createNetflixIntent(false);

        if (DEBUG) Log.d(TAG, "localIntent: " + localIntent);
        context.sendBroadcast(localIntent, NETFLIX_KEY_PERMISSION);
    }

    private void launchNetflixSplash(Context context, boolean waitBootup, long delayMs) {
        Intent localIntent = new Intent();

        mWaitForSplashCompleted = true;
        localIntent.setComponent(new ComponentName(BRCM_SPLASH_PACKAGE, BRCM_SPLASH_ACTIVITY));
        localIntent.putExtra(BRCM_SPLASH_EXTRA_TEXT, "Starting up Netflix...");
        localIntent.putExtra(BRCM_SPLASH_EXTRA_BC_INTENT, createNetflixIntent(true));
        localIntent.putExtra(BRCM_SPLASH_EXTRA_BC_PERMISSION, NETFLIX_KEY_PERMISSION);
        localIntent.putExtra(BRCM_SPLASH_EXTRA_BC_WAIT_BOOTUP, waitBootup);
        localIntent.putExtra(BRCM_SPLASH_EXTRA_BC_DELAY, delayMs);
        localIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        if (DEBUG) Log.d(TAG, "waitBootup: " + waitBootup + " localIntent: " + localIntent);
        context.startActivity(localIntent);
    }
}
