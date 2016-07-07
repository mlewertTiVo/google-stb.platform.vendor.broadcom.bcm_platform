package com.broadcom.TrellisUI;

import android.app.Activity;
import android.os.Bundle;
import android.widget.EditText;
import android.widget.Button;
import android.view.View.OnClickListener;
import android.view.View;
import android.content.Intent;

public class config extends Activity {
	
	private EditText url;
	private Button btn;

	
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        setContentView(R.layout.ui);
        
        url = (EditText) findViewById(R.id.editText1);
        btn = (Button) findViewById(R.id.button1);
        
        btn.setOnClickListener(new OnClickListener(){
        	public void onClick(View v){
            	//jws.setUrl(url.getText().toString());
            	settings.setUrl(url.getText().toString());
            	startActivity(new Intent(config.this, WebsocketActivity.class));
				finish();
        	}
        });
    }
}
