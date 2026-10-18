#include "NearGlow.h"
#include "CameraLabController.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
using namespace larp::led;
namespace {
void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
bool rgb(Pixel p, unsigned r, unsigned g, unsigned b) { return p.red == r && p.green == g && p.blue == b; }
near::Command cmd(unsigned slot=0) {
    near::Command c; c.slot=slot; c.generation=c.sequence=1; c.parameters.level=1000; c.parameters.valid=true; return c;
}
void apply(near::Scene &s, const near::Command &c, std::uint32_t now) {
    s.service(now); check(std::strcmp(s.apply(c,now),"applied")==0,"command rejected");
}
void defaults() {
    near::Scene s; auto c=cmd(); apply(s,c,0);
    check(rgb(s.render(250)[0],6,3,0),"quarter default");
    check(rgb(s.render(500)[0],12,6,1),"half default");
    check(rgb(s.render(750)[0],18,9,1),"three-quarter default");
    check(rgb(s.render(1000)[0],24,12,2),"full default");
    c.operation=near::Operation::Update; ++c.sequence; c.parameters.level=500; apply(s,c,1000);
    check(rgb(s.render(1500)[0],18,9,1),"default update");
    c.parameters.valid=false; ++c.sequence; apply(s,c,2000);
    check(rgb(s.render(2500)[0],6,3,0),"invalid fade");
    c.parameters.level=123; ++c.sequence; apply(s,c,2500);
    check(s.instance(0).startedAt==2000,"ignored invalid level restarted fade");
    check(rgb(s.render(3000)[0],0,0,0) && s.instance(0).state==near::State::Active,"invalid remains active");
}
void reconfigure() {
    near::Scene s; auto c=cmd(); apply(s,c,0);
    const auto before=near::Scene::value(s.instance(0),333);
    c.operation=near::Operation::Update; ++c.sequence; c.parameters.colour.red=0;
    c.parameters.colour.green=32; c.parameters.colour.blue=16; c.parameters.gain=500;
    c.parameters.duration=2000; c.parameters.curve=1; apply(s,c,333);
    check(near::Scene::value(s.instance(0),333)==before,"active colour/gain/curve update jumped");
    const auto half=near::Scene::value(s.instance(0),1333);
    check(half[0]==3996000 && half[1]==9998000 && half[2]==4333000,"rebased smooth half");
    check(rgb(s.render(2333)[0],0,16,8),"new live target");
    ++c.sequence; apply(s,c,2400); const auto anchor=s.instance(0).startedAt;
    ++c.sequence; apply(s,c,2500); check(s.instance(0).startedAt==anchor,"equal configuration restarted");
}
void curves_and_bounds() {
    for (unsigned curve=0;curve<2;++curve) for(unsigned level=0;level<=1000;++level) {
        near::Scene s; auto c=cmd(); c.parameters.curve=curve; c.parameters.level=level; c.parameters.duration=5000;
        c.parameters.colour.red=c.parameters.colour.green=c.parameters.colour.blue=32; apply(s,c,0);
        std::uint32_t previous=0;
        for(unsigned t: {0U,1250U,2500U,3750U,5000U}) {
            const auto value=near::Scene::value(s.instance(0),t);
            check(value[0]>=previous && value[0]<=32000000U,"curve monotonic/range"); previous=value[0];
        }
        const auto frame=s.render(5000); for(const auto &p:frame) check(rgb(p,32*level/1000,32*level/1000,32*level/1000),"range endpoint");
    }
    near::Scene s; auto c=cmd(); c.parameters.curve=1; apply(s,c,0);
    check(near::Scene::value(s.instance(0),250)[0]==3750000,"smooth quarter");
    check(near::Scene::value(s.instance(0),750)[0]==20250000,"smooth three-quarter");
}
void stop_and_instances() {
    near::Scene s; auto a=cmd(), b=cmd(1); apply(s,a,0); apply(s,b,0);
    Pixel bg; bg.red=bg.green=bg.blue=4; check(s.background(bg),"background");
    check(rgb(s.render(1000)[0],32,28,8),"ADD32");
    a.operation=near::Operation::Update; a.parameters.valid=false; ++a.sequence; apply(s,a,1000);
    a.operation=near::Operation::Stop; ++a.sequence; apply(s,a,1500);
    check(near::Scene::value(s.instance(0),2000)[0]==6000000,"stop during invalid full own duration");
    ++a.sequence; apply(s,a,2000); check(s.instance(0).startedAt==1500,"repeat stop extends");
    check(rgb(s.render(2500)[0],28,16,6),"targeted stop harmed other contribution");
    a.operation=near::Operation::Update; ++a.sequence;
    check(std::strcmp(s.apply(a,2500),"lifecycle")==0,"revived stopped effect");
    a.operation=near::Operation::Start; a.generation=2; a.sequence=1; a.parameters.valid=true; apply(s,a,2500);
    check(near::Scene::value(s.instance(0),2500)[0]==0,"restart not zero");
    s.reset(); for(const auto &p:s.render(2501)) check(rgb(p,0,0,0),"reset not black");
}
void identity_and_atomicity() {
    near::Scene s; auto c=cmd(); apply(s,c,0); const auto revision=s.revision();
    check(std::strcmp(s.apply(c,100),"duplicate")==0 && s.revision()==revision,"duplicate mutation");
    c.parameters.level=500; check(std::strcmp(s.apply(c,100),"conflict")==0,"payload conflict");
    c.sequence=2; c.operation=near::Operation::Start;
    check(std::strcmp(s.apply(c,100),"lifecycle")==0 && s.instance(0).parameters.level==1000,"start partial mutation");
    c.operation=near::Operation::Update; c.parameters.duration=0;
    check(std::strcmp(s.apply(c,100),"parameters")==0 && s.revision()==revision,"invalid duration mutation");
    c.parameters.duration=1000; c.operation=near::Operation::Stop;
    check(std::strcmp(s.apply(c,100),"stop-parameters")==0,"stop secretly changes config");
}
void time() {
    near::Scene s; auto c=cmd(); const std::uint32_t t=0xffffff00U; apply(s,c,t);
    check(rgb(s.render(t+500U)[0],12,6,1),"wrap ramp");
    check(rgb(s.render(t+1000U)[0],24,12,2),"wrap target");
    for(std::uint32_t now: {1000000000U,2000000000U,3000000000U,4000000000U,0U,1000000000U})
        check(rgb(s.render(now)[0],24,12,2),"long uptime replayed ramp");
}
void protocol() {
    camera::LiveController c; check(c.begin("0123456789abcdef"),"begin");
    const char *first="GLOW 0123456789abcdef 1 0 1 start 1000 1 24 12 2 1000 1000 0";
    check(std::strcmp(c.command(first,0).kind,"APPLIED")==0,"protocol start");
    check(std::strcmp(c.command(first,100).kind,"DUPLICATE")==0,"protocol duplicate");
    check(std::strcmp(c.command("BG 0123456789abcdef 1 0 0 0",100).reason,"conflict")==0,"global conflict");
    const auto revision=c.scene().revision();
    for(const char *bad: {"GLOW 0123456789abcdef 2 0 1 update -1 1 24 12 2 1000 1000 0", "GLOW 0123456789abcdef 2 0 1 update 1000 1 256 12 2 1000 1000 0", "GLOW 0123456789abcdef 4294967296 0 1 update 1000 1 24 12 2 1000 1000 0", "GLOW 0123456789abcdef 2 0 1 update 1000 1 24 12 2 1000 1000 0 extra"}) {
        check(std::strcmp(c.command(bad,100).kind,"REJECT")==0 && c.scene().revision()==revision,"malformed command not atomic");
    }
    check(std::strcmp(c.command("GLOW fedcba9876543210 2 0 1 update 500 1 24 12 2 1000 1000 0",100).reason,"token")==0,"token fence");
    check(std::strcmp(c.command("BG 0123456789abcdef 2 4 4 4",100).kind,"APPLIED")==0,"background protocol");
    check(std::strcmp(c.command(first,100).reason,"stale")==0,"global stale");
}
}
int main() {
    try { defaults(); reconfigure(); curves_and_bounds(); stop_and_instances(); identity_and_atomicity(); time(); protocol();
        std::cout << "NearGlow: 7 groups including 2002 level/curve cases passed\n";
    } catch(const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
