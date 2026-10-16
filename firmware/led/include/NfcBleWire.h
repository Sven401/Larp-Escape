#pragma once
#include "LedWire.h"

namespace larp { namespace nfcble {
struct Reply {
    // 0..8 = nfcprobe::Result; 9 unavailable, 10 busy, 11 expired,
    // 12 conflicting duplicate, 13 out of order/exhausted.
    std::uint8_t outcome = 9, kind = 0, uidSize = 0, payloadSize = 0;
    std::uint8_t uid[10] = {}, payload[25] = {};
};
class Port {
 public:
    virtual ~Port() {}
    virtual bool submit(bool change, const std::uint8_t *uid, std::uint8_t size) = 0;
    virtual bool poll(Reply &) = 0;
    virtual void cancel() = 0;
};
// Owned by the foreground protocol loop, never by a BLE callback or NFC worker.
class Session {
 public:
    Session(ledwire::Session &auth, Port &port) : auth_(auth), port_(port) {}
    void reset();
    bool receive(const ledwire::Message &, std::uint32_t now, ledwire::Message &reply);
    bool service(std::uint32_t now, ledwire::Message &reply);
    bool busy() const { return pending_; }
 private:
    bool result(const Reply &, std::uint32_t request, std::uint32_t now, ledwire::Message &);
    ledwire::Session &auth_;
    Port &port_;
    std::uint32_t next_ = 1, sequence_ = 0, request_ = 0, last_ = 0;
    bool pending_ = false;
    ledwire::Message command_, previous_, cached_;
};
} }
