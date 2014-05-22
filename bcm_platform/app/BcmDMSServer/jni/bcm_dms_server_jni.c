/******************************************************************************
 *    (c)2011 Broadcom Corporation
 * 
 * This program is the proprietary software of Broadcom Corporation and/or its licensors,
 * and may only be used, duplicated, modified or distributed pursuant to the terms and
 * conditions of a separate, written license agreement executed between you and Broadcom
 * (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 * no license (express or implied), right to use, or waiver of any kind with respect to the
 * Software, and Broadcom expressly reserves all rights in and to the Software and all
 * intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 * HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 * NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.  
 *  
 * Except as expressly set forth in the Authorized License,
 *  
 * 1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 * secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 * and to use this information only in connection with your use of Broadcom integrated circuit products.
 *  
 * 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS" 
 * AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR 
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO 
 * THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES 
 * OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, 
 * LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION 
 * OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF 
 * USE OR PERFORMANCE OF THE SOFTWARE.
 * 
 * 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS 
 * LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR 
 * EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR 
 * USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT 
 * ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE 
 * LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF 
 * ANY LIMITED REMEDY.
 *
 * $brcm_Workfile: bcm_dms_server_jni.c $
 * $brcm_Revision: 1 $
 * $brcm_Date: 11/8/11 10:25p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/app/BcmDMSServer/jni/bcm_dms_server_jni.c $
 * 
 * 1   11/8/11 10:25p kagrawal
 * SW7425-1661: Ported DMS to GB
 * 
 * 2   6/27/11 3:13p kagrawal
 * SW7420-1961: Move to the latest DLNA and UPNP codebase
 * 
 * 1   4/29/11 9:50p kagrawal
 * SW7420-1812: Initial support for DMS Android application
 * 
 *****************************************************************************/
#include <jni.h>

#include <sys/types.h>
#include <unistd.h>

#include "bdlna.h"
#include "DMS.h"
#include "trace.h"
#include <getopt.h>
#include <signal.h>

#include <utils/Log.h>

#define DMS_SSDP_REFRESH	1800 /* seconds */
static int dms_scan_interval=0; /*second*/

/* DMS Handle */
BDlnaDmsHandle	DmsHandle=NULL;

/* extern */
#ifdef __cplusplus
extern "C" {
#endif
int host_getipaddress(const char* interface, char *ipAddress, size_t length);
#ifdef __cplusplus
}
#endif


void
DmsUsage()
{
    LOGI("Usage: bcmmserver [-i <interface name>] [-h] \n");
    LOGI("Options are:\n");
    LOGI(" -i <interface name>   # name of the interface to bind to, default eth0\n");
    LOGI(" -h                    # prints this usage\n");
    LOGI("\n");
}

#if 0
void
parseCmdOptions(int argc, char *argv[], BDlna_Dms_OpenSettings *dmsOpt)
{
	char ch;
	while ((ch = getopt(argc, argv, "i:h:")) != -1) {
        switch (ch) {
			case 'i':
				strncpy(dmsOpt->interfaceName, optarg, sizeof(dmsOpt->interfaceName));
				break;
			case 'h':
			default:
				DmsUsage();
				exit(1);
		}
	}
}
#endif

void
parseConfOptions(BDlna_Dms_OpenSettings *dmsOpt, BUPnPOpenSettings *upnpOpt)
{
	char	*token, *val;
	FILE	*fp;
	char	buf[256];
	char	seps[] = ",\t\n\r=";

	dmsOpt->metaDbDir[0] = 0;
    dmsOpt->extMediaDir[0] = 0;
    dmsOpt->metaInfoDir[0] = 0;
    dmsOpt->friendlyName = NULL;
    dmsOpt->legalFileExtension = NULL;
    dmsOpt->enableThumbnailGen = 1;
    dmsOpt->SupportXbox = 1;

    fp = fopen("/system/etc/dms.conf", "r");
	if (fp) {
		while (fgets(buf, 256, fp)) {		
			token = val = NULL;
			if (buf[0] == '#' || buf[0] == '\n' || buf[0] == '\r') {
				continue;
			} else {			
				token = strtok(buf, seps);
				val = strtok(NULL, seps);
				if (!token || !val)
					continue;
				if (!strcmp(token, "uuid")) {
					strncpy(dmsOpt->udn, val, sizeof(dmsOpt->udn));
				} else if (!strcmp(token, "netInterface")) {
					strncpy(dmsOpt->interfaceName, val, sizeof(dmsOpt->interfaceName));
				} else if (!strcmp(token, "wanInterface")) {
					strncpy(dmsOpt->wanInterfaceName, val, sizeof(dmsOpt->wanInterfaceName));
				} else if (!strcmp(token, "mediaDir")) {
					strncpy(dmsOpt->contentDir, val, sizeof(dmsOpt->contentDir));
				} else if (!strcmp(token, "extMediaDir")) {
					strncpy(dmsOpt->extMediaDir, val, sizeof(dmsOpt->extMediaDir));
				} else if (!strcmp(token, "metaDbDir")) {
					if ( strlen(val) < sizeof(dmsOpt->metaDbDir) )
						strcpy(dmsOpt->metaDbDir, val);
					else{
						strncpy(dmsOpt->metaDbDir, val, sizeof(dmsOpt->metaDbDir));
						dmsOpt->metaDbDir[sizeof(dmsOpt->metaDbDir)] = 0;
						}
				} else if (!strcmp(token, "metaInfoDir")) {
					if ( strlen(val) < sizeof(dmsOpt->metaInfoDir) )
						strcpy(dmsOpt->metaInfoDir, val);
					else {
						strncpy(dmsOpt->metaInfoDir, val, sizeof(dmsOpt->metaInfoDir));
						dmsOpt->metaInfoDir[sizeof(dmsOpt->metaDbDir)] = 0;
						}
				} else if (!strcmp(token, "recordedDir")) {
					strncpy(dmsOpt->recordDir, val, sizeof(dmsOpt->recordDir));
					if(sizeof(val) > sizeof(dmsOpt->recordDir)) {
						LOGI("truncated recordDir path -- error\n");
					}
				} else if (!strcmp(token, "ssdpRefresh")) {
					upnpOpt->ssdpRefreshPeriod = atoi(val);
				} else if (!strcmp(token,"DTCPprotected")) {
					dmsOpt->DTCPprotected = atoi(val);
				} else if (!strcmp(token,"friendlyName")) {	
					dmsOpt->friendlyName = (char *) malloc(strlen(val)+1);
					if ( dmsOpt->friendlyName )
						strcpy(dmsOpt->friendlyName, val);
					else
						perror("malloc(friendlyName)");
				}else if(!strcmp(token, "ScanInterval")) {
					dms_scan_interval = atoi(val);
				}else if(!strcmp(token, "SupportXbox")) {
					dmsOpt->SupportXbox = atoi(val);
				}else if(!strcmp(token, "fileExtension")) {
                                        LOGI("parseConfOptions(): fileExtension=%s\n", val);
                                        dmsOpt->legalFileExtension = (char *) malloc(strlen(val)+1);
                                        if ( dmsOpt->legalFileExtension )
                                                strcpy(dmsOpt->legalFileExtension, val);
                                        else
                                                LOGE("malloc(fileExtension)");
                                }else if(!strcmp(token, "disableThumbnailGen")) {
                                        LOGI("parseConfOptions(): disableThumbnailGen=%s\n", val);
                                        if ( !strcmp(val,"y") ){
                                                dmsOpt->enableThumbnailGen = 0;
                                                LOGI("parseConfOptions(): Disable thumbnail icon generation\n");
                                                }
                                        else {
                                                dmsOpt->enableThumbnailGen = 1;
                                                LOGI("parseConfOptions(): Enable thumbnail icon generation\n");
                                                }
				
				

				
				}else {
					LOGI("Unknown token=%s", token);
				}
			}
		}
		fclose(fp);
	} else {
		LOGE("\ndms.conf file does not exist!!\n");
		return;
	}
 /* make sure that mediaDir is set in dms.conf */
 if ( (strlen(dmsOpt->contentDir) < 2) &&  (strlen(dmsOpt->extMediaDir) < 2) ){
	LOGE("dms.conf: mediaDir or extMediaDir must be set in dms.conf !!!\n");
	exit(1);
	}
 /* make sure extMediaDir is started with "/mnt" */
 if ( strlen(dmsOpt->extMediaDir) > 0 ){ 
	if ( strncmp(dmsOpt->extMediaDir, "/mnt/", 5) ){
		LOGE("dms.conf: extMediaDir must be set to started with /mnt/ in dms.conf!!!\n");
		exit(1);
		}
	}
 /* make sure that "mediaDir" and "recordedDir" are not overlapped in dms.conf */
 if ( (strlen(dmsOpt->recordDir) > 1) && (strlen(dmsOpt->contentDir) > 1) ){ 
	if(!strncmp(dmsOpt->contentDir,dmsOpt->recordDir,(strlen(dmsOpt->contentDir)<strlen(dmsOpt->recordDir)?strlen(dmsOpt->contentDir):strlen(dmsOpt->recordDir)))) {
		LOGE("dms.conf: mediaDir=%s or recordedDir=%s should not be contained by another !!!\n",dmsOpt->contentDir,dmsOpt->recordDir);
		exit(1);
		}
	}
  /* make sure that "mediaDir" and "extMediaDir" are not overlapped in dms.conf */
  if ( (strlen(dmsOpt->extMediaDir) > 1) && (strlen(dmsOpt->contentDir) > 1) ){ 
	if(!strncmp(dmsOpt->contentDir,dmsOpt->extMediaDir,(strlen(dmsOpt->contentDir)<strlen(dmsOpt->extMediaDir)?strlen(dmsOpt->contentDir):strlen(dmsOpt->extMediaDir)))) {
		LOGE("dms.conf: mediaDir=%s or extMediaDir=%s should not be contained by another !!!\n",dmsOpt->contentDir,dmsOpt->extMediaDir);
		exit(1);
		}
	}

  /* make sure that "mediaDir" and "metaInfoDir" are not overlapped in dms.conf */
  if ( (strlen(dmsOpt->metaInfoDir) > 1) && (strlen(dmsOpt->contentDir) > 1) ){ 
	if(!strncmp(dmsOpt->contentDir,dmsOpt->metaInfoDir,(strlen(dmsOpt->contentDir)<strlen(dmsOpt->metaInfoDir)?strlen(dmsOpt->contentDir):strlen(dmsOpt->metaInfoDir)))) {
		LOGE("dms.conf: mediaDir=%s or metaInfoDir=%s should not be contained by another !!!\n",dmsOpt->contentDir,dmsOpt->metaInfoDir);
		exit(1);
		}
	}
 /* make sure that "metaInfoDir" and "extMediaDir" are not overlapped in dms.conf */
  if ( (strlen(dmsOpt->extMediaDir) > 1) && (strlen(dmsOpt->metaInfoDir) >1) ){ 
	if(!strncmp(dmsOpt->contentDir,dmsOpt->extMediaDir,(strlen(dmsOpt->metaInfoDir)<strlen(dmsOpt->extMediaDir)?strlen(dmsOpt->metaInfoDir):strlen(dmsOpt->extMediaDir)))) {
		LOGE("dms.conf: metaInfoDir=%s or extMediaDir=%s should not be contained by another !!!\n",dmsOpt->metaInfoDir,dmsOpt->extMediaDir);
		exit(1);
		}
	}
}

static void
dmsShutdown()
{
	if (DmsHandle) {
		LOGI("DMS is stopping...\n");
		BUPnP_Stop();
		LOGI("DMS has stopped...\n");	
		BDlna_Dms_Destroy(DmsHandle);
		LOGI("DMS shutdown complete...\n");
	}
}

static void
siginit_handler(int sig)
{
	LOGI("signal handler called\n");
	dmsShutdown();
	signal(SIGINT, SIG_DFL);
	exit(0);
}

#define DMS_RESCAN_DIR	"/mnt/usb"
static int dms_rescan=0;
static int dms_started=0;
static void sigusr1_handler(int sig){
 LOGI("sigusr1_handler(): Catch signal(SIGUSR1)\n");

 dms_rescan = 1;
}

#if 0
int main(int argc, char *argv[])
#else
jint
Java_com_broadcom_bcmdmsserver_BcmDMSService_startDMS( JNIEnv* env,
                                                  jobject thiz )
#endif
{
	char option;
	char ContentDir[256];
	char ipAddr[31], wanAddr[31];
	BUPnPOpenSettings			UPnPOpenSettings;
	BDlna_Dms_OpenSettings		DmsOpenSettings;	
	BUPnPError					ret;

#ifdef WIN32
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD( 2, 2 );
	WSAStartup( wVersionRequested, &wsaData );
#endif

	LOGI("Digital Media Server Version [%d.%d Build %s %s]\n", DMS_MAJOR_VER, DMS_MINOR_VER, DMS_BUILD_DATE, DMS_BUILD_TIME);
	memset(&DmsOpenSettings,0,sizeof(DmsOpenSettings));

	/* Read the media directory and interface from dms.conf */
	parseConfOptions(&DmsOpenSettings, &UPnPOpenSettings);

#if 0
	/* Next parse the cmdline input */
	parseCmdOptions(argc, argv, &DmsOpenSettings);
#endif

	/* Interface is not set at the cmdline or the dms.conf file, try eth0 */
	if (!DmsOpenSettings.interfaceName[0]) {
		strcpy(DmsOpenSettings.interfaceName, "eth0");
	}

	/* Wan Interface is not set in the dms.conf file, try eth1 */
	if (!DmsOpenSettings.interfaceName[0]) {
		strcpy(DmsOpenSettings.interfaceName, "eth1");
	}
	LOGI("uuid: %s\nmediaDir: %s\nlanInterface: %s\nwanInterface: %s\n", DmsOpenSettings.udn, DmsOpenSettings.contentDir, 
		DmsOpenSettings.interfaceName, DmsOpenSettings.wanInterfaceName);

	host_getipaddress(DmsOpenSettings.wanInterfaceName, wanAddr, 31);
	LOGI("WAN ipAddr: %s\n", wanAddr);
	host_getipaddress(DmsOpenSettings.interfaceName, ipAddr, 31);
	LOGI("LAN ipAddr: %s\n", ipAddr);
	if (!ipAddr[0]) {
		LOGE("IP addr is NULL, quitting DMS server!!\n");
		exit(1);
	}

    /* Don't need this for Android */
#ifndef ANDROID
	/* Setup ^C signal */
	signal(SIGINT, siginit_handler);
	signal(SIGUSR1, sigusr1_handler);
#endif

	DmsOpenSettings.ContentXferPort = CONTENT_XFER_PORT;	/*This the content transfer port for server*/
	UPnPOpenSettings.ipAddress = ipAddr;					/*This is the IP address that we run on*/
	UPnPOpenSettings.portNumber = 2468;						/*This is the media server port number*/
	UPnPOpenSettings.networkInterface = DmsOpenSettings.interfaceName;
	if (UPnPOpenSettings.ssdpRefreshPeriod < 5 || UPnPOpenSettings.ssdpRefreshPeriod > 5000)	
		UPnPOpenSettings.ssdpRefreshPeriod = DMS_SSDP_REFRESH;

	/*Initialize the UPnP library*/	
	ret = BUPnP_Initialize(&UPnPOpenSettings);
	if(UPNP_E_SUCCESS != ret) {
		LOGE("Failed to Initialize the upnp framework\n");
		return 1; 
	}

	DmsHandle = BDlna_Dms_Create(&DmsOpenSettings);
	if (!DmsHandle) {
		LOGE("Failed to Create DMS\n");
		return 1; 
	}

	BUPnP_Start();
	LOGI("DMS is ready...my pid is: %d\n", getpid());

    dms_started = 1;

    /* Don't need this for Android */
#ifndef ANDROID
	/*Press ^C to exit */
	while(1) {
		/* 
		 * Event handler 
		 */
		if ( dms_rescan ){ /* drivern by SIGUSR1 */
			LOGI("To re-scan %s\n", DMS_RESCAN_DIR);
			BDlna_Dms_ReScan((char *)DMS_RESCAN_DIR);
			dms_rescan = 0;
			}
			
		/* 
	  	 * Periodic routines 
	  	 */	 
#if 0
	  	BDlna_Dms_UpdateMeta();
#endif		
		sleep(1);
	}

	dmsShutdown();
#endif /* ANDROID */

	return 0;
}

jint
Java_com_broadcom_bcmdmsserver_BcmDMSService_stopDMS( JNIEnv* env,
                                                  jobject thiz )
{
	LOGI("stopDMS()\n");

    /* TODO: This needs fixing */
    dmsShutdown();
    dms_started = 0;
    
    return 0;
}

jint
Java_com_broadcom_bcmdmsserver_BcmDMSService_isDMSStarted( JNIEnv* env,
                                                  jobject thiz )
{
	LOGI("isDMSStarted() dms_started = %d\n",dms_started);
    return dms_started;    
}

