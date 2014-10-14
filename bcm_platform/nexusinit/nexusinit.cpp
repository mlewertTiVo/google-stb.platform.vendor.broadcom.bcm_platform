/******************************************************************************
 *    (c)2011-2014 Broadcom Corporation
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

#ifdef NEXUS_MODE_proxy
#define NEXUS_DRIVER_FILENAME          "/system/etc/nexus.ko"
#else
#define NEXUS_DRIVER_FILENAME          "/system/etc/bcmdriver.ko"
#endif
#define NEXUS_IR_INPUT_DRIVER_FILENAME "/system/etc/nexus_ir_input_event.ko"
#define EVENT_FORWARD_DRIVER_FILENAME  "/system/etc/event_forward.ko"
#define EVENT_WRITE_DRIVER_FILENAME    "/system/etc/event_write.ko"
#define NXSERVER_FILENAME              "/system/bin/nxserver"
#define NX_ASHMEM_DRIVER_FILENAME      "/system/etc/nx_ashmem.ko"

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

    if (fstat(fd, &sbuf) < 0) {
        close(fd);
        return NULL;
    }

    /* 0 (zero) (or negative - off_t is signed long) size doesn't make sense */
    if (0 >= sbuf.st_size) {
        close(fd);
        return NULL;
    }
    bytes2Read = (size_t) sbuf.st_size;

    buffer = (unsigned char *) malloc(sbuf.st_size);
    if (!buffer) {
        close(fd);
        return NULL;
    }

    p = buffer;
    while ( 0 < (bytesRead = read(fd, p, bytes2Read)) ) {
        p += bytesRead;
        if (bytesRead > bytes2Read) break; /* should not happen */
        bytes2Read -= bytesRead;
    }
    /* release resouce as soon as possible if it is not needed anymore */
    close(fd);

    /* 0: end-of-file, -1: error on read(), and bytes2Read should be 0 (zero) */
    if (0 > bytesRead || 0 < bytes2Read) {
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
    if (!file) {
        LOGE("nexusinit: nexusinit_grab_file returned NULL!");
        return BERR_UNKNOWN;
    }

    ret = init_module(file, len, args);
    free(file);
    if (ret != 0) {
         LOGE("nexusinit: init_module returned %ld!",ret);
         return BERR_UNKNOWN;
    }

    return BERR_SUCCESS;
}

BERR_Code nexusinit_rmmod(const char *name)
{
    long int ret;

    ret = delete_module(name,0,0,0);
    if (ret != 0) {
        return BERR_UNKNOWN;
    }

    return BERR_SUCCESS;
}

BERR_Code nexusinit_ir()
{
#ifdef NEXUS_MODE_proxy
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
        if (nexusinit_insmod(NEXUS_IR_INPUT_DRIVER_FILENAME, (strcmp(value,"remote_a")==0) ? "remote_a=1" : "") != BERR_SUCCESS) {
            LOGE("nexusinit: insmod failed on %s!",NEXUS_IR_INPUT_DRIVER_FILENAME);
            return BERR_UNKNOWN;
        }
    }
#endif

    /* Add the nx_ashmem module which is required for gralloc to function */
    if (nexusinit_insmod(NX_ASHMEM_DRIVER_FILENAME, "") != BERR_SUCCESS) {
        LOGE("nexusinit: insmod failed on %s!", NX_ASHMEM_DRIVER_FILENAME);
        return BERR_UNKNOWN;
    }

    return BERR_SUCCESS;
}

void startNxServer(void)
{
    char cmdRunNxServer[256];
    char value[PROPERTY_VALUE_MAX];
    /* Start the NxServer after loading the driver. Make sure
       to let the server know that we're handling the IR driver */
    LOGI("nexusinit: Launching NxServer with clients in unprotected mode (runs in the background so that we get control back");
    /* Turn off transcode for stand-alone Android to enable MVC/SVC playback and disable
       IR if we are not wanting to run NxClient apps... */

    strcpy(cmdRunNxServer, NXSERVER_FILENAME);
    strcat(cmdRunNxServer, " ");

    property_get("ro.ir_remote", value, NULL);
    if (strcmp(value, "nxclient") != 0) {
        strcat(cmdRunNxServer, "-ir none ");
    }
    /* If the HD output resolution is set to 1080p, then we can use the HDMI preferred output format
       (typically 1080p also) and not ignore the HDMI EDID information from the connected TV. */
    if (property_get("ro.hd_output_format", value, NULL)) {
        if (strncmp((char *) value, "1080p", 5) != 0) {
            strcat(cmdRunNxServer, "-ignore_edid ");
        }
    }
#ifndef BCM_OMX_SUPPORT_ENCODER
    strcat(cmdRunNxServer, "-transcode off ");
#endif
#if !ANDROID_SUPPORTS_FRONTEND_SERVICE
    strcat(cmdRunNxServer, "-frontend off ");
#endif
    strcat(cmdRunNxServer, "&");
    ALOGI("NXSERVER CMD: %s",cmdRunNxServer);
    system(cmdRunNxServer);
}

int main(void)
{
    char value[PROPERTY_VALUE_MAX];
    char value2[2 * PROPERTY_VALUE_MAX] = { 0 };
    NEXUS_Error rc;
    NEXUS_PlatformSettings platformSettings;
    NEXUS_PlatformStartServerSettings serverSettings;
    NEXUS_PlatformConfiguration platformConfig;

    if (daemon(0, 0) < 0) {
        LOGE("nexusinit: FATAL: Daemonise Failed!");
        _exit( 1 );
    }

    android::ProcessState::self()->startThreadPool();

#if ((ATP_BUILD==0) && (ANDROID_UNDER_LXC==0))
#ifdef NEXUS_MODE_proxy
    /* Pull the nexus arguments and pass them to the module init */
    property_get("ro.nexus_config", value, NULL);

    if (strlen(value) > 0) {
        strcat(value2, "config=");
        strcat(value2, value);
        LOGD("nexusinit: ro.nexus_config = %s", value2);
    }
#else
    mode_t old_umask = umask(0000);
    mknod("/dev/brcm0", 0666 | S_IFCHR, makedev(30, 0));
    umask(old_umask);
#endif

    property_get("ro.nexus.devname", value, NULL);
    if (strlen(value) > 0) {
        if (strlen(value2) > 0) {
           strcat(value2, " ");
        }
        strcat(value2, "devname=\"");
        strcat(value2, value);
        strcat(value2, "\"");
        LOGD("nexusinit: ro.nexus.devname=%s", value2);
    }

    /* insmod nexus.ko driver first */
    if(nexusinit_insmod(NEXUS_DRIVER_FILENAME, value2) != BERR_SUCCESS) {
        LOGE("nexusinit: FATAL: insmod failed on %s!",NEXUS_DRIVER_FILENAME);
        _exit(1);
    }
    else {
        LOGI("nexusinit: insmod %s succeeded", NEXUS_DRIVER_FILENAME);
    }

    /* Are we using our own nxserver integrated in to nexus service or not? */
#ifdef ANDROID_SUPPORTS_NXCLIENT
    struct stat buffer;

    if (stat(NXSERVER_FILENAME, &buffer) == 0) {

        startNxServer();

        /* Make sure we wait a bit before installing the IR input driver */
        sleep(3);

        property_set("hw.nexus.platforminit", "on");

        if (nexusinit_ir() != BERR_SUCCESS) {
            LOGE("nexusinit: FATAL: Could not initialise IR!");
            _exit(1);
        }
    } else {
        LOGE("nexusinit: FATAL: Could not find \"" NXSERVER_FILENAME "\"!");
        _exit(1);
    }

#else /* ANDROID_SUPPORTS_NXCLIENT */
    NEXUS_Platform_GetDefaultSettings(&platformSettings);

    platformSettings.mode = NEXUS_ClientMode_eVerified;
#if ANDROID_SUPPORTS_FRONTEND_SERVICE
    platformSettings.openFrontend = true;
#else
    platformSettings.openFrontend = false;
#endif

    while (NEXUS_Platform_Init(&platformSettings) != NEXUS_SUCCESS) {
        LOGE("nexusinit: NEXUS_Platform_Init Failed!");
        sleep(1);
    }

    property_set("hw.nexus.platforminit", "on");

    LOGI("nexusinit: NEXUS_Platform_Init Success");

    if (nexusinit_ir() != BERR_SUCCESS) {
        LOGE("nexusinit: FATAL: Could not initialise IR!");
        _exit(1);
    }

#endif /* ANDROID_SUPPORTS_NXCLIENT */
#else // ATP_BUILD && ANDROID_UNDER_LXC cases

    FILE *f_list;
    char line[4096];
    bool bFound = false;

    do
    {
        system("ps &> /tmp/proclist.txt");

        f_list = fopen("/tmp/proclist.txt", "r");

        if (!f_list)
        {
            LOGE("Failed to open /tmp/proclist.txt");
            return BERR_UNKNOWN;
        }

        while (fgets(line, sizeof(line), f_list))
        {
            char *p_service;

            p_service = strstr (line, "platform_service");

            if (p_service)
            {
                bFound = true;
                LOGE("platform_service is now running!!");
                break;
            }

            else
            {
                LOGE("Waiting for platform_service...");
            }
        }

        if (f_list)
            fclose(f_list);
    }while(!bFound);

    // Wait for a brief moment before installing the nx_ashmem driver
    sleep(3);

    /* Add the nx_ashmem module which is required for gralloc to function */
    if (nexusinit_insmod(NX_ASHMEM_DRIVER_FILENAME, "") != BERR_SUCCESS) {
        LOGE("nexusinit: insmod failed on %s!", NX_ASHMEM_DRIVER_FILENAME);
        return BERR_UNKNOWN;
    }

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
    if (nexusinit_insmod(EVENT_FORWARD_DRIVER_FILENAME, "") != BERR_SUCCESS) {
        LOGE("nexusinit: FATAL: insmod failed on %s!", EVENT_FORWARD_DRIVER_FILENAME);
        _exit(1);
    }

    LOGI("nexusinit: insmod succeeded for %s", EVENT_FORWARD_DRIVER_FILENAME);

    if (nexusinit_insmod(EVENT_WRITE_DRIVER_FILENAME, "") != BERR_SUCCESS) {
        LOGE("nexusinit: FATAL: insmod failed on %s!", EVENT_WRITE_DRIVER_FILENAME);
        _exit(1);
    }

    LOGI("nexusinit: insmod succeeded for %s", EVENT_WRITE_DRIVER_FILENAME);
#endif

    android::IPCThreadState::self()->joinThreadPool();
}

