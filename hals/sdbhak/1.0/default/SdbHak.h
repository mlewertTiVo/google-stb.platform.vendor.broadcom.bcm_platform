#ifndef BCM_HARDWARE_SDBHAK_V1_0_IMPL_H
#define BCM_HARDWARE_SDBHAK_V1_0_IMPL_H

#include <bcm/hardware/sdbhak/1.0/ISdbHak.h>
#include <utils/Mutex.h>

namespace bcm {
namespace hardware {
namespace sdbhak {
namespace V1_0 {
namespace implementation {

using ::bcm::hardware::sdbhak::V1_0::ISdbHak;
using ::android::hidl::base::V1_0::IBase;
using ::android::hardware::hidl_string;
using ::android::hardware::Return;
using ::android::sp;
using ::android::Mutex;

struct SdbHak : public ISdbHak {
   Return<int32_t> create(const hidl_string& p);
   Return<int32_t> destroy();
   Return<int32_t> start(int32_t x, int32_t y, int32_t w, int32_t h);
   Return<int32_t> stop();
   Return<int32_t> position(int32_t x, int32_t y, int32_t w, int32_t h);

   mutable Mutex l;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace sdbhak
}  // namespace hardware
}  // namespace bcm

#endif  // BCM_HARDWARE_SDBHAK_V1_0_IMPL_H
