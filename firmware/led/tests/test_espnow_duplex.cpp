#include "EspNowDuplex.h"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace larp::espnowtest;
int main() {
    const Peer a{{2,1,2,3,4,5}}, b{{2,1,2,3,4,6}};
    TxLane lane;
    assert(lane.idle());
    assert(lane.begin(TxLane::Kind::Request, a));
    assert(!lane.begin(TxLane::Kind::Receipt, a));
    assert(lane.complete(b) == TxLane::Kind::None && !lane.idle());
    assert(lane.complete(a) == TxLane::Kind::Request && lane.idle());
    assert(lane.complete(a) == TxLane::Kind::None); // duplicate drained callback
    assert(lane.begin(TxLane::Kind::Receipt, a));
    assert(!lane.begin(TxLane::Kind::Request, a)); // no callback reclassification
    assert(lane.complete(a) == TxLane::Kind::Receipt && lane.idle());
    Peer invalid{}; assert(!lane.begin(TxLane::Kind::Request, invalid));

    Responder response(0x12345678);
    Token token{}; token[0] = 42;
    const auto packet = request(token);
    assert(!response.offer(a, packet.data(), 55, 10));
    auto malformed = packet; malformed[24] ^= 1;
    assert(!response.offer(a, malformed.data(), 56, 10));
    assert(!response.offer(invalid, packet.data(), 56, 10));
    assert(response.offer(a, packet.data(), 56, 100));
    assert(response.peer() == a && response.pending(101));
    assert(validPacket(response.packet().data(), 64, true));
    assert(std::equal(token.begin(), token.end(), response.packet().begin() + 8));
    assert(decode32(response.packet().data()+56) == 0x12345678);
    assert(decode32(response.packet().data()+60) == 1);
    assert(!response.offer(b, packet.data(), 56, 102)); // one bounded pending slot
    assert(response.peer() == a && response.pending(2099));
    assert(!response.pending(2100)); // old request never starts a late reply
    const auto wrap = std::numeric_limits<std::uint32_t>::max() - 100U;
    assert(response.offer(b, packet.data(), 56, wrap));
    assert(response.pending(10));
    assert(!response.pending(2000));
    assert(response.offer(b, packet.data(), 56, 3000));
    assert(decode32(response.packet().data()+60) == 3);
    response.sent(); assert(!response.pending(3001));
    // Integrate the owner lane with the existing request core: receipt delivery
    // must not clear an outstanding local request or set its link outcome.
    struct Radio : IRadio { bool send(const Peer &, const Packet &) override { return true; } } radio;
    Core core(radio); core.enable(true); assert(core.start(a, token, 0));
    assert(lane.begin(TxLane::Kind::Receipt, a));
    const auto kind = lane.complete(a);
    if (kind == TxLane::Kind::Request) core.delivery(a, true);
    assert(core.snapshot().pending && core.snapshot().link == 0 && !core.snapshot().ready);
    assert(lane.begin(TxLane::Kind::Request, a));
    if (lane.complete(a) == TxLane::Kind::Request) core.delivery(a, true);
    assert(core.snapshot().pending && core.snapshot().link == 1);
    core.cancel(); assert(!core.snapshot().pending);
    // Cancellation cannot release a physical send whose callback is outstanding.
    Core canceled(radio); canceled.enable(true); assert(canceled.start(a, token, 10));
    assert(lane.begin(TxLane::Kind::Request, a));
    canceled.cancel();
    assert(!lane.idle() && !canceled.snapshot().ready && !canceled.snapshot().received);
    assert(!lane.begin(TxLane::Kind::Receipt, a));
    assert(!canceled.start(a, token, 11));
    if (lane.complete(a) == TxLane::Kind::Request) canceled.delivery(a, true);
    assert(lane.idle() && canceled.snapshot().ready && !canceled.snapshot().received && canceled.snapshot().link == 0);
    assert(lane.begin(TxLane::Kind::Receipt, a));
    assert(lane.complete(a) == TxLane::Kind::Receipt);
    assert(canceled.start(a, token, 12));
    assert(lane.begin(TxLane::Kind::Request, a));
    canceled.overflow(); // lost callback/queue overflow stays faulted even if a late TX arrives
    assert(!canceled.snapshot().pending && !canceled.snapshot().ready && !canceled.snapshot().received);
    assert(!canceled.start(a, token, 13) && !lane.begin(TxLane::Kind::Receipt, a));
    if (lane.complete(a) == TxLane::Kind::Request) canceled.delivery(a, true);
    assert(!canceled.snapshot().ready && !canceled.snapshot().received && canceled.snapshot().link == 0);
    assert(!canceled.start(a, token, 14));
    std::puts("ESP-NOW duplex: serialized callbacks, bounded responder, malformed frames and clock wrap passed");
}
