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
import android.os.Bundle;
import android.util.Log;

import static com.broadcom.customizer.Constants.DEBUG;
import static com.broadcom.customizer.Constants.TAG_BCM_CUSTOMIZER;
import static com.broadcom.customizer.Constants.TAG_WITH_CLASS_NAME;

/**
 * Support forced-OTA-on-boot
 * https://support.google.com/androidpartners_androidtv/answer/7506564#OTA-on-boot
 *
 * The tvlauncher starts this activity with startActivityForResult and expects
 * the result RESULT_OK. If the result is different, it restarts the app.
 *
 * We also use this as a trigger to show splash screens while launching an app
 * from cold boot, as an alternative to the deprecated method described here:
 * https://support.google.com/androidpartners_androidtv/answer/6361722
 */
public class ForcedOtaActivity extends Activity {
    private static final String TAG = TAG_WITH_CLASS_NAME ?
            "ForcedOtaActivity" : TAG_BCM_CUSTOMIZER;

    private SplashScreenManager mSplashScreenManager = SplashScreenManager.getInstance();

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (needsOTA()) {
            // TODO: OEMs implement the forced OTA
            // setContentView(), etc.
        } else if (mSplashScreenManager.checkStartSplashColdBoot(this)) {
            // Splash boot, don't display OTA activity
            setResult(Activity.RESULT_OK);
            finish();
        } else {
            // Normal boot, don't display OTA activity
            setResult(Activity.RESULT_OK);
            finish();
        }
    }

    private boolean needsOTA() {
        // TODO: OEMs return true if the box needs a forced OTA
        return false;
    }
}