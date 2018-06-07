/*
 * Copyright 2016-2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.broadcom.tvinput;

import android.util.Log;
import android.view.Surface;


/* Singleton class */
public class BcmTunerJniBridge {

    //Load native tuner
    static {
        System.loadLibrary("bcmtuner");
    }

    //List of all native methods available in bcmtuner. java->jni
    public native int getMsgFromJni();
    public native int tune(int tsid, int sid);
    public native int stop();
    public native int closeSession();
    public native int servicesScan();
    public native int servicesAvailable();
    public native int uninitializeTuner();
    public native String getScanResults();

    public native int releaseSdb();
    public native int initialiseSdb(Surface s);

    private static BcmTunerJniBridge instance;

    private BcmTunerJniBridge(){}

    public static BcmTunerJniBridge getInstance(){
        if(instance == null){
            instance = new BcmTunerJniBridge();
        }
        return instance;
    }
}

