#ifndef BCM_HARDWARE_BP3_V1_0_IMPL_H
#define BCM_HARDWARE_BP3_V1_0_IMPL_H

#include <bcm/hardware/bp3/1.0/Ibp3.h>
#include <utils/Mutex.h>
#include <utils/Vector.h>

namespace bcm {
namespace hardware {
namespace bp3 {
namespace V1_0 {
namespace implementation {

using ::bcm::hardware::bp3::V1_0::Ibp3;
using ::android::hidl::base::V1_0::IBase;
using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;
using ::android::Mutex;
using ::android::Vector;

struct bp3 : public Ibp3 {
   Return<void> status(status_cb _hidl_cb);
   Return<int32_t> provision(const hidl_string& srvUrl,
                             const hidl_string& key,
                             const hidl_vec<int32_t>& lic);

   mutable Mutex l;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace bp3
}  // namespace hardware
}  // namespace bcm

#endif  // BCM_HARDWARE_BP3_V1_0_IMPL_H
