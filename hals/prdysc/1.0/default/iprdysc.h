#ifndef BCM_HARDWARE_PRDYSC_V1_0_IMPL_H
#define BCM_HARDWARE_PRDYSC_V1_0_IMPL_H

#include <bcm/hardware/prdysc/1.0/Iprdysc.h>
#include <utils/Mutex.h>

namespace bcm {
namespace hardware {
namespace prdysc {
namespace V1_0 {
namespace implementation {

using ::bcm::hardware::prdysc::V1_0::Iprdysc;
using ::android::hidl::base::V1_0::IBase;
using ::android::hidl::memory::V1_0::IMemory;
using ::android::hardware::hidl_memory;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;
using ::android::Mutex;

struct prdysc : public Iprdysc {
   Return<void> challenge(const hidl_memory& in,
                          const hidl_memory& out,
                          challenge_cb _hidl_cb);

   mutable Mutex l;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace prdysc
}  // namespace hardware
}  // namespace bcm

#endif  // BCM_HARDWARE_PRDYSC_V1_0_IMPL_H
