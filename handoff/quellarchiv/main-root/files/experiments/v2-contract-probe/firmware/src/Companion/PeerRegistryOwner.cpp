#include "Companion/PeerRegistry.h"
namespace larp {
namespace companion {
namespace {
class PeerRegistryCall {
  public:
    explicit PeerRegistryCall(bool &busy) : busy_(busy) {
        busy_ = true;
    }
    ~PeerRegistryCall() {
        busy_ = false;
    }

  private:
    bool &busy_;
};
bool generationsCurrent(const PeerRegistrySnapshot &before, const PeerRegistrySnapshot &after) {
    for (unsigned i = 0; i < after.count; ++i) {
        const auto &next = after.peers[i].credential;
        bool existing = false;
        for (unsigned j = 0; j < before.count; ++j) {
            const auto &old = before.peers[j].credential;
            if (!(old.peer == next.peer))
                continue;
            existing = true;
            if (old.key == next.key) {
                if (old.generation != next.generation)
                    return false;
            } else if (old.generation == UINT32_MAX || next.generation != old.generation + 1)
                return false;
        }
        if (!existing && next.generation != 1)
            return false;
    }
    return true;
}
} // namespace
bool PeerRegistryOwner::validKeys(const PeerRegistrySnapshot &selected) noexcept {
    for (unsigned i = 0; i < selected.count; ++i)
        if (!crypto_.validKey(selected.peers[i].credential.key))
            return false;
    return true;
}
PeerRegistryOutcome PeerRegistryOwner::fail(PeerRegistryOutcome value) noexcept {
    ready_ = false;
    phase_ = Phase::Failed;
    runtime_.fence(); // No delayed role install may republish credentials.
    store_.fence();
    return last_ = value;
}
PeerRegistryOutcome PeerRegistryOwner::begin(const Identity &caller) noexcept {
    const auto device = caller;
    if (busy_)
        return PeerRegistryOutcome::Busy;
    PeerRegistryCall call(busy_);
    if (attempted_)
        return device == current_.device ? last_ : fail(PeerRegistryOutcome::Invalid);
    attempted_ = true;
    current_.device = device;
    // First and only begin: next_ is already the constructor-zeroed empty snapshot.
    next_.device = device;
    phase_ = Phase::BeginClose;
    last_ = PeerRegistryOutcome::Pending;
    return advance();
}
PeerRegistryOutcome PeerRegistryOwner::replaceTrusted(std::uint32_t expected,
                                                      const PeerRegistrySnapshot &caller) noexcept {
    const auto selected = caller;
    if (busy_)
        return PeerRegistryOutcome::Busy;
    if (phase_ != Phase::Idle && phase_ != Phase::Failed) {
        return expected == current_.generation && selected == next_ ? PeerRegistryOutcome::Pending
                                                                    : PeerRegistryOutcome::Busy;
    }
    if (!ready_)
        return PeerRegistryOutcome::NotReady;
    PeerRegistryCall call(busy_);
    if (expected != current_.generation)
        return PeerRegistryOutcome::Stale;
    if (expected == UINT32_MAX)
        return PeerRegistryOutcome::CounterExhausted;
    if (!(selected.device == current_.device) || selected.generation != expected + 1 ||
        !validPeerRegistry(selected) || !generationsCurrent(current_, selected))
        return PeerRegistryOutcome::Invalid;
    if (selected.count == current_.count && selected.peers == current_.peers)
        return PeerRegistryOutcome::Duplicate;
    if (!validKeys(selected))
        return PeerRegistryOutcome::CryptoRejected;
    if (!encodePeerRegistry(selected, nextRecord_))
        return PeerRegistryOutcome::Invalid;
    next_ = selected;
    ready_ = false;
    phase_ = Phase::EditClose;
    completion_ = PeerRegistryOutcome::Saved;
    last_ = PeerRegistryOutcome::Pending;
    return advance();
}
PeerRegistryOutcome PeerRegistryOwner::service() noexcept {
    if (busy_)
        return PeerRegistryOutcome::Busy;
    PeerRegistryCall call(busy_);
    return advance();
}
PeerRegistryOutcome PeerRegistryOwner::advance() noexcept {
    // At most three phases, no waiting/reposting. A synchronous trusted port
    // retains the direct API; an SDK port yields immediately until its ACK.
    for (unsigned step = 0; step < 3; ++step) {
        if (phase_ == Phase::Idle || phase_ == Phase::Failed)
            return last_;
        if (phase_ == Phase::BeginClose || phase_ == Phase::EditClose) {
            const auto result = runtime_.close();
            if (result == PeerRegistryRuntimeResult::Pending)
                return last_ = PeerRegistryOutcome::Pending;
            if (result != PeerRegistryRuntimeResult::Confirmed)
                return fail(PeerRegistryOutcome::RuntimeRejected);
            if (phase_ == Phase::BeginClose) {
                if (!current_.device.valid())
                    return fail(PeerRegistryOutcome::Invalid);
                phase_ = Phase::Read;
            } else
                phase_ = Phase::Write;
        } else if (phase_ == Phase::Read) {
            const auto read = store_.read(nextRecord_);
            if (read == PeerRegistryRead::Missing) {
                completion_ = PeerRegistryOutcome::Empty;
            } else {
                if (read != PeerRegistryRead::Present)
                    return fail(PeerRegistryOutcome::ReadFailure);
                if (!decodePeerRegistry(nextRecord_, next_) || !(next_.device == current_.device))
                    return fail(PeerRegistryOutcome::Corrupt);
                if (!validKeys(next_))
                    return fail(PeerRegistryOutcome::CryptoRejected);
                completion_ = PeerRegistryOutcome::Loaded;
            }
            phase_ = Phase::Install;
        } else if (phase_ == Phase::Write) {
            const auto written = store_.replace(present_ ? &record_ : nullptr, nextRecord_);
            if (written == PeerRegistryWrite::Conflict)
                return fail(PeerRegistryOutcome::WriteConflict);
            if (written == PeerRegistryWrite::Uncertain)
                return fail(PeerRegistryOutcome::Uncertain);
            if (written != PeerRegistryWrite::Confirmed)
                return fail(PeerRegistryOutcome::WriteFailure);
            PeerRegistryRecord confirmed;
            PeerRegistrySnapshot decoded;
            if (store_.read(confirmed) != PeerRegistryRead::Present ||
                !(confirmed == nextRecord_) || !decodePeerRegistry(confirmed, decoded) ||
                !(decoded == next_))
                return fail(PeerRegistryOutcome::VerifyFailure);
            phase_ = Phase::Install;
        } else {
            const auto result = runtime_.replaceClosed(next_);
            if (result == PeerRegistryRuntimeResult::Pending)
                return last_ = PeerRegistryOutcome::Pending;
            if (result != PeerRegistryRuntimeResult::Confirmed)
                return fail(PeerRegistryOutcome::RuntimeRejected);
            current_ = next_;
            record_ = nextRecord_;
            present_ = completion_ != PeerRegistryOutcome::Empty;
            ready_ = true;
            phase_ = Phase::Idle;
            return last_ = completion_;
        }
    }
    return last_ = PeerRegistryOutcome::Pending;
}
bool PeerRegistryOwner::snapshot(PeerRegistrySnapshot &destination) const noexcept {
    if (!ready_ || busy_)
        return false;
    destination = current_;
    return true;
}
bool PeerRegistryOwner::credential(const Identity &caller,
                                   PeerCredential &destination) const noexcept {
    const auto peer = caller;
    if (!ready_ || busy_ || !peer.valid())
        return false;
    for (unsigned i = 0; i < current_.count; ++i)
        if (current_.peers[i].credential.peer == peer) {
            destination = current_.peers[i].credential;
            return true;
        }
    return false;
}
bool PeerRegistryOwner::credentialCurrent(const PeerCredential &caller) const noexcept {
    const auto selected = caller;
    PeerCredential current;
    return credential(selected.peer, current) && current == selected;
}
} // namespace companion
} // namespace larp
