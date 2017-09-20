#ifndef HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_INEXUS_H
#define HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_INEXUS_H

#include <android/hidl/base/1.0/IBase.h>
#include <bcm/hardware/nexus/1.0/INexusDspCb.h>
#include <bcm/hardware/nexus/1.0/INexusHpdCb.h>
#include <bcm/hardware/nexus/1.0/types.h>

#include <android/hidl/manager/1.0/IServiceNotification.h>

#include <hidl/HidlSupport.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <utils/NativeHandle.h>
#include <utils/misc.h>

namespace bcm {
namespace hardware {
namespace nexus {
namespace V1_0 {

struct INexus : public ::android::hidl::base::V1_0::IBase {
    virtual bool isRemote() const override { return false; }


    virtual ::android::hardware::Return<uint64_t> client(int32_t pid) = 0;

    virtual ::android::hardware::Return<NexusStatus> registerHpdCb(uint64_t cId, const ::android::sp<INexusHpdCb>& cb) = 0;

    virtual ::android::hardware::Return<NexusStatus> registerDspCb(uint64_t cId, const ::android::sp<INexusDspCb>& cb) = 0;

    using getPwr_cb = std::function<void(const NexusPowerState& p)>;
    virtual ::android::hardware::Return<void> getPwr(getPwr_cb _hidl_cb) = 0;

    virtual ::android::hardware::Return<NexusStatus> setPwr(const NexusPowerState& p) = 0;

    using interfaceChain_cb = std::function<void(const ::android::hardware::hidl_vec<::android::hardware::hidl_string>& descriptors)>;
    virtual ::android::hardware::Return<void> interfaceChain(interfaceChain_cb _hidl_cb) override;

    virtual ::android::hardware::Return<void> debug(const ::android::hardware::hidl_handle& fd, const ::android::hardware::hidl_vec<::android::hardware::hidl_string>& options) override;

    using interfaceDescriptor_cb = std::function<void(const ::android::hardware::hidl_string& descriptor)>;
    virtual ::android::hardware::Return<void> interfaceDescriptor(interfaceDescriptor_cb _hidl_cb) override;

    using getHashChain_cb = std::function<void(const ::android::hardware::hidl_vec<::android::hardware::hidl_array<uint8_t, 32>>& hashchain)>;
    virtual ::android::hardware::Return<void> getHashChain(getHashChain_cb _hidl_cb) override;

    virtual ::android::hardware::Return<void> setHALInstrumentation() override;

    virtual ::android::hardware::Return<bool> linkToDeath(const ::android::sp<::android::hardware::hidl_death_recipient>& recipient, uint64_t cookie) override;

    virtual ::android::hardware::Return<void> ping() override;

    using getDebugInfo_cb = std::function<void(const ::android::hidl::base::V1_0::DebugInfo& info)>;
    virtual ::android::hardware::Return<void> getDebugInfo(getDebugInfo_cb _hidl_cb) override;

    virtual ::android::hardware::Return<void> notifySyspropsChanged() override;

    virtual ::android::hardware::Return<bool> unlinkToDeath(const ::android::sp<::android::hardware::hidl_death_recipient>& recipient) override;
    // cast static functions
    static ::android::hardware::Return<::android::sp<INexus>> castFrom(const ::android::sp<INexus>& parent, bool emitError = false);
    static ::android::hardware::Return<::android::sp<INexus>> castFrom(const ::android::sp<::android::hidl::base::V1_0::IBase>& parent, bool emitError = false);

    static const char* descriptor;

    static ::android::sp<INexus> tryGetService(const std::string &serviceName="default", bool getStub=false);
    static ::android::sp<INexus> tryGetService(const char serviceName[], bool getStub=false)  { std::string str(serviceName ? serviceName : "");      return tryGetService(str, getStub); }
    static ::android::sp<INexus> tryGetService(const ::android::hardware::hidl_string& serviceName, bool getStub=false)  { std::string str(serviceName.c_str());      return tryGetService(str, getStub); }
    static ::android::sp<INexus> tryGetService(bool getStub) { return tryGetService("default", getStub); }
    static ::android::sp<INexus> getService(const std::string &serviceName="default", bool getStub=false);
    static ::android::sp<INexus> getService(const char serviceName[], bool getStub=false)  { std::string str(serviceName ? serviceName : "");      return getService(str, getStub); }
    static ::android::sp<INexus> getService(const ::android::hardware::hidl_string& serviceName, bool getStub=false)  { std::string str(serviceName.c_str());      return getService(str, getStub); }
    static ::android::sp<INexus> getService(bool getStub) { return getService("default", getStub); }
    ::android::status_t registerAsService(const std::string &serviceName="default");
    static bool registerForNotifications(
            const std::string &serviceName,
            const ::android::sp<::android::hidl::manager::V1_0::IServiceNotification> &notification);
};

std::string toString(const ::android::sp<INexus>&);

}  // namespace V1_0
}  // namespace nexus
}  // namespace hardware
}  // namespace bcm

#endif  // HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_INEXUS_H
