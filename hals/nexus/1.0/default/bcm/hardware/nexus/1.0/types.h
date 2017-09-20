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

struct NexusPowerState final {
    bool enet_en __attribute__ ((aligned(1)));
    bool moca_en __attribute__ ((aligned(1)));
    bool sata_en __attribute__ ((aligned(1)));
    bool tp1_en __attribute__ ((aligned(1)));
    bool tp2_en __attribute__ ((aligned(1)));
    bool tp3_en __attribute__ ((aligned(1)));
    bool cpufreq_scale_en __attribute__ ((aligned(1)));
    bool ddr_pm_en __attribute__ ((aligned(1)));
};

static_assert(offsetof(NexusPowerState, enet_en) == 0, "wrong offset");
static_assert(offsetof(NexusPowerState, moca_en) == 1, "wrong offset");
static_assert(offsetof(NexusPowerState, sata_en) == 2, "wrong offset");
static_assert(offsetof(NexusPowerState, tp1_en) == 3, "wrong offset");
static_assert(offsetof(NexusPowerState, tp2_en) == 4, "wrong offset");
static_assert(offsetof(NexusPowerState, tp3_en) == 5, "wrong offset");
static_assert(offsetof(NexusPowerState, cpufreq_scale_en) == 6, "wrong offset");
static_assert(offsetof(NexusPowerState, ddr_pm_en) == 7, "wrong offset");
static_assert(sizeof(NexusPowerState) == 8, "wrong size");
static_assert(__alignof(NexusPowerState) == 1, "wrong alignment");

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

std::string toString(const NexusPowerState&);

bool operator==(const NexusPowerState&, const NexusPowerState&);

bool operator!=(const NexusPowerState&, const NexusPowerState&);


}  // namespace V1_0
}  // namespace nexus
}  // namespace hardware
}  // namespace bcm

#endif  // HIDL_GENERATED_BCM_HARDWARE_NEXUS_V1_0_TYPES_H
