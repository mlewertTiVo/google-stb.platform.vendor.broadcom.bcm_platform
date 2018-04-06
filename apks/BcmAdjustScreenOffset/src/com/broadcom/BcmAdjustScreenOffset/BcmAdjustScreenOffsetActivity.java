package com.broadcom.BcmAdjustScreenOffset;

import android.widget.Button;
import android.app.Activity;
import android.os.Bundle;
import java.util.Scanner;
import android.view.View;
import android.view.View.OnClickListener;
import android.util.Log;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileNotFoundException;
import java.lang.Runtime;
import java.lang.Process;
import android.graphics.Rect;
import android.content.Context;
import android.widget.Toast;
import android.os.RemoteException;
import bcm.hardware.dspsvcext.V1_0.IDspSvcExt;
import bcm.hardware.dspsvcext.V1_0.DspSvcExtOvs;

public class BcmAdjustScreenOffsetActivity extends Activity {
    /** Called when the activity is first created. */
    private static final String TAG = "BcmAdjustScreenOffset";
    private Button button_x_inc, button_x_dec, button_y_inc, button_y_dec, button_reset;
    private IDspSvcExt server;
    private final static int step_x = 4;
    private final static int step_y = 4;


    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        button_x_inc = (Button)findViewById(R.id.button_x_inc);
        button_x_dec = (Button)findViewById(R.id.button_x_dec);
        button_y_inc = (Button)findViewById(R.id.button_y_inc);
        button_y_dec = (Button)findViewById(R.id.button_y_dec);
        button_reset = (Button)findViewById(R.id.button_reset);

        button_x_inc.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                Log.i(TAG,"click on button_x_inc");
                adjustScreenOffset(-step_x, 0);
            }
        });

        button_x_dec.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                Log.i(TAG,"click on button_x_dec");
                adjustScreenOffset(step_x, 0);
            }
        });

        button_y_inc.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                Log.i(TAG,"click on button_y_inc");
                adjustScreenOffset(0, -step_y);
            }
        });

        button_y_dec.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                Log.i(TAG,"click on button_y_dec");
                adjustScreenOffset(0, step_y);
            }
        });

        button_reset.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                Log.i(TAG,"click on button_y_dec");
                resetScreenOffset();
            }
        });

    }

    private void adjustScreenOffset(int dx, int dy) {
        try {
           server = IDspSvcExt.getService();
        } catch (RemoteException e) {
           Log.w(TAG, "RemoteException trying to reach IDspSvcExt ", e);
           return;
        }
        if (server == null) {
           throw new IllegalArgumentException("display service extension is null");
        }

        Rect offset = new Rect();
        DspSvcExtOvs ovs_get = new DspSvcExtOvs();
        try {
           ovs_get = server.getOvs();
        } catch (RemoteException e) {
           Log.w(TAG, "RemoteException calling IDspSvcExt.getOvs ", e);
           return;
        }

        offset.left   = ovs_get.ovs_x;
        offset.top    = ovs_get.ovs_y;
        offset.right  = ovs_get.ovs_x + ovs_get.ovs_w;
        offset.bottom = ovs_get.ovs_y + ovs_get.ovs_h;

        offset.inset(dx, dy);

        DspSvcExtOvs ovs_set = new DspSvcExtOvs();
        ovs_set.ovs_x = offset.left;
        ovs_set.ovs_y = offset.top;
        ovs_set.ovs_w = offset.right - offset.left;
        ovs_set.ovs_h = offset.bottom - offset.top;
        try {
           server.setOvs(ovs_set);
        } catch (RemoteException e) {
           Log.w(TAG, "RemoteException calling IDspSvcExt.setOvs ", e);
           return;
        }

        DspSvcExtOvs ovs_new = new DspSvcExtOvs();
        try {
           ovs_new = server.getOvs();
        } catch (RemoteException e) {
           Log.w(TAG, "RemoteException calling IDspSvcExt.getOvs ", e);
           return;
        }
        if (!ovs_new.equals(ovs_set)) {
            Toast.makeText(getApplicationContext(),
               "Failed to modify overscan coordinates.",
               Toast.LENGTH_LONG).show();
        }
    }

    private void resetScreenOffset() {

        DspSvcExtOvs ovs = new DspSvcExtOvs();

        try {
           server = IDspSvcExt.getService();
        } catch (RemoteException e) {
           Log.w(TAG, "RemoteException trying to reach IDspSvcExt ", e);
           return;
        }
        if (server == null) {
           throw new IllegalArgumentException("display service extension is null");
        }

        ovs.ovs_x = 0;
        ovs.ovs_y = 0;
        ovs.ovs_w = 0;
        ovs.ovs_h = 0;

        try {
           server.setOvs(ovs);
        } catch (RemoteException e) {
           Log.w(TAG, "RemoteException calling IDspSvcExt.setOvs ", e);
        }
    }

    private static Scanner findToken(Scanner scanner, String token) {
        while (true) {
            String next = scanner.next();
            if (next.equals(token)) {
                return scanner;
            }
        }

        // Scanner will exhaust input and throw an exception before getting here.
    }

}
