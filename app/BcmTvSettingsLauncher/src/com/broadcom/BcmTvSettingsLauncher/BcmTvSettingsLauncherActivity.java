package com.broadcom.BcmTvSettingsLauncher;

import android.app.Activity;
import android.os.Bundle;
import android.content.Intent;
import android.provider.Settings;

public class BcmTvSettingsLauncherActivity extends Activity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Intent settings = new Intent(android.provider.Settings.ACTION_SETTINGS);
        settings.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                            | Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);

        startActivity(settings);
        finish();
    }
}
