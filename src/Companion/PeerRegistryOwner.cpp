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
    store_.fence();
    runtime_.close(); // Original actor must preserve any replacement authority.
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
    if (!runtime_.close())
        return fail(PeerRegistryOutcome::RuntimeRejected);
    if (!device.valid())
        return fail(PeerRegistryOutcome::Invalid);
    PeerRegistryRecord record;
    const auto read = store_.read(record);
    if (read == PeerRegistryRead::Missing) {
        ready_ = true;
        return last_ = PeerRegistryOutcome::Empty;
    }
    if (read != PeerRegistryRead::Present)
        return fail(PeerRegistryOutcome::ReadFailure);
    PeerRegistrySnapshot selected;
    if (!decodePeerRegistry(record, selected) || !(selected.device == device))
        return fail(PeerRegistryOutcome::Corrupt);
    if (!validKeys(selected))
        return fail(PeerRegistryOutcome::CryptoRejected);
    if (!runtime_.replaceClosed(selected))
        return fail(PeerRegistryOutcome::RuntimeRejected);
    current_ = selected;
    record_ = record;
    present_ = ready_ = true;
    return last_ = PeerRegistryOutcome::Loaded;
}
PeerRegistryOutcome PeerRegistryOwner::replaceTrusted(std::uint32_t expected,
                                                      const PeerRegistrySnapshot &caller) noexcept {
    const auto selected = caller;
    if (busy_)
        return PeerRegistryOutcome::Busy;
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
    PeerRegistryRecord next;
    if (!encodePeerRegistry(selected, next))
        return PeerRegistryOutcome::Invalid;
    ready_ = false; // Registry readers cannot prove a key during any mutation.
    if (!runtime_.close())
        return fail(PeerRegistryOutcome::RuntimeRejected);
    const auto written = store_.replace(present_ ? &record_ : nullptr, next);
    if (written == PeerRegistryWrite::Conflict)
        return fail(PeerRegistryOutcome::WriteConflict);
    if (written == PeerRegistryWrite::Uncertain)
        return fail(PeerRegistryOutcome::Uncertain);
    if (written != PeerRegistryWrite::Confirmed)
        return fail(PeerRegistryOutcome::WriteFailure);
    PeerRegistryRecord confirmed;
    PeerRegistrySnapshot decoded;
    if (store_.read(confirmed) != PeerRegistryRead::Present || !(confirmed == next) ||
        !decodePeerRegistry(confirmed, decoded) || !(decoded == selected))
        return fail(PeerRegistryOutcome::VerifyFailure);
    if (!runtime_.replaceClosed(selected))
        return fail(PeerRegistryOutcome::RuntimeRejected);
    record_ = confirmed;
    current_ = selected;
    present_ = ready_ = true;
    return last_ = PeerRegistryOutcome::Saved;
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
