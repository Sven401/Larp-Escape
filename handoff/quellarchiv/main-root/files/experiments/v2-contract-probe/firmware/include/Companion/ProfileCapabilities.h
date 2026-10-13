#pragma once
#include "Companion/ConfiguredProtocol.h"
namespace larp {
namespace companion {
// Describes the compiled profile language, not attached/working devices,
// physical transport availability, device identity or an authorization grant.
constexpr std::size_t maxProfileCapabilitiesJsonBytes = 3072;
ConfiguredEncodeResult encodeProfileCapabilities(char *buffer, std::size_t capacity,
                                                 std::size_t &written);
} // namespace companion
} // namespace larp
