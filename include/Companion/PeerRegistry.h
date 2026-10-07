#pragma once
#include "Companion/PeerProof.h"
namespace larp {
namespace companion {
struct PeerRegistryLimits {
    static const std::size_t Peers = 4, EntryBytes = 92, RecordBytes = 404;
};
struct PeerRegistryEntry {
    PeerCredential credential;
    std::uint32_t principal = 0;
    bool gameMaster = false;
    bool operator==(const PeerRegistryEntry &other) const noexcept {
        return credential == other.credential && principal == other.principal &&
               gameMaster == other.gameMaster;
    }
};
struct PeerRegistrySnapshot {
    Identity device;
    std::uint32_t generation = 0;
    std::uint8_t count = 0;
    std::array<PeerRegistryEntry, PeerRegistryLimits::Peers> peers = {};
    bool operator==(const PeerRegistrySnapshot &other) const noexcept {
        return device == other.device && generation == other.generation && count == other.count &&
               peers == other.peers;
    }
};
struct PeerRegistryRecord {
    std::array<std::uint8_t, PeerRegistryLimits::RecordBytes> bytes = {};
    bool operator==(const PeerRegistryRecord &other) const noexcept {
        return bytes == other.bytes;
    }
};
// Canonical sorted unique labels/keys, explicit GM, unique assigned principals.
// Generation0 is only the in-memory positively missing empty registry.
// Codec validates structure/CRC, not the P256 curve, storage origin or roles.
bool validPeerRegistry(const PeerRegistrySnapshot &) noexcept;
bool encodePeerRegistry(const PeerRegistrySnapshot &, PeerRegistryRecord &) noexcept;
bool decodePeerRegistry(const PeerRegistryRecord &, PeerRegistrySnapshot &) noexcept;
enum class PeerRegistryRead : std::uint8_t { Present, Missing, Failure };
enum class PeerRegistryWrite : std::uint8_t { Confirmed, Conflict, Uncertain, Failure };
class IPeerRegistryStore {
  public:
    virtual ~IPeerRegistryStore() = default;
    virtual PeerRegistryRead read(PeerRegistryRecord &) noexcept = 0;
    // nullptr expects positively absent key; otherwise exact current bytes.
    // CAS and confirm under one namespace owner. Uncertain may have persisted.
    virtual PeerRegistryWrite replace(const PeerRegistryRecord *expected,
                                      const PeerRegistryRecord &next) noexcept = 0;
    // Same-boot shared fence survives store/owner object replacement. No reset.
    virtual void fence() noexcept = 0;
};
class IPeerRegistryRuntime {
  public:
    virtual ~IPeerRegistryRuntime() = default;
    virtual bool close() noexcept = 0;
    // Serialized on the original IdentityAccess owner. Always close the current
    // exact authority and invalidate old authentication BEFORE replacing roles.
    // Never opens a link. An SDK worker needs original attributed actor/ack.
    virtual bool replaceClosed(const PeerRegistrySnapshot &) noexcept = 0;
};
enum class PeerRegistryOutcome : std::uint8_t {
    Loaded,
    Empty,
    Saved,
    Duplicate,
    Busy,
    NotReady,
    Invalid,
    Stale,
    CounterExhausted,
    ReadFailure,
    Corrupt,
    CryptoRejected,
    RuntimeRejected,
    WriteConflict,
    WriteFailure,
    Uncertain,
    VerifyFailure
};
// One serialized trusted local admin/startup worker, never an App JSON write or
// callback. Public keys only; crypto/NVS may allocate and block outside critical sections.
// A failed IO/closure/restore permanently closes this owner and shared store.
class PeerRegistryOwner {
  public:
    PeerRegistryOwner(IPeerRegistryStore &store, IPeerProofCrypto &crypto,
                      IPeerRegistryRuntime &runtime)
        : store_(store), crypto_(crypto), runtime_(runtime) {}
    PeerRegistryOwner(const PeerRegistryOwner &) = delete;
    PeerRegistryOwner &operator=(const PeerRegistryOwner &) = delete;
    PeerRegistryOutcome begin(const Identity &) noexcept;
    // Explicit complete local edit at current generation; next is generation+1.
    // Existing key rotation increments its credential generation exactly once.
    PeerRegistryOutcome replaceTrusted(std::uint32_t expectedGeneration,
                                       const PeerRegistrySnapshot &next) noexcept;
    bool snapshot(PeerRegistrySnapshot &) const noexcept;
    bool credential(const Identity &, PeerCredential &) const noexcept;
    bool credentialCurrent(const PeerCredential &) const noexcept;

  private:
    IPeerRegistryStore &store_;
    IPeerProofCrypto &crypto_;
    IPeerRegistryRuntime &runtime_;
    PeerRegistrySnapshot current_;
    PeerRegistryRecord record_;
    PeerRegistryOutcome last_ = PeerRegistryOutcome::NotReady;
    bool attempted_ = false, ready_ = false, busy_ = false, present_ = false;
    bool validKeys(const PeerRegistrySnapshot &) noexcept;
    PeerRegistryOutcome fail(PeerRegistryOutcome) noexcept;
};
} // namespace companion
} // namespace larp
