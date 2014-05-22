package com.broadcom.appmgr;

import android.util.Log;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.Binder;
import android.content.Context;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.Notification.Builder;
import android.app.PendingIntent;

public class BcmAppSvc extends Service {
    public String TAG = "BcmAppMgr:Service";

    // This is the object that receives interactions from clients.  See
    // RemoteService for a more complete example.
    private final IBinder mBinder = new LocalBinder();

    /**
     * Class for clients to access.  Because we know this service always
     * runs in the same process as its clients, we don't need to deal with
     * IPC.
     */
    public class LocalBinder extends Binder {
        BcmAppSvc getService() {
            return BcmAppSvc.this;
        }
    }

    @Override
    public void onCreate() {
        // Display a notification about us starting.  We put an icon in the status bar.
        Log.e(TAG, "onCreate:Enter...");

        Context context = getApplicationContext();

        Intent StarterIntent = new Intent(context, BcmAppHandler.class);
        StarterIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        StarterIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        StarterIntent.addFlags(Intent.FLAG_ACTIVITY_NO_HISTORY);
        StarterIntent.addFlags(Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);
        context.startActivity(StarterIntent);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.e(TAG, "onStartCommand:startId: " + startId + " intent: " + intent);

        Notification.Builder builder =  new Notification.Builder(this)  
                                            .setContentTitle("BcmAppSvc Notification")  
                                            .setContentText("This is a notification from BcmappMgr");  

        PendingIntent pi = PendingIntent.getActivity(this, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
        NotificationManager nm = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        Notification noti;

        builder.setContentIntent(pi);

        // Even though getNotification() is deprecated, we don't seem to 
        // have build() yet. So, let's just use the older call for now
//        noti = builder.build();
        noti = builder.getNotification();

        // Add as notification  
//        nm.notify(id, noti);

        noti.flags |= Notification.FLAG_NO_CLEAR;
		startForeground(21, noti);

        // We want this service to continue running until
        // it is explicitly stopped, so return sticky
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    public void onDestroy ()
    {
        Log.e(TAG, "onDestroy:Enter...");
    }
}