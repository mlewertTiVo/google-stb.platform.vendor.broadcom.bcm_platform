/******************************************************************************
 *    (c)2014 Broadcom Corporation
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
  *****************************************************************************/
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <cutils/log.h>
#include "cutils/properties.h"
#include "brcm_memtrack.h"
#include "nx_ashmem.h"
#include "nxclient.h"
#include "bkni.h"

/* only ever report a single record per process.
*/
#define MEMTRACK_HAL_NUM_RECORDS_MAX 1
int ashmem_fd;

int brcm_memtrack_init(const struct memtrack_module *module)
{
    char value[PROPERTY_VALUE_MAX];
    char value2[PROPERTY_VALUE_MAX];

    if (!module) {
       return -ENOMEM;
    }

    property_get("ro.nexus.ashmem.devname", value, "nx_ashmem");
    strcpy(value2, "/dev/");
    strcat(value2, value);
    ashmem_fd = open(value2, O_RDWR, 0);

    return 0;
}

int brcm_memtrack_get_memory(const struct memtrack_module *module,
                                pid_t pid,
                                int type,
                                struct memtrack_record *records,
                                size_t *num_records)
{
   int rc = 0;
   NEXUS_ClientHandle client;
   NEXUS_PlatformObjectInstance *objects = NULL;
   NEXUS_InterfaceName interfaceName;
   size_t num, i;
   size_t queried;
   unsigned total = 0;
   NxClient_JoinSettings joinSettings;
   int playback = 0;

   if (!module) {
       ALOGE("%s: invalid module.", __FUNCTION__);
       goto exit;
   }

   NxClient_GetDefaultJoinSettings(&joinSettings);
   joinSettings.ignoreStandbyRequest = true;

   if (NxClient_Join(&joinSettings) != NEXUS_SUCCESS) {
       rc = -EBUSY;
       goto exit;
   }

   client = NxClient_Config_LookupClient(pid);
   if (client == NULL) {
      ALOGV("%s: failed getting nexus client for pid %d", __FUNCTION__, pid);
      rc = -EINVAL;
      goto exit_clean;
   }

   if (!*num_records) {
      *num_records = MEMTRACK_HAL_NUM_RECORDS_MAX;
      goto exit_clean;
   }

   switch(type) {
       case MEMTRACK_TYPE_GRAPHICS:
       {
           playback = 0;
           if (ashmem_fd >= 0) {
              ioctl(ashmem_fd, NX_ASHMEM_CHK_PLAY, &playback);
           }
           if (playback) {
              total = 0;
              ALOGI("%s: pid %d, skips reports - multimedia activity.", __FUNCTION__, pid);
              goto exit_early;
           }
           strcpy(interfaceName.name, "NEXUS_Surface");
           num = 0;
           NEXUS_Platform_GetClientObjects(client, &interfaceName, NULL, 0, &num);
           ALOGV("%s: pid %d, queries %d NEXUS_Surface.", __FUNCTION__, pid, num);
           if (num > 0) {
              objects = (NEXUS_PlatformObjectInstance *)BKNI_Malloc(num*sizeof(NEXUS_PlatformObjectInstance));
              if (objects == NULL) {
                 num = 0;
              } else {
                 BKNI_Memset(objects, 0, num*sizeof(NEXUS_PlatformObjectInstance));
                 NEXUS_Platform_GetClientObjects(client, &interfaceName, objects, num, &num);
              }
           }
           for (i = 0; i < num; i++) {
               NEXUS_SurfaceCreateSettings createSettings;
               NEXUS_SurfaceStatus status;
               NEXUS_Surface_GetCreateSettings((NEXUS_SurfaceHandle)objects[i].object, &createSettings);
               NEXUS_Surface_GetStatus((NEXUS_SurfaceHandle)objects[i].object, &status);
               total += status.height * status.pitch;
           }
           if (objects != NULL) {
              BKNI_Free(objects);
              objects = NULL;
           }
           strcpy(interfaceName.name, "NEXUS_MemoryBlock");
           num = 0;
           NEXUS_Platform_GetClientObjects(client, &interfaceName, NULL, 0, &num);
           ALOGV("%s: pid %d, queries %d NEXUS_MemoryBlock.", __FUNCTION__, pid, num);
           if (num > 0) {
              objects = (NEXUS_PlatformObjectInstance *)BKNI_Malloc(num*sizeof(NEXUS_PlatformObjectInstance));
              if (objects == NULL) {
                 num = 0;
              } else {
                 BKNI_Memset(objects, 0, num*sizeof(NEXUS_PlatformObjectInstance));
                 NEXUS_Platform_GetClientObjects(client, &interfaceName, objects, num, &num);
              }
           }
           for (i = 0; i < num; i++) {
               NEXUS_MemoryBlockProperties prop;
               NEXUS_MemoryBlock_GetProperties((NEXUS_MemoryBlockHandle)objects[i].object, &prop);
               total += prop.size;
           }
           if (objects != NULL) {
              BKNI_Free(objects);
              objects = NULL;
           }
           ALOGI("%s: pid %d, reports %d bytes of gpx.", __FUNCTION__, pid, total);
       }
       break;

       case MEMTRACK_TYPE_GL:
       case MEMTRACK_TYPE_CAMERA:
       case MEMTRACK_TYPE_MULTIMEDIA:
       case MEMTRACK_TYPE_OTHER:
       default:
          rc = -EINVAL;
          goto exit_clean;
   }

exit_early:
   records[0].size_in_bytes = total;
   records[0].flags = MEMTRACK_FLAG_SHARED | MEMTRACK_FLAG_DEDICATED | MEMTRACK_FLAG_NONSECURE;
exit_clean:
   NxClient_Uninit();
exit:
   return rc;
}

static struct hw_module_methods_t memtrack_module_methods = {
    .open = NULL,
};

struct memtrack_module HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        module_api_version: MEMTRACK_MODULE_API_VERSION_0_1,
        hal_api_version: HARDWARE_HAL_API_VERSION,
        id: MEMTRACK_HARDWARE_MODULE_ID,
        name: "Memory Tracker HAL for BCM STB",
        author: "Broadcom Canada Ltd.",
        methods: &memtrack_module_methods,
        dso: 0,
        reserved: {0}
    },

    init: brcm_memtrack_init,
    getMemory: brcm_memtrack_get_memory,
};


