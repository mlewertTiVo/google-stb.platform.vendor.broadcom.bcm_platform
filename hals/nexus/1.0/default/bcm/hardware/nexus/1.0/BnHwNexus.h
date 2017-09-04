#ifndef HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_BNHWNEXUS_H
#define HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_BNHWNEXUS_H

#include <bcm/hardware/nexus/1.0/IHwNexus.h>

namespace bcm {
namespace hardware {
namespace nexus {
namespace V1_0 {

struct BnHwNexus : public ::android::hidl::base::V1_0::BnHwBase {
    explicit BnHwNexus(const ::android::sp<INexus> &_hidl_impl);
    explicit BnHwNexus(const ::android::sp<INexus> &_hidl_impl, const std::string& HidlInstrumentor_package, const std::string& HidlInstrumentor_interface);

    ::android::status_t onTransact(
            uint32_t _hidl_code,
            const ::android::hardware::Parcel &_hidl_data,
            ::android::hardware::Parcel *_hidl_reply,
            uint32_t _hidl_flags = 0,
            TransactCallback _hidl_cb = nullptr) override;

    ::android::sp<INexus> getImpl() { return _hidl_mImpl; };
private:
    // Methods from INexus follow.

    // Methods from ::android::hidl::base::V1_0::IBase follow.
    ::android::hardware::Return<void> ping();
    using getDebugInfo_cb = ::android::hidl::base::V1_0::IBase::getDebugInfo_cb;
    ::android::hardware::Return<void> getDebugInfo(getDebugInfo_cb _hidl_cb);

    ::android::sp<INexus> _hidl_mImpl;
};

}  // namespace V1_0
}  // namespace nexus
}  // namespace hardware
}  // namespace bcm

#endif  // HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_BNHWNEXUS_H
