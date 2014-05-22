package com.broadcom.nolock;

import android.content.Context;
import android.content.Intent;
import android.app.Activity;
import android.util.Log;
import android.content.BroadcastReceiver;

import android.app.KeyguardManager;
import android.app.KeyguardManager.KeyguardLock;

public class BcmNoLock extends BroadcastReceiver
{
    public String TAG = "BcmNoLock";

    @Override
    public void onReceive(Context context, Intent intent)
    {
        Log.e(TAG, "onReceive");
        if ("android.intent.action.BOOT_COMPLETED".equals(intent.getAction()))
        {
            Log.e(TAG, "onReceive: Received BOOT_COMPLETED...");

            KeyguardManager keyguardManager = (KeyguardManager)context.getSystemService(Activity.KEYGUARD_SERVICE);
            KeyguardLock lock = keyguardManager.newKeyguardLock(context.KEYGUARD_SERVICE);
            lock.disableKeyguard();
        }
    }
}
