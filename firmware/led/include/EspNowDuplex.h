#pragma once
#include "EspNowTest.h"

namespace larp { namespace espnowtest {
// Loop-owned serialization: callbacks only enqueue events. A lane remains held
// until its callback is drained; receipt callbacks never confirm a local probe.
class TxLane {
public:
    enum class Kind { None, Request, Receipt };
    bool begin(Kind kind, const Peer &peer) {
        if (kind == Kind::None || kind_ != Kind::None || !validPeer(peer)) return false;
        kind_ = kind; peer_ = peer; return true;
    }
    bool idle() const { return kind_ == Kind::None; }
    Kind complete(const Peer &peer) {
        if (peer != peer_) return Kind::None;
        const auto kind = kind_; kind_ = Kind::None; return kind;
    }
private:
    Kind kind_ = Kind::None;
    Peer peer_{};
};

class Responder {
    Peer peer_{};
    Packet packet_{};
    std::uint32_t boot_ = 0, count_ = 0, queuedAt_ = 0;
    bool pending_ = false;
public:
    explicit Responder(std::uint32_t boot = 0) : boot_(boot) {}
    void boot(std::uint32_t value) { boot_ = value; }
    bool offer(const Peer &peer, const std::uint8_t *bytes, std::size_t size, std::uint32_t now) {
        expire(now);
        if (pending_ || count_ == UINT32_MAX || !validPeer(peer) || !validPacket(bytes, size, false)) return false;
        peer_ = peer; std::copy(bytes, bytes + size, packet_.begin()); packet_[5] = 2;
        encode32(packet_.data() + 56, boot_); encode32(packet_.data() + 60, ++count_);
        pending_ = true; queuedAt_ = now; return true;
    }
    bool pending(std::uint32_t now) { expire(now); return pending_; }
    const Peer &peer() const { return peer_; }
    const Packet &packet() const { return packet_; }
    void sent() { pending_ = false; }
private:
    void expire(std::uint32_t now) { if (pending_ && now - queuedAt_ >= 2000U) pending_ = false; }
};
}}
