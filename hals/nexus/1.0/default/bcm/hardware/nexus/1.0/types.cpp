#define LOG_TAG "bcm.hardware.nexus@1.0::types"

#include <android/log.h>
#include <cutils/trace.h>
#include <hidl/HidlTransportSupport.h>

#include <bcm/hardware/nexus/1.0/types.h>
#include <bcm/hardware/nexus/1.0/hwtypes.h>

namespace bcm {
namespace hardware {
namespace nexus {
namespace V1_0 {

template<>
std::string toString<NexusStatus>(uint32_t o) {
    using ::android::hardware::details::toHexString;
    std::string os;
    uint32_t flipped = 0;
    bool first = true;
    if ((o & NexusStatus::SUCCESS) == static_cast<uint32_t>(NexusStatus::SUCCESS)) {
        os += (first ? "" : " | ");
        os += "SUCCESS";
        first = false;
        flipped |= NexusStatus::SUCCESS;
    }
    if ((o & NexusStatus::ALREADY_REGISTERED) == static_cast<uint32_t>(NexusStatus::ALREADY_REGISTERED)) {
        os += (first ? "" : " | ");
        os += "ALREADY_REGISTERED";
        first = false;
        flipped |= NexusStatus::ALREADY_REGISTERED;
    }
    if ((o & NexusStatus::BAD_VALUE) == static_cast<uint32_t>(NexusStatus::BAD_VALUE)) {
        os += (first ? "" : " | ");
        os += "BAD_VALUE";
        first = false;
        flipped |= NexusStatus::BAD_VALUE;
    }
    if ((o & NexusStatus::UNKNOWN) == static_cast<uint32_t>(NexusStatus::UNKNOWN)) {
        os += (first ? "" : " | ");
        os += "UNKNOWN";
        first = false;
        flipped |= NexusStatus::UNKNOWN;
    }
    if (o != flipped) {
        os += (first ? "" : " | ");
        os += toHexString(o & (~flipped));
    }os += " (";
    os += toHexString(o);
    os += ")";
    return os;
}

std::string toString(NexusStatus o) {
    using ::android::hardware::details::toHexString;
    if (o == NexusStatus::SUCCESS) {
        return "SUCCESS";
    }
    if (o == NexusStatus::ALREADY_REGISTERED) {
        return "ALREADY_REGISTERED";
    }
    if (o == NexusStatus::BAD_VALUE) {
        return "BAD_VALUE";
    }
    if (o == NexusStatus::UNKNOWN) {
        return "UNKNOWN";
    }
    std::string os;
    os += toHexString(static_cast<uint32_t>(o));
    return os;
}

static_assert(sizeof(::android::hardware::MQDescriptor<char, ::android::hardware::kSynchronizedReadWrite>) == 32, "wrong size");
static_assert(sizeof(::android::hardware::hidl_handle) == 16, "wrong size");
static_assert(sizeof(::android::hardware::hidl_memory) == 40, "wrong size");
static_assert(sizeof(::android::hardware::hidl_string) == 16, "wrong size");
static_assert(sizeof(::android::hardware::hidl_vec<char>) == 16, "wrong size");

}  // namespace V1_0
}  // namespace nexus
}  // namespace hardware
}  // namespace bcm
