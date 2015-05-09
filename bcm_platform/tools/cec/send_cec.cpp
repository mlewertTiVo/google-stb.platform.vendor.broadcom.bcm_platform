/******************************************************************************
 *    (c)2010-2014 Broadcom Corporation
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
#define LOG_TAG "send_cec"

#include <cutils/properties.h>
#include "nexus_ipc_client_factory.h"

using namespace android;

// ---------------------------------------------------------------------------

#define MAX_MESSAGE_LEN 16

/* Usage: send_cec [-s source address] [-d destination address] -m "message"
   e.g. send_cec -m "0x36"                  [put TV in to standby on CEC0]
        send_cec -m "0x04"                  [bring TV out of standby on CEC0]
        send_cec -d 0xF -m "0x82 0x30 0x00" [set TV HDMI input to CEC0]
        send_cec -i 1 -m "0x36              [put TV in to standby on CEC1]
*/

void usage(char *cmd)
{
    fprintf(stderr, "Usage: %s [-i cec id] [-s source address] [-d destination address] -m \"message\"\n"
                    "e.g. %s -m \"0x36\"                  [put TV in to standby on CEC0]\n"
                    "     %s -m \"0x04\"                  [bring TV out of standby on CEC0]\n"
                    "     %s -d 0xF -m \"0x82 0x30 0x00\" [set TV HDMI input to STB]\n"
                    "     %s -i 1 -m \"0x36\"             [put TV in to standby on CEC1]\n\n", cmd, cmd, cmd, cmd, cmd);
}

int parseArgs(int argc, char *argv[], uint8_t *pCecId, uint8_t *pSrcAddr, uint8_t *pDestAddr, size_t *pLength, uint8_t *pMessage)
{
   int opt;
   char *endptr;
   char *token;
   size_t i;
   size_t len = 0;
   long tmpValue;
   char messageBuffer[3*sizeof(char)*MAX_MESSAGE_LEN + 1];
   char *pBuf = messageBuffer;
   bool messageFlag = false;

   if (argc < 2) {
       usage(argv[0]);
       return -1;
   }

   *pSrcAddr  = 4;  // Default address is that of STB
   *pDestAddr = 0;  // Default address is that of TV
   *pCecId    = 0;  // Default to CEC0

   while ((opt = getopt(argc, argv, "s:d:i:m:h")) != -1) {
       switch (opt) {
       case 's':
           if (optarg != NULL) {
               tmpValue = strtol(optarg, &endptr, 0);
               if (*endptr == '\0') {
                   if (tmpValue > 0xf) {
                       fprintf(stderr, "ERROR: source address 0x%08lx out of range (0x0 to 0xf)!\n", tmpValue);
                       return -1;
                   }
                   else {
                       *pSrcAddr = *(uint8_t *)&tmpValue;
                   }
               }
               else {
                    fprintf(stderr, "ERROR: Invalid source address (must be between 0x0 and 0xF)!\n");
                    return -1;
               }
           }
           else {
                fprintf(stderr, "ERROR: No source address specified!\n");
           }
           break;
       case 'd':
           if (optarg != NULL) {
               tmpValue = strtol(optarg, &endptr, 0);
               if (*endptr == '\0') {
                   if (tmpValue > 0xf) {
                       fprintf(stderr, "ERROR: destination address 0x%08lx out of range (0x0 to 0xf)!\n", tmpValue);
                       return -1;
                   }
                   else {
                       *pDestAddr = *(uint8_t *)&tmpValue;
                   }
               }
               else {
                    fprintf(stderr, "ERROR: Invalid destination address (must be between 0x0 and 0xF)!\n");
                    return -1;
               }
           }
           else {
                fprintf(stderr, "ERROR: No destination address specified!\n");
           }
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
       case 'm':
           len = strlen(optarg);
           if (len > 3 * MAX_MESSAGE_LEN) {
               fprintf(stderr, "ERROR: Message is too long (max 16 space separated HEX digits)!\n");
               return -1;
           }
           strcpy(messageBuffer, optarg);
           messageFlag = true;
           break;
       case 'h':
       default: /* '?' */
           usage(argv[0]);
           return -1;
       }
   }

   if (messageFlag == false) {
       fprintf(stderr, "ERROR: must specify a message!!!\n");
       usage(argv[0]);
       return -1;
   }

   i = 0;
   while ((token = strtok(pBuf, " ,")) != NULL) {
       pBuf = NULL;
       tmpValue = strtol(token, &endptr, 0);
       if (*endptr == '\0') {
           if (tmpValue > 0xFF) {
               fprintf(stderr, "ERROR: Message parameter %d value 0x%08lx is out of range (0x00 to 0xFF)!\n", i, tmpValue);
               return -1;
           }
           else {
               *pMessage++ = *(uint8_t *)&tmpValue;
           }
       }
       i++;
   }
   *pLength = i;

   return 0;
}

bool sendCecMessage(uint8_t cecId, uint8_t srcAddr, uint8_t destAddr, size_t length, uint8_t *pMessage)
{
    bool success;
    NexusIPCClientBase *pIpcClient = NexusIPCClientFactory::getClient("send_cec");

    success = pIpcClient->sendCecMessage(cecId, srcAddr, destAddr, length, pMessage);
    delete pIpcClient;

    return success;
}

bool isCecEnabled(uint8_t cecId)
{
    bool cecEnabled;
    NexusIPCClientBase *pIpcClient = NexusIPCClientFactory::getClient("send_cec");

    cecEnabled = pIpcClient->isCecEnabled(cecId);
    delete pIpcClient;

    return cecEnabled;
}

int main(int argc, char** argv)
{
    uint8_t message[MAX_MESSAGE_LEN];
    uint8_t cecId;
    uint8_t srcAddr;
    uint8_t destAddr;
    size_t length;

    if (parseArgs(argc, argv, &cecId, &srcAddr, &destAddr, &length, message) != 0) {
        return -1;
    }
    else if (isCecEnabled(cecId)) {
        if (length == 0 || length > MAX_MESSAGE_LEN) {
            fprintf(stderr, "ERROR: Invalid message length %d!\n", length);
            return -1;
        }
        if (sendCecMessage(cecId, srcAddr, destAddr, length, message) == false) {
            fprintf(stderr, "ERROR: Cannot send CEC%d message with optcode 0x%02X!\n", cecId, message[0]);
            return -1;
        }
        else {
            printf("Successfully sent CEC%d message with opcode 0x%02X\n", cecId, message[0]);
        }
    }
    else {
        fprintf(stderr, "HDMI CEC functionality disabled!\n");
    }
    return 0;
}
