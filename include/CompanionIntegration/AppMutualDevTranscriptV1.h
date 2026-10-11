#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace larp {
namespace app_mutual_dev {

using Id16 = std::array<std::uint8_t, 16>;
using PublicP256 = std::array<std::uint8_t, 65>;
using Address6 = std::array<std::uint8_t, 6>;

// Fixture format only. It does not choose an APP_MUTUAL production suite,
// validate elliptic-curve points, sign a transcript or install any key.
struct Transcript {
    Address6 chip = {};
    Id16 deviceId = {};
    std::uint64_t bootId = 0;
    std::uint32_t sourceRegistration = 0;
    std::uint32_t sdkGeneration = 0;
    std::uint8_t gattInterface = 0xff;
    Address6 peerAddress = {};
    std::uint32_t connectionToken = 0;
    std::uint16_t connectionHandle = 0;
    Id16 phoneInstallId = {};
    Id16 boxKeyId = {};
    PublicP256 boxPublic = {};
    Id16 phoneKeyId = {};
    PublicP256 phonePublic = {};
    Id16 boxNonce = {};
    Id16 phoneNonce = {};
    std::uint32_t attempt = 0;
    std::uint32_t boxEnrollmentGeneration = 0;
    std::uint32_t phoneEnrollmentGeneration = 0;
    std::uint32_t boxRevocationGeneration = 0;
    std::uint32_t phoneRevocationGeneration = 0;
};

static const std::size_t FrameSize = 289;
using Frame = std::array<std::uint8_t, FrameSize>;

bool encode(const Transcript &source, Frame &out) noexcept;
bool decode(const std::uint8_t *bytes, std::size_t size, Transcript &out) noexcept;

enum class State : std::uint8_t {
    Empty, AwaitFrame, TranscriptCheckedClosed, Closed
};

// One-shot syntax/comparison fake. TranscriptCheckedClosed means that a supplied
// frame matched trusted fixture bytes. It is NOT a cryptographic proof and
// this type has no grant, role, GATT, key-store or authority output.
class ClosedFake final {
  public:
    bool begin(const Transcript &trustedLocal) noexcept;
    bool check(const std::uint8_t *received, std::size_t size) noexcept;
    void close() noexcept { if (state_ == State::AwaitFrame) state_ = State::Closed; }
    State state() const noexcept { return state_; }

  private:
    State state_ = State::Empty;
    Frame expected_ = {};
};

} // namespace app_mutual_dev
} // namespace larp
