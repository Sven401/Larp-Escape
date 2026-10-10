#pragma once
#include <cstddef>
#include <cstdint>

namespace larp {
namespace companion {
namespace sdk446 {
// Fixed prefix of the pinned SDK BT_HDR. Payload belongs to the original BTU
// callback and is never retained. Independent source ABI test checks all fields.
struct HciHeader {
    std::uint16_t event, length, offset, layerSpecific;
};
static_assert(sizeof(HciHeader) == 8 && alignof(HciHeader) == 2 &&
                  offsetof(HciHeader, length) == 2 && offsetof(HciHeader, offset) == 4 &&
                  offsetof(HciHeader, layerSpecific) == 6,
              "IDF 4.4.6 HCI header prefix");
} // namespace sdk446
} // namespace companion
} // namespace larp

extern "C" void __real_btu_hcif_process_event(std::uint8_t, larp::companion::sdk446::HciHeader *);
extern "C" void __wrap_btu_hcif_process_event(std::uint8_t, larp::companion::sdk446::HciHeader *);
