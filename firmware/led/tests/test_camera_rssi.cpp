#include <Arduino.h>
#include "LedOutput.h"
#include "EspNowRadio.h"
#include "EspNowTest.h"
#include "RssiMeasurement.h"
#include <vector>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <limits>
std::uint32_t testNow = 0;
TestSerial Serial;
using namespace larp::led;
namespace {
void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
struct FakeBus final : LedBus {
    bool busy = false, stuck = false;
    std::uint32_t submitted = 0;
    std::vector<Frame> frames;
    bool begin() override { return true; }
    bool submit(const Frame &frame) override {
        check(!busy, "LED transfers overlap"); busy = true; submitted = testNow;
        frames.push_back(frame); return true;
    }
    TxResult poll() override {
        check(busy, "LED poll without transfer");
        if (stuck || testNow - submitted < 3) return TxResult::Busy;
        busy = false; return TxResult::Complete;
    }
    bool reset() override { busy = stuck = false; return true; }
} fake;
}
namespace rf {
using namespace larp::espnowtest;
struct Wire final : IRadio {
    bool accept = true;
    unsigned sends = 0;
    Peer peer{}; Packet packet{};
    bool send(const Peer &p, const Packet &bytes) override {
        ++sends; peer = p; packet = bytes; return accept;
    }
} wire;
Core core(wire);
larp::rssi::Filter filter;
Token sampled{};
Peer selected{};
struct Event {
    unsigned kind; // 0 reply, 1 delivery success, 2 delivery failure, 3 queue overflow
    Peer peer; Packet packet; int raw;
};
std::vector<Event> queue;
std::vector<std::uint32_t> serviced;
std::vector<std::string> cancelOutput;
unsigned nonceNumber = 0, beginCount = 0, advanceClock = 0;
void reply(int raw = -70, unsigned count = 1) {
    auto ack = wire.packet; ack[5] = 2;
    encode32(ack.data()+56, 12345); encode32(ack.data()+60, count);
    queue.push_back({0, wire.peer, ack, raw});
}
void delivery(bool ok = true) { queue.push_back({ok ? 1U : 2U, wire.peer, {}, 0}); }
}
namespace larp { namespace espnowradio {
bool begin() { ++rf::beginCount; rf::core.enable(true); return true; }
void service(std::uint32_t now) {
    rf::serviced.push_back(now); rf::core.service(now);
    for (const auto &e : rf::queue) {
        if (e.kind == 0) rf::core.receive(e.peer, e.packet.data(), e.packet.size(), e.raw);
        else if (e.kind == 3) rf::core.overflow();
        else rf::core.delivery(e.peer, e.kind == 1);
    }
    rf::queue.clear();
    const auto state = rf::core.snapshot();
    if (state.received && state.rssiValid && state.token != rf::sampled) {
        rf::sampled = state.token; rf::filter.sample(state.rssi, now);
    }
    testNow += rf::advanceClock; rf::advanceClock = 0;
}
ledwire::EspNowSnapshot snapshot() {
    auto s = rf::core.snapshot(); s.rssiSupported = true;
    if (s.rssiValid) s.filteredRssi = static_cast<std::int8_t>(rf::filter.value());
    return s;
}
bool submit(const std::array<std::uint8_t,6> &peer, std::uint32_t now, bool rssi, bool intensity) {
    check(rssi && !intensity, "lab enabled old intensity path");
    rf::Token nonce{}; rf::encode32(nonce.data(), ++rf::nonceNumber);
    if (peer != rf::selected) { rf::filter.clear(); rf::sampled.fill(0); }
    if (!rf::core.start(peer, nonce, now, rssi)) return false;
    rf::selected = peer; return true;
}
void cancel() {
    rf::cancelOutput.push_back(Serial.output);
    rf::core.cancel(); rf::filter.clear(); rf::sampled.fill(0);
}
}}
namespace larp { namespace led { LedBus &cameraLabBus() { return fake; }}}
#include "../src/CameraLabMain.cpp"
namespace {
const std::string session = "0123456789abcdef", other = "fedcba9876543210", destination = "a842e3ceded8";
void ticks(unsigned n) { for (unsigned i = 0; i < n; ++i) loop(); }
void send(const std::string &text) { Serial.send(text); do { loop(); } while (Serial.available()); }
void request(const std::string &id, const std::string &peer = destination, const std::string &idToken = session) {
    send("RSSI " + idToken + " " + id + " " + peer);
}
std::vector<std::vector<std::string>> records(const std::string &verb) {
    std::vector<std::vector<std::string>> rows;
    std::istringstream lines(Serial.output); std::string line;
    while (std::getline(lines, line)) {
        std::istringstream parts(line); std::vector<std::string> fields; std::string word;
        while (parts >> word) fields.push_back(word);
        if (!fields.empty() && fields[0] == verb) rows.push_back(fields);
    }
    return rows;
}
void reject(const std::string &id, const std::string &reason, const std::string &idToken = session) {
    const auto rows = records("MEASURE_REJECT"); check(!rows.empty(), "rejection missing");
    const auto &r = rows.back();
    check(r.size()==5 && r[1]==idToken && r[2]==id && r[3]==reason, "wrong rejection");
}
std::vector<std::string> result(unsigned count, unsigned outcome) {
    const auto rows = records("MEASURE"); check(rows.size()==count, "terminal result count wrong");
    const auto &r = rows.back();
    check(r.size()==15 && r[4]==std::to_string(outcome), "wrong terminal grammar/outcome"); return r;
}
void liveBegin() { send("LIVE " + session); check(liveMode && running, "LIVE admission failed"); }
void keepTicks(unsigned n) {
    for (unsigned i=0;i<n;++i) { if(i%400==0) Serial.send("KEEP " + session); loop(); }
}
bool black() {
    for (const auto &p : fake.frames.back()) if(p.red || p.green || p.blue) return false;
    return true;
}
}
int main(int argc, char **argv) {
    try {
        check(argc==2,"need case"); const std::string name=argv[1];
        setup(); ticks(10); send("HELLO");
        check(rf::beginCount==1 && Serial.output.find("LARP-RSSI/1 1 2000 3000\n")!=std::string::npos,"capability/init wrong");
        check(rf::serviced.size()==11,"idle loop omitted radio service");
        if(name=="admission") { request("1"); reject("1","inactive"); }
        liveBegin();
        if(name=="success") {
            const auto admitted=testNow; request("1");
            check(records("MEASURING").back()==std::vector<std::string>({"MEASURING",session,"1",std::to_string(admitted)}),"admission timestamp wrong");
            rf::reply(); const auto received=testNow; rf::advanceClock=7; loop();
            auto r=result(1,0);
            check(r[1]==session && r[2]=="1" && r[3]==std::to_string(received) && rf::serviced.back()==received,"result did not retain radio service time");
            check(r[5]=="1" && r[6]=="0" && r[7]=="1" && r[8]=="1" && r[9]=="-70" && r[10]=="-70" && r[11]=="12345" && r[12]=="1","RSSI observation wrong");
            check(r[13]=="01000000000000000000000000000000" && r[14]==destination,"nonce/peer wrong");
            request("2"); reject("2","radio-unavailable"); // Response may precede TX callback.
            rf::delivery(); loop(); request("2");
            rf::delivery(); rf::reply(-50,2); loop(); r=result(2,0);
            check(r[6]=="1" && r[9]=="-50" && r[10]=="-65","existing 1/4 filter not preserved");
            ticks(10); result(2,0);
        } else if(name=="admission") {
            request("1",destination,other); reject("1","token",other);
            request("0"); reject("0","sample-id");
            request("4294967296"); reject("0","sample-id");
            request("-1"); reject("0","sample-id");
            request("+1"); reject("0","sample-id");
            request("1","000000000000"); reject("1","peer");
            request("1","a942e3ceded8"); reject("1","peer");
            request("1","a842e3cededg"); reject("1","peer");
            send("RSSI "+session+" 1 "+destination+" extra"); reject("1","syntax");
            check(rf::wire.sends==0,"rejected request transmitted");
            request("1"); request("2"); reject("2","busy");
            request("1"); reject("1","stale");
            check(rf::wire.sends==1,"busy/repeated request resent");
            rf::delivery(); rf::reply(); loop(); result(1,0);
            request("4294967295","A842E3CEDED8");
            rf::delivery(); rf::reply(); loop(); result(2,0);
            request("4294967295"); reject("4294967295","stale");
            request("1"); reject("1","stale");
            check(rf::wire.sends==2,"UINT32 boundary was not strict");
        } else if(name=="stop" || name=="watchdog" || name=="bus-fault") {
            request("1"); const auto nonce=rf::core.snapshot().token;
            if(name=="watchdog") ticks(1600);
            else if(name=="bus-fault") {
                while (!bus.inFlight) { loop(); }
                fake.stuck=true; keepTicks(2050);
            } else send("STOP");
            auto r=result(1,14);
            check(r[5]=="1" && r[7]=="0" && r[8]=="0" && r[9]=="0" && r[10]=="0" && r[11]=="0" && r[12]=="0","cancel retained measurement");
            check(r[13]=="01000000000000000000000000000000" && r[14]==destination,"cancel lost request identity");
            check(!rf::cancelOutput.empty() && rf::cancelOutput.back().find("MEASURE "+session+" 1 ")!=std::string::npos,"radio cancelled before terminal output");
            check(!rf::core.snapshot().ready && rf::core.snapshot().token!=nonce,"cancel cleared send fence or retained nonce");
            const auto services=rf::serviced.size(); ticks(10); check(rf::serviced.size()==services+10,"stop/idle omitted service");
            check(!running && !stopPending && black(),"cancel did not complete LED black stop");
            rf::reply(-40); rf::delivery(); ticks(3); result(1,14);
            check(rf::core.snapshot().ready,"late send callback not drained while idle");
            send("LIVE "+other); request("1",destination,other);
            check(rf::wire.sends==2,"new LIVE did not reset sample namespace");
            // Old nonce/peer response cannot complete the new request.
            auto old=larp::espnowtest::request(nonce); old[5]=2;
            larp::espnowtest::encode32(old.data()+60,3);
            rf::queue.push_back({0,rf::wire.peer,old,-30}); loop(); result(1,14);
            rf::delivery(); rf::reply(-60,4); loop(); r=result(2,0);
            check(r[1]==other && r[2]=="1" && r[10]=="-60","new session identity/filter wrong");
        } else if(name=="timeout") {
            request("1"); const auto admitted=std::stoul(records("MEASURING").back()[3]);
            keepTicks(2000); auto r=result(1,13);
            check(std::stoul(r[3])-admitted==2000,"timeout not at established 2000 ms deadline");
            request("2"); reject("2","radio-unavailable");
            rf::reply(); rf::delivery(); loop(); result(1,13);
            request("2"); check(rf::wire.sends==2,"late callback did not release lane");
        } else if(name=="overflow") {
            request("1"); rf::queue.push_back({3,{},{},0}); loop(); result(1,10);
            request("2"); reject("2","radio-unavailable");
            rf::reply(); rf::delivery(); ticks(5); result(1,10);
            check(rf::wire.sends==1,"queue overflow resumed transmitting");
        } else if(name=="send-failure") {
            rf::wire.accept=false; request("1");
            check(records("MEASURING").size()==1,"local send failure lost admission");
            loop(); auto r=result(1,10); check(r[5]=="0" && r[7]=="0","local send failure looked accepted");
            request("1"); reject("1","stale");
            rf::wire.accept=true; request("2"); rf::delivery(false); loop(); r=result(2,12);
            check(r[5]=="1" && r[6]=="2" && r[8]=="0","delivery failure wrong");
        } else if(name=="independent") {
            send("GLOW "+session+" 1 0 1 start 1000 1 24 12 2 1000 1000 0");
            const auto revision=live.scene().revision(); request("1"); rf::delivery(); rf::reply(); loop();
            check(live.scene().revision()==revision,"measurement changed LED scene");
            send("GLOW "+session+" 2 0 1 update 500 1 24 12 2 1000 1000 0");
            check(live.scene().revision()==revision+1,"RSSI consumed GLOW sequence");
            const auto v=live.scene().revision(); request("2");
            check(live.scene().revision()==v,"pending measurement changed scene");
            keepTicks(1100); check(fake.frames.back()[0].red==12,"effect loop blocked by pending radio");
            send("STOP"); ticks(10); result(2,14);
        } else if(name=="wrap") {
            testNow=std::numeric_limits<std::uint32_t>::max()-1000;
            lastKeep=started=testNow;
            request("1"); rf::delivery(); loop(); keepTicks(2000); auto r=result(1,13);
            check(std::stoul(r[3])==999,"radio timeout wrap arithmetic wrong");
        } else throw std::runtime_error("unknown case");
        std::cout << "RSSI " << name << " passed\n";
    } catch(const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
