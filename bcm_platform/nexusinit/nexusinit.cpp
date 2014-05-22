/******************************************************************************
 *    (c)2011-2012 Broadcom Corporation
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
 * $brcm_Workfile: nexusinit.cpp $
 * $brcm_Revision: 10 $
 * $brcm_Date: 11/29/12 4:34p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/nexusinit/nexusinit.cpp $
 * 
 * 10   11/29/12 4:34p nitinb
 * SWANDROID-265: Use input events forwarded by Trellis in SBS mode
 * 
 * 9   7/9/12 3:50p alexpan
 * SWANDROID-124: Add camera record support for 7435
 * 
 * 8   5/29/12 6:58p ajitabhp
 * SWANDROID-96: Fixed the problem with environment variable
 * 
 * 7   5/7/12 3:45p ajitabhp
 * SWANDROID-96: Initial checkin for android side by side implementation.
 * 
 * 6   3/28/12 6:07p alexpan
 * SWANDROID-46: Add VCE heap to serverSettings for camera record
 * 
 * 5   3/13/12 10:37a ttrammel
 * SWANDROID-38: Add support for Broadcom silver remote (nec protocol).
 * 
 * 4   2/21/12 9:50p alexpan
 * SWANDROID-20: Remove platform dependent settings in
 *  NEXUS_PlatformStartServerSettings
 * 
 * 3   2/8/12 2:52p kagrawal
 * SWANDROID-12: Initial support for Nexus client-server mode
 * 
 * 2   12/28/11 10:40a alexpan
 * SW7425-1908: Update to use latest Nexus Multiprocess server settings
 *  and heap assignment for client
 * 
 * 3   11/3/11 7:14p alexpan
 * SW7425-1660: Update server settings to follow Nexus Multiprocess API
 *  changes in 7425 Phase3.5
 * 
 * 2   8/1/11 2:43p franktcc
 * SW7420-2016: Instanciate nexus service after nexus platform init,
 *  fixing first time boot-up unstable issue.
 * 
 * 1   7/28/11 3:31p franktcc
 * SW7420-2012: Use binder interface for handle sharing
 * 
 * 2   2/11/11 2:41p gayatriv
 * SW7420-1391: Changes to make android work with multiprocess nexus
 * 
 *****************************************************************************/
#include "bstd.h"
#include "berr.h"
#include "nexus_platform.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "bkni.h"
#include "bkni_multi.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/mman.h>
#include <linux/kdev_t.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>

#include "nexusinit.h"

#include "nexusnxservice.h"
#if ANDROID_SUPPORTS_FRONTEND_SERVICE
#include "nexusfrontendservice.h"
#endif

#define NEXUS_PROXY_DRIVER_FILENAME		"/system/etc/nexus.ko"
#define NEXUS_IR_INPUT_DRIVER_FILENAME	"/system/etc/nexus_ir_input_event.ko"
#define EVENT_FORWARD_DRIVER_FILENAME	"/system/etc/event_forward.ko"
#define EVENT_WRITE_DRIVER_FILENAME		"/system/etc/event_write.ko"
#define NXSERVER_FILENAME               "/system/bin/nxserver"

extern "C" long int init_module(void *, unsigned long, const char*);
extern "C" long int delete_module(const char *, unsigned long, unsigned long, unsigned long);

void *nexusinit_grab_file(const char *filename, unsigned long *size)
{
    int fd;
    ssize_t bytes2Read;
    ssize_t bytesRead;
    unsigned char *p, *buffer;
    struct stat sbuf;

    if (NULL == filename || NULL == size) return NULL;

    fd = open(filename, O_RDONLY);
    if (fd < 0) return NULL;

    if (fstat(fd, &sbuf) < 0)
    {
        close(fd);
        return NULL;
    }

    /* 0 (zero) (or negative - off_t is signed long) size doesn't make sense */
    if (0 >= sbuf.st_size)
    {
        close(fd);
        return NULL;
    }
    bytes2Read = (size_t) sbuf.st_size;

    buffer = (unsigned char *) malloc(sbuf.st_size);
    if (!buffer)
    {
        close(fd);
        return NULL;
    }

    p = buffer;
    while ( 0 < (bytesRead = read(fd, p, bytes2Read)) )
    {
        p += bytesRead;
        if (bytesRead > bytes2Read) break; /* should not happen */
        bytes2Read -= bytesRead;
    }
    /* release resouce as soon as possible if it is not needed anymore */
    close(fd);

    /* 0: end-of-file, -1: error on read(), and bytes2Read should be 0 (zero) */
    if (0 > bytesRead || 0 < bytes2Read)
    {
        free(buffer);
        return NULL;
    }

    *size = (unsigned long) sbuf.st_size;
    return buffer;
}

BERR_Code nexusinit_insmod(const char *filename, const char *args)
{
    long int ret;
    unsigned long len;
    void *file;

    file = nexusinit_grab_file(filename, &len);
    if (!file)
    {
        LOGE("nexusinit: nexusinit_grab_file returned NULL!");
        return BERR_UNKNOWN;
    }

    ret = init_module(file, len, args);
    free(file);
    if (ret != 0)
    {
         LOGE("nexusinit: init_module returned %ld!",ret);
         return BERR_UNKNOWN;
    }

    return BERR_SUCCESS;
}

BERR_Code nexusinit_rmmod(const char *name)
{
    long int ret;

    ret = delete_module(name,0,0,0);
    if (ret != 0)
    {
        return BERR_UNKNOWN;
    }

    return BERR_SUCCESS;
}

BERR_Code nexusinit_ir()
{
    char module_arg[16];
    char value[PROPERTY_VALUE_MAX];	

    strncpy(module_arg, "", sizeof(module_arg));

    /* Runtime check for remote type, default to silver remote */
    property_get("ro.ir_remote", value, NULL);

    /* Don't insmod our IR driver, if we want to test the NxClient applications using IR R/C */
#ifdef ANDROID_SUPPORTS_NXCLIENT
    if (strcmp(value, "nxclient") != 0)
#endif
    {
        /* After the platform init, insmod the ir event driver */
        if (nexusinit_insmod(NEXUS_IR_INPUT_DRIVER_FILENAME, (strcmp(value,"remote_a")==0) ? "remote_a=1" : "") != BERR_SUCCESS)
        {
            LOGE("nexusinit: insmod failed on %s!",NEXUS_IR_INPUT_DRIVER_FILENAME);
            return BERR_UNKNOWN;
        }
    }
    return BERR_SUCCESS;
}

int main(void)
{
    char value[PROPERTY_VALUE_MAX];
    char value2[PROPERTY_VALUE_MAX + sizeof("config=")] = { 0 };
    NEXUS_Error rc;
    NEXUS_PlatformSettings platformSettings;
    NEXUS_PlatformStartServerSettings serverSettings;
    NEXUS_PlatformConfiguration platformConfig;

#if 1
    if (daemon(0, 0) < 0)
    {
        LOGE("nexusinit: FATAL: Daemonise Failed!");
        _exit( 1 );
    }
#endif

    android::ProcessState::self()->startThreadPool();

#if ((ATP_BUILD==0) && (ANDROID_UNDER_LXC==0))
    /* Pull the nexus arguments and pass them to the module init */
    property_get("ro.nexus_config", value, NULL);

    if (strlen(value) > 0)
    {
        strcat(value2, "config=");
        strcat(value2, value);
        LOGD("nexusinit: ro.nexus_config = %s", value2);
    }

    /* insmod nexus.ko driver first */
    if(nexusinit_insmod(NEXUS_PROXY_DRIVER_FILENAME, value2) != BERR_SUCCESS)
    {
        LOGE("nexusinit: FATAL: insmod failed on %s!",NEXUS_PROXY_DRIVER_FILENAME);
        _exit(1);
    }
    else
        LOGI("nexusinit: nexus.ko insmod Success");

    /* Are we using our own nxserver integrated in to nexus service or not? */
#ifdef ANDROID_SUPPORTS_NXCLIENT
    struct stat buffer;

    if (stat(NXSERVER_FILENAME, &buffer) == 0) {
        /* Start the NxServer after loading the driver. Make sure
           to let the server know that we're handling the IR driver */
        LOGI("nexusinit: Launching NxServer with clients in unprotected mode (runs in the background so that we get control back");
        /* Turn off transcode for stand-alone Android to enable MVC/SVC playback and disable
           IR if we are not wanting to run NxClient apps... */
        property_get("ro.ir_remote", value, NULL);
        if (strcmp(value, "nxclient") != 0) {
            system(NXSERVER_FILENAME " -ir none -ignore_edid -transcode off &");
        } else {
            system(NXSERVER_FILENAME " -ignore_edid -transcode off &");
        }

        /* Make sure we wait a bit before installing the IR input driver */
        sleep(3);

        property_set("hw.nexus.platforminit", "on");

        if (nexusinit_ir() != BERR_SUCCESS)
        {
            LOGE("nexusinit: FATAL: Could not initialise IR!");
            _exit(1);
        }
    } else {
        LOGE("nexusinit: FATAL: Could not find \"" NXSERVER_FILENAME "\"!");
        _exit(1);
    }

#else /* ANDROID_SUPPORTS_NXCLIENT */
    NEXUS_Platform_GetDefaultSettings(&platformSettings);

#if ANDROID_SUPPORTS_FRONTEND_SERVICE
    platformSettings.openFrontend = true;
#endif

    while (NEXUS_Platform_Init(&platformSettings) != NEXUS_SUCCESS)
    {
        LOGE("nexusinit: NEXUS_Platform_Init Failed!");
        sleep(1);
    }

    property_set("hw.nexus.platforminit", "on");

    LOGI("nexusinit: NEXUS_Platform_Init Success");

    if (nexusinit_ir() != BERR_SUCCESS)
    {
        LOGE("nexusinit: FATAL: Could not initialise IR!");
        _exit(1);
    }

#endif /* ANDROID_SUPPORTS_NXCLIENT */
#else // ATP_BUILD && ANDROID_UNDER_LXC cases
	
	// Just set this property to unhook Android
	property_set("hw.nexus.platforminit", "on");
#endif

#ifdef ANDROID_SUPPORTS_NXCLIENT
    NexusNxService::instantiate();
    LOGI("nexusinit: NexusNxService is now ready");
#else
    NexusService::instantiate();
    LOGI("nexusinit: NexusService is now ready");
#endif

#if ANDROID_SUPPORTS_FRONTEND_SERVICE
    NexusFrontendService::instantiate();
    LOGI("nexusinit: NexusfrontendService is now ready");
#endif

#ifdef SBS_USES_TRELLIS_INPUT_EVENTS
	// Frist, create the device node
	system("busybox mknod /dev/event_write c 34 0");
	system("busybox chmod 666 /dev/event_write");

	// Now, install the drivers
    if (nexusinit_insmod(EVENT_FORWARD_DRIVER_FILENAME, "") != BERR_SUCCESS)
    {
        LOGE("nexusinit: FATAL: insmod failed on %s!", EVENT_FORWARD_DRIVER_FILENAME);
        _exit(1);
    }

    LOGI("nexusinit: insmod succeeded for %s", EVENT_FORWARD_DRIVER_FILENAME);

    if (nexusinit_insmod(EVENT_WRITE_DRIVER_FILENAME, "") != BERR_SUCCESS)
    {
        LOGE("nexusinit: FATAL: insmod failed on %s!", EVENT_WRITE_DRIVER_FILENAME);
        _exit(1);
    }

    LOGI("nexusinit: insmod succeeded for %s", EVENT_WRITE_DRIVER_FILENAME);
#endif

    android::IPCThreadState::self()->joinThreadPool();
}

