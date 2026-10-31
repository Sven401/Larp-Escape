#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace larp {
namespace companion {
namespace event_transport {

constexpr std::size_t MessageLimit = 2048; // D0-FORMAT, not an ESP-NOW frame size.
constexpr std::size_t QueueCapacity = 4;
constexpr std::uint64_t DispatchIntervalMs = 100;
constexpr std::uint64_t RetryLimitMs = 1600;
using Id = std::array<std::uint8_t, 16>;
using Digest = std::array<std::uint8_t, 32>;

struct FactId {
    Id event{};
    Id origin{};
    Id epoch{};
    std::uint64_t sequence = 0;
};
bool operator==(const FactId &left, const FactId &right);

// Metadata must come from verification of the ORIGINAL signed bytes. Digest is
// SHA-256 of canonical payload bytes, never of the signature or transport frame.
struct Fact {
    FactId id{};
    Id run{};
    Digest digest{};
};
enum class Stage { Transport, Durable, Applied, Observed, Rejected, Deferred, EffectUnknown };
struct Receipt {
    Fact fact{};
    Id confirmer{};
    Id recipient{};
    Id receiptId{};
    Stage stage = Stage::Transport;
};
enum class Verification { Unavailable, Rejected, Verified };

// D0-G1/G2 boundary. No production verifier is supplied. Implementations must
// validate framing, signature, context, current trust/revocation and permissions.
// verifyReceipt also validates D0 stage/reason combinations and response identity.
class OriginVerifier {
  public:
    virtual ~OriginVerifier() = default;
    virtual Verification verifyFact(const std::uint8_t *, std::size_t, Fact &) {
        return Verification::Unavailable;
    }
    virtual Verification verifyReceipt(const std::uint8_t *, std::size_t, Receipt &) {
        return Verification::Unavailable;
    }
};

enum class Publication { Unavailable, Denied, Durable };
enum class Acceptance { Unavailable, Rejected, Deferred, Conflict, Durable, AlreadyDurable };
enum class ReceiptCommit { Unavailable, Unmatched, Retained, Conflict, TargetDurable };

// D1 boundary, not a storage implementation. All operations must be bounded and
// nonblocking. Recheck authority at the durable commit point. Retain journal,
// replay and application records independently of these volatile queue slots.
class DurablePort {
  public:
    virtual ~DurablePort() = default;
    virtual Publication mayPublish(const Fact &, const Id &, const std::uint8_t *, std::size_t) {
        return Publication::Unavailable;
    }
    virtual Acceptance accept(const Fact &, const std::uint8_t *, std::size_t) {
        return Acceptance::Unavailable;
    }
    // Correlate fact/run/digest/confirmer/recipient against the durable journal,
    // including after volatile slot release/reboot; return Unmatched if unknown.
    // Merge stages/conflicts durably; TargetDurable means this exact target's
    // durable receipt is committed. Never manufacture Applied/Observed here.
    virtual ReceiptCommit recordReceipt(const Receipt &) {
        return ReceiptCommit::Unavailable;
    }
};

enum class Bearer { EspNow, Wlan };
enum class Admission { Queued, Duplicate, Full, Invalid, Unauthorized, Unavailable, Conflict };
enum class Poll { Idle, Ready, Blocked, ClockError, Exhausted };
enum class LinkResult { Accepted, Failed };
enum class ReceiptResult {
    Invalid,
    Unauthorized,
    Unavailable,
    Unmatched,
    Retained,
    Released,
    Conflict,
    RateLimited,
    ClockError
};

// Owned copy: a radio adapter may keep this after the scheduler changes slots.
// token is process-local attempt correlation, never a wire or event identifier.
struct Attempt {
    std::array<std::uint8_t, MessageLimit> bytes{};
    std::size_t size = 0;
    Fact fact{};
    Id target{};
    Bearer bearer = Bearer::EspNow;
    std::uint64_t token = 0;
};
struct IncomingResult {
    Fact fact{};
    Acceptance acceptance = Acceptance::Unavailable;
    Verification verification = Verification::Unavailable;
};

// One serialized owner only, including callbacks. Not ISR/thread-safe or
// reentrant. No sleeps, allocations, radio APIs, application actions or codecs.
// Clock is monotonic milliseconds for this instance; backward/overflow fails
// closed. Reconstruct after reboot from D1, never from these volatile buffers.
class Core {
  public:
    Core(OriginVerifier &verifier, DurablePort &durable);
    Core(const Core &) = delete;
    Core &operator=(const Core &) = delete;
    Admission enqueue(const std::uint8_t *bytes, std::size_t size, const Id &target);
    Admission receive(const std::uint8_t *bytes, std::size_t size);
    Poll poll(std::uint64_t now, Bearer bearer, bool contact, Attempt &attempt);
    // Link/MAC acceptance is diagnostic only. Never releases or confirms a fact.
    bool linkResult(std::uint64_t token, LinkResult result);
    Poll processIncoming(std::uint64_t now, IncomingResult &result);
    ReceiptResult receipt(std::uint64_t now, const std::uint8_t *bytes, std::size_t size);
    std::size_t outgoingCount() const;
    std::size_t incomingCount() const;

  private:
    struct Message {
        std::array<std::uint8_t, MessageLimit> bytes{};
        std::size_t size = 0;
        std::uint64_t next = 0;
        std::uint64_t delay = DispatchIntervalMs;
    };
    struct Outgoing {
        Message message{};
        Fact fact{};
        Id target{};
        std::uint64_t token = 0;
        bool blocked = false;
        bool conflict = false;
    };
    bool time(std::uint64_t now);
    static bool schedule(Message &message, std::uint64_t now);
    OriginVerifier &verifier_;
    DurablePort &durable_;
    std::array<Outgoing, QueueCapacity> outgoing_{};
    std::array<Message, QueueCapacity> incoming_{};
    std::size_t nextOut_ = 0;
    std::size_t nextIn_ = 0;
    std::uint64_t lastTime_ = 0;
    std::uint64_t nextSend_ = 0;
    std::uint64_t nextReceive_ = 0;
    std::uint64_t token_ = 0;
};

// Budget applies to the core itself, not caller copies, verifier/D1 or SDK RAM.
static_assert(sizeof(Core) <= 20 * 1024, "D2 core exceeds its fixed RAM budget");

} // namespace event_transport
} // namespace companion
} // namespace larp
