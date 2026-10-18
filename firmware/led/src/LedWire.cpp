#include "LedWire.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace larp {
namespace ledwire {
namespace {
const char *const ChallengeDomain = "LARP-LED/2/challenge";
const char *const ProofDomain = "LARP-LED/2/proof";
const char *const SessionDomain = "LARP-LED/2/session";
const char *const CommandDomain = "LARP-LED/2/command";
const char *const StatusDomain = "LARP-LED/2/status";
const char *const ResultDomain = "LARP-LED/2/result";
const char *const AudioCommandDomain = "LARP-LED/2/audio-command";
const char *const AudioResultDomain = "LARP-LED/2/audio-result";
bool nonzero(const std::uint8_t *p, std::size_t n) noexcept {
    std::uint8_t any = 0;
    for (std::size_t i = 0; i < n; ++i) any |= p[i];
    return any != 0;
}
bool equal(const std::uint8_t *a, const std::uint8_t *b, std::size_t n) noexcept {
    std::uint8_t difference = 0;
    for (std::size_t i = 0; i < n; ++i) difference |= a[i] ^ b[i];
    return difference == 0;
}
bool same(const Message &a, const Message &b) noexcept {
    return a.size == b.size && equal(a.bytes.data(), b.bytes.data(), a.size);
}
std::uint16_t read16(const std::uint8_t *p) noexcept {
    return static_cast<std::uint16_t>(p[0] | (std::uint16_t(p[1]) << 8));
}
void write16(std::uint8_t *p, std::uint16_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
}
bool valid(const Snapshot &s) noexcept {
    const auto effect = static_cast<unsigned>(s.effect);
    return effect <= 4 && (s.effect != Effect::Resonance || s.resonanceSupported) &&
           (!s.resonanceSupported || s.wakeSupported) &&
           static_cast<unsigned>(s.mode) <= 4 && (s.flags & ~31U) == 0 &&
           ((s.flags & Off) != 0) == (s.effect == Effect::Off) &&
           !((s.flags & Ready) && (s.flags & Fault)) &&
           (s.wakeMask & ~15U) == 0 && (!s.wakeValid || s.wakeSupported) &&
           (s.wakeValid || s.wakeMask == 0) &&
           (!s.matrixValid || s.matrixSupported) &&
           (!s.matrixAmbiguous || s.matrixValid) &&
           (s.matrixBits.back() & ~1U) == 0 &&
           (s.matrixValid || !nonzero(s.matrixBits.data(), s.matrixBits.size()));
}
bool valid(const AudioSnapshot &s) noexcept {
    return s.track <= 255 && s.volume <= 10 && (!s.busyLow || s.busyValid) &&
           !(s.ready && s.fault) && !(s.active && s.paused) && (!(s.active || s.paused) || (s.track && s.volume));
}
void writeAudio(std::uint8_t *p, const AudioSnapshot &s) noexcept {
    write32(p, s.revision); write16(p + 4, s.track); p[6] = s.volume;
    p[7] = (s.ready ? 1 : 0) | (s.pending ? 2 : 0) | (s.active ? 4 : 0) |
           (s.busyValid ? 8 : 0) | (s.busyLow ? 16 : 0) | (s.fault ? 32 : 0) | (s.paused ? 64 : 0);
}
Snapshot commandState(const Snapshot &led, const AudioSnapshot &audio, bool isAudio) noexcept {
    if (!isAudio) return led;
    Snapshot result = led;
    result.revision = audio.revision; result.appliedRevision = audio.appliedRevision;
    result.effect = audio.active ? Effect::Twinkle : Effect::Off;
    result.flags = (audio.ready ? Ready : 0) | (audio.pending ? Pending : 0) |
                   (audio.fault ? Fault : 0) | (audio.active ? 0 : Off);
    return result;
}
} // namespace
std::uint32_t read32(const std::uint8_t *p) noexcept {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
void write32(std::uint8_t *p, std::uint32_t v) noexcept {
    for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}
bool Session::configure(const Nonce &device, const Secret &secret) noexcept {
    if (connected() || !nonzero(device.data(), device.size()) ||
        !nonzero(secret.data(), secret.size())) return false;
    device_ = device;
    secret_ = secret;
    configured_ = true;
    return true;
}
bool Session::beginConnection(const Nonce &boot, const Nonce &connection,
                              std::uint32_t now) noexcept {
    if (connected() || !configured_ || !nonzero(boot.data(), boot.size()) ||
        !nonzero(connection.data(), connection.size())) return false;
    boot_ = boot;
    connection_ = connection;
    client_.fill(0);
    session_.fill(0);
    context_.fill(0);
    challenge_ = {};
    lastCommand_ = {};
    lastReply_ = {};
    pendingCommand_ = {};
    pending_ = false; nfcPending_ = false;
    pendingAudio_ = false; pendingEspNow_ = false;
    nextRequest_ = 1;
    statusSequence_ = lastRequest_ = 0;
    connectedAt_ = now;
    phase_ = Phase::Hello;
    return true;
}
void Session::disconnect() noexcept {
    const bool wasConnected = connected();
    phase_ = Phase::Closed;
    pending_ = false; nfcPending_ = false;
    client_.fill(0);
    session_.fill(0);
    context_.fill(0);
    lastCommand_ = {};
    pendingCommand_ = {};
    lastReply_ = {};
    challenge_ = {};
    if (wasConnected) control_.stopOnDisconnect();
}
bool Session::tag(const char *domain, const std::uint8_t *p, std::size_t n,
                  Secret &out) noexcept {
    std::array<std::uint8_t, 160> input = {};
    const std::size_t prefix = std::strlen(domain);
    if (prefix + n > input.size()) return false;
    std::memcpy(input.data(), domain, prefix);
    std::memcpy(input.data() + prefix, p, n);
    return crypto_.calculate(secret_, input.data(), prefix + n, out);
}
bool Session::verify(const char *domain, const Message &m, std::size_t body) noexcept {
    Secret expected = {};
    return m.size == body + expected.size() && tag(domain, m.bytes.data(), body, expected) &&
           equal(expected.data(), m.bytes.data() + body, expected.size());
}
SessionResult Session::reject(Message &out) noexcept {
    out.size = 0;
    disconnect();
    return SessionResult::Rejected;
}
bool Session::status(std::uint8_t type, std::uint32_t now, Message &out) noexcept {
    out = {};
    const auto state = control_.snapshot();
    const auto audio = control_.audioSnapshot();
    if (!authenticated() || !valid(state) || (audio.supported && !valid(audio)) ||
        statusSequence_ == std::numeric_limits<std::uint32_t>::max()) {
        disconnect();
        return false;
    }
    const std::size_t body = 59;
    out.size = static_cast<std::uint16_t>(body + 32);
    out.bytes[0] = 2;
    out.bytes[1] = type;
    std::copy(session_.begin(), session_.end(), out.bytes.begin() + 2);
    write32(out.bytes.data() + 18, state.revision);
    write32(out.bytes.data() + 22, nextRequest_);
    write32(out.bytes.data() + 26, now);
    write32(out.bytes.data() + 30, ++statusSequence_);
    out.bytes[34] = static_cast<std::uint8_t>(state.effect);
    out.bytes[35] = state.flags;
    out.bytes[36] = static_cast<std::uint8_t>(state.mode);
    out.bytes[37] = (state.wakeSupported ? 0x80 : 0) |
                    (state.wakeValid ? 0x40 : 0) | (state.nfcSupported ? 0x20 : 0) |
                    (state.resonanceSupported ? 0x10 : 0) | state.wakeMask;
    const auto esp = control_.espNowSnapshot();
    out.bytes[38] = (audio.supported ? 1 : 0) | (esp.supported ? 2 : 0) | (esp.supported && esp.ready ? 4 : 0) | (esp.rssiSupported ? 8 : 0);
    if (audio.supported) writeAudio(out.bytes.data() + 39, audio);
    if (state.matrixSupported) {
        out.bytes[47] = 0x80 | (state.matrixValid ? 0x40 : 0) |
                        (state.matrixAmbiguous ? 0x20 : 0);
        std::copy(state.matrixBits.begin(), state.matrixBits.end(), out.bytes.begin() + 48);
    }
    Secret mac = {};
    if (!tag(StatusDomain, out.bytes.data(), body, mac)) {
        disconnect(); out.size = 0; return false;
    }
    std::copy(mac.begin(), mac.end(), out.bytes.begin() + body);
    return true;
}
bool Session::result(Outcome outcome, std::uint32_t request, std::uint32_t now,
                     Message &out, bool isAudio, bool isEspNow, bool isRssi) noexcept {
    out = {};
    const auto state = control_.snapshot();
    const auto audio = control_.audioSnapshot();
    if (!authenticated() || !valid(state) || (isAudio && !valid(audio)) ||
        statusSequence_ == std::numeric_limits<std::uint32_t>::max()) {
        disconnect(); return false;
    }
    const std::size_t body = isRssi ? 80 : isEspNow ? 76 : isAudio ? 50 : 42;
    out.size = static_cast<std::uint16_t>(body + 32);
    out.bytes[0] = 2; out.bytes[1] = isRssi ? 0x88 : isEspNow ? 0x87 : isAudio ? 0x85 : 0x83;
    std::copy(session_.begin(), session_.end(), out.bytes.begin() + 2);
    write32(out.bytes.data() + 18, request);
    write32(out.bytes.data() + 22, nextRequest_);
    write32(out.bytes.data() + 26, state.revision);
    write32(out.bytes.data() + 30, now);
    write32(out.bytes.data() + 34, ++statusSequence_);
    out.bytes[38] = static_cast<std::uint8_t>(outcome);
    out.bytes[39] = static_cast<std::uint8_t>(state.effect);
    out.bytes[40] = state.flags;
    out.bytes[41] = static_cast<std::uint8_t>(state.mode);
    if (isAudio) writeAudio(out.bytes.data() + 42, audio);
    if (isEspNow) {
        EspNowSnapshot esp{};
        const auto code = static_cast<unsigned>(outcome);
        if (pendingEspNow_ && pendingRequest_ == request && (code == 0 || code == 10 || code == 12 || code == 13))
            esp = control_.espNowSnapshot();
        out.bytes[42] = esp.accepted ? 1 : 0;
        out.bytes[43] = esp.link; out.bytes[44] = esp.received ? 1 : 0; out.bytes[45] = 1;
        std::copy(esp.peer.begin(), esp.peer.end(), out.bytes.begin() + 46);
        std::copy(esp.token.begin(), esp.token.end(), out.bytes.begin() + 52);
        write32(out.bytes.data() + 68, esp.receiverCount);
        write32(out.bytes.data() + 72, esp.receiverBoot);
        if (isRssi) {
            out.bytes[76] = esp.rssiValid ? 1 : 0;
            out.bytes[77] = static_cast<std::uint8_t>(esp.rssi);
            out.bytes[78] = static_cast<std::uint8_t>(esp.filteredRssi);
            out.bytes[79] = esp.intensity;
        }
    }
    Secret mac = {};
    if (!tag(isRssi ? "LARP-LED/2/rssi-result" : isEspNow ? "LARP-LED/2/espnow-result" : isAudio ? AudioResultDomain : ResultDomain, out.bytes.data(), body, mac)) {
        disconnect(); out.size = 0; return false;
    }
    std::copy(mac.begin(), mac.end(), out.bytes.begin() + body);
    return true;
}
bool Session::finish(Outcome outcome, const Message &command, std::uint32_t now,
                     Message &out) noexcept {
    const auto request = read32(command.bytes.data() + 18);
    if (request != nextRequest_ || nextRequest_ == std::numeric_limits<std::uint32_t>::max())
        return false;
    ++nextRequest_;
    pending_ = false;
    if (!result(outcome, request, now, out, command.bytes[1] == 4, command.bytes[1] == 6 || command.bytes[1] == 7, command.bytes[1] == 7)) return false;
    lastRequest_ = request;
    lastCommand_ = command;
    lastReply_ = out;
    return true;
}
SessionResult Session::receive(const Message &m, std::uint32_t now, Message &out) noexcept {
    out = {};
    if (!connected() || m.size < 2 || m.size > m.bytes.size() || m.bytes[0] != 2)
        return reject(out);
    if (!authenticated() && now - connectedAt_ >= 10000U) return reject(out);
    if (m.bytes[1] == 1) {
        if (m.size != 18 || !nonzero(m.bytes.data() + 2, 16)) return reject(out);
        if (phase_ == Phase::Proof && equal(client_.data(), m.bytes.data() + 2, 16)) {
            out = challenge_; return SessionResult::Reply;
        }
        if (phase_ != Phase::Hello) return reject(out);
        std::copy_n(m.bytes.data() + 2, 16, client_.begin());
        std::copy(device_.begin(), device_.end(), context_.begin());
        std::copy(boot_.begin(), boot_.end(), context_.begin() + 16);
        std::copy(connection_.begin(), connection_.end(), context_.begin() + 32);
        std::copy(client_.begin(), client_.end(), context_.begin() + 48);
        challenge_ = {};
        challenge_.size = 98;
        challenge_.bytes[0] = 2; challenge_.bytes[1] = 0x81;
        std::copy(context_.begin(), context_.end(), challenge_.bytes.begin() + 2);
        Secret mac = {};
        if (!tag(ChallengeDomain, challenge_.bytes.data(), 66, mac)) return reject(out);
        std::copy(mac.begin(), mac.end(), challenge_.bytes.begin() + 66);
        phase_ = Phase::Proof;
        out = challenge_; return SessionResult::Reply;
    }
    if (m.bytes[1] == 2) {
        Secret expected = {}, identity = {};
        if (phase_ != Phase::Proof || m.size != 34 ||
            !tag(ProofDomain, context_.data(), context_.size(), expected) ||
            !equal(expected.data(), m.bytes.data() + 2, expected.size()) ||
            !tag(SessionDomain, context_.data(), context_.size(), identity))
            return reject(out);
        std::copy_n(identity.begin(), 16, session_.begin());
        phase_ = Phase::Authenticated;
        return status(0x82, now, out) ? SessionResult::Reply : SessionResult::Rejected;
    }
    const bool isAudio = m.bytes[1] == 4;
    const bool isRssi = m.bytes[1] == 7;
    const bool isEspNow = m.bytes[1] == 6 || isRssi;
    const auto body = isEspNow ? 38U : isAudio ? 34U : 32U;
    if (!authenticated() || (m.bytes[1] != 3 && !isAudio && !isEspNow) || m.size != body + 32 ||
        !equal(session_.data(), m.bytes.data() + 2, 16) ||
        !verify(isRssi ? "LARP-LED/2/rssi-command" : isEspNow ? "LARP-LED/2/espnow-command" : isAudio ? AudioCommandDomain : CommandDomain, m, body)) return reject(out);
    const auto request = read32(m.bytes.data() + 18);
    if (!request) return reject(out);
    if (lastRequest_ && request == lastRequest_) {
        if (same(m, lastCommand_)) { out = lastReply_; return SessionResult::Reply; }
        return result(Outcome::Conflict, request, now, out, isAudio, isEspNow, isRssi) ? SessionResult::Reply
                                                          : SessionResult::Rejected;
    }
    if (nfcPending_)
        return result(Outcome::Busy, request, now, out, isAudio, isEspNow, isRssi) ? SessionResult::Reply : SessionResult::Rejected;
    if (pending_) {
        if (request == pendingRequest_ && same(m, pendingCommand_))
            return SessionResult::Accepted;
        return result(request == pendingRequest_ ? Outcome::Conflict : Outcome::Busy,
                      request, now, out, isAudio, isEspNow, isRssi) ? SessionResult::Reply : SessionResult::Rejected;
    }
    if (nextRequest_ == std::numeric_limits<std::uint32_t>::max())
        return result(Outcome::Exhausted, request, now, out, isAudio, isEspNow, isRssi) ? SessionResult::Reply
                                                           : SessionResult::Rejected;
    if (request != nextRequest_)
        return result(Outcome::OutOfOrder, request, now, out, isAudio, isEspNow, isRssi) ? SessionResult::Reply
                                                            : SessionResult::Rejected;
    const auto audio = control_.audioSnapshot();
    const auto state = commandState(control_.snapshot(), audio, isAudio);
    Outcome rejection = Outcome::Applied;
    const auto remaining = read32(m.bytes.data() + 26) - now;
    if (isEspNow) {
        const auto esp = control_.espNowSnapshot();
        std::array<std::uint8_t, 6> peer{};
        std::copy_n(m.bytes.data() + 32, 6, peer.begin());
        std::uint8_t any = 0; for (auto b : peer) any |= b;
        Outcome reason = Outcome::Applied;
        if (!remaining || remaining > 3000U) reason = Outcome::Expired;
        else if (read32(m.bytes.data() + 22) != 0 || (m.bytes[30] != 1 && !(isRssi && m.bytes[30] == 2)) || m.bytes[31] != 1 || !any || (peer[0] & 1)) reason = Outcome::InvalidCommand;
        else if (!esp.supported || !esp.ready || (isRssi && !esp.rssiSupported)) reason = Outcome::Unavailable;
        else if (state.mode != Mode::LedOnly && state.mode != Mode::Stopped) reason = Outcome::GameActive;
        if (reason != Outcome::Applied) return finish(reason, m, now, out) ? SessionResult::Reply : SessionResult::Rejected;
        pendingEspNow_ = true; pendingAudio_ = false; pendingRequest_ = request;
        pendingCommand_ = m; pendingAt_ = now;
        if (!(isRssi ? control_.submitRssi(peer, now, m.bytes[30] == 2) : control_.submitEspNow(peer, now)))
            return finish(Outcome::Unavailable, m, now, out) ? SessionResult::Reply : SessionResult::Rejected;
        pending_ = true;
        return SessionResult::Accepted;
    }
    const auto operation = static_cast<Operation>(m.bytes[30]);
    const auto effect = static_cast<Effect>(m.bytes[31]);
    const auto track = isAudio ? read16(m.bytes.data() + 31) : 0;
    const auto volume = isAudio ? m.bytes[33] : 0;
    const bool commandValid = isAudio ?
        ((operation == Operation::Start && track >= 1 && track <= 255 && volume >= 1 && volume <= 10) ||
         ((operation == Operation::Stop || operation == Operation::Pause || operation == Operation::Resume) && track == 0 && volume == 0) ||
         (operation == Operation::Volume && track == 0 && volume >= 1 && volume <= 10)) :
        ((operation == Operation::Start && m.bytes[31] >= 1 && m.bytes[31] <= 4) ||
         (operation == Operation::Stop && effect == Effect::Off));
    if (!remaining || remaining > 3000U) rejection = Outcome::Expired;
    else if (!commandValid) rejection = Outcome::InvalidCommand;
    else if (!isAudio && effect == Effect::Resonance && !state.resonanceSupported)
        rejection = Outcome::Unavailable;
    else if (isAudio && (!audio.supported || !valid(audio))) rejection = Outcome::Unavailable;
    else if (!valid(state) || !(state.flags & Ready) || (state.flags & Fault))
        rejection = Outcome::Unavailable;
    else if (state.mode != Mode::LedOnly && state.mode != Mode::Stopped)
        rejection = Outcome::GameActive;
    else if (state.flags & Pending) rejection = Outcome::Busy;
    else if (read32(m.bytes.data() + 22) != state.revision) rejection = Outcome::StaleRevision;
    if (rejection != Outcome::Applied)
        return finish(rejection, m, now, out) ? SessionResult::Reply : SessionResult::Rejected;
    std::uint32_t target = 0;
    const bool submitted = isAudio ? control_.submitAudio(operation, track, volume, state.revision, target) :
                                    control_.submit(operation, effect, state.revision, target);
    if (!submitted ||
        target <= state.revision)
        return finish(Outcome::Unavailable, m, now, out) ? SessionResult::Reply
                                                        : SessionResult::Rejected;
    pending_ = true;
    pendingAudio_ = isAudio;
    pendingEspNow_ = false;
    pendingRequest_ = request;
    pendingRevision_ = target;
    pendingAt_ = now;
    pendingCommand_ = m;
    return SessionResult::Accepted;
}
bool Session::service(std::uint32_t now, Message &out) noexcept {
    out = {};
    if (connected() && !authenticated() && now - connectedAt_ >= 10000U) {
        disconnect(); return false;
    }
    if (!authenticated() || !pending_) return false;
    if (pendingEspNow_) {
        const auto esp = control_.espNowSnapshot();
        const auto mode = control_.snapshot().mode;
        if (mode != Mode::LedOnly && mode != Mode::Stopped) {
            control_.stopOnDisconnect();
            const auto command = pendingCommand_;
            return finish(Outcome::GameActive, command, now, out);
        }
        if (!esp.pending || now - pendingAt_ >= 2500U) {
            const auto command = pendingCommand_;
            const auto outcome = esp.pending ? 10U : esp.outcome;
            return finish(static_cast<Outcome>(outcome), command, now, out);
        }
        return false;
    }
    const auto audio = control_.audioSnapshot();
    const auto state = commandState(control_.snapshot(), audio, pendingAudio_);
    if (!valid(state) || (pendingAudio_ && (!audio.supported || !valid(audio))) || (state.flags & Fault)) {
        const auto command = pendingCommand_;
        return finish(Outcome::DriverFault, command, now, out);
    }
    if (state.mode != Mode::LedOnly && state.mode != Mode::Stopped) {
        control_.stopOnDisconnect();
        const auto command = pendingCommand_;
        return finish(Outcome::GameActive, command, now, out);
    }
    if (state.appliedRevision == pendingRevision_ && !(state.flags & Pending)) {
        const auto command = pendingCommand_;
        return finish(Outcome::Applied, command, now, out);
    }
    if (state.revision != pendingRevision_) {
        const auto command = pendingCommand_;
        return finish(Outcome::Superseded, command, now, out);
    }
    if (now - pendingAt_ >= 2000U) {
        control_.stopOnDisconnect();
        const auto command = pendingCommand_;
        return finish(Outcome::DriverFault, command, now, out);
    }
    return false;
}
bool Session::publishStatus(std::uint32_t now, Message &out) noexcept {
    return status(0x84, now, out);
}
bool Session::verifyNfc(const Message &m) noexcept {
    return authenticated() && m.size == 70 && m.bytes[0] == 2 && m.bytes[1] == 5 && control_.snapshot().nfcSupported &&
        equal(session_.data(), m.bytes.data() + 2, 16) && verify("LARP-NFC/2/command", m, 38);
}
bool Session::reserveNfc(std::uint32_t request) noexcept {
    if (!authenticated() || pending_ || nfcPending_ || request != nextRequest_ || nextRequest_ == UINT32_MAX)
        return false;
    nfcPending_ = true;
    return true;
}
bool Session::finishNfc(std::uint32_t request) noexcept {
    if (!authenticated() || !nfcPending_ || request != nextRequest_ || nextRequest_ == UINT32_MAX)
        return false;
    nfcPending_ = false;
    ++nextRequest_;
    // Only the latest completed request may replay on the common stream.
    lastRequest_ = 0; lastCommand_ = {}; lastReply_ = {};
    return true;
}
bool Session::sealNfc(Message &m) noexcept {
    if (!authenticated() || m.size != 105) return false;
    m.bytes[0] = 2; m.bytes[1] = 0x86;
    std::copy(session_.begin(), session_.end(), m.bytes.begin() + 2);
    Secret mac{};
    if (!tag("LARP-NFC/2/result", m.bytes.data(), 73, mac)) return false;
    std::copy(mac.begin(), mac.end(), m.bytes.begin() + 73);
    return true;
}
void Assembler::reset() noexcept {
    current_ = {};
    transfer_ = completed_ = received_ = previousOffset_ = previousSize_ = 0;
    started_ = 0;
}
FrameResult Assembler::accept(const std::uint8_t *bytes, std::size_t size,
                              std::uint32_t now, Message &out) noexcept {
    out.size = 0;
    if (!bytes || size <= 8 || size > 20 || bytes[0] != 1 || bytes[1] != 0x4c)
        return FrameResult::Rejected;
    const auto transfer = read16(bytes + 2), total = read16(bytes + 4), offset = read16(bytes + 6);
    const auto count = static_cast<std::uint16_t>(size - 8);
    if (!transfer || !total || total > 128 || offset + count > total)
        return FrameResult::Rejected;
    if (!transfer_) {
        if (transfer <= completed_ || offset) return FrameResult::Rejected;
        transfer_ = transfer; current_.size = total; received_ = 0; started_ = now;
    }
    if (now - started_ >= 2000U || transfer != transfer_ || total != current_.size)
        return FrameResult::Rejected;
    if (received_ && offset == previousOffset_ && count == previousSize_ &&
        equal(bytes + 8, current_.bytes.data() + offset, count)) return FrameResult::Duplicate;
    if (offset != received_) return FrameResult::Rejected;
    std::copy_n(bytes + 8, count, current_.bytes.begin() + offset);
    previousOffset_ = offset; previousSize_ = count;
    received_ = static_cast<std::uint16_t>(received_ + count);
    if (received_ != total) return FrameResult::Receiving;
    out = current_; completed_ = transfer_; transfer_ = 0;
    previousOffset_ = previousSize_ = received_ = 0;
    return FrameResult::Complete;
}
bool encodeFrame(const Message &message, std::uint16_t transfer, std::uint16_t offset,
                  std::array<std::uint8_t, 20> &bytes, std::size_t &written) noexcept {
    written = 0;
    if (!transfer || !message.size || message.size > message.bytes.size() || offset >= message.size)
        return false;
    bytes.fill(0);
    bytes[0] = 1; bytes[1] = 0x4c;
    write16(bytes.data() + 2, transfer); write16(bytes.data() + 4, message.size);
    write16(bytes.data() + 6, offset);
    const auto count = std::min<std::size_t>(12, message.size - offset);
    std::copy_n(message.bytes.data() + offset, count, bytes.begin() + 8);
    written = 8 + count;
    return true;
}
} // namespace ledwire
} // namespace larp
