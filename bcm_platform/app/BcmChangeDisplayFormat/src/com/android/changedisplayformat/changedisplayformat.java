package com.android.changedisplayformat;

import android.app.Activity;
import android.widget.Button;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.util.Log;
import android.view.Window;

public class changedisplayformat extends Activity{
    /** Called when the activity is first created. */
    private static final String TAG = "changedisplayformat";
    private Button button1, button2, button3, button4, button5, button6, button7, button8, button9, button10;
    private native_changedisplayformat nav;
    private int curDisplayFormat, preDisplayFormat;
	

    @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            this.requestWindowFeature(Window.FEATURE_NO_TITLE);            
            setContentView(R.layout.main);

		button1 = (Button)findViewById(R.id.button1);		
		button2 = (Button)findViewById(R.id.button2);		
		button3 = (Button)findViewById(R.id.button3);	
		button4 = (Button)findViewById(R.id.button4);		
		button5 = (Button)findViewById(R.id.button5);	
		button6 = (Button)findViewById(R.id.button6);	
		button7 = (Button)findViewById(R.id.button7);	
		button8 = (Button)findViewById(R.id.button8);
		button9 = (Button)findViewById(R.id.button9);
    button10 = (Button)findViewById(R.id.button10);
    
		button1.setBackgroundColor(0xffdcdcdc);	
		button2.setBackgroundColor(0xffdcdcdc);	
		button3.setBackgroundColor(0xffdcdcdc);	
		button4.setBackgroundColor(0xffdcdcdc);	
		button5.setBackgroundColor(0xffdcdcdc);	
		button6.setBackgroundColor(0xffdcdcdc);	
		button7.setBackgroundColor(0xffdcdcdc);	
		button8.setBackgroundColor(0xffdcdcdc);
		button9.setBackgroundColor(0xffdcdcdc);		
    button10.setBackgroundColor(0xffdcdcdc);
    
    button1.setTextColor(0xff000000);	
		button2.setTextColor(0xff000000);	
		button3.setTextColor(0xff000000);	
		button4.setTextColor(0xff000000);	
		button5.setTextColor(0xff000000);	
		button6.setTextColor(0xff000000);	
		button7.setTextColor(0xff000000);	
		button8.setTextColor(0xff000000);
		button9.setTextColor(0xff000000);
 		button10.setTextColor(0xff000000);
    		
		nav=new native_changedisplayformat();
		curDisplayFormat = nav.getDisplayFormat();
//	        curDisplayFormat = 1;
		switch(curDisplayFormat)
		{
		case 0:
			button1.setBackgroundColor(0xffffff00);	
			break;
		case 1:
			button2.setBackgroundColor(0xffffff00);	
			break;
		case 2:
			button3.setBackgroundColor(0xffffff00);	
			break;
		case 3:
			button4.setBackgroundColor(0xffffff00);	
			break;
		case 4:
			button5.setBackgroundColor(0xffffff00);	
			break;
		case 5:
			button6.setBackgroundColor(0xffffff00);	
			break;
		case 6:
			button7.setBackgroundColor(0xffffff00);
			break;
		case 7:
			button8.setBackgroundColor(0xffffff00);
			break;
		case 8:
			button9.setBackgroundColor(0xffffff00);
			break;
    case 9:
			button10.setBackgroundColor(0xffffff00);
			break;

		default:
			button2.setBackgroundColor(0xffffff00);	
		}
		preDisplayFormat = curDisplayFormat;

		button1.setOnClickListener(new OnClickListener()
		{
			public void onClick(View v)
			{
				Log.v(TAG,"click on check button1");
				changeButtonFocus(0, preDisplayFormat);
				nav.setDisplayFormat(0);
				preDisplayFormat = 0;
			}
		});

		button2.setOnClickListener(new OnClickListener()
		{
			public void onClick(View v)
			{
				Log.v(TAG,"click on check button2");
				changeButtonFocus(1, preDisplayFormat);
				nav.setDisplayFormat(1);
				preDisplayFormat = 1;
			}
		});

		button3.setOnClickListener(new OnClickListener()
		{
			public void onClick(View v)
			{
				Log.v(TAG,"click on check button3");
				changeButtonFocus(2, preDisplayFormat);
				nav.setDisplayFormat(2);
				preDisplayFormat = 2;
			}
		});

		button4.setOnClickListener(new OnClickListener()
		{
			public void onClick(View v)
			{
				Log.v(TAG,"click on check button4");
				changeButtonFocus(3, preDisplayFormat);
				nav.setDisplayFormat(3);
				preDisplayFormat = 3;
			}
		});

		button5.setOnClickListener(new OnClickListener()
		{
			public void onClick(View v)
			{
				Log.v(TAG,"click on check button5");
				changeButtonFocus(4, preDisplayFormat);
				nav.setDisplayFormat(4);
				preDisplayFormat = 4;
			}
		});

		button6.setOnClickListener(new OnClickListener()
		{
			public void onClick(View v)
			{
				Log.v(TAG,"click on check button6");
				changeButtonFocus(5, preDisplayFormat);
				nav.setDisplayFormat(5);
				preDisplayFormat = 5;
			}
		});

		button7.setOnClickListener(new OnClickListener()
		{
			public void onClick(View v)
			{
				Log.v(TAG,"click on check button7");
				changeButtonFocus(6, preDisplayFormat);
				nav.setDisplayFormat(6);
				preDisplayFormat = 6;
			}
		});

		button8.setOnClickListener(new OnClickListener()
		{
			public void onClick(View v)
			{
				Log.v(TAG,"click on check button8");
				changeButtonFocus(7, preDisplayFormat);
				nav.setDisplayFormat(7);
				preDisplayFormat = 7;
			}
		});

		button9.setOnClickListener(new OnClickListener()
		{
			public void onClick(View v)
			{
				Log.v(TAG,"click on check button9");
				changeButtonFocus(8, preDisplayFormat);
				nav.setDisplayFormat(8);
				preDisplayFormat = 8;
			}
		});

		button10.setOnClickListener(new OnClickListener()
		{
			public void onClick(View v)
			{
				Log.v(TAG,"click on check button10");
				changeButtonFocus(9, preDisplayFormat);
				nav.setDisplayFormat(9);
				preDisplayFormat = 9;
			}
		});

  }
	
	private int changeButtonFocus(int  cur, int pre)
	{
		switch(cur)
		{
		case 0:
			button1.setBackgroundColor(0xffffff00);	
			break;
		case 1:
			button2.setBackgroundColor(0xffffff00);	
			break;
		case 2:
			button3.setBackgroundColor(0xffffff00);	
			break;
		case 3:
			button4.setBackgroundColor(0xffffff00);	
			break;
		case 4:
			button5.setBackgroundColor(0xffffff00);	
			break;
		case 5:
			button6.setBackgroundColor(0xffffff00);	
			break;
		case 6:
			button7.setBackgroundColor(0xffffff00);	
			break;
		case 7:
			button8.setBackgroundColor(0xffffff00);
			break;
    case 8:
			button9.setBackgroundColor(0xffffff00);
			break;
    case 9:
			button10.setBackgroundColor(0xffffff00);
			break;

		default:
			button2.setBackgroundColor(0xffffff00);	
		}

		switch(pre)
		{
		case 0:
			button1.setBackgroundColor(0xffdcdcdc);	
			break;
		case 1:
			button2.setBackgroundColor(0xffdcdcdc);	
			break;
		case 2:
			button3.setBackgroundColor(0xffdcdcdc);	
			break;
		case 3:
			button4.setBackgroundColor(0xffdcdcdc);	
			break;
		case 4:
			button5.setBackgroundColor(0xffdcdcdc);	
			break;
		case 5:
			button6.setBackgroundColor(0xffdcdcdc);	
			break;
		case 6:
			button7.setBackgroundColor(0xffdcdcdc);	
			break;
		case 7:
			button8.setBackgroundColor(0xffdcdcdc);
			break;
    case 8:
			button9.setBackgroundColor(0xffdcdcdc);
			break;
    case 9:
			button10.setBackgroundColor(0xffdcdcdc);
			break;

		default:
			button2.setBackgroundColor(0xffdcdcdc);

		}

		return 0;
	}
}
