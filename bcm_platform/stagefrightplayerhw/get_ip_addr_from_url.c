/******************************************************************************
 *    (c)2011-2014 Broadcom Corporation
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
 * $brcm_Workfile: get_ip_addr_from_url.c $
 * 
 *****************************************************************************/

#define LOG_TAG "bcmPlayer"
#include <cutils/log.h>

#include <netdb.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>

int get_hostname_uri_from_url(char *urlstr, char *hostname, unsigned int *portnum, char **uri)
{
    char *hostPtr, *portPtr, *uriPtr;

    if (!urlstr || !hostname || !portnum || !uri) {
        LOGE("%s: !urlstr || !hostname || !portnum || !uri ", __FUNCTION__);
        return -1;
    }

    // Skip over the protocol if it's there
    hostPtr = strstr(urlstr, "://");
    if (hostPtr) {
        hostPtr +=3;
    } else {
        // Not found
        hostPtr = urlstr;
    }

    // Find the port if it's there
    uriPtr = strstr(hostPtr, "/");
    portPtr = strstr(hostPtr, ":");
    if (portPtr) {
       if (portPtr < uriPtr || !uriPtr) {
            if (atoi(portPtr+1)) {
                *portnum = atoi(portPtr+1);
            }
            // We can now copy the hostname as we know where it ends
            // Hostname can only be 255 chars
            memset(hostname, 0, 256);
            strncpy(hostname, hostPtr, ((portPtr - hostPtr) < 255)? (portPtr - hostPtr) : 255);
       } else {
           portPtr = NULL;  // Reset Port Pointer
       }
    }

    if (!portPtr && uriPtr) {
        // No port specified so still need to fill in hostname
        memset(hostname, 0, 256);
        strncpy(hostname, hostPtr, ((uriPtr - hostPtr) < 255)? (uriPtr - hostPtr) : 255);
    } else if (!portPtr && !uriPtr) {
        // No port and no uri, copy the rest
        memset(hostname, 0, 256);
        strncpy(hostname, hostPtr, 255);
    }

    *uri = uriPtr;

    LOGI("%s: Hostname %s", __FUNCTION__, hostname);
    LOGI("%s: Port %d", __FUNCTION__, *portnum);
    LOGI("%s: URI %s", __FUNCTION__, uri? *uri : "");

    return 0;
}
