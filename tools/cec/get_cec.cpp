/******************************************************************************
 *    (c)2010-2013 Broadcom Corporation
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
 ******************************************************************************/
#define LOG_TAG "get_cec"

#include <cutils/properties.h>
#include "nexus_ipc_client_factory.h"

using namespace android;

// ---------------------------------------------------------------------------

#define MAX_MESSAGE_LEN 16

/* Usage: get_cec [-i cec id] -p
Where: -p [Get TV Power Status]
       -i [ CEC ID]
   e.g. get_cec -p        [Get Power Status of TV on CEC0]
        get_cec -i 1 -p   [Get Power Status of TV on CEC1]
*/

void usage(char *cmd)
{
    fprintf(stderr, "Usage: %s [-i cec id] -p\n"
                    "e.g. %s -p         [Get Power Status of TV on CEC0]\n"
                    "     %s -i 1 -p    [Get Power Status of TV on CEC1]\n\n", cmd, cmd, cmd);
}

int parseArgs(int argc, char *argv[], uint8_t *pCecId, bool *pGetPowerStatus)
{
   int opt;
   char *endptr;
   long tmpValue;

   if (argc < 2) {
       usage(argv[0]);
       return -1;
   }

   *pCecId = 0; // Default to CEC0
   *pGetPowerStatus = false;

   while ((opt = getopt(argc, argv, "pi:h")) != -1) {
       switch (opt) {
       case 'p':
           *pGetPowerStatus = true;
           break;
       case 'i':
           if (optarg != NULL) {
               tmpValue = strtol(optarg, &endptr, 0);
               if (*endptr == '\0') {
                   if (tmpValue >= NEXUS_NUM_CEC) {
                       if (NEXUS_NUM_CEC > 0) {
                           fprintf(stderr, "ERROR: CEC id out of range (0 to %d)!\n", NEXUS_NUM_CEC-1);
                       } else {
                           fprintf(stderr, "ERROR: No CEC ports on device!\n");
                       }
                       return -1;
                   }
                   else {
                       *pCecId = *(uint8_t *)&tmpValue;
                   }
               }
               else {
                    fprintf(stderr, "ERROR: Invalid CEC id!\n");
                    return -1;
               }
           }
           else {
                fprintf(stderr, "ERROR: No CEC id specified!\n");
           }
           break;
       case 'h':
       default: /* '?' */
           usage(argv[0]);
           return -1;
       }
   }
   return 0;
}

bool getCecPowerStatus(uint8_t cecId, uint8_t *pPowerStatus)
{
    bool success;
    bool enableCec;
    NexusIPCClientBase *pIpcClient = NexusIPCClientFactory::getClient("get_cec");

    enableCec = NexusIPCCommon::isCecEnabled(cecId);
    if (enableCec) {
        success = pIpcClient->getCecPowerStatus(cecId, pPowerStatus);
    }
    else {
        fprintf(stderr, "HDMI CEC functionality disabled!\n");
        success = false;
    }
    delete pIpcClient;

    return success;
}

static const char *powerStatusStrings[4] =
{
    "On",
    "Standby",
    "Standby->On",
    "On->Standby"
};

int main(int argc, char** argv)
{
    uint8_t cecId;
    uint8_t powerStatus;
    bool getPowerStatus;

    if (parseArgs(argc, argv, &cecId, &getPowerStatus) != 0) {
        return -1;
    }
    else if (getPowerStatus == true) {
        if (getCecPowerStatus(cecId, &powerStatus) == false) {
            fprintf(stderr, "ERROR: Cannot get CEC%d TV power state!\n", cecId);
            return -1;
        }
        else {
            printf("Successfully got CEC%d TV power state=%d [%s]\n", cecId, powerStatus, powerStatusStrings[powerStatus]);
        }
    }
    return 0;
}
