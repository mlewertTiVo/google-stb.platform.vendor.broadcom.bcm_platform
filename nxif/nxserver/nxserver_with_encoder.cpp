/******************************************************************************
 *    (c) 2016 Broadcom
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
#include "nxserver.h"
#include "nxclient.h"

#define ENCODER_RES_WIDTH_DEFAULT      (1280)
#define ENCODER_RES_HEIGHT_DEFAULT     (720)
#define ENCODER_RES_WIDTH_PROP         "ro.nx.enc.max.width"
#define ENCODER_RES_HEIGHT_PROP        "ro.nx.enc.max.height"

void trim_encoder_mem_config(NEXUS_MemoryConfigurationSettings *pMemConfigSettings)
{
   int i = 0;

   /* *** HARDCODE *** only request a single encoder and limit its capabilities. */
   for (i = 1; i < NEXUS_MAX_VIDEO_ENCODERS; i++) {
      pMemConfigSettings->videoEncoder[i].used = false;
   }
   if (NEXUS_MAX_VIDEO_ENCODERS) {
      pMemConfigSettings->videoEncoder[0].maxWidth =
         property_get_int32(ENCODER_RES_WIDTH_PROP , ENCODER_RES_WIDTH_DEFAULT);
      pMemConfigSettings->videoEncoder[0].maxHeight =
         property_get_int32(ENCODER_RES_HEIGHT_PROP, ENCODER_RES_HEIGHT_DEFAULT);
   }
}

bool keep_display_for_encoder(int disp_ix, int enc_ix, NEXUS_PlatformCapabilities *pPlatformCap)
{
   if (pPlatformCap->display[disp_ix].supported &&
       pPlatformCap->display[disp_ix].encoder &&
       pPlatformCap->videoEncoder[enc_ix].supported &&
       (pPlatformCap->videoEncoder[enc_ix].displayIndex == (unsigned int)disp_ix)) {
      return true;
   } else {
      return false;
   }
}

void defer_init_encoder(NEXUS_PlatformSettings *pPlatformSettings, bool defer)
{
   pPlatformSettings->videoEncoderSettings.deferInit = defer;
}

