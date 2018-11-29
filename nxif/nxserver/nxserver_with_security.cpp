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
#include <log/log.h>
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
#if defined(SECV2)
#include "nexus_pcie_window.h"
#else
#include "nexus_bsp_config.h"
#endif

#include "vendor_bcm_props.h"

#define DHD_SECDMA_PARAMS_PATH         "/data/vendor/nexus/secdma"
#define NEXUS_TRUSTED_DATA_PATH        "/data/vendor/misc/nexus"

pthread_t secdma_thread;

static void wait_for_data_available(void)
{
   // very simple check here, we need to wait until we are told that
   // the key has been written, which means the /data/vendor has been
   // populated and is available.
   while (1) {
      if (!access(NEXUS_TRUSTED_DATA_PATH, R_OK)) {
         return;
      }

      /* arbitrary wait... */
      BKNI_Sleep(100);
      ALOGV("waiting 100 msecs for data available...");
   }
}

static void *alloc_secdma_task(void *argv)
{
   NX_SERVER_T *nx_server = (NX_SERVER_T *)argv;
   bool exit = false;
   NEXUS_Error rc = NEXUS_SUCCESS;
   NEXUS_Addr secdmaPhysicalOffset = 0;
   uint32_t secdmaMemSize = 0;
   char value[PROPERTY_VALUE_MAX];
   char secdma_param_file[PROPERTY_VALUE_MAX];
   FILE *pFile;
   char nx_key[PROPERTY_VALUE_MAX];
   int rci = 0;

   memset (value, 0, sizeof(value));
   prctl(PR_SET_NAME, "nxserer.secdma");

   /* wait for data (re)mount. */
   wait_for_data_available();

   /* trigger nexus authentication re-init now that /data is there. */
   sprintf(nx_key, "%s/nx_key", NEXUS_TRUSTED_DATA_PATH);
   rci = nxserver_parse_password_file(nx_server->server, nx_key);
   if (rci) {
      ALOGE("refreshing auth. credentials ('%s'), FAILED: %d", nx_key, rci);
   } else {
      ALOGI("refreshed auth. credentials ('%s')", nx_key);
   }

   /* allocate the secure dma region if possible. */
   property_get(BCM_RO_NX_DHD_SECDMA, value, NULL);
   if (strlen(value))
      secdmaMemSize = strtoul(value, NULL, 0);
   if (secdmaMemSize > 0) {
      sprintf(secdma_param_file, "%s/stbpriv.txt", DHD_SECDMA_PARAMS_PATH);
      pFile = fopen(secdma_param_file, "w");
      if (pFile == NULL) {
         ALOGE("couldn't open %s", secdma_param_file);
      } else {
         nx_server->sdmablk = NEXUS_MemoryBlock_Allocate(
            NULL, /* use default (main) heap */
            secdmaMemSize,
            0x1000,
            NULL);
         if (nx_server->sdmablk == NULL) {
            ALOGE("NEXUS_MemoryBlock_Allocate failed");
            fclose(pFile);
             goto done;
         }
         rc = NEXUS_MemoryBlock_LockOffset(nx_server->sdmablk, &secdmaPhysicalOffset);
         if (rc != NEXUS_SUCCESS) {
            ALOGE("NEXUS_MemoryBlock_LockOffset returned %d", rc);
            NEXUS_MemoryBlock_Free(nx_server->sdmablk);
            nx_server->sdmablk = NULL;
            fclose(pFile);
            goto done;
         }
         ALOGI("secdmaPhysicalOffset 0x%" PRIX64 " secdmaMemSize 0x%x", secdmaPhysicalOffset, secdmaMemSize);
         rc = NEXUS_Security_SetPciERestrictedRange( secdmaPhysicalOffset, (size_t) secdmaMemSize, (unsigned)0 );
         if (rc != NEXUS_SUCCESS) {
            ALOGE("NEXUS_Security_SetPciERestrictedRange returned %d", rc);
            NEXUS_MemoryBlock_Unlock(nx_server->sdmablk);
            NEXUS_MemoryBlock_Free(nx_server->sdmablk);
            nx_server->sdmablk = NULL;
            fclose(pFile);
            goto done;
         }
         fprintf(pFile, "secdma_cma_addr=0x%" PRIX64 " secdma_cma_size=0x%x\n", secdmaPhysicalOffset, secdmaMemSize);
         fclose(pFile);
      }
   }

done:
    /* trigger waiter.  if secmda allocation failed while it was expected, things
     * are likely to fail (no wifi) beyond this point, but at least we tried.
     */
    property_set(BCM_VDR_NX_WIFI_TRIGGER, "init");
    return NULL;
}

void alloc_secdma(NX_SERVER_T *srv)
{
   char value[PROPERTY_VALUE_MAX];
   memset (value, 0, sizeof(value));
   property_set(BCM_VDR_NX_WIFI_TRIGGER, "null");
   if (property_get(BCM_RO_NX_DHD_SECDMA, value, NULL)) {
      /* async wait to allow the rest of the system to continue
       * booting.
       */
      pthread_attr_t attr;
      pthread_attr_init(&attr);
      if (pthread_create(&secdma_thread, &attr,
                         alloc_secdma_task, (void *)srv) != 0) {
         ALOGE("secdma setup thread FAILED!");
      }
      pthread_attr_destroy(&attr);
   } else {
      /* nothing to wait on in particular, trigger now. */
      property_set(BCM_VDR_NX_WIFI_TRIGGER, "init");
   }
}

