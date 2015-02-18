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

#define NEXUS_TRUSTED_DATA_PATH        "/data/misc/nexus"
#define NXSERVER_FILENAME              "/system/bin/nxserver"

extern "C" long int init_module(void *, unsigned long, const char*);
extern "C" long int delete_module(const char *, unsigned long, unsigned long, unsigned long);

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
#ifdef USE_MMA
    strcat(cmdRunNxServer, "-growHeapBlockSize 32m -heap gfx,32m ");
#endif

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

int main(void)
{
    char value[PROPERTY_VALUE_MAX];
    NEXUS_Error rc;
    FILE *key = NULL;
    NxClient_JoinSettings joinSettings;
    struct stat sbuf;

    if (daemon(0, 0) < 0) {
        LOGE("nexusinit: FATAL: Daemonise Failed!");
        _exit( 1 );
    }

    android::ProcessState::self()->startThreadPool();

    const char *devName = getenv("NEXUS_DEVICE_NODE");
    if (!devName)
    {
        devName = "/dev/nexus";
    }

    /* Delay until nexus device is present and writable */
    while (stat(devName, &sbuf) == -1 || !(sbuf.st_mode & S_IWOTH))
    {
        ALOGW("Waiting for %s device...\n", devName);
        sleep(1);
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
        joinSettings.timeout = 120;

        joinSettings.mode = NEXUS_ClientMode_eUntrusted;
        if (strlen(value)) {
           if (strstr(value, "trusted:") == value) {
              const char *password = &value[8];
              joinSettings.mode = NEXUS_ClientMode_eVerified;
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
        LOGI("%s: \"%s\"; joins %s mode (%d)", __FUNCTION__, joinSettings.name,
             (joinSettings.mode == NEXUS_ClientMode_eVerified) ? "VERIFIED" : "UNTRUSTED",
             joinSettings.mode);
        /* okay, we are ready... */
        NxClient_Uninit();

        /* trigger waiter on nexus server initialization. */
        property_set("hw.nexus.platforminit", "on");
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
