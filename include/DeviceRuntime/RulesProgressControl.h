#pragma once
#include "DeviceRuntime/RulesProgressEvidence.h"
namespace larp {
namespace devices {
namespace rules_progress {
// Canonical fixed map labels0..11. No owner authority/clock assertion follows
// from these structural bytes; ISignedControlEvidence checks actual originals.
enum class ControlError : std::uint8_t { None, Argument, Alias, Capacity, Canonical, Value };
struct ControlResult {
    bool valid = false;
    ControlError error = ControlError::None;
    std::size_t offset = 0;
};
constexpr std::size_t MaximumControlEvidence = 2048;
bool controlDomain(const std::uint8_t *, std::size_t);
ControlResult encodeControl(const Control &, rules::Pool<std::uint8_t> &);
ControlResult decodeControl(const std::uint8_t *, std::size_t, Control &,
                            rules::Pool<std::uint8_t> &ownedEvidence);
} // namespace rules_progress
} // namespace devices
} // namespace larp
