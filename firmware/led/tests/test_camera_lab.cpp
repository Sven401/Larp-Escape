#include <Arduino.h>
#include "LedOutput.h"
#include <vector>
#include <stdexcept>
#include <iostream>
std::uint32_t testNow = 0;
TestSerial Serial;
using namespace larp::led;
namespace {
void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
struct FakeBus final : LedBus {
    bool busy = false, stuck = false;
    unsigned resets = 0;
    std::uint32_t completeAt = 0;
    std::vector<Frame> frames;
    bool begin() override { return true; }
    bool submit(const Frame &frame) override {
        check(!busy, "overlapping transfer"); busy = true; completeAt = testNow + 3;
        frames.push_back(frame); return true;
    }
    TxResult poll() override {
        check(busy, "poll without transfer");
        if (stuck || testNow < completeAt) return TxResult::Busy;
        busy = false; return TxResult::Complete;
    }
    bool reset() override { busy = stuck = false; ++resets; return true; }
} fake;
bool black(const Frame &frame) {
    for (const auto &p : frame) if (p.red || p.green || p.blue) return false;
    return true;
}
}
namespace larp { namespace led { LedBus &cameraLabBus() { return fake; }}}
#include "../src/CameraLabMain.cpp"
int main(int argc, char **argv) {
    try {
        check(argc == 2, "need case");
        setup(); for (unsigned i = 0; i < 10; ++i) loop();
        check(engine.snapshot().ready && black(fake.frames.back()), "boot not confirmed dark");
        const std::string name = argv[1];
        Serial.send("CAL 0123456789abcdef"); loop();
        if (name == "sequence") {
            for (unsigned i = 0; i < 45000; ++i) {
                if (i % 200 == 0) Serial.send("KEEP 0123456789abcdef");
                loop();
            }
            check(!running && !stopPending && black(fake.frames.back()), "sequence did not finish dark");
            // Initial black, 133 fixed calibration stages, final fresh black.
            check(fake.frames.size() == 135, "missing/duplicate calibration frame");
            for (unsigned s = 0; s < 133; ++s) {
                const auto expected = camera::step(s);
                const auto &frame = fake.frames[s+1];
                for (unsigned i = 0; i < 23; ++i) {
                    Pixel pixel;
                    if (expected.index == -1 || expected.index == int(i)) pixel = expected.pixel;
                    check(frame[i].red == pixel.red && frame[i].green == pixel.green &&
                          frame[i].blue == pixel.blue, "wrong index/color in emitted calibration frame");
                }
            }
        } else if (name == "watchdog") {
            for (unsigned i = 0; i < 1700; ++i) loop();
            check(!running && !stopPending && black(fake.frames.back()), "watchdog failed dark");
            check(Serial.output.find("watchdog") != std::string::npos, "watchdog not reported");
        } else if (name == "burst" || name == "ripple" || name == "twinkle") {
            Serial.send("STOP"); for (unsigned i=0;i<10;++i) loop();
            Serial.send("RUN 0123456789abcdef "+name);
            for (unsigned i=0;i<8000;++i) {
                if (i%200==0) Serial.send("KEEP 0123456789abcdef");
                loop();
                if (i==5000) check(running && !black(fake.frames.back()),
                                   "effect ended before its actual output duration");
            }
            check(!running && !stopPending && black(fake.frames.back()),"effect did not finish dark");
        } else if (name == "stop-inflight") {
            // This transfer belongs to the direct calibration path, not engine.
            Serial.send("STOP"); for (unsigned i = 0; i < 10; ++i) loop();
            check(!stopPending && black(fake.frames.back()), "stop direct failed");
            Serial.send("RUN 0123456789abcdef burst");
            for (unsigned i = 0; i < 4300; ++i) {
                if (i % 200 == 0) Serial.send("KEEP 0123456789abcdef");
                loop();
            }
            Serial.send("STOP"); for (unsigned i = 0; i < 10; ++i) loop();
            check(!stopPending && black(fake.frames.back()), "stop engine failed");
        } else if (name == "live" || name == "live-watchdog" || name == "live-stop-timeout") {
            Serial.send("STOP"); for(unsigned i=0;i<10;++i) loop();
            Serial.send("LIVE 0123456789abcdef"); loop();
            check(liveMode && running, "LIVE not entered");
            Serial.send("GLOW 0123456789abcdef 1 0 1 start 1000 1 24 12 2 1000 1000 0");
            for(unsigned i=0;i<4;++i) loop();
            check(Serial.output.find("APPLIED 0123456789abcdef 1 2 ")!=std::string::npos,"full command not accepted");
            if(name == "live-stop-timeout") {
                while(!bus.inFlight) loop();
                fake.stuck=true; Serial.send("STOP");
                for(unsigned i=0;i<2050;++i) loop();
                check(fake.resets==1 && !stopPending && black(fake.frames.back()),"stuck transfer prevented confirmed black stop");
            } else if(name == "live-watchdog") {
                for(unsigned i=0;i<1700;++i) loop();
                check(!running && !stopPending && black(fake.frames.back()),"LIVE watchdog not dark");
            } else {
                // A pending frame must keep its submit-time revision when the config changes.
                while(!bus.inFlight) loop();
                const auto oldRevision=bus.pendingRevision; const auto oldFrame=bus.pendingNumber;
                Serial.send("GLOW 0123456789abcdef 2 0 1 update 1000 1 0 32 16 500 1000 1");
                for(unsigned i=0;i<4;++i) loop();
                check(live.scene().revision()==3,"live update not applied");
                check(Serial.output.find("L 0123456789abcdef "+std::to_string(oldRevision)+" "+std::to_string(oldFrame)+" ")!=std::string::npos,"inflight revision replaced");
                for(unsigned i=0;i<1100;++i) { if(i%200==0) Serial.send("KEEP 0123456789abcdef"); loop(); }
                check(fake.frames.back()[0].red==0 && fake.frames.back()[0].green==16 && fake.frames.back()[0].blue==8,"runtime params not reaching real loop frame");
                Serial.send("GLOW 0123456789abcdef 3 1 1 start 1000 1 24 12 2 1000 1000 0");
                for(unsigned i=0;i<1100;++i) { if(i%200==0) Serial.send("KEEP 0123456789abcdef"); loop(); }
                check(fake.frames.back()[0].red==24 && fake.frames.back()[0].green==28 && fake.frames.back()[0].blue==10,"parallel scene wrong");
                Serial.send("STOP"); for(unsigned i=0;i<10;++i) loop();
                check(!stopPending && black(fake.frames.back()),"LIVE STOP not confirmed black");
                check(Serial.output.find("DONE 0123456789abcdef")!=std::string::npos,"LIVE DONE absent");
                check(Serial.output.find("L 0123456789abcdef 5 ")!=std::string::npos,"black reset revision absent");
            }
        } else if (name == "seal" || name == "seal-watchdog" || name == "seal-stop-timeout") {
            Serial.send("STOP"); for (unsigned i=0;i<10;++i) loop();
            Serial.send("HELLO"); loop();
            check(Serial.output.find("LARP-SEAL/1 1 23 32") != std::string::npos,"seal capability absent");
            Serial.send("LIVE 0123456789abcdef"); loop();
            Serial.send("SEAL 0123456789abcdef 1 1 1 start 1 1 0 0 0 0 8 12 4 18 24 600 150 300 800");
            for (unsigned i=0;i<350;++i) loop();
            check(live.revision()==2 && fake.frames.back()[0].green==8 &&
                  fake.frames.back()[0].blue==12 && fake.frames.back()[8].blue==0,"seal snapshot did not reach bus");
            check(Serial.output.find("SEALSTATE 0123456789abcdef 2 ") != std::string::npos,"seal state absent");
            if (name == "seal-watchdog") {
                for (unsigned i=0;i<1700;++i) loop();
                check(!running && !stopPending && black(fake.frames.back()),"seal watchdog failed black");
                check(live.sealScene().snapshot().state==seal::State::Stopped,"seal watchdog retained effect");
            } else if (name == "seal-stop-timeout") {
                while (!bus.inFlight) loop();
                fake.stuck=true; Serial.send("STOP");
                for (unsigned i=0;i<2050;++i) loop();
                check(fake.resets==1 && !stopPending && black(fake.frames.back()),"seal stuck transfer blocked black stop");
            } else {
                while (!bus.inFlight) loop();
                const auto oldRevision=bus.pendingRevision, oldFrame=bus.pendingNumber;
                Serial.send("SEAL 0123456789abcdef 2 1 1 update 2 1 1 0 0 0 8 12 4 18 24 600 150 300 800");
                for (unsigned i=0;i<10;++i) loop();
                check(live.revision()==3,"seal revision not advanced");
                check(Serial.output.find("L 0123456789abcdef "+std::to_string(oldRevision)+" "+std::to_string(oldFrame)+" ")!=std::string::npos,"seal update relabelled old transfer");
                Serial.send("SEAL 0123456789abcdef 3 1 1 update 3 1 1 0 1 0 8 12 4 18 24 600 150 300 800");
                for (unsigned i=0;i<1500;++i) { if(i%200==0) Serial.send("KEEP 0123456789abcdef"); loop(); }
                for(const auto &p:fake.frames.back()) check(p.red==0 && p.green==8 && p.blue==12,"seal pulse did not settle");
                check(live.sealScene().snapshot().pulseConsumed && !live.sealScene().snapshot().pulsePending,"seal pulse state wrong");
                Serial.send("GLOW 0123456789abcdef 4 0 1 start 1000 1 24 8 0 1000 50 0");
                for(unsigned i=0;i<100;++i) loop();
                check(fake.frames.back()[0].red==24 && fake.frames.back()[0].green==16 && fake.frames.back()[0].blue==12,"seal/glow mix wrong");
                Serial.send("SEAL 0123456789abcdef 5 1 1 stop 3 1 1 0 0 0 8 12 4 18 24 600 150 300 800");
                for(unsigned i=0;i<350;++i) { if(i%200==0) Serial.send("KEEP 0123456789abcdef"); loop(); }
                check(fake.frames.back()[0].red==24 && fake.frames.back()[0].green==8 && fake.frames.back()[0].blue==0,"targeted seal stop harmed glow");
                const auto rev=live.revision();
                Serial.send("SEAL 0123456789abcdef 6 1 1 update 3 1 1 0 1 0 8 12 4 18 24 600 150 300 800");
                for(unsigned i=0;i<10;++i) loop();
                check(live.revision()==rev && live.sealScene().snapshot().state==seal::State::Stopped,"update revived stopped seal");
                Serial.send("STOP"); for(unsigned i=0;i<10;++i) loop();
                check(!stopPending && black(fake.frames.back()),"seal global stop not black");
                check(Serial.output.find("DONE 0123456789abcdef")!=std::string::npos,"seal stop unconfirmed");

                camera::LiveController p; check(p.begin("0123456789abcdef"),"protocol setup");
                const char *start="SEAL 0123456789abcdef 1 1 1 start 0 1 0 0 0 0 8 12 4 18 24 600 150 300 800";
                check(std::strcmp(p.command(start,0).kind,"APPLIED")==0,"seal parser start");
                check(std::strcmp(p.command(start,20).kind,"DUPLICATE")==0 && p.revision()==2,"seal duplicate changed revision");
                check(std::strcmp(p.command("BG 0123456789abcdef 1 0 0 0",20).reason,"conflict")==0,"seal/glow global sequence conflict absent");
                for(const char *bad:{
                    "SEAL fedcba9876543210 2 1 1 update 1 1 1 0 0 0 8 12 4 18 24 600 150 300 800",
                    "SEAL 0123456789abcdef 2 1 1 update 4 1 1 0 0 0 8 12 4 18 24 600 150 300 800",
                    "SEAL 0123456789abcdef 2 1 1 update 1 1 1 0 0 256 8 12 4 18 24 600 150 300 800",
                    "SEAL 0123456789abcdef 2 1 1 update 1 1 1 0 0 0 8 12 4 18 24 600 150 300 801",
                    "SEAL 0123456789abcdef 2 1 1 update 1 1 1 0 0 0 8 12 4 18 24 100 150 300 800",
                    "SEAL 0123456789abcdef 2 1 1 update 1 1 1 0 0 0 8 12 4 18 24 4294967296 150 300 800",
                    "SEAL 0123456789abcdef 2 1 1 update 1 1 1 0 0 0 8 12 4 18 24 -600 150 300 800",
                    "SEAL 0123456789abcdef 2 1 1 update 1 1 1 0 0 0 8 12 4 18 24 600 150 300 800 extra"})
                    check(std::strcmp(p.command(bad,20).kind,"REJECT")==0 && p.revision()==2 &&
                          p.sealScene().snapshot().parameters.count==0,"seal parser rejection not atomic");
                check(std::strcmp(p.command("BG 0123456789abcdef 2 32 32 32",20).kind,"APPLIED")==0,"global seq after seal reject");
                check(std::strcmp(p.command(start,20).reason,"stale")==0,"seal stale not rejected");
                for(const auto &pixel:p.render(400)) check(pixel.red==32 && pixel.green==32 && pixel.blue==32,"ADD32 saturated background wrong");
            }
        } else throw std::runtime_error("unknown case");
        check(!camera::validToken("bad") && !camera::validToken("0123456789abcdef0") &&
              camera::validToken("0123456789abcdef"), "token validation");
        std::cout << name << " passed\n";
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
