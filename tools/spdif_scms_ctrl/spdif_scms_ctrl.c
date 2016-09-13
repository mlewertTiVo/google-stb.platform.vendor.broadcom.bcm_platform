/******************************************************************************
 *  Copyright (C) 2016 Broadcom. The term "Broadcom" refers to Broadcom Limited and/or its subsidiaries.
 *
 *  This program is the proprietary software of Broadcom and/or its licensors,
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

 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include "bstd.h"
#include "bkni.h"

#include "nxclient.h"
#include "nexus_platform.h"
#include "nexus_spdif_output.h"


BDBG_MODULE(spdif_scms_ctrl);

static void print_help(void);
static int set_spdif_scms_ctrl( int cp_Bit, int l_Bit, int cgsmA_val );

int main(int argc, char* argv[])
{
   NxClient_JoinSettings joinSettings;
   NxClient_AllocSettings nxAllocSettings;
   NxClient_AllocResults allocResults;
   int rc = NEXUS_SUCCESS;

   if(argc != 4)
   {
      BDBG_ERR(("\tInvalid number of command line arguments (%u)\n", argc));
      print_help();
      rc = -1;
      goto handle_error;
   }

   NxClient_GetDefaultJoinSettings(&joinSettings);
   snprintf(joinSettings.name, NXCLIENT_MAX_NAME, "spdif_scms_ctrl");
   joinSettings.ignoreStandbyRequest = true;
   rc = NxClient_Join(&joinSettings);
   if (rc) return -1;

   if((argv[1] != NULL) && (argv[2] != NULL) )
   {
      int cp_Bit = atoi( argv[1] );
      int l_Bit = atoi( argv[2] );
      int cgsmA_val = atoi( argv[3] );
      rc = set_spdif_scms_ctrl( cp_Bit, l_Bit, cgsmA_val );
      if ( rc ) print_help();
   }

handle_error:

    NxClient_Uninit();

    if (rc) {
        BDBG_ERR(("Failure in SPDIF SCMS Ctrl test rc=%d\n",rc));
    }
    return rc;
}


static void print_help(void)
{
    printf("\n");
    printf(" Usage: \n");
    printf(" spdif_scms_ctrl <Cp-bit> <L-bit> <cgsma value>\n");
    printf("   Cp-bit values: [-1|0|1]\n");
    printf("   L-bit  values: [-1|0|1]\n");
    printf("   cgsmaA values: [-1|0|1|2|3]\n");
    printf("   Use (-1) to dump current value\n");
    printf("   Example: spdif_scms_ctrl 1 0 -1, sets Cp-bit to 1, L-bit to 0, dumps cgsma value\n");
    return;
}

static int set_spdif_scms_ctrl( int cp_Bit, int l_Bit, int cgsmA_val )
{
   NEXUS_PlatformConfiguration platformConfig;
   NEXUS_Platform_GetConfiguration( &platformConfig );
   NEXUS_SpdifOutputSettings spdifSettings;
   int rc = 0;

   BDBG_LOG(("set_spdif_scms_ctrl: Setting cp_Bit:[%d], l_Bit:[%d], cgsmaA_val:[%d]", cp_Bit, l_Bit, cgsmA_val ));

   if ( cp_Bit != 0 && cp_Bit != 1 && cp_Bit != -1 ) return -1;
   if ( l_Bit != 0 && l_Bit != 1 && l_Bit != -1 ) return -1;
   if ( cgsmA_val < -1 || cgsmA_val > 3 ) return -1;

   NEXUS_SpdifOutput_GetSettings( platformConfig.outputs.spdif[0], &spdifSettings );
   BDBG_LOG(("Before: initial SPDIF SCMS Control bits values: swCopyRight=[%d], categoryGode=[%d], cgmsA=[%d]",
         spdifSettings.channelStatusInfo.swCopyRight,
         spdifSettings.channelStatusInfo.categoryCode,
         spdifSettings.channelStatusInfo.cgmsA
         ));


   /* Set Cp-bit */
   if ( cp_Bit != -1 ) {
      spdifSettings.channelStatusInfo.swCopyRight = cp_Bit;
   }

   /* L bit is MSB of the category code */
   if ( l_Bit != -1 ) {
      if ( l_Bit ) {
         /* Set L-bit */
         spdifSettings.channelStatusInfo.categoryCode |= (1 << 15);
      } else {
         /* Clear L-bit */
         spdifSettings.channelStatusInfo.categoryCode &= ~(1 << 15);
      }
   }


   /* Set cgmsA bits per newer spec */
   if ( cgsmA_val != -1 ) {
      spdifSettings.channelStatusInfo.cgmsA = cgsmA_val;
   }


   rc = NEXUS_SpdifOutput_SetSettings( platformConfig.outputs.spdif[0], &spdifSettings );

   NEXUS_SpdifOutput_GetSettings( platformConfig.outputs.spdif[0], &spdifSettings );
   BDBG_LOG(("After: initial SPDIF SCMS Control bits values: swCopyRight=[%d], categoryGode=[%d], cgmsA=[%d]",
         spdifSettings.channelStatusInfo.swCopyRight,
         spdifSettings.channelStatusInfo.categoryCode,
         spdifSettings.channelStatusInfo.cgmsA
         ));

   return rc;
}
