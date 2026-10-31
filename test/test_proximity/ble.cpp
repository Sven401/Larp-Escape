#include "ProximityBleProtocol.h"
#include "ble_vectors.h"
#include <cstdio>
#include <stdexcept>
#ifdef LARP_PROXIMITY_REAL_BLE_PSA
#include "ProximityBleCrypto.h"
extern "C" std::size_t larp_psa_volatile_slots();
#else
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/hmac.h>
#include <openssl/sha.h>
#endif
#endif
using namespace larp::proximity;
using larp::proximity::ble::Message;
unsigned assertions=0;
void check(bool value,const char *message){++assertions;if(!value)throw std::runtime_error(message);}
#ifdef LARP_PROXIMITY_REAL_BLE_PSA
using HostCrypto=ble::NativeCrypto;
#else
class HostCrypto final:public ble::Crypto {
    bool calculate(const Secret *key,const std::uint8_t *data,std::size_t count,Secret &out){
#ifdef _WIN32
        BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
        if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,key?BCRYPT_ALG_HANDLE_HMAC_FLAG:0)!=0)return false;
        const auto result=BCryptCreateHash(algorithm,&hash,nullptr,0,key?const_cast<PUCHAR>(key->data()):nullptr,key?key->size():0,0);bool ok=result==0;
        if(ok)ok=BCryptHashData(hash,const_cast<PUCHAR>(data),count,0)==0;
        if(ok)ok=BCryptFinishHash(hash,out.data(),out.size(),0)==0;
        if(hash)BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm,0);return ok;
#else
        if(!key)return SHA256(data,count,out.data())!=nullptr;
        unsigned written=0;return HMAC(EVP_sha256(),key->data(),key->size(),data,count,out.data(),&written)&&written==32;
#endif
    }
public:
    bool broken=false;
    bool hmac(const Secret &key,const std::uint8_t *data,std::size_t count,Secret &out)override{return !broken&&calculate(&key,data,count,out);}
    bool hash(const std::uint8_t *data,std::size_t count,Secret &out)override{return !broken&&calculate(nullptr,data,count,out);}
};
#endif
template<std::size_t N> Message fixture(const unsigned char(&bytes)[N]){Message m;m.size=N;std::copy(bytes,bytes+N,m.bytes.begin());return m;}
bool same(const Message&a,const Message&b){return a.size==b.size&&std::equal(a.bytes.begin(),a.bytes.begin()+a.size,b.bytes.begin());}
void resign(HostCrypto &crypto,Message &m,const char *domain){Secret key{};std::copy(fixtures::sessionKey,fixtures::sessionKey+32,key.begin());std::array<std::uint8_t,320> data{};auto size=std::strlen(domain);std::memcpy(data.data(),domain,size);std::copy(m.bytes.begin(),m.bytes.begin()+m.size-32,data.begin()+size);Secret tag{};check(crypto.hmac(key,data.data(),size+m.size-32,tag),"test HMAC");std::copy(tag.begin(),tag.end(),m.bytes.begin()+m.size-32);}
void curves(){
    ProximityCurve curve;check(curve.valid(),"default curve invalid");
    for(int rssi=-127;rssi<0;++rssi)check(curve.level(rssi)==std::max(0,std::min(1000,20*(rssi+90))),"default curve changed baseline");
    auto invalid=curve;invalid.points[1].rssi=invalid.points[0].rssi;check(!invalid.valid(),"duplicate x accepted");
    invalid=curve;invalid.points[1].percent=101;check(!invalid.valid(),"percent >100 accepted");
    invalid=curve;invalid.points[1].percent=90;check(!invalid.valid(),"decreasing y accepted");
    auto plateau=curve;for(auto &p:plateau.points)p.percent=100;check(plateau.valid()&&plateau.level(-127)==1000,"plateau rejected");
    Token boot;boot.fill(1);Core core(boot);HostCrypto crypto;
    struct RadioCrypto:larp::proximity::Crypto{bool mac(const std::uint8_t*,std::size_t,Secret &o)override{o.fill(7);return true;}} radioCrypto;
    const Mac a{{2,1,2,3,4,5}},b{{2,1,2,3,4,6}};Codec ab(radioCrypto,a,b),ba(radioCrypto,b,a);check(ab.initialize()&&ba.initialize(),"core codecs");
    Packet request;Token nonce;nonce.fill(2);check(core.arm(0)&&core.beginProbe(nonce,1,ab,request),"core start");
    Packet reply;Token peerBoot;peerBoot.fill(3);check(ba.encode(true,peerBoot,get32(request.data()+52),nonce,boot,reply)&&core.response(reply,-65,10,10),"core sample");
    check(core.render(1010).red==12,"baseline half gold");const auto age=core.sampleAge(1010),seq=core.acceptedSequence();
    check(core.setCurve(plateau,1010)&&core.level()==1000&&core.sampleAge(1010)==age&&core.acceptedSequence()==seq,"tuning changed source freshness");
    check(core.render(1010).red==12&&core.render(2010).red==24,"tuning jumped instead of soft update");
    core.service(3010);check(!core.fresh(3010)&&core.level()==0,"curve overrode source loss");check(core.render(4010).red==0,"source loss did not fade0");
    core.stop(4020);check(core.setCurve(curve,4021)&&!core.armed(),"curve rearmed STOP");
}
void protocol(){
    HostCrypto crypto;ble::Credentials access;std::copy(fixtures::deviceId,fixtures::deviceId+16,access.device.begin());std::copy(fixtures::accessSecret,fixtures::accessSecret+32,access.secret.begin());
    Token boot,link;std::copy(fixtures::bootNonce,fixtures::bootNonce+16,boot.begin());std::copy(fixtures::linkNonce,fixtures::linkNonce+16,link.begin());
    ble::CurveView view;view.revision=7;view.durable=true;check(ble::digest(crypto,view.curve,view.digest)&&std::equal(view.digest.begin(),view.digest.end(),fixtures::curveDigest),"curve SHA golden");
    ble::Session session(crypto);Message out;check(session.begin(access,boot,link,0,view,out)&&same(out,fixture(fixtures::challenge)),"challenge golden");
    check(session.proof(fixture(fixtures::proof),1000,view,out)&&same(out,fixture(fixtures::authOK)),"proof/auth golden");
    bool apply=false;ProximityCurve points;std::uint32_t request=0;
    auto get=fixture(fixtures::getCurve);check(session.command(get,1000,view,apply,points,request,out)&&!apply&&same(out,fixture(fixtures::getResult)),"GET golden");
    auto conflict=get;put32(conflict.bytes.data()+24,999);resign(crypto,conflict,"LARP-PROX-BLE/1/get-curve");
    check(session.command(conflict,1000,view,apply,points,request,out)&&out.bytes[24]==7,"request conflict");
    check(session.command(get,1200,view,apply,points,request,out)&&same(out,fixture(fixtures::getResult)),"conflict poisoned replay cache");
    ble::Telemetry state;state.deviceMs=1000;state.flags=0x4ff;state.raw=-63;state.filtered=-65;state.targetLevel=500;state.age=40;state.sampleSequence=17;state.peer={{2,1,2,3,4,6}};
    state.computed.red=12;state.computed.green=4;state.submitted.red=11;state.submitted.green=3;state.confirmed.red=10;state.confirmed.green=3;state.submittedAt=990;state.confirmedAt=995;state.submittedCurveRevision=state.confirmedCurveRevision=7;
    check(session.telemetry(state,view,out)&&same(out,fixture(fixtures::telemetry)),"145byte telemetry golden");
    const auto set=fixture(fixtures::setCurve);check(session.command(set,1100,view,apply,points,request,out)&&apply&&!out.size&&request==2,"SET admission");
    view.curve=points;++view.revision;check(ble::digest(crypto,points,view.digest),"new digest");check(session.result(request,0,view,out)&&same(out,fixture(fixtures::setResult)),"SET durable result golden");
    check(session.command(set,1200,view,apply,points,request,out)&&!apply&&same(out,fixture(fixtures::setResult)),"SET replay caused write");
    for(unsigned i=0;i<set.size;++i){auto bad=set;bad.bytes[i]^=1;check(!session.command(bad,1200,view,apply,points,request,out),"tampered command accepted");}
    auto next=set;put32(next.bytes.data()+20,3);put32(next.bytes.data()+30,8);resign(crypto,next,"LARP-PROX-BLE/1/set-curve");check(session.command(next,3001,view,apply,points,request,out)&&!apply&&out.bytes[24]==3,"expired SET accepted");
    put32(next.bytes.data()+20,4);put32(next.bytes.data()+24,4000);resign(crypto,next,"LARP-PROX-BLE/1/set-curve");check(session.command(next,1000,view,apply,points,request,out)&&!apply&&out.bytes[24]==3,"future tick accepted");
    put32(next.bytes.data()+20,5);put32(next.bytes.data()+24,1000);next.bytes[36]=next.bytes[34];resign(crypto,next,"LARP-PROX-BLE/1/set-curve");check(session.command(next,1000,view,apply,points,request,out)&&!apply&&out.bytes[24]==2,"bad points accepted");
    next=set;put32(next.bytes.data()+20,6);resign(crypto,next,"LARP-PROX-BLE/1/set-curve");check(session.command(next,1000,view,apply,points,request,out)&&!apply&&out.bytes[24]==1,"bad CAS accepted");
    put32(next.bytes.data()+20,7);put32(next.bytes.data()+30,8);resign(crypto,next,"LARP-PROX-BLE/1/set-curve");view.durable=false;check(session.command(next,1000,view,apply,points,request,out)&&!apply&&out.bytes[24]==5&&!out.bytes[69],"uncertain NVS accepted");
    session.reset();check(!session.command(set,1000,view,apply,points,request,out),"disconnected command accepted");link[0]++;check(session.begin(access,boot,link,2000,view,out)&&!session.proof(fixture(fixtures::proof),2001,view,out),"old link proof accepted");
    access.secret.fill(0);check(!session.begin(access,boot,link,0,view,out),"default zero accesskey accepted");
}
void framing(){
    const auto message=fixture(fixtures::telemetry);ble::Assembler assembler;Message out;std::array<std::uint8_t,20> bytes{};std::size_t size=0;std::uint16_t offset=0;
    while(offset<message.size){check(ble::encodeFrame(message,1,offset,bytes,size),"encode145");const auto result=assembler.accept(bytes.data(),size,offset,out);offset+=size-8;check(result==(offset==message.size?ble::FrameResult::Complete:ble::FrameResult::Receiving),"assemble145");}
    check(same(out,message),"145byte roundtrip truncated");check(assembler.accept(bytes.data(),size,100,out)==ble::FrameResult::Rejected,"completed transfer replay accepted");
    assembler.reset();check(ble::encodeFrame(message,1,0,bytes,size),"frame start");check(assembler.accept(bytes.data(),size,0,out)==ble::FrameResult::Receiving,"frame begin");check(assembler.accept(bytes.data(),size,1,out)==ble::FrameResult::Duplicate,"frame duplicate not idempotent");
    check(ble::encodeFrame(message,1,12,bytes,size),"frame next");check(assembler.accept(bytes.data(),size,2000,out)==ble::FrameResult::Rejected,"frame timeout boundary");
    Message full;full.size=256;for(unsigned i=0;i<256;++i)full.bytes[i]=i;assembler.reset();offset=0;
    while(offset<256){check(ble::encodeFrame(full,2,offset,bytes,size),"encode256");const auto result=assembler.accept(bytes.data(),size,offset,out);offset+=size-8;check(result==(offset==256?ble::FrameResult::Complete:ble::FrameResult::Receiving),"assemble256");}
    check(same(full,out),"max frame bytes changed");full.size=257;check(!ble::encodeFrame(full,3,0,bytes,size),"257byte message accepted");
    full.size=256;
    for(const auto budget:{9U,20U,153U,244U})for(const auto &value:{message,full}){
        std::array<std::uint8_t,244> wide{};assembler.reset();offset=0;unsigned frames=0;
        while(offset<value.size){check(ble::encodeFrame(value,1,offset,wide,size,budget)&&size<=budget,"MTU encode bound");const auto result=assembler.accept(wide.data(),size,frames,out);offset+=size-8;++frames;check(result==(offset==value.size?ble::FrameResult::Complete:ble::FrameResult::Receiving),"MTU variable assembly");}
        check(same(value,out),"MTU roundtrip changed signed bytes");if(budget==153&&value.size==145)check(frames==1&&size==153,"telemetry not one ATTframe");if(budget==244&&value.size==256)check(frames==2&&size==28,"256 budget244 not two frames");
    }
    std::array<std::uint8_t,245> oversized{};check(!ble::encodeFrame(message,1,0,oversized,size,8)&&!ble::encodeFrame(message,1,0,oversized,size,245),"invalid frame budget");
    check(ble::encodeFrame(full,1,0,oversized,size,244)&&size==244,"244 frame encode");assembler.reset();check(assembler.accept(oversized.data(),245,0,out)==ble::FrameResult::Rejected,"245 frame accepted");check(assembler.accept(oversized.data(),244,0,out)==ble::FrameResult::Receiving,"244 frame rejected");
    check(assembler.accept(oversized.data(),243,1,out)==ble::FrameResult::Rejected,"truncated duplicate accepted");

}
#ifdef LARP_PROXIMITY_REAL_BLE_PSA
void keyLifetime(){
    HostCrypto crypto;ble::Credentials access;std::copy(fixtures::deviceId,fixtures::deviceId+16,access.device.begin());std::copy(fixtures::accessSecret,fixtures::accessSecret+32,access.secret.begin());
    Token boot,link,client;std::copy(fixtures::bootNonce,fixtures::bootNonce+16,boot.begin());std::copy(fixtures::linkNonce,fixtures::linkNonce+16,link.begin());std::copy(fixtures::clientNonce,fixtures::clientNonce+16,client.begin());
    ble::CurveView view;view.durable=true;check(ble::digest(crypto,view.curve,view.digest),"PSA repeated digest");
    const auto before=larp_psa_volatile_slots();
    for(unsigned i=0;i<64;++i){
        link[0]=i+1;ble::Session session(crypto);Message out;check(session.begin(access,boot,link,0,view,out),"PSA repeated begin");
        std::array<std::uint8_t,96> data{};const char domain[]="LARP-PROX-BLE/1/proof";constexpr auto domainBytes=sizeof(domain)-1;std::memcpy(data.data(),domain,domainBytes);
        std::copy(access.device.begin(),access.device.end(),data.begin()+domainBytes);std::copy(boot.begin(),boot.end(),data.begin()+domainBytes+16);std::copy(link.begin(),link.end(),data.begin()+domainBytes+32);std::copy(client.begin(),client.end(),data.begin()+domainBytes+48);
        Message proof;proof.size=52;proof.bytes[0]=1;proof.bytes[1]=1;proof.bytes[2]=52;std::copy(client.begin(),client.end(),proof.bytes.begin()+4);Secret tag{};
        check(crypto.hmac(access.secret,data.data(),domainBytes+64,tag),"PSA repeated proof tag");std::copy(tag.begin(),tag.end(),proof.bytes.begin()+20);
        check(session.proof(proof,1,view,out),"PSA repeated proof");ble::Telemetry state;state.deviceMs=500;check(session.telemetry(state,view,out)&&session.result(1,0,view,out),"PSA repeated telemetry/result");session.reset();
    }
    check(larp_psa_volatile_slots()==before,"BLE leaked PSA volatile key slots");
}
#endif
int main(){try{curves();protocol();framing();
#ifdef LARP_PROXIMITY_REAL_BLE_PSA
keyLifetime();
#endif
std::printf("PASS PROX-BLE: %u assertions; PWL/source-loss isolation, real-HMAC golden, CAS/replay, 145/256-byte frames\n",assertions);return 0;}catch(const std::exception &e){std::fprintf(stderr,"FAIL PROX-BLE %s\n",e.what());return 1;}}
