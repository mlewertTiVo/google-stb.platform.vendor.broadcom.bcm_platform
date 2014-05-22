package com.android.generalSTBFunctions;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.util.Log;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Button;
import android.view.KeyEvent;


public class BcmGeneralSTBFunctions extends Activity {
    /** Called when the activity is first created. */
    private static final String TAG = "BcmGeneralSTBFunctions";
    private SeekBar brightness_set,contrast_set,saturation_set,hue_set;
    private TextView brightness_show,contrast_show,saturation_show,hue_show;
    private native_generalSTBFunctions nav;
    int get_value = 100;    
    private Button remove_output;
    
    
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        
        nav=new native_generalSTBFunctions();
        
        brightness_set = (SeekBar)findViewById(R.id.brightness_seek);
        contrast_set = (SeekBar)findViewById(R.id.contrast_seek);
        saturation_set = (SeekBar)findViewById(R.id.saturation_seek);
        hue_set = (SeekBar)findViewById(R.id.hue_seek);

        brightness_show = (TextView)findViewById(R.id.brightness_text);
        contrast_show = (TextView)findViewById(R.id.contrast_text);
        saturation_show = (TextView)findViewById(R.id.saturation_text);
        hue_show = (TextView)findViewById(R.id.hue_text);
        remove_output = (Button)findViewById(R.id.remove_output);
      
        /*set all the SeekBar*/
        get_value = nav.getBrightness();
        brightness_set.setMax(100);
        brightness_set.setProgress(get_value);
        brightness_show.setText(String.format(getString(R.string.brightness_progress),get_value+"%"));
         
        get_value = nav.getContrast();        
        contrast_set.setMax(100);        
        contrast_set.setProgress(get_value);
        contrast_show.setText(String.format(getString(R.string.contrast_progress), get_value+"%")); 
        
        get_value = nav.getSaturation();
        saturation_set.setMax(100);
        saturation_set.setProgress(get_value);
        saturation_show.setText(String.format(getString(R.string.saturation_progress), get_value+"%")); 
        
        get_value = nav.getHue();
        hue_set.setMax(100);
        hue_set.setProgress(get_value);
        hue_show.setText(String.format(getString(R.string.hue_progress), get_value+"%"));
        brightness_set.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() { 
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                nav.setBrightness(seekBar.getProgress());
            } 
  
            @Override  
            public void onStartTrackingTouch(SeekBar seekBar) {
            } 
  
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) { 
                brightness_show.setText(String.format(getString(R.string.brightness_progress), progress+"%")); 
            } 
        });
        
        contrast_set.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() { 
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                nav.setContrast(seekBar.getProgress());
            } 

            @Override  
            public void onStartTrackingTouch(SeekBar seekBar) {
            } 
  
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) { 
                contrast_show.setText(String.format(getString(R.string.contrast_progress), progress+"%"));
            } 
        });
       
        saturation_set.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() { 
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                nav.setSaturation(seekBar.getProgress());
            } 
  
            @Override  
            public void onStartTrackingTouch(SeekBar seekBar) {
            } 
  
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) { 
                saturation_show.setText(String.format(getString(R.string.saturation_progress), progress+"%")); 
            } 
        });
        
        hue_set.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() { 
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                nav.setHue(seekBar.getProgress());
            } 
  
            @Override  
            public void onStartTrackingTouch(SeekBar seekBar) {
            } 
  
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) { 
                hue_show.setText(String.format(getString(R.string.hue_progress), progress+"%"));
            } 
        });

        /*set remove output Button*/
        remove_output.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                Log.i(TAG,"click on button remove output");
                nav.removeOutput();
            }
        });
    }
    
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) 
    { 
        Log.i(TAG,"key down "+keyCode);
        switch (keyCode) 
        { 
            /*use number key 1 to add output*/
            case KeyEvent.KEYCODE_1:
                Log.i(TAG,"key down is KEYCODE_1");
                nav.addOutput();
                return true; 
            
            default: 
                break; 
        } 
        return super.onKeyDown(keyCode, event);
    } 
}
