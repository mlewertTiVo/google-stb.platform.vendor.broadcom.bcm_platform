/******************************************************************************
 *    (c)2009-2011 Broadcom Corporation
 *
 * This program is the proprietary software of Broadcom Corporation and/or its licensors,
 * and may only be used, duplicated, modified or distributed pursuant to the terms and
 * conditions of a separate, written license agreement executed between you and Broadcom
 * (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 * no license (express or implied), right to use, or waiver of any kind with respect to the
 * Software, and Broadcom expressly reserves all rights in and to the Software and all
 * intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 * HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 * NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 * secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 * and to use this information only in connection with your use of Broadcom integrated circuit products.
 *
 * 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES
 * OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE,
 * LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION
 * OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF
 * USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
 * LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR
 * EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR
 * USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT
 * ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
 * LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF
 * ANY LIMITED REMEDY.
 *
 * $brcm_Workfile: BcmDemoPIP.java $
 * $brcm_Revision:  $
 * $brcm_Date:  $
 *
 * Module Description:
 *
 * Revision History:
 * 
 *
 *****************************************************************************/
package com.broadcom.BcmUriPlayer;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;

import android.net.Uri;
import android.text.Editable;
import android.widget.TextView;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.content.Intent;
import android.content.SharedPreferences;
import android.view.View;
import android.view.View.OnClickListener;

public class BcmUriPlayer extends Activity {
    private static final String TAG = "BcmUriPlayer";
    public static final String PREFS_NAME = "BcmUriPlayerPrefs";
    EditText uriEditText;
    Intent myIntent;
     
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        TextView txtView = new TextView(this);    
        TextView uriExampleView = new TextView(this);    
        uriEditText = new EditText(this);
        Button btnView = new Button(this);
        String savedUriString;
        btnView.setText("PLAY");
        uriExampleView.setText(
            "URI examples:\n[Local File] /mnt/sdcard/xxx.mp4\n[UDP Multicast] udp://ip_addr:port\n[RTP Multicast] rtp://ip_addr:port\netc...\n\nType-in URI here");
        uriExampleView.setTextSize(20);

        SharedPreferences settings = getSharedPreferences(PREFS_NAME, 0);
        savedUriString = settings.getString("SavedURI", "");
        Log.i(TAG, "Restrored URI:"+savedUriString);
        uriEditText.setText(savedUriString);
        
        LinearLayout ll = new LinearLayout(this);
        ll.setOrientation(LinearLayout.VERTICAL);

        LinearLayout.LayoutParams llp_wrap = new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.WRAP_CONTENT, 
                        LinearLayout.LayoutParams.WRAP_CONTENT);

        ll.addView(uriExampleView, llp_wrap);
 
        ll.addView(uriEditText, llp_wrap);
        ll.addView(btnView, llp_wrap);
        
        setContentView(ll);

        myIntent = new Intent(this, MediaPlayerActivity.class);
                
        btnView.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                    Editable uriString = uriEditText.getText();
                    myIntent.putExtra("CoverURI", uriString.toString());
                    startActivity(myIntent);            
                }
            });        
        
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.d(TAG, "onPause...");

        SharedPreferences settings = getSharedPreferences(PREFS_NAME, 0);
        SharedPreferences.Editor editor = settings.edit();
        editor.putString("SavedURI", uriEditText.getText().toString());
        editor.commit();
        Log.d(TAG, "Saved URI:"+uriEditText.getText().toString());
        
        finish();
    }
    
}



