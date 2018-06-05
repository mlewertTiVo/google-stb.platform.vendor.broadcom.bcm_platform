package com.broadcom.BcmBP3Config;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.text.InputType;
import android.text.method.PasswordTransformationMethod;
import android.text.method.ScrollingMovementMethod;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.RelativeLayout;
import android.widget.TextView;
import android.widget.Toast;
import android.util.Log;
import android.os.RemoteException;

import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;

import bcm.hardware.bp3.V1_0.Ibp3;

public class BP3Activity extends Activity
{
	private static final String TAG = "BcmBP3Config";
    enum Mode
    {
        STATUS, SERVICE, PROVISION, INVALID
    }

	enum State
	{		
		INIT, SERVICE_SEARCH, SERVICE_FOUND, SERVICE_FAILED, INTERFACE_VALID, STATUS_INVALID, STATUS_RETRIEVED
	}

    Context mContext;
    private RadioGroup mRadioGroup;
    private RadioButton mStatusButton, mServerButton, mProvisionButton;
    private TextView mInfo1, mInfo2;
    private EditText mArgs1, mArgs2;
    private Button mRunButton;

    private String strStatusResult = "N/A";
	/* = "Broadcom - H264/AVC\n" +
                                        "Broadcom - MPEG-2\n" +
                                        "Broadcom - AVS\n" +
                                        "Broadcom - H265 (HEVC)\n" +
                                        "Broadcom - VP9\n" +
                                        "Broadcom - CA Multi2\n" +
                                        "Broadcom - CA DVB-CSA3\n" +
                                        "Dolby - Post Proc: DAP\n" +
                                        "Dolby - Decode Dolby Digital\n" +
                                        "Dolby - Decode Dolby Digital Plus\n" +
                                        "Dolby - Decode AC4\n" +
                                        "Dolby - Decode TrueHD\n" +
                                        "Dolby - MS10/11\n" +
                                        "Dolby - MS12 v1\n" +
                                        "Dolby - MS12 v2\n" +
                                        "DTS - DTS TruVolume\n" +
                                        "DTS - DTS Digital Surround\n" +
                                        "DTS - DTS-HD (M6)\n" +
                                        "DTS - DTS-HDMA (M8)\n" +
                                        "DTS - DTS Headphone:X\n" +
                                        "DTS - DTS Virtual:X\n" +
                                        "DTS - DTS:X";*/
    private String strStatusInfo = "Displays the currently enabled licenses\n";
    private String strServerInfo = "IP Address (xx.xx.xx.xx)";
    private String strKeyInfo = "Authentication Key:";
    private String strLicenseInfo = "Licenses (ex: 3,4):";
    private String strPortInfo = "Port (default is 80):";

    private String strStatusArgs = "N/A";
    private String strServerArgs = "";
    private String strKey = "", strLicenses = "";
    private Mode iMode = Mode.STATUS;
	private State iState = State.INIT;
    private String strRun = "Run App", strStop = "Stop App";
    private RelativeLayout.LayoutParams mParams1;

    SharedPreferences sharedPref;
    SharedPreferences.Editor editor;

	private Ibp3 bp3Interface;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_bp3);
//        Toast.makeText(this,"In main page...", Toast.LENGTH_LONG).show();

        mContext= this.getApplicationContext();
//        sharedPref = PreferenceManager.getDefaultSharedPreferences(mContext);
//        editor = sharedPref.edit();

		iState = State.INIT;

        // Get the widgets reference from XML layout
        mRadioGroup = (RadioGroup) findViewById(R.id.idRadioGroup);
        mStatusButton = (RadioButton) findViewById(R.id.idStatusButton);
        mServerButton = (RadioButton) findViewById(R.id.idServerButton);
        mProvisionButton = (RadioButton) findViewById(R.id.idProvisionButton);

        // TextView with scrolling enabled
        mInfo1 = (TextView) findViewById(R.id.idInfo1);
        mInfo1.setMovementMethod(new ScrollingMovementMethod());

        // LayoutParams for mInfo1 TextView
        mParams1 = (RelativeLayout.LayoutParams) mInfo1.getLayoutParams();
        mParams1.height = getResources().getDimensionPixelSize(R.dimen.large_height);
        mInfo1.setLayoutParams(mParams1);

        mArgs1 = (EditText) findViewById(R.id.idArgs1);

        mInfo2 = (TextView) findViewById(R.id.idInfo2);
        mArgs2 = (EditText) findViewById(R.id.idArgs2);

        mRunButton = (Button) findViewById(R.id.idRunApp);

        // Enable only the status button to begin with
        mStatusButton.setChecked(true);
        mInfo1.setText(strStatusInfo);
        mArgs1.setVisibility(View.INVISIBLE);
        mArgs1.setText(strStatusArgs);
        mArgs1.setEnabled(false);

        mInfo2.setVisibility(View.INVISIBLE);
        mArgs2.setVisibility(View.INVISIBLE);

		iState = State.SERVICE_SEARCH;
		Log.w(TAG, "iState = SERVICE_SEARCH");

		// Get the service handle from the HAL
		try {
           bp3Interface = Ibp3.getService();
        } catch (RemoteException e) {
			iState = State.SERVICE_FAILED;
//		   Toast.makeText(mContext, "RemoteException trying to reach Ibp3", Toast.LENGTH_LONG).show();
           Log.w(TAG, "RemoteException trying to reach Ibp3", e);
           return;
        }

		iState = State.SERVICE_FOUND;
		Log.w(TAG, "iState = SERVICE_FOUND");

        if (bp3Interface == null) {
//		   Toast.makeText(mContext, "BP3 service extension is null", Toast.LENGTH_LONG).show();
		   Log.w(TAG, "BP3 service extension is null");
           throw new IllegalArgumentException("BP3 service extension is null");
        }

		iState = State.INTERFACE_VALID;
		Log.w(TAG, "iState = INTERFACE_VALID");

        mRadioGroup.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup group, int checkedId) {
                // checkedId is the RadioButton selected
                if (checkedId == R.id.idStatusButton) {
//                    Toast.makeText(group.getContext(),"idStatusButton...", Toast.LENGTH_LONG).show();
                    mParams1.height = getResources().getDimensionPixelSize(R.dimen.large_height);
                    mInfo1.setLayoutParams(mParams1);
                    mInfo1.setText(strStatusInfo);

                    mArgs1.setText(strStatusArgs);
                    mArgs1.setEnabled(false);
                    mArgs1.setVisibility(View.INVISIBLE);
                    mArgs1.setInputType(InputType.TYPE_CLASS_TEXT);

                    mInfo2.setVisibility(View.INVISIBLE);
                    mArgs2.setVisibility(View.INVISIBLE);
                    iMode = Mode.STATUS;
                } else if (checkedId == R.id.idServerButton) {
//                    Toast.makeText(group.getContext(),"idServerButton...", Toast.LENGTH_LONG).show();
                    mParams1.height = getResources().getDimensionPixelSize(R.dimen.small_height);
                    mInfo1.setText(strServerInfo);

                    mArgs1.setVisibility(View.VISIBLE);
                    mArgs1.setText(strServerArgs);
                    mArgs1.setEnabled(true);
                    mArgs1.setInputType(InputType.TYPE_CLASS_TEXT);

                    mInfo2.setVisibility(View.VISIBLE);
                    mArgs2.setVisibility(View.VISIBLE);

                    mInfo2.setText(strPortInfo);
                    mArgs2.setText(strLicenses);
                    mArgs2.setEnabled(true);

                    iMode = Mode.SERVICE;
                } else if (checkedId == R.id.idProvisionButton) {
//                    Toast.makeText(group.getContext(),"idProvisionButton...", Toast.LENGTH_LONG).show();

//                    Toast.makeText(group.getContext(),"Authentication-Key: " + sharedPref.getString("AuthKey", "invalid"), Toast.LENGTH_LONG).show();

                    mParams1.height = getResources().getDimensionPixelSize(R.dimen.small_height);
                    mInfo1.setLayoutParams(mParams1);

                    mInfo1.setText(strKeyInfo);
                    mArgs1.setVisibility(View.VISIBLE);
                    mArgs1.setText(strKey);
                    mArgs1.setEnabled(true);

//                    editor.putString("AuthKey", "brcm1234").apply();

//                    mArgs1.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
//                    mArgs1.setTransformationMethod(PasswordTransformationMethod.getInstance());

                    mInfo2.setVisibility(View.VISIBLE);
                    mArgs2.setVisibility(View.VISIBLE);

                    mInfo2.setText(strLicenseInfo);
                    mArgs2.setText(strLicenses);
                    mArgs2.setEnabled(true);

                    iMode = Mode.PROVISION;
                }
            }
        });

        mRunButton.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                String buttonStr = mRunButton.getText().toString();

//                Toast.makeText(v.getContext(),"Mode = " +iMode, Toast.LENGTH_LONG).show();

                switch (iMode) {
                    // Retrieving the currently enabled licenses
                    case STATUS:
                    default:
//                        Toast.makeText(v.getContext(), "Retrieving the status...", Toast.LENGTH_LONG).show();

						try {
						  strStatusResult = bp3Interface.status();
						} catch (RemoteException e) {
						   iState = State.STATUS_INVALID;
						   Log.w(TAG, "RemoteException calling Ibp3.status ", e);
						   return;
						}

						iState = State.STATUS_RETRIEVED;
						Log.w(TAG, "iState = STATUS_RETRIEVED");

						Log.w(TAG, "strStatusResult = " + strStatusResult);

						if (!strStatusResult.isEmpty())
							mInfo1.setText(strStatusResult);
						else
							mInfo1.setText("N/A");
                        break;

                    // Run as a service
                    case SERVICE:
                        // Check if we need to stop the service
                        if (buttonStr.equals(strStop)) {
//                            Toast.makeText(v.getContext(), "Stopping the service...", Toast.LENGTH_LONG).show();

                            // Stop the service now
                            // TBD

                            // Change the button text to Run
                            mRunButton.setText(strRun);

                            // Enable the other buttons
                            mStatusButton.setEnabled(true);
                            mProvisionButton.setEnabled(true);
                            mArgs1.setEnabled(true);
                        }

                        // Or, if we need to run in Service mode
                        else //if (iMode == Mode.SERVICE)
                        {
//                            Toast.makeText(v.getContext(), "Starting the service...", Toast.LENGTH_LONG).show();

                            // Change the button text to Stop
                            mRunButton.setText(strStop);

                            // Also, disable the other 2 radio buttons. Can only be enabled once the Service is stopped
                            mStatusButton.setEnabled(false);
                            mProvisionButton.setEnabled(false);
                            mArgs1.setEnabled(false);
                        }
                        break;

                    // Provisioning the box with new licenses
                    case PROVISION:
//                        Toast.makeText(v.getContext(), "Provisioning the box...", Toast.LENGTH_LONG).show();
                        break;
                }
            }
        });
    }
}
