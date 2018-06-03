#ifndef BCM_HARDWARE_DPTHAK_V1_0_IMPL_H
#define BCM_HARDWARE_DPTHAK_V1_0_IMPL_H

#include <bcm/hardware/dpthak/1.0/IDptHak.h>
#include <utils/Mutex.h>

namespace bcm {
namespace hardware {
namespace dpthak {
namespace V1_0 {
namespace implementation {

using ::bcm::hardware::dpthak::V1_0::IDptHak;
using ::bcm::hardware::dpthak::V1_0::DptHakHfrMode;
using ::android::hidl::base::V1_0::IBase;
using ::android::hardware::hidl_string;
using ::android::hardware::Return;
using ::android::sp;
using ::android::Mutex;

struct DptHak : public IDptHak {
   Return<int32_t> netcoal(const hidl_string& p);
   Return<int32_t> hfrvideo(const DptHakHfrMode m);

   mutable Mutex l;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace dpthak
}  // namespace hardware
}  // namespace bcm

#endif  // BCM_HARDWARE_DPTHAK_V1_0_IMPL_H
