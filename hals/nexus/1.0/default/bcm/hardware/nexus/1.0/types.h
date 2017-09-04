#ifndef HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_TYPES_H
#define HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_TYPES_H

#include <hidl/HidlSupport.h>
#include <hidl/MQDescriptor.h>
#include <utils/NativeHandle.h>
#include <utils/misc.h>

namespace bcm {
namespace hardware {
namespace nexus {
namespace V1_0 {

enum class NexusStatus : uint32_t {
    SUCCESS = 0u, // 0
    ALREADY_REGISTERED = 1u, // 1
    BAD_VALUE = 2u, // 2
    UNKNOWN = 3u, // 3
};

constexpr uint32_t operator|(const NexusStatus lhs, const NexusStatus rhs) {
    return static_cast<uint32_t>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr uint32_t operator|(const uint32_t lhs, const NexusStatus rhs) {
    return static_cast<uint32_t>(lhs | static_cast<uint32_t>(rhs));
}

constexpr uint32_t operator|(const NexusStatus lhs, const uint32_t rhs) {
    return static_cast<uint32_t>(static_cast<uint32_t>(lhs) | rhs);
}

constexpr uint32_t operator&(const NexusStatus lhs, const NexusStatus rhs) {
    return static_cast<uint32_t>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

constexpr uint32_t operator&(const uint32_t lhs, const NexusStatus rhs) {
    return static_cast<uint32_t>(lhs & static_cast<uint32_t>(rhs));
}

constexpr uint32_t operator&(const NexusStatus lhs, const uint32_t rhs) {
    return static_cast<uint32_t>(static_cast<uint32_t>(lhs) & rhs);
}

constexpr uint32_t &operator|=(uint32_t& v, const NexusStatus e) {
    v |= static_cast<uint32_t>(e);
    return v;
}

constexpr uint32_t &operator&=(uint32_t& v, const NexusStatus e) {
    v &= static_cast<uint32_t>(e);
    return v;
}

template<typename>
std::string toString(uint32_t o);
template<>
std::string toString<NexusStatus>(uint32_t o);

std::string toString(NexusStatus o);


}  // namespace V1_0
}  // namespace nexus
}  // namespace hardware
}  // namespace bcm

#endif  // HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_TYPES_H
