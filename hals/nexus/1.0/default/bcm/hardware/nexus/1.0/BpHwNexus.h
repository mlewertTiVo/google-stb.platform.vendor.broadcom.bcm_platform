#ifndef HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_BPHWNEXUS_H
#define HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_BPHWNEXUS_H

#include <hidl/HidlTransportSupport.h>

#include <bcm/hardware/nexus/1.0/IHwNexus.h>

namespace bcm {
namespace hardware {
namespace nexus {
namespace V1_0 {

struct BpHwNexus : public ::android::hardware::BpInterface<INexus>, public ::android::hardware::details::HidlInstrumentor {
    explicit BpHwNexus(const ::android::sp<::android::hardware::IBinder> &_hidl_impl);

    typedef INexus Pure;

    virtual bool isRemote() const override { return true; }

    // Methods from INexus follow.
    static ::android::hardware::Return<uint64_t>  _hidl_client(::android::hardware::IInterface* _hidl_this, ::android::hardware::details::HidlInstrumentor *_hidl_this_instrumentor, int32_t pid);
    static ::android::hardware::Return<NexusStatus>  _hidl_registerHpdCb(::android::hardware::IInterface* _hidl_this, ::android::hardware::details::HidlInstrumentor *_hidl_this_instrumentor, uint64_t cId, const ::android::sp<INexusHpdCb>& cb);
    static ::android::hardware::Return<NexusStatus>  _hidl_registerDspCb(::android::hardware::IInterface* _hidl_this, ::android::hardware::details::HidlInstrumentor *_hidl_this_instrumentor, uint64_t cId, const ::android::sp<INexusDspCb>& cb);
    static ::android::hardware::Return<void>  _hidl_getPwr(::android::hardware::IInterface* _hidl_this, ::android::hardware::details::HidlInstrumentor *_hidl_this_instrumentor, getPwr_cb _hidl_cb);
    static ::android::hardware::Return<NexusStatus>  _hidl_setPwr(::android::hardware::IInterface* _hidl_this, ::android::hardware::details::HidlInstrumentor *_hidl_this_instrumentor, const NexusPowerState& p);

    // Methods from INexus follow.
    ::android::hardware::Return<uint64_t> client(int32_t pid) override;
    ::android::hardware::Return<NexusStatus> registerHpdCb(uint64_t cId, const ::android::sp<INexusHpdCb>& cb) override;
    ::android::hardware::Return<NexusStatus> registerDspCb(uint64_t cId, const ::android::sp<INexusDspCb>& cb) override;
    ::android::hardware::Return<void> getPwr(getPwr_cb _hidl_cb) override;
    ::android::hardware::Return<NexusStatus> setPwr(const NexusPowerState& p) override;

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

#endif  // HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_BPHWNEXUS_H
