#pragma once
#include "GameExecution/RulesPlanner.h"
#include <array>

namespace larp {
namespace integration {
class EventAuthoritySetupOwner;
}
namespace devices {
namespace event_authority {
using configuration::module_ir::Pool;
using execution::rules::Digest;
using execution::rules::Identity;
constexpr std::size_t MaximumEvidenceBytes = 2048;
constexpr std::size_t RecordBytes = 2200;
enum class Phase : std::uint8_t { Closed = 1, Bound = 2 };
struct Snapshot {
    Identity device, event;
    std::uint64_t generation = 0;
    std::uint32_t registryGeneration = 0;
    Digest registryDigest, originalAuthorizationDigest;
    Phase phase = Phase::Closed;
};
struct Record {
    std::array<std::uint8_t, RecordBytes> bytes{};
};
enum class Read : std::uint8_t { Present, Missing, Failure };
enum class Write : std::uint8_t { Confirmed, Conflict, Uncertain, Failure };
class IStore {
  public:
    virtual ~IStore() = default;
    // Observable shared backend session. fence() permanently closes read and
    // replace for ALL new owner objects on this session, not just this object.
    // Only a genuinely reopened/qualified backend may provide a fresh session;
    // no reset flag, cached record or recreated Storage revives the old one.
    virtual bool fenced() const = 0;
    virtual Read read(Record &) = 0;
    // Full exact CAS+durable readback, no erase or automatic migration.
    // Failure and Conflict guarantee this request changed NO bytes. Every
    // possibly partial/complete write without qualified durable confirmation
    // MUST return Uncertain, including failed readback after an actual CAS.
    virtual Write replace(const Record *expected, const Record &next) = 0;
    // Shared same-boot close survives replacement of owner objects.
    virtual void fence() = 0;
};
enum class Proof : std::uint8_t { VerifiedOriginal, Rejected, Unavailable };
class IOriginalAuthorization {
  public:
    virtual ~IOriginalAuthorization() = default;
    // Must verify the ACTUAL original setup/install evidence and all copied
    // box/event/registry bindings. A record hash, caller bool, current GM role,
    // D0 origin-key signature or current generic registry is NOT this proof.
    // No positive implementation is provided until the separate setup format,
    // first enrollment and local administrator proof are actually implemented.
    virtual Proof verify(const Snapshot &, const std::uint8_t *, std::size_t) = 0;
};
struct Workspace {
    Record current, next, readback;
};
enum class Outcome : std::uint8_t { Closed, Ready, Missing, Rejected, Unknown };
enum class Error : std::uint8_t {
    None,
    Busy,
    Context,
    Canonical,
    Crypto,
    Authorization,
    Unavailable,
    Storage,
    Conflict,
    Overflow
};
struct Result {
    Outcome outcome = Outcome::Rejected;
    Error error = Error::None;
};
// Sole serialized setup/startup worker, not a manual-control install channel.
// All records/workspace/hash/verifier are exclusive and callbacks nonreentrant.
// Public load/close cannot create event rights. Positive installation is private
// to a future concrete setup owner; generic registry writes cannot open this gate.
class Storage {
  public:
    Storage(IStore &store, configuration::IPackageDigest &hash, IOriginalAuthorization &proof,
            Workspace &workspace)
        : store_(store), hash_(hash), proof_(proof), work_(workspace) {}
    Storage(const Storage &) = delete;
    Storage &operator=(const Storage &) = delete;
    Result load(const Identity &device);
    Result close(std::uint64_t expectedGeneration);
    bool snapshot(Snapshot &);
    // Reads original owned evidence only for an already fully checked binding.
    bool evidence(Pool<std::uint8_t> &destination);

  private:
    friend class larp::integration::EventAuthoritySetupOwner;
    IStore &store_;
    configuration::IPackageDigest &hash_;
    IOriginalAuthorization &proof_;
    Workspace &work_;
    Snapshot state_;
    std::uint16_t evidenceBytes_ = 0;
    bool operating_ = false, ready_ = false, present_ = false, failed_ = false;
    Result fail(Error, bool unknown = false);
    bool storedCurrent();
    bool digest(const std::uint8_t *, std::size_t, Digest &);
    // Future setup owner must first durably close event rights AND old source
    // connections, then change registry while closed, then call this with the
    // independently verified original install proof. Every crash phase is closed.
    Result install(std::uint64_t expectedGeneration, const Snapshot &,
                   const std::uint8_t *originalEvidence, std::size_t count);
};
} // namespace event_authority
} // namespace devices
} // namespace larp
