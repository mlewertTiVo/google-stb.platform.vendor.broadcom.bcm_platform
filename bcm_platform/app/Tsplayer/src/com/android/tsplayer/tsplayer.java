package com.android.tsplayer;

import android.app.Activity;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.app.AlertDialog;

import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;

import android.widget.EditText;
import android.view.View;
import android.view.View.OnClickListener;
import android.util.Log;

public class tsplayer extends Activity implements OnClickListener{
    /** Called when the activity is first created. */
    private static final String TAG = "tsplayer";
    private EditText freq,sym;
    private RadioGroup mRadioGroup1; 
    private RadioButton mRadio1,mRadio2,mRadio3;
    private int cur_mode;

    @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            setContentView(R.layout.main);

            cur_mode = 0;
            freq = (EditText)findViewById(R.id.editText1);
            sym = (EditText)findViewById(R.id.editText2);

            mRadioGroup1 = (RadioGroup) findViewById(R.id.radioGroup1);
            mRadio1 = (RadioButton) findViewById(R.id.radio0);
            mRadio2 = (RadioButton) findViewById(R.id.radio1);
            mRadio3 = (RadioButton) findViewById(R.id.radio2);

            View start = findViewById(R.id.button1);

            mRadioGroup1.setOnCheckedChangeListener(mChangeRadio);
            start.setOnClickListener(this);
        }
    private RadioGroup.OnCheckedChangeListener mChangeRadio = new 
        RadioGroup.OnCheckedChangeListener()
        { 
            public void onCheckedChanged(RadioGroup group, int checkedId)
            { 
                Log.v(TAG,"click on check button");
                if (checkedId == mRadio1.getId())
                {
                    cur_mode = 0;
                    return;
                }
                if (checkedId == mRadio2.getId())
                {
                    cur_mode = 1;
                    return;
                }
                if (checkedId == mRadio3.getId())
                {
                    cur_mode = 2;
                    return;
                }
            }
        }; 
    public void onClick(View v) {

        Log.v(TAG,"on Click scan button");
        switch (v.getId()) {
            case R.id.button1:
                Log.v(TAG,"11111on Click scan button");
                String f,s,m;
                f = freq.getText().toString();
                if (f.compareTo("") == 0)
                {
                    show_error("please input frequence");
                    break;
                }

                s = sym.getText().toString();
                if (s.compareTo("") == 0)
                {
                    show_error("please input Symbol rate");
                    break;
                }
                Intent myIntent = new Intent(this, show_video.class);

                myIntent.putExtra("frequency", f);
                myIntent.putExtra("symbol_rate", s);
                switch(cur_mode)
                {
                    case 0: m="64";break;
                    case 1: m="256";break;
                    case 2: m="1024";break;
                    default: m="64";break;
                }

                myIntent.putExtra("mode", m);
                startActivity(myIntent);

                break;
        }
    }
    private int show_error(CharSequence title){
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle(title);

        builder.setPositiveButton("ok", new DialogInterface.OnClickListener(){
            public void onClick(DialogInterface dialog, int item) {

                dialog.dismiss();
            }
        });

        builder.create().show();
        return 0;
    }

}
