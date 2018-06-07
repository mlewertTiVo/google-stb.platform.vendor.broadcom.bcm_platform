#define LOG_TAG "bcm-hidl-dh"

#include "DptHak.h"

#include <inttypes.h>
#include <android-base/logging.h>
#include <log/log.h>

extern "C" int do_ethcoal(const char* mode);
extern "C" int do_hfrvideo(int mode);

namespace bcm {
namespace hardware {
namespace dpthak {
namespace V1_0 {
namespace implementation {

Return<int32_t> DptHak::netcoal(const hidl_string& p) {
   int32_t rc = 0;
   rc = do_ethcoal(p.c_str());
   ALOGI("do_ethcoal(%s) returns %d", p.c_str(), rc);
   return rc;
}

Return<int32_t> DptHak::hfrvideo(const DptHakHfrMode m) {
   int32_t rc = 0;
   rc = do_hfrvideo((m == DptHakHfrMode::INVALID)?(int)-1:(int)m);
   ALOGI("do_hfrvideo(%d) returns %d", m, rc);
   return rc;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace dpthak
}  // namespace hardware
}  // namespace bcm
