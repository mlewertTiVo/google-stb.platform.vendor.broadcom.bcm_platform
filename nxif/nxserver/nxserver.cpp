/******************************************************************************
 *    (c) 2015-2017 Broadcom
 *
 * This program is the proprietary software of Broadcom and/or its licensors,
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
#include <sched.h>
#include <sys/resource.h>
#include <cutils/sched_policy.h>
#include <inttypes.h>
#include <sys/prctl.h>
#include <semaphore.h>
#include <linux/watchdog.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>

#include "nxserver.h"
#include "nxclient.h"
#include "nxserverlib.h"
#include "nxserverlib_impl.h"
#include "namevalue.h"
#include "nx_ashmem.h"

#include "nexus_base_mmap.h"
#include "nexus_watchdog.h"

#include "PmLibService.h"
#include "NxServer.h"
#include "nxwrap_common.h"

#define NEXUS_TRUSTED_DATA_PATH        "/data/misc/nexus"
#define NEXUS_LOGGER_DATA_PATH         "disabled" // Disable logger use of filesystem
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
#define NSC_FB_NUMBER                  (3)

#define GRAPHICS_RES_WIDTH_DEFAULT     (1920)
#define GRAPHICS_RES_HEIGHT_DEFAULT    (1080)
#define GRAPHICS_RES_WIDTH_PROP        "ro.nx.hwc2.nfb.w"
#define GRAPHICS_RES_HEIGHT_PROP       "ro.nx.hwc2.nfb.h"

#define NX_ACT_GC                      "ro.nx.act.gc"
#define NX_ACT_GS                      "ro.nx.act.gs"
#define NX_ACT_LMK                     "ro.nx.act.lmk"
#define NX_ACT_WD                      "ro.nx.act.wd"
#define NX_MMA_GROW_SIZE               "ro.nx.heap.grow"
#define NX_MMA_SHRINK_THRESHOLD        "ro.nx.heap.shrink"
#define NX_MMA_SHRINK_THRESHOLD_DEF    "2m"
#define NX_AUDIO_LOUDNESS              "ro.nx.audio_loudness"
#define NX_CAPABLE_COMP_BYPASS         "ro.nx.capable.cb"
#define NX_COMP_VIDEO                  "ro.nx.cvbs"
#define NX_CAPABLE_FRONT_END           "ro.nx.capable.fe"
#define NX_NO_OUTPUT_VIDEO             "ro.nx.output.dis"
#define NX_WD_TIMEOUT                  "ro.nx.wd.timeout"
#define NX_WD_TIMEOUT_DEF              60
#define NX_CAPABLE_DTU                 "ro.nx.capable.dtu"
#define NX_DOLBY_MS                    "ro.nx.dolby.ms"

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
#define NX_HEAP_XRR                    "ro.nx.heap.export"
#define NX_HEAP_GROW                   "ro.nx.heap.grow"
#define NX_SVP                         "ro.nx.svp"
#define NX_SPLASH                      "ro.nx.splash"
#define NX_HDCP_MODE                   "ro.nx.hdcp.mode"

#define NX_HD_OUT_FMT                  "nx.vidout.force" /* needs prefixing. */
#define NX_HDCP1X_KEY                  "ro.nexus.nxserver.hdcp1x_keys"
#define NX_HDCP2X_KEY                  "ro.nexus.nxserver.hdcp2x_keys"

#define NX_LOGGER_DISABLED             "ro.nx.logger_disabled"
#define NX_LOGGER_SIZE                 "ro.nx.logger_size"
#define NX_AUDIO_LOG                   "ro.nx.audio_log"

#define NX_HEAP_DYN_FREE_THRESHOLD     (1920*1080*4) /* one 1080p RGBA. */

#define NX_PROP_ENABLED                "1"
#define NX_PROP_DISABLED               "0"
#define NX_INVALID                     -1

#define NX_ANDROID_BOOTCOMPLETE        "sys.boot_completed"
#define NX_STATE                       "dyn.nx.state"

/* begnine trimming config - not needed for ATV experience - default ENABLED. */
#define NX_TRIM_VC1                    "ro.nx.trim.vc1"
#define NX_TRIM_PIP                    "ro.nx.trim.pip"
#define NX_TRIM_MOSAIC                 "ro.nx.trim.mosaic"
#define NX_TRIM_STILLS                 "ro.nx.trim.stills"
#define NX_TRIM_MINFMT                 "ro.nx.trim.minfmt"
#define NX_TRIM_DISP                   "ro.nx.trim.disp"
#define NX_TRIM_VIDIN                  "ro.nx.trim.vidin"
#define NX_TRIM_MTG                    "ro.nx.trim.mtg"
#define NX_TRIM_HDMIIN                 "ro.nx.trim.hdmiin"
/* destructive trimming config - feature set limitation - default DISABLED. */
#define NX_TRIM_VP9                    "ro.nx.trim.vp9"
#define NX_TRIM_4KDEC                  "ro.nx.trim.4kdec"
#define NX_TRIM_10BCOL                 "ro.nx.trim.10bcol"

#define NX_HWC2_FBCOMP                 "ro.nx.hwc2.tweak.fbcomp"

typedef enum {
   SVP_MODE_NONE,
   SVP_MODE_NONE_TRANSCODE,
   SVP_MODE_PLAYBACK,
   SVP_MODE_PLAYBACK_TRANSCODE,
   SVP_MODE_DTU,
   SVP_MODE_DTU_TRANSCODE,
} SVP_MODE_T;

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
   pthread_t catcher;
   int running;
   /* use native sem_t as nexus may not be available. */
   sem_t catcher_run;

} CATCHER_T;

typedef struct {
    int fd;
    NEXUS_WatchdogCallbackHandle nx;
    bool inStandby;
    BKNI_MutexHandle lock;
    bool init;
    bool want;
} WDOG_T;

typedef struct {
    BKNI_MutexHandle lock;
    nxserver_t server;
    BINDER_T binder;
    RUNNER_T proactive_runner;
    BINDER_T standby_monitor;
    unsigned refcnt;
    BKNI_MutexHandle clients_lock;
    unsigned standbyId;
    BKNI_MutexHandle standby_lock;
    NxClient_StandbyStatus standbyState;
    int dcma_index;
    struct {
        nxclient_t client;
        NxClient_JoinSettings joinSettings;
    } clients[APP_MAX_CLIENTS];
    WDOG_T wdog;
    CATCHER_T sigterm;
    NxServer *nxb;
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

static const char WATCHDOG_TERMINATE[] = "V";
static const char WATCHDOG_KICK[]      = "\0";
static void watchdogWrite(const char *msg)
{
    int ret, retries = 3;

    do {
        ret = write(g_app.wdog.fd, msg, 1);
        if (ret != 1) {
            ALOGE("could not write to watchdog, retrying...");
        } else {
            break;
        }
    } while(retries--);

    if (retries <= 0 && ret != 1) {
        ALOGE("watchdog write failed, platform will reboot!!!");
    }
}

static void nx_wdog_midpoint(void *context, int param)
{
   NX_SERVER_T *nx_server = (NX_SERVER_T *)context;
   BSTD_UNUSED(param);

   if (BKNI_AcquireMutex(nx_server->wdog.lock) == BERR_SUCCESS) {
      NEXUS_Watchdog_StartTimer();
      BKNI_ReleaseMutex(nx_server->wdog.lock);
   }
}

static void *binder_task(void *argv)
{
    NX_SERVER_T *nx_server = (NX_SERVER_T *)argv;

    prctl(PR_SET_NAME, "nxserver.binder");

    do {
       android::ProcessState::self()->startThreadPool();
       nx_server->nxb = NxServer::instantiate();
       PmLibService::instantiate();
       android::IPCThreadState::self()->joinThreadPool();

    } while(nx_server->binder.running);

done:
    return NULL;
}

static void *standby_monitor_task(void *argv)
{
    NX_SERVER_T *nx_server = (NX_SERVER_T *)argv;
    NxClient_StandbyStatus prevStatus;
    NEXUS_Error rc;

    prctl(PR_SET_NAME, "nxserver.standby");

    nx_server->standbyId = NxClient_RegisterAcknowledgeStandby();
    rc = NxClient_GetStandbyStatus(&prevStatus);
    ALOG_ASSERT(!rc);

    do {
       if (BKNI_AcquireMutex(nx_server->standby_lock) == BERR_SUCCESS) {
          rc = NxClient_GetStandbyStatus(&nx_server->standbyState);
          if (rc == NEXUS_SUCCESS) {
             if (nx_server->standbyState.transition == NxClient_StandbyTransition_eAckNeeded) {
                ALOGD("nxserver: ack state %d\n", nx_server->standbyState.settings.mode);
                NxClient_AcknowledgeStandby(nx_server->standbyId);
                if (nx_server->wdog.nx != NULL) {
                   NEXUS_Watchdog_StopTimer();
                   nx_server->wdog.inStandby = true;
                }
             }
             else if (nx_server->standbyState.settings.mode == NEXUS_PlatformStandbyMode_eOn &&
                      prevStatus.settings.mode != NEXUS_PlatformStandbyMode_eOn &&
                      nx_server->wdog.nx != NULL) {
                if (BKNI_AcquireMutex(nx_server->wdog.lock) == BERR_SUCCESS) {
                   nx_server->wdog.inStandby = false;
                   NEXUS_Watchdog_StartTimer();
                   BKNI_ReleaseMutex(nx_server->wdog.lock);
                }
            }
            prevStatus = nx_server->standbyState;
          }
          BKNI_ReleaseMutex(nx_server->standby_lock);
       }
       BKNI_Sleep(NXCLIENT_STANDBY_MONITOR_TIMEOUT_IN_MS);

    } while(nx_server->standby_monitor.running);

    NxClient_UnregisterAcknowledgeStandby(nx_server->standbyId);
    return NULL;
}

static void *sigterm_catcher_task(void *argv)
{
    NX_SERVER_T *nx_server = (NX_SERVER_T *)argv;

    prctl(PR_SET_NAME, "nxserver.sigterm");

    set_sched_policy(0, SP_BACKGROUND);
    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_BACKGROUND);

    do
    {
       sem_wait(&nx_server->sigterm.catcher_run);

       const char *logger = getenv("nexus_logger_file");
       ALOGW("must close \'%s\'", logger);

    } while(nx_server->sigterm.running);

done:
    return NULL;
}

static void *proactive_runner_task(void *argv)
{
    NX_SERVER_T *nx_server = (NX_SERVER_T *)argv;
    unsigned gfx_heap_grow_size = 0;
    unsigned gfx_heap_shrink_threshold = 0;
    int active_gc, active_lmk, active_gs, active_wd;
    int gc_tick = 0;
    int lmk_tick = 0;
    char value[PROPERTY_VALUE_MAX];
    bool needs_growth;
    int i, j;

    prctl(PR_SET_NAME, "nxserver.proac");

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
    active_gc  = property_get_int32(NX_ACT_GC,  1);
    active_gs  = property_get_int32(NX_ACT_GS,  1);
    active_lmk = property_get_int32(NX_ACT_LMK, 0);
    active_wd  = property_get_int32(NX_ACT_WD,  1);

    ALOGI("%s: launching, gpx-grow: %u, gpx-shrink: %u, active-gc: %c, active-gs: %c, active-lmk: %c, active-wd: %c",
          __FUNCTION__, gfx_heap_grow_size, gfx_heap_shrink_threshold,
          active_gc ? 'o' : 'x',
          active_gs ? 'o' : 'x',
          active_lmk ? 'o' : 'x',
          active_wd ? 'o' : 'x');

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
        *
        * 4) kick the platform's reset watchdog timer. The watchdog can be kicked by
        *    writing any single character to the watchdog device. On a normal exit, the
        *    magic letter 'V' must be written to the watchdog before closing the file,
        *    which will stop the device. If not kicked within 30 seconds, the watchdog
        *    will expire and trigger a system reset.
        *
        */

        if (++lmk_tick > RUNNER_LMK_THRESHOLD) {
           if (!active_lmk) {
              goto skip_lmk;
           }
        }
skip_lmk:
        if (gfx_heap_grow_size) {
           if (BKNI_AcquireMutex(g_app.standby_lock) == BERR_SUCCESS) {
              if (g_app.standbyState.settings.mode == NEXUS_PlatformStandbyMode_eOn) {
                 NEXUS_PlatformConfiguration platformConfig;
                 NEXUS_MemoryStatus heapStatus;
                 NEXUS_Platform_GetConfiguration(&platformConfig);
                 NEXUS_Heap_GetStatus(platformConfig.heap[g_app.dcma_index], &heapStatus);

                 ALOGV("%s: dyn-heap largest free = %u", __FUNCTION__, heapStatus.largestFreeBlock);
                 needs_growth = false;
                 if (heapStatus.largestFreeBlock < NX_HEAP_DYN_FREE_THRESHOLD) {
                    needs_growth = true;
                 }

                 if (active_gc) {
                    if (++gc_tick > RUNNER_GC_THRESHOLD) {
                       gc_tick = 0;
                       if (!needs_growth) {
                          NEXUS_Platform_ShrinkHeap(
                             platformConfig.heap[g_app.dcma_index],
                             (size_t)gfx_heap_grow_size,
                             (size_t)gfx_heap_shrink_threshold);
                       }
                    }
                 }

                 if (active_gs && needs_growth) {
                    ALOGI("%s: proactive allocation %u", __FUNCTION__, gfx_heap_grow_size);
                    NEXUS_Platform_GrowHeap(
                       platformConfig.heap[g_app.dcma_index],
                       (size_t)gfx_heap_grow_size);
                 }
              }
              BKNI_ReleaseMutex(g_app.standby_lock);
           }
        }

        if (!g_app.wdog.init) {
           if (property_get(NX_ANDROID_BOOTCOMPLETE, value, NULL)) {
              if (strlen(value) && !strncmp(value, NX_PROP_ENABLED, strlen(value))) {
                 if (g_app.wdog.want) {
                    int wdog_timeout = property_get_int32(NX_WD_TIMEOUT, NX_WD_TIMEOUT_DEF);
                    g_app.wdog.fd = open("/dev/watchdog", O_WRONLY);
                    if (g_app.wdog.fd >= 0 && wdog_timeout) {
                       ALOGI("sys.boot_completed detected, launching wdog processing (to=%ds)", wdog_timeout);
                       NEXUS_Error rc;
                       NEXUS_WatchdogCallbackSettings wdogSettings;
                       watchdogWrite(WATCHDOG_TERMINATE);
                       NEXUS_WatchdogCallback_GetDefaultSettings(&wdogSettings);
                       wdogSettings.midpointCallback.callback = nx_wdog_midpoint;
                       wdogSettings.midpointCallback.context = (void *)&g_app;
                       g_app.wdog.nx = NEXUS_WatchdogCallback_Create(&wdogSettings);
                       g_app.wdog.inStandby = false;
                       NEXUS_Watchdog_SetTimeout(wdog_timeout);
                       rc = NEXUS_Watchdog_StartTimer();
                       if (rc != NEXUS_SUCCESS) {
                          NEXUS_WatchdogCallback_Destroy(g_app.wdog.nx);
                          g_app.wdog.nx = NULL;
                          ALOGE("unable to create nexus watchdog support (reason:%d)!", rc);
                          watchdogWrite(WATCHDOG_TERMINATE);
                          close(g_app.wdog.fd);
                          g_app.wdog.fd = NX_INVALID;
                       }
                       g_app.wdog.init = true;
                    } else if (g_app.wdog.fd >= 0 && !wdog_timeout) {
                       ALOGI("wdog disabled on sys.boot_completed");
                       watchdogWrite(WATCHDOG_TERMINATE);
                       close(g_app.wdog.fd);
                       g_app.wdog.fd = NX_INVALID;
                       g_app.wdog.init = true;
                    } else {
                       ALOGE("unable to create watchdog support (reason:%s)!", strerror(errno));
                       g_app.wdog.fd = NX_INVALID;
                       g_app.wdog.nx = NULL;
                       g_app.wdog.want = false;
                    }
                 } else {
                    g_app.wdog.fd = open("/dev/watchdog", O_WRONLY);
                    if (g_app.wdog.fd >= 0) {
                       watchdogWrite(WATCHDOG_TERMINATE);
                       close(g_app.wdog.fd);
                       g_app.wdog.fd = NX_INVALID;
                    }
                    g_app.wdog.init = true;
                 }
              }
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
        ALOGE("failed to add nx-client %p", client);
        goto out;
    }
    for (i = 0; i < APP_MAX_CLIENTS; i++) {
        if (g_app.clients[i].client &&
            g_app.clients[i].client == client) {
            ALOGW("nx-RE.connect(%d): '%s'::%p::%d", i,
                  g_app.clients[i].joinSettings.name,
                  g_app.clients[i].client,
                  g_app.clients[i].joinSettings.mode);
            goto out_lock;
        }
    }
    for (i = 0; i < APP_MAX_CLIENTS; i++) {
        if (!g_app.clients[i].client) {
            g_app.clients[i].client = client;
            g_app.clients[i].joinSettings = *pJoinSettings;
            ALOGV("nx-connect(%d): '%s'::%p::%d", i,
                  g_app.clients[i].joinSettings.name,
                  g_app.clients[i].client,
                  g_app.clients[i].joinSettings.mode);
            break;
        }
    }
out_lock:
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
            ALOGV("nx-disconnect(%d): '%s'::%p", i, g_app.clients[i].joinSettings.name, g_app.clients[i].client);
            g_app.clients[i].client = NULL;
            break;
        }
    }
    BKNI_ReleaseMutex(g_app.clients_lock);
out:
    return;
}

static int lookup_heap_type(const NEXUS_PlatformSettings *pPlatformSettings, unsigned heapType, bool nullsized = false)
{
    unsigned i;
    for (i=0;i<NEXUS_MAX_HEAPS;i++) {
        if (nullsized && (pPlatformSettings->heap[i].heapType & heapType)) return i;
        if (!nullsized && pPlatformSettings->heap[i].size && (pPlatformSettings->heap[i].heapType & heapType)) return i;
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

static void pre_trim_mem_config(NEXUS_MemoryConfigurationSettings *pMemConfigSettings, bool cvbs)
{
   if (!cvbs) {
      pMemConfigSettings->display[1].maxFormat = NEXUS_VideoFormat_eUnknown;
   }
}

static void trim_mem_config(NEXUS_MemoryConfigurationSettings *pMemConfigSettings, SVP_MODE_T svp)
{
   int i, j;
   char value[PROPERTY_VALUE_MAX];
   int dec_used = 0;
   NEXUS_PlatformCapabilities platformCap;
   bool transcode = false;
   bool cvbs = property_get_int32(NX_COMP_VIDEO, 0) ? true : false;
   NEXUS_GetPlatformCapabilities(&platformCap);
   bool pip = false;

   if ((svp == SVP_MODE_PLAYBACK_TRANSCODE) ||
       (svp == SVP_MODE_NONE_TRANSCODE) ||
       (svp == SVP_MODE_DTU_TRANSCODE)) {
      ALOGI("transcode is enabled.");
      transcode = true;
   }

   /* 1. encoder configuration. */
   trim_encoder_mem_config(pMemConfigSettings);

   /* 2. additional display(s). */
   if (property_get(NX_TRIM_DISP, value, NX_PROP_ENABLED)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         /* start index -> 1 */
         for (i = 1; i < NEXUS_MAX_DISPLAYS; i++) {
            /* keep display associated with encoder(s) if transcode wanted. */
            if (keep_display_for_encoder(i, &platformCap)) {
               if (transcode) {
                  ALOGI("keeping display %d for transcode session on encoder", i);
                  for (j = 0; j < NEXUS_MAX_VIDEO_WINDOWS; j++) {
                     pMemConfigSettings->display[i].window[j].secure = NEXUS_SecureVideo_eUnsecure;
                  }
               } else {
                  ALOGI("encoder using display %d: disable display, windows and deinterlacer", i);
                  pMemConfigSettings->display[i].maxFormat = NEXUS_VideoFormat_eUnknown;
                  for (j = 0; j < NEXUS_MAX_VIDEO_WINDOWS; j++) {
                     pMemConfigSettings->display[i].window[j].deinterlacer = NEXUS_DeinterlacerMode_eNone;
                     pMemConfigSettings->display[i].window[j].used = false;
                  }
               }
            /* keep display 1 if composite video wanted, but remove all window except the first one. */
            } else if ((i == 1) && cvbs && platformCap.display[i].supported) {
               ALOGI("keeping display %d for composite video out", i);
               for (j = 1; j < NEXUS_MAX_VIDEO_WINDOWS; j++) {
                  pMemConfigSettings->display[i].window[j].used = false;
               }
            } else {
               pMemConfigSettings->display[i].maxFormat = NEXUS_VideoFormat_eUnknown;
               for (j = 0; j < NEXUS_MAX_VIDEO_WINDOWS; j++) {
                  pMemConfigSettings->display[i].window[j].used = false;
               }
            }
         }
      }
   }

   /* 3. mtg. */
   if (property_get(NX_TRIM_MTG, value, NX_PROP_ENABLED)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         for (i =0; i < NEXUS_MAX_DISPLAYS; i++) {
            for (j = 0; j < NEXUS_MAX_VIDEO_WINDOWS; j++) {
               pMemConfigSettings->display[i].window[j].mtg = false;
            }
         }
      }
   }

   /* 4. video input. */
   if (property_get(NX_TRIM_VIDIN, value, NX_PROP_ENABLED)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         pMemConfigSettings->videoInputs.ccir656 = false;
      }
   }
   if (property_get(NX_TRIM_HDMIIN, value, NX_PROP_ENABLED)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         pMemConfigSettings->videoInputs.hdDvi = false;
      }
   }

   /* 5. vc1 decoder. */
   if (property_get(NX_TRIM_VC1, value, NX_PROP_ENABLED)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         for (i = 0; i < NEXUS_MAX_VIDEO_DECODERS; i++) {
            pMemConfigSettings->videoDecoder[i].supportedCodecs[NEXUS_VideoCodec_eVc1] = false;
            pMemConfigSettings->videoDecoder[i].supportedCodecs[NEXUS_VideoCodec_eVc1SimpleMain] = false;
         }
         pMemConfigSettings->stillDecoder[0].supportedCodecs[NEXUS_VideoCodec_eVc1] = false;
         pMemConfigSettings->stillDecoder[0].supportedCodecs[NEXUS_VideoCodec_eVc1SimpleMain] = false;
      }
   }

   /* 6. stills decoder. */
   if (property_get(NX_TRIM_STILLS, value, NX_PROP_ENABLED)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         for (i = 0 ; i < NEXUS_MAX_STILL_DECODERS ; i++) {
            pMemConfigSettings->stillDecoder[i].used = false;
         }
      }
   }

   /* 7. mosaic video. */
   if (property_get(NX_TRIM_MOSAIC, value, NX_PROP_ENABLED)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         for (i = 0; i < NEXUS_MAX_VIDEO_DECODERS; i++) {
            pMemConfigSettings->videoDecoder[i].mosaic.maxNumber = 0;
            pMemConfigSettings->videoDecoder[i].mosaic.maxHeight = 0;
            pMemConfigSettings->videoDecoder[i].mosaic.maxWidth = 0;
         }
      }
   }

   /* 8. pip. */
   for (i = 0; i < NEXUS_MAX_VIDEO_DECODERS; i++) {
      if (pMemConfigSettings->videoDecoder[i].used) {
         ++dec_used;
      }
   }
   if (property_get(NX_TRIM_PIP, value, NX_PROP_ENABLED)) {
      if (strlen(value)) {
         if (strtoul(value, NULL, 0) > 0) {
            pip = false;
            if (!has_encoder() || (dec_used > MIN_PLATFORM_DEC)) {
               pMemConfigSettings->videoDecoder[1].used = false;
            }
            pMemConfigSettings->display[0].window[1].used = false;
         } else {
            pip = true;
         }
      }
   }

   if (transcode) {
      for (i = 1; i < NEXUS_MAX_VIDEO_DECODERS; i++) {
         if (pMemConfigSettings->videoDecoder[i].used) {
             pMemConfigSettings->videoDecoder[i].secure = NEXUS_SecureVideo_eUnsecure;
         }
      }
   } else if (!has_encoder()) {
      for (i = 1; i < NEXUS_MAX_VIDEO_DECODERS; i++) {
         /* start index -> 1.  beware interaction with pip above. */
         if (i == 1 && pip) {
            continue;
         }
         pMemConfigSettings->videoDecoder[i].used = false;
      }
   } else {
     /* 9. *** TEMPORARY *** force lowest format for mandated transcode decoder until
      *    we can instantiate an encoder without decoder back-end (architectural change).
      */
      if (property_get(NX_TRIM_MINFMT, value, NX_PROP_ENABLED)) {
         if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
            /* start index -> 1.  beware interaction with pip above. */
            for (i = 1; i < NEXUS_MAX_VIDEO_DECODERS; i++) {
               if (i == 1 && pip) {
                  continue;
               }
               if (pMemConfigSettings->videoDecoder[i].used) {
                  pMemConfigSettings->videoDecoder[i].maxFormat = NEXUS_VideoFormat_eNtsc;
               }
            }
         }
      }
   }

   /* 10. vp9. */
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

   /* 11. uhd decoder. */
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

   /* 12. color depth. */
   if (property_get(NX_TRIM_10BCOL, value, NULL)) {
      if (strlen(value) && (strtoul(value, NULL, 0) > 0)) {
         for (i = 0; i < NEXUS_MAX_VIDEO_DECODERS; i++) {
            if (pMemConfigSettings->videoDecoder[i].used) {
               pMemConfigSettings->videoDecoder[i].colorDepth = 8;
            }
         }
      }
   }

   /* 13. extra picture buffers for VP9 worst-case decode */
   for (i = 0; i < NEXUS_MAX_VIDEO_DECODERS; i++) {
      if ( pMemConfigSettings->videoDecoder[i].used && pMemConfigSettings->videoDecoder[i].supportedCodecs[NEXUS_VideoCodec_eVp9] ) {
         pMemConfigSettings->videoDecoder[i].extraPictureBuffers = 2;
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
    char key_hdcp1x[PROPERTY_VALUE_MAX];
    char key_hdcp2x[PROPERTY_VALUE_MAX];
    int ix, jx;
    char nx_key[PROPERTY_VALUE_MAX];
    FILE *key = NULL;
    NEXUS_VideoFormat forced_format;
    SVP_MODE_T svp;
    bool cvbs = property_get_int32(NX_COMP_VIDEO, 0) ? true : false;
    int32_t dolby = 0;

    if (g_app.refcnt == 1) {
        g_app.refcnt++;
        return g_app.server;
    }
    if (g_app.server) {
        return NULL;
    }

    memset(&cmdline_settings, 0, sizeof(cmdline_settings));
    cmdline_settings.frontend = property_get_bool(NX_CAPABLE_FRONT_END, 0);
    cmdline_settings.dtu = property_get_bool(NX_CAPABLE_DTU, 0);

    nxserver_get_default_settings(&settings);
    NEXUS_Platform_GetDefaultSettings(&platformSettings);
    NEXUS_GetDefaultMemoryConfigurationSettings(&memConfigSettings);

    platformSettings.videoDecoderModuleSettings.deferInit = property_get_bool(NX_SPLASH, 0);
    defer_init_encoder(&platformSettings, platformSettings.videoDecoderModuleSettings.deferInit);

    memset(value, 0, sizeof(value));
    if ( property_get(NX_AUDIO_LOUDNESS, value, "disabled") ) {
        if ( !strcmp(value, "atsc") ) {
            ALOGI("Enabling ATSC A/85 Loudness Equivalence");
            cmdline_settings.loudnessMode = NEXUS_AudioLoudnessEquivalenceMode_eAtscA85;
        } else if ( !strcmp(value, "ebu") ) {
            ALOGI("Enabling EBU-R128 Loudness Equivalence");
            cmdline_settings.loudnessMode = NEXUS_AudioLoudnessEquivalenceMode_eEbuR128;
        } else if ( strcmp(value, "disabled") ) {
            ALOGE("Unrecognized value '%s' for %s - expected disabled, atsc, or ebu", value, NX_AUDIO_LOUDNESS);
        }
    }

    dolby = property_get_int32(NX_DOLBY_MS,0);
    switch (dolby) {
    case 11:
       ALOGI("enabling dolby-ms11");
       settings.session[0].dolbyMs = nxserverlib_dolby_ms_type_ms11;
    break;
    case 12:
       ALOGI("enabling dolby-ms12");
       settings.session[0].dolbyMs = nxserverlib_dolby_ms_type_ms12;
       /* enable unconditionally when using MS12 until needed otherwise. */
       settings.session[0].persistentDecoderSupport = true;
    break;
    default:
    break;
    }

    /* setup the configuration we want for the device.  right now, hardcoded for a generic
       android device, longer term, we want more flexibility. */

    /* -ir none */
    settings.session[0].ir_input.mode[0] = NEXUS_IrInputMode_eMax;
    settings.session[0].ir_input.mode[1] = NEXUS_IrInputMode_eMax;
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
    settings.transcode = nxserver_settings::nxserver_transcode_off;
    /* -grab off */
    settings.grab = 0;
    /* -sd off (unless composite video enabled) */
    if (!cvbs) {
       settings.session[0].output.sd = false;
    } else {
       settings.session[0].output.sd = (property_get_int32(NX_NO_OUTPUT_VIDEO, 0) > 0) ? false : true;
    }
    settings.session[0].output.encode = false;
    settings.session[0].output.hd = (property_get_int32(NX_NO_OUTPUT_VIDEO, 0) > 0) ? false : true;
    /* -enablePassthroughBuffer */
    settings.audioDecoder.enablePassthroughBuffer = true;
    if (settings.session[0].audioPlaybacks > 0) {
       /* Reserve one for the decoder instead of playback */
       settings.session[0].audioPlaybacks--;
    }
    settings.display.hdmiPreferences.enabled = false;
    settings.display.componentPreferences.enabled = false;
    settings.display.compositePreferences.enabled = false;

    settings.allowCompositionBypass = property_get_int32(NX_CAPABLE_COMP_BYPASS, 0) ? true : false;
    if (cvbs) {
       settings.allowCompositionBypass = false;
    }
    settings.framebuffers = NSC_FB_NUMBER;
    settings.pixelFormat = property_get_bool(NX_HWC2_FBCOMP ,0) ?
       NEXUS_PixelFormat_eCompressed_A8_R8_G8_B8 : NEXUS_PixelFormat_eA8_R8_G8_B8;

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

    if (property_get(NX_HEAP_XRR, value, NULL)) {
       int index = lookup_heap_type(&platformSettings, NEXUS_HEAP_TYPE_EXPORT_REGION, true);
       if (strlen(value) && (index != -1)) {
          /* -heap export,XXy */
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

    if (property_get(NX_HEAP_GFX2, value, NULL)) {
       int index = lookup_heap_type(&platformSettings, NEXUS_HEAP_TYPE_SECONDARY_GRAPHICS);
       if (strlen(value) && (index != -1)) {
          /* -heap gfx2,XXy */
          platformSettings.heap[index].size = calc_heap_size(value);
       }
    }

    if (property_get(NX_HEAP_GROW, value, NULL)) {
       if (strlen(value)) {
          /* -growHeapBlockSize XXy */
          settings.growHeapBlockSize = calc_heap_size(value);
       }
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

    /* -hdcp1x_keys some-key-file */
    memset(key_hdcp1x, 0, sizeof(key_hdcp1x));
    property_get(NX_HDCP1X_KEY, key_hdcp1x, NULL);
    if (strlen(key_hdcp1x)) {
       strncpy(settings.hdcp.hdcp1xBinFile, key_hdcp1x, strlen(key_hdcp1x));
    }
    /* -hdcp2x_keys some-key-file */
    memset(key_hdcp2x, 0, sizeof(key_hdcp2x));
    property_get(NX_HDCP2X_KEY, key_hdcp2x, NULL);
    if (strlen(key_hdcp2x)) {
       strncpy(settings.hdcp.hdcp2xBinFile, key_hdcp2x, strlen(key_hdcp2x));
    }
    settings.hdcp.versionSelect = (NxClient_HdcpVersion)property_get_int32(NX_HDCP_MODE, 0);

    pre_trim_mem_config(&memConfigSettings, cvbs);

    /* svp configuration. */
    memset(value, 0, sizeof(value));
    if (cmdline_settings.dtu) {
       property_get(NX_SVP, value, "dtu");
       svp = SVP_MODE_DTU;
       if (strstr(value, "trans") != NULL) {
          sprintf(value, "dtu-trans");
          svp = SVP_MODE_DTU_TRANSCODE;
       }
    } else {
       property_get(NX_SVP, value, "play");
       svp = SVP_MODE_PLAYBACK;
       if (!strncmp(value, "none-trans", strlen("none-trans"))) {
          svp = SVP_MODE_NONE_TRANSCODE;
       } else if (!strncmp(value, "none", strlen("none"))) {
          svp = SVP_MODE_NONE;
       } else if (!strncmp(value, "play-trans", strlen("play-trans"))) {
          svp = SVP_MODE_PLAYBACK_TRANSCODE;
       }
    }
    if ((svp == SVP_MODE_DTU) || (svp == SVP_MODE_DTU_TRANSCODE)) {
       /* keep default, does not matter as [-dtu] option prevails. */
    } else if (!((svp == SVP_MODE_NONE) || (svp == SVP_MODE_NONE_TRANSCODE))) {
       settings.svp = nxserverlib_svp_type_cdb_urr;
    }
    ALOGI("%s: svp-mode: \'%s\' (%d)", __FUNCTION__, value, svp);

    rc = nxserver_modify_platform_settings(&settings, &cmdline_settings, &platformSettings, &memConfigSettings);
    if (rc) {
       ALOGE("FATAL: failed nxserver_modify_platform_settings");
       return NULL;
    }
    if (settings.growHeapBlockSize) {
       int index = lookup_heap_type(&platformSettings, NEXUS_HEAP_TYPE_GRAPHICS);
       g_app.dcma_index = settings.heaps.dynamicHeap;
       platformSettings.heap[g_app.dcma_index].heapType |= NX_ASHMEM_NEXUS_DCMA_MARKER;
       ALOGI("dynamic: %d (0x%x) (gfx: %d)", g_app.dcma_index,
          platformSettings.heap[g_app.dcma_index].heapType, index);
    }

    /* now, just before applying the configuration, try to reduce the memory footprint
     * allocated by nexus for the platform needs.
     *
     * savings are essentially made through feature disablement. thus you have to be careful
     * and aware of the feature targetted for the platform.
     */
    trim_mem_config(&memConfigSettings, svp);

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

    /* With MS12, SPDIF output needs to be clocked from different PLL to avoid conflict
     * with the DSP mixer.
     */
    if (settings.session[0].dolbyMs == nxserverlib_dolby_ms_type_ms12) {
       NEXUS_PlatformConfiguration platformConfig;
       NEXUS_AudioOutputSettings outputSettings;

       NEXUS_Platform_GetConfiguration(&platformConfig);
       NEXUS_AudioOutput_GetSettings(NEXUS_SpdifOutput_GetConnector(platformConfig.outputs.spdif[0]), &outputSettings);
       ALOGI("SPDIF PLL %d -> %d", outputSettings.pll, NEXUS_AudioOutputPll_e1);
       outputSettings.pll = NEXUS_AudioOutputPll_e1;
       NEXUS_AudioOutput_SetSettings(NEXUS_SpdifOutput_GetConnector(platformConfig.outputs.spdif[0]), &outputSettings);
    }

    rc = nxserver_ipc_init(g_app.server, g_app.lock);
    if (rc) {
       ALOGE("FATAL: failed nxserver_ipc_init");
       return NULL;
    }

    BKNI_CreateMutex(&g_app.clients_lock);
    BKNI_CreateMutex(&g_app.standby_lock);
    BKNI_CreateMutex(&g_app.wdog.lock);
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
    BKNI_DestroyMutex(g_app.standby_lock);
    BKNI_DestroyMutex(g_app.wdog.lock);
    NEXUS_Platform_Uninit();
}

static void sigterm_signal_handler(int signal) {
    (void)signal;
    sem_post(&g_app.sigterm.catcher_run);
}

static bool is_wakeup_only_from_alarm_timer()
{
    NEXUS_Error rc = NEXUS_SUCCESS;
    NxClient_StandbyStatus standbyStatus;

   rc = NxClient_GetStandbyStatus(&standbyStatus);
   return (rc==0 && standbyStatus.status.wakeupStatus.timeout &&
          !(standbyStatus.status.wakeupStatus.ir ||
            standbyStatus.status.wakeupStatus.uhf ||
            standbyStatus.status.wakeupStatus.keypad ||
            standbyStatus.status.wakeupStatus.gpio ||
            standbyStatus.status.wakeupStatus.cec ||
            standbyStatus.status.wakeupStatus.transport));
}

static int set_video_outputs_state(bool enabled)
{
    NEXUS_Error rc = NEXUS_SUCCESS;
    NxClient_DisplaySettings displaySettings;

    do {
        NxClient_GetDisplaySettings(&displaySettings);

        if (displaySettings.hdmiPreferences.enabled != enabled) {
            displaySettings.hdmiPreferences.enabled      = enabled;
            displaySettings.componentPreferences.enabled = enabled;
            displaySettings.compositePreferences.enabled = enabled;
            rc = NxClient_SetDisplaySettings(&displaySettings);
        }

        if (rc) {
            ALOGE("%s: Could not set display settings [rc=%d]!!!", __FUNCTION__, rc);
        }
        else {
            ALOGV("%s: %s Video outputs...", __FUNCTION__, enabled ? "Enabling" : "Disabling");
        }
    } while (rc == NXCLIENT_BAD_SEQUENCE_NUMBER);
    return rc;
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
    NEXUS_MemoryBlockHandle hSecDmaMemoryBlock = NULL;
    NxClient_JoinSettings joinSettings;
    NEXUS_Error rc;
    struct sched_param param;

    memset(&g_app, 0, sizeof(g_app));

    sem_init(&g_app.sigterm.catcher_run, 0, 0);
    signal(SIGTERM, sigterm_signal_handler);
    ALOGI("starting sigterm catcher.");
    pthread_attr_init(&attr);
    memset(&param, 0, sizeof(param));
    pthread_attr_setschedparam(&attr, &param);
    pthread_attr_setschedpolicy(&attr, SCHED_BATCH);
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) {
        ALOGW("failed sigterm catcher task attributes.");
    }
    g_app.sigterm.running = 1;
    if (pthread_create(&g_app.sigterm.catcher, &attr,
                       sigterm_catcher_task, (void *)&g_app) != 0) {
        ALOGE("failed sigterm catcher task, critical for encryption...");
        /* should abort here if encryption is enforced... tbd. */
        g_app.sigterm.running = 0;
    }
    pthread_attr_destroy(&attr);

    setpriority(PRIO_PROCESS, 0, PRIORITY_URGENT_DISPLAY);
    set_sched_policy(0, SP_FOREGROUND);

    property_set(NX_STATE, "init");

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
    if (loggerDisabled) {
        setenv("nexus_logger", "disabled", 1);
    } else {
        setenv("nexus_logger", "/vendor/bin/nxlogger", 1);
        setenv("nexus_logger_file", NEXUS_LOGGER_DATA_PATH, 1);
    }
    char loggerSize[PROPERTY_VALUE_MAX];
    if (property_get(NX_LOGGER_SIZE, loggerSize, "0") && loggerSize[0] != '0') {
        setenv("debug_log_size", loggerSize, 1);
    }
    if (property_get_int32(NX_AUDIO_LOG, 0)) {
        ALOGD("Enabling audio DSP logs to /data/nxmedia");
        setenv("audio_uart_file", "/data/nxmedia/audio_uart", 1);
        setenv("audio_debug_file", "/data/nxmedia/audio_debug", 1);
        setenv("audio_core_file", "/data/nxmedia/audio_core", 1);
    }

    ALOGI("init nxserver - nexus side.");
    nx_srv = init_nxserver();
    if (nx_srv == NULL) {
        ALOGE("FATAL: Daemonise Failed!");
        _exit(1);
    }

    if (property_get_int32(NX_ACT_WD, 1)) {
       g_app.wdog.want = true;
    } else {
       g_app.wdog.fd = NX_INVALID;
       g_app.wdog.nx = NULL;
       g_app.wdog.want = false;
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

    ALOGI("connecting ourselves.");
    NxClient_GetDefaultJoinSettings(&joinSettings);
    rc = NxClient_Join(&joinSettings);
    if (rc != NEXUS_SUCCESS) {
       ALOGE("failed to join the server!");
    } else {
       ALOGI("starting standby-monitor.");
       g_app.standby_monitor.running = 1;
       pthread_attr_init(&attr);
       if (pthread_create(&g_app.standby_monitor.runner, &attr,
                          standby_monitor_task, (void *)&g_app) != 0) {
          ALOGE("failed standby monitor start, ignoring...");
          g_app.standby_monitor.running = 0;
       }
       pthread_attr_destroy(&attr);
    }

    ALOGI("starting binder-ipc.");
    g_app.binder.running = 1;
    pthread_attr_init(&attr);
    if (pthread_create(&g_app.binder.runner, &attr,
                       binder_task, (void *)&g_app) != 0) {
        ALOGE("failed binder start, ignoring...");
        g_app.binder.running = 0;
    }
    pthread_attr_destroy(&attr);

    ALOGI("trigger memory configuration setup.");
    property_set(NX_STATE, "memcfg");

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

    if (!property_get_bool(NX_NO_OUTPUT_VIDEO, 0) && !is_wakeup_only_from_alarm_timer()) {
       rc = set_video_outputs_state(true);
       if (rc) {
          ALOGE("could not enable video outputs!");
       }
    }

    alloc_secdma(&hSecDmaMemoryBlock, g_app.server);

    ALOGI("trigger nexus waiters now.");
    property_set(NX_STATE, "loaded");

    ALOGI("init done.");
    while (1) {
       BKNI_Sleep(SEC_TO_MSEC * RUNNER_SEC_THRESHOLD);
       BKNI_SetEvent(g_app.proactive_runner.runner_run);
       if (g_exit) break;
    }

    if(hSecDmaMemoryBlock != NULL) {
       NEXUS_MemoryBlock_Unlock(hSecDmaMemoryBlock);
       NEXUS_MemoryBlock_Free(hSecDmaMemoryBlock);
    }

    ALOGI("terminating nxserver.");

    property_set(NX_STATE, "ended");

    if (g_app.wdog.nx) {
       NEXUS_Watchdog_StopTimer();
       NEXUS_WatchdogCallback_Destroy(g_app.wdog.nx);
       g_app.wdog.nx = NULL;
    }
    if (g_app.wdog.fd != NX_INVALID) {
        watchdogWrite(WATCHDOG_TERMINATE);
        close(g_app.wdog.fd);
    }
    g_app.wdog.init = false;
    g_app.wdog.want = false;

    if (g_app.standby_monitor.running) {
       g_app.standby_monitor.running = 0;
       pthread_join(g_app.standby_monitor.runner, NULL);
    }
    NxClient_Uninit();

    if (g_app.proactive_runner.running) {
       g_app.proactive_runner.running = 0;
       pthread_join(g_app.proactive_runner.runner, NULL);
    }
    BKNI_DestroyEvent(g_app.proactive_runner.runner_run);

    g_app.nxb->terminate();
    if (g_app.binder.running) {
       g_app.binder.running = 0;
       pthread_join(g_app.binder.runner, NULL);
    }

    if (g_app.sigterm.running) {
       g_app.sigterm.running = 0;
       pthread_join(g_app.sigterm.catcher, NULL);
    }
    sem_destroy(&g_app.sigterm.catcher_run);

    uninit_nxserver(nx_srv);
    return 0;
}
