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

#define LOG_TAG "nxdispfmt"

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
#include <cutils/properties.h>
#include "namevalue.h"

#define NX_HD_OUT_FMT  "nx.vidout.force" /* needs prefixing. */
#define NX_HD_OUT_SET  "dyn.nx.vidout.set"

BDBG_MODULE(nxdispfmt);

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

   if ((forced_format == NEXUS_VideoFormat_eUnknown) || (forced_format >= NEXUS_VideoFormat_eMax)) {
      memset(value, 0, sizeof(value));
      sprintf(name, "dyn.%s", NX_HD_OUT_FMT);
      if (property_get(name, value, "")) {
         if (strlen(value)) {
            forced_format = (NEXUS_VideoFormat)lookup(g_videoFormatStrs, value);
         }
      }
   }

   return forced_format;
}

int main(int argc, char **argv)
{
   int err = 0;
   NEXUS_Error rc;
   NxClient_JoinSettings joinSettings;
   NxClient_DisplaySettings displaySettings;
   NxClient_DisplayStatus status;
   NEXUS_VideoFormat format, old_format;

   (void)argc;
   (void)argv;

   NxClient_GetDefaultJoinSettings(&joinSettings);
   joinSettings.ignoreStandbyRequest = true;
   joinSettings.timeout = 60;
   rc = NxClient_Join(&joinSettings);
   if (rc) {
      err = -1;
      ALOGE("failed to join.");
      return err;
   }

   format = forced_output_format();
   if (format == NEXUS_VideoFormat_eUnknown) {
      err = -1;
      ALOGE("no valid configured format.");
      goto out;
   }

   rc = NxClient_GetDisplayStatus(&status);
   if (rc) {
      err = -1;
      ALOGE("failed to get display status.");
      goto out;
   }

   if (!status.hdmi.status.videoFormatSupported[format]) {
      err = -1;
      ALOGE("format selected %d - unsupported by remote.", format);
      goto out;
   }

   NxClient_GetDisplaySettings(&displaySettings);
   if (displaySettings.format != format) {
      old_format = displaySettings.format;
      displaySettings.format = format;
      rc = NxClient_SetDisplaySettings(&displaySettings);
      if (rc) {
         ALOGE("failed to set display settings.");
         err = -1;
         goto out;
      }
   } else {
      ALOGI("format unchanged %d.", format);
      goto out;
   }

   ALOGI("changed format %d -> %d.", old_format, format);

out:
   NxClient_Uninit();
   property_set(NX_HD_OUT_SET, "0");
   return rc;
}
