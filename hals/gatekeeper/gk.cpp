/******************************************************************************
 *    (c)2018 Broadcom Corporation
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
  *****************************************************************************/
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <log/log.h>
#include "cutils/properties.h"
#include "gk.h"


static int brcm_gk_enroll(const struct gatekeeper_device *dev, uint32_t uid,
   const uint8_t *current_password_handle, uint32_t current_password_handle_length,
   const uint8_t *current_password, uint32_t current_password_length,
   const uint8_t *desired_password, uint32_t desired_password_length,
   uint8_t **enrolled_password_handle, uint32_t *enrolled_password_handle_length) {

   (void)dev;
   (void)uid;
   (void)current_password_handle;
   (void)current_password_handle_length;
   (void)current_password;
   (void)current_password_length;
   (void)desired_password;
   (void)desired_password_length;

   *enrolled_password_handle = NULL;
   *enrolled_password_handle_length = 0;
   return -EINVAL;
}

static int brcm_gk_verify(const struct gatekeeper_device *dev, uint32_t uid, uint64_t challenge,
   const uint8_t *enrolled_password_handle, uint32_t enrolled_password_handle_length,
   const uint8_t *provided_password, uint32_t provided_password_length,
   uint8_t **auth_token, uint32_t *auth_token_length, bool *request_reenroll) {

   (void)dev;
   (void)uid;
   (void)challenge;
   (void)enrolled_password_handle;
   (void)enrolled_password_handle_length;
   (void)provided_password;
   (void)provided_password_length;

   *auth_token = NULL;
   *auth_token_length = 0;
   *request_reenroll = false;
   return -EINVAL;
}

static int brcm_gk_del_u(const struct gatekeeper_device *dev,  uint32_t uid) {
   (void)dev;
   (void)uid;

   return 0;
}

static int brcm_gk_del_all_u(const struct gatekeeper_device *dev) {
   (void)dev;

   return 0;
}

static int brcm_gk_close(hw_device_t *dev) {
    free(dev);
    return 0;
}

static int brcm_gk_open(const hw_module_t *module, const char *name, hw_device_t **device) {
   if (strcmp(name, HARDWARE_GATEKEEPER) != 0) {
      return -EINVAL;
   }
   if (device == NULL) {
      ALOGE("NULL device on open");
      return -EINVAL;
   }
   struct gatekeeper_device *dev = (struct gatekeeper_device *)malloc(sizeof(struct gatekeeper_device));
   memset(dev, 0, sizeof(gatekeeper_device_t));

   dev->common.tag     = HARDWARE_DEVICE_TAG;
   dev->common.version = 0;
   dev->common.module  = (struct hw_module_t*) module;
   dev->common.close   = brcm_gk_close;

   dev->enroll            = brcm_gk_enroll,
   dev->verify            = brcm_gk_verify,
   dev->delete_user       = brcm_gk_del_u,
   dev->delete_all_users  = brcm_gk_del_all_u,

   *device = (hw_device_t*) dev;
   return 0;
}

static struct hw_module_methods_t gatekeeper_module_methods = {
    .open = brcm_gk_open,
};

struct gatekeeper_module HAL_MODULE_INFO_SYM = {
   .common = {
      .tag                = HARDWARE_MODULE_TAG,
      .module_api_version = GATEKEEPER_MODULE_API_VERSION_0_1,
      .hal_api_version    = HARDWARE_HAL_API_VERSION,
      .id                 = GATEKEEPER_HARDWARE_MODULE_ID,
      .name               = "gatekeeper for set-top-box platforms",
      .author             = "Broadcom",
      .methods            = &gatekeeper_module_methods,
      .dso                = 0,
      .reserved           = {0}
    },
};

