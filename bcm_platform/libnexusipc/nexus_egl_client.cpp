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
 * $brcm_Workfile: nexus_egl_client.cpp $
 * $brcm_Revision: 6 $
 * $brcm_Date: 12/3/12 3:23p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/libnexusipc/nexus_egl_client.cpp $
 * 
 * 6   12/3/12 3:23p saranya
 * SWANDROID-266: Removed Non-IPC Standalone Mode
 * 
 * 5   9/13/12 11:30a kagrawal
 * SWANDROID-104: Added support for dynamic display resolution change,
 *  1080p and screen resizing
 * 
 * 4   7/6/12 9:11p ajitabhp
 * SWANDROID-128: FIXED Graphics Window Resource Leakage in SBS and NSC
 *  mode.
 * 
 * 3   6/20/12 6:00p kagrawal
 * SWANDROID-118: Extended get_output_format() to return width and height
 * 
 * 2   5/7/12 3:44p ajitabhp
 * SWANDROID-96: Initial checkin for android side by side implementation.
 * 
 * 1   2/24/12 1:50p kagrawal
 * SWANDROID-12: Initial version of ipc over binder
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
      static void* IEGL_nexus_join(char *client_process_name);
      static void IEGL_nexus_unjoin(void *nexus_client);
};

void* nexus_egl_client::IEGL_nexus_join(char *client_process_name)
{
     b_refsw_client_client_configuration config;
     NexusClientContext *nexus_client=NULL;
     uint32_t w, h;
     
     NexusIPCClientBase *pIpcClient = NexusIPCClientFactory::getClient("Android-EGL");

     BKNI_Memset(&config, 0, sizeof(config));
     BKNI_Snprintf(config.name.string,sizeof(config.name.string),"%s_%d_",client_process_name,getpid());

     /**
      * By default we will not request the screen, only NexusSurface.cpp::init() function will request the
      * Screen by calling addGraphicsWindow from the context of surfaceFlinger initilaization sequence
      **/
     config.resources.screen.required = false;
     config.resources.screen.position.x = 0;
     config.resources.screen.position.y = 0;
     // pass w=0, h=0, so that server decides on the size of the surface as per the HD display format
     config.resources.screen.position.width = 0;
     config.resources.screen.position.height = 0;
     config.resources.audioDecoder = false;
     config.resources.audioPlayback = false;
     config.resources.videoDecoder = false;

     /* Create the client context that will be used by Gralloc */
     nexus_client = pIpcClient->createClientContext(&config);
     if (nexus_client != NULL) {
         LOGI("%s: Client successfully created : client=%p", __FUNCTION__, (void *)nexus_client);
     }
     else {
        LOGE("%s: Client could NOT be created!", __FUNCTION__);
     }
     
     delete pIpcClient;
     return (reinterpret_cast<void *>(nexus_client));
}

void nexus_egl_client::IEGL_nexus_unjoin(void *nexus_client)
{
    NexusIPCClientBase *pIpcClient = NexusIPCClientFactory::getClient("Android-EGL");

    /* Destroy the client context using IPC */
    pIpcClient->destroyClientContext((reinterpret_cast<NexusClientContext *>(nexus_client)));
    delete pIpcClient;
    return;
}

extern "C" void* EGL_nexus_join(char *client_process_name);
extern "C" void EGL_nexus_unjoin(void *nexus_client);

void* EGL_nexus_join(char *client_process_name)
{
    void *nexus_client = NULL;

    LOGE("%s:%d str = %s",__FUNCTION__,__LINE__,client_process_name);
 
    LOGD("==============================================================");
    LOGD(" EGL_nexus_join");
    LOGD("==============================================================");
 
    do
    {
        nexus_client = nexus_egl_client::IEGL_nexus_join(client_process_name);
        if (!nexus_client)
        {
            LOGE("IEGL_nexus_join Failed\n");
            sleep(1);
        } else {
            LOGI("IEGL_nexus_join Success\n");
        }
    } while (!nexus_client);

    return nexus_client;
}

void EGL_nexus_unjoin(void *nexus_client)
{
    nexus_egl_client::IEGL_nexus_unjoin(nexus_client);
    return;
}

