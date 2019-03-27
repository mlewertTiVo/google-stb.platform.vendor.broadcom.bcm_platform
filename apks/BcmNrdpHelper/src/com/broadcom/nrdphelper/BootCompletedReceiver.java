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
package com.broadcom.nrdphelper;

import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.provider.Settings;
import android.util.Log;

import com.google.gson.Gson;

import static com.broadcom.nrdphelper.Constants.DEBUG;
import static com.broadcom.nrdphelper.Constants.TAG_BCM_NRDPHELPER;
import static com.broadcom.nrdphelper.Constants.TAG_WITH_CLASS_NAME;

import com.broadcom.nrdphelper.PlatformCapabilitiesRoot;

/**
 * Boot completed receiver for BcmNrdpHelper app.
 */
public class BootCompletedReceiver extends BroadcastReceiver {
    private static final String TAG = TAG_WITH_CLASS_NAME ?
            "BootCompletedReceiver" : TAG_BCM_NRDPHELPER;

    private final String forceEotfPropertyKey = "ro.nx.hwc2.tweak.force_eotf";
    private final String nrdpSettingKey = "nrdp_platform_capabilities";

    private static final String NRDPHELPER_PACKAGE = "com.broadcom.nrdphelper";
    private static final String NRDPHELPER_HDMI_AUDIO_PLUG_SERVICE = "com.broadcom.nrdphelper.HdmiAudioPlugService";

    public final void setCapabilities(Context context) {
       String hdrOutputTypeStr = getHdrOutputTypeStr();
       String jsonString = new Gson().toJson(new PlatformCapabilitiesRoot(false, hdrOutputTypeStr, "none", "disable", "7", "16384"));

       Settings.Global.putString(context.getContentResolver(), nrdpSettingKey, jsonString);
       Log.i(TAG, nrdpSettingKey + " set to " + jsonString);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (DEBUG) Log.d(TAG, "boot completed " + intent);

        setCapabilities(context);

        Intent localIntent = new Intent();
        localIntent.setComponent(new ComponentName(NRDPHELPER_PACKAGE, NRDPHELPER_HDMI_AUDIO_PLUG_SERVICE));
        context.startService(localIntent);
    }

    /**
     *  Support NRDP HdrOutputTypes:
     *    - "always": Force the mode to HDR always, regardless of the source data.
     *    - "input": Let the sink output mode track the input.
     */
    private String getHdrOutputTypeStr() {
        String nrdpHdrOutputTypeStr = "always";
        boolean forceEotf = android.os.SystemProperties.getBoolean(forceEotfPropertyKey, true);
        if (!forceEotf) {
            nrdpHdrOutputTypeStr = "input";
        }
        return nrdpHdrOutputTypeStr;
    }
}
