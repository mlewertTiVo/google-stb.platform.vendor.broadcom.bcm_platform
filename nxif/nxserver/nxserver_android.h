/******************************************************************************
 *    (c) 2018 Broadcom
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

#ifndef __NX_SERVER_ANDROID_H__
#define __NX_SERVER_ANDROID_H__

#define NX_CLIENT_USAGE_LOG            0
#define NX_CLIENT_EVM_LOG              0

#define NEXUS_TRUSTED_DATA_PATH        "/data/vendor/misc/nexus"
#define NEXUS_LOGGER_DATA_PATH         "disabled" // Disable logger use of filesystem
#define APP_MAX_CLIENTS                (72)
#define MB                             (1024*1024)
#define KB                             (1024)
#define SEC_TO_MSEC                    (1000LL)
#define RUNNER_SEC_THRESHOLD           (10)
#define RUNNER_GC_THRESHOLD            (3)
#define RUNNER_LMK_THRESHOLD           (10)
#define NUM_NX_OBJS                    (128)
#define MAX_NX_OBJS                    (2048)
#define MIN_PLATFORM_DEC               (2)
#define NSC_FB_NUMBER                  (2)
#define OOM_SCORE_IGNORE               (-16)
#define OOM_CONSUME_MAX                (192)
#define OOM_THRESHOLD_AGRESSIVE        (16)
#define GRAPHICS_RES_WIDTH_DEFAULT     (1920)
#define GRAPHICS_RES_HEIGHT_DEFAULT    (1080)
#define NX_MMA_SHRINK_THRESHOLD_DEF    "2m"
#define NX_WD_TIMEOUT_DEF              60
#define NX_HEAP_DYN_FREE_THRESHOLD     (1920*1080*4) /* one 1080p RGBA. */
#define NX_PROP_ENABLED                "1"
#define NX_PROP_DISABLED               "0"
#define NX_INVALID                     -1
#define NX_NOGRAB_MAGIC                "AWnG"
#define EVM_CNT_DEF                    (14)
#define MMU_MAX_CLIENTS                (32) /* 16 in reality at the moment. */
#define NX_AP_FIFO_SZ_DEF              "16k"
#define NX_AP_FIFO_THRESHOLD_DEF       "1024"

typedef enum {
   SVP_MODE_NONE,
   SVP_MODE_NONE_TRANSCODE,
   SVP_MODE_PLAYBACK,
   SVP_MODE_PLAYBACK_TRANSCODE,
   SVP_MODE_DTU,
   SVP_MODE_DTU_TRANSCODE,
} SVP_MODE_T;

typedef enum {
   EVM_NONE,
   EVM_SOFT,
   EVM_ZEALOUS,
} EVICTOR_MODE_T;

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
        unsigned pid;
        NEXUS_ClientHandle handle;
        unsigned long mmuvrt; /* total mapped for process. */
        unsigned long mmushm; /* linux allocation (vs cma). */
        unsigned long mmucma; /* cma allocation (vs linux). */
        unsigned long nxmem;  /* nexus cma gfx allocation. */
        unsigned long gfxmem; /* nexus bmem gfx allocation. */
        int score;
    } clients[APP_MAX_CLIENTS];
    struct {
        unsigned pid;
        int score;
    } mmu_cli[MMU_MAX_CLIENTS];
    unsigned connected;
    WDOG_T wdog;
    CATCHER_T sigterm;
    NexusImpl *nxi;
    EVICTOR_MODE_T evm;
    NEXUS_MemoryBlockHandle sdmablk;
} NX_SERVER_T;

#endif /* __NX_SERVER_ANDROID_H__ */
