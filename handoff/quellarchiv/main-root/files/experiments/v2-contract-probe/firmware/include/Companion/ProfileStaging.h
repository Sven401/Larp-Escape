#pragma once
#include "Companion/ConfiguredProtocol.h"
#include "Companion/ProfileFrames.h"
namespace larp {
namespace companion {
class IProfileStagingStore : public configuration::IProfileReader {
  public:
    // Exclusive bounded temporary source, never the active profile journal.
    // Each operation must be bounded. Append confirms the exact copied bytes.
    virtual bool begin(std::uint32_t bytes) noexcept = 0;
    virtual bool append(const char *bytes, std::size_t count) noexcept = 0;
    // Exact complete bytes, immutable read-only source, rewind to0 or Failure.
    virtual bool seal() noexcept = 0;
    // Close/discard only this temporary transfer; idempotent if already empty.
    virtual bool discard() noexcept = 0;
};
enum class ProfileReceivePhase : std::uint8_t { Empty, Receiving, Complete, Failed };
enum class ProfileReceiveOutcome : std::uint8_t {
    Receiving,
    Complete,
    Discarded,
    Busy,
    InvalidEnvelope,
    WrongRoute,
    Unauthorized,
    Expired,
    NotReceiving,
    InvalidFrame,
    Incomplete,
    DigestMismatch,
    StorageFailure
};
// Sole synchronous owner, never a transport callback. Data calls are <=128bytes;
// a future callback queue must retain original route/timestamp. No heap/Profile.
class ProfileStaging : public configuration::IProfileReader {
  public:
    ProfileStaging(IProfileStagingStore &store, const IGameControlChannel &authority,
                   IProfileImportClock &clock) noexcept;
    ProfileStaging(const ProfileStaging &) = delete;
    ProfileStaging &operator=(const ProfileStaging &) = delete;
    ProfileReceiveOutcome start(const ProfileSourceEnvelope &envelope,
                                const TrustedAuthority &original,
                                std::uint32_t receivedAt) noexcept;
    ProfileReceiveOutcome data(const char *bytes, std::size_t count,
                               const TrustedAuthority &original) noexcept;
    // Trusted owner operation, NOT a wire command or implicit empty-queue EOF.
    ProfileReceiveOutcome finish(const TrustedAuthority &original, std::uint16_t transfer) noexcept;
    ProfileReceiveOutcome service() noexcept;
    ProfileReceiveOutcome discard() noexcept;
    configuration::ReadStatus readByte(std::uint8_t &byte) override;
    ProfileReceivePhase phase() const noexcept {
        return phase_;
    }
    ProfileReceiveOutcome outcome() const noexcept {
        return outcome_;
    }
    std::uint32_t received() const noexcept {
        return received_;
    }
    std::uint32_t receivedAt() const noexcept {
        return receivedAt_;
    }
    const ProfileSourceEnvelope &envelope() const noexcept {
        return envelope_;
    }
    const TrustedAuthority &original() const noexcept {
        return original_;
    }

  private:
    bool current() noexcept;
    ProfileReceiveOutcome fail(ProfileReceiveOutcome outcome) noexcept;
    IProfileStagingStore &store_;
    const IGameControlChannel &authority_;
    IProfileImportClock &clock_;
    ProfileSourceEnvelope envelope_;
    TrustedAuthority original_;
    std::uint32_t receivedAt_ = 0, received_ = 0, crc_ = 0xffffffffu, read_ = 0,
                  readCrc_ = 0xffffffffu;
    ProfileReceivePhase phase_ = ProfileReceivePhase::Empty;
    ProfileReceiveOutcome outcome_ = ProfileReceiveOutcome::Discarded;
    bool busy_ = false, ended_ = false;
};
} // namespace companion
} // namespace larp
