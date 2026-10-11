#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace larp {
namespace nfc_capability {

using Nonce = std::array<std::uint8_t, 16>;
using SelectFrame = std::array<std::uint8_t, 14>;
using ChallengeFrame = std::array<std::uint8_t, 21>;

// Public correlation values only. The caller must obtain challenge from the
// device CSPRNG; this pure component cannot establish random provenance.
struct ReaderContext {
    std::uint64_t ownerTicket = 0;
    std::uint64_t readerSession = 0;
    std::uint64_t bleGeneration = 0;
    bool exclusiveReader = false;
    bool gameParked = false;
};

enum class Result : std::uint8_t {
    NotStarted, AwaitSelect, AwaitEcho, ProbeEchoOk,
    ReaderBusy, InvalidStart, Timeout, OwnerChanged, SessionChanged,
    BleGenerationChanged, Deactivated, MalformedSelect, MalformedEcho
};

SelectFrame selectFrame() noexcept;
ChallengeFrame challengeFrame(const Nonce &challenge) noexcept;
bool validSelectResponse(const std::uint8_t *bytes, std::size_t size,
                         Nonce &runId) noexcept;
bool validEchoResponse(const std::uint8_t *bytes, std::size_t size,
                       const Nonce &challenge) noexcept;

// One attempt per instance. A fresh instance is required after any terminal
// result. This component never assigns a role, grant, key, or GATT permission.
class Probe final {
  public:
    bool begin(const ReaderContext &context, const Nonce &challenge, std::uint64_t nowMs,
               std::uint32_t durationMs) noexcept;
    bool onSelectResponse(const ReaderContext &context, std::uint64_t nowMs,
                          const std::uint8_t *bytes, std::size_t size) noexcept;
    bool onEchoResponse(const ReaderContext &context, std::uint64_t nowMs,
                        const std::uint8_t *bytes, std::size_t size) noexcept;
    void deactivate() noexcept;
    void tick(const ReaderContext &context, std::uint64_t nowMs) noexcept;
    Result result() const noexcept { return result_; }
    bool probeEchoOk() const noexcept { return result_ == Result::ProbeEchoOk; }
    const Nonce &runId() const noexcept { return runId_; }
    const Nonce &challenge() const noexcept { return challenge_; }

  private:
    bool current(const ReaderContext &context, std::uint64_t nowMs) noexcept;
    Result result_ = Result::NotStarted;
    ReaderContext context_;
    Nonce runId_ = {};
    Nonce challenge_ = {};
    std::uint64_t startedMs_ = 0;
    std::uint32_t durationMs_ = 0;
};

} // namespace nfc_capability
} // namespace larp
