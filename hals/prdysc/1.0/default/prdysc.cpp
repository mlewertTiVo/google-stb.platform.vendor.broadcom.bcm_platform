#define LOG_TAG "bcm-hidl-prdysc"

#include <inttypes.h>
#include <utils/Log.h>
#include "iprdysc.h"
#include "nxwrap.h"
#include <hidlmemory/mapping.h>
#include <cutils/properties.h>

#define DEFAULT_STSURL_OVERRIDE             1
#define STSURL_DEFAULT_STR                  "http://go.microsoft.com/fwlink/?LinkID=746259"
#define PR_SECURE_TIME_SERVER_URL_PROP      "ro.nx.pr.stsurl"

extern "C" {
#include "nexus_platform.h"
#include "nexus_memory.h"
#include "prdy_http.h"
#include "drmsecuretimeconstants.h"
}

#define MAX_TIME_CHALLENGE_RESPONSE_LENGTH (1024*64)
#define MAX_URL_LENGTH (512)

namespace bcm {
namespace hardware {
namespace prdysc {
namespace V1_0 {
namespace implementation {

static NxWrap *nxwrap = NULL;

Return<void> prdysc::challenge(
   const hidl_memory& in,
   const hidl_memory& out,
   challenge_cb _hidl_cb) {

   NEXUS_MemoryAllocationSettings a;
   char *tcURL = NULL;
   char *stsURL = NULL;
   char stURL[MAX_URL_LENGTH];
   int32_t petRC = 0;
   uint32_t petRespCode = 0;
   uint32_t post_ret, offset, len = 0;
   bool r = true;
   int32_t rc = 0;
   void *inData, *outData;
   char value[PROPERTY_VALUE_MAX];

   sp<IMemory> inMem = mapMemory(in);
   sp<IMemory> outMem = mapMemory(out);

   if (inMem == NULL) {
      rc = ENOMEM;
      ALOGE("failed to map in memory");
      goto out;
   }

   if (outMem == NULL) {
      rc = ENOMEM;
      ALOGE("failed to map out memory");
      goto out;
   }

   inData = inMem->getPointer();
   outData = outMem->getPointer();

   if (nxwrap == NULL) {
      nxwrap = new NxWrap("hal-prdysc");
      if (nxwrap == NULL) {
         rc = EINVAL;
         goto out;
      }
      nxwrap->join();
   }

   NEXUS_Memory_GetDefaultAllocationSettings(&a);
   rc = NEXUS_Memory_Allocate(MAX_URL_LENGTH, &a, (void **)(&tcURL));
   if (rc != NEXUS_SUCCESS) {
      ALOGE("NEXUS_Memory_Allocate failed for time challenge response buffer, rc = %d\n", rc);
      goto out;
   }

   // Standard STS URL is available from the PR3x library. Vendor can configure custom STS URL.
#if DEFAULT_STSURL_OVERRIDE
   // TODO: There is a known issue with the PR3x library URL macro which is missing the last character and also
   //       the terminating null. We need to work around this by using a local override with the same URL.
   if (property_get((const char*)PR_SECURE_TIME_SERVER_URL_PROP, value, STSURL_DEFAULT_STR)) {
#else
   if (property_get((const char*)PR_SECURE_TIME_SERVER_URL_PROP, value, "")) {
#endif
      // Use STS URL configured via vendor property
      stsURL = value;
   } else {
      // Use STS URL provided in PR3x library
      stsURL = (char*)g_dstrHttpSecureTimeServerUrl.pszString;
   }

   petRC = PRDY_HTTP_Client_GetForwardLinkUrl(stsURL,
                                              &petRespCode,
                                              (char**)&tcURL);
   if (petRC != 0) {
      ALOGE("Secure Time forward link petition request failed, rc = %d\n", petRC);
      rc = petRC;
      goto out;
   }

   do {
      r = false;
      if( petRespCode == 200) {
         r = false;
      } else if (petRespCode == 302 || petRespCode == 301) {
         r = true;
         memset(stURL, 0, MAX_URL_LENGTH);
         strcpy(stURL, tcURL);
         memset(tcURL, 0, MAX_URL_LENGTH);
         petRC = PRDY_HTTP_Client_GetSecureTimeUrl(stURL,
                                                   &petRespCode,
                                                   (char**)&tcURL);
         if (petRC != 0) {
            ALOGE("Secure Time forward link petition request failed, rc = %d\n", petRC);
            rc = petRC;
            goto out;
         }
      } else {
         ALOGE("Secure Clock Petition responded with unsupported result, rc = %d, can't get the time challenge URL\n", petRespCode);
         rc = EINVAL;
         goto out;
      }
   } while (r);


   NEXUS_Memory_GetDefaultAllocationSettings(&a);
   outMem->update();
   BKNI_Memset(outData, 0, MAX_TIME_CHALLENGE_RESPONSE_LENGTH);
   post_ret = PRDY_HTTP_Client_SecureTimeChallengePost(tcURL,
                                                       (char *)inData,
                                                       1,
                                                       150,
                                                       (unsigned char**)&(outData),
                                                       &offset,
                                                       &len);
   outMem->commit();
   if (post_ret != 0) {
      ALOGE("Secure Time Challenge request failed, rc = %d\n", post_ret);
      rc = post_ret;
      goto out;
   }

out:
   if (tcURL != NULL)
      NEXUS_Memory_Free(tcURL);

   _hidl_cb(rc, len);
   return Void();
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace prdysc
}  // namespace hardware
}  // namespace bcm
