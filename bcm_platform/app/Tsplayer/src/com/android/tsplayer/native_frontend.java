package com.android.tsplayer;
import android.util.Log;
public class native_frontend {
    private static final String TAG = "native_frontend";
    public final int ARR_NUM=30; 
    private int num_services = 0;
    static {
        System.loadLibrary("jni_tune");
    }

    public class services_info {
        int input,a,v,p,a_c,v_c;
        int ecm,emm;
        boolean inited;
    }
    public services_info  info[] = new services_info[ARR_NUM];

    public int service_amount()
    {
        int i,co=0;
        for (i =0;i<ARR_NUM;i++)
            if (info[i].inited == true)
                co ++;
        return co;
    }

    public void set_services(int inputband,int a_pid,int v_pid,int p_pid,int a_c,int v_c,int ecm, int emm,int idx)
    {
        Log.v(TAG,"scan result:"+inputband+a_pid + v_pid + p_pid + a_c + v_c + ecm +emm+idx + "num_services: " + num_services);

        if (idx >= ARR_NUM)
            return ;
        info[num_services].input = inputband;    
        info[num_services].a = a_pid;
        info[num_services].v = v_pid;
        info[num_services].p = p_pid;
        info[num_services].a_c = a_c;
        info[num_services].v_c = v_c;
        info[num_services].ecm = ecm;
        info[num_services].emm = emm;
        info[num_services].inited = true;
        num_services ++;

        return ;
    }

    public void reset(){
        int i;
        for (i = 0;i<ARR_NUM;i++)
        {
            info[i] = new services_info();
            info[i].inited = false;
        }
        num_services = 0;
    }
    /** 
     * tune a frequence.
     */
    public native double lock_signal(int type,int freq,int symbol,int mode);

    /**
     * get services list
     */
    public native int services_get(int inputband);

    public native int dvbca_playset(int v,int a,int ecm,int emm);
}
