/******************************************************************************
 *    (c) 2015 Broadcom Corporation
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
 *****************************************************************************/
#define LOG_TAG "nxserver"

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

#include "nxserver.h"
#include "nxclient.h"
#include "nxserverlib.h"
#include "nexusnxservice.h"

#define NEXUS_TRUSTED_DATA_PATH        "/data/misc/nexus"
#define APP_MAX_CLIENTS 20
#define MB (1024*1024)

static struct {
    BKNI_MutexHandle lock;
    nxserver_t server;
    unsigned refcnt;
    struct {
        nxclient_t client;
        NxClient_JoinSettings joinSettings;
    } clients[APP_MAX_CLIENTS]; /* index provides id */
} g_app;

static bool g_exit = false;


static int client_connect(nxclient_t client, const NxClient_JoinSettings *pJoinSettings, NEXUS_ClientSettings *pClientSettings)
{
    unsigned i;
    BSTD_UNUSED(pClientSettings);
    /* server app has opportunity to reject client altogether, or modify pClientSettings */
    for (i=0;i<APP_MAX_CLIENTS;i++) {
        if (!g_app.clients[i].client) {
            g_app.clients[i].client = client;
            g_app.clients[i].joinSettings = *pJoinSettings;
            break;
        }
    }
    return 0;
}

static void client_disconnect(nxclient_t client, const NxClient_JoinSettings *pJoinSettings)
{
    unsigned i;
    BSTD_UNUSED(pJoinSettings);
    for (i=0;i<APP_MAX_CLIENTS;i++) {
        if (g_app.clients[i].client == client) {
            g_app.clients[i].client = NULL;
            break;
        }
    }
}

static nxserver_t init_nxserver(void)
{
    NEXUS_Error rc;
    NEXUS_PlatformSettings platformSettings;
    NEXUS_MemoryConfigurationSettings memConfigSettings;
    struct nxserver_settings settings;
    char value[PROPERTY_VALUE_MAX];
    char value2[PROPERTY_VALUE_MAX];
    unsigned ix;
    char nx_key[PROPERTY_VALUE_MAX];
    FILE *key = NULL;

    if (g_app.refcnt == 1) {
        g_app.refcnt++;
        return g_app.server;
    }
    if (g_app.server) {
        return NULL;
    }

    nxserver_get_default_settings(&settings);
    NEXUS_Platform_GetDefaultSettings(&platformSettings);
    NEXUS_GetDefaultMemoryConfigurationSettings(&memConfigSettings);

    /* setup the configuration we want for the device.  right now, hardcoded for a generic
       android device, longer term, we want more flexibility. */

    /* -ir none */
    settings.session[0].ir_input_mode = NEXUS_IrInputMode_eMax;
    memset(value, 0, sizeof(value));
    if (property_get("ro.hd_output_format", value, NULL)) {
        if (strncmp((char *) value, "1080p", 5) != 0) {
            /* -ignore_edid */
            settings.session[0].edid_mode = nxserver_hdmi_edid_mode_ignore;
        } else {
            /* -fbsize 1920,1080 */
            settings.fbsize.width = 1920;
            settings.fbsize.height = 1080;
        }
    }
#ifndef BCM_OMX_SUPPORT_ENCODER
    /* -transcode off */
    settings.transcode = false;
#endif
    /* -svp */
    settings.svp = true;
    /* -grab off */
    settings.grab = 0;
    /* -sd off */
    settings.session[0].output.sd = settings.session[0].output.encode = false;
    settings.session[0].output.hd = true;
    /* -memconfig display,hddvi=off */
    memConfigSettings.videoInputs.hdDvi = false;
#ifdef USE_MMA
    /* -growHeapBlockSize 32m -heap gfx,128m */
    settings.growHeapBlockSize = 32 * MB;
    ix = memConfigSettings.memoryLayout.heapIndex.graphics[0];
    if (ix != (unsigned)-1) {
       platformSettings.heap[ix].size = 128 * MB;
    }
#endif
    /* -heap video_secure,80m */
    platformSettings.heap[NEXUS_VIDEO_SECURE_HEAP].size = 80 * MB;
    /* -password file-path-name */
    sprintf(nx_key, "%s/nx_key", NEXUS_TRUSTED_DATA_PATH);
    key = fopen(nx_key, "r");
    if (key == NULL) {
       ALOGE("%s: failed to open key file \'%s\', err=%d (%s)\n", __FUNCTION__, nx_key, errno, strerror(errno));
    } else {
       settings.client_mode = NEXUS_ClientMode_eUntrusted;
       while (!feof(key)) {
          memset(value, 0, sizeof(value));
          fgets(value, sizeof(value), key);
          unsigned len = strlen(value);
          if (len && value[len-1] == '\n') value[--len] = 0;
          if (len && value[len-1] == '\r') value[--len] = 0;
          if (!len || value[0] == '#') continue;
          if (strstr(value, "trusted:") == value) {
             const char *password = &value[8];
             settings.certificate.length = strlen(password);
             memcpy(settings.certificate.data, password, settings.certificate.length);
          }
       }
       fclose(key);
    }
    /* -hdcp1x_keys some-key-file  -- setup last. */
    memset(value, 0, sizeof(value));
    property_get("ro.nexus.nxserver.hdcp1x_keys", value, NULL);
    if (strlen(value)) {
       settings.hdcp.hdcp1xBinFile = value;
    }
    /* -hdcp2x_keys some-key-file  -- setup last. */
    memset(value2, 0, sizeof(value2));
    property_get("ro.nexus.nxserver.hdcp2x_keys", value2, NULL);
    if (strlen(value2)) {
       settings.hdcp.hdcp2xBinFile = value2;
    }

    /* TODO: some bit of 'post cmdline parsing' magic which nexus bundles up
       with the cmdline, but which should really be independent, once nexus lib
       provides a cleaned out support for this via a proper function call, we can
       remove it and make an api call instead.
    */
    platformSettings.openCec = false;
    platformSettings.openFrontend = false;
    platformSettings.audioModuleSettings.maxAudioDspTasks = 2;
    memConfigSettings.videoDecoder[0].avc51Supported = false;
    for (ix = 0 ; ix < NXCLIENT_MAX_SESSIONS ; ix++) {
        if (settings.session[ix].output.sd) break;
    }
    if (ix == NXCLIENT_MAX_SESSIONS) {
       memConfigSettings.display[1].maxFormat = NEXUS_VideoFormat_eUnknown;
       memConfigSettings.display[0].window[0].precisionLipSync = false;
    }
    /* TODO - end. */

    rc = NEXUS_Platform_MemConfigInit(&platformSettings, &memConfigSettings);
    if (rc) {
       ALOGE("FATAL: failed NEXUS_Platform_MemConfigInit");
       return NULL;
    }

    BKNI_CreateMutex(&g_app.lock);
    settings.lock = g_app.lock;
    nxserver_set_client_heaps(&settings, &memConfigSettings);
    settings.client.connect = client_connect;
    settings.client.disconnect = client_disconnect;
    settings.memConfigSettings = memConfigSettings;
    g_app.server = nxserverlib_init(&settings);
    if (!g_app.server) {
       ALOGE("FATAL: failed nxserverlib_init");
       return NULL;
    }

    rc = nxserver_ipc_init(g_app.server, g_app.lock);
    if (rc) {
       ALOGE("FATAL: failed nxserver_ipc_init");
       return NULL;
    }

    g_app.refcnt++;
    return g_app.server;
}

static void uninit_nxserver(nxserver_t server)
{
    BDBG_ASSERT(server == g_app.server);
    if (--g_app.refcnt) return;

    nxserver_ipc_uninit();
    nxserverlib_uninit(server);
    BKNI_DestroyMutex(g_app.lock);
    NEXUS_Platform_Uninit();
}

int main(void)
{
    struct stat sbuf;
    nxserver_t nx_srv = NULL;

    memset(&g_app, 0, sizeof(g_app));

    const char *devName = getenv("NEXUS_DEVICE_NODE");
    if (!devName) {
        devName = "/dev/nexus";
    }

    /* delay until nexus device is present and writable */
    while (stat(devName, &sbuf) == -1 || !(sbuf.st_mode & S_IWOTH)) {
        ALOGW("Waiting for %s device...\n", devName);
        sleep(1);
    }

    nx_srv = init_nxserver();
    if (nx_srv == NULL) {
        ALOGE("FATAL: Daemonise Failed!");
        _exit(1);
    }

    /* trigger waiter on nexus server initialization. */
    property_set("hw.nexus.platforminit", "on");

    /* start binder ipc. */
    android::ProcessState::self()->startThreadPool();
    NexusNxService::instantiate();
    android::IPCThreadState::self()->joinThreadPool();

    /* loop forever. */
    while (1) {
       BKNI_Sleep(1000);
       if (g_exit) break;
    }

    uninit_nxserver(nx_srv);
    return 0;
}
