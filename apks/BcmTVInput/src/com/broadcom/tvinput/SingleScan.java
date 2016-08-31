package com.broadcom.tvinput;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RadioGroup;
import android.content.Intent;
import android.util.Log;

public class SingleScan extends Activity {

    private static final String TAG = SingleScan.class.getSimpleName();
    private int scanType; //scanType identifies whether this activity is started for
                          //network scan or frequncy scan
    private int frontendType; //this stores frontend type needed for crating the
                              //scanParams needed for network scan

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        scanType = getIntent().getIntExtra(MainActivity.SCAN_TYPE,MainActivity.ACTION_NETWORK_SCAN);
        frontendType = getIntent().getIntExtra(MainActivity.FRONTEND_TYPE,MainActivity.FrontendType.Satellite.ordinal());

        if( frontendType == MainActivity.TUNE_SIGNAL_QPSK ) {
            //single scan cureently supported only for SAT FE
            setContentView(R.layout.frequency_scan);


            final Button next_button = (Button) findViewById(R.id.next_button);
            final EditText mTransFreqId = (EditText) findViewById(R.id.transFreqId);
            final EditText mSymRateId = (EditText) findViewById(R.id.symRateId);

            next_button.setOnFocusChangeListener(new View.OnFocusChangeListener(){
                @Override
                public void onFocusChange(View v, boolean gainFocus)
                {
                    if(gainFocus == true) {
                        next_button.setBackgroundResource(R.drawable.roundedcorner_bg_focus);
                    }
                    else
                    {
                        next_button.setBackgroundResource(R.drawable.roundedcorner_bg);
                    }
                }
            });

            mTransFreqId.setOnFocusChangeListener(new View.OnFocusChangeListener(){
                @Override
                public void onFocusChange(View v, boolean gainFocus)
                {
                    if(gainFocus == true) {
                        mTransFreqId.setBackgroundResource(R.drawable.roundedcorner_bg_focus);
                    }
                    else
                    {
                        mTransFreqId.setBackgroundResource(R.drawable.roundedcorner_bg);
                    }
                }
            });


            mSymRateId.setOnFocusChangeListener(new View.OnFocusChangeListener(){
                @Override
                public void onFocusChange(View v, boolean gainFocus)
                {
                    if(gainFocus == true) {
                        mSymRateId.setBackgroundResource(R.drawable.roundedcorner_bg_focus);
                    }
                    else
                    {
                        mSymRateId.setBackgroundResource(R.drawable.roundedcorner_bg);
                    }
                }
            });
        }
        else if (frontendType == MainActivity.TUNE_SIGNAL_COFDM ) {
            ScanParams sp = new ScanParams();
            sp.deliverySystem = ScanParams.DeliverySystem.Dvbt;
            sp.scanMode = ScanParams.ScanMode.Home;

            Bundle b = new Bundle();
            b.setClassLoader(ScanParams.class.getClassLoader());
            b.putParcelable("scanParams", sp);

            Intent intent = new Intent(this,TunerSettings.class);
            intent.putExtras(b);
            startActivity(intent);

        }
    }

    public void startFrequencyScan(View v)
    {
        ScanParams sp = new ScanParams();
        //Getting reference to input elements
        RadioGroup mSatModeId = (RadioGroup)findViewById(R.id.satModeId);
        EditText mTransFreqId = (EditText)findViewById(R.id.transFreqId);
        RadioGroup mPolarId = (RadioGroup)findViewById(R.id.polarId);
        EditText mSymRateId = (EditText)findViewById(R.id.symRateId);
        RadioGroup mSearchTypeId = (RadioGroup)findViewById(R.id.search_type_Id);

        //Populate ScanParams
        switch(frontendType) {
            case MainActivity.TUNE_SIGNAL_QPSK:
                sp.deliverySystem = ScanParams.DeliverySystem.Dvbs;
                break;
            case MainActivity.TUNE_SIGNAL_COFDM:
                sp.deliverySystem = ScanParams.DeliverySystem.Dvbt;
                break;
            default:
                sp.deliverySystem = ScanParams.DeliverySystem.Undefined;
        }

        switch(scanType) {
            case MainActivity.ACTION_NETWORK_SCAN:
                sp.scanMode = ScanParams.ScanMode.Home;
                break;
            case MainActivity.ACTION_FREQUENCY_SCAN:
                sp.scanMode = ScanParams.ScanMode.Single;
                break;
            default:
                sp.scanMode = ScanParams.ScanMode.Undefined;
        }

        switch(mSatModeId.getCheckedRadioButtonId()) {
            case R.id.sat_mode_choice1:
                sp.satelliteMode = ScanParams.SatelliteMode.SatQpskLdpc;
                break;
            case R.id.sat_mode_choice2:
                sp.satelliteMode = ScanParams.SatelliteMode.Sat8pskLdpc;
                break;
            default:
        }

        //Handle case of empty input values
        if(mTransFreqId.getText().length() == 0) {
            Log.e(TAG,"Invalid frequency, using default value of 11627Mhz");
            sp.freqKHz = 11627;
        }
        else {
            sp.freqKHz = Integer.parseInt(mTransFreqId.getText().toString());
        }

        switch(mPolarId.getCheckedRadioButtonId())    {
            case R.id.pol_choice1:
                sp.satellitePolarity = ScanParams.SatellitePolarity.Vertical;
                break;
            case R.id.pol_choice2:
                sp.satellitePolarity = ScanParams.SatellitePolarity.Horizontal;
                break;
            default:
        }

        if(mSymRateId.getText().length() == 0) {
            Log.e(TAG,"Invalid symbolrate, using default value of 22000");
            sp.symK = 22000;
        }
        else {
            sp.symK = Integer.parseInt(mSymRateId.getText().toString());
        }

        switch(mSearchTypeId.getCheckedRadioButtonId())    {
            case R.id.search_type_choice1:
                sp.encrypted = false;
                break;
            case R.id.search_type_choice2:
                sp.encrypted = true;
                break;
            default:
        }

        Log.d(TAG,"startFrequency() satMode: " + sp.satelliteMode + " freq: " + sp.freqKHz + " polarity: " + sp.satellitePolarity + " symRate: " + sp.symK + " encrypted: " + sp.encrypted);

        Bundle b = new Bundle();
        b.setClassLoader(ScanParams.class.getClassLoader());
        b.putParcelable("scanParams", sp);

        Intent intent = new Intent(this,TunerSettings.class);
        intent.putExtras(b);
        startActivity(intent);

    }
}
