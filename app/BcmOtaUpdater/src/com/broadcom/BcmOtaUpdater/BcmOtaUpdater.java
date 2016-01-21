/******************************************************************************
 *    (c)2009-2015 Broadcom Corporation
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
 *****************************************************************************/
package com.broadcom.BcmOtaUpdater;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.os.Bundle;
import android.os.RecoverySystem;
import android.os.AsyncTask;
import android.util.Log;

import android.webkit.URLUtil;
import android.net.Uri;
import android.text.Editable;
import android.widget.TextView;
import android.widget.Button;
import android.widget.Toast;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.view.View;
import android.view.View.OnClickListener;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.provider.Settings;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import java.net.URL;
import java.net.HttpURLConnection;
import java.io.DataOutputStream;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileInputStream;
import java.io.OutputStream;
import java.io.InputStream;
import java.io.IOException;

public class BcmOtaUpdater extends Activity {
    private static final String TAG = "BcmOtaUpdater";
    public static final String PREFS_NAME = "BcmOtaUpdaterPrefs";
    private static final String DEFAULT_LOCATION = "/cache/update.zip";
    EditText urlEditText;
    Intent myIntent;
    ProgressDialog pDialogView;
    private Button btnView;

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if ("com.broadcom.BcmOtaUpdater.DONE".equals(intent.getAction().toString())) {
                Toast.makeText(context, "Download Complete", Toast.LENGTH_LONG).show();
                generateAlert(DEFAULT_LOCATION);
                btnView.setEnabled(true);
            }
        }
    };

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        TextView uriExampleView = new TextView(this);
        urlEditText = new EditText(this);
        btnView = new Button(this);
        pDialogView = new ProgressDialog(this);

        pDialogView.setIndeterminate(true);
        pDialogView.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);
        pDialogView.setCancelable(false);

        String savedString;
        btnView.setText("Download");
        uriExampleView.setText(
            "Examples:\n[Local File] /cache/xxx.zip\n[Http Download] http://<server_addr>/<file_name>\n\nType-in URL here");
        uriExampleView.setTextSize(20);

        SharedPreferences settings = getSharedPreferences(PREFS_NAME, 0);
        savedString = settings.getString("SavedURL", "");
        Log.i(TAG, "Restrored URL:"+savedString);
        urlEditText.setText(savedString);

        LinearLayout ll = new LinearLayout(this);
        ll.setOrientation(LinearLayout.VERTICAL);

        LinearLayout.LayoutParams llp_wrap = new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.WRAP_CONTENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT);

        ll.addView(uriExampleView, llp_wrap);
        ll.addView(urlEditText, llp_wrap);
        ll.addView(btnView, llp_wrap);

        setContentView(ll);

        ConnectivityManager cm = (ConnectivityManager) this.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo ni = cm.getActiveNetworkInfo();
        boolean connected = ni != null && ni.isConnected() &&
                (ni.getType() == ConnectivityManager.TYPE_WIFI || ni.getType() == ConnectivityManager.TYPE_ETHERNET);

        if (!connected) {
            AlertDialog.Builder builder = new AlertDialog.Builder(this);
            builder.setTitle("No Network Connection detected");
            builder.setMessage("Please connect to a network to use ota updater");
            builder.setCancelable(false);
            builder.setNegativeButton("Exit", new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    dialog.dismiss();
                    finish();
                }
            });
            builder.setPositiveButton("Network Setting", new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    dialog.dismiss();
                    Intent i = new Intent(Settings.ACTION_WIFI_SETTINGS);
                    startActivity(i);
                }
            });
            final AlertDialog dlg = builder.create();
            dlg.show();
        }

        btnView.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                // check to see if the input is local file or URL
                String urlString;

                try {
                    urlString = urlEditText.getText().toString();

                    if (urlString.contains("http:") || urlString.contains("https:")) {
                        URL url = new URL(urlString);
                        if (URLUtil.isValidUrl(urlString)){
                            Log.i(TAG, "Found download string: " + url.toString());
                            pDialogView.setMessage("Downloading...");
                            new DownloadTask(getApplicationContext()).execute(urlString, "URL");
                        } else {
                            Toast.makeText(getApplicationContext(), "Non valid URL", Toast.LENGTH_LONG).show();
                        }
                    } else {
                        // if there is no http or https pattern, assume this is a local file
                        File file = new File(urlString);
                        if (file.exists()) {
                            if (urlString.indexOf("/cache/") == 0) {
                                Log.i(TAG, "update zip is already in /cache partition");
                                generateAlert(urlString);
                            } else {
                                pDialogView.setMessage("Copying to /cache ...");
                                new DownloadTask(getApplicationContext()).execute(urlString, "LOCAL");
                            }
                        } else {
                            Toast.makeText(getApplicationContext(), "File " + urlString + " does not exist!!", Toast.LENGTH_LONG).show();
                        }
                    }
                } catch (Exception e){
                    Toast.makeText(getApplicationContext(), "Error in URL " + e.toString(), Toast.LENGTH_LONG).show();
                }
            }
        });

        IntentFilter intentFilterAdd = new IntentFilter("com.broadcom.BcmOtaUpdater.DONE");
        this.registerReceiver(mReceiver, intentFilterAdd);
    }

    private void applyUpdate(String fileString) {
        Log.i(TAG, "update file path location is: " + fileString);
        RecoverySystem recoverySystem = new RecoverySystem();

        try {
            File updateFile = new File(fileString);
            recoverySystem.installPackage(getApplicationContext(), updateFile);
        } catch (Exception e) {
            Toast.makeText(getApplicationContext(), "Error: " + e.toString(), Toast.LENGTH_LONG).show();
        }

        return;
    }

    private void generateAlert(String updatefileLocation) {
        final String location = updatefileLocation;

        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("Apply the update");
        builder.setMessage("Are you sure to apply the update file?");
        builder.setCancelable(false);
        builder.setNegativeButton("No", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                dialog.dismiss();
            }
        });
        builder.setPositiveButton("yes", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                dialog.dismiss();
        applyUpdate(location);
            }
        });

        AlertDialog dlg = builder.create();
        dlg.show();
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.d(TAG, "onPause...");

        SharedPreferences settings = getSharedPreferences(PREFS_NAME, 0);
        SharedPreferences.Editor editor = settings.edit();
        editor.putString("SavedURL", urlEditText.getText().toString());
        editor.commit();
        Log.d(TAG, "Saved URL:"+urlEditText.getText().toString());

        this.unregisterReceiver(mReceiver);
        finish();
    }

    private class DownloadTask extends AsyncTask <String, Integer, String> {
        private Context context;
        public DownloadTask (Context context) {
            this.context = context;
        }

        @Override
        protected String doInBackground(String... sUrl){
            InputStream input = null;
            OutputStream output = null;
            HttpURLConnection conn = null;
            String isURL = sUrl[1];
            long fileLength = 0;
            File existingFile = null;

            if (isURL.compareTo("LOCAL") == 0) {
                existingFile = new File(sUrl[0]);
            }

            try {
                if (isURL.compareTo("URL") == 0) {
                    URL url = new URL(sUrl[0]);
                    conn = (HttpURLConnection) url.openConnection();
                    conn.connect();
                    fileLength = conn.getContentLength();
                    input = conn.getInputStream();
                } else if (isURL.compareTo("LOCAL") == 0) {
                    fileLength = existingFile.length();
                    input = new FileInputStream(sUrl[0]);
                } else
                    throw new Exception("Neither LOCAL nor URL is provided");

                output = new FileOutputStream(DEFAULT_LOCATION);

                byte data[] = new byte[4096];
                long total = 0;
                int count;
                while ((count = input.read(data)) != -1) {
                    total += count;
                    if (fileLength > 0)
                        publishProgress((int)(total * 100/fileLength));
                    output.write(data, 0, count);
                }
            } catch (Exception e) {
                File updateFile = new File(DEFAULT_LOCATION);
                if ( updateFile.delete())
                    Log.i(TAG, "deleted the tmp file failed to download");
                else
                    Log.e(TAG, "Failed to delete the tmp file failed to download");

                return e.toString();
            } finally {
                try {
                    if (output !=null)
                        output.close();
                    if (input !=null)
                        input.close();
                } catch (IOException ignored) {
                }

                if (conn != null)
                    conn.disconnect();
            }
        return null;
        }

        @Override
        protected void onPreExecute() {
            super.onPreExecute();
            btnView.setEnabled(false);
            pDialogView.show();
        }

        @Override
        protected void onProgressUpdate(Integer... progress) {
            super.onProgressUpdate(progress);
            pDialogView.setIndeterminate(false);
            pDialogView.setMax(100);
            pDialogView.setProgress(progress[0]);
        }

        @Override
        protected void onPostExecute(String result) {
            pDialogView.dismiss();
            if (result !=null) {
                Toast.makeText(context, "Download or Copy error: " + result, Toast.LENGTH_LONG).show();
                btnView.setEnabled(true);
            } else {
                Intent intent = new Intent("com.broadcom.BcmOtaUpdater.DONE");
                context.sendBroadcast(intent);
            }
        }
    }
}
