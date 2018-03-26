/***************************************************************************
*     (c)2015 Broadcom Corporation
*
*  This program is the proprietary software of Broadcom Corporation and/or its licensors,
*  and may only be used, duplicated, modified or distributed pursuant to the terms and
*  conditions of a separate, written license agreement executed between you and Broadcom
*  (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
*  no license (express or implied), right to use, or waiver of any kind with respect to the
*  Software, and Broadcom expressly reserves all rights in and to the Software and all
*  intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
*  HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
*  NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
*
*  Except as expressly set forth in the Authorized License,
*
*  1.     This program, including its structure, sequence and organization, constitutes the valuable trade
*  secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
*  and to use this information only in connection with your use of Broadcom integrated circuit products.
*
*  2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
*  AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
*  WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
*  THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES
*  OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE,
*  LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION
*  OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF
*  USE OR PERFORMANCE OF THE SOFTWARE.
*
*  3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
*  LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR
*  EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR
*  USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF
*  THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT
*  ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
*  LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF
*  ANY LIMITED REMEDY.
*
***************************************************************************/
#define LOG_TAG "nxproxyif"

#include <stdlib.h>
#include <string.h>
#include "nexus_platform_priv.h"
#include <log/log.h>
#include <sched.h>
#include <sys/resource.h>
#include <cutils/sched_policy.h>
#include <system/thread_defs.h>

#define PID_NAME 128

static int nexus_p_client_getprocname(pid_t pid, char *buf, int len) {
   char *filename;
   FILE *f;
   int rc = 0;
   static const char* unknown_cmdline = "<unknown>";

   if (len <= 0) {
      return -1;
   }
   if (asprintf(&filename, "/proc/%d/cmdline", pid) < 0) {
      rc = 1;
      goto exit;
   }
   f = fopen(filename, "r");
   if (f == NULL) {
       rc = 2;
       goto releasefilename;
   }
   if (fgets(buf, len, f) == NULL) {
       rc = 3;
       goto closefile;
   }

closefile:
   (void) fclose(f);
releasefilename:
   free(filename);
exit:
   if (rc != 0) {
      if (strlcpy(buf, unknown_cmdline, (size_t)len) >= (size_t)len) {
         rc = 4;
      }
   }
   return rc;
}

#define MEDIA_SERVER "/system/bin/mediaserver"
#define OMX_COMPONENT "OMX.broadcom."

void nexus_p_set_scheduler_thread_priority(NEXUS_ModulePriority priority)
{
    char name[PID_NAME];

    /* intercept client slave_callback of interest and update its priority. */
    if (priority == NEXUS_ModulePriority_eMax) {
       nexus_p_client_getprocname(getpid(), name, PID_NAME);
       if (!strncmp(name, OMX_COMPONENT, strlen(OMX_COMPONENT)) ||
           !strncmp(name, MEDIA_SERVER, strlen(MEDIA_SERVER))) {
          setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_FOREGROUND+ANDROID_PRIORITY_MORE_FAVORABLE);
          set_sched_policy(0, SP_FOREGROUND);
          ALOGI("nx-slave-prio:%d for \'%s\'", ANDROID_PRIORITY_FOREGROUND+ANDROID_PRIORITY_MORE_FAVORABLE, name);
       }
    }

    return;
}
