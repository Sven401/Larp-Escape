#include "ProximityCore.h"
#include "golden.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/hmac.h>
#endif
using namespace larp::proximity;
unsigned assertions=0;
void check(bool ok,const char *message){++assertions;if(!ok)throw std::runtime_error(message);}
class HostCrypto final:public Crypto {
public:
    Secret key{};bool broken=false;
    HostCrypto(){for(unsigned i=0;i<32;++i)key[i]=i;}
    bool mac(const std::uint8_t *p,std::size_t n,Secret &out)override {
        if(broken)return false;
#ifdef _WIN32
        BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
        if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,BCRYPT_ALG_HANDLE_HMAC_FLAG)!=0)return false;
        bool ok=BCryptCreateHash(alg,&hash,nullptr,0,key.data(),key.size(),0)==0;
        if(ok)ok=BCryptHashData(hash,const_cast<PUCHAR>(p),n,0)==0;
        if(ok)ok=BCryptFinishHash(hash,out.data(),out.size(),0)==0;
        if(hash)BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(alg,0);return ok;
#else
        unsigned size=0;return HMAC(EVP_sha256(),key.data(),key.size(),p,n,out.data(),&size)&&size==32;
#endif
    }
};
const Mac A{{2,1,2,3,4,5}},B{{2,1,2,3,4,6}};
Token token(unsigned byte){Token t;t.fill(byte);return t;}
bool black(larp::led::Pixel p){return !p.red&&!p.green&&!p.blue;}
void configTests(){
    Config c;c.peer=B;for(unsigned i=0;i<32;++i)c.secret[i]=i;
    const auto bytes=encodeConfig(c);Config decoded;
    check(decodeConfig(bytes.data(),bytes.size(),decoded)&&decoded.peer==B&&!decoded.armed&&decoded.secret==c.secret,"config roundtrip");
    for(unsigned n=0;n<48;++n)check(!decodeConfig(bytes.data(),n,decoded),"short config accepted");
    for(unsigned i=0;i<48;++i){auto bad=bytes;bad[i]^=1;check(!decodeConfig(bad.data(),bad.size(),decoded),"corrupt config accepted");}
    c.armed=true;auto armed=encodeConfig(c);check(decodeConfig(armed.data(),48,decoded)&&decoded.armed,"armed roundtrip");
    armed[2]=2;put32(armed.data()+44,crc(armed.data(),44));check(!decodeConfig(armed.data(),48,decoded),"wrong config version");
    check(parse("PAIR 02:01:02:03:04:06 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f").kind==UsbCommand::Kind::Pair,"USB pair");
    for(const auto *s:{"PAIR 02:01:02:03:04:06 0000000000000000000000000000000000000000000000000000000000000000","PAIR 03:01:02:03:04:06 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f","ARM ","STOP x","PAIR x","REBOOT x"})check(parse(s).kind==UsbCommand::Kind::Invalid,"invalid USB accepted");
    check(parse("STOP").kind==UsbCommand::Kind::Stop&&parse("REBOOT").kind==UsbCommand::Kind::Reboot&&parse("INFO").kind==UsbCommand::Kind::Info,"USB commands");
}
void packetTests(){
    HostCrypto crypto;Codec ab(crypto,A,B),ba(crypto,B,A);check(ab.initialize()&&ba.initialize(),"codec init");
    Packet request;check(ab.encode(false,token(1),1,token(7),{},request),"request encode");
    check(std::equal(request.begin(),request.end(),Golden),"independent Python golden request");
    check(ba.validate(request.data(),120,A,B),"valid auth request");
    for(unsigned n=0;n<120;++n)check(!ba.validate(request.data(),n,A,B),"short packet accepted");
    for(unsigned i=0;i<120;++i){auto bad=request;bad[i]^=1;check(!ba.validate(bad.data(),120,A,B),"tampered packet accepted");}
    check(!ba.validate(request.data(),120,B,A),"wrong routes accepted");
    Packet response;check(ba.encode(true,token(2),1,token(7),token(1),response)&&ab.validate(response.data(),120,B,A),"response auth");
    check(!ba.validate(response.data(),120,A,B),"reflection accepted");
    HostCrypto wrong;wrong.key[0]++;Codec other(wrong,A,B);check(other.initialize()&&!other.validate(response.data(),120,B,A),"wrong pair secret");
    crypto.broken=true;Packet ignored;check(!ab.encode(false,token(1),1,token(7),{},ignored)&&!ab.validate(response.data(),120,B,A),"crypto failure");
}
Packet responseFor(Codec &remote,const Packet &request,unsigned remoteBoot=2){
    Token nonce,echo;std::copy(request.begin()+56,request.begin()+72,nonce.begin());std::copy(request.begin()+36,request.begin()+52,echo.begin());
    Packet response;check(remote.encode(true,token(remoteBoot),get32(request.data()+52),nonce,echo,response),"reply encode");return response;
}
void lifecycleTests(){
    HostCrypto crypto;Codec ab(crypto,A,B),ba(crypto,B,A);check(ab.initialize()&&ba.initialize(),"core codecs");
    Core core(token(1));Packet request;
    check(!core.armed()&&black(core.render(0))&&!core.beginProbe(token(7),0,ab,request),"boot must be dark/disarmed");
    check(core.arm(0)&&black(core.render(0)),"arm starts unknown black");
    check(core.beginProbe(token(7),100,ab,request),"initial probe");auto response=responseFor(ba,request);
    check(ab.validate(response.data(),120,B,A)&&core.response(response,-40,110,120),"fresh response");
    check(core.fresh(3109)&&!core.fresh(3110),"exact freshness boundary");
    auto p=core.render(1120);check(p.red==24&&p.green==8&&!p.blue,"NearGlow exact gold ramp");
    check(!core.response(response,-40,1200,1200),"duplicate renewed freshness");
    check(core.beginProbe(token(8),1200,ab,request),"second probe");
    check(!core.response(response,-40,1210,1210),"old nonce accepted");response=responseFor(ba,request);
    check(core.response(response,-40,1210,1210),"second response");
    p=core.render(1220);check(p.red==24&&p.green==8,"identical sample restarted ramp");
    core.service(4210);check(!core.fresh(4210),"source not invalidated at3000");
    check(black(core.render(5210)),"invalid did not fade to zero, or 20percent fallback");
    check(core.beginProbe(token(9),5220,ab,request),"return probe");response=responseFor(ba,request);
    check(core.response(response,-40,5230,5230),"source return");check(black(core.render(5230)),"return jumped brightness");
    check(core.render(6230).red==24,"return did not ramp same run");
    check(core.beginProbe(token(10),6240,ab,request),"peerboot probe");response=responseFor(ba,request,3);
    check(core.response(response,-90,6250,6250)&&core.filtered()==-90&&core.level()==0,"peerboot filter not reset");
    core.stop(6260);core.service(7260);check(!core.armed()&&black(core.render(7260)),"stop completion");
    check(!core.response(response,-40,7261,7261)&&!core.beginProbe(token(11),7261,ab,request),"STOP reactivated");
    check(core.arm(7270),"explicit rearm");core.fail(7280);check(core.fault()&&!core.armed()&&!core.arm(8000),"fault rearmed");
    Core delayed(token(1));check(delayed.arm(0)&&delayed.beginProbe(token(3),100,ab,request),"delayed setup");response=responseFor(ba,request);
    check(!delayed.response(response,-40,110,3110),"queue draining made old RX fresh");
    check(delayed.response(response,-40,110,3109),"2999ms observation rejected");delayed.service(3110);check(!delayed.fresh(3110),"drain time refreshed sample");
    const auto wrap=UINT32_MAX-100U;Core wrapped(token(1));check(wrapped.arm(wrap)&&wrapped.beginProbe(token(4),wrap,ab,request),"wrap setup");response=responseFor(ba,request);
    check(wrapped.response(response,-65,10,20),"uint32 wrap valid response rejected");wrapped.service(3010);check(!wrapped.fresh(3010),"wrap expiry");
    Core rebooted(token(7));check(rebooted.arm(0)&&!rebooted.fresh(0)&&black(rebooted.render(0)),"persisted ARM recovered stale effect");
}
void laneTests(){
    TxLane lane;check(lane.begin(TxLane::Kind::Request,B,100),"lane begin");
    check(!lane.begin(TxLane::Kind::Response,B,101)&&!lane.idle(),"outstanding callback lane reused");
    check(lane.complete(A)==TxLane::Kind::None&&!lane.idle(),"wrong callback released lane");
    check(!lane.expired(2099)&&lane.expired(2100),"callback timeout exact boundary");
    check(lane.complete(B)==TxLane::Kind::Request&&lane.idle(),"request callback wrongkind");
    check(lane.begin(TxLane::Kind::Response,B,2200)&&lane.complete(B)==TxLane::Kind::Response,"receipt callback misclassified");
}
int main(){try{configTests();packetTests();lifecycleTests();laneTests();std::printf("PASS proximity: %u assertions; real HMAC+Python golden, strict config/USB, shared NearGlow, freshness/stop/reboot/clockwrap/TX lanes\n",assertions);return 0;}catch(const std::exception &e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
