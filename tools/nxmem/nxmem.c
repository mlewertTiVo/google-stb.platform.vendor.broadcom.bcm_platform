/***************************************************************************
 *     (c)2015 Broadcom Corporation
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
 **************************************************************************/
#ifndef BSTD_CPU_ENDIAN
#define BSTD_CPU_ENDIAN BSTD_ENDIAN_LITTLE
#endif

#define LOG_TAG "nxmem"

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
#include <cutils/log.h>
#include "nxclient.h"
#include <fcntl.h>
#include <errno.h>

BDBG_MODULE(client);

#define NEXUS_TRUSTED_DATA_PATH        "/data/misc/nexus"

static void usage(void)
{
   ALOGI("nxmem [--heap <index>|--all|--status] [--verbose]");
}

int main(int argc, char **argv)
{
    NEXUS_PlatformConfiguration platformConfig;
    NxClient_JoinSettings joinSettings;
    NEXUS_Error rc;
    int i = 1;
    int heap_index = -1, chart_it = 0, status = 0, detail = 0;

    while (i < argc) {
       if (!strcmp(argv[i], "--heap")) {
          if (i + 1 < argc) {
             heap_index = strtol(argv[++i], NULL, 10);
             ALOGI("selecting heap index %d.", heap_index);
          } else {
             usage();
             return -EINVAL;
          }
       }
       if (!strcmp(argv[i], "--all")) {
          heap_index = NEXUS_MAX_HEAPS;
       }
       if (!strcmp(argv[i], "--chart")) {
          chart_it = 1;
       }
       if (!strcmp(argv[i], "--status")) {
          status = 1;
       }
       if (!strcmp(argv[i], "--verbose")) {
          detail = 1;
       }
       if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
          usage();
          return 0;
       }
       i++;
    };

    NxClient_GetDefaultJoinSettings(&joinSettings);
    joinSettings.ignoreStandbyRequest = true;
    joinSettings.timeout = 60;
    rc = NxClient_Join(&joinSettings);
    if (rc) {
       ALOGE("failed to join nexus.  aborting.");
       return -1;
    }

    NEXUS_Platform_GetConfiguration(&platformConfig);
    if (status) {
       NEXUS_Memory_PrintStatus();
    } else if (heap_index == NEXUS_MAX_HEAPS) {
       NEXUS_Memory_PrintHeaps();
       if (detail) {
          for (i = 0 ; i < NEXUS_MAX_HEAPS ; i++) {
             if (platformConfig.heap[i]) {
                NEXUS_Heap_Dump(platformConfig.heap[i]);
             }
          }
       }
    } else if (heap_index >= 0 && heap_index < NEXUS_MAX_HEAPS) {
       NEXUS_Heap_Dump(platformConfig.heap[heap_index]);
    } else {
       usage();
       NxClient_Uninit();
       return -EINVAL;
    }

    NxClient_Uninit();
    return 0;
}
