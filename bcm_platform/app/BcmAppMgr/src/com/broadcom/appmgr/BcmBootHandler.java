package com.broadcom.appmgr;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.util.Log;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.content.BroadcastReceiver;

public class BcmBootHandler extends BroadcastReceiver
{
    public String TAG = "BcmAppMgr:BootHandler";

    @Override
    public void onReceive(Context context, Intent intent) 
    {
        Log.e(TAG, "onReceive");
        if ("android.intent.action.BOOT_COMPLETED".equals(intent.getAction()))
        {
            Log.e(TAG, "onReceive: Received BOOT_COMPLETED (starting BcmAppSvc)");

            Intent service = new Intent(context, BcmAppSvc.class);
            context.startService(service);
        }
    }
}
