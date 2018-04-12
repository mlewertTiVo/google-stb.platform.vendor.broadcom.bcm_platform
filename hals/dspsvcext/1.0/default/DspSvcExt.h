#ifndef BCM_HARDWARE_DSPSVCEXT_V1_0_IMPL_H
#define BCM_HARDWARE_DSPSVCEXT_V1_0_IMPL_H

#include <bcm/hardware/dspsvcext/1.0/IDspSvcExt.h>
#include <bcm/hardware/dspsvcext/1.0/IDspSvcExtSdbGeomCb.h>
#include <utils/Mutex.h>
#include <utils/Vector.h>

namespace bcm {
namespace hardware {
namespace dspsvcext {
namespace V1_0 {
namespace implementation {

using ::bcm::hardware::dspsvcext::V1_0::IDspSvcExt;
using ::bcm::hardware::dspsvcext::V1_0::DspSvcExtStatus;
using ::bcm::hardware::dspsvcext::V1_0::DspSvcExtOvs;
using ::bcm::hardware::dspsvcext::V1_0::IDspSvcExtSdbGeomCb;
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

struct SdbGeomCb {
   int32_t i;
   int32_t v;
   sp<IDspSvcExtSdbGeomCb> cb;
};

struct DspSvcExt : public IDspSvcExt {
   Return<DspSvcExtStatus> start();
   Return<DspSvcExtStatus> setOvs(const DspSvcExtOvs& o);
   Return<void> getOvs(getOvs_cb _hidl_cb);
   Return<int32_t> regSdbCb(int32_t i, const ::android::sp<IDspSvcExtSdbGeomCb>& cb);

   mutable Mutex l;
   Vector<struct SdbGeomCb> s;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace dspsvcext
}  // namespace hardware
}  // namespace bcm

#endif  // BCM_HARDWARE_DSPSVCEXT_V1_0_IMPL_H
