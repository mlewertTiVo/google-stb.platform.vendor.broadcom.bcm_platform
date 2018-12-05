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

import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

import bcm.hardware.nexus.V1_1.INexus;

import java.util.Map;

import static com.broadcom.customizer.Constants.DEBUG;
import static com.broadcom.customizer.Constants.TAG_BCM_CUSTOMIZER;
import static com.broadcom.customizer.Constants.TAG_WITH_CLASS_NAME;

public class HdmiAudioPlugService extends Service {
    private static final String TAG = TAG_WITH_CLASS_NAME ?
            "HdmiAudioPlugService" : TAG_BCM_CUSTOMIZER;

    @Override
    public void onCreate() {
        super.onCreate();

        this.registerReceiver(new BroadcastReceiver() {
            private INexus NexusInterface;

            @Override
            public void onReceive(Context context, Intent intent) {
                if (DEBUG) Log.d(TAG, "plug received " + intent);

                if (intent.getIntExtra(AudioManager.EXTRA_AUDIO_PLUG_STATE, 0) != 0) {
                    final byte PCM_output = 1;
                    final byte auto_output = 0;
                    int formats[] = intent.getIntArrayExtra(AudioManager.EXTRA_ENCODINGS);
                    boolean PCMOnly = false;

                    if (formats != null) {
                        PCMOnly = true;
                        for (int i = 0; i < formats.length; i++) {
                            if ((formats[i] == AudioFormat.ENCODING_AC3) ||
                                (formats[i] == AudioFormat.ENCODING_E_AC3) ||
                                (formats[i] == AudioFormat.ENCODING_DTS) ||
                                (formats[i] == AudioFormat.ENCODING_DTS_HD)) {
                                // Not PCM only
                                PCMOnly = false;
                                break;
                            }
                        }
                    }

                    if (DEBUG) Log.d(TAG, "PCM only: " + PCMOnly);

                    // Get the service handle from the HAL
                    try {
                        NexusInterface = INexus.getService();
                    } catch (RemoteException e) {
                        Log.e(TAG, "RemoteException trying to reach INexus", e);
                        return;
                    }

                    if (NexusInterface == null) {
                        Log.e(TAG, "Nexus service is null");
                        throw new IllegalArgumentException("Nexus service is null");
                    }
                    try {
                        if (PCMOnly) {
                            Log.i(TAG, "Force PCM output");
                            NexusInterface.forcedPCM(PCM_output);
                        } else {
                            Log.i(TAG, "Auto audio output");
                            NexusInterface.forcedPCM(auto_output);
                        }
                    } catch (RemoteException e) {
                        Log.e(TAG, "RemoteException trying to call INexus", e);
                    }
                }
            }
        }, new IntentFilter(AudioManager.ACTION_HDMI_AUDIO_PLUG));
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
