package com.broadcom.flashplayer;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.widget.TextView;
import android.util.Log;
import java.io.*;
import android.view.ActionMode;
import java.util.Timer;
import java.util.TimerTask;
import android.os.Handler;
import android.text.method.ScrollingMovementMethod;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.LinearLayout.LayoutParams;
import android.view.View;

import android.app.ActivityManager;
import android.app.ActivityManager.RunningAppProcessInfo;
import java.util.List;
import java.util.Iterator;
import android.os.Process;
import android.app.AlertDialog;
import android.app.AlertDialog.Builder;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;

public class BcmFlashPlayer extends Activity
{
    public String TAG = "BcmFlashPlayer";
    final String Heading = "Broadcom's implementation of Adobe Flash Player 11.x";
    LinearLayout LLSub;
    TextView TVMain;
    Context FPContext;
    private BcmFlashPlayer_JNI FP_native;

    @Override
    public void onCreate(Bundle savedInstanceState) 
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        setTitle(R.string.app_name);
        FPContext = this;

		FP_native = new BcmFlashPlayer_JNI();

        // Get UI
        TVMain      = (TextView) findViewById(R.id.TextView_Main);
        TVMain.append(Heading + '\n');

        // Create the dialog box
        AlertDialog.Builder builder = new AlertDialog.Builder(FPContext);
        builder.setCancelable(true);

//        builder.setIcon(R.drawable.icon);
        builder.setTitle("Flash Player");
        builder.setInverseBackgroundForced(true);
        builder.setMessage("Switching to Trellis side now. Press 'Yes' to continue or 'No' to stay with Android");

        builder.setPositiveButton("Yes", new DialogInterface.OnClickListener() {
          @Override
          public void onClick(DialogInterface dialog, int which) {
            FP_native.switchToTrellis();

            dialog.dismiss();
            finish();
          }
        });

        builder.setNegativeButton("No", new DialogInterface.OnClickListener() {
          @Override
          public void onClick(DialogInterface dialog, int which) {
            dialog.dismiss();
            finish();
          }
        });

        // Show the box
        AlertDialog alert = builder.create();
        alert.show();
    }
}