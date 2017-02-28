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

public class BcmAdjustScreenOffsetActivity extends Activity {
    /** Called when the activity is first created. */
    private static final String TAG = "BcmAdjustScreenOffset";
    private Button button_x_inc, button_x_dec, button_y_inc, button_y_dec, button_reset;
    private native_adjustScreenOffset nav;
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
        if (nav == null)
            nav = new native_adjustScreenOffset();

        Rect offset = new Rect();
        nav.getScreenOffset(offset);
        offset.inset(dx, dy);
        nav.setScreenOffset(offset);

        Rect newoffset = new Rect();
        nav.getScreenOffset(newoffset);
        if (!newoffset.equals(offset)) {
            Toast.makeText(getApplicationContext(), "Can't modify screen offset. Is SE-Linux enforced?", Toast.LENGTH_LONG).show();
        }
    }

    private void resetScreenOffset() {
        if (nav == null)
            nav = new native_adjustScreenOffset();

        nav.resetScreenOffset();
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
