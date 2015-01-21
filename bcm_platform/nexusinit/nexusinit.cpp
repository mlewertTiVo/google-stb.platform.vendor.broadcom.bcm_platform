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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/mman.h>
#include <linux/kdev_t.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>

#include "nexusinit.h"
#include "nxclient.h"
#include "nexusnxservice.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "NexusInit"

#define VENDOR_DRIVER_PATH             "system/vendor/drivers"
#define NEXUS_TRUSTED_DATA_PATH        "/data/misc/nexus"

#define WAKEUP_DRIVER_FILENAME         "wakeup_drv.ko"
#define NEXUS_DRIVER_FILENAME          "nexus.ko"
#define NX_ASHMEM_DRIVER_FILENAME      "nx_ashmem.ko"

#define NXSERVER_FILENAME              "/system/bin/nxserver"

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
    char full_name[256];

    memset(full_name, 0, sizeof(full_name));
    snprintf(full_name, sizeof(full_name), "%s/%s", VENDOR_DRIVER_PATH, filename);

    file = nexusinit_grab_file(full_name, &len);
    if (!file) {
        LOGE("nexusinit: nexusinit_grab_file(%s) returned NULL!", full_name);
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

BERR_Code nexusinit_ashmem()
{
    char value[PROPERTY_VALUE_MAX];
    char value2[2*PROPERTY_VALUE_MAX];

    memset(value2, 0, sizeof(value2));
    property_get("ro.nexus.ashmem.devname", value, NULL);
    if (strlen(value) > 0) {
        strcpy(value2, "devname=\"");
        strcat(value2, value);
        strcat(value2, "\"");
        LOGD("nexusinit: ro.nexus.ashmem.devname=%s", value);
    }
    if (nexusinit_insmod(NX_ASHMEM_DRIVER_FILENAME, value2) != BERR_SUCCESS) {
        LOGE("nexusinit: insmod failed on %s!", NX_ASHMEM_DRIVER_FILENAME);
        return BERR_UNKNOWN;
    }

    return BERR_SUCCESS;
}

void startNxServer(void)
{
    char cmdRunNxServer[512];
    char value[PROPERTY_VALUE_MAX];
    char nx_key[PROPERTY_VALUE_MAX];
    struct timeval t;
    FILE *key = NULL;

    sprintf(nx_key, "%s/nx_key", NEXUS_TRUSTED_DATA_PATH);
    key = fopen(nx_key, "r");
    if (key == NULL) {
       ALOGE("%s: failed to open key file \'%s\', err=%d (%s)\n", __FUNCTION__, nx_key, errno, strerror(errno));
    }

    memset(cmdRunNxServer, 0, sizeof(cmdRunNxServer));
    strcpy(cmdRunNxServer, NXSERVER_FILENAME);
    strcat(cmdRunNxServer, " ");

    strcat(cmdRunNxServer, "-ir none ");

    memset(value, 0, sizeof(value));
    if (property_get("ro.hd_output_format", value, NULL)) {
        /* If the HD output resolution is set to 1080p, then we can use the HDMI preferred output format
           (typically 1080p also) and not ignore the HDMI EDID information from the connected TV. */
        if (strncmp((char *) value, "1080p", 5) != 0) {
            strcat(cmdRunNxServer, "-ignore_edid ");
        }
        /* ensure we setup the surface compositor to 1080p. */
        else {
            strcat(cmdRunNxServer, "-fbsize 1920,1080 ");
        }
    }
#ifndef BCM_OMX_SUPPORT_ENCODER
    strcat(cmdRunNxServer, "-transcode off ");
#endif
    strcat(cmdRunNxServer, "-svp ");
    strcat(cmdRunNxServer, "-grab off ");
    strcat(cmdRunNxServer, "-sd off -memconfig display,hddvi=off ");

    memset(value, 0, sizeof(value));
    property_get("ro.nexus.nxserver.hdcp1x_keys", value, NULL);
    if (strlen(value)) {
       snprintf(cmdRunNxServer, sizeof(cmdRunNxServer), "%s-hdcp1x_keys %s ", cmdRunNxServer, value);
    }
    memset(value, 0, sizeof(value));
    property_get("ro.nexus.nxserver.hdcp2x_keys", value, NULL);
    if (strlen(value)) {
       snprintf(cmdRunNxServer, sizeof(cmdRunNxServer), "%s-hdcp2x_keys %s ", cmdRunNxServer, value);
    }

    if (key) {
       snprintf(cmdRunNxServer, sizeof(cmdRunNxServer), "%s-password %s ", cmdRunNxServer, nx_key);
       fclose(key);
    }

    ALOGI("NXSERVER CMD (%d): %s", strlen(cmdRunNxServer), cmdRunNxServer);
    strcat(cmdRunNxServer, "&");
    system(cmdRunNxServer);
}

#include <stdlib.h>
static void
b_parse_env(char *env)
{
    char *s;
    const char *name;
    const char *value;
    /* traverse  string, and split it to name/value pairs */
    for(s=env, name=env, value=NULL;;s++) {
        switch(*s) {
        case '\0':
            goto done;
        case '=':
            *s = '\0';
            value = s+1;
            break;
        case ' ':
        case ':':
        case ';':
            *s = '\0';
            if (value==NULL) {
                value=s;
            }
            setenv(name, value, 1);
            name = s+1;
            value = NULL;
            break;
        default:
            break;
        }
    }
done:
    if(*name) {
        if (value==NULL) {
            value=s;
        }
        setenv(name, value, 1);
    }
    return;
}

int main(void)
{
    char value[PROPERTY_VALUE_MAX];
    char value2[2 * PROPERTY_VALUE_MAX] = { 0 };
    NEXUS_Error rc;
    FILE *key = NULL;
    NxClient_JoinSettings joinSettings;

    if (daemon(0, 0) < 0) {
        LOGE("nexusinit: FATAL: Daemonise Failed!");
        _exit( 1 );
    }

    android::ProcessState::self()->startThreadPool();

    property_get("ro.nexus.wake.devname", value, NULL);
    if (strlen(value) > 0) {
        strcpy(value2, "devname=\"");
        strcat(value2, value);
        strcat(value2, "\"");
        LOGD("nexusinit: ro.nexus.wake.devname=%s", value);
    }

    /* insmod wakeup driver first */
    if(nexusinit_insmod(WAKEUP_DRIVER_FILENAME, value2) != BERR_SUCCESS) {
        LOGE("nexusinit: insmod failed on %s!", WAKEUP_DRIVER_FILENAME);
        /* failure is non fatal. */
    }
    else {
        LOGI("nexusinit: insmod %s succeeded", WAKEUP_DRIVER_FILENAME);
    }

    property_get("ro.nexus_config", value, NULL);
    value2[0] = '\0';
    if (strlen(value) > 0) {
        strcat(value2, "config=");
        strcat(value2, value);
        LOGD("nexusinit: ro.nexus_config = %s", value);
        /* Setup environment variables for nxserver to inherit as well */
        b_parse_env(value);
    }

    property_get("ro.nexus.devname", value, NULL);
    if (strlen(value) > 0) {
        if (strlen(value2) > 0) {
           strcat(value2, " ");
        }
        strcat(value2, "devname=\"");
        strcat(value2, value);
        strcat(value2, "\"");
        LOGD("nexusinit: ro.nexus.devname=%s", value);
    }

    /* insmod nexus driver second */
    if(nexusinit_insmod(NEXUS_DRIVER_FILENAME, value2) != BERR_SUCCESS) {
        LOGE("nexusinit: FATAL: insmod failed on %s!", NEXUS_DRIVER_FILENAME);
        /* failure is FATAL. */
        _exit(1);
    }
    else {
        LOGI("nexusinit: insmod %s succeeded", NEXUS_DRIVER_FILENAME);
    }

    struct stat buffer;
    if (stat(NXSERVER_FILENAME, &buffer) == 0)
    {
        startNxServer();

        sprintf(value, "%s/nx_key", NEXUS_TRUSTED_DATA_PATH);
        key = fopen(value, "r");

        if (key == NULL) {
           ALOGE("%s: failed to open key file \'%s\', err=%d (%s)\n", __FUNCTION__, value, errno, strerror(errno));
        } else {
           memset(value, 0, sizeof(value));
           fread(value, PROPERTY_VALUE_MAX, 1, key);
           fclose(key);
        }

        NxClient_GetDefaultJoinSettings(&joinSettings);
        snprintf(joinSettings.name, NXCLIENT_MAX_NAME, "nexusinit");
        joinSettings.timeout = 60;

        joinSettings.mode = NEXUS_ClientMode_eUntrusted;
        if (strlen(value)) {
           if (strstr(value, "trusted:") == value) {
              const char *password = &value[8];
              joinSettings.mode = NEXUS_ClientMode_eProtected;
              joinSettings.certificate.length = strlen(password);
              memcpy(joinSettings.certificate.data, password, joinSettings.certificate.length);
           }
        }

        /* ... wait for server to be ready. */
        if (NxClient_Join(&joinSettings) != NEXUS_SUCCESS)
        {
            LOGE("nexusinit: FATAL: join failed, is server running?");
            _exit(1);
        }
        LOGI("%s: \"%s\"; joins %s mode", __FUNCTION__, joinSettings.name,
             (joinSettings.mode == NEXUS_ClientMode_eProtected) ? "PROTECTED" : "UNTRUSTED");
        /* okay, we are ready... */
        NxClient_Uninit();

        property_set("hw.nexus.platforminit", "on");

        /* Add the nx_ashmem module which is required for gralloc to function */
        if (nexusinit_ashmem() != BERR_SUCCESS) {
            LOGE("nexusinit: FATAL: Could not initialise ashmem!");
            _exit(1);
        }

    }
    else
    {
        LOGE("nexusinit: FATAL: Could not find \"" NXSERVER_FILENAME "\"!");
        _exit(1);
    }

    NexusNxService::instantiate();
    LOGI("nexusinit: NexusNxService is now ready");
    android::IPCThreadState::self()->joinThreadPool();
}
