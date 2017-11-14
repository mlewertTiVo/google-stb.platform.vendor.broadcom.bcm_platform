/******************************************************************************
 *    (c)2017 Broadcom Corporation
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

//#define LOG_NDEBUG 0

#undef LOG_TAG
#define LOG_TAG "bcm-vk"

#include <hardware/hwvulkan.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include <cutils/log.h>

static void *vk_icd_dyn_lib;

PFN_vkEnumerateInstanceExtensionProperties enumerateInstanceExtensionProperties;
PFN_vkCreateInstance createInstance;
PFN_vkGetInstanceProcAddr getInstanceProcAddr;

static void *nxwrap = NULL;
static void *nexus_client = NULL;

static int vulkan_device_open(const struct hw_module_t *module,
        const char *name, struct hw_device_t **device);

static struct hw_module_methods_t vulkan_module_methods = {
      .open = vulkan_device_open
};

extern "C" void* nxwrap_create_client(void **wrap);
extern "C" void nxwrap_destroy_client(void *wrap);

// The vulkan module
struct hw_module_t HAL_MODULE_INFO_SYM = {
   .tag                = HARDWARE_MODULE_TAG,
   .module_api_version = HWVULKAN_MODULE_API_VERSION_0_1,
   .hal_api_version    = HARDWARE_HAL_API_VERSION,
   .id                 = HWVULKAN_HARDWARE_MODULE_ID,
   .name               = "vulkan for set-top-box platforms",
   .author             = "Broadcom",
   .methods            = &vulkan_module_methods,
   .dso                = 0,
   .reserved           = {0}
};

static int vulkan_device_close(struct hw_device_t *dev)
{
   struct hwvulkan_device_t *hwvk_dev = reinterpret_cast<struct hwvulkan_device_t *>(dev);

   if (nxwrap != NULL) {
      nxwrap_destroy_client(nxwrap);
      nxwrap = NULL;
   }
   free(hwvk_dev);

   dlclose(vk_icd_dyn_lib);

   return 0;
}

static int vulkan_device_open(const struct hw_module_t *module,
        const char *name, struct hw_device_t **device)
{
   int status = -EINVAL;
   const char *error;
   *device = NULL;

   if (strcmp(name, HWVULKAN_DEVICE_0) == 0) {

      struct hwvulkan_device_t *hwvk_dev = (struct hwvulkan_device_t *)malloc(sizeof(*hwvk_dev));

      if (nxwrap != NULL) {
         nxwrap_destroy_client(nxwrap);
      }

      nexus_client = nxwrap_create_client(&nxwrap);

      if (nexus_client == NULL) {
         ALOGE("%s: Could not create Nexux Client!!!", __FUNCTION__);
         goto out;
      }

      *device = reinterpret_cast<hw_device_t *>(hwvk_dev);

      /* initialize our state here */
      memset(hwvk_dev, 0, sizeof(*hwvk_dev));

      hwvk_dev->common.tag = HARDWARE_DEVICE_TAG;
      hwvk_dev->common.version = 0;
      hwvk_dev->common.module = const_cast<hw_module_t*>(module);
      hwvk_dev->common.close = vulkan_device_close;

      vk_icd_dyn_lib = dlopen("libbcmvulkan_icd.so", RTLD_LAZY | RTLD_LOCAL);
      if (!vk_icd_dyn_lib) {
         vk_icd_dyn_lib = dlopen("/vendor/lib/libbcmvulkan_icd.so", RTLD_LAZY | RTLD_LOCAL);
         if (!vk_icd_dyn_lib) {
            ALOGE("failed loading Vulkan ICD library '%s': <%s>!", "libbcmvulkan_icd.so", dlerror());
            goto out;
         }
      }

      dlerror();    /* Clear any existing error */

      *(void **) (&enumerateInstanceExtensionProperties) = dlsym(vk_icd_dyn_lib, "vkEnumerateInstanceExtensionProperties");
      if ((error = dlerror()) != NULL)  {
         ALOGE("loading symbol '%s': <%s>!", "vkEnumerateInstanceExtensionProperties", dlerror());
         goto out;
      }

      *(void **) (&createInstance) = dlsym(vk_icd_dyn_lib, "vkCreateInstance");
      if ((error = dlerror()) != NULL)  {
         ALOGE("loading symbol '%s': <%s>!", "vkCreateInstance", dlerror());
         goto out;
      }

      *(void **) (&getInstanceProcAddr) = dlsym(vk_icd_dyn_lib, "vkGetInstanceProcAddr");
      if ((error = dlerror()) != NULL)  {
         ALOGE("loading symbol '%s': <%s>!", "vkGetInstanceProcAddr", dlerror());
         goto out;
      }

      hwvk_dev->EnumerateInstanceExtensionProperties = enumerateInstanceExtensionProperties;
      hwvk_dev->CreateInstance = createInstance;
      hwvk_dev->GetInstanceProcAddr = getInstanceProcAddr;

      status = 0;
   }

out:
   if (status) {
      ALOGE("=========== Error loading bcm vulkan icd driver: Vulkan applications will not be supported");
      vulkan_device_close(*device);
      *device = NULL;
   }

   return status;
}
