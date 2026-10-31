#include "ProximityBleRuntime.h"
#include "Sdk.h"
#include "../../firmware/idf-proximity/main/ProximityBleCrypto.cpp"
#include "../../firmware/idf-proximity/main/ProximityBleRuntime.cpp"
#include <deque>
#include <stdexcept>
#include <cstdio>
using namespace larp::proximity;
using ble::Message;
unsigned assertions=0;
void check(bool ok,const char *message){++assertions;if(!ok)throw std::runtime_error(message);}
class Transport final:public ble::Transport {
public:
    bool begun=false,live=true,broken=false;unsigned revoked=0;std::uint32_t epoch=1;
    std::deque<ble::Ingress> in;std::vector<Message> out;
    bool begin(const Token&)override{begun=true;return true;}
    bool receive(ble::Ingress &e)override{if(in.empty())return false;e=in.front();in.pop_front();return true;}
    bool send(std::uint32_t e,const Message &m)override{if(!current(e))return false;out.push_back(m);return true;}
    bool current(std::uint32_t e)const override{return begun&&live&&!broken&&e==epoch;}
    void revoke(std::uint32_t)override{++revoked;live=false;}
    bool ready()const override{return begun&&!broken;}
    bool fault()const override{return broken;}
    void message(const Message &m,std::uint16_t transfer){std::uint16_t offset=0;while(offset<m.size){std::array<std::uint8_t,20> bytes{};std::size_t count=0;check(ble::encodeFrame(m,transfer,offset,bytes,count),"runtime test encode");ble::Ingress e;e.epoch=epoch;e.at=fake::now;e.size=count;e.bytes=bytes;in.push_back(e);offset+=count-8;}}
};
Token token(unsigned value){Token b;b.fill(value);return b;}
void setup(){fake::multipleNvs=true;fake::records.clear();fake::staged.clear();fake::handles.clear();fake::incrementalRandom=true;fake::psaMacOk=true;fake::psaHashOk=true;fake::now=0;fake::writes=fake::commits=0;fake::commitAdvanceMs=0;fake::setFail=fake::commitFail=fake::readFail=fake::readCorrupt=false;fake::destroyFail=false;fake::activeKeys=fake::destroyCalls=0;}
void authenticate(Transport &t,ble::Runtime &runtime,Core &core){
    ble::Ingress sub;sub.kind=ble::Ingress::Kind::Subscribed;sub.epoch=t.epoch;sub.at=fake::now;t.in.push_back(sub);runtime.process(core);check(!t.out.empty()&&t.out.back().bytes[1]==0x81,"runtime challenge");
    Message proof;proof.size=52;proof.bytes[0]=1;proof.bytes[1]=1;proof.bytes[2]=52;std::fill(proof.bytes.begin()+4,proof.bytes.begin()+20,9);std::fill(proof.bytes.begin()+20,proof.bytes.begin()+52,7);t.message(proof,1);runtime.process(core);check(t.out.back().bytes[1]==0x82,"runtime auth result");
}
Message set(unsigned request,unsigned expected,const ProximityCurve &curve){Message m;m.size=74;m.bytes[0]=1;m.bytes[1]=3;m.bytes[2]=74;std::fill(m.bytes.begin()+4,m.bytes.begin()+20,7);put32(m.bytes.data()+20,request);put32(m.bytes.data()+24,fake::now);m.bytes[28]=0xd0;m.bytes[29]=7;put32(m.bytes.data()+30,expected);const auto bytes=curve.encode();std::copy(bytes.begin(),bytes.end(),m.bytes.begin()+34);std::fill(m.bytes.begin()+42,m.bytes.begin()+74,7);return m;}
void provisioning(){
    setup();Core core(token(1));Transport t;ble::Runtime runtime(t);check(runtime.begin(core,true,0),"missing setup is valid");check(!runtime.view().durable&&!t.begun&&fake::commits==0,"missing default wrote/advertised");
    check(runtime.provision(core,true,0)&&t.begun&&runtime.view().durable&&fake::commits==2,"explicit provisioning");ble::Credentials access;check(runtime.credential(access),"credential readback");const auto commits=fake::commits;check(runtime.provision(core,true,0)&&fake::commits==commits,"repeat export rotated/wrote");ble::Credentials same;check(runtime.credential(same)&&same.secret==access.secret&&same.device==access.device,"repeat export changed identity");
    check(fake::activeKeys==0,"BLE PSA handle leaked");
    fake::records["prox_ble:curve-v1"][8]^=1;Core reboot(token(2));Transport bad;ble::Runtime corrupt(bad);check(!corrupt.begin(reboot,true,0)&&reboot.fault()&&!reboot.arm(0),"corrupt curve allowed persistent ARM/default light");check(fake::commits==commits,"corrupt curve auto-repaired");
    setup();Core fresh(token(1));Transport partial;ble::Runtime failed(partial);check(failed.begin(fresh,true,0),"partial setup begin");fake::commitFail=true;check(!failed.provision(fresh,true,0)&&!partial.begun,"failed provisioning announced full success");
}
void owner(){
    setup();Core core(token(1));Transport t;ble::Runtime runtime(t);check(runtime.begin(core,true,0)&&runtime.provision(core,true,0),"owner setup");check(core.arm(0),"owner arm");
    struct PairCrypto:Crypto{bool mac(const std::uint8_t*,std::size_t,Secret &s)override{s.fill(7);return true;}} radioCrypto;
    const Mac a{{2,1,2,3,4,5}},b{{2,1,2,3,4,6}};Codec ab(radioCrypto,a,b),ba(radioCrypto,b,a);check(ab.initialize()&&ba.initialize(),"owner radio codecs");Packet request,response;const auto nonce=token(4),peerBoot=token(2);check(core.beginProbe(nonce,1,ab,request)&&ba.encode(true,peerBoot,get32(request.data()+52),nonce,core.boot(),response)&&core.response(response,-65,10,10),"owner source");
    fake::now=100;const auto before=fake::commits;authenticate(t,runtime,core);check(core.armed()&&core.acceptedSequence()==1&&fake::commits==before,"viewer changed autonomous state");
    fake::now=3009;ProximityCurve plateau;for(auto &p:plateau.points)p.percent=100;auto command=set(1,1,plateau);t.message(command,2);fake::commitAdvanceMs=2;runtime.process(core);fake::commitAdvanceMs=0;check(fake::now==3011&&runtime.view().revision==2&&runtime.revision()==2,"curve CAS/readback");check(!core.fresh(fake::now)&&core.level()==0,"NVS latency renewed stale source/positive target");
    ble::Telemetry snapshot;snapshot.deviceMs=fake::now;snapshot.age=core.sampleAge(fake::now);snapshot.targetLevel=core.fresh(fake::now)?core.level():0;snapshot.flags=core.fresh(fake::now)?16:0;runtime.publish(snapshot);check(t.out.back().bytes[1]==0x84&&get32(t.out.back().bytes.data()+24)==3011&&get32(t.out.back().bytes.data()+69)==2&&t.out.back().bytes[36]==0,"post-commit snapshot incoherent");
    const auto stored=fake::commits;t.message(command,3);runtime.process(core);check(fake::commits==stored&&t.out.back().bytes[24]==0,"duplicate config rewrote NVS");
    t.live=false;runtime.process(core);check(core.armed()&&!core.fault(),"BLE disconnect stopped autonomous core");
    t.live=true;++t.epoch;fake::now=4000;authenticate(t,runtime,core);fake::commitFail=true;auto next=set(1,2,ProximityCurve{});t.message(next,2);runtime.process(core);check(t.out.back().bytes[24]==5&&!runtime.view().durable&&runtime.revision()==2&&runtime.view().revision==2,"storage failure reported apply/success");check(core.armed()&&!core.fault(),"BLE storage fault stopped healthy active core");
    fake::commitFail=false;fake::now=6500;ble::Ingress old;old.epoch=t.epoch;old.at=4000;old.size=9;old.bytes[0]=1;old.bytes[1]=0x4c;t.in.push_back(old);runtime.process(core);check(t.revoked&&core.armed(),"old BLE frame accepted or revoked core");
    check(fake::activeKeys==0,"repeated auth/config leaked PSA slots");
}
int main(){try{provisioning();owner();std::printf("PASS PROX-BLE owner: %u assertions; explicit/read-only provisioning, corrupt curve gate, CAS/readback/replay, NVS2999/3000, BLE isolation\n",assertions);return 0;}catch(const std::exception &e){std::fprintf(stderr,"FAIL PROX-BLE owner %s\n",e.what());return 1;}}
