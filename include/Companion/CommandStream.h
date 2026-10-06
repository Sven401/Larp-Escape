#pragma once
#include "Companion/CommandFrame.h"
#include "Companion/CommandIngress.h"
#include <array>
namespace larp {
namespace companion {
enum class CommandStreamOutcome : std::uint8_t {
    Idle,
    Receiving,
    DuplicateFragment,
    Queued,
    QueueFull,
    DecodeRejected,
    InvalidFrame,
    UnsupportedFrame,
    Unauthorized,
    Busy,
    OutOfOrder,
    FragmentConflict,
    Expired,
    Uninitialized
};
struct CommandStreamResult {
    CommandStreamOutcome outcome = CommandStreamOutcome::Idle;
    std::uint16_t message = 0, total = 0, received = 0;
    // Only Queued/QueueFull/DecodeRejected mean the decoder was reached.
    GameSubmission submission;
};
struct CommandStreamSnapshot {
    std::uint16_t message = 0, total = 0, received = 0;
    bool complete = false;
};
// One task owns receive/retry/poll/snapshot. No ISR/concurrent callback use.
// Only copied values reach the application. No heap or whole-profile storage.
// Principal/epoch belong to the verified frame's connection, NEVER wire bytes.
class CommandStream {
  public:
    explicit CommandStream(ICommandIngress &sink) : sink_(sink) {}
    CommandStream(const CommandStream &) = delete;
    CommandStream &operator=(const CommandStream &) = delete;
    CommandStreamResult receive(const char *bytes, std::size_t count, std::uint32_t principal,
                                std::uint32_t epoch, std::uint32_t now) noexcept;
    // Queued callback path: original trusted receipt time sets the first-frame
    // absolute deadline. Reject expired/future timestamp before any admission.
    CommandStreamResult receive(const char *bytes, std::size_t count, std::uint32_t principal,
                                std::uint32_t epoch, std::uint32_t receivedAt,
                                std::uint32_t now) noexcept;
    // Explicit bounded retry when complete + QueueFull. Does not tick the game.
    CommandStreamResult retry(std::uint32_t now) noexcept;
    // Owner polls even without frames to discard expired/revoked partial data.
    CommandStreamResult poll(std::uint32_t now) noexcept;
    CommandStreamSnapshot snapshot() const noexcept;

  private:
    ICommandIngress &sink_;
    std::array<char, maxGameCommandDocumentBytes> buffer_ = {};
    std::uint32_t principal_ = 0, epoch_ = 0, started_ = 0;
    std::uint16_t message_ = 0, total_ = 0, received_ = 0;
    void discard() noexcept;
    bool refresh(std::uint32_t now, TrustedAuthority &authority,
                 CommandStreamOutcome &event) noexcept;
    CommandStreamResult result(CommandStreamOutcome outcome) const noexcept;
    CommandStreamResult submit() noexcept;
};
} // namespace companion
} // namespace larp
