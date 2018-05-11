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

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.widget.TextView;

import static com.broadcom.customizer.Constants.DEBUG;
import static com.broadcom.customizer.Constants.TAG_BCM_CUSTOMIZER;
import static com.broadcom.customizer.Constants.TAG_WITH_CLASS_NAME;

public class SplashActivity extends Activity {
    private static final String TAG = TAG_WITH_CLASS_NAME ?
            "SplashActivity" : TAG_BCM_CUSTOMIZER;

    private static final String EXTRA_TEXT = "text";
    private static final String EXTRA_BROADCAST_INTENT = "broadcast_intent";
    private static final String EXTRA_BROADCAST_PERMISSION = "broadcast_permission";
    private static final String EXTRA_BROADCAST_WAIT_BOOTUP = "broadcast_wait_bootup";
    private static final String EXTRA_BROADCAST_DELAY = "broadcast_delay";

    private TextView mTextToDisplay;
    private Intent mIntent;
    private boolean mBroadcastAfterBootup;
    private long mBroadcastDelay;
    private Handler mBroadcastDelayHandler = new Handler();
    private boolean mHasBooted = false;

    private SplashScreenManager mSplashScreenManager = SplashScreenManager.getInstance();

    private final Runnable mBroadcastRunnable = new Runnable() {
        @Override
        public void run() {
            if (DEBUG) Log.d(TAG, "mBroadcastRunnable");
            // Delayed broadcast sending
            if (mIntent.hasExtra(EXTRA_BROADCAST_INTENT)) {
                Intent intent = (Intent)mIntent.getParcelableExtra(EXTRA_BROADCAST_INTENT);
                if (DEBUG) Log.d(TAG, "Broadcast intent=" + intent);
                String permission = mIntent.getStringExtra(EXTRA_BROADCAST_PERMISSION);
                sendBroadcast(intent, permission);

                // Tell BcmCustomizer splash has finished
                mSplashScreenManager.splashCompleted();
            }
        }
    };

    public void onBootComplete() {
        if (DEBUG) Log.d(TAG, "onBootComplete");
        mHasBooted = true;
        if (mBroadcastAfterBootup) {
             mBroadcastDelayHandler.postDelayed(mBroadcastRunnable, mBroadcastDelay);
        }
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mSplashScreenManager.setSplashActivity(this);

        setContentView(R.layout.splash);
        mTextToDisplay = (TextView)findViewById(R.id.fullscreen_content);

        mIntent = new Intent(getIntent());

        /* Set display text */
        if (mIntent.hasExtra(EXTRA_TEXT)) {
            mTextToDisplay.setText(mIntent.getStringExtra(EXTRA_TEXT));
        }

        /* Set broadcast delay if specified */
        mBroadcastDelay = mIntent.getLongExtra(EXTRA_BROADCAST_DELAY, 0);

        if (mSplashScreenManager.receivedBootupIntent()) {
            mHasBooted = true;
        }

        /* Set to broadcast after boot up if required */
        if (mIntent.getBooleanExtra(EXTRA_BROADCAST_WAIT_BOOTUP, false) && !mHasBooted) {
            mBroadcastAfterBootup = true;
        }
        else {
            mBroadcastAfterBootup = false;
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        if (hasFocus) {
            /* Send broadcast intent */
            if (!mBroadcastAfterBootup) {
                mBroadcastDelayHandler.postDelayed(mBroadcastRunnable, mBroadcastDelay);
            }
        }
    }
}
