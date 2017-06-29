/******************************************************************************
 *    (c) 2017 Broadcom
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

#include "nxserver.h"
#include "nxclient.h"
#include "nxserverlib.h"
#include "nexus_bsp_config.h"

#define DHD_SECDMA_PROP                "ro.dhd.secdma"
#define DHD_SECDMA_PARAMS_PATH         "/data/nexus/secdma"
#define NEXUS_TRUSTED_DATA_PATH        "/data/misc/nexus"

static void wait_for_data_available(void)
{
   char prop[PROPERTY_VALUE_MAX];
   int state = 0;

   while (1) {
      state = property_get("vold.decrypt", prop, NULL);
      if (state) {
         if ((strncmp(prop, "trigger_restart_min_framework", state) == 0) ||
             (strncmp(prop, "trigger_restart_framework", state) == 0)) {
            return;
         }
      }
      /* arbitrary wait... */
      BKNI_Sleep(100);
      ALOGV("waiting 100 msecs for cryptfs sync...");
   }
}

void alloc_secdma(NEXUS_MemoryBlockHandle *hMemoryBlock, nxserver_t server)
{
    NEXUS_Error rc = NEXUS_SUCCESS;
    NEXUS_Addr secdmaPhysicalOffset = 0;
    uint32_t secdmaMemSize;
    char value[PROPERTY_VALUE_MAX];
    char secdma_param_file[PROPERTY_VALUE_MAX];
    FILE *pFile;
    char nx_key[PROPERTY_VALUE_MAX];
    NxClient_ClientModeSettings ms;

    memset (value, 0, sizeof(value));

    if (property_get(DHD_SECDMA_PROP, value, NULL)) {
        /* wait for data (re)mount, trigger nexus authentication. */
        wait_for_data_available();
        sprintf(nx_key, "%s/nx_key", NEXUS_TRUSTED_DATA_PATH);
        nxserver_parse_password_file(server, nx_key);
        NxClient_GetDefaultClientModeSettings(&ms);
        NxClient_SetClientMode(&ms);
        ALOGW("force nexus re-authentication with '%s'", nx_key);

        secdmaMemSize = strtoul(value, NULL, 0);
        if (strlen(value) && (secdmaMemSize > 0)) {
            sprintf(secdma_param_file, "%s/stbpriv.txt", DHD_SECDMA_PARAMS_PATH);
            pFile = fopen(secdma_param_file, "w");
            if (pFile == NULL) {
                ALOGE("couldn't open %s", secdma_param_file);
            } else {
                *hMemoryBlock = NEXUS_MemoryBlock_Allocate(NEXUS_MEMC0_MAIN_HEAP, secdmaMemSize, 0x1000, NULL);
                if (*hMemoryBlock == NULL) {
                    ALOGE("NEXUS_MemoryBlock_Allocate failed");
                    fclose(pFile);
                    return;
                }
                rc = NEXUS_MemoryBlock_LockOffset(*hMemoryBlock, &secdmaPhysicalOffset);
                if (rc != NEXUS_SUCCESS) {
                    ALOGE("NEXUS_MemoryBlock_LockOffset returned %d", rc);
                    NEXUS_MemoryBlock_Free(*hMemoryBlock);
                    *hMemoryBlock = NULL;
                    fclose(pFile);
                    return;
                }
                ALOGV("secdmaPhysicalOffset 0x%" PRIX64 " secdmaMemSize 0x%x", secdmaPhysicalOffset, secdmaMemSize);
                rc = NEXUS_Security_SetPciERestrictedRange( secdmaPhysicalOffset, (size_t) secdmaMemSize, (unsigned)0 );
                if (rc != NEXUS_SUCCESS) {
                    ALOGE("NEXUS_Security_SetPciERestrictedRange returned %d", rc);
                    NEXUS_MemoryBlock_Unlock(*hMemoryBlock);
                    NEXUS_MemoryBlock_Free(*hMemoryBlock);
                    *hMemoryBlock = NULL;
                    fclose(pFile);
                    return;
                }
                fprintf(pFile, "secdma_cma_addr=0x%" PRIX64 " secdma_cma_size=0x%x\n", secdmaPhysicalOffset, secdmaMemSize);
                fclose(pFile);
            }
        } else {
            ALOGE("secdma size not set");
        }
    }
}

