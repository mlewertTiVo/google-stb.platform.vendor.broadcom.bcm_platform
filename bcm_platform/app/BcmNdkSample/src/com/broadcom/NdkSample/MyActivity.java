package com.broadcom.NdkSample;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;

public class MyActivity extends Activity {
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        
        MyJni JniTest = new MyJni();
        
        TextView t = (TextView)findViewById(R.id.myTextView);
        t.setText(JniTest.hello());
    }
}