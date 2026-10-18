static Message espCommand(Harness &h, std::uint32_t id = 1) {
    auto m = h.command(id); m.bytes[1] = 6;
    m.bytes[30] = 1; m.bytes[31] = 1;
    const std::uint8_t peer[] = {2,3,4,5,6,7};
    std::copy(peer, peer + 6, m.bytes.begin() + 32);
    sign(h.crypto, h.key, "espnow-command", m, 38);
    return m;
}
static void espNowCases() {
    using namespace larp::espnowtest;
    class Radio : public IRadio {
    public:
        bool accepts = true;
        unsigned sends = 0;
        Packet last{};
        bool send(const Peer &, const Packet &p) override { ++sends; last = p; return accepts; }
    } radio;
    Core core(radio); core.enable(true);
    Peer peer{{2,3,4,5,6,7}}, wrong{{2,3,4,5,6,8}};
    Token token{}; token.fill(9);
    check(core.start(peer, token, 100), "one deliberate send");
    check(radio.sends == 1 && validPacket(radio.last.data(), 56, false), "exact bounded request");
    check(!core.start(peer, token, 101), "single in-flight lane");
    auto ack = radio.last; ack[5] = 2; encode32(ack.data()+56, 77); encode32(ack.data()+60, 1);
    core.delivery(peer, true);
    check(core.snapshot().pending && !core.snapshot().received, "link ACK is not received application");
    core.receive(wrong, ack.data(), ack.size());
    auto bad = ack; bad[8] ^= 1; core.receive(peer, bad.data(), bad.size());
    bad = ack; bad[55] = 1; core.receive(peer, bad.data(), bad.size());
    core.receive(peer, ack.data(), 63);
    check(core.snapshot().pending, "wrong source, token, payload and length cannot confirm");
    core.receive(peer, ack.data(), ack.size());
    check(core.snapshot().received && core.snapshot().receiverCount == 1 && core.snapshot().outcome == 0,
          "only matching validated ACK confirms");
    token.fill(10); check(core.start(peer, token, UINT32_MAX - 1000), "start across clock wrap");
    core.receive(peer, ack.data(), ack.size());
    check(!core.snapshot().received, "old successful receipt cannot confirm fresh test");
    core.delivery(peer, true); core.service(998);
    check(core.snapshot().pending, "deadline exclusive 1999ms"); core.service(999);
    check(!core.snapshot().pending && core.snapshot().outcome == 13, "deadline at 2000ms across wrap");
    check(core.start(peer, token, 1000), "explicit retry after completed timeout");
    core.cancel(); check(!core.start(peer, token, 1001), "cancel keeps old callback lane occupied");
    core.delivery(peer, true); check(core.start(peer, token, 1002), "late callback releases old lane only");
    core.delivery(peer, false); check(core.snapshot().outcome == 12 && !core.snapshot().received, "link failure distinct");
    radio.accepts = false; check(core.start(peer, token, 2000), "local failure has terminal observation");
    check(!core.snapshot().accepted && core.snapshot().outcome == 10, "local rejection never delivery");
    core.overflow(); check(!core.start(peer, token, 2001), "queue overflow fails closed");
    {
        Harness h; h.auth(); Message out;
        h.control.esp.supported = h.control.esp.ready = true;
        check(h.session.publishStatus(1004,out) && out.bytes[38] == 6, "explicit supported/ready capability");
        auto command = espCommand(h);
        check(h.session.receive(command,1010,out) == SessionResult::Accepted && !out.size, "authenticated ESP command");
        check(h.control.espSubmissions == 1 && h.session.nextRequest() == 1, "common slot reserved until receipt");
        check(h.session.receive(command,1011,out) == SessionResult::Accepted && h.control.espSubmissions == 1, "pending duplicate never resends");
        check(h.session.receive(h.command(2),1012,out) == SessionResult::Reply && out.bytes[38] == 9, "LED blocked during ESP command");
        check(!h.session.reserveNfc(1), "NFC blocked during ESP command");
        h.control.esp.pending = false; h.control.esp.received = true; h.control.esp.link = 1;
        h.control.esp.outcome = 0; h.control.esp.receiverCount = 3; h.control.esp.receiverBoot = 77;
        check(h.session.service(1020,out) && out.size == 108 && out.bytes[1] == 0x87 && out.bytes[44] == 1, "signed reception result");
        auto expected = out; sign(h.crypto,h.key,"espnow-result",expected,76);
        check(same(out,expected) && h.session.nextRequest() == 2, "result MAC and shared progression");
        const auto cached = out;
        check(h.session.receive(command,1021,out) == SessionResult::Reply && same(out,cached) && h.control.espSubmissions == 1, "completed duplicate cached only");
        auto invalid = espCommand(h,2); invalid.bytes[32] = 1; sign(h.crypto,h.key,"espnow-command",invalid,38);
        check(h.session.receive(invalid,1022,out) == SessionResult::Reply && out.bytes[38] == 3 && !out.bytes[44], "new rejection never carries old success");
    }
    for (unsigned offset : {0U,1U,2U,18U,31U,32U,37U,69U}) {
        Harness h; h.auth(); h.control.esp.supported = h.control.esp.ready = true; Message out;
        auto command = espCommand(h); command.bytes[offset] ^= 1;
        check(h.session.receive(command,1010,out) == SessionResult::Rejected && !h.control.espSubmissions,
              "modified authenticated envelope never sends");
    }
    {
        Harness h; h.auth(); Message out;
        check(h.session.receive(espCommand(h),1010,out) == SessionResult::Reply && out.bytes[38] == 4 && !out.bytes[44], "unsupported cannot execute or claim receipt");
    }
}
