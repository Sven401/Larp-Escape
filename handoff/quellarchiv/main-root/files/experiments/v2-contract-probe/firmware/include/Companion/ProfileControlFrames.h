#pragma once
#include "Companion/ConfiguredProtocol.h"
namespace larp {
namespace companion {
struct ProfileControlLimits {
    static const std::size_t CommandBytes = 12, ResponseBytes = 20;
    static const std::uint8_t Version = 1, CommandKind = 7, ResponseKind = 8;
    // Separate local policy, NOT command framing or a measured RF allowance.
    static const std::uint32_t QueueMilliseconds = 10000, ResponseMilliseconds = 10000;
};
enum class ProfileOperation : std::uint8_t { Finish = 1, Abort = 2, Apply = 3 };
enum class ProfileResponseCode : std::uint8_t {
    DraftReady = 1,
    ApplyQueued = 2,
    Saved = 3,
    Aborted = 4,
    ReceiveRejected = 5,
    ImportRejected = 6,
    ApplyRejected = 7,
    Unauthorized = 8,
    RequestConflict = 9,
    RequestExpired = 10,
    OutOfOrder = 11,
    Expired = 12,
    ApplyFailed = 13
};
enum class ProfileResponseDetail : std::uint16_t {
    None = 0,
    Busy = 1,
    WrongTransfer = 2,
    Incomplete = 3,
    DigestMismatch = 4,
    StorageFailure = 5,
    InvalidSource = 6,
    NotReady = 7,
    GameRunning = 8,
    StaleRevision = 9,
    NoReceipt = 10,
    Failed = 11
};
struct ProfileControlFrame {
    std::uint16_t transfer = 0;
    ProfileOperation operation = ProfileOperation::Finish;
    std::uint32_t request = 0;
    bool valid() const noexcept;
    bool operator==(const ProfileControlFrame &other) const noexcept {
        return transfer == other.transfer && operation == other.operation &&
               request == other.request;
    }
};
struct ProfileResponseFrame {
    ProfileControlFrame command;
    ProfileResponseCode code = ProfileResponseCode::ReceiveRejected;
    ProfileResponseDetail detail = ProfileResponseDetail::Failed;
    std::uint32_t draftRevision = 0, configurationRevision = 0;
    bool valid() const noexcept;
};
struct RoutedProfileResponse {
    TrustedAuthority original;
    std::uint32_t producedAt = 0;
    ProfileResponseFrame response;
};
bool decodeProfileControl(const char *, std::size_t, ProfileControlFrame &) noexcept;
bool encodeProfileControl(const ProfileControlFrame &, char *, std::size_t, std::size_t &) noexcept;
bool decodeProfileResponse(const char *, std::size_t, ProfileResponseFrame &) noexcept;
bool encodeProfileResponse(const ProfileResponseFrame &, char *, std::size_t,
                           std::size_t &) noexcept;
// Sole application context only. ready() provides one-response backpressure.
class IProfileControlSink {
  public:
    virtual ~IProfileControlSink() = default;
    virtual void service() noexcept = 0;
    virtual bool ready() const noexcept = 0;
    virtual void sourceAttempted(const TrustedAuthority &original) noexcept = 0;
    virtual void handle(const ProfileControlFrame &, const TrustedAuthority &,
                        std::uint32_t) noexcept = 0;
};
} // namespace companion
} // namespace larp
