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

//#define LOG_NDEBUG 0
#define GK_LOG_ALL_IN 1

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <log/log.h>
#include "cutils/properties.h"
#include "gk.h"

#include <gatekeeper_tl.h>
#include "vendor_bcm_props.h"
extern "C" {
#include <sage_manufacturing_api.h>
#include "nexus_security_datatypes.h"
#if (NEXUS_SECURITY_API_VERSION == 1)
#include "nexus_otpmsp.h"
#else
#include "nexus_otp_msp.h"
#endif
}
#include "nxwrap.h"

struct bcm_gk {
   struct gatekeeper_device  dev;
   NxWrap                   *nxwrap;
   pthread_mutex_t           mtx;
   GatekeeperTl_Handle       handle;
   GatekeeperTl_InitSettings is;
   uint8_t                  *fbkCfg;
   size_t                    fbkSize;
};

extern "C" void* nxwrap_create_verified_client(void **wrap);
extern "C" void nxwrap_destroy_client(void *wrap);

#define OTP_MSP0_VALUE_ZS (0x02)
#define OTP_MSP1_VALUE_ZS (0x02)
#if (NEXUS_SECURITY_API_VERSION == 1)
static bool gk_chip_prod(void) {
   NEXUS_ReadMspParms readMspParms;
   NEXUS_ReadMspIO readMsp0;
   NEXUS_ReadMspIO readMsp1;
   NEXUS_Error rc = NEXUS_SUCCESS;

   readMspParms.readMspEnum = NEXUS_OtpCmdMsp_eReserved233;
   rc = NEXUS_Security_ReadMSP(&readMspParms,&readMsp0);
   if(rc != NEXUS_SUCCESS) goto done;
   readMspParms.readMspEnum = NEXUS_OtpCmdMsp_eReserved234;
   rc = NEXUS_Security_ReadMSP(&readMspParms,&readMsp1);
   if(rc != NEXUS_SUCCESS) goto done;

   if((readMsp0.mspDataBuf[3] == OTP_MSP0_VALUE_ZS) &&
      (readMsp1.mspDataBuf[3] == OTP_MSP1_VALUE_ZS)){
      return false;
   }
done:
   return true;
}
#else
static bool gk_chip_prod(void) {
   NEXUS_OtpMspRead readMsp0;
   NEXUS_OtpMspRead readMsp1;
   uint32_t Msp0Data;
   uint32_t Msp1Data;
   NEXUS_Error rc = NEXUS_SUCCESS;
#if NEXUS_ZEUS_VERSION < NEXUS_ZEUS_VERSION_CALC(5,0)
   rc = NEXUS_OtpMsp_Read(233, &readMsp0);
   if (rc) BERR_TRACE(rc);
   rc = NEXUS_OtpMsp_Read(234, &readMsp1);
   if (rc) BERR_TRACE(rc);
#else
   rc = NEXUS_OtpMsp_Read(224, &readMsp0);
   if (rc) BERR_TRACE(rc);
   rc = NEXUS_OtpMsp_Read(225, &readMsp1);
   if (rc) BERR_TRACE(rc);
#endif
   Msp0Data = readMsp0.data & readMsp0.valid;
   Msp1Data = readMsp1.data & readMsp1.valid;
   if((Msp0Data == OTP_MSP0_VALUE_ZS) && (Msp1Data == OTP_MSP1_VALUE_ZS)) {
      return false;
   }
   return true;
}
#endif

#define GK_GEN_PROV_FALLBACK_PATH "/vendor/usr/kmgk/km.zx.bin"
#define GK_CUS_PROV_FALLBACK_PATH "/vendor/usr/kmgk/km.xx.cus.bin"
static BERR_Code gk_fallback(struct bcm_gk *gk_hdl) {
   BERR_Code gk_err = BSAGE_ERR_BFM_DRM_TYPE_NOT_FOUND;
   FILE *f = NULL;
   size_t gk_size, c;
   int rc, v, s;
   uint8_t *d;
   char *key_path = NULL;

   if ((gk_chip_prod() == true) &&
       !access(GK_CUS_PROV_FALLBACK_PATH, R_OK)) {
      key_path = (char *)GK_CUS_PROV_FALLBACK_PATH;
   } else {
      key_path = (char *)GK_GEN_PROV_FALLBACK_PATH;
   }

   if ((key_path == NULL) || access(key_path, R_OK)) {
      ALOGE("gk_fallback: no key accessible @%s, aborting.", key_path);
      goto out;
   }

   // always use otp index A for this fallback.
   gk_err = SAGE_Manufacturing_Init(SAGE_Manufacturing_OTP_Index_A);
   if (gk_err != BERR_SUCCESS) {
      ALOGE("gk_fallback: cannot setup manufacturing (%d).", gk_err);
      gk_err = BSAGE_ERR_BFM_DRM_TYPE_NOT_FOUND;
      goto out;
   }

   // input manipulation and copy to sage.
   gk_err = BSAGE_ERR_BFM_DRM_TYPE_NOT_FOUND;
   f = fopen(key_path, "rb");
   if (f == NULL) goto out_fin;
   if(fseek (f, 0, SEEK_END) != 0) goto out_fin;
   gk_size = ftell(f);
   if (fseek(f, 0, SEEK_SET) != 0) goto out_fin;

   rc = SAGE_Manufacturing_AllocBinBuffer(gk_size, &d);
   if (rc) goto out_fin;
   gk_hdl->fbkCfg = (uint8_t *)malloc(sizeof(uint8_t)*gk_size);
   if (!gk_hdl->fbkCfg) goto out_dealloc;

   c = fread(d, 1, gk_size, f);
   if (c != gk_size) {
      ALOGE("gk_fallback: failed to copy %s (%zu) to sage, copied %zu",
         key_path, gk_size, c);
      goto out_error;
   }

   // now the real work to validate the key and transform it if necessay.
   rc = SAGE_Manufacturing_BinFile_ParseAndDisplay(d, gk_size, &v);
   if (rc) goto out_error;

   rc = SAGE_Manufacturing_VerifyDrmBinFileType(d, v);
   if (rc <= DRM_BIN_FILE_TYPE_2) {
      rc = 0;
      gk_err = SAGE_Manufacturing_Provision_BinData(&rc);
      if (gk_err != BERR_SUCCESS || rc) {
         gk_err = BSAGE_ERR_BFM_DRM_TYPE_NOT_FOUND;
         goto out_error;
      }
      gk_err = BSAGE_ERR_BFM_DRM_TYPE_NOT_FOUND;
   } else if (rc == DRM_BIN_FILE_TYPE_3 || rc == DRM_BIN_FILE_TYPE_3_PRIME) {
      // already valid format (strange, but why not).
   } else goto out_error;

   // note: there is no validation command associated with keymaster drm, so
   //       skip that part and hope for the best.

   // copy the updated key file, to be used to retry loading gatekeeper.
   memcpy(gk_hdl->fbkCfg, d, gk_size);
   gk_hdl->fbkSize = gk_size;
   gk_err = BERR_SUCCESS;
   goto out_dealloc;

out_error:
   free(gk_hdl->fbkCfg);
   gk_hdl->fbkCfg = NULL;
   gk_hdl->fbkSize = 0;
out_dealloc:
   SAGE_Manufacturing_DeallocBinBuffer();
out_fin:
   if (f) {
      fclose(f);
      f = NULL;
   }
   SAGE_Manufacturing_Uninit();
out:
   return gk_err;
}

static int gk_berr_2_interr(
   BERR_Code berr) {
   int rc = ENOMEM;
   switch (berr) {
   case BERR_SUCCESS: rc = 0; break;
   // missing drm fragment or bad drm.
   case BSAGE_ERR_BFM_PROC_IN1_MISMATCH:
   case BSAGE_ERR_BFM_PROC_IN2_MISMATCH:
   case BSAGE_ERR_BFM_DRM_TYPE_NOT_FOUND: rc = ENOTSUP; break;
   // default.
   default: break;
   }
   ALOGE("gk_berr_2_interr: %s (%x -> %d)", rc==0?"success":"FAILED", berr, rc);
   return ((rc==0)?rc:-rc);
}

static int gk_init(struct bcm_gk *gk_hdl) {
   BERR_Code gk_err;

   // already good to go.
   if (gk_hdl->handle) {
      return 0;
   }

   ALOGI_IF(GK_LOG_ALL_IN, "gk_init: starting tl-side.");
   GatekeeperTl_GetDefaultInitSettings(&gk_hdl->is);
   gk_err = BERR_OS_ERROR;
   if (!pthread_mutex_lock(&gk_hdl->mtx)) {
      gk_err = GatekeeperTl_Init(&gk_hdl->handle, &gk_hdl->is);
      pthread_mutex_unlock(&gk_hdl->mtx);
   }
   if (gk_err != BERR_SUCCESS) {
      ALOGI_IF(GK_LOG_ALL_IN, "gk_init: tl error'ed out: %x (%u).", gk_err, gk_err);
      if (gk_err == BSAGE_ERR_BFM_DRM_TYPE_NOT_FOUND) {
         gk_err = gk_fallback(gk_hdl);
         ALOGI_IF(GK_LOG_ALL_IN, "gk_init: tl fallback: %x (%u).", gk_err, gk_err);
         if (gk_err == BERR_SUCCESS) {
            memset(gk_hdl->is.drm_binfile_path, 0,
                   sizeof(gk_hdl->is.drm_binfile_path));
            gk_hdl->is.drm_binfile_buffer = gk_hdl->fbkCfg;
            gk_hdl->is.drm_binfile_size = gk_hdl->fbkSize;
            gk_err = BERR_OS_ERROR;
            if (!pthread_mutex_lock(&gk_hdl->mtx)) {
               // will return BERR_SUCCESS if fallback succeeded.
               gk_err = GatekeeperTl_Init(&gk_hdl->handle, &gk_hdl->is);
               pthread_mutex_unlock(&gk_hdl->mtx);
            }
         }
      }
      ALOGI_IF(GK_LOG_ALL_IN, "gk_init: tl final: (%x) %u.", gk_err, gk_err);
      return gk_berr_2_interr(gk_err);
   }

   return 0;
}

static int brcm_gk_enroll(
   const struct gatekeeper_device *dev,
   uint32_t uid,
   const uint8_t *current_password_handle,
   uint32_t current_password_handle_length,
   const uint8_t *current_password,
   uint32_t current_password_length,
   const uint8_t *desired_password,
   uint32_t desired_password_length,
   uint8_t **enrolled_password_handle,
   uint32_t *enrolled_password_handle_length) {

   struct bcm_gk *gk_hdl = (struct bcm_gk *)dev;

   int gk_init_err = gk_init(gk_hdl);
   if (gk_init_err) return -EINVAL;

   GatekeeperTl_Password gk_pwd, gk_orig_pwd;
   gk_password_handle_t gk_in_hdl, gk_out_hdl;
   bool gk_in_hdl_set = false, gk_in_pwd_set = false;
   uint32_t retry = 0;
   BERR_Code gk_err;
   memset(&gk_in_hdl, 0, sizeof(gk_password_handle_t));
   memset(&gk_out_hdl, 0, sizeof(gk_password_handle_t));
   if (current_password_handle && current_password_handle_length) {
      if (current_password_handle_length == sizeof(gk_password_handle_t)) {
         gk_in_hdl_set = true;
         memcpy(&gk_in_hdl, current_password_handle, current_password_handle_length);
      }
   }
   if (current_password && current_password_length) {
      gk_orig_pwd.size = current_password_length;
      gk_orig_pwd.buffer = (uint8_t *) current_password;
      gk_in_pwd_set = true;
   }
   gk_pwd.size = desired_password_length;
   gk_pwd.buffer = (uint8_t *) desired_password;
   gk_err = BERR_OS_ERROR;
   if (!pthread_mutex_lock(&gk_hdl->mtx)) {
      if (gk_hdl->handle)
         gk_err = GatekeeperTl_Enroll(gk_hdl->handle, uid,
            gk_in_hdl_set ? &gk_in_hdl : NULL,   /* optional */
            gk_in_pwd_set ? &gk_orig_pwd : NULL, /* optional */
            &gk_pwd, &retry, &gk_out_hdl);
      pthread_mutex_unlock(&gk_hdl->mtx);
   }
   if (gk_err != BERR_SUCCESS) {
      ALOGE("gk_enroll: failed, err: %u, retry: %u", gk_err, retry);
      if (retry > 0) {
         return retry;
      }
      return gk_berr_2_interr(gk_err);
   }
   *enrolled_password_handle = new uint8_t[sizeof(gk_password_handle_t)];
   if (*enrolled_password_handle == NULL) {
      ALOGE("gk_enroll: failed allocating password handle");
      return -ENOMEM;
   }
   memcpy(*enrolled_password_handle, &gk_out_hdl, sizeof(gk_password_handle_t));
   *enrolled_password_handle_length = sizeof(gk_password_handle_t);
   return 0;
}

static int brcm_gk_verify(
   const struct gatekeeper_device *dev,
   uint32_t uid,
   uint64_t challenge,
   const uint8_t *enrolled_password_handle,
   uint32_t enrolled_password_handle_length,
   const uint8_t *provided_password,
   uint32_t provided_password_length,
   uint8_t **auth_token,
   uint32_t *auth_token_length,
   bool *request_reenroll) {

   struct bcm_gk *gk_hdl = (struct bcm_gk *)dev;

   int gk_init_err = gk_init(gk_hdl);
   if (gk_init_err) return -EINVAL;

   BERR_Code gk_err;
   GatekeeperTl_Password gk_pwd;
   gk_password_handle_t gk_pwd_hdl;
   km_hw_auth_token_t gk_token;
   uint32_t retry;
   memset(&gk_pwd_hdl, 0, sizeof(gk_password_handle_t));
   if (enrolled_password_handle_length == sizeof(gk_password_handle_t)) {
      memcpy(&gk_pwd_hdl, enrolled_password_handle, enrolled_password_handle_length);
   }
   gk_pwd.size = provided_password_length;
   gk_pwd.buffer = (uint8_t *) provided_password;
   gk_err = BERR_OS_ERROR;
   if (!pthread_mutex_lock(&gk_hdl->mtx)) {
      if (gk_hdl->handle)
         gk_err = GatekeeperTl_Verify(gk_hdl->handle, uid,
            challenge, &gk_pwd_hdl, &gk_pwd, &retry, &gk_token);
      pthread_mutex_unlock(&gk_hdl->mtx);
   }
   if (gk_err != BERR_SUCCESS) {
      ALOGE("gk_verify: failed, err: %u, retry: %u", gk_err, retry);
      if (retry > 0) {
         return retry;
      }
      return gk_berr_2_interr(gk_err);
   }
   *auth_token = new uint8_t[sizeof(km_hw_auth_token_t)];
   if (*auth_token == NULL) {
      ALOGE("gk_verify: failed allocating auth-token");
      return -ENOMEM;
   }
   memcpy(*auth_token, &gk_token, sizeof(km_hw_auth_token_t));
   *auth_token_length = sizeof(km_hw_auth_token_t);
   *request_reenroll = false;  /* always. */
   return 0;
}

static int brcm_gk_close(
   hw_device_t *dev) {
   struct bcm_gk *gk_hdl = (struct bcm_gk *)dev;
   if (gk_hdl != NULL) {
      if (gk_hdl->nxwrap) {
         gk_hdl->nxwrap->leave();
         delete gk_hdl->nxwrap;
      }
      if (!pthread_mutex_lock(&gk_hdl->mtx)) {
         if (gk_hdl->handle)
            GatekeeperTl_Uninit(gk_hdl->handle);
         pthread_mutex_unlock(&gk_hdl->mtx);
      }
      pthread_mutex_destroy(&gk_hdl->mtx);
      if (gk_hdl->fbkCfg)
         free(gk_hdl->fbkCfg);
      free(gk_hdl);
   }
   free(dev);
   return 0;
}

static bool gk_stdby(
   void *context) {
   struct bcm_gk *gk_hdl =(struct bcm_gk *)context;

   if (gk_hdl == NULL)
      return true;
   if (pthread_mutex_lock(&gk_hdl->mtx))
      return true;
   if (gk_hdl->handle == NULL) {
      pthread_mutex_unlock(&gk_hdl->mtx);
      return true;
   }
   nxwrap_pwr_state pwr_s;
   bool pwr = nxwrap_get_pwr_info(&pwr_s, NULL);
   if (pwr && (pwr_s > ePowerState_S2)) {
      GatekeeperTl_Uninit(gk_hdl->handle);
      gk_hdl->handle = NULL;
   }
   pthread_mutex_unlock(&gk_hdl->mtx);
   return true;
}

static int brcm_gk_open(
   const hw_module_t *module,
   const char *name,
   hw_device_t **device) {

   char nexus[PROPERTY_VALUE_MAX];
   int c = 0;
   static int c_max = 20;
   pthread_mutexattr_t mattr;

   if (strcmp(name, HARDWARE_GATEKEEPER) != 0) {
      return -EINVAL;
   }

   struct bcm_gk *gk_hdl = (struct bcm_gk *)malloc(sizeof(struct bcm_gk));
   if (gk_hdl == NULL) {
      ALOGE("failed to alloc gatekeeper, aborting.");
      return -1;
   }
   memset(gk_hdl, 0, sizeof(struct bcm_gk));

   pthread_mutexattr_init(&mattr);
   pthread_mutex_init(&gk_hdl->mtx, &mattr);
   pthread_mutexattr_destroy(&mattr);

   // busy loop wait for nexus readiness, without valid client, the
   // gatekeeper cannot function.
   property_get(BCM_DYN_NX_STATE, nexus, "");
   while (1) {
      if (!strncmp(nexus, "loaded", strlen("loaded")))
         break;
      else {
         ALOGW("nexus not ready for gatekeeper, 0.25 second delay...");
         usleep(1000000/4);
         property_get(BCM_DYN_NX_STATE, nexus, "");
      }
   }
   gk_hdl->nxwrap = new NxWrap("gk");
   if (gk_hdl->nxwrap == NULL) {
      LOG_ALWAYS_FATAL("failed to alloc gatekeeper nexus client, aborting.");
      free(gk_hdl);
      return -EINVAL;
   }
   gk_hdl->nxwrap->join_v(gk_stdby, (void *)gk_hdl);
   gk_hdl->nxwrap->sraiClient();
   // busy loop wait for keymaster readiness, the gatekeeper is linked
   // to the keymaster through the TA service effectively.
   property_get(BCM_DYN_KM_STATE, nexus, "");
   while (1) {
      if (!strncmp(nexus, "init", strlen("init"))) {
         // all is well.
         ALOGI("gatekeeper can be started.");
         break;
      } else if (!strncmp(nexus, "ended", strlen("ended"))) {
         // keymaster attempted, but failed, try without.
         ALOGW("gatekeeper attempt despite keymaster failure.");
         break;
      } else {
         // something is wrong, ssd may not even be present.
         ALOGW("keymaster not ready for gatekeeper, 0.25 second delay...");
         usleep(1000000/4);
         property_get(BCM_DYN_KM_STATE, nexus, "");
         c++;
         if (c > c_max) {
            ALOGE("gatekeeper timeout waiting for keymaster service, proceed without.");
            break;
         }
      }
   }

   // now try to load up gatekeeper on tee side once.
   int gk_init_err = gk_init(gk_hdl);

   gk_hdl->dev.common.tag     = HARDWARE_DEVICE_TAG;
   gk_hdl->dev.common.version = 0;
   gk_hdl->dev.common.module  = (struct hw_module_t*) module;
   gk_hdl->dev.common.close   = brcm_gk_close;

   gk_hdl->dev.enroll            = brcm_gk_enroll;
   gk_hdl->dev.verify            = brcm_gk_verify;
   gk_hdl->dev.delete_user       = nullptr;
   gk_hdl->dev.delete_all_users  = nullptr;

   *device = (hw_device_t*) &gk_hdl->dev.common;
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

