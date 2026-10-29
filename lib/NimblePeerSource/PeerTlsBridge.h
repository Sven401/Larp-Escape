#pragma once
#include "NimblePeerSource.h"
namespace larp { namespace companion {
// Host produces ingress, one crypto task consumes it; the reverse for egress.
// Credentials must outlive this process-lifetime bridge. No role is granted.
class PeerTlsBridge final : public IPeerCipherIngress {
public:
    PeerTlsBridge(TlsPem certificate, TlsPem key, TlsPem peer) noexcept
        : certificate_(certificate), key_(key), peer_(peer) {}
    bool open(const TlsOrigin &origin) noexcept override {
        Item item{}; item.open = true; item.fragment.origin = origin;
        return input_.put(item);
    }
    bool push(const PeerCipherFragment &fragment) noexcept override {
        if (!fragment.count || fragment.count > fragment.bytes.size()) return false;
        Item item{}; item.fragment = fragment; return input_.put(item);
    }
    // Pairing approval belongs to a trusted physical UI, never to TLS success.
    void compare(const TlsOrigin &, std::uint32_t) noexcept override {}
    bool poll(NimblePeerSource &source) noexcept override {
        Item item{};
        if (!output_.peek(item)) return true;
        const auto result = source.notify(item.fragment);
        if (result == PeerNotify::Busy) return true; // retain identical bytes
        output_.pop();
        return result != PeerNotify::Failed;
    }
    // Exactly one crypto task owns tls and calls step/read/send. No callback crypto.
    void step(NimblePeerSource &source, PinnedPeerTls &tls, std::uint32_t now) noexcept {
        // Retire a disconnected session before a reconnect open can reach begin().
        // A queued stale open itself must never close a newer TLS context.
        tls.service(now);
        Item item{};
        if (input_.peek(item)) {
            input_.pop();
            const auto &origin = item.fragment.origin;
            if (source.current(origin)) {
                bool ok = false;
                if (item.open) {
                    ok = tls.begin(origin, certificate_, key_, peer_, now);
                    if (ok) active_ = origin;
                } else if (origin == active_) {
                    ok = tls.feed(origin, item.fragment.bytes.data(), item.fragment.count, now);
                }
                if (!ok) source.revoke(origin);
            }
        }
        const auto state = tls.service(now);
        if (state == TlsProgress::Failed) source.revoke(active_);
        if (!source.current(active_) || output_.full()) return;
        Item outgoing{}; outgoing.fragment.origin = active_;
        outgoing.fragment.count = static_cast<std::uint16_t>(tls.drain(active_,
            outgoing.fragment.bytes.data(), PeerCipherFragment::NotifyBytes, now));
        if (outgoing.fragment.count && !output_.put(outgoing)) source.revoke(active_);
    }
private:
    struct Item { bool open = false; PeerCipherFragment fragment{}; };
    class Mailbox {
    public:
        bool put(const Item &item) noexcept {
            portENTER_CRITICAL(&lock_);
            const bool room = count_ < items_.size();
            if (room) { items_[(head_ + count_) % items_.size()] = item; ++count_; }
            portEXIT_CRITICAL(&lock_); return room;
        }
        bool peek(Item &item) noexcept {
            portENTER_CRITICAL(&lock_);
            const bool present = count_ != 0;
            if (present) item = items_[head_];
            portEXIT_CRITICAL(&lock_); return present;
        }
        void pop() noexcept {
            portENTER_CRITICAL(&lock_);
            if (count_) { items_[head_] = Item{}; head_ = (head_ + 1) % items_.size(); --count_; }
            portEXIT_CRITICAL(&lock_);
        }
        bool full() noexcept {
            portENTER_CRITICAL(&lock_); const bool result = count_ == items_.size();
            portEXIT_CRITICAL(&lock_); return result;
        }
    private:
        portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
        std::array<Item, 16> items_{};
        std::size_t head_ = 0, count_ = 0;
    } input_, output_;
    TlsPem certificate_, key_, peer_;
    TlsOrigin active_{};
};
} }
