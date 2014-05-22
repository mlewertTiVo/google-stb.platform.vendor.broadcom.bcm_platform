package com.broadcom.tvarchive;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.widget.TextView;
import android.util.Log;
import java.io.*;
import android.view.ActionMode;
import java.util.Timer;
import java.util.TimerTask;
import android.os.Handler;
import android.text.method.ScrollingMovementMethod;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.LinearLayout.LayoutParams;
import android.view.View;

import android.app.ActivityManager;
import android.app.ActivityManager.RunningAppProcessInfo;
import java.util.List;
import java.util.Iterator;
import android.os.Process;
import android.app.AlertDialog;
import android.app.AlertDialog.Builder;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;

public class BcmTVArchive
{
    public String TAG = "BcmTVArchive";
    private BcmTVArchive_JNI TV_native;

    public BcmTVArchive()
    {
//		Log.e(TAG, "Creating BcmTVArchive_JNI object!!");
		TV_native = new BcmTVArchive_JNI();
    }

	public boolean setChannel(int pos)
	{
		boolean bRet;

//		Log.e(TAG, "Calling setChannel, pos = " +pos);
		bRet = TV_native.setChannel(pos);
//		Log.e(TAG, "Returned from setChannel, bRet = " +bRet);

		return bRet;
	}

	public int stop()
	{
		int iRet = -1;

//		Log.e(TAG, "Calling stop");
		iRet = TV_native.stop();
//		Log.e(TAG, "Returned from stop");

		return iRet;
	}

    public String[] getChannelList(int pos)
    {
        String[] iList;

//		Log.e(TAG, "Calling getChannelList");
		iList = TV_native.getChannelList(pos);
//		Log.e(TAG, "Returned from getChannelList");

        return iList;
    }

	public int setChannel_pip(int pos, int x, int y, int w, int h)
	{
		int iRet = -1;

		iRet = TV_native.setChannel_pip(pos, x, y, w, h);

		return iRet;
	}

	public int stop_pip()
	{
		int iRet = -1;

//		Log.e(TAG, "Calling stop_pip");
		iRet = TV_native.stop_pip();
//		Log.e(TAG, "Returned from stop_pip");

		return iRet;
	}

	public int setMode(boolean bIsFCCOn)
	{
		int iRet = -1;

//		Log.e(TAG, "Calling setMode");
		iRet = TV_native.setMode(bIsFCCOn);
//		Log.e(TAG, "Returned from setMode");

		return iRet;
	}

	public int reprime(int iNum)
	{
		int iRet = -1;

//		Log.e(TAG, "Calling reprime");
		iRet = TV_native.reprime(iNum);
//		Log.e(TAG, "Returned from reprime");

		return iRet;
	}

	public String[] getPrimedList()
	{
        String[] iList;

//		Log.e(TAG, "Calling getPrimedList");
		iList = TV_native.getPrimedList();
//		Log.e(TAG, "Returned from getPrimedList");

        return iList;
	}

	public void enforce_scan()
	{
		TV_native.force_scan();
	}

	public int close()
	{
		int iRet = -1;

//		Log.e(TAG, "Calling close");
		iRet = TV_native.close();
//		Log.e(TAG, "Returned from close");

		return iRet;
	}
}