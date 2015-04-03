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
#include "namevalue.h"

#define NEXUS_TRUSTED_DATA_PATH        "/data/misc/nexus"
#define APP_MAX_CLIENTS 20
#define MB (1024*1024)
#define KB (1024)

#define GRAPHICS_RES_WIDTH_DEFAULT     1920
#define GRAPHICS_RES_HEIGHT_DEFAULT    1080
#define GRAPHICS_RES_WIDTH_PROP        "ro.graphics_resolution.width"
#define GRAPHICS_RES_HEIGHT_PROP       "ro.graphics_resolution.height"

#define NX_MMA                         "ro.nx.mma"
#define NX_TRANSCODE                   "ro.nx.transcode"

#define NX_HEAP_MAIN                   "ro.nx.heap.main"
#define NX_HEAP_GFX                    "ro.nx.heap.gfx"
#define NX_HEAP_GFX2                   "ro.nx.heap.gfx2"
#define NX_HEAP_VIDEO_SECURE           "ro.nx.heap.video_secure"
#define NX_HEAP_GROW                   "ro.nx.heap.grow"

#define NX_HD_OUT_FMT                  "persist.hd_output_format"
#define NX_HDCP1X_KEY                  "ro.nexus.nxserver.hdcp1x_keys"
#define NX_HDCP2X_KEY                  "ro.nexus.nxserver.hdcp2x_keys"

#define NX_TRIM_VP9                    "ro.nx.trim.vp9"
#define NX_TRIM_PIP                    "ro.nx.trim.pip"
#define NX_TRIM_MOSAIC                 "ro.nx.trim.mosaic"

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

static int lookup_heap_type(const NEXUS_PlatformSettings *pPlatformSettings, unsigned heapType)
{
    unsigned i;
    for (i=0;i<NEXUS_MAX_HEAPS;i++) {
        if (pPlatformSettings->heap[i].size && pPlatformSettings->heap[i].heapType & heapType) return i;
    }
    return -1;
}

static unsigned calc_heap_size(const char *value)
{
   if (strchr(value, 'M') || strchr(value, 'm')) {
     return atof(value)*MB;
   } else if (strchr(value, 'K') || strchr(value, 'k')) {
     return atof(value)*KB;
   } else {
     return strtoul(value, NULL, 0);
   }
}

static void trim_mem_config(NEXUS_MemoryConfigurationSettings *pMemConfigSettings)
{
   int i;
   char value[PROPERTY_VALUE_MAX];

   /* no SD display - this is hardcoded knowledge. */
   pMemConfigSettings->display[1].window[0].used = false;
   pMemConfigSettings->display[1].window[1].used = false;

   /* need vp9? */
   if (property_get(NX_TRIM_VP9, value, NULL)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         for (i = 0 ; i < NEXUS_NUM_VIDEO_DECODERS ; i++) {
            pMemConfigSettings->videoDecoder[i].supportedCodecs[NEXUS_VideoCodec_eVp9] = false;
         }
         pMemConfigSettings->stillDecoder[0].supportedCodecs[NEXUS_VideoCodec_eVp9] = false;
      }
   }

   /* need mosaic? */
   if (property_get(NX_TRIM_MOSAIC, value, NULL)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         pMemConfigSettings->videoDecoder[0].mosaic.maxNumber = 0;
         pMemConfigSettings->videoDecoder[0].mosaic.maxHeight = 0;
         pMemConfigSettings->videoDecoder[0].mosaic.maxWidth = 0;

      }
   }

   /* need pip? */
   if (property_get(NX_TRIM_PIP, value, NULL)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         pMemConfigSettings->videoDecoder[1].used = false;
         pMemConfigSettings->display[0].window[1].used = false;
      }
   }
}

static nxserver_t init_nxserver(void)
{
    NEXUS_Error rc;
    NEXUS_PlatformSettings platformSettings;
    NEXUS_MemoryConfigurationSettings memConfigSettings;
    struct nxserver_settings settings;
    struct nxserver_cmdline_settings cmdline_settings;

    char value[PROPERTY_VALUE_MAX];
    char value2[PROPERTY_VALUE_MAX];
    int ix, uses_mma = 0;
    char nx_key[PROPERTY_VALUE_MAX];
    FILE *key = NULL;

    if (g_app.refcnt == 1) {
        g_app.refcnt++;
        return g_app.server;
    }
    if (g_app.server) {
        return NULL;
    }

    memset(&cmdline_settings, 0, sizeof(cmdline_settings));
    cmdline_settings.frontend = true;

    nxserver_get_default_settings(&settings);
    NEXUS_Platform_GetDefaultSettings(&platformSettings);
    NEXUS_GetDefaultMemoryConfigurationSettings(&memConfigSettings);

    memset(value, 0, sizeof(value));
    if (property_get(NX_MMA, value, "0")) {
       uses_mma = (strtoul(value, NULL, 10) > 0) ? 1 : 0;
    }

    /* setup the configuration we want for the device.  right now, hardcoded for a generic
       android device, longer term, we want more flexibility. */

    /* -ir none */
    settings.session[0].ir_input_mode = NEXUS_IrInputMode_eMax;
    /* -fbsize w,h */
    settings.fbsize.width = property_get_int32(
        GRAPHICS_RES_WIDTH_PROP, GRAPHICS_RES_WIDTH_DEFAULT);
    settings.fbsize.height = property_get_int32(
        GRAPHICS_RES_HEIGHT_PROP, GRAPHICS_RES_HEIGHT_DEFAULT);
    memset(value, 0, sizeof(value));
    if (property_get(NX_HD_OUT_FMT, value, "")) {
        if (strlen(value)) {
            /* -display_format XX */
            settings.display.format = (NEXUS_VideoFormat)lookup(g_videoFormatStrs, value);
            ALOGI("%s: display format = %s", __FUNCTION__, lookup_name(
                g_videoFormatStrs, settings.display.format));
            /* -ignore_edid */
            settings.display.hdmiPreferences.followPreferredFormat = false;
        }
    }
    /* -transcode off */
    settings.transcode = (property_get_int32(NX_TRANSCODE, 0) ? true : false);
    /* -svp */
    settings.svp = true;
    /* -grab off */
    settings.grab = 0;
    /* -sd off */
    settings.session[0].output.sd = settings.session[0].output.encode = false;
    settings.session[0].output.hd = true;
    /* -memconfig display,hddvi=off */
    memConfigSettings.videoInputs.hdDvi = false;

    if (property_get(NX_HEAP_VIDEO_SECURE, value, NULL)) {
       int index = lookup_heap_type(&platformSettings, NEXUS_HEAP_TYPE_COMPRESSED_RESTRICTED_REGION);
       if (strlen(value) && (index != -1)) {
          /* -heap video_secure,XXy */
          platformSettings.heap[index].size = calc_heap_size(value);
       }
    }

    if (property_get(NX_HEAP_MAIN, value, NULL)) {
       int index = lookup_heap_type(&platformSettings, NEXUS_HEAP_TYPE_MAIN);
       if (strlen(value) && (index != -1)) {
          /* -heap main,XXy */
          platformSettings.heap[index].size = calc_heap_size(value);
       }
    }

    if (property_get(NX_HEAP_GFX, value, NULL)) {
       int index = lookup_heap_type(&platformSettings, NEXUS_HEAP_TYPE_GRAPHICS);
       if (strlen(value) && (index != -1)) {
          /* -heap gfx,XXy */
          platformSettings.heap[index].size = calc_heap_size(value);
       }
    }

    if (!uses_mma) {
       if (property_get(NX_HEAP_GFX2, value, NULL)) {
          int index = lookup_heap_type(&platformSettings, NEXUS_HEAP_TYPE_SECONDARY_GRAPHICS);
          if (strlen(value) && (index != -1)) {
             /* -heap gfx2,XXy */
             platformSettings.heap[index].size = calc_heap_size(value);
          }
       }
    }

    if (uses_mma) {
       if (property_get(NX_HEAP_GROW, value, NULL)) {
          if (strlen(value)) {
             /* -growHeapBlockSize XXy */
             settings.growHeapBlockSize = calc_heap_size(value);
          }
       }
    }

    if (uses_mma && settings.growHeapBlockSize) {
       int index = lookup_heap_type(&platformSettings, NEXUS_HEAP_TYPE_GRAPHICS);
       if (index >= NEXUS_MAX_HEAPS) {
           ALOGE("growHeapBlockSize: requires platform implement NEXUS_PLATFORM_P_GET_FRAMEBUFFER_HEAP_INDEX");
           return NULL;
       }
       platformSettings.heap[NEXUS_MAX_HEAPS-2].memcIndex = platformSettings.heap[index].memcIndex;
       platformSettings.heap[NEXUS_MAX_HEAPS-2].subIndex = platformSettings.heap[index].subIndex;
       platformSettings.heap[NEXUS_MAX_HEAPS-2].size = 4096;
       platformSettings.heap[NEXUS_MAX_HEAPS-2].memoryType = NEXUS_MEMORY_TYPE_MANAGED|NEXUS_MEMORY_TYPE_ONDEMAND_MAPPED;
    }

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
    property_get(NX_HDCP1X_KEY, value, NULL);
    if (strlen(value)) {
       settings.hdcp.hdcp1xBinFile = value;
    }
    /* -hdcp2x_keys some-key-file  -- setup last. */
    memset(value2, 0, sizeof(value2));
    property_get(NX_HDCP2X_KEY, value2, NULL);
    if (strlen(value2)) {
       settings.hdcp.hdcp2xBinFile = value2;
    }

    rc = nxserver_modify_platform_settings(&settings, &cmdline_settings, &platformSettings, &memConfigSettings);
    if (rc) {
       ALOGE("FATAL: failed nxserver_modify_platform_settings");
       return NULL;
    }

    /* now, just before applying the configuration, try to reduce the memory footprint
     * allocated by nexus for the platform needs.
     *
     * savings are essentially made through feature disablement. thus you have to be careful
     * and aware of the feature targetted for the platform.
     */
    trim_mem_config(&memConfigSettings);

    rc = NEXUS_Platform_MemConfigInit(&platformSettings, &memConfigSettings);
    if (rc) {
       ALOGE("FATAL: failed NEXUS_Platform_MemConfigInit");
       return NULL;
    }

    BKNI_CreateMutex(&g_app.lock);
    settings.lock = g_app.lock;
    nxserver_set_client_heaps(&settings, &platformSettings);
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
