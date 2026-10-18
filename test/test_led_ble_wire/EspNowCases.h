#include "RssiMeasurement.h"
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
    {
        Core measurement(radio); measurement.enable(true);
        Peer target{{2,3,4,5,6,7}}; Token id{}; id.fill(3);
        check(measurement.start(target,id,100,true), "RSSI probe admitted");
        auto reply=request(id); reply[5]=2; encode32(reply.data()+60,1);
        measurement.delivery(target,true);
        measurement.receive(target,reply.data(),64);
        check(measurement.snapshot().pending, "ordinary callback cannot invent RSSI");
        auto wrong=target; wrong[5]++;
        measurement.receive(wrong,reply.data(),64,-60);
        auto stale=reply; stale[8]++;
        measurement.receive(target,stale.data(),64,-60);
        check(measurement.snapshot().pending, "RSSI bound to exact source and nonce");
        measurement.receive(target,reply.data(),64,-60);
        check(measurement.snapshot().rssiValid && measurement.snapshot().rssi == -60, "matching sample accepted");
        larp::rssi::Filter f;
        check(!f.valid(0) && f.target(0)==20, "unknown has conservative intensity");
        f.sample(-90,UINT32_MAX-1000); check(f.target(UINT32_MAX-1000)==20, "weak lower endpoint");
        f.sample(-40,UINT32_MAX-999); check(f.value() < -70 && f.target(0)<60, "smoothing limits jump");
        check(f.valid(1998) && !f.valid(2000), "RSSI expiry handles wrap");
        f.sample(-20,2001); check(f.target(2001)==100, "strong upper clamp");
        f.clear(); f.sample(-120,2002); check(f.target(2002)==20, "weak clamp after reset");
        std::array<std::uint8_t,107> frame{};
        frame[0]=0xd0; frame[24]=127; frame[25]=frame[34]=0x18;
        frame[26]=frame[35]=0xfe; frame[27]=frame[36]=0x34;
        frame[32]=221; frame[33]=69; frame[37]=4; frame[38]=1;
        std::copy(target.begin(),target.end(),frame.begin()+10);
        std::copy(target.begin(),target.end(),frame.begin()+4);
        std::copy(reply.begin(),reply.end(),frame.begin()+39);
        larp::rssi::Observation observation;
        check(larp::rssi::decode(frame.data(),107,-60,observation) && observation.source==target && observation.packet==reply,
              "bounded vendor action parser associates payload and source");
        for (unsigned n=0;n<107;++n) check(!larp::rssi::decode(frame.data(),n,-60,observation),"truncated frames rejected");
        for (unsigned at : {0U,24U,25U,26U,27U,32U,33U,34U,35U,36U,37U,38U,39U,94U}) {
            auto bad=frame; bad[at]^=1; check(!larp::rssi::decode(bad.data(),107,-60,observation),"wrong protocol frame rejected");
        }
        check(!larp::rssi::decode(frame.data(),107,0,observation),"absent RSSI rejected");
    }
    radio.sends=0;
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
        h.control.esp.supported = h.control.esp.ready = h.control.esp.rssiSupported = true;
        check(h.session.publishStatus(1004,out) && out.bytes[38]==14,"RSSI capability explicit");
        auto command=espCommand(h); command.bytes[1]=7; command.bytes[30]=2; sign(h.crypto,h.key,"rssi-command",command,38);
        check(h.session.receive(command,1010,out)==SessionResult::Accepted,"RSSI has authenticated common slot");
        h.control.esp.pending=false; h.control.esp.received=true; h.control.esp.rssiValid=true;
        h.control.esp.rssi=-63; h.control.esp.filteredRssi=-61; h.control.esp.intensity=66;
        h.control.esp.outcome=0; h.control.esp.receiverCount=1;
        check(h.session.service(1020,out) && out.size==112 && out.bytes[1]==0x88 && out.bytes[76]==1 &&
            out.bytes[77]==static_cast<std::uint8_t>(-63) && out.bytes[79]==66,"RSSI signed result metadata");
        auto signedResult=out; sign(h.crypto,h.key,"rssi-result",signedResult,80);
        check(same(out,signedResult),"RSSI uses separate HMAC domain");
        const auto cached=out;
        check(h.session.receive(command,1021,out)==SessionResult::Reply && same(out,cached) && h.control.espSubmissions==1,
              "RSSI duplicate never repeats measurement or LED action");
    }
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
