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
#include "nxserverlib_impl.h"
#include "nexusnxservice.h"
#include "namevalue.h"
#include "nx_ashmem.h"

#define NEXUS_TRUSTED_DATA_PATH        "/data/misc/nexus"
#define APP_MAX_CLIENTS                (64)
#define MB                             (1024*1024)
#define KB                             (1024)
#define SEC_TO_MSEC                    (1000LL)
#define RUNNER_SEC_THRESHOLD           (10)
#define RUNNER_GC_THRESHOLD            (3)
#define RUNNER_LMK_THRESHOLD           (10)
#define NUM_NX_OBJS                    (128)
#define MAX_NX_OBJS                    (2048)
#define MIN_PLATFORM_DEC               (2)

#define GRAPHICS_RES_WIDTH_DEFAULT     (1920)
#define GRAPHICS_RES_HEIGHT_DEFAULT    (1080)
#define GRAPHICS_RES_WIDTH_PROP        "ro.graphics_resolution.width"
#define GRAPHICS_RES_HEIGHT_PROP       "ro.graphics_resolution.height"

#define NX_MMA                         "ro.nx.mma"
#define NX_MMA_ACT_GC                  "ro.nx.mma.act.gc"
#define NX_MMA_ACT_GS                  "ro.nx.mma.act.gs"
#define NX_MMA_ACT_LMK                 "ro.nx.mma.act.lmk"
#define NX_MMA_GROW_SIZE               "ro.nx.heap.grow"
#define NX_MMA_SHRINK_THRESHOLD        "ro.nx.heap.shrink"
#define NX_MMA_SHRINK_THRESHOLD_DEF    "2m"
#define NX_TRANSCODE                   "ro.nx.transcode"
#define NX_AUDIO_LOUDNESS              "ro.nx.audio_loudness"

#define NX_ODV                         "ro.nx.odv"
#define NX_ODV_ALT_THRESHOLD           "ro.nx.odv.use.alt"
#define NX_ODV_ALT_1_USAGE             "ro.nx.odv.a1.use"
#define NX_ODV_ALT_2_USAGE             "ro.nx.odv.a2.use"

#define NX_HEAP_MAIN                   "ro.nx.heap.main"
#define NX_HEAP_GFX                    "ro.nx.heap.gfx"
#define NX_HEAP_GFX2                   "ro.nx.heap.gfx2"
#define NX_HEAP_VIDEO_SECURE           "ro.nx.heap.video_secure"
#define NX_HEAP_HIGH_MEM               "ro.nx.heap.highmem"
#define NX_HEAP_DRV_MANAGED            "ro.nx.heap.drv_managed"
#define NX_HEAP_GROW                   "ro.nx.heap.grow"

#define NX_HD_OUT_FMT                  "nx.vidout.force" /* needs prefixing. */
#define NX_HDCP1X_KEY                  "ro.nexus.nxserver.hdcp1x_keys"
#define NX_HDCP2X_KEY                  "ro.nexus.nxserver.hdcp2x_keys"

#define NX_LOGGER_DISABLED             "ro.nx.logger_disabled"
#define NX_LOGGER_SIZE                 "ro.nx.logger_size"
#define NX_AUDIO_LOG                   "ro.nx.audio_log"

#define NX_HEAP_DYN_FREE_THRESHOLD     (1920*1080*4) /* one 1080p RGBA. */

/* begnine trimming config - not needed for ATV experience. */
#define NX_TRIM_VC1                    "ro.nx.trim.vc1"
#define NX_TRIM_PIP                    "ro.nx.trim.pip"
#define NX_TRIM_MOSAIC                 "ro.nx.trim.mosaic"
#define NX_TRIM_STILLS                 "ro.nx.trim.stills"
#define NX_TRIM_MINFMT                 "ro.nx.trim.minfmt"
#define NX_TRIM_DISP                   "ro.nx.trim.disp"
#define NX_TRIM_VIDIN                  "ro.nx.trim.vidin"
/* destructive trimming config - feature set limitation. */
#define NX_TRIM_VP9                    "ro.nx.trim.vp9"
#define NX_TRIM_4KDEC                  "ro.nx.trim.4kdec"
#define NX_TRIM_10BCOL                 "ro.nx.trim.10bcol"

typedef struct {
   pthread_t runner;
   int running;
   BKNI_EventHandle runner_run;

} RUNNER_T;

typedef struct {
   pthread_t runner;
   int running;

} BINDER_T;

typedef struct {
    BKNI_MutexHandle lock;
    nxserver_t server;
    BINDER_T binder;
    RUNNER_T proactive_runner;
    unsigned refcnt;
    int uses_mma;
    BKNI_MutexHandle clients_lock;
    struct {
        nxclient_t client;
        NxClient_JoinSettings joinSettings;
    } clients[APP_MAX_CLIENTS]; /* index provides id */

} NX_SERVER_T;

static NX_SERVER_T g_app;
static bool g_exit = false;

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

static void *binder_task(void *argv)
{
    NX_SERVER_T *nx_server = (NX_SERVER_T *)argv;

    do
    {
       android::ProcessState::self()->startThreadPool();
       NexusNxService::instantiate();
       android::IPCThreadState::self()->joinThreadPool();

    } while(nx_server->binder.running);

done:
    return NULL;
}

static void *proactive_runner_task(void *argv)
{
    NX_SERVER_T *nx_server = (NX_SERVER_T *)argv;
    unsigned gfx_heap_grow_size = 0;
    unsigned gfx_heap_shrink_threshold = 0;
    int active_gc, active_lmk, active_gs;
    int gc_tick = 0;
    int lmk_tick = 0;
    char value[PROPERTY_VALUE_MAX];
    bool needs_growth;
    int i, j;

    if (property_get(NX_MMA_GROW_SIZE, value, NULL)) {
       if (strlen(value)) {
          gfx_heap_grow_size = calc_heap_size(value);
       }
    }
    if (property_get(NX_MMA_SHRINK_THRESHOLD, value, NX_MMA_SHRINK_THRESHOLD_DEF)) {
       if (strlen(value)) {
          gfx_heap_shrink_threshold = calc_heap_size(value);
       }
    }
    active_gc  = property_get_int32(NX_MMA_ACT_GC, 1);
    active_gs  = property_get_int32(NX_MMA_ACT_GS, 1);
    active_lmk = property_get_int32(NX_MMA_ACT_LMK, 1);

    ALOGI("%s: launching, gpx-grow: %u, gpx-shrink: %u, active-gc: %c, active-gs: %c, active-lmk: %c",
          __FUNCTION__, gfx_heap_grow_size, gfx_heap_shrink_threshold,
          active_gc ? 'o' : 'x',
          active_gs ? 'o' : 'x',
          active_lmk ? 'o' : 'x');

    do
    {
       BKNI_WaitForEvent(nx_server->proactive_runner.runner_run, BKNI_INFINITE);

       /* the proactive runner can be used for several purposes, but primarely
        * meant for memory management monitoring:
        *
        * 1) proactive dynamic heap allocation to ensure sufficient head room to avoid
        *    last minute application requests if possible.
        *
        * 2) proactive dynamic heap shrink to free up memory not actually used by the
        *    application but which client is still alive (default shrink only runs on
        *    nexus client disconnect).
        *
        * 3) proactive LMK on behalf of android for nexus managed memory not seen in
        *    standard LMK.
        */

        if (++lmk_tick > RUNNER_LMK_THRESHOLD) {
           NEXUS_InterfaceName interfaceName;
           size_t num, queried;
           NEXUS_PlatformObjectInstance *objects = NULL;
           NEXUS_Error nrc;
           unsigned client_allocation_size;

           if (!active_lmk) {
              goto skip_lmk;
           }

           strcpy(interfaceName.name, "NEXUS_MemoryBlock");
           /* find memory allocation for all registered clients. */
           if (BKNI_AcquireMutex(g_app.clients_lock) == BERR_SUCCESS) {
              for (i = 0; i < APP_MAX_CLIENTS; i++) {
                 client_allocation_size = 0;
                 if (g_app.clients[i].client) {
                    num = NUM_NX_OBJS;
                    do {
                       queried = num;
                       if (objects != NULL) {
                          BKNI_Free(objects);
                          objects = NULL;
                       }
                       objects = (NEXUS_PlatformObjectInstance *)BKNI_Malloc(num*sizeof(NEXUS_PlatformObjectInstance));
                       if (objects == NULL) {
                          num = 0;
                          nrc = NEXUS_SUCCESS;
                       }
                       nrc = NEXUS_Platform_GetClientObjects(g_app.clients[i].client->nexusClient, &interfaceName, objects, num, &num);
                       if (nrc == NEXUS_PLATFORM_ERR_OVERFLOW) {
                          num = 2 * queried;
                          if (num > MAX_NX_OBJS) {
                             num = 0;
                             nrc = NEXUS_SUCCESS;
                          }
                       }
                    } while (nrc == NEXUS_PLATFORM_ERR_OVERFLOW);
                    for (j = 0; j < (int)num; j++) {
                       NEXUS_MemoryBlockProperties prop;
                       NEXUS_MemoryBlock_GetProperties((NEXUS_MemoryBlockHandle)objects[j].object, &prop);
                       client_allocation_size += prop.size;
                    }
                    if (objects != NULL) {
                       BKNI_Free(objects);
                       objects = NULL;
                    }
                 }
                 if (g_app.clients[i].client && client_allocation_size) {
                    ALOGI("lmk-runner(%d): '%s'::%p -> %u bytes", i, g_app.clients[i].joinSettings.name, g_app.clients[i].client, client_allocation_size);
                 }
              }
              BKNI_ReleaseMutex(g_app.clients_lock);
           }

           /* sort clients via oom_adj score and select kill. */
           lmk_tick = 0;
        }

skip_lmk:
        if (nx_server->uses_mma && gfx_heap_grow_size) {
           NEXUS_PlatformConfiguration platformConfig;
           NEXUS_MemoryStatus heapStatus;
           NEXUS_Platform_GetConfiguration(&platformConfig);
           NEXUS_Heap_GetStatus(platformConfig.heap[NEXUS_MAX_HEAPS-2], &heapStatus);

           ALOGV("%s: dyn-heap largest free = %u", __FUNCTION__, heapStatus.largestFreeBlock);
           needs_growth = false;
           if (heapStatus.largestFreeBlock < NX_HEAP_DYN_FREE_THRESHOLD) {
              needs_growth = true;
           }

           if (active_gc) {
              if (++gc_tick > RUNNER_GC_THRESHOLD) {
                 gc_tick = 0;
                 if (!needs_growth) {
                    NEXUS_Platform_ShrinkHeap(platformConfig.heap[NEXUS_MAX_HEAPS-2], (size_t)gfx_heap_grow_size, (size_t)gfx_heap_shrink_threshold);
                 }
              }
           }

           if (active_gs && needs_growth) {
              ALOGI("%s: proactive allocation %u", __FUNCTION__, gfx_heap_grow_size);
              NEXUS_Platform_GrowHeap(platformConfig.heap[NEXUS_MAX_HEAPS-2], (size_t)gfx_heap_grow_size);
           }
        }

    } while(nx_server->proactive_runner.running);

done:
    return NULL;
}

static int client_connect(nxclient_t client, const NxClient_JoinSettings *pJoinSettings, NEXUS_ClientSettings *pClientSettings)
{
    unsigned i;
    BSTD_UNUSED(pClientSettings);
    if (BKNI_AcquireMutex(g_app.clients_lock) != BERR_SUCCESS) {
        ALOGE("failed to add client %p", client);
        goto out;
    }
    for (i = 0; i < APP_MAX_CLIENTS; i++) {
        if (!g_app.clients[i].client) {
            g_app.clients[i].client = client;
            g_app.clients[i].joinSettings = *pJoinSettings;
            ALOGI("client_connect(%d): '%s'::%p", i, g_app.clients[i].joinSettings.name, g_app.clients[i].client);
            break;
        }
    }
    BKNI_ReleaseMutex(g_app.clients_lock);
out:
    return 0;
}

static void client_disconnect(nxclient_t client, const NxClient_JoinSettings *pJoinSettings)
{
    unsigned i;
    BSTD_UNUSED(pJoinSettings);
    if (BKNI_AcquireMutex(g_app.clients_lock) != BERR_SUCCESS) {
        ALOGE("failed to remove client %p", client);
        goto out;
    }
    for (i=0;i<APP_MAX_CLIENTS;i++) {
        if (g_app.clients[i].client == client) {
            ALOGI("client_disconnect(%d): '%s'::%p", i, g_app.clients[i].joinSettings.name, g_app.clients[i].client);
            g_app.clients[i].client = NULL;
            break;
        }
    }
    BKNI_ReleaseMutex(g_app.clients_lock);
out:
    return;
}

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

static void trim_mem_config(NEXUS_MemoryConfigurationSettings *pMemConfigSettings)
{
   int i, j;
   char value[PROPERTY_VALUE_MAX];
   int dec_used = 0;

   /* 1. additional display(s). */
   if (property_get(NX_TRIM_DISP, value, NULL)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         /* start index -> 1 */
         for (i = 1; i < NEXUS_MAX_DISPLAYS; i++) {
            pMemConfigSettings->display[i].maxFormat = NEXUS_VideoFormat_eUnknown;
            for (j = 0; j < NEXUS_MAX_VIDEO_WINDOWS; j++) {
               pMemConfigSettings->display[i].window[j].used = false;
            }
         }
      }
   }

   /* 2. video input. */
   if (property_get(NX_TRIM_VIDIN, value, NULL)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         pMemConfigSettings->videoInputs.hdDvi = false;
         pMemConfigSettings->videoInputs.ccir656 = false;
      }
   }

   /* 3. *** HARDCODE *** only request a single encoder. */
   for (i = 1; i < NEXUS_MAX_VIDEO_ENCODERS; i++) {
      pMemConfigSettings->videoEncoder[i].used = false;
   }

   /* 4. vc1 decoder. */
   if (property_get(NX_TRIM_VC1, value, NULL)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         for (i = 0; i < NEXUS_MAX_VIDEO_DECODERS; i++) {
            pMemConfigSettings->videoDecoder[i].supportedCodecs[NEXUS_VideoCodec_eVc1] = false;
            pMemConfigSettings->videoDecoder[i].supportedCodecs[NEXUS_VideoCodec_eVc1SimpleMain] = false;
         }
         pMemConfigSettings->stillDecoder[0].supportedCodecs[NEXUS_VideoCodec_eVc1] = false;
         pMemConfigSettings->stillDecoder[0].supportedCodecs[NEXUS_VideoCodec_eVc1SimpleMain] = false;
      }
   }

   /* 5. stills decoder. */
   if (property_get(NX_TRIM_STILLS, value, NULL)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         for (i = 0 ; i < NEXUS_MAX_STILL_DECODERS ; i++) {
            pMemConfigSettings->stillDecoder[i].used = false;
         }
      }
   }

   /* 6. mosaic video. */
   if (property_get(NX_TRIM_MOSAIC, value, NULL)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         pMemConfigSettings->videoDecoder[0].mosaic.maxNumber = 0;
         pMemConfigSettings->videoDecoder[0].mosaic.maxHeight = 0;
         pMemConfigSettings->videoDecoder[0].mosaic.maxWidth = 0;

      }
   }

   /* 7. pip. */
   for (i = 0; i < NEXUS_MAX_VIDEO_DECODERS; i++) {
      if (pMemConfigSettings->videoDecoder[i].used) {
         ++dec_used;
      }
   }
   if (property_get(NX_TRIM_PIP, value, NULL)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         if (dec_used > MIN_PLATFORM_DEC) {
            pMemConfigSettings->videoDecoder[1].used = false;
         }
         pMemConfigSettings->display[0].window[1].used = false;
      }
   }

   /* 8. *** TEMPORARY *** force lowest format for mandated transcode decoder until
    *    we can instantiate an encoder without decoder back-end (architectural change).
    */
   if (property_get(NX_TRIM_MINFMT, value, NULL)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         /* start index -> 1.  beware interaction with pip above. */
         for (i = 1; i < NEXUS_MAX_VIDEO_DECODERS; i++) {
            if (pMemConfigSettings->videoDecoder[i].used) {
               pMemConfigSettings->videoDecoder[i].maxFormat = NEXUS_VideoFormat_eNtsc;
            }
         }
      }
   }

   /* 9. vp9. */
   if (property_get(NX_TRIM_VP9, value, NULL)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         for (i = 0; i < NEXUS_MAX_VIDEO_DECODERS; i++) {
            if (pMemConfigSettings->videoDecoder[i].used) {
               pMemConfigSettings->videoDecoder[i].supportedCodecs[NEXUS_VideoCodec_eVp9] = false;
            }
         }
         if (pMemConfigSettings->stillDecoder[0].used) {
            pMemConfigSettings->stillDecoder[0].supportedCodecs[NEXUS_VideoCodec_eVp9] = false;
         }
      }
   }

   /* 10. uhd decoder. */
   if (property_get(NX_TRIM_4KDEC, value, NULL)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         for (i = 0; i < NEXUS_MAX_VIDEO_DECODERS; i++) {
            if (pMemConfigSettings->videoDecoder[i].used &&
                (pMemConfigSettings->videoDecoder[i].maxFormat > NEXUS_VideoFormat_e1080p)) {
               pMemConfigSettings->videoDecoder[i].maxFormat = NEXUS_VideoFormat_e1080p;
            }
         }
      }
   }

   /* 11. color depth. */
   if (property_get(NX_TRIM_10BCOL, value, NULL)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         for (i = 0; i < NEXUS_MAX_VIDEO_DECODERS; i++) {
            if (pMemConfigSettings->videoDecoder[i].used) {
               pMemConfigSettings->videoDecoder[i].colorDepth = 8;
            }
         }
      }
   }
}

static NEXUS_VideoFormat forced_output_format(void)
{
   NEXUS_VideoFormat forced_format = NEXUS_VideoFormat_eUnknown;
   char value[PROPERTY_VALUE_MAX];
   char name[PROPERTY_VALUE_MAX];

   memset(value, 0, sizeof(value));
   sprintf(name, "persist.%s", NX_HD_OUT_FMT);
   if (property_get(name, value, "")) {
      if (strlen(value)) {
         forced_format = (NEXUS_VideoFormat)lookup(g_videoFormatStrs, value);
      }
   }

   if ((forced_format == NEXUS_VideoFormat_eUnknown) || (forced_format >= NEXUS_VideoFormat_eMax)) {
      memset(value, 0, sizeof(value));
      sprintf(name, "ro.%s", NX_HD_OUT_FMT);
      if (property_get(name, value, "")) {
         if (strlen(value)) {
            forced_format = (NEXUS_VideoFormat)lookup(g_videoFormatStrs, value);
         }
      }
   }

   return forced_format;
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
    NEXUS_VideoFormat forced_format;

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

    memset(value, 0, sizeof(value));
    if ( property_get(NX_AUDIO_LOUDNESS, value, "disabled") ) {
        if ( !strcmp(value, "atsc") ) {
            ALOGI("Enabling ATSC A/85 Loudness Equivalence");
            platformSettings.audioModuleSettings.loudnessMode = NEXUS_AudioLoudnessEquivalenceMode_eAtscA85;
        } else if ( !strcmp(value, "ebu") ) {
            ALOGI("Enabling EBU-R128 Loudness Equivalence");
            platformSettings.audioModuleSettings.loudnessMode = NEXUS_AudioLoudnessEquivalenceMode_eEbuR128;
        } else if ( strcmp(value, "disabled") ) {
            ALOGE("Unrecognized value '%s' for %s - expected disabled, atsc, or ebu", value, NX_AUDIO_LOUDNESS);
        }
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
    forced_format = forced_output_format();
    if ((forced_format != NEXUS_VideoFormat_eUnknown) && (forced_format < NEXUS_VideoFormat_eMax)) {
       /* -display_format XX */
       settings.display.format = forced_format;
       ALOGI("%s: display format = %s", __FUNCTION__, lookup_name(g_videoFormatStrs, settings.display.format));
       /* -ignore_edid */
       settings.display.hdmiPreferences.followPreferredFormat = false;
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

    settings.videoDecoder.dynamicPictureBuffers = property_get_int32(NX_ODV, 0) ? true : false;
    if (settings.videoDecoder.dynamicPictureBuffers) {
       unsigned d;
       for (d = 0; d < NEXUS_MAX_VIDEO_DECODERS; d++) {
          memConfigSettings.videoDecoder[d].dynamicPictureBuffers = true;
       }
       for (d = 0; d < NEXUS_MAX_HEAPS; d++) {
          if (platformSettings.heap[d].heapType & NEXUS_HEAP_TYPE_PICTURE_BUFFERS) {
             platformSettings.heap[d].memoryType = NEXUS_MEMORY_TYPE_MANAGED | NEXUS_MEMORY_TYPE_ONDEMAND_MAPPED;
          }
       }
    }

    if (property_get(NX_HEAP_HIGH_MEM, value, "0m")) {
       /* high-mem heap is used for 40 bits addressing. */
       int index = lookup_heap_memory_type(&platformSettings, NEXUS_MEMORY_TYPE_HIGH_MEMORY);
       if (strlen(value) && (index != -1)) {
          platformSettings.heap[index].size = calc_heap_size(value);
       }
    }

    if (property_get(NX_HEAP_DRV_MANAGED, value, NULL)) {
       /* driver-managed heap is used for encoder on some platforms only. */
       int index = lookup_heap_memory_type(&platformSettings,
          (NEXUS_MEMORY_TYPE_MANAGED|NEXUS_MEMORY_TYPE_DRIVER_UNCACHED|NEXUS_MEMORY_TYPE_DRIVER_CACHED|NEXUS_MEMORY_TYPE_APPLICATION_CACHED));
       if (strlen(value) && (index != -1)) {
          platformSettings.heap[index].size = calc_heap_size(value);
       }
    }

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
       if (index == -1) {
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

    BKNI_CreateMutex(&g_app.clients_lock);
    g_app.uses_mma = uses_mma;
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
    BKNI_DestroyMutex(g_app.clients_lock);
    NEXUS_Platform_Uninit();
}

int main(void)
{
    struct timespec t;
    int64_t now, then;
    struct stat sbuf;
    nxserver_t nx_srv = NULL;
    pthread_attr_t attr;
    bool mem_cfg = false;
    char device[PROPERTY_VALUE_MAX];
    char name[PROPERTY_VALUE_MAX];
    int memCfgFd = -1;

    memset(&g_app, 0, sizeof(g_app));

    const char *devName = getenv("NEXUS_DEVICE_NODE");
    if (!devName) {
        devName = "/dev/nexus";
    }

    /* delay until nexus device is present and writable */
    while (stat(devName, &sbuf) == -1 || !(sbuf.st_mode & S_IWOTH)) {
        ALOGW("waiting for %s device...\n", devName);
        sleep(1);
    }

    /* Setup logger environment */
    int32_t loggerDisabled;
    loggerDisabled = property_get_int32(NX_LOGGER_DISABLED, 0);
    if ( loggerDisabled ) {
        setenv("nexus_logger", "disabled", 1);
    } else {
        setenv("nexus_logger", "/system/bin/nxlogger", 1);
        setenv("nexus_logger_file", "/data/nexus/nexus.log", 1);
    }
    char loggerSize[PROPERTY_VALUE_MAX];
    if ( property_get(NX_LOGGER_SIZE, loggerSize, "0") && loggerSize[0] != '0' )
    {
        setenv("debug_log_size", loggerSize, 1);
    }
    if ( property_get_int32(NX_AUDIO_LOG, 0) ) {
        ALOGD("Enabling audio DSP logs to /data/nexus");
        setenv("audio_uart_file", "/data/nexus/audio_uart", 1);
        setenv("audio_debug_file", "/data/nexus/audio_debug", 1);
        setenv("audio_core_file", "/data/nexus/audio_core", 1);
    }

    ALOGI("init nxserver - nexus side.");
    nx_srv = init_nxserver();
    if (nx_srv == NULL) {
        ALOGE("FATAL: Daemonise Failed!");
        _exit(1);
    }

    ALOGI("starting proactive runner.");
    BKNI_CreateEvent(&g_app.proactive_runner.runner_run);
    g_app.proactive_runner.running = 1;
    pthread_attr_init(&attr);
    if (pthread_create(&g_app.proactive_runner.runner, &attr,
                       proactive_runner_task, (void *)&g_app) != 0) {
        ALOGE("failed proactive runner start, ignoring...");
        g_app.proactive_runner.running = 0;
    }
    pthread_attr_destroy(&attr);

    ALOGI("starting binder-ipc.");
    g_app.binder.running = 1;
    pthread_attr_init(&attr);
    if (pthread_create(&g_app.binder.runner, &attr,
                       binder_task, (void *)&g_app) != 0) {
        ALOGE("failed binder start, ignoring...");
        g_app.binder.running = 0;
    }
    pthread_attr_destroy(&attr);

    ALOGI("trigger nexus waiters now.");
    property_set("hw.nexus.platforminit", "on");

    struct nx_ashmem_mgr_cfg ashmem_mgr_cfg;
    memset(&ashmem_mgr_cfg, 0, sizeof(struct nx_ashmem_mgr_cfg));
    if (property_get_int32(NX_ODV, 0)) {
       property_get(NX_ODV_ALT_THRESHOLD, device, "0m");
       ashmem_mgr_cfg.alt_use_threshold = calc_heap_size(device);
       ashmem_mgr_cfg.alt_use_max[0] = property_get_int32(NX_ODV_ALT_1_USAGE, -1);
       ashmem_mgr_cfg.alt_use_max[1] = property_get_int32(NX_ODV_ALT_2_USAGE, -1);
    }

    property_get("ro.nexus.ashmem.devname", device, NULL);
    if (strlen(device)) {
       strcpy(name, "/dev/");
       strcat(name, device);
    } else {
       ALOGE("failed to find memory manager - some features will not work!");
       mem_cfg = true;
    }

    while (mem_cfg == false) {
       memCfgFd = open(name, O_RDWR, 0);
       if (memCfgFd >= 0) {
          int ret = ioctl(memCfgFd, NX_ASHMEM_MGR_CFG, &ashmem_mgr_cfg);
          if (ret < 0) {
             ALOGE("failed to configure memory manager - some features will not work!");
          }
          close(memCfgFd);
          mem_cfg = true;
       } else {
          ALOGI("waiting for memory manager availability...");
          BKNI_Sleep(100);
          continue;
       }
    }

    ALOGI("init done.");
    while (1) {
       BKNI_Sleep(SEC_TO_MSEC * RUNNER_SEC_THRESHOLD);
       BKNI_SetEvent(g_app.proactive_runner.runner_run);
       if (g_exit) break;
    }

    ALOGI("terminating nxserver.");
    if (g_app.proactive_runner.running) {
       g_app.proactive_runner.running = 0;
       pthread_join(g_app.proactive_runner.runner, NULL);
    }
    BKNI_DestroyEvent(g_app.proactive_runner.runner_run);

    if (g_app.binder.running) {
       g_app.binder.running = 0;
       pthread_join(g_app.binder.runner, NULL);
    }

    uninit_nxserver(nx_srv);
    return 0;
}
