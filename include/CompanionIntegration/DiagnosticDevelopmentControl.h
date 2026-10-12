#pragma once

#include "CompanionIntegration/DiagnosticDevelopmentAdmission.h"
#include "CompanionIntegration/DiagnosticTransport.h"

#if defined(LARP_DIAGNOSTIC_TEST_FIRMWARE)
namespace larp {
namespace diagnostic {

// Test-image-only use of the existing 69-byte DG request shape. A trusted
// original BLE actor supplies the link and fresh device-generated grant UUID.
// This processor never grants a game role or executes a component step.
class DevelopmentControl {
  public:
    DevelopmentControl(DevelopmentAdmission &admission,
                       const companion::Identity &device, std::uint64_t boot) noexcept
        : admission_(admission), device_(device), boot_(boot) {}
    bool process(const std::uint8_t *bytes, std::size_t count,
                 const DevelopmentLink &link, const Uuid &deviceGrant,
                 std::uint64_t now, std::uint8_t *reply,
                 std::size_t capacity) noexcept;
    void disconnect(const companion::PeerTransportBinding &link) noexcept {
        admission_.disconnect(link);
    }

  private:
    DevelopmentAdmission &admission_;
    companion::Identity device_;
    std::uint64_t boot_;
};

} // namespace diagnostic
} // namespace larp
#endif
