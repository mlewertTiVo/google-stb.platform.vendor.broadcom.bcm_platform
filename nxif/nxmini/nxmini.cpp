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
#define LOG_TAG "nxmini"

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

#include "nxmini.h"
#include "nxserverlib.h"

#define MB (1024*1024)
#define KB (1024)

// default lower bound that fits all platforms needs.
//
#define NX_HEAP_VIDEO_SECURE       1
#define NX_HEAP_VIDEO_SECURE_VALUE "32m"
#define NX_HEAP_MAIN               1
#define NX_HEAP_MAIN_VALUE         "48m"
#define NX_HEAP_GFX                1
#define NX_HEAP_GFX_VALUE          "64m"
#define NX_HEAP_GFX2               0
#define NX_HEAP_GFX2_VALUE         "0m"
#define NX_HEAP_HIGH_MEM           1
#define NX_HEAP_HIGH_MEM_VALUE     "0m"
#define NX_HEAP_DRV_MANAGED        1
#define NX_HEAP_DRV_MANAGED_VALUE  "4m"

static struct {
    BKNI_MutexHandle lock;
    nxserver_t server;
    unsigned refcnt;
} g_app;

static bool g_exit = false;

static int lookup_heap_type(const NEXUS_PlatformSettings *pPlatformSettings, unsigned heapType)
{
    unsigned i;
    for (i=0;i<NEXUS_MAX_HEAPS;i++) {
        if (pPlatformSettings->heap[i].size && pPlatformSettings->heap[i].heapType & heapType) return i;
    }
    return -1;
}

static int lookup_heap_memory_type(const NEXUS_PlatformSettings *pPlatformSettings, unsigned memoryType)
{
    unsigned i;
    for (i=0;i<NEXUS_MAX_HEAPS;i++) {
        if (pPlatformSettings->heap[i].size &&
            ((pPlatformSettings->heap[i].memoryType & memoryType) == memoryType)) return i;
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

static void pre_trim_mem_config(NEXUS_MemoryConfigurationSettings *pMemConfigSettings)
{
   /* sd 'off' - hardcoded knowledge. */
   pMemConfigSettings->display[1].maxFormat = NEXUS_VideoFormat_eUnknown;
}

static void trim_mem_config(NEXUS_MemoryConfigurationSettings *pMemConfigSettings)
{
   int i, j;

   /* nxmini - removes almost everything, fit in a tiny footprint the necessary
    *          platform support, keep a decoder just in case.
    */

   for (i = 1; i < NEXUS_MAX_DISPLAYS; i++) {
       pMemConfigSettings->display[i].maxFormat = NEXUS_VideoFormat_eUnknown;
       for (j = 0; j < NEXUS_MAX_VIDEO_WINDOWS; j++) {
           pMemConfigSettings->display[i].window[j].used = false;
       }
   }
   pMemConfigSettings->display[0].window[1].used = false;
   pMemConfigSettings->display[0].window[0].secure = NEXUS_SecureVideo_eUnsecure;

   pMemConfigSettings->videoInputs.hdDvi = false;
   pMemConfigSettings->videoInputs.ccir656 = false;

   trim_encoder_mem_config(pMemConfigSettings);

   for (i = 1; i < NEXUS_MAX_VIDEO_DECODERS; i++) {
      pMemConfigSettings->videoDecoder[i].used = false;
   }
   for (i = 0; i < NEXUS_NUM_STILL_DECODES; i++) {
      pMemConfigSettings->stillDecoder[i].used = false;
   }

   pMemConfigSettings->videoDecoder[0].maxFormat = NEXUS_VideoFormat_eNtsc;
   pMemConfigSettings->videoDecoder[0].colorDepth = 8;
   pMemConfigSettings->videoDecoder[0].supportedCodecs[NEXUS_VideoCodec_eVp9] = false;
   pMemConfigSettings->videoDecoder[0].supportedCodecs[NEXUS_VideoCodec_eVc1] = false;
   pMemConfigSettings->videoDecoder[0].supportedCodecs[NEXUS_VideoCodec_eH263] = false;
}

static nxserver_t init_nxserver(void)
{
    NEXUS_Error rc;
    NEXUS_PlatformSettings platformSettings;
    NEXUS_MemoryConfigurationSettings memConfigSettings;
    struct nxserver_settings settings;
    struct nxserver_cmdline_settings cmdline_settings;

    if (g_app.refcnt == 1) {
        g_app.refcnt++;
        return g_app.server;
    }
    if (g_app.server) {
        return NULL;
    }

    memset(&cmdline_settings, 0, sizeof(cmdline_settings));
    cmdline_settings.frontend = false;

    nxserver_get_default_settings(&settings);
    NEXUS_Platform_GetDefaultSettings(&platformSettings);
    NEXUS_GetDefaultMemoryConfigurationSettings(&memConfigSettings);

    settings.svp = nxserverlib_svp_type_none;
    settings.session[0].ir_input_mode = NEXUS_IrInputMode_eMax;
    settings.transcode = false;
    settings.grab = 0;
    settings.session[0].output.sd = false;
    settings.session[0].output.encode = false;
    settings.session[0].output.hd = false;

    if (NX_HEAP_VIDEO_SECURE) {
       int index = lookup_heap_type(&platformSettings, NEXUS_HEAP_TYPE_COMPRESSED_RESTRICTED_REGION);
       if (index != -1) {
          platformSettings.heap[index].size = calc_heap_size(NX_HEAP_VIDEO_SECURE_VALUE);
       }
    }

    if (NX_HEAP_MAIN) {
       int index = lookup_heap_type(&platformSettings, NEXUS_HEAP_TYPE_MAIN);
       if (index != -1) {
          platformSettings.heap[index].size = calc_heap_size(NX_HEAP_MAIN_VALUE);
       }
    }

    if (NX_HEAP_GFX) {
       int index = lookup_heap_type(&platformSettings, NEXUS_HEAP_TYPE_GRAPHICS);
       if (index != -1) {
          platformSettings.heap[index].size = calc_heap_size(NX_HEAP_GFX_VALUE);
       }
    }

    if (NX_HEAP_GFX2) {
       int index = lookup_heap_type(&platformSettings, NEXUS_HEAP_TYPE_SECONDARY_GRAPHICS);
       if (index != -1) {
          platformSettings.heap[index].size = calc_heap_size(NX_HEAP_GFX2_VALUE);
       }
    }

    if (NX_HEAP_HIGH_MEM) {
       int index = lookup_heap_memory_type(&platformSettings, NEXUS_MEMORY_TYPE_HIGH_MEMORY);
       if (index != -1) {
          platformSettings.heap[index].size = calc_heap_size(NX_HEAP_HIGH_MEM_VALUE);
       }
    }

    if (NX_HEAP_DRV_MANAGED) {
       int index = lookup_heap_memory_type(&platformSettings,
          (NEXUS_MEMORY_TYPE_MANAGED|NEXUS_MEMORY_TYPE_DRIVER_UNCACHED|NEXUS_MEMORY_TYPE_DRIVER_CACHED|NEXUS_MEMORY_TYPE_APPLICATION_CACHED));
       if (index != -1) {
          platformSettings.heap[index].size = calc_heap_size(NX_HEAP_DRV_MANAGED_VALUE);
       }
    }

    pre_trim_mem_config(&memConfigSettings);

    rc = nxserver_modify_platform_settings(&settings, &cmdline_settings, &platformSettings, &memConfigSettings);
    if (rc) {
       ALOGE("FATAL: failed nxserver_modify_platform_settings");
       return NULL;
    }

    trim_mem_config(&memConfigSettings);

    rc = NEXUS_Platform_MemConfigInit(&platformSettings, &memConfigSettings);
    if (rc) {
       ALOGE("FATAL: failed NEXUS_Platform_MemConfigInit");
       return NULL;
    }

    BKNI_CreateMutex(&g_app.lock);
    settings.lock = g_app.lock;
    nxserver_set_client_heaps(&settings, &platformSettings);
    settings.memConfigSettings = memConfigSettings;
    g_app.server = nxserverlib_init(&settings);
    if (!g_app.server) {
       ALOGE("FATAL: failed nxserverlib_init");
       return NULL;
    }

    g_app.refcnt++;
    return g_app.server;
}

static void uninit_nxserver(nxserver_t server)
{
    BDBG_ASSERT(server == g_app.server);
    if (--g_app.refcnt) return;

    nxserverlib_uninit(server);
    BKNI_DestroyMutex(g_app.lock);
    NEXUS_Platform_Uninit();
}

int main(void)
{
    struct stat sbuf;
    nxserver_t nx_srv = NULL;

    memset(&g_app, 0, sizeof(g_app));

    nx_srv = init_nxserver();
    if (nx_srv == NULL) {
        ALOGE("FATAL: Daemonise Failed!");
        _exit(1);
    }

    /* trigger waiter on nexus server initialization. */
    property_set("hw.nexus.platforminit", "on");

    /* loop forever. */
    while (1) {
       BKNI_Sleep(1000);
       if (g_exit) break;
    }

    uninit_nxserver(nx_srv);
    return 0;
}
