/***************************************************************************
*     (c)2010-2011 Broadcom Corporation
*
*  This program is the proprietary software of Broadcom Corporation and/or its licensors,
*  and may only be used, duplicated, modified or distributed pursuant to the terms and
*  conditions of a separate, written license agreement executed between you and Broadcom
*  (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
*  no license (express or implied), right to use, or waiver of any kind with respect to the
*  Software, and Broadcom expressly reserves all rights in and to the Software and all
*  intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
*  HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
*  NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
*
*  Except as expressly set forth in the Authorized License,
*
*  1.     This program, including its structure, sequence and organization, constitutes the valuable trade
*  secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
*  and to use this information only in connection with your use of Broadcom integrated circuit products.
*
*  2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
*  AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
*  WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
*  THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES
*  OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE,
*  LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION
*  OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF
*  USE OR PERFORMANCE OF THE SOFTWARE.
*
*  3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
*  LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR
*  EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR
*  USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF
*  THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT
*  ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
*  LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF
*  ANY LIMITED REMEDY.
*
* $brcm_Workfile: BcmSysInfoWidgetProvider.java $
* $brcm_Revision: 1 $
* $brcm_Date: 2/9/11 10:43a $
*
* API Description:
*
* Revision History:
*
* $brcm_Log:  $
* 
*
***************************************************************************/


package com.broadcom.BcmSysInfoWidget;

//Android package
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.Context;
import android.widget.RemoteViews;
import android.util.Log;
import android.net.wifi.WifiManager;
import android.net.DhcpInfo;

//Java package
import java.net.NetworkInterface;
import java.util.Enumeration;
import java.net.InetAddress;
import java.net.SocketException;

  
public class BcmSysInfoWidgetProvider extends AppWidgetProvider {
    
	private static final String TAG = "BcmSysInfoWidgetProvider";

	/** For retriving current IP string */
	static public String getLocalIpAddress(Context context) {
		try {
			//Generic way to obtain IP addr
			for (Enumeration<NetworkInterface> en = NetworkInterface.getNetworkInterfaces(); en.hasMoreElements();) {
				NetworkInterface NwkInterface = en.nextElement();
				for (Enumeration<InetAddress> enumIpAddr = NwkInterface.getInetAddresses(); enumIpAddr.hasMoreElements();) {
					InetAddress inetAddress = enumIpAddr.nextElement();
					if (!inetAddress.isLoopbackAddress()) {
						Log.d(TAG, "Got IP from generic nwk interface! addr=" + inetAddress.getHostAddress().toString());
						if( inetAddress.getHostAddress().toString().indexOf(':') == -1)
                            return inetAddress.getHostAddress().toString();
					}
				}
			}
		} catch (SocketException ex) {
			Log.e(TAG, ex.toString());
		}
		
		//Can't get IP in generic way, try get it from wifi manager

		WifiManager wifiManager =  (WifiManager)context.getSystemService(Context.WIFI_SERVICE);
		
		if(wifiManager != null){		
			if(wifiManager.isWifiEnabled())
			{		
				DhcpInfo dhcpInfo = wifiManager.getDhcpInfo();

				String[] dhcpInfos = dhcpInfo.toString().split(" ");
				String WifiIP = "null";
				int i = 0;
				while (i < dhcpInfos.length) {
					if (dhcpInfos[i].equals("ipaddr")) {
						WifiIP = dhcpInfos[i+1];
						Log.d(TAG, "Got IP from Wifi Manager! addr=" + WifiIP);
						return WifiIP;
					}
					i++;
				}
			}
			else{
				Log.w(TAG, "Wifi is not enabled");		
			}
		} else{
			Log.e(TAG, "Can't get wifiManager instance");		
		}
		
		Log.d(TAG, "Can't get IP from Network interface or Wifi manager!");
		return null;
	}
	
	/** Widget content refresh callback */
    public void onUpdate(Context context, AppWidgetManager appWidgetManager, int[] appWidgetIds) {
        final int N = appWidgetIds.length;
        for (int i=0; i<N; i++) {
            int appWidgetId = appWidgetIds[i];
            updateWidgetContent(context, appWidgetManager, appWidgetId);
        }
    }      
  
    /** Implementation of receiving widget fresh event */
    static void updateWidgetContent(Context context, AppWidgetManager appWidgetManager,
            int appWidgetId) {
    	CharSequence text;
    	
    	Log.d(TAG, "updateWidgetContent now");
      		
    	text = String.format("[System Info]\nIP: %s\n", getLocalIpAddress(context));
      	
        RemoteViews rViews = new RemoteViews(context.getPackageName(), R.layout.main);
        rViews.setTextViewText(R.id.appwidget_text, text);
  
        appWidgetManager.updateAppWidget(appWidgetId, rViews);
    }
}
