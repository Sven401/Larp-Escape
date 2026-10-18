#pragma once
#include "LedWire.h"
#include <algorithm>
#include <cstring>

namespace larp { namespace espnowtest {
using Peer = std::array<std::uint8_t, 6>;
using Token = std::array<std::uint8_t, 16>;
using Packet = std::array<std::uint8_t, 64>;
inline std::uint32_t decode32(const std::uint8_t *p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
inline void encode32(std::uint8_t *p, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8*i));
}
inline bool validPeer(const Peer &p) {
    std::uint8_t any = 0;
    for (auto b : p) any |= b;
    return any && !(p[0] & 1);
}
inline bool validToken(const Token &p) {
    return std::any_of(p.begin(), p.end(), [](std::uint8_t b) { return b != 0; });
}
inline Packet request(const Token &token) {
    Packet p{};
    const std::uint8_t header[] = {'L','E','N','T',1,1,32,0};
    std::copy(header, header + 8, p.begin());
    std::copy(token.begin(), token.end(), p.begin() + 8);
    std::memcpy(p.data() + 24, "ACTIVATE", 8);
    return p;
}
inline bool validPacket(const std::uint8_t *p, std::size_t n, bool ack) {
    if (!p || n != (ack ? 64U : 56U)) return false;
    Token t{}; std::copy(p + 8, p + 24, t.begin());
    auto expected = request(t); expected[5] = ack ? 2 : 1;
    return validToken(t) && std::memcmp(p, expected.data(), 56) == 0;
}
class IRadio {
public:
    virtual ~IRadio() = default;
    virtual bool send(const Peer &, const Packet &) = 0;
};
// Only the application owner calls Core. Callbacks must copy into a bounded queue.
class Core {
    IRadio &radio_;
    ledwire::EspNowSnapshot state_{};
    bool enabled_ = false, flight_ = false, fault_ = false, needRssi_ = false;
    std::uint32_t started_ = 0;
public:
    explicit Core(IRadio &r) : radio_(r) { state_.supported = true; }
    void enable(bool ok) { enabled_ = ok; }
    ledwire::EspNowSnapshot snapshot() const {
        auto out = state_; out.ready = enabled_ && !fault_ && !flight_; return out;
    }
    bool start(const Peer &peer, const Token &token, std::uint32_t now, bool needRssi = false) {
        if (!snapshot().ready || state_.pending || !validPeer(peer) || !validToken(token)) return false;
        needRssi_ = needRssi;
        state_ = {}; state_.supported = true; state_.pending = true;
        state_.peer = peer; state_.token = token; started_ = now;
        flight_ = true;
        state_.accepted = radio_.send(peer, request(token));
        if (!state_.accepted) {
            flight_ = false; state_.pending = false; state_.outcome = 10;
        }
        return true; // Request was handled, including an observed local send failure.
    }
    void delivery(const Peer &peer, bool ok) {
        if (!flight_ || peer != state_.peer) return;
        flight_ = false;
        if (!state_.pending) return;
        state_.link = ok ? 1 : 2;
        if (!ok) { state_.pending = false; state_.outcome = 12; }
    }
    void receive(const Peer &peer, const std::uint8_t *bytes, std::size_t size, int rssi = 0) {
        if ((needRssi_ && (rssi < -127 || rssi >= 0)) || !state_.pending || peer != state_.peer || !validPacket(bytes, size, true) ||
            std::memcmp(bytes + 8, state_.token.data(), 16) != 0) return;
        const auto count = decode32(bytes + 60);
        if (!count) return;
        state_.rssiValid = needRssi_; state_.rssi = static_cast<std::int8_t>(needRssi_ ? rssi : 0);
        state_.receiverBoot = decode32(bytes + 56);
        state_.receiverCount = count; state_.received = true;
        state_.pending = false; state_.outcome = 0;
    }
    void service(std::uint32_t now) {
        if (state_.pending && now - started_ >= 2000U) {
            state_.pending = false; state_.outcome = 13;
            // Do not reuse a lane with an uncorrelatable outstanding send callback.
        }
    }
    void cancel() {
        state_.pending = false; state_.received = false; state_.accepted = false;
        state_.rssiValid = false; state_.rssi = state_.filteredRssi = 0; state_.intensity = 0;
        state_.link = 0; state_.token.fill(0); state_.receiverCount = state_.receiverBoot = 0;
        // Keep peer and flight until the old callback has been drained.
    }
    void overflow() {
        fault_ = true; state_.pending = false; state_.outcome = 10;
        state_.rssiValid = false; state_.rssi = state_.filteredRssi = 0; state_.intensity = 0;
        state_.received = false; state_.receiverCount = state_.receiverBoot = 0;
    }
};
}} // namespace
