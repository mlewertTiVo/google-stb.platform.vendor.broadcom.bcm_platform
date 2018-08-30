#define LOG_TAG "bcm-hidl-bp3"

#include <inttypes.h>
#include <android-base/logging.h>
#include <utils/Vector.h>
#include "ibp3.h"

extern "C" {
#include "cJSON.h"
#include <curl/curl.h>
#include "sage_app_utils.h"
#include "bp3_session.h"
#include "bchp_scpu_globalram.h"
#include "bchp_common.h"
#include "bchp_sun_top_ctrl.h"
#include "bchp_jtag_otp.h"
#include "bchp_bsp_glb_control.h"
#include "priv/bsagelib_shared_globalsram.h"
#include "bp3_features.h"
#include "nexus_security_types.h"
#include "nexus_security.h"
#include "nexus_base_os.h"
#include "nexus_platform.h"

static void *nxwrap = NULL;
static void *nexus_client = NULL;

void* nxwrap_create_client(void **wrap);
void nxwrap_destroy_client(void *wrap);

extern char bp3_bin_file_name[];
extern char bp3_bin_file_path[];
extern bp3featuresStruct bp3_features[];
#define FEATURE_BYTES 32
static bitmapStruct bitmap[FEATURE_BYTES * 8];
static const char *ipOwners[] = {"Unused", "Broadcom", "Dolby", "Rovi", "Technicolor", "DTS"};
static const char *tzTaCustomers[] = {"", "Broadcom", "", "Novel-SuperTV"};
static uint32_t features[GlobalSram_IPLicensing_Info_size];

static struct {
    uint8_t OwnerId;
    eSramMap SramMap[6];
} owners[] = {
    {1, {Video0, Video1, Host, Audio, Sage, TrustZone} },
    {2, {Audio, Host} },
    {3, {Host} },
    {4, {Info, Host} },
    {5, {Audio} }
};

typedef struct _curl_memory_t {
   uint8_t *memory;
   curl_off_t size;
   curl_off_t position;
} curl_mem_t;

static bool isOn(uint8_t owner, uint8_t bit)
{
   bitmapStruct i = bitmap[bit];
   return ((features[(int)owners[owner-1].SramMap[i.key]] & i.value) == 0);
}

static int read_features(uint32_t *features) {
   int i, rc = 0;
   for(i=0; i< GlobalSram_IPLicensing_Info_size; i++) {
      rc = NEXUS_Platform_ReadRegister(BSAGElib_GlobalSram_GetRegister(BSAGElib_GlobalSram_eIPLicensing) + i*4, features+i);
      if (rc) return rc;
   }
   return rc;
}

static void reset(CURL *curl, struct curl_httppost **post, struct curl_httppost **last) {
   curl_formfree(*post);
   *post = NULL;
   *last = NULL;
   curl_easy_reset(curl);
   curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
   curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
   curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
}

static size_t write_mem_callback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  curl_mem_t *mem = (curl_mem_t *) userp;

  mem->memory = (uint8_t *)realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

static int read_part_number(uint32_t *prodId, uint32_t *securityCode) {
   int rc = NEXUS_Platform_ReadRegister(BCHP_SUN_TOP_CTRL_PRODUCT_ID, prodId);
   if (rc) return rc;
   return NEXUS_Platform_ReadRegister(BCHP_JTAG_OTP_GENERAL_STATUS_8, securityCode);
}
}

namespace bcm {
namespace hardware {
namespace bp3 {
namespace V1_0 {
namespace implementation {

static long response_code = 0;
static char *output = NULL;
#define STR_CONCAT(fmt, ...) do { \
    int _c, _l = 0; \
    _c = printf(fmt, ##__VA_ARGS__); \
    if (output == NULL) output = (char*) malloc((_c + 1) * sizeof(char)); \
    else { \
      _l = strlen(output); \
      output = (char *)realloc(output, (_l + _c + 1) * sizeof(char)); \
    } \
    sprintf(output + _l, fmt, ##__VA_ARGS__); \
  } while (0)

#define SET_API_KEY \
    if (headers) { \
      curl_slist_free_all(headers); \
      headers = NULL; \
    } \
    snprintf(buf, sizeof(buf), "Authorization: bearer %s", apitoken); \
    headers = curl_slist_append(headers, buf); \
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

#define ADD_PART(name, fmt, ...) \
    sprintf(buf, fmt, __VA_ARGS__); \
    curl_formadd(&post, &last, \
      CURLFORM_COPYNAME, name, \
      CURLFORM_COPYCONTENTS, buf, CURLFORM_END);

Return<void> bp3::status(
   status_cb _hidl_cb) {

   int32_t rc = 0;
   hidl_string str = "";
   uint32_t feats[GlobalSram_IPLicensing_Info_size];
   uint32_t otpIdHi = 0, otpIdLo = 0;
   uint8_t *session = NULL;
   uint32_t sessionSize = 0;
#ifdef BP3_TA_FEATURE_READ_SUPPORT
   uint32_t prodId = 0;
   uint32_t securityCode = 0;
   uint8_t  featureList[20];
   uint32_t featureListSize = 20;
   uint32_t bondOption = 0xFFFFFFFF;
   bool     provisioned = false;
#endif

   rc = SAGE_app_join_nexus();
   if (rc) goto out;

   rc = read_features(feats);
   if (rc) goto out_leave;

   rc = bp3_session_start(&session, &sessionSize);
   if (rc) goto out_leave;
#if BP3_TA_FEATURE_READ_SUPPORT
   rc = bp3_get_otp_id(&otpIdHi, &otpIdLo);
   if (rc) goto out_leave;
#endif
   if (otpIdHi == 0 && otpIdLo == 0) {
      NEXUS_Platform_ReadRegister(BCHP_BSP_GLB_CONTROL_v_PubOtpUniqueID_hi, &otpIdHi);
      NEXUS_Platform_ReadRegister(BCHP_BSP_GLB_CONTROL_v_PubOtpUniqueID_lo, &otpIdLo);
   }
   STR_CONCAT("UId = 0x%08x%08x\n\n", otpIdHi, otpIdLo);

   for (int i = 0; i < FEATURE_BYTES * 8; i++) {
      bitmap[i].key = i / 32;
      bitmap[i].value = 1 << (i % 32);
   }
   for (int i = 0; i < GlobalSram_IPLicensing_Info_size; i++) {
     features[i] = feats[i];
   }
   features[(uint32_t)Host] = (feats[(uint32_t)Sage] << 16) | (feats[(uint32_t)Host] & 0x0000FFFF);
   features[(uint32_t)TrustZone] = (feats[(uint32_t)TrustZone] & 0xFFFF0000) | (feats[(uint32_t)Host] >> 16);
   for (int i = 0; bp3_features[i].Name != NULL; i++) {
      if (bp3_features[i].Bit == TZTA_CUSTOMER_ID && isOn(bp3_features[i].OwnerId, bp3_features[i].Bit - 4)) {
         uint8_t v = 0;
         for (int j = 0; j < 8; j++)
            v |= isOn(bp3_features[i].OwnerId, bp3_features[i].Bit + j) ? 0 : (1 << j);
         if (v < sizeof(tzTaCustomers))
            STR_CONCAT("%s - %s %s\n", ipOwners[bp3_features[i].OwnerId], bp3_features[i].Name, tzTaCustomers[v]);
         else
            STR_CONCAT("%s - %s %d\n", ipOwners[bp3_features[i].OwnerId], bp3_features[i].Name, v);
         continue;
      }
      STR_CONCAT("%s - %s [%s]\n", ipOwners[bp3_features[i].OwnerId], bp3_features[i].Name,
             isOn(bp3_features[i].OwnerId, bp3_features[i].Bit) ? "Enabled" : "Disabled");
   }

#ifdef BP3_TA_FEATURE_READ_SUPPORT
   rc = bp3_get_chip_info (
      (uint8_t *)featureList,
      featureListSize,
      &prodId,
      &securityCode,
      &bondOption,
      &provisioned);
   if (rc) goto out_leave;

   STR_CONCAT("prodId: 0x%08x, securityCode: 0x%08x\n",prodId, securityCode);
   STR_CONCAT("bondOption: 0x%08x\n",bondOption);
   STR_CONCAT("provisioned = %s\n",provisioned==true ? "true" : "false");
#endif

out_leave:
   if (session) bp3_session_end(NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL);
   SAGE_app_leave_nexus();
out:
   if (!rc) str.setToExternal(output, strlen(output));
   _hidl_cb(str);
   free(output);
   output = NULL;
   return Void();
}

Return<int32_t> bp3::provision(
   const hidl_string& srvUrl,
   const hidl_string& key,
   const hidl_vec<int32_t>& lic) {

   int32_t rc = 0;
#ifdef BP3_TA_FEATURE_READ_SUPPORT
   uint32_t bp3SageStatus;
   uint8_t  featureList[20];
   uint32_t featureListSize = 20;
   uint32_t bondOption;
   bool     provisioned;
#endif
   char buf[2048];
   char apiVersion[] = "v1";
   uint32_t otpIdHi = 0, otpIdLo = 0;
   uint32_t prodId, securityCode;
   uint8_t *session = NULL;
   uint32_t sessionSize = 0;
   curl_mem_t ccf;
   struct curl_slist *headers = NULL;
   struct curl_httppost* post = NULL;
   struct curl_httppost* last = NULL;
   char *apikey = NULL, *apitoken = NULL;
   int long_index = 0;
   int apiVer = 1;
   uint8_t *logbuf = NULL;
   uint32_t logsize = 0;
   uint32_t* status = NULL;
   uint32_t statusSize = 0;
   int errCode;

   if (srvUrl.c_str() == NULL || key.c_str() == NULL)
      return -EINVAL;

   if (nxwrap != NULL)
      nxwrap_destroy_client(nxwrap);
   nexus_client = nxwrap_create_client(&nxwrap);
   if (nexus_client == NULL) {
      ALOGW("failed to associate nexus client, bailing...");
      return -ENOMEM;
   }

   curl_global_init(CURL_GLOBAL_ALL);
   CURL *curl = curl_easy_init();
   if (curl == NULL) return -CURLE_FAILED_INIT;

   reset(curl, &post, &last);

   curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "/tmp/bp3_cookie.txt");

   for (; apiVer >= 0; apiVer--) {
      if (apiVer == 1)
         snprintf(buf, sizeof(buf), "%s/api/%s/Token/Key", srvUrl.c_str(), apiVersion);
      else
         snprintf(buf, sizeof(buf), "%s/api/account/login", srvUrl.c_str());
      curl_easy_setopt(curl, CURLOPT_URL, buf);
      char *k = curl_easy_escape(curl, key.c_str(), strlen(key.c_str()));
      snprintf(buf, sizeof(buf), "key=%s", k);
      curl_free(k);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buf);
      curl_mem_t apiTokenKey;
      memset(&apiTokenKey, 0, sizeof(curl_mem_t));
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_mem_callback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &apiTokenKey);
      rc = curl_easy_perform(curl);
      if (apiVer == 1) {
         curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
         if (rc || (response_code == 400 || response_code == 401 || response_code == 404)) {
            ALOGE("Authentication on server %s failed: %lu (%s)",
               srvUrl.c_str(), response_code, curl_easy_strerror((CURLcode)rc));
            goto out;
         }
         cJSON *root = cJSON_Parse((const char *)apiTokenKey.memory);
         char *token = cJSON_GetObjectItem(root, "token")->valuestring;
         char *apiKey = cJSON_GetObjectItem(root, "apiKey")->valuestring;
         apikey = strdup(apiKey);
         apitoken = strdup(token);
         cJSON_Delete(root);
      }
      reset(curl, &post, &last);
      if (apiVer == 0) break;
      if (apikey == NULL) goto out;

      if (apitoken == NULL) {
         snprintf(buf, sizeof(buf), "%s/api/%s/Token", srvUrl.c_str(), apiVersion);
         curl_easy_setopt(curl, CURLOPT_URL, buf);
         snprintf(buf, sizeof(buf), "apiKey=%s", apikey);
         curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buf);

         curl_mem_t apiTokenKey;
         memset(&apiTokenKey, 0, sizeof(curl_mem_t));
         curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_mem_callback);
         curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &apiTokenKey);

         rc = curl_easy_perform(curl);
         curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
         if (rc || (response_code == 400 || response_code == 401 || response_code == 404)) {
            ALOGE("Authentication token on server %s failed: %lu (%s)",
               srvUrl.c_str(), response_code, curl_easy_strerror((CURLcode)rc));
            goto out;
         }
         cJSON *root = cJSON_Parse((const char *)apiTokenKey.memory);
         char *token = cJSON_GetObjectItem(root, "token")->valuestring;
         apitoken = strdup(token);
         cJSON_Delete(root);
         reset(curl, &post, &last);
      }
      if (apiVer == 1) break;
   }

   rc = read_part_number(&prodId, &securityCode);
   rc = bp3_session_start(&session, &sessionSize);
   rc = bp3_get_otp_id(&otpIdHi, &otpIdLo);
   if (otpIdHi == 0 && otpIdLo == 0) {
      NEXUS_Platform_ReadRegister(BCHP_BSP_GLB_CONTROL_v_PubOtpUniqueID_hi, &otpIdHi);
      NEXUS_Platform_ReadRegister(BCHP_BSP_GLB_CONTROL_v_PubOtpUniqueID_lo, &otpIdLo);
   }
#ifdef BP3_TA_FEATURE_READ_SUPPORT
   rc = bp3_get_chip_info (
      (uint8_t *)featureList,
      featureListSize,
      &prodId,
      &securityCode,
      &bondOption,
      &provisioned);
#endif

   if (apiVer == 0)
      snprintf(buf, sizeof(buf), "%s/api/provision/ccf", srvUrl.c_str());
   else {
      SET_API_KEY
      snprintf(buf, sizeof(buf), "%s/api/%s/provision/ccf", srvUrl.c_str(), apiVersion);
   }
   curl_easy_setopt(curl, CURLOPT_URL, buf);
   ADD_PART("otpId", "0x%x%08x", otpIdHi, otpIdLo)
#ifdef BP3_TA_FEATURE_READ_SUPPORT
   ADD_PART("otpSelect", "%u", BP3_OTPKeyTypeA)
#else
   ADD_PART("otpSelect", "%u", 0)
#endif
   ADD_PART("prodId", "0x%x", prodId)
   ADD_PART("secCode", "0x%x", securityCode & 0x03FFC000)
   {
      char token[32];
      for (int i=0; i<(int)lic.size(); i++) {
         sprintf(token, "%d", lic[i]);
         ADD_PART("lic", "%s", token);
      }
   }

   if (apiVer == 0) {
      curl_formadd(&post, &last,
         CURLFORM_COPYNAME, "token",
         CURLFORM_BUFFER, "token.bin",
         CURLFORM_BUFFERPTR, session,
         CURLFORM_BUFFERLENGTH, (long) sessionSize,
         CURLFORM_END);
      curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
   } else {
      for (int i=0; i<(int)sessionSize; i++) {
         ADD_PART("token", "%d", session[i])
      }
   }
   curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);

   memset(&ccf, 0, sizeof(curl_mem_t));
   curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_mem_callback);
   curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &ccf);
   rc = curl_easy_perform(curl);
   curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
   if (rc || (response_code == 400 || response_code == 401 || response_code == 404)) {
      ALOGE("Create CCF failed (srv: %s): %lu (%s)", srvUrl.c_str(), response_code, curl_easy_strerror((CURLcode)rc));
      goto out;
   }
   reset(curl, &post, &last);
   errCode = bp3_session_end(ccf.memory, ccf.size,
                             &logbuf, &logsize,
                             &status, &statusSize,
                             NULL, NULL);
   session = NULL;
   if (ccf.memory) free(ccf.memory);
   if (errCode != 0)
      ALOGW("Provision failed with error %d", errCode);

   if (apiVer == 0)
      snprintf(buf, sizeof(buf), "%s/api/provision/result", srvUrl.c_str());
   else {
      SET_API_KEY
      snprintf(buf, sizeof(buf), "%s/api/%s/provision/uploadlog", srvUrl.c_str(), apiVersion);
   }
   curl_easy_setopt(curl, CURLOPT_URL, buf);
   for (uint32_t i = 0; i < statusSize; i++) {
      ADD_PART("status", "%u", status[i])
   }
   free(status);
   ADD_PART("errCode", "%d", errCode)

   if (logsize > 0) {
      curl_formadd(&post, &last,
         CURLFORM_COPYNAME, apiVer == 0 ? "log" : "logFile",
         CURLFORM_BUFFER, "log.bin",
         CURLFORM_BUFFERPTR, logbuf,
         CURLFORM_BUFFERLENGTH, (long) logsize,
         CURLFORM_END);
   }

   snprintf(buf, sizeof(buf), "%s/%s", bp3_bin_file_path, bp3_bin_file_name);
   curl_formadd(&post, &last,
      CURLFORM_COPYNAME, apiVer == 0 ? "bp3" : "bp3File",
      CURLFORM_FILE, buf,
      CURLFORM_END);

   curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
   rc = curl_easy_perform(curl);
   curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
   if (rc || (response_code == 400 || response_code == 401 || response_code == 404)) {
      ALOGE("Upload log failed: %lu (%s)", response_code, curl_easy_strerror((CURLcode)rc));
      goto out;
   }

out:
   if ((response_code == 400) || (response_code == 401) || (response_code == 404))
      rc = -1;
   if (logbuf) free(logbuf);
   if (session)
      bp3_session_end(NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL);
   if (post) curl_formfree(post);
   curl_slist_free_all(headers);
   if (curl) curl_easy_cleanup(curl);
   curl_global_cleanup();
   if (nxwrap != NULL)
      nxwrap_destroy_client(nxwrap);
   nxwrap = NULL;

   return rc;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace bp3
}  // namespace hardware
}  // namespace bcm
