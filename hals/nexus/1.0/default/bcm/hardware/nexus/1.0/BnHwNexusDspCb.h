#ifndef HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_BNHWNEXUSDSPCB_H
#define HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_BNHWNEXUSDSPCB_H

#include <bcm/hardware/nexus/1.0/IHwNexusDspCb.h>

namespace bcm {
namespace hardware {
namespace nexus {
namespace V1_0 {

struct BnHwNexusDspCb : public ::android::hidl::base::V1_0::BnHwBase {
    explicit BnHwNexusDspCb(const ::android::sp<INexusDspCb> &_hidl_impl);
    explicit BnHwNexusDspCb(const ::android::sp<INexusDspCb> &_hidl_impl, const std::string& HidlInstrumentor_package, const std::string& HidlInstrumentor_interface);

    virtual ~BnHwNexusDspCb();

    ::android::status_t onTransact(
            uint32_t _hidl_code,
            const ::android::hardware::Parcel &_hidl_data,
            ::android::hardware::Parcel *_hidl_reply,
            uint32_t _hidl_flags = 0,
            TransactCallback _hidl_cb = nullptr) override;


    typedef INexusDspCb Pure;

    ::android::sp<INexusDspCb> getImpl() { return _hidl_mImpl; };
    // Methods from INexusDspCb follow.
    static ::android::status_t _hidl_onDsp(
            ::android::hidl::base::V1_0::BnHwBase* _hidl_this,
            const ::android::hardware::Parcel &_hidl_data,
            ::android::hardware::Parcel *_hidl_reply,
            TransactCallback _hidl_cb);



private:
    // Methods from INexusDspCb follow.

    // Methods from ::android::hidl::base::V1_0::IBase follow.
    ::android::hardware::Return<void> ping();
    using getDebugInfo_cb = ::android::hidl::base::V1_0::IBase::getDebugInfo_cb;
    ::android::hardware::Return<void> getDebugInfo(getDebugInfo_cb _hidl_cb);

    ::android::sp<INexusDspCb> _hidl_mImpl;
};

}  // namespace V1_0
}  // namespace nexus
}  // namespace hardware
}  // namespace bcm

#endif  // HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_BNHWNEXUSDSPCB_H
