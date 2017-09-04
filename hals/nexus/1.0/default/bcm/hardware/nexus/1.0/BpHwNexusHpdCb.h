#ifndef HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_BPHWNEXUSHPDCB_H
#define HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_BPHWNEXUSHPDCB_H

#include <hidl/HidlTransportSupport.h>

#include <bcm/hardware/nexus/1.0/IHwNexusHpdCb.h>

namespace bcm {
namespace hardware {
namespace nexus {
namespace V1_0 {

struct BpHwNexusHpdCb : public ::android::hardware::BpInterface<INexusHpdCb>, public ::android::hardware::details::HidlInstrumentor {
    explicit BpHwNexusHpdCb(const ::android::sp<::android::hardware::IBinder> &_hidl_impl);

    virtual bool isRemote() const override { return true; }

    // Methods from INexusHpdCb follow.
    ::android::hardware::Return<void> onHpd(bool connected) override;

    // Methods from ::android::hidl::base::V1_0::IBase follow.
    ::android::hardware::Return<void> interfaceChain(interfaceChain_cb _hidl_cb) override;
    ::android::hardware::Return<void> debug(const ::android::hardware::hidl_handle& fd, const ::android::hardware::hidl_vec<::android::hardware::hidl_string>& options) override;
    ::android::hardware::Return<void> interfaceDescriptor(interfaceDescriptor_cb _hidl_cb) override;
    ::android::hardware::Return<void> getHashChain(getHashChain_cb _hidl_cb) override;
    ::android::hardware::Return<void> setHALInstrumentation() override;
    ::android::hardware::Return<bool> linkToDeath(const ::android::sp<::android::hardware::hidl_death_recipient>& recipient, uint64_t cookie) override;
    ::android::hardware::Return<void> ping() override;
    ::android::hardware::Return<void> getDebugInfo(getDebugInfo_cb _hidl_cb) override;
    ::android::hardware::Return<void> notifySyspropsChanged() override;
    ::android::hardware::Return<bool> unlinkToDeath(const ::android::sp<::android::hardware::hidl_death_recipient>& recipient) override;

private:
    std::mutex _hidl_mMutex;
    std::vector<::android::sp<::android::hardware::hidl_binder_death_recipient>> _hidl_mDeathRecipients;
};

}  // namespace V1_0
}  // namespace nexus
}  // namespace hardware
}  // namespace bcm

#endif  // HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_BPHWNEXUSHPDCB_H
