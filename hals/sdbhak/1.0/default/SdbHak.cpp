#define LOG_TAG "bcm-hidl-sh"

#include "SdbHak.h"

#include <bcmsidebandplayerfactory.h>
#include <inttypes.h>
#include <android-base/logging.h>
#include <log/log.h>

namespace bcm {
namespace hardware {
namespace sdbhak {
namespace V1_0 {
namespace implementation {

static BcmSidebandPlayer *sdbpl = NULL;
static void *nxwrap = NULL;
static void *nexus_client = NULL;

extern "C" void* nxwrap_create_client(void **wrap);
extern "C" void nxwrap_destroy_client(void *wrap);

Return<int32_t> SdbHak::create(const hidl_string& p) {
   int32_t rc = 0;
   if (sdbpl)
      return -ENOMEM;
   if (nxwrap != NULL)
      nxwrap_destroy_client(nxwrap);
   nexus_client = nxwrap_create_client(&nxwrap);
   if (nexus_client == NULL) {
      ALOGW("failed to associate nexus client, bailing...");
      return -ENOMEM;
   }
   sdbpl = BcmSidebandPlayerFactory::createFilePlayer(p.c_str());
   ALOGI("create player %p for \'%s\'", sdbpl, p.c_str());
   if (sdbpl == NULL)
      rc = -ENOMEM;
   return rc;
}

Return<int32_t> SdbHak::destroy() {
   int32_t rc = 0;
   if (sdbpl) {
      BcmSidebandPlayerFactory::destroy(sdbpl);
      sdbpl = NULL;
   } else
      rc = -EINVAL;

   if (nxwrap != NULL) {
      nxwrap_destroy_client(nxwrap);
      nxwrap = NULL;
   }
   ALOGI("destroy player %p: %d", sdbpl, rc);
   return rc;
}

Return<int32_t> SdbHak::start(int32_t x, int32_t y, int32_t w, int32_t h) {
   int32_t rc = 0;
   if (sdbpl)
      rc = sdbpl->start(x, y, w, h);
   else
      rc = -EINVAL;
   ALOGI("start player %p @ %d,%d,%dx%d: %d", sdbpl, x, y, w, h, rc);
   return rc;
}

Return<int32_t> SdbHak::stop() {
   int32_t rc = 0;
   if (sdbpl)
      sdbpl->stop();
   else
      rc = -EINVAL;
   ALOGI("stop player %p: %d", sdbpl, rc);
   return rc;
}

Return<int32_t> SdbHak::position(int32_t x, int32_t y, int32_t w, int32_t h) {
   int32_t rc = 0;
   if (sdbpl)
      sdbpl->setWindowPosition(x, y, w, h);
   else
      rc = -EINVAL;
   ALOGI("position player %p @ %d,%d,%dx%d: %d", sdbpl, x, y, w, h, rc);
   return rc;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace sdbhak
}  // namespace hardware
}  // namespace bcm
