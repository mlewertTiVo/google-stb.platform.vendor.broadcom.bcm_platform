#define LOG_TAG "bcm-hidl-tse"

#include "TviSvcExt.h"
#include <inttypes.h>
#include <android-base/logging.h>

namespace bcm {
namespace hardware {
namespace tvisvcext {
namespace V1_0 {
namespace implementation {

static void *nxwrap = NULL;
static void *nexus_client = NULL;

extern "C" void* nxwrap_create_client(void **wrap);
extern "C" void nxwrap_destroy_client(void *wrap);

Return<int32_t> TviSvcExt::tunerAcquire() {
   int32_t rc = 0;

   if (nxwrap != NULL)
      nxwrap_destroy_client(nxwrap);
   nexus_client = nxwrap_create_client(&nxwrap);
   if (nexus_client == NULL) {
      ALOGW("failed to associate nexus client, bailing...");
      return -ENOMEM;
   }

   return rc;
}

Return<int32_t> TviSvcExt::tunerRelease(int32_t t) {
   int32_t rc = 0;

   if (nxwrap != NULL) {
      nxwrap_destroy_client(nxwrap);
      nxwrap = NULL;
   }

   return rc;
}

Return<int32_t> TviSvcExt::tunerTune(int32_t t, uint32_t f) {
   int32_t rc = 0;

   (void)t;
   (void)f;

   return rc;
}

Return<int32_t> TviSvcExt::tunerStatus(int32_t t) {
   int32_t rc = 0;

   (void)t;

   return rc;
}

Return<TviSvcExtTunerType> TviSvcExt::tunerType(int32_t t) {
   TviSvcExtTunerType rc = TviSvcExtTunerType::UNKNOWN;

   (void)t;

   return rc;
}

Return<int32_t> TviSvcExt::tunerSigStrength(int32_t t) {
   int32_t rc = 0;

   (void)t;

   return rc;
}

Return<int32_t> TviSvcExt::tunerSigQuality(int32_t t) {
   int32_t rc = 0;

   (void)t;

   return rc;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace tvisvcext
}  // namespace hardware
}  // namespace bcm
