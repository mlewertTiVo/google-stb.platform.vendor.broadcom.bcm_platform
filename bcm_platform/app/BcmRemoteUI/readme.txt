This readme.txt describes purpose and usage of Remote UI app. It also explains how to install Remote UI apk on 
Android mobile phone from Windows PC.

Purpose: Remote UI app is a remote interface for Broadcom's Trellis application as well as Broadcom
android box. It connects to Trellis application or android box using websocket. Remote UI app can run on Broadcom 
Android reference box and also on Android mobile phone.

Usage:
1. Remote UI for Trellis: To connect to Trellis application, feed in IP address of Broadcom 
   reference box on which Trellis is running in "Remote Box URL" text box and click "Connect" button.
2. Remote UI for Android Box: To connect to Android box, feed in URL "<IP address>:7681" in "Remote Box URL" text 
   box and click "Connect" button. Where <IP address> is the IP address of Broadcom android box.

Installation steps: After building android-rfs, Remote UI can be found at 
android-rfs/system/app/BcmRemoteUI.apk.

1. Remote UI app can be installed on Android mobile phone using adb (Android Debug Bridge).
   You can find the adb tool in <sdk>/platform-tools/. Android sdk can be installed from below link.
   http://developer.android.com/sdk/index.html

2. On Windows PC install appropriate USB driver for your Android mobile phone. You can search for the
   windows USB driver in below link.
   http://developer.android.com/tools/extras/oem-usb.html

3. Make below changes in Android phone settings.
   a) Enable Settings->Applications->Unknown sources
   b) Enable Settings->Applications->Development->USB debugging

4. Connect Android phone to Windows PC by using USB and run Windows CMD shell.

5. Modify environment variable PATH to include <sdk>/platform-tools.

6. Check if adb detects the mobile phone using below command.
   adb devices

7. Install Remote UI app using below command.
   adb install <path_to_apk>