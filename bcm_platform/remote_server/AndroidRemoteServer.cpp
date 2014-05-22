/******************************************************************************
 *    (c)2011-2013 Broadcom Corporation
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
 * $brcm_Workfile: AndroidRemoteServer.cpp $
 * $brcm_Revision: 2 $
 * $brcm_Date: 7/11/12 8:56a $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 */

//#define LOG_NDEBUG 0

#define LOG_TAG "AndroidRemoteServer"

#define INPUTDIR "/dev/input"
#define MOUSE_EV_NODE "/dev/input/mouse0"
#define MAX_KEY_LINE_LEN 100
#define SECOND_TAP_TIMEOUT 300 /* milli second */

#include "AndroidRemoteServer.h"
#include "RemoteServer.h"


static int mouse_ev_fd = -1;
static int nexusIr_fd = -1;
static float boottime;
static int lastX = 0, curX = 0;
static int lastY = 0, curY = 0;
static int lastScrollY = 0, curScrollY = 0;
static struct input_event ev_null;
static int iSecondTapTimeout = 0;
static bool bMouseScrolling = false;

int main(void)
{
	android::ProcessState::self()->startThreadPool();

	struct input_event ev;

	RemoteServer::Instance().Startup();

	android::IPCThreadState::self()->joinThreadPool();
}


AndroidRemoteServer::AndroidRemoteServer(){
	openInputDev(&mouse_ev_fd, "Mouse");
	openInputDev(&nexusIr_fd, "nexus ir input");
	memset(&ev_null, 0, sizeof(struct input_event));
	eTouchStatus = TS_UNTOUCHED;
	ALOGD( "AndroidRemoteServer Constructor...");
}

AndroidRemoteServer::~AndroidRemoteServer(){
	closeInputDev(nexusIr_fd);
	closeInputDev(mouse_ev_fd);
	ALOGD( "AndroidRemoteServer Destructor...");
}

/* This function should be called periodically by the server thread, loopDelay is the estimated thread delay for a loop */
void AndroidRemoteServer::loop(int loopDelayMilli){
	if(eTouchStatus == TS_MOVING || eTouchStatus == TS_DRAGGING){
		onMouseMove();
	}else if(eTouchStatus == TS_SCROLLING){
		onScrolling();
	}
	
	/* timeout control for double tap */
	if(TS_PREPARE_DRAGGING == eTouchStatus){
		iSecondTapTimeout -= loopDelayMilli;
		if(iSecondTapTimeout < loopDelayMilli){
			eTouchStatus = TS_UNTOUCHED;				
			ALOGD("eTouchStatus = TS_UNTOUCHED(timeout from preparing dragging)");
		}
	}

}

void AndroidRemoteServer::handleInput(void* in, size_t len){

	int chr = -1;
	char * payload = (char *) in, *temp;
	int iOffset;

	ALOGV( "Message recieved: %s", payload);

	if(strncmp(payload, "onMouseMove", strlen("onMouseMove")) == 0){
		temp = strtok(payload, ",");
		temp = strtok(NULL, ",");
		lastX = atoi(temp);
		temp = strtok(NULL, ",");
		lastY = atoi(temp);
		ALOGV("Mouse X = %d", lastX);
		ALOGV("Mouse Y = %d", lastY);

		/* Touch State Control */
		if(TS_FIRST_TOUCH == eTouchStatus || TS_FIRST_DRAGGING == eTouchStatus){
			if(curX != lastX || curY != lastY){
				curX = lastX;
				curY = lastY;
				ALOGD("Sync Coordinate of X and Y...");
				if(TS_FIRST_TOUCH == eTouchStatus){
					eTouchStatus = TS_MOVING;
					ALOGD("eTouchStatus = TS_MOVING");
				}
				else if(TS_FIRST_DRAGGING == eTouchStatus){
					eTouchStatus = TS_DRAGGING;
					ALOGD("eTouchStatus = TS_DRAGGING");
				}
			}
		}		
	}
	else if(strncmp(payload, "onScrolling", strlen("onScrolling")) == 0){
		iOffset = strlen("onScrolling")+1;
		lastScrollY = atoi(&payload[iOffset]);
		ALOGV("Scrolling Y = %d", lastScrollY);

		if(TS_SCROLLING != eTouchStatus){
			curScrollY = lastScrollY;
			ALOGD("Sync Scroll Y...");
			eTouchStatus = TS_SCROLLING;
		}
	}	
	else if(strncmp(payload, "onMouseClickDown", strlen("onMouseClickDown")) == 0){
		onMouseClickDown();
	}
	else if(strncmp(payload, "onMouseClickUp", strlen("onMouseClickUp")) == 0){
		onMouseClickUp();
	}
	else if(strncmp(payload, "onTouchStart", strlen("onTouchStart")) == 0){
		onTouchStart();
	}
	else if(strncmp(payload, "onTouchEnd", strlen("onTouchEnd")) == 0){
		onTouchEnd();
	}
	else if(strncmp(payload, "onTwoFingers", strlen("onTwoFingers")) == 0){
		onTwoFingers();
	}	
	else{
		if(strncmp(payload, "onKeyBack", strlen("onKeyBack")) == 0)
			chr = KEY_ESC;
		if(strncmp(payload, "onKeyMenu", strlen("onKeyMenu")) == 0)
			chr = KEY_MENU;

		if(chr != -1){
			SendEventToNexusIrInput(chr, 1);
			SendEventToNexusIrInput(chr, 0);
		}else{
			ALOGW("%s Received unimplemented command: %s", __FUNCTION__, payload);
		}
	}

}

void AndroidRemoteServer::SendEventToNexusIrInput(int chr, int press)
{
	uint32_t evtime_sec,evtime_usec;
	uint16_t one = 1;
	char event[16];
	float now;

    if ( nexusIr_fd < 0 ){
		ALOGE("%s Nexus IR fd is not valid...retry", __FUNCTION__);
		openInputDev(&nexusIr_fd, "nexus ir input");
        return;
    }	

	get_boot_time();		

	now=time(NULL)-boottime;
	evtime_sec=now;
	evtime_usec=(now-evtime_sec)*1e9;

	memcpy(event+ 0, &evtime_sec,  4);
	memcpy(event+ 4, &evtime_usec, 4);
	memcpy(event+ 8, &one,         2); /* type == Keyboard event */
	memcpy(event+10, &chr,         2);
	memcpy(event+12, &press,       4);

	if(write(nexusIr_fd,event,16) < 0)
		ALOGE("%s Error! write: %s\n", __FUNCTION__, strerror(errno));
}


int AndroidRemoteServer::onMouseClickDown(void) {
	struct input_event ev;
	ALOGD("%s", __FUNCTION__);
	
    if ( mouse_ev_fd < 0 ){
		ALOGE("%s mouse fd is not valid...retry", __FUNCTION__);
		openInputDev(&mouse_ev_fd, "Mouse");
        return -1;
    }	

	ev.type = EV_KEY;
	ev.code = BTN_LEFT;
	ev.value = 1;
	if(write(mouse_ev_fd, &ev, sizeof(struct input_event)) < 0)	
		ALOGE("%s Error! write: %s\n", __FUNCTION__, strerror(errno));
	if(write(mouse_ev_fd, &ev_null, sizeof(struct input_event)) < 0)
		ALOGE("%s Error! write: %s\n", __FUNCTION__, strerror(errno));

    return 0;
}

int AndroidRemoteServer::onMouseClickUp(void) {
	struct input_event ev;
	ALOGD("%s", __FUNCTION__);
	
    if ( mouse_ev_fd < 0 ){
		ALOGE("%s mouse fd is not valid...retry", __FUNCTION__);
		openInputDev(&mouse_ev_fd, "Mouse");
        return -1;
    }	

	ev.type = EV_KEY;
	ev.code = BTN_LEFT;
	ev.value = 0;
	if(write(mouse_ev_fd, &ev, sizeof(struct input_event)) < 0)		
		ALOGE("%s Error! write: %s\n", __FUNCTION__, strerror(errno));

	if(write(mouse_ev_fd, &ev_null, sizeof(struct input_event)) < 0)
		ALOGE("%s Error! write: %s\n", __FUNCTION__, strerror(errno));
	
    return 0;
}

int AndroidRemoteServer::onScrolling(void){
	struct input_event ev;
	ALOGD("%s", __FUNCTION__);
	
    if ( mouse_ev_fd < 0 ){
		ALOGE("%s mouse fd is not valid...retry", __FUNCTION__);
		openInputDev(&mouse_ev_fd, "Mouse");
        return -1;
    }	

	if(curScrollY == lastScrollY) return 0;

	ev.type = EV_REL;
	ev.code = REL_WHEEL;

	ev.value = (curScrollY - lastScrollY)/2;

	curScrollY = lastScrollY;
	
	if(write(mouse_ev_fd, &ev, sizeof(struct input_event)) < 0)
		ALOGE("%s Error! write: %s\n", __FUNCTION__, strerror(errno));
	
	if(write(mouse_ev_fd, &ev_null, sizeof(struct input_event)) < 0)
		ALOGE("%s Error! write: %s\n", __FUNCTION__, strerror(errno));
	
    return 0;	
}


int AndroidRemoteServer::onTwoFingers(void){
	ALOGV("%s", __FUNCTION__);
	if(!bMouseScrolling){
		onMouseClickDown();
		eTouchStatus = TS_FIRST_DRAGGING;
		ALOGD("eTouchStatus = TS_DRAGGING(onMouseClickDown)");
		bMouseScrolling = true;
	}
	return 0;
}



int AndroidRemoteServer::onTouchStart(void){
	ALOGD("%s", __FUNCTION__);

	if(TS_PREPARE_DRAGGING == eTouchStatus){
		/* Second Tap Start*/
		onMouseClickDown();
		eTouchStatus = TS_FIRST_DRAGGING;
		ALOGD("eTouchStatus = TS_DRAGGING(onMouseClickDown)");
	}
	else{
		/* First Tap Start*/
		eTouchStatus = TS_FIRST_TOUCH;
		ALOGD("eTouchStatus = TS_FIRST_TOUCH");
	}
	
	return 0;
}

int AndroidRemoteServer::onTouchEnd(void){
	ALOGD("%s", __FUNCTION__);

	if(TS_FIRST_TOUCH == eTouchStatus){
		/* First Tap End */
		eTouchStatus = TS_PREPARE_DRAGGING;
		ALOGD("eTouchStatus = TS_PREPARE_DRAGGING");
		iSecondTapTimeout = SECOND_TAP_TIMEOUT;
	}
	else if(TS_DRAGGING == eTouchStatus || TS_FIRST_DRAGGING == eTouchStatus){
		/* Second Tap End*/
		eTouchStatus = TS_UNTOUCHED;
		onMouseClickUp();
		ALOGD("eTouchStatus = TS_UNTOUCHED(onMouseClickUp)");
	}
	else{
		eTouchStatus = TS_UNTOUCHED;
		ALOGD("eTouchStatus = TS_UNTOUCHED");
	}

	/* reset status */
	bMouseScrolling = false;
	
	return 0;
}


int AndroidRemoteServer::onMouseMove(void) {
	struct input_event ev;
	
    if ( mouse_ev_fd < 0 ){
		ALOGE("%s mouse fd is not valid...retry", __FUNCTION__);
		openInputDev(&mouse_ev_fd, "Mouse");
        return -1;
    }	

	ev.type = EV_REL;
	if(curX != lastX){
		ev.code = REL_X;
		ev.value = 2*(lastX - curX);
		curX = lastX;
		
		if(write(mouse_ev_fd, &ev, sizeof(struct input_event)) < 0)
			ALOGE("%s Error! write: %s\n", __FUNCTION__, strerror(errno));
		
		if(write(mouse_ev_fd, &ev_null, sizeof(struct input_event)) < 0)
			ALOGE("%s Error! write: %s\n", __FUNCTION__, strerror(errno));
		
		ALOGV("Tracking X for %d, curX=%d", ev.value, curX);
	}

	if(curY != lastY){
		ev.code = REL_Y;
		ev.value = 2*(lastY - curY);
		curY = lastY;
		
		if(write(mouse_ev_fd, &ev, sizeof(struct input_event)) < 0)
			ALOGE("%s Error! write: %s\n", __FUNCTION__, strerror(errno));
		
		if(write(mouse_ev_fd, &ev_null, sizeof(struct input_event)) < 0)
			ALOGE("%s Error! write: %s\n", __FUNCTION__, strerror(errno));
		
		ALOGV("Tracking Y for %d, curY=%d", ev.value, curY);
	}
	
    return 0;
}


extern "C" {
static void get_boot_time(){
	FILE * boot;
	char line[MAX_KEY_LINE_LEN];
	float uptime;

	boot=fopen("/proc/uptime","r");
  if (boot == NULL)
  {
    ALOGE("Error opening /proc/uptime: %s", strerror(errno));
    return;
  }

  fgets(line,MAX_KEY_LINE_LEN,boot);
	uptime=atof(line);
	ALOGI("Got uptime: %f\n",uptime);
	boottime=time((time_t*)NULL)-uptime;

	fclose(boot);
}

static void closeInputDev(int fd){
	if(fd) close(fd);
}

static void openInputDev(int *fd, char *devName)
{
	DIR * inp;
	struct dirent * dir;
	char dev_name[MAX_KEY_LINE_LEN];
	int tmpFd;
	
	ALOGI("Finding input dev...");

	inp=opendir(INPUTDIR);
	if(inp == NULL)
  {
		ALOGE("Can't open " INPUTDIR "\n");
	};
	while( (dir=readdir(inp)) != NULL)
  {
		if(dir->d_name[0]=='.')
			continue;

		snprintf(dev_name,MAX_KEY_LINE_LEN,"%s/%s",INPUTDIR,dir->d_name);

		ALOGI("Checking input: %s\n",dev_name);

		if ((tmpFd = open(dev_name, O_RDWR)) < 0) 
    {
			ALOGE("open('%s'): %s\n", dev_name, strerror(errno));
		}
    else
    {
			if (ioctl(tmpFd, EVIOCGNAME(MAX_KEY_LINE_LEN), dev_name) < 0) 
      {
				ALOGE("Error!EVIOCGNAME: %s\n", strerror(errno));
			}
      else
      {
				ALOGI("Checking device name --- %s\n", dev_name);
				if(strstr(dev_name, devName)!=NULL)
        {
					ALOGI("Matched");
					*fd = tmpFd;
          closedir(inp);
					return;
				}
			}
			closeInputDev(tmpFd);				
		}
	};

	ALOGE("Error!! Can't find a match input name!!\n");
  closedir(inp);
	return;
}

}/* extern "C" */


