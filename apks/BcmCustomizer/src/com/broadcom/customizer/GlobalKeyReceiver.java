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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.ComponentName;
import android.os.SystemProperties;
import android.util.Log;
import android.view.KeyEvent;

import static com.broadcom.customizer.Constants.DEBUG;
import static com.broadcom.customizer.Constants.TAG_BCM_CUSTOMIZER;
import static com.broadcom.customizer.Constants.TAG_WITH_CLASS_NAME;

/**
 * This class also posts notifications that are used by BT pairing and Wifi WPS hardware button
 * It also allows the system/launcher to start the correct component when input event are
 * detected.
 *
 * Packages using this broadcast receiver should have the corresponding input event
 * defined in the key layout file.
 */
public class GlobalKeyReceiver extends BroadcastReceiver {
    private static final String TAG = TAG_WITH_CLASS_NAME ?
            "GlobalKeyReceiver" : TAG_BCM_CUSTOMIZER;

    private static final String ACTION_GLOBAL_BUTTON = "android.intent.action.GLOBAL_BUTTON";
    private static final String ACTION_CONNECT_INPUT = "com.google.android.intent.action.CONNECT_INPUT";
    private static final String INTENT_EXTRA_NO_INPUT_MODE = "no_input_mode";

    private static final String TV_SETTING_PACKAGE = "com.android.tv.settings";
    private static final String TV_SETTING_WPS_ACTIVITY = "com.android.tv.settings.connectivity.WpsConnectionActivity";

    private static long sLastEventTime;
    private SplashScreenManager mSplashScreenManager = SplashScreenManager.getInstance();

    @Override
    public void onReceive(Context context, Intent intent) {
        if (DEBUG) Log.d(TAG, "onReceive intent=" + intent);

        if (ACTION_GLOBAL_BUTTON.equals(intent.getAction())) {
            KeyEvent event = intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
            int keyCode = event.getKeyCode();
            int action = event.getAction();
            long eventTime = event.getEventTime();
            if (action == KeyEvent.ACTION_UP && sLastEventTime != eventTime) {
                // Workaround for b/23947504, the same key event may be sent twice, filter it.
                sLastEventTime = eventTime;
                if (DEBUG) Log.d(TAG, "KeyCode is " + keyCode);

                switch (keyCode) {
                case KeyEvent.KEYCODE_PAIRING:
                    /* Start BT pairing mode, assuming no input */
                    if (DEBUG) Log.d(TAG, "Got BT Pairing key");

                    intent = new Intent(ACTION_CONNECT_INPUT);
                    intent.putExtra(Intent.EXTRA_KEY_EVENT, event);
                    intent.putExtra(INTENT_EXTRA_NO_INPUT_MODE, true);
                    intent.setFlags(intent.FLAG_RECEIVER_FOREGROUND);
                    if (DEBUG) Log.d(TAG, "intent: " + intent);
                    context.startActivity(intent);
                    break;
                case KeyEvent.KEYCODE_TV_NETWORK:
                    /* Start WPS activity */
                    if (DEBUG) Log.d(TAG, "Got Wifi WPS key");

                    intent = new Intent();
                    intent.setComponent(new ComponentName(TV_SETTING_PACKAGE, TV_SETTING_WPS_ACTIVITY));
                    intent.setFlags(intent.FLAG_ACTIVITY_NEW_TASK);
                    if (DEBUG) Log.d(TAG, "intent: " + intent);
                    context.startActivity(intent);
                    break;
                case KeyEvent.KEYCODE_BUTTON_16:
                    /* Start Netflix activity */
                    if (DEBUG) Log.d(TAG, "Got Netflix key");
                    mSplashScreenManager.checkStartSplashResume(context);
                    break;
                }
            }
        }
    }
}
