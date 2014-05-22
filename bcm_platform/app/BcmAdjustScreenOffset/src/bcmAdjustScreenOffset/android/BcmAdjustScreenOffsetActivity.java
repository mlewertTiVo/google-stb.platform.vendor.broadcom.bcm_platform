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

public class BcmAdjustScreenOffsetActivity extends Activity {
    /** Called when the activity is first created. */
    private static final String TAG = "BcmAdjustScreenOffset";
    private Button button_x_inc, button_x_dec, button_y_inc, button_y_dec, button_fmt_1080i, button_fmt_720p;
    private static int xoff = 0, yoff = 0, width = 1280, height = 720;
    private native_adjustScreenOffset nav;
    
    
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        
        nav=new native_adjustScreenOffset();
        
        button_x_inc = (Button)findViewById(R.id.button_x_inc);		
		button_x_dec = (Button)findViewById(R.id.button_x_dec);		
		button_y_inc = (Button)findViewById(R.id.button_y_inc);	
		button_y_dec = (Button)findViewById(R.id.button_y_dec);
		
		button_x_inc.setOnClickListener(new OnClickListener()
		{
			public void onClick(View v)
			{
                        Log.i(TAG,"click on button_x_inc");
                        nav.getScreenOffset(xoff, yoff, width, height);
                        xoff -= 4;
                        width += 8;                        
                        nav.setScreenOffset(xoff, yoff, width, height);
			}
		});
		
		button_x_dec.setOnClickListener(new OnClickListener()
		{
			public void onClick(View v)
			{
                        Log.i(TAG,"click on button_x_dec");
                        nav.getScreenOffset(xoff, yoff, width, height);
                        xoff += 4;
                        width -= 8;                        
                        nav.setScreenOffset(xoff, yoff, width, height);
			}
		});
		
		button_y_inc.setOnClickListener(new OnClickListener()
		{
			public void onClick(View v)
			{
                        Log.i(TAG,"click on button_y_inc");
                        nav.getScreenOffset(xoff, yoff, width, height);
                        yoff -= 4;
                        height += 8;                        
                        nav.setScreenOffset(xoff, yoff, width, height);
			}
		});
		
		button_y_dec.setOnClickListener(new OnClickListener()
		{
			public void onClick(View v)
			{
                        Log.i(TAG,"click on button_y_dec");
                        nav.getScreenOffset(xoff, yoff, width, height);
                        yoff += 4;
                        height -= 8;                        
                        nav.setScreenOffset(xoff, yoff, width, height);

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
