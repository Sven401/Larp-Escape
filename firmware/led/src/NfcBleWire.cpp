#include "NfcBleWire.h"
#include <cstring>

namespace larp { namespace nfcble {
void Session::reset() {
    port_.cancel(); next_ = 1; sequence_ = request_ = last_ = 0;
    pending_ = false; command_ = {}; previous_ = {}; cached_ = {};
}
bool Session::result(const Reply &r, std::uint32_t request, std::uint32_t now, ledwire::Message &out) {
    if (sequence_ == UINT32_MAX || r.uidSize > 10 || (r.payloadSize != 0 && r.payloadSize != 25)) return false;
    out = {}; out.size = 105;
    ledwire::write32(out.bytes.data() + 18, request);
    ledwire::write32(out.bytes.data() + 22, next_);
    ledwire::write32(out.bytes.data() + 26, ++sequence_);
    ledwire::write32(out.bytes.data() + 30, now);
    out.bytes[34] = r.outcome; out.bytes[35] = r.kind; out.bytes[36] = r.uidSize;
    std::memcpy(out.bytes.data() + 37, r.uid, r.uidSize);
    out.bytes[47] = r.payloadSize;
    std::memcpy(out.bytes.data() + 48, r.payload, r.payloadSize);
    return auth_.sealNfc(out);
}
bool Session::receive(const ledwire::Message &m, std::uint32_t now, ledwire::Message &out) {
    out = {};
    if (!auth_.verifyNfc(m)) return false;
    const auto id = ledwire::read32(m.bytes.data() + 18);
    const auto same = [](const ledwire::Message &a, const ledwire::Message &b) {
        return a.size == b.size && std::memcmp(a.bytes.data(), b.bytes.data(), a.size) == 0;
    };
    if (!id) return false;
    Reply rejection;
    if (id == last_) {
        if (same(m, previous_)) { out = cached_; return true; }
        rejection.outcome = 12; return result(rejection, id, now, out);
    }
    if (pending_) {
        if (id == request_ && same(m, command_)) return true;
        rejection.outcome = id == request_ ? 12 : 10;
        return result(rejection, id, now, out);
    }
    if (id != next_ || next_ == UINT32_MAX) {
        rejection.outcome = 13; return result(rejection, id, now, out);
    }
    const auto remaining = ledwire::read32(m.bytes.data() + 22) - now;
    const auto op = m.bytes[26], size = m.bytes[27];
    if ((op != 1 && op != 2) || (op == 1 && size != 0) || (op == 2 && size != 7)) return false;
    for (unsigned i = size; i < 10; ++i) if (m.bytes[28 + i]) return false;
    if (!remaining || remaining > 3000) rejection.outcome = 11;
    else if (port_.submit(op == 2, m.bytes.data() + 28, size)) {
        command_ = m; request_ = id; pending_ = true; return true;
    }
    ++next_; last_ = id; previous_ = m;
    if (!result(rejection, id, now, out)) return false;
    cached_ = out; return true;
}
bool Session::service(std::uint32_t now, ledwire::Message &out) {
    out = {};
    Reply r;
    if (!pending_) { port_.poll(r); return true; }
    if (!port_.poll(r)) return true;
    pending_ = false; ++next_; last_ = request_; previous_ = command_;
    if (!result(r, request_, now, out)) return false;
    cached_ = out; return true;
}
} }
