/******************************************************************************
 *    (c)2010-2012 Broadcom Corporation
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
 *****************************************************************************/

#define LOG_TAG "EGL_nexus_client"

#include "nexus_types.h"
#include "nexus_platform.h"

#include <utils/Log.h>
#include <string.h>
#include <cutils/atomic.h>
#include <utils/Errors.h>
#include "cutils/properties.h"

#include "nexus_ipc_client_factory.h"

class nexus_egl_client : public NexusIPCClientBase
{
public:
      static void* IEGL_nexus_join();
      static void IEGL_nexus_unjoin(void *nexus_client);
};

void* nexus_egl_client::IEGL_nexus_join()
{
     NexusClientContext *nexus_client=NULL;
     uint32_t w, h;

     NexusIPCClientBase *pIpcClient = NexusIPCClientFactory::getClient("Android-EGL");

     if (pIpcClient != NULL) {
         nexus_client = pIpcClient->createClientContext();
         if (nexus_client == NULL) {
            ALOGE("%s: Client \"%s\" could NOT be created!", __FUNCTION__, pIpcClient->getClientName());
         }
         delete pIpcClient;
     }
     else {
        ALOGE("%s: FATAL: Could not create \"Android-EGL\" Nexus IPC Client!!!", __FUNCTION__);
     }
     return (reinterpret_cast<void *>(nexus_client));
}

void nexus_egl_client::IEGL_nexus_unjoin(void *nexus_client)
{
    NexusIPCClientBase *pIpcClient = NexusIPCClientFactory::getClient("Android-EGL");

    pIpcClient->destroyClientContext((reinterpret_cast<NexusClientContext *>(nexus_client)));
    delete pIpcClient;
    return;
}

extern "C" void* EGL_nexus_join(char *client_process_name);
extern "C" void EGL_nexus_unjoin(void *nexus_client);

void* EGL_nexus_join(char *client_process_name)
{
    void *nexus_client = NULL;

    do
    {
        nexus_client = nexus_egl_client::IEGL_nexus_join();
        if (!nexus_client)
        {
            /* really?  infinite loop here? */
            sleep(1);
        }
    } while (!nexus_client);

    return nexus_client;
}

void EGL_nexus_unjoin(void *nexus_client)
{
    nexus_egl_client::IEGL_nexus_unjoin(nexus_client);
    return;
}
