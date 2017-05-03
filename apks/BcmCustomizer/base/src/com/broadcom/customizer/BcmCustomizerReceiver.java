/*
 * Copyright (C) 2017 The Android Open Source Project
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

import android.content.Context;
import android.content.Intent;
import android.util.Log;
import com.broadcom.customizer.BcmCustomizerReceiverBase;

/**
 * Barebone receiver, see BcmCustomizerReceiverBase for core functionalities.
 */
public class BcmCustomizerReceiver extends BcmCustomizerReceiverBase {
    private static final String TAG = "BcmCustomizerReceiver";
    private static final boolean DEBUG = false;

    @Override
    public void onReceive(Context context, Intent intent) {
        if (DEBUG) Log.d(TAG, "onReceive intent=" + intent);
        super.onReceive(context, intent);
    }
}
