#ifndef BCM_HARDWARE_TVISVCEXT_V1_0_IMPL_H
#define BCM_HARDWARE_TVISVCEXT_V1_0_IMPL_H

#include <bcm/hardware/tvisvcext/1.0/ITviSvcExt.h>
#include <utils/Mutex.h>

namespace bcm {
namespace hardware {
namespace tvisvcext {
namespace V1_0 {
namespace implementation {

using ::bcm::hardware::tvisvcext::V1_0::ITviSvcExt;
using ::bcm::hardware::tvisvcext::V1_0::TviSvcExtTunerType;
using ::android::hidl::base::V1_0::IBase;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;
using ::android::Mutex;

struct TviSvcExt : public ITviSvcExt {
   Return<int32_t> tunerAcquire();
   Return<int32_t> tunerRelease(int32_t t);
   Return<int32_t> tunerTune(int32_t t, uint32_t f);
   Return<int32_t> tunerStatus(int32_t t);
   Return<TviSvcExtTunerType> tunerType(int32_t t);
   Return<int32_t> tunerSigStrength(int32_t t);
   Return<int32_t> tunerSigQuality(int32_t t);

   mutable Mutex l;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace tvisvcext
}  // namespace hardware
}  // namespace bcm

#endif  // BCM_HARDWARE_TVISVCEXT_V1_0_IMPL_H
