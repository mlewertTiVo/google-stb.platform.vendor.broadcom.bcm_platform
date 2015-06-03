package com.android.adjustScreenOffset;

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

public class BcmAdjustScreenOffsetActivity extends Activity {
    /** Called when the activity is first created. */
    private static final String TAG = "BcmAdjustScreenOffset";
    private Button button_x_inc, button_x_dec, button_y_inc, button_y_dec;
    private native_adjustScreenOffset nav;
    private final static int step_x = 4;
    private final static int step_y = 4;


    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        nav=new native_adjustScreenOffset();

        button_x_inc = (Button)findViewById(R.id.button_x_inc);
        button_x_dec = (Button)findViewById(R.id.button_x_dec);
        button_y_inc = (Button)findViewById(R.id.button_y_inc);
        button_y_dec = (Button)findViewById(R.id.button_y_dec);

        button_x_inc.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                Log.i(TAG,"click on button_x_inc");
                Rect offset = new Rect();
                nav.getScreenOffset(offset);
                offset.left -= step_x;
                offset.right += step_x;
                nav.setScreenOffset(offset);
            }
        });

        button_x_dec.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                Log.i(TAG,"click on button_x_dec");
                Rect offset = new Rect();
                nav.getScreenOffset(offset);
                offset.left += step_x;
                offset.right -= step_x;
                nav.setScreenOffset(offset);
            }
        });

        button_y_inc.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                Log.i(TAG,"click on button_y_inc");
                Rect offset = new Rect();
                nav.getScreenOffset(offset);
                offset.top -= step_y;
                offset.bottom += step_y;
                nav.setScreenOffset(offset);
            }
        });

        button_y_dec.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                Log.i(TAG,"click on button_y_dec");
                Rect offset = new Rect();
                nav.getScreenOffset(offset);
                offset.top += step_y;
                offset.bottom -= step_y;
                nav.setScreenOffset(offset);
            }
        });

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
