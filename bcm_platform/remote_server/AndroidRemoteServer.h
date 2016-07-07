
#ifndef ANDROID_REMOTE_SERVER_H
#define ANDROID_REMOTE_SERVER_H

#include <cutils/log.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <math.h>

extern "C" {
static void get_boot_time(void);
static void openInputDev(int *fd, char *devName);
static void closeInputDev(int fd);
}

typedef enum{
	TS_UNTOUCHED = 0,
	TS_FIRST_TOUCH,
	TS_FIRST_MOVE,
	TS_MOVING,
	TS_PREPARE_DRAGGING,
	TS_FIRST_DRAGGING,
	TS_DRAGGING,
	TS_SCROLLING
} TOUCH_STATUS;

class AndroidRemoteServer{
public:
	AndroidRemoteServer();
	virtual ~AndroidRemoteServer();
	void handleInput(void* in, size_t len);

	/* Should be called periodically */
	void loop(int loopDelay);

	/* Api to send events to android */
	void SendEventToNexusIrInput(int chr, int press);
	int onMouseClickDown();
	int onMouseClickUp();
	int onTouchStart();
	int onTouchEnd();
	int onMouseMove();
	int onTwoFingers();
	int onScrolling();

private:
	TOUCH_STATUS eTouchStatus;
};

#endif/* ANDROID_REMOTE_SERVER_H */

