/*
 *  Copyright (C) 2018 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you
 *  may not use this file except in compliance with the License.  You may
 *  obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 *  implied.  See the License for the specific language governing
 *  permissions and limitations under the License.
 */

//#define LOG_NDEBUG 0
#define KM_LOG_ALL_IN 1

#define LOG_TAG "bcm-km"
#include <log/log.h>

#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <ftw.h>
#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <hardware/keymaster_common.h>
#include <hardware/keymaster2.h>
#include <keymaster/authorization_set.h>
#include <keymaster/android_keymaster_utils.h>
#include <keymaster/UniquePtr.h>

#include "sage_srai.h"
#include <keymaster_tl.h>
extern "C" {
#include <sage_manufacturing_api.h>
#include "nexus_security_datatypes.h"
#if (NEXUS_SECURITY_API_VERSION == 1)
#include "nexus_otpmsp.h"
#else
#include "nexus_otp_msp.h"
#endif
}

extern "C" void* nxwrap_create_verified_client(void **wrap);
extern "C" void nxwrap_destroy_client(void *wrap);

using namespace keymaster;

typedef UniquePtr<keymaster2_device_t> Unique_keymaster_device_t;
struct bcm_km {
   void *nxwrap;
   KeymasterTl_Handle handle;
   KeymasterTl_InitSettings is;
   size_t maxFinishInput;
   uint8_t *fbkCfg;
   size_t fbkSize;
};

static uint8_t *km_dup_2_kmblob(
   uint8_t *buffer,
   uint32_t size) {
   if (size == 0 || buffer == NULL) {
      return NULL;
   }
   uint8_t *dup = reinterpret_cast<uint8_t*>(malloc(size));
   if (dup) {
      memcpy(dup, buffer, size);
   }
   return dup;
}

static int km_close(
   hw_device_t *dev) {
   keymaster2_device_t* km_dev = (keymaster2_device_t *)dev;
   struct bcm_km *km_hdl =(struct bcm_km *)km_dev->context;
   if (km_hdl != NULL) {
      if (km_hdl->nxwrap)
         nxwrap_destroy_client(km_hdl->nxwrap);
      if (km_hdl->handle)
         KeymasterTl_Uninit(km_hdl->handle);
      if (km_hdl->fbkCfg)
         free(km_hdl->fbkCfg);
      free(km_hdl);
   }
   free(dev);
   return 0;
}

static keymaster_error_t km_berr_2_kmerr(
   BERR_Code berr) {
   switch (berr) {
   case BERR_INVALID_PARAMETER: return KM_ERROR_INVALID_ARGUMENT; break;
   case BERR_OUT_OF_DEVICE_MEMORY: return KM_ERROR_MEMORY_ALLOCATION_FAILED; break;
   // berr matching km errors.
   case BSAGE_ERR_KM_ROOT_OF_TRUST_ALREADY_SET: return KM_ERROR_ROOT_OF_TRUST_ALREADY_SET; break;
   case BSAGE_ERR_KM_UNSUPPORTED_PURPOSE: return KM_ERROR_UNSUPPORTED_PURPOSE; break;
   case BSAGE_ERR_KM_INCOMPATIBLE_PURPOSE: return KM_ERROR_INCOMPATIBLE_PURPOSE; break;
   case BSAGE_ERR_KM_UNSUPPORTED_ALGORITHM: return KM_ERROR_UNSUPPORTED_ALGORITHM; break;
   case BSAGE_ERR_KM_INCOMPATIBLE_ALGORITHM: return KM_ERROR_INCOMPATIBLE_ALGORITHM; break;
   case BSAGE_ERR_KM_UNSUPPORTED_KEY_SIZE: return KM_ERROR_UNSUPPORTED_KEY_SIZE; break;
   case BSAGE_ERR_KM_UNSUPPORTED_BLOCK_MODE: return KM_ERROR_UNSUPPORTED_BLOCK_MODE; break;
   case BSAGE_ERR_KM_INCOMPATIBLE_BLOCK_MODE: return KM_ERROR_INCOMPATIBLE_BLOCK_MODE; break;
   case BSAGE_ERR_KM_UNSUPPORTED_MAC_LENGTH: return KM_ERROR_UNSUPPORTED_MAC_LENGTH; break;
   case BSAGE_ERR_KM_UNSUPPORTED_PADDING_MODE: return KM_ERROR_UNSUPPORTED_PADDING_MODE; break;
   case BSAGE_ERR_KM_INCOMPATIBLE_PADDING_MODE: return KM_ERROR_INCOMPATIBLE_PADDING_MODE; break;
   case BSAGE_ERR_KM_UNSUPPORTED_DIGEST: return KM_ERROR_UNSUPPORTED_DIGEST; break;
   case BSAGE_ERR_KM_INCOMPATIBLE_DIGEST: return KM_ERROR_INCOMPATIBLE_DIGEST; break;
   case BSAGE_ERR_KM_INVALID_EXPIRATION_TIME: return KM_ERROR_INVALID_EXPIRATION_TIME; break;
   case BSAGE_ERR_KM_INVALID_USER_ID: return KM_ERROR_INVALID_USER_ID; break;
   case BSAGE_ERR_KM_INVALID_AUTHORIZATION_TIMEOUT: return KM_ERROR_INVALID_AUTHORIZATION_TIMEOUT; break;
   case BSAGE_ERR_KM_UNSUPPORTED_KEY_FORMAT: return KM_ERROR_UNSUPPORTED_KEY_FORMAT; break;
   case BSAGE_ERR_KM_INCOMPATIBLE_KEY_FORMAT: return KM_ERROR_INCOMPATIBLE_KEY_FORMAT; break;
   case BSAGE_ERR_KM_UNSUPPORTED_KEY_ENCRYPTION_ALGORITHM: return KM_ERROR_UNSUPPORTED_KEY_ENCRYPTION_ALGORITHM; break;
   case BSAGE_ERR_KM_UNSUPPORTED_KEY_VERIFICATION_ALGORITHM: return KM_ERROR_UNSUPPORTED_KEY_VERIFICATION_ALGORITHM; break;
   case BSAGE_ERR_KM_INVALID_INPUT_LENGTH: return KM_ERROR_INVALID_INPUT_LENGTH; break;
   case BSAGE_ERR_KM_KEY_EXPORT_OPTIONS_INVALID: return KM_ERROR_KEY_EXPORT_OPTIONS_INVALID; break;
   case BSAGE_ERR_KM_DELEGATION_NOT_ALLOWED: return KM_ERROR_DELEGATION_NOT_ALLOWED; break;
   case BSAGE_ERR_KM_KEY_NOT_YET_VALID: return KM_ERROR_KEY_NOT_YET_VALID; break;
   case BSAGE_ERR_KM_KEY_EXPIRED: return KM_ERROR_KEY_EXPIRED; break;
   case BSAGE_ERR_KM_KEY_USER_NOT_AUTHENTICATED: return KM_ERROR_KEY_USER_NOT_AUTHENTICATED; break;
   case BSAGE_ERR_KM_OUTPUT_PARAMETER_NULL: return KM_ERROR_OUTPUT_PARAMETER_NULL; break;
   case BSAGE_ERR_KM_INVALID_OPERATION_HANDLE: return KM_ERROR_INVALID_OPERATION_HANDLE; break;
   case BSAGE_ERR_KM_INSUFFICIENT_BUFFER_SPACE: return KM_ERROR_INSUFFICIENT_BUFFER_SPACE; break;
   case BSAGE_ERR_KM_VERIFICATION_FAILED: return KM_ERROR_VERIFICATION_FAILED; break;
   case BSAGE_ERR_KM_TOO_MANY_OPERATIONS: return KM_ERROR_TOO_MANY_OPERATIONS; break;
   case BSAGE_ERR_KM_UNEXPECTED_NULL_POINTER: return KM_ERROR_UNEXPECTED_NULL_POINTER; break;
   case BSAGE_ERR_KM_INVALID_KEY_BLOB: return KM_ERROR_INVALID_KEY_BLOB; break;
   case BSAGE_ERR_KM_IMPORTED_KEY_NOT_ENCRYPTED: return KM_ERROR_IMPORTED_KEY_NOT_ENCRYPTED; break;
   case BSAGE_ERR_KM_IMPORTED_KEY_DECRYPTION_FAILED: return KM_ERROR_IMPORTED_KEY_DECRYPTION_FAILED; break;
   case BSAGE_ERR_KM_IMPORTED_KEY_NOT_SIGNED: return KM_ERROR_IMPORTED_KEY_NOT_SIGNED; break;
   case BSAGE_ERR_KM_IMPORTED_KEY_VERIFICATION_FAILED: return KM_ERROR_IMPORTED_KEY_VERIFICATION_FAILED; break;
   case BSAGE_ERR_KM_UNSUPPORTED_TAG: return KM_ERROR_UNSUPPORTED_TAG; break;
   case BSAGE_ERR_KM_INVALID_TAG: return KM_ERROR_INVALID_TAG; break;
   case BSAGE_ERR_KM_IMPORT_PARAMETER_MISMATCH: return KM_ERROR_IMPORT_PARAMETER_MISMATCH; break;
   case BSAGE_ERR_KM_SECURE_HW_ACCESS_DENIED: return KM_ERROR_SECURE_HW_ACCESS_DENIED; break;
   case BSAGE_ERR_KM_OPERATION_CANCELLED: return KM_ERROR_OPERATION_CANCELLED; break;
   case BSAGE_ERR_KM_CONCURRENT_ACCESS_CONFLICT: return KM_ERROR_CONCURRENT_ACCESS_CONFLICT; break;
   case BSAGE_ERR_KM_SECURE_HW_BUSY: return KM_ERROR_SECURE_HW_BUSY; break;
   case BSAGE_ERR_KM_UNSUPPORTED_EC_FIELD: return KM_ERROR_UNSUPPORTED_EC_FIELD; break;
   case BSAGE_ERR_KM_MISSING_NONCE: return KM_ERROR_MISSING_NONCE; break;
   case BSAGE_ERR_KM_INVALID_NONCE: return KM_ERROR_INVALID_NONCE; break;
   case BSAGE_ERR_KM_MISSING_MAC_LENGTH: return KM_ERROR_MISSING_MAC_LENGTH; break;
   case BSAGE_ERR_KM_KEY_RATE_LIMIT_EXCEEDED: return KM_ERROR_KEY_RATE_LIMIT_EXCEEDED; break;
   case BSAGE_ERR_KM_CALLER_NONCE_PROHIBITED: return KM_ERROR_CALLER_NONCE_PROHIBITED; break;
   case BSAGE_ERR_KM_KEY_MAX_OPS_EXCEEDED: return KM_ERROR_KEY_MAX_OPS_EXCEEDED; break;
   case BSAGE_ERR_KM_INVALID_MAC_LENGTH: return KM_ERROR_INVALID_MAC_LENGTH; break;
   case BSAGE_ERR_KM_MISSING_MIN_MAC_LENGTH: return KM_ERROR_MISSING_MIN_MAC_LENGTH; break;
   case BSAGE_ERR_KM_UNSUPPORTED_MIN_MAC_LENGTH: return KM_ERROR_UNSUPPORTED_MIN_MAC_LENGTH; break;
   case BSAGE_ERR_KM_UNSUPPORTED_KDF: return KM_ERROR_UNSUPPORTED_KDF; break;
   case BSAGE_ERR_KM_UNSUPPORTED_EC_CURVE: return KM_ERROR_UNSUPPORTED_EC_CURVE; break;
   case BSAGE_ERR_KM_KEY_REQUIRES_UPGRADE: return KM_ERROR_KEY_REQUIRES_UPGRADE; break;
   case BSAGE_ERR_KM_ATTESTATION_CHALLENGE_MISSING: return KM_ERROR_ATTESTATION_CHALLENGE_MISSING; break;
   case BSAGE_ERR_KM_KEYMASTER_NOT_CONFIGURED: return KM_ERROR_KEYMASTER_NOT_CONFIGURED; break;
   case BSAGE_ERR_KM_VERSION_MISMATCH: return KM_ERROR_VERSION_MISMATCH; break;
   default: return KM_ERROR_UNKNOWN_ERROR;
   }
}

static int km_berr_2_interr(
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
   ALOGE("km_berr_2_interr: %s (%x -> %d)", rc==0?"success":"FAILED", berr, rc);
   return ((rc==0)?rc:-rc);
}

#define OTP_MSP0_VALUE_ZS (0x02)
#define OTP_MSP1_VALUE_ZS (0x02)
#if (NEXUS_SECURITY_API_VERSION == 1)
static bool km_chip_zb(void) {
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
static bool km_chip_zb(void) {
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

#define KM_ZD_PROV_FALLBACK_PATH "/vendor/usr/kmgk/km.zd.bin"
#define KM_ZB_PROV_FALLBACK_PATH "/vendor/usr/kmgk/km.zb.bin"
static BERR_Code km_fallback(struct bcm_km *km_hdl) {
   BERR_Code km_err = BSAGE_ERR_BFM_DRM_TYPE_NOT_FOUND;
   FILE *f = NULL;
   size_t km_size, c;
   int rc, v, s;
   uint8_t *d;
   char *key_path = (char *)((km_chip_zb() == true) ? KM_ZB_PROV_FALLBACK_PATH : KM_ZD_PROV_FALLBACK_PATH);

   if (access(key_path, R_OK)) {
      ALOGE("km_fallback: no key accessible @%s, aborting.", key_path);
      goto out;
   }

   // always use otp index A for this fallback.
   km_err = SAGE_Manufacturing_Init(SAGE_Manufacturing_OTP_Index_A);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_fallback: cannot setup manufacturing (%d).", km_err);
      km_err = BSAGE_ERR_BFM_DRM_TYPE_NOT_FOUND;
      goto out;
   }

   // input manipulation and copy to sage.
   km_err = BSAGE_ERR_BFM_DRM_TYPE_NOT_FOUND;
   f = fopen(key_path, "rb");
   if (f == NULL) goto out_fin;
   if(fseek (f, 0, SEEK_END) != 0) goto out_fin;
   km_size = ftell(f);
   if (fseek(f, 0, SEEK_SET) != 0) goto out_fin;

   rc = SAGE_Manufacturing_AllocBinBuffer(km_size, &d);
   if (rc) goto out_fin;
   km_hdl->fbkCfg = (uint8_t *)malloc(sizeof(uint8_t)*km_size);
   if (!km_hdl->fbkCfg) goto out_dealloc;

   c = fread(d, 1, km_size, f);
   if (c != km_size) {
      ALOGE("km_fallback: failed to copy %s (%d) to sage, copied %d",
         key_path, km_size, c);
      goto out_error;
   }

   // now the real work to validate the key and transform it if necessay.
   rc = SAGE_Manufacturing_BinFile_ParseAndDisplay(d, km_size, &v);
   if (rc) goto out_error;

   rc = SAGE_Manufacturing_VerifyDrmBinFileType(d, v);
   if (rc <= DRM_BIN_FILE_TYPE_2) {
      rc = 0;
      km_err = SAGE_Manufacturing_Provision_BinData(&rc);
      if (km_err != BERR_SUCCESS || rc) {
         km_err = BSAGE_ERR_BFM_DRM_TYPE_NOT_FOUND;
         goto out_error;
      }
      km_err = BSAGE_ERR_BFM_DRM_TYPE_NOT_FOUND;
   } else if (rc == DRM_BIN_FILE_TYPE_3 || rc == DRM_BIN_FILE_TYPE_3_PRIME) {
      // already valid format (strange, but why not).
   } else goto out_error;

   // note: there is no alidation command associated with keymaster drm, so
   //       skip that part and hope for the best.

   // copy the updated key file, to be used to retry loading keymaster.
   memcpy(km_hdl->fbkCfg, d, km_size);
   km_hdl->fbkSize = km_size;
   km_err = BERR_SUCCESS;
   goto out_dealloc;

out_error:
   free(km_hdl->fbkCfg);
   km_hdl->fbkCfg = NULL;
   km_hdl->fbkSize = 0;
out_dealloc:
   SAGE_Manufacturing_DeallocBinBuffer();
out_fin:
   if (f) {
      fclose(f);
      f = NULL;
   }
   SAGE_Manufacturing_Uninit();
out:
   ALOGW("km_fallback: exit with code %x (%u)", km_err, km_err);
   return km_err;
}

static int km_init(struct bcm_km *km_hdl) {
   BERR_Code km_err;

   // already good to go.
   if (km_hdl->handle) {
      return 0;
   }

   ALOGI_IF(KM_LOG_ALL_IN, "km_init: starting tl-side.");
   KeymasterTl_GetDefaultInitSettings(&km_hdl->is);
   km_err = KeymasterTl_Init(&km_hdl->handle, &km_hdl->is);
   if (km_err != BERR_SUCCESS) {
      ALOGI_IF(KM_LOG_ALL_IN, "km_init: tl error'ed out: %x (%u).", km_err, km_err);
      if (km_err == BSAGE_ERR_BFM_DRM_TYPE_NOT_FOUND) {
         km_err = km_fallback(km_hdl);
         ALOGI_IF(KM_LOG_ALL_IN, "km_init: tl fallback: %x (%u).", km_err, km_err);
         if (km_err == BERR_SUCCESS) {
            memset(km_hdl->is.drm_binfile_path, 0,
                   sizeof(km_hdl->is.drm_binfile_path));
            km_hdl->is.drm_binfile_buffer = km_hdl->fbkCfg;
            km_hdl->is.drm_binfile_size = km_hdl->fbkSize;
            // will return BERR_SUCCESS if fallback succeeded.
            km_err = KeymasterTl_Init(&km_hdl->handle, &km_hdl->is);
         }
      }
      ALOGI_IF(KM_LOG_ALL_IN, "km_init: tl final: (%x) %u.", km_err, km_err);
      property_set("dyn.nx.km.state", (km_err == BERR_SUCCESS) ? "init" : "ended");
      return km_berr_2_interr(km_err);
   }

   property_set("dyn.nx.km.state", (km_err == BERR_SUCCESS) ? "init" : "ended");
   return 0;
}

static keymaster_error_t km_config(
   const struct keymaster2_device* dev,
   const keymaster_key_param_set_t* params) {

   keymaster2_device_t* km_dev = (keymaster2_device_t *)dev;
   struct bcm_km *km_hdl =(struct bcm_km *)km_dev->context;
   if (km_hdl == NULL) {
      ALOGE("km_config: null bcm-km handle?!");
      return KM_ERROR_UNKNOWN_ERROR;
   }
   if (params == NULL) {
      ALOGE("km_config: null params?!");
      return KM_ERROR_UNEXPECTED_NULL_POINTER;
   }

   BERR_Code km_err;
   KM_Tag_ContextHandle km_params = NULL;
   uint32_t os_version, os_level;

   int km_init_err = km_init(km_hdl);
   // init should always have succeeded in km_config technically.
   if (km_init_err) return KM_ERROR_INVALID_ARGUMENT;

   km_err = KM_Tag_NewContext(&km_params);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_config: failed tag-context: %u", km_err);
      return km_berr_2_kmerr(km_err);
   }
   AuthorizationSet set;
   set.Reinitialize(*params);
   if (!set.GetTagValue(TAG_OS_VERSION, &os_version) ||
       !set.GetTagValue(TAG_OS_PATCHLEVEL, &os_level)) {
      ALOGE("km_config: invalid params input");
      KM_Tag_DeleteContext(km_params);
      return KM_ERROR_INVALID_ARGUMENT;
   }
   km_err = KM_Tag_AddInteger(km_params, SKM_TAG_OS_VERSION, os_version);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_config: failed tag-integer: %u, err: %u (%d)", KM_TAG_OS_VERSION,
         km_err, km_berr_2_kmerr(km_err));
      KM_Tag_DeleteContext(km_params);
      return km_berr_2_kmerr(km_err);
   }
   ALOGI_IF(KM_LOG_ALL_IN, "km_config: added tag: %u, value: %u", KM_TAG_OS_VERSION, os_version);
   km_err = KM_Tag_AddInteger(km_params, SKM_TAG_OS_PATCHLEVEL, os_level);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_config: failed tag-integer: %u, err: %u (%d)", KM_TAG_OS_PATCHLEVEL,
         km_err, km_berr_2_kmerr(km_err));
      KM_Tag_DeleteContext(km_params);
      return km_berr_2_kmerr(km_err);
   }
   ALOGI_IF(KM_LOG_ALL_IN, "km_config: added tag: %u, value: %u", KM_TAG_OS_PATCHLEVEL, os_level);
   km_err = KeymasterTl_Configure(km_hdl->handle, km_params);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_config: failed configuration: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      KM_Tag_DeleteContext(km_params);
      return km_berr_2_kmerr(km_err);
   }

   return KM_ERROR_OK;
}

static keymaster_error_t km_add_rng_entropy(
   const struct keymaster2_device* dev,
   const uint8_t* data,
   size_t data_length) {

   keymaster2_device_t* km_dev = (keymaster2_device_t *)dev;
   struct bcm_km *km_hdl =(struct bcm_km *)km_dev->context;
   if (km_hdl == NULL) {
      ALOGE("km_add_rng_entropy: null bcm-km handle?!");
      return KM_ERROR_UNKNOWN_ERROR;
   }

   int km_init_err = km_init(km_hdl);
   // init failed on subsequent calls post km_config.
   if (km_init_err) return KM_ERROR_KEYMASTER_NOT_CONFIGURED;

   BERR_Code km_err;
   KeymasterTl_DataBlock km_db;
   memset(&km_db, 0, sizeof(km_db));
   if (data == NULL || !data_length) {
      ALOGE("km_add_rng_entropy: null params?!");
      return KM_ERROR_UNEXPECTED_NULL_POINTER;
   }
   km_db.buffer = (uint8_t *)SRAI_Memory_Allocate(data_length, SRAI_MemoryType_Shared);
   if (!km_db.buffer) {
      ALOGE("km_add_rng_entropy: failed srai-alloc-size: %u", data_length);
      return km_berr_2_kmerr(BERR_OUT_OF_SYSTEM_MEMORY);
   }
   km_db.size = data_length;
   memcpy(km_db.buffer, data, km_db.size);
   km_err = KeymasterTl_AddRngEntropy(km_hdl->handle, &km_db);
   SRAI_Memory_Free(km_db.buffer);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_add_rng_entropy: failed: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      return km_berr_2_kmerr(km_err);
   }

   return KM_ERROR_OK;
}

static keymaster_error_t km_generate_key(
   const struct keymaster2_device* dev,
   const keymaster_key_param_set_t* params,
   keymaster_key_blob_t* key_blob,
   keymaster_key_characteristics_t* characteristics) {

   keymaster2_device_t* km_dev = (keymaster2_device_t *)dev;
   struct bcm_km *km_hdl =(struct bcm_km *)km_dev->context;
   if (km_hdl == NULL) {
      ALOGE("km_generate_key: null bcm-km handle?!");
      return KM_ERROR_UNKNOWN_ERROR;
   }
   if (params == NULL) {
      ALOGE("km_generate_key: null params?!");
      return KM_ERROR_UNEXPECTED_NULL_POINTER;
   }
   if (key_blob == NULL) {
      ALOGE("km_generate_key: null ouput-params?!");
      return KM_ERROR_OUTPUT_PARAMETER_NULL;
   }

   int km_init_err = km_init(km_hdl);
   // init failed on subsequent calls post km_config.
   if (km_init_err) return KM_ERROR_KEYMASTER_NOT_CONFIGURED;

   BERR_Code km_err;
   KeymasterTl_DataBlock km_key;
   int ix;
   keymaster_algorithm_t km_algo;
   keymaster_purpose_t km_purpose;
   hw_authenticator_type_t km_authtype;
   uint64_t km_secid;
   uint32_t km_timeout;
   memset(&km_key, 0, sizeof(km_key));

   AuthorizationSet set;
   set.Reinitialize(*params);
   if (set.is_valid() != keymaster::AuthorizationSet::OK) {
      ALOGE("km_generate_key: failed to build set for in params");
      return KM_ERROR_UNKNOWN_ERROR;
   }
   // validate inputs.
   // mandatory: KM_TAG_ALGORITHM
   if (!set.GetTagValue(TAG_ALGORITHM, &km_algo)) {
      ALOGE("km_generate_key: missing mandatory TAG_ALGORITHM");
      return KM_ERROR_INVALID_ARGUMENT;
   }
   ALOGI_IF(KM_LOG_ALL_IN, "km_generate_key: tag: TAG_ALGORITHM, value: %u", km_algo);
   // mandatory: KM_TAG_PURPOSE (one or more)
   ix = 0;
   do {
      if (!set.GetTagValue(TAG_PURPOSE, ix, &km_purpose)) break;
      ix++;
      ALOGI_IF(KM_LOG_ALL_IN, "km_generate_key: tag: TAG_PURPOSE[%d], value: %u", ix, km_purpose);
   } while (true);
   if (!ix) {
      ALOGE("km_generate_key: missing mandatory TAG_PURPOSE");
      return KM_ERROR_INVALID_ARGUMENT;
   }
   // mandatory: (KM_TAG_USER_SECURE_ID and KM_TAG_USER_AUTH_TYPE) or KM_TAG_NO_AUTH_REQUIRED
   if (set.GetTagValue(TAG_NO_AUTH_REQUIRED)) {
      ALOGI_IF(KM_LOG_ALL_IN, "km_generate_key: tag: TAG_NO_AUTH_REQUIRED");
   } else if (set.GetTagValue(TAG_USER_SECURE_ID, &km_secid) &&
              set.GetTagValue(TAG_USER_AUTH_TYPE, &km_authtype)) {
      ALOGI_IF(KM_LOG_ALL_IN, "km_generate_key: tag: TAG_USER_SECURE_ID, value: %llu", km_secid);
      ALOGI_IF(KM_LOG_ALL_IN, "km_generate_key: tag: TAG_USER_AUTH_TYPE, value: %u", km_authtype);
   } else {
      ALOGE("km_generate_key: missing mandatory (TAG_USER_SECURE_ID,TAG_USER_AUTH_TYPE)|TAG_NO_AUTH_REQUIRED,");
      return KM_ERROR_INVALID_ARGUMENT;
   }
   // recommended: KM_TAG_AUTH_TIMEOUT, unless KM_TAG_NO_AUTH_REQUIRED
   if (set.GetTagValue(TAG_AUTH_TIMEOUT, &km_timeout)) {
      ALOGI_IF(KM_LOG_ALL_IN, "km_generate_key: tag: TAG_AUTH_TIMEOUT, value: %u", km_timeout);
   }
   // optional: KM_TAG_ORIGIN, KM_TAG_ROLLBACK_RESISTANT, KM_TAG_CREATION_DATETIME - ignored.
   size_t serial_len = set.SerializedSize();
   UniquePtr<uint8_t[]> serial(new uint8_t[serial_len]);
   set.Serialize(serial.get(), serial.get() + serial_len);
   KM_Tag_ContextHandle km_params = NULL;
   km_err = KM_Tag_CreateContextFromAndroidBlob(serial.get(), serial.get() + serial_len, &km_params);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_generate_key: failed create-context: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      return km_berr_2_kmerr(km_err);
   }
   // insert a "all applications" tag if none specified.
   if (!set.Contains(TAG_APPLICATION_ID) && !set.Contains(TAG_ALL_APPLICATIONS)) {
      KM_Tag_AddBool(km_params, SKM_TAG_ALL_APPLICATIONS, true);
   }
   // insert a creation time because there is no wall clock in sage.
   if (!set.Contains(TAG_CREATION_DATETIME)) {
      KM_Tag_AddDate(km_params, SKM_TAG_CREATION_DATETIME, java_time(time(NULL)));
   }
   // generate.
   km_err = KeymasterTl_GenerateKey(km_hdl->handle, km_params, &km_key);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_generate_key: failed generating key, err: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      KM_Tag_DeleteContext(km_params);
      if (km_key.buffer) SRAI_Memory_Free(km_key.buffer);
      return km_berr_2_kmerr(km_err);
   }
   // copy key in the passed in blob, passing ownership.
   key_blob->key_material_size = km_key.size;
   key_blob->key_material = km_dup_2_kmblob(km_key.buffer, km_key.size);
   if (!key_blob->key_material) {
      KM_Tag_DeleteContext(km_params);
      if (km_key.buffer) {
         (void)KeymasterTl_DeleteKey(km_hdl->handle, &km_key);
         SRAI_Memory_Free(km_key.buffer);
      }
      ALOGE("km_generate_key: failed copying generated key");
      return KM_ERROR_MEMORY_ALLOCATION_FAILED;
   }

   // reply if requested.
   if (characteristics) {
      KeymasterTl_GetKeyCharacteristicsSettings km_characs;
      KeymasterTl_GetDefaultKeyCharacteristicsSettings(&km_characs);
      km_characs.in_key_blob.buffer = km_key.buffer;
      km_characs.in_key_blob.size = km_key.size;
      km_characs.in_params = km_params;
      km_err = KeymasterTl_GetKeyCharacteristics(km_hdl->handle, &km_characs);
      if (km_err != BERR_SUCCESS) {
         ALOGE("km_generate_key: failed getting key characteristics, err: %u (%d)",
            km_err, km_berr_2_kmerr(km_err));
         // error out? (if so, kill km_key.buffer && km_params).
      } else {
         uint32_t serial_size;
         uint8_t *serial;
         ix = KM_Tag_GetNumPairs(km_characs.out_hw_enforced);
         if (ix) {
            km_err = KM_Tag_SerializeToAndroidBlob(km_characs.out_hw_enforced, NULL, &serial_size);
            if (km_err == BERR_SUCCESS) {
               serial = reinterpret_cast<uint8_t*>(malloc(serial_size));
               if (serial != NULL) {
                  km_err = KM_Tag_SerializeToAndroidBlob(km_characs.out_hw_enforced, serial, &serial_size);
                  if (km_err == BERR_SUCCESS) {
                     AuthorizationSet set;
                     const uint8_t *s = serial;
                     if (!set.Deserialize(&s, serial+serial_size)) {
                        ALOGE("km_generate_key: failed importing hw_enforced");
                        // continue as we may still work with the key.
                     } else {
                        set.CopyToParamSet(&characteristics->hw_enforced);
                     }
                  }
                  free(serial);
               }
            }
         }
         KM_Tag_DeleteContext(km_characs.out_hw_enforced);
         ix = KM_Tag_GetNumPairs(km_characs.out_sw_enforced);
         if (ix) {
            km_err = KM_Tag_SerializeToAndroidBlob(km_characs.out_sw_enforced, NULL, &serial_size);
            if (km_err == BERR_SUCCESS) {
               serial = reinterpret_cast<uint8_t*>(malloc(serial_size));
               if (serial != NULL) {
                  km_err = KM_Tag_SerializeToAndroidBlob(km_characs.out_sw_enforced, serial, &serial_size);
                  if (km_err == BERR_SUCCESS) {
                     AuthorizationSet set;
                     const uint8_t *s = serial;
                     if (!set.Deserialize(&s, serial+serial_size)) {
                        ALOGE("km_generate_key: failed importing sw_enforced");
                        // continue as we may still work with the key.
                     } else {
                        set.CopyToParamSet(&characteristics->sw_enforced);
                     }
                  }
                  free(serial);
               }
            }
         }
         KM_Tag_DeleteContext(km_characs.out_sw_enforced);
      }
   }

   KM_Tag_DeleteContext(km_params);
   if (km_key.buffer) SRAI_Memory_Free(km_key.buffer);

   return KM_ERROR_OK;
}

static keymaster_error_t km_get_key_characteristics(
   const struct keymaster2_device* dev,
   const keymaster_key_blob_t* key_blob,
   const keymaster_blob_t* client_id,
   const keymaster_blob_t* app_data,
   keymaster_key_characteristics_t* characteristics) {

   keymaster2_device_t* km_dev = (keymaster2_device_t *)dev;
   struct bcm_km *km_hdl =(struct bcm_km *)km_dev->context;
   if (km_hdl == NULL) {
      ALOGE("km_get_key_characteristics: null bcm-km handle?!");
      return KM_ERROR_UNKNOWN_ERROR;
   }
   if (!key_blob || !key_blob->key_material) {
      ALOGE("km_get_key_characteristics: null key-blob in?!");
      return KM_ERROR_UNEXPECTED_NULL_POINTER;
   }
   if (!characteristics) {
      ALOGE("km_get_key_characteristics: null out?!");
      return KM_ERROR_OUTPUT_PARAMETER_NULL;
   }

   int km_init_err = km_init(km_hdl);
   // init failed on subsequent calls post km_config.
   if (km_init_err) return KM_ERROR_KEYMASTER_NOT_CONFIGURED;

   BERR_Code km_err;
   KM_Tag_ContextHandle km_params = NULL;
   KeymasterTl_GetKeyCharacteristicsSettings km_characs;
   int ix;
   KeymasterTl_GetDefaultKeyCharacteristicsSettings(&km_characs);
   km_characs.in_key_blob.buffer = (uint8_t *)key_blob->key_material;
   km_characs.in_key_blob.size = key_blob->key_material_size;
   if ((client_id && client_id->data) || (app_data && app_data->data)) {
      km_err = KM_Tag_NewContext(&km_params);
      if (km_err != BERR_SUCCESS) {
         ALOGE("km_get_key_characteristics: failed tag-context: %u", km_err);
         return km_berr_2_kmerr(km_err);
      }
      if (client_id && client_id->data) {
         km_err = KM_Tag_AddBlob(km_params, SKM_TAG_APPLICATION_ID, client_id->data_length, client_id->data);
         if (km_err != BERR_SUCCESS) {
            ALOGE("km_get_key_characteristics: failed tag-blob: %u, err: %u (%d)", KM_TAG_APPLICATION_ID,
               km_err, km_berr_2_kmerr(km_err));
            KM_Tag_DeleteContext(km_params);
            return km_berr_2_kmerr(km_err);
         }
         ALOGI_IF(KM_LOG_ALL_IN, "km_get_key_characteristics: added tag: %u, length: %u", KM_TAG_APPLICATION_ID, client_id->data_length);
      }
      if (app_data && app_data->data) {
         km_err = KM_Tag_AddBlob(km_params, SKM_TAG_APPLICATION_DATA, app_data->data_length, app_data->data);
         if (km_err != BERR_SUCCESS) {
            ALOGE("km_get_key_characteristics: failed tag-blob: %u, err: %u (%d)", KM_TAG_APPLICATION_DATA,
               km_err, km_berr_2_kmerr(km_err));
            KM_Tag_DeleteContext(km_params);
            return km_berr_2_kmerr(km_err);
         }
         ALOGI_IF(KM_LOG_ALL_IN, "km_get_key_characteristics: added tag: %u, length: %u", KM_TAG_APPLICATION_DATA, app_data->data_length);
      }
   }
   km_characs.in_params = km_params;
   km_err = KeymasterTl_GetKeyCharacteristics(km_hdl->handle, &km_characs);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_get_key_characteristics: failed getting key characteristics, err: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      if (km_characs.in_params) KM_Tag_DeleteContext(km_params);
      return km_berr_2_kmerr(km_err);
   }
   if (km_characs.in_params) KM_Tag_DeleteContext(km_params);

   uint32_t serial_size;
   uint8_t *serial;
   keymaster_error_t kme = KM_ERROR_OK;

   ix = KM_Tag_GetNumPairs(km_characs.out_hw_enforced);
   if (ix) {
      km_err = KM_Tag_SerializeToAndroidBlob(km_characs.out_hw_enforced, NULL, &serial_size);
      if (km_err == BERR_SUCCESS) {
         serial = reinterpret_cast<uint8_t*>(malloc(serial_size));
         if (serial != NULL) {
            km_err = KM_Tag_SerializeToAndroidBlob(km_characs.out_hw_enforced, serial, &serial_size);
            if (km_err == BERR_SUCCESS) {
               AuthorizationSet set;
               const uint8_t *s = serial;
               if (!set.Deserialize(&s, serial+serial_size)) {
                  ALOGE("km_get_key_characteristics: failed importing hw_enforced");
                  kme = KM_ERROR_UNKNOWN_ERROR;
               } else {
                  set.CopyToParamSet(&characteristics->hw_enforced);
               }
            }
            free(serial);
         }
      }
   }
   KM_Tag_DeleteContext(km_characs.out_hw_enforced);
   ix = KM_Tag_GetNumPairs(km_characs.out_sw_enforced);
   if (ix) {
      km_err = KM_Tag_SerializeToAndroidBlob(km_characs.out_sw_enforced, NULL, &serial_size);
      if (km_err == BERR_SUCCESS) {
         serial = reinterpret_cast<uint8_t*>(malloc(serial_size));
         if (serial != NULL) {
            km_err = KM_Tag_SerializeToAndroidBlob(km_characs.out_sw_enforced, serial, &serial_size);
            if (km_err == BERR_SUCCESS) {
               AuthorizationSet set;
               const uint8_t *s = serial;
               if (!set.Deserialize(&s, serial+serial_size)) {
                  ALOGE("km_get_key_characteristics: failed importing sw_enforced");
                  kme = KM_ERROR_UNKNOWN_ERROR;
               } else {
                  set.CopyToParamSet(&characteristics->sw_enforced);
               }
            }
            free(serial);
         }
      }
   }
   KM_Tag_DeleteContext(km_characs.out_sw_enforced);

   return kme;
}

static keymaster_error_t km_import_key(
   const struct keymaster2_device* dev,
   const keymaster_key_param_set_t* params,
   keymaster_key_format_t key_format,
   const keymaster_blob_t* key_data,
   keymaster_key_blob_t* key_blob,
   keymaster_key_characteristics_t* characteristics) {

   keymaster2_device_t* km_dev = (keymaster2_device_t *)dev;
   struct bcm_km *km_hdl =(struct bcm_km *)km_dev->context;
   if (km_hdl == NULL) {
      ALOGE("km_import_key: null bcm-km handle?!");
      return KM_ERROR_UNKNOWN_ERROR;
   }
   if (!params || !key_data) {
      ALOGE("km_import_key: null inputs?!");
      return KM_ERROR_UNEXPECTED_NULL_POINTER;
   }
   if (!key_blob) {
      ALOGE("km_import_key: null output?!");
      return KM_ERROR_OUTPUT_PARAMETER_NULL;
   }

   int km_init_err = km_init(km_hdl);
   // init failed on subsequent calls post km_config.
   if (km_init_err) return KM_ERROR_KEYMASTER_NOT_CONFIGURED;

   AuthorizationSet set;
   set.Reinitialize(*params);
   if (set.is_valid() != keymaster::AuthorizationSet::OK) {
      ALOGE("km_import_key: failed to build set for in params");
      return KM_ERROR_UNKNOWN_ERROR;
   }
   size_t serial_len = set.SerializedSize();
   UniquePtr<uint8_t[]> serial(new uint8_t[serial_len]);
   set.Serialize(serial.get(), serial.get() + serial_len);
   BERR_Code km_err;
   KM_Tag_ContextHandle km_params = NULL;
   int ix;
   km_err = KM_Tag_CreateContextFromAndroidBlob(serial.get(), serial.get() + serial_len, &km_params);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_import_key: failed create-context: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      return km_berr_2_kmerr(km_err);
   }
   // insert a "all applications" tag if none specified.
   if (!set.Contains(TAG_APPLICATION_ID) && !set.Contains(TAG_ALL_APPLICATIONS)) {
      KM_Tag_AddBool(km_params, SKM_TAG_ALL_APPLICATIONS, true);
   }
   // insert a creation time because there is no wall clock in sage.
   if (!set.Contains(TAG_CREATION_DATETIME)) {
      KM_Tag_AddDate(km_params, SKM_TAG_CREATION_DATETIME, java_time(time(NULL)));
   }
   // insert a key size tag if none specified.
   if (!set.Contains(TAG_KEY_SIZE)) {
      KM_Tag_AddInteger(km_params, SKM_TAG_KEY_SIZE, key_data->data_length);
   }
   KeymasterTl_ImportKeySettings km_import;
   KeymasterTl_GetDefaultImportKeySettings(&km_import);
   km_import.in_key_params = km_params;
   km_import.in_key_format = (km_key_format_t)key_format;
   km_import.in_key_blob.buffer = (uint8_t *)key_data->data;
   km_import.in_key_blob.size = key_data->data_length;
   km_err = KeymasterTl_ImportKey(km_hdl->handle, &km_import);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_import_key: failed importing, err: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      if (km_import.in_key_params) KM_Tag_DeleteContext(km_import.in_key_params);
      return km_berr_2_kmerr(km_err);
   }
   // copy key in the passed in blob, passing ownership.
   key_blob->key_material_size = km_import.out_key_blob.size;
   key_blob->key_material = km_dup_2_kmblob(km_import.out_key_blob.buffer, km_import.out_key_blob.size);
   if (!key_blob->key_material) {
      if (km_import.in_key_params) KM_Tag_DeleteContext(km_import.in_key_params);
      if (km_import.out_key_blob.buffer) {
         (void)KeymasterTl_DeleteKey(km_hdl->handle, &km_import.out_key_blob);
         SRAI_Memory_Free(km_import.out_key_blob.buffer);
      }
      ALOGE("km_import_key: failed copying out key");
      return KM_ERROR_MEMORY_ALLOCATION_FAILED;
   }

   // give characteristics of key if requested.
   if (characteristics) {
      KeymasterTl_GetKeyCharacteristicsSettings km_characs;
      KeymasterTl_GetDefaultKeyCharacteristicsSettings(&km_characs);
      km_characs.in_key_blob.buffer = km_import.out_key_blob.buffer;
      km_characs.in_key_blob.size = km_import.out_key_blob.size;
      km_characs.in_params = km_import.in_key_params;
      km_err = KeymasterTl_GetKeyCharacteristics(km_hdl->handle, &km_characs);
      if (km_err != BERR_SUCCESS) {
         ALOGE("km_import_key: failed getting key characteristics, err: %u (%d)",
            km_err, km_berr_2_kmerr(km_err));
         // error out? (if so, kill km_key.buffer && km_params).
      } else {
         uint32_t serial_size;
         uint8_t *serial;
         ix = KM_Tag_GetNumPairs(km_characs.out_hw_enforced);
         if (ix) {
            km_err = KM_Tag_SerializeToAndroidBlob(km_characs.out_hw_enforced, NULL, &serial_size);
            if (km_err == BERR_SUCCESS) {
               serial = reinterpret_cast<uint8_t*>(malloc(serial_size));
               if (serial != NULL) {
                  km_err = KM_Tag_SerializeToAndroidBlob(km_characs.out_hw_enforced, serial, &serial_size);
                  if (km_err == BERR_SUCCESS) {
                     AuthorizationSet set;
                     const uint8_t *s = serial;
                     if (!set.Deserialize(&s, serial+serial_size)) {
                        ALOGE("km_import_key: failed importing hw_enforced");
                        // continue as we may still work with the key.
                     } else {
                        set.CopyToParamSet(&characteristics->hw_enforced);
                     }
                  }
                  free(serial);
               }
            }
         }
         KM_Tag_DeleteContext(km_characs.out_hw_enforced);
         ix = KM_Tag_GetNumPairs(km_characs.out_sw_enforced);
         if (ix) {
            km_err = KM_Tag_SerializeToAndroidBlob(km_characs.out_sw_enforced, NULL, &serial_size);
            if (km_err == BERR_SUCCESS) {
               serial = reinterpret_cast<uint8_t*>(malloc(serial_size));
               if (serial != NULL) {
                  km_err = KM_Tag_SerializeToAndroidBlob(km_characs.out_sw_enforced, serial, &serial_size);
                  if (km_err == BERR_SUCCESS) {
                     AuthorizationSet set;
                     const uint8_t *s = serial;
                     if (!set.Deserialize(&s, serial+serial_size)) {
                        ALOGE("km_import_key: failed importing sw_enforced");
                        // continue as we may still work with the key.
                     } else {
                        set.CopyToParamSet(&characteristics->sw_enforced);
                     }
                  }
                  free(serial);
               }
            }
         }
         KM_Tag_DeleteContext(km_characs.out_sw_enforced);
      }
   }

   if (km_import.in_key_params) KM_Tag_DeleteContext(km_import.in_key_params);
   if (km_import.out_key_blob.buffer) SRAI_Memory_Free(km_import.out_key_blob.buffer);

   return KM_ERROR_OK;
}

static keymaster_error_t km_export_key(
   const struct keymaster2_device* dev,
   keymaster_key_format_t export_format,
   const keymaster_key_blob_t* key_to_export,
   const keymaster_blob_t* client_id,
   const keymaster_blob_t* app_data,
   keymaster_blob_t* export_data) {

   keymaster2_device_t* km_dev = (keymaster2_device_t *)dev;
   struct bcm_km *km_hdl =(struct bcm_km *)km_dev->context;
   if (km_hdl == NULL) {
      ALOGE("km_export_key: null bcm-km handle?!");
      return KM_ERROR_UNKNOWN_ERROR;
   }
   if (!key_to_export || !key_to_export->key_material) {
      ALOGE("km_export_key: null inputs?!");
      return KM_ERROR_UNEXPECTED_NULL_POINTER;
   }
   if (!export_data) {
      ALOGE("km_export_key: null output?!");
      return KM_ERROR_OUTPUT_PARAMETER_NULL;
   }

   int km_init_err = km_init(km_hdl);
   // init failed on subsequent calls post km_config.
   if (km_init_err) return KM_ERROR_KEYMASTER_NOT_CONFIGURED;

   BERR_Code km_err;
   KM_Tag_ContextHandle km_params = NULL;
   KeymasterTl_ExportKeySettings km_export;
   KeymasterTl_GetDefaultExportKeySettings(&km_export);
   km_export.in_key_format = (km_key_format_t) export_format;
   km_export.in_key_blob.buffer = (uint8_t *)key_to_export->key_material;
   km_export.in_key_blob.size = key_to_export->key_material_size;
   if ((client_id && client_id->data) || (app_data && app_data->data)) {
      km_err = KM_Tag_NewContext(&km_params);
      if (km_err != BERR_SUCCESS) {
         ALOGE("km_export_key: failed tag-context: %u (%d)",
            km_err, km_berr_2_kmerr(km_err));
         return km_berr_2_kmerr(km_err);
      }
      if (client_id && client_id->data) {
         km_err = KM_Tag_AddBlob(km_params, SKM_TAG_APPLICATION_ID, client_id->data_length, client_id->data);
         if (km_err != BERR_SUCCESS) {
            ALOGE("km_export_key: failed tag-blob: %u, err: %u (%d)", KM_TAG_APPLICATION_ID,
               km_err, km_berr_2_kmerr(km_err));
            KM_Tag_DeleteContext(km_params);
            return km_berr_2_kmerr(km_err);
         }
         ALOGI_IF(KM_LOG_ALL_IN, "km_export_key: added tag: %u, length: %u", KM_TAG_APPLICATION_ID, client_id->data_length);
      }
      if (app_data && app_data->data) {
         km_err = KM_Tag_AddBlob(km_params, SKM_TAG_APPLICATION_DATA, app_data->data_length, app_data->data);
         if (km_err != BERR_SUCCESS) {
            ALOGE("km_export_key: failed tag-blob: %u, err: %u (%d)", KM_TAG_APPLICATION_DATA,
               km_err, km_berr_2_kmerr(km_err));
            KM_Tag_DeleteContext(km_params);
            return km_berr_2_kmerr(km_err);
         }
         ALOGI_IF(KM_LOG_ALL_IN, "km_export_key: added tag: %u, length: %u", KM_TAG_APPLICATION_DATA, app_data->data_length);
      }
   }
   km_export.in_params = km_params;
   km_err = KeymasterTl_ExportKey(km_hdl->handle, &km_export);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_export_key: failed exporting, err: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      if (km_export.in_params) KM_Tag_DeleteContext(km_export.in_params);
      return km_berr_2_kmerr(km_err);
   }
   if (km_export.in_params) KM_Tag_DeleteContext(km_export.in_params);
   // copy key in the passed in blob, passing ownership.
   export_data->data_length = km_export.out_key_blob.size;
   export_data->data = km_dup_2_kmblob(km_export.out_key_blob.buffer, km_export.out_key_blob.size);
   if (!export_data->data) {
      if (km_export.out_key_blob.buffer) SRAI_Memory_Free(km_export.out_key_blob.buffer);
      ALOGE("km_export_key: failed copying exported key");
      return KM_ERROR_MEMORY_ALLOCATION_FAILED;
   }
   if (km_export.out_key_blob.buffer) SRAI_Memory_Free(km_export.out_key_blob.buffer);

   return KM_ERROR_OK;
}

static keymaster_error_t km_attest_key(
   const struct keymaster2_device* dev,
   const keymaster_key_blob_t* key_to_attest,
   const keymaster_key_param_set_t* attest_params,
   keymaster_cert_chain_t* cert_chain) {

   keymaster2_device_t* km_dev = (keymaster2_device_t *)dev;
   struct bcm_km *km_hdl =(struct bcm_km *)km_dev->context;
   if (km_hdl == NULL) {
      ALOGE("km_attest_key: null bcm-km handle?!");
      return KM_ERROR_UNKNOWN_ERROR;
   }
   if (!key_to_attest || !attest_params) {
      ALOGE("km_attest_key: null inputs?!");
      return KM_ERROR_UNEXPECTED_NULL_POINTER;
   }
   if (!cert_chain) {
      ALOGE("km_attest_key: null output?!");
      return KM_ERROR_OUTPUT_PARAMETER_NULL;
   }

   int km_init_err = km_init(km_hdl);
   // init failed on subsequent calls post km_config.
   if (km_init_err) return KM_ERROR_KEYMASTER_NOT_CONFIGURED;

   AuthorizationSet set;
   set.Reinitialize(*attest_params);
   if (set.is_valid() != keymaster::AuthorizationSet::OK) {
      ALOGE("km_attest_key: failed to build set for in params");
      return KM_ERROR_UNKNOWN_ERROR;
   }
   size_t serial_len = set.SerializedSize();
   UniquePtr<uint8_t[]> serial(new uint8_t[serial_len]);
   set.Serialize(serial.get(), serial.get() + serial_len);
   if (!set.Contains(TAG_ATTESTATION_APPLICATION_ID)) {
      ALOGE("km_attest_key: missing TAG_ATTESTATION_APPLICATION_ID");
      return KM_ERROR_ATTESTATION_APPLICATION_ID_MISSING;
   }

   BERR_Code km_err;
   KM_Tag_ContextHandle km_params = NULL;
   int i;
   km_err = KM_Tag_CreateContextFromAndroidBlob(serial.get(), serial.get() + serial_len, &km_params);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_attest_key: failed create-context: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      return km_berr_2_kmerr(km_err);
   }

   KeymasterTl_AttestKeySettings km_attest;
   KeymasterTl_GetDefaultAttestKeySettings(&km_attest);
   km_attest.in_params = km_params;
   km_attest.in_key_blob.buffer = (uint8_t *)key_to_attest->key_material;
   km_attest.in_key_blob.size = key_to_attest->key_material_size;
   km_err = KeymasterTl_AttestKey(km_hdl->handle, &km_attest);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_attest_key: failed attestation, err: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      if (km_attest.in_params) KM_Tag_DeleteContext(km_attest.in_params);
      return km_berr_2_kmerr(km_err);
   }
   if (km_attest.in_params) KM_Tag_DeleteContext(km_attest.in_params);
   // copy key in the passed in blob, passing ownership.
   cert_chain->entry_count = km_attest.out_cert_chain.num;
   cert_chain->entries = reinterpret_cast<keymaster_blob_t*>(malloc(sizeof(keymaster_blob_t)*cert_chain->entry_count));
   for (i = 0 ; i < (int)cert_chain->entry_count ; i++) {
      cert_chain->entries[i].data_length = km_attest.out_cert_chain.certificates[i].length;
      cert_chain->entries[i].data = km_dup_2_kmblob(
         &km_attest.out_cert_chain_buffer.buffer[km_attest.out_cert_chain.certificates[i].offset],
         km_attest.out_cert_chain.certificates[i].length);
      if (!cert_chain->entries[i].data) {
         break;
      }
   }
   if (i != (int)cert_chain->entry_count) {
      if (km_attest.out_cert_chain_buffer.buffer) SRAI_Memory_Free(km_attest.out_cert_chain_buffer.buffer);
      for (int j = 0 ; j < (int)i ; j++) {
         free((void *)cert_chain->entries[j].data);
      }
      free((void *)cert_chain->entries);
      ALOGE("km_attest_key: failed copying certificate chain");
      return KM_ERROR_MEMORY_ALLOCATION_FAILED;
   }
   if (km_attest.out_cert_chain_buffer.buffer) SRAI_Memory_Free(km_attest.out_cert_chain_buffer.buffer);

   return KM_ERROR_OK;
}

static keymaster_error_t km_upgrade_key(
   const struct keymaster2_device* dev,
   const keymaster_key_blob_t* key_to_upgrade,
   const keymaster_key_param_set_t* upgrade_params,
   keymaster_key_blob_t* upgraded_key) {

   keymaster2_device_t* km_dev = (keymaster2_device_t *)dev;
   struct bcm_km *km_hdl =(struct bcm_km *)km_dev->context;
   if (km_hdl == NULL) {
      ALOGE("km_upgrade_key: null bcm-km handle?!");
      return KM_ERROR_UNKNOWN_ERROR;
   }
   if (!key_to_upgrade || !upgrade_params) {
      ALOGE("km_upgrade_key: null inputs?!");
      return KM_ERROR_UNEXPECTED_NULL_POINTER;
   }
   if (!upgraded_key) {
      ALOGE("km_upgrade_key: null output?!");
      return KM_ERROR_OUTPUT_PARAMETER_NULL;
   }

   int km_init_err = km_init(km_hdl);
   // init failed on subsequent calls post km_config.
   if (km_init_err) return KM_ERROR_KEYMASTER_NOT_CONFIGURED;

   AuthorizationSet set;
   set.Reinitialize(*upgrade_params);
   if (set.is_valid() != keymaster::AuthorizationSet::OK) {
      ALOGE("km_upgrade_key: failed to build set for in params");
      return KM_ERROR_UNKNOWN_ERROR;
   }
   size_t serial_len = set.SerializedSize();
   UniquePtr<uint8_t[]> serial(new uint8_t[serial_len]);
   set.Serialize(serial.get(), serial.get() + serial_len);
   BERR_Code km_err;
   KM_Tag_ContextHandle km_params = NULL;
   km_err = KM_Tag_CreateContextFromAndroidBlob(serial.get(), serial.get() + serial_len, &km_params);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_upgrade_key: failed create-context: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      return km_berr_2_kmerr(km_err);
   }

   KeymasterTl_DataBlock km_in_key, km_out_key;
   km_in_key.size = key_to_upgrade->key_material_size;
   km_in_key.buffer = (uint8_t *)key_to_upgrade->key_material;
   km_err = KeymasterTl_UpgradeKey(km_hdl->handle, &km_in_key, km_params, &km_out_key);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_upgrade_key: failed upgrade, err: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      if (km_params) KM_Tag_DeleteContext(km_params);
      return km_berr_2_kmerr(km_err);
   }
   if (km_params) KM_Tag_DeleteContext(km_params);
   upgraded_key->key_material_size = km_out_key.size;
   upgraded_key->key_material = km_dup_2_kmblob(km_out_key.buffer, km_out_key.size);
   if (!upgraded_key->key_material) {
      if (km_out_key.buffer) SRAI_Memory_Free(km_out_key.buffer);
      ALOGE("km_upgrade_key: failed copying out key");
      return KM_ERROR_MEMORY_ALLOCATION_FAILED;
   }
   if (km_out_key.buffer) SRAI_Memory_Free(km_out_key.buffer);

   return KM_ERROR_OK;
}

static keymaster_error_t km_delete_key(
   const struct keymaster2_device* dev,
   const keymaster_key_blob_t* key) {

   keymaster2_device_t* km_dev = (keymaster2_device_t *)dev;
   struct bcm_km *km_hdl =(struct bcm_km *)km_dev->context;
   if (km_hdl == NULL) {
      ALOGE("km_delete_key: null bcm-km handle?!");
      return KM_ERROR_UNKNOWN_ERROR;
   }
   if (!key || !key->key_material) {
      ALOGE("km_delete_key: null key-blob in?!");
      return KM_ERROR_UNEXPECTED_NULL_POINTER;
   }

   int km_init_err = km_init(km_hdl);
   // init failed on subsequent calls post km_config.
   if (km_init_err) return KM_ERROR_KEYMASTER_NOT_CONFIGURED;

   BERR_Code km_err;
   KeymasterTl_DataBlock km_key;
   km_key.size = key->key_material_size;
   km_key.buffer = (uint8_t *)key->key_material;
   km_err = KeymasterTl_DeleteKey(km_hdl->handle, &km_key);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_delete_key: failed deleting key, err: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      return km_berr_2_kmerr(km_err);
   }

   return KM_ERROR_OK;
}

static keymaster_error_t km_delete_all_keys(
   const struct keymaster2_device* dev) {

   keymaster2_device_t* km_dev = (keymaster2_device_t *)dev;
   struct bcm_km *km_hdl =(struct bcm_km *)km_dev->context;
   if (km_hdl == NULL) {
      ALOGE("km_delete_key: null bcm-km handle?!");
      return KM_ERROR_UNKNOWN_ERROR;
   }

   int km_init_err = km_init(km_hdl);
   // init failed on subsequent calls post km_config.
   if (km_init_err) return KM_ERROR_KEYMASTER_NOT_CONFIGURED;

   BERR_Code km_err;
   km_err = KeymasterTl_DeleteAllKeys(km_hdl->handle);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_delete_all_keys: failed deleting *all* keys, err: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      return km_berr_2_kmerr(km_err);
   }

   return KM_ERROR_OK;
}

static keymaster_error_t km_begin(
   const struct keymaster2_device* dev,
   keymaster_purpose_t purpose,
   const keymaster_key_blob_t* key,
   const keymaster_key_param_set_t* in_params,
   keymaster_key_param_set_t* out_params,
   keymaster_operation_handle_t* operation_handle) {

   keymaster2_device_t* km_dev = (keymaster2_device_t *)dev;
   struct bcm_km *km_hdl =(struct bcm_km *)km_dev->context;
   if (km_hdl == NULL) {
      ALOGE("km_begin: null bcm-km handle?!");
      return KM_ERROR_UNKNOWN_ERROR;
   }
   if (!key || !key->key_material) {
      ALOGE("km_begin: null key-blob in?!");
      return KM_ERROR_UNEXPECTED_NULL_POINTER;
   }
   if (!operation_handle) {
      ALOGE("km_begin: null operation handle?!");
      return KM_ERROR_OUTPUT_PARAMETER_NULL;
   }

   int km_init_err = km_init(km_hdl);
   // init failed on subsequent calls post km_config.
   if (km_init_err) return KM_ERROR_KEYMASTER_NOT_CONFIGURED;

   AuthorizationSet set;
   set.Reinitialize(*in_params);
   if (set.is_valid() != keymaster::AuthorizationSet::OK) {
      ALOGE("km_begin: failed to build set for in params");
      return KM_ERROR_UNKNOWN_ERROR;
   }
   size_t serial_len = set.SerializedSize();
   UniquePtr<uint8_t[]> serial(new uint8_t[serial_len]);
   set.Serialize(serial.get(), serial.get() + serial_len);
   BERR_Code km_err;
   KM_Tag_ContextHandle km_params = NULL;
   km_err = KM_Tag_CreateContextFromAndroidBlob(serial.get(), serial.get() + serial_len, &km_params);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_begin: failed create-context: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      return km_berr_2_kmerr(km_err);
   }

   KeymasterTl_CryptoBeginSettings km_cbs;
   km_operation_handle_t km_out_hdl;
   KeymasterTl_GetDefaultCryptoBeginSettings(&km_cbs);
   km_cbs.purpose = (km_purpose_t)purpose;
   km_cbs.in_key_blob.size = key->key_material_size;
   km_cbs.in_key_blob.buffer = (uint8_t *)key->key_material;
   km_cbs.in_params = km_params;
   km_err = KeymasterTl_CryptoBegin(km_hdl->handle, &km_cbs, &km_out_hdl);
   if (km_err != BERR_SUCCESS) {
      if (km_cbs.in_params) KM_Tag_DeleteContext(km_cbs.in_params);
      ALOGE("km_begin: failed crypto operation: begin, err: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      return km_berr_2_kmerr(km_err);
   }
   if (km_cbs.in_params) KM_Tag_DeleteContext(km_cbs.in_params);
   if (km_cbs.out_params && KM_Tag_GetNumPairs(km_cbs.out_params)) {
      if (out_params) {
         uint32_t serial_size;
         uint8_t *serial;
         km_err = KM_Tag_SerializeToAndroidBlob(km_cbs.out_params, NULL, &serial_size);
         if (km_err == BERR_SUCCESS) {
            serial = reinterpret_cast<uint8_t*>(malloc(serial_size));
            if (serial != NULL) {
               km_err = KM_Tag_SerializeToAndroidBlob(km_cbs.out_params, serial, &serial_size);
               if (km_err == BERR_SUCCESS) {
                  AuthorizationSet set;
                  const uint8_t *s = serial;
                  if (!set.Deserialize(&s, serial+serial_size)) {
                     ALOGE("km_begin: failed importing out_params");
                     // continue as we may still work with the key.
                  } else {
                     set.CopyToParamSet(out_params);
                  }
               }
               free(serial);
            }
         }
      }
      KM_Tag_DeleteContext(km_cbs.out_params);
   }
   *operation_handle = (keymaster_operation_handle_t)km_out_hdl;

   return KM_ERROR_OK;
}

static keymaster_error_t km_update(
   const struct keymaster2_device* dev,
   keymaster_operation_handle_t operation_handle,
   const keymaster_key_param_set_t* in_params,
   const keymaster_blob_t* input,
   size_t* input_consumed,
   keymaster_key_param_set_t* out_params,
   keymaster_blob_t* output) {

   keymaster2_device_t* km_dev = (keymaster2_device_t *)dev;
   struct bcm_km *km_hdl =(struct bcm_km *)km_dev->context;
   if (km_hdl == NULL) {
      ALOGE("km_update: null bcm-km handle?!");
      return KM_ERROR_UNKNOWN_ERROR;
   }
   if (!input) {
      ALOGE("km_update: null input?!");
      return KM_ERROR_UNEXPECTED_NULL_POINTER;
   }
   if (!input_consumed) {
      ALOGE("km_update: null input consumed?!");
      return KM_ERROR_OUTPUT_PARAMETER_NULL;
   }

   int km_init_err = km_init(km_hdl);
   // init failed on subsequent calls post km_config.
   if (km_init_err) return KM_ERROR_KEYMASTER_NOT_CONFIGURED;

   AuthorizationSet set;
   set.Reinitialize(*in_params);
   if (set.is_valid() != keymaster::AuthorizationSet::OK) {
      ALOGE("km_update: failed to build set for in params");
      return KM_ERROR_UNKNOWN_ERROR;
   }
   size_t serial_len = set.SerializedSize();
   UniquePtr<uint8_t[]> serial(new uint8_t[serial_len]);
   set.Serialize(serial.get(), serial.get() + serial_len);
   BERR_Code km_err;
   KM_Tag_ContextHandle km_params = NULL;
   km_err = KM_Tag_CreateContextFromAndroidBlob(serial.get(), serial.get() + serial_len, &km_params);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_update: failed create-context: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      return km_berr_2_kmerr(km_err);
   }

   KeymasterTl_CryptoUpdateSettings km_cus;
   KeymasterTl_GetDefaultCryptoUpdateSettings(&km_cus);
   km_cus.in_params = km_params;
   km_cus.in_data.buffer = (uint8_t *)input->data;
   km_cus.in_data.size = input->data_length;
   km_err = KeymasterTl_CryptoUpdate(km_hdl->handle, operation_handle, &km_cus);
   if (km_err != BERR_SUCCESS) {
      if (km_cus.in_params) KM_Tag_DeleteContext(km_cus.in_params);
      ALOGE("km_finish: failed crypto operation: finish, err: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      return km_berr_2_kmerr(km_err);
   }
   if (km_cus.in_params) KM_Tag_DeleteContext(km_cus.in_params);
   *input_consumed = km_cus.out_input_consumed;
   if (km_cus.out_params && KM_Tag_GetNumPairs(km_cus.out_params)) {
      if (out_params) {
         uint32_t serial_size;
         uint8_t *serial;
         km_err = KM_Tag_SerializeToAndroidBlob(km_cus.out_params, NULL, &serial_size);
         if (km_err == BERR_SUCCESS) {
            serial = reinterpret_cast<uint8_t*>(malloc(serial_size));
            if (serial != NULL) {
               km_err = KM_Tag_SerializeToAndroidBlob(km_cus.out_params, serial, &serial_size);
               if (km_err == BERR_SUCCESS) {
                  AuthorizationSet set;
                  const uint8_t *s = serial;
                  if (!set.Deserialize(&s, serial+serial_size)) {
                     ALOGE("km_update: failed importing out_params");
                     // continue as we may still work with the key.
                  } else {
                     set.CopyToParamSet(out_params);
                  }
               }
               free(serial);
            }
         }
      }
      KM_Tag_DeleteContext(km_cus.out_params);
   }
   if (km_cus.out_data.buffer && km_cus.out_data.size) {
      if (output) {
         output->data_length = km_cus.out_data.size;
         output->data = km_dup_2_kmblob(km_cus.out_data.buffer, km_cus.out_data.size);
         if (!output->data) {
            if (km_cus.out_data.buffer) SRAI_Memory_Free(km_cus.out_data.buffer);
            ALOGE("km_finish: failed copying generated key");
            return KM_ERROR_MEMORY_ALLOCATION_FAILED;
         }
      }
      if (km_cus.out_data.buffer) SRAI_Memory_Free(km_cus.out_data.buffer);
   }

   return KM_ERROR_OK;
}

static keymaster_error_t km_finish(
   const struct keymaster2_device* dev,
   keymaster_operation_handle_t operation_handle,
   const keymaster_key_param_set_t* in_params,
   const keymaster_blob_t* input,
   const keymaster_blob_t* signature,
   keymaster_key_param_set_t* out_params,
   keymaster_blob_t* output) {

   keymaster2_device_t* km_dev = (keymaster2_device_t *)dev;
   struct bcm_km *km_hdl =(struct bcm_km *)km_dev->context;
   if (km_hdl == NULL) {
      ALOGE("km_finish: null bcm-km handle?!");
      return KM_ERROR_UNKNOWN_ERROR;
   }
   if (!input) {
      ALOGE("km_finish: null input?!");
      return KM_ERROR_UNEXPECTED_NULL_POINTER;
   }
   if (input->data_length > km_hdl->maxFinishInput) {
      ALOGE("km_finish: oversized input?!");
      return KM_ERROR_INVALID_INPUT_LENGTH;
   }

   int km_init_err = km_init(km_hdl);
   // init failed on subsequent calls post km_config.
   if (km_init_err) return KM_ERROR_KEYMASTER_NOT_CONFIGURED;

   AuthorizationSet set;
   set.Reinitialize(*in_params);
   if (set.is_valid() != keymaster::AuthorizationSet::OK) {
      ALOGE("km_finish: failed to build set for in params");
      return KM_ERROR_UNKNOWN_ERROR;
   }
   size_t serial_len = set.SerializedSize();
   UniquePtr<uint8_t[]> serial(new uint8_t[serial_len]);
   set.Serialize(serial.get(), serial.get() + serial_len);
   BERR_Code km_err;
   KM_Tag_ContextHandle km_params = NULL;
   km_err = KM_Tag_CreateContextFromAndroidBlob(serial.get(), serial.get() + serial_len, &km_params);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_finish: failed create-context: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      return km_berr_2_kmerr(km_err);
   }

   KeymasterTl_CryptoFinishSettings km_cfs;
   KeymasterTl_GetDefaultCryptoFinishSettings(&km_cfs);
   km_cfs.in_params = km_params;
   km_cfs.in_signature.buffer = (uint8_t *)signature->data;
   km_cfs.in_signature.size = signature->data_length;
   km_cfs.in_data.buffer = (uint8_t *)input->data;
   km_cfs.in_data.size = input->data_length;
   km_err = KeymasterTl_CryptoFinish(km_hdl->handle, operation_handle, &km_cfs);
   if (km_err != BERR_SUCCESS) {
      if (km_cfs.in_params) KM_Tag_DeleteContext(km_cfs.in_params);
      ALOGE("km_finish: failed crypto operation: finish, err: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      return km_berr_2_kmerr(km_err);
   }
   if (km_cfs.in_params) KM_Tag_DeleteContext(km_cfs.in_params);
   if (km_cfs.out_params && KM_Tag_GetNumPairs(km_cfs.out_params)) {
      if (out_params) {
         uint32_t serial_size;
         uint8_t *serial;
         km_err = KM_Tag_SerializeToAndroidBlob(km_cfs.out_params, NULL, &serial_size);
         if (km_err == BERR_SUCCESS) {
            serial = reinterpret_cast<uint8_t*>(malloc(serial_size));
            if (serial != NULL) {
               km_err = KM_Tag_SerializeToAndroidBlob(km_cfs.out_params, serial, &serial_size);
               if (km_err == BERR_SUCCESS) {
                  AuthorizationSet set;
                  const uint8_t *s = serial;
                  if (!set.Deserialize(&s, serial+serial_size)) {
                     ALOGE("km_finish: failed importing out_params");
                     // continue as we may still work with the key.
                  } else {
                     set.CopyToParamSet(out_params);
                  }
               }
               free(serial);
            }
         }
      }
      KM_Tag_DeleteContext(km_cfs.out_params);
   }
   if (km_cfs.out_data.buffer && km_cfs.out_data.size) {
      if (output) {
         output->data_length = km_cfs.out_data.size;
         output->data = km_dup_2_kmblob(km_cfs.out_data.buffer, km_cfs.out_data.size);
         if (!output->data) {
            if (km_cfs.out_data.buffer) SRAI_Memory_Free(km_cfs.out_data.buffer);
            ALOGE("km_finish: failed copying generated key");
            return KM_ERROR_MEMORY_ALLOCATION_FAILED;
         }
      }
      if (km_cfs.out_data.buffer) SRAI_Memory_Free(km_cfs.out_data.buffer);
   }

   return KM_ERROR_OK;
}

static keymaster_error_t km_abort(
   const struct keymaster2_device* dev,
   keymaster_operation_handle_t operation_handle) {

   keymaster2_device_t* km_dev = (keymaster2_device_t *)dev;
   struct bcm_km *km_hdl =(struct bcm_km *)km_dev->context;
   if (km_hdl == NULL) {
      ALOGE("km_abort: null bcm-km handle?!");
      return KM_ERROR_UNKNOWN_ERROR;
   }

   int km_init_err = km_init(km_hdl);
   // init failed on subsequent calls post km_config.
   if (km_init_err) return KM_ERROR_KEYMASTER_NOT_CONFIGURED;

   BERR_Code km_err;
   km_err = KeymasterTl_CryptoAbort(km_hdl->handle, operation_handle);
   if (km_err != BERR_SUCCESS) {
      ALOGE("km_abort: failed operation, err: %u (%d)",
         km_err, km_berr_2_kmerr(km_err));
      return km_berr_2_kmerr(km_err);
   }

   return KM_ERROR_OK;
}

static int km_open(
   const hw_module_t* module,
   const char* name,
   hw_device_t** device) {

   char nexus[PROPERTY_VALUE_MAX];
   int c = 0;
   static int c_max = 20;

   if (strcmp(name, KEYSTORE_KEYMASTER) != 0)
      return -EINVAL;

   struct bcm_km *km_hdl = (struct bcm_km *)malloc(sizeof(struct bcm_km));
   if (km_hdl == NULL) {
      ALOGE("failed to alloc bmc_km, aborting.");
      return -1;
   }

   Unique_keymaster_device_t dev(new keymaster2_device_t);
   if (dev.get() == NULL) {
      ALOGE("failed to alloc keymaster2 device, aborting.");
      free(km_hdl);
      return -ENOMEM;
   }

   km_hdl->maxFinishInput = 2048;

   // busy loop wait for nexus readiness, without valid client, the
   // keymaster cannot function.
   property_get("dyn.nx.state", nexus, "");
   while (1) {
      if (!strncmp(nexus, "loaded", strlen("loaded")))
         break;
      else {
         ALOGW("nexus not ready for keymaster, 0.25 second delay...");
         usleep(1000000/4);
         property_get("dyn.nx.state", nexus, "");
      }
   }
   void *nexus_client = nxwrap_create_verified_client(&km_hdl->nxwrap);
   if (nexus_client == NULL) {
      ALOGE("failed to alloc keymaster2 nexus client, aborting.");
      free(km_hdl);
      return -EINVAL;
   }
   // busy loop wait for ssd readiness (rpmb), without rpmb, the
   // keymaster may not fully function, but should work to some degree.
   property_get("dyn.nx.ssd.state", nexus, "");
   while (1) {
      if (!strncmp(nexus, "init", strlen("init"))) {
         // all is well.
         ALOGI("keymaster operating with ssd service (rpmb|vfs).");
         break;
      } else if (!strncmp(nexus, "ended", strlen("ended"))) {
         // ssd attempted, but crashed or is not properly setup, try without.
         ALOGW("keymaster operating without ssd service (rpmb), ssd failure.");
         break;
      } else {
         // something is wrong, ssd may not even be present.
         ALOGW("ssd not ready for keymaster, 0.25 second delay...");
         usleep(1000000/4);
         property_get("dyn.nx.ssd.state", nexus, "");
         c++;
         if (c > c_max) {
            ALOGE("keymaster timeout waiting for ssd service, proceed without.");
            break;
         }
      }
   }

   // now try to load up keymaster on tee side once.
   int km_init_err = km_init(km_hdl);

   dev->context        = (void *)km_hdl;
   dev->common.tag     = HARDWARE_DEVICE_TAG;
   dev->common.version = 1;
   dev->common.module  = (struct hw_module_t*) module;
   dev->common.close   = km_close;
   dev->flags          = 0; /* keymaster2 api's. */

   dev->configure               = km_config;
   dev->add_rng_entropy         = km_add_rng_entropy;

   dev->generate_key            = km_generate_key;
   dev->get_key_characteristics = km_get_key_characteristics;
   dev->import_key              = km_import_key;
   dev->export_key              = km_export_key;
   dev->attest_key              = km_attest_key;
   dev->upgrade_key             = km_upgrade_key;
   dev->delete_key              = km_delete_key;
   dev->delete_all_keys         = km_delete_all_keys;

   dev->begin                   = km_begin;
   dev->update                  = km_update;
   dev->finish                  = km_finish;
   dev->abort                   = km_abort;

   *device = reinterpret_cast<hw_device_t*>(dev.release());
   return 0;
}

static struct hw_module_methods_t keystore_module_methods = {
    .open = km_open,
};

struct keystore_module HAL_MODULE_INFO_SYM
__attribute__ ((visibility ("default"))) = {
   .common = {
      .tag                = HARDWARE_MODULE_TAG,
      .module_api_version = KEYMASTER_MODULE_API_VERSION_2_0,
      .hal_api_version    = HARDWARE_HAL_API_VERSION,
      .id                 = KEYSTORE_HARDWARE_MODULE_ID,
      .name               = "keymaster hal for bcm stb",
      .author             = "broadcom canada ltd.",
      .methods            = &keystore_module_methods,
      .dso                = 0,
      .reserved           = {},
   },
};
