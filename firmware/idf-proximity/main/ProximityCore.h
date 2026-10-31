#pragma once
#include "NearGlow.h"
#include "ProximityCurve.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
namespace larp { namespace proximity {
using Mac = std::array<std::uint8_t,6>;
using Secret = std::array<std::uint8_t,32>;
using Token = std::array<std::uint8_t,16>;
using Packet = std::array<std::uint8_t,120>;
inline bool nonzero(const std::uint8_t *p, std::size_t n) { std::uint8_t a=0; while(n--) a|=*p++; return a!=0; }
inline bool validMac(const Mac &m) { return nonzero(m.data(),m.size()) && !(m[0]&1); }
inline void put32(std::uint8_t *p,std::uint32_t n) { for(unsigned i=0;i<4;++i)p[i]=n>>(i*8); }
inline std::uint32_t get32(const std::uint8_t *p) { return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24); }
inline std::uint32_t crc(const std::uint8_t *p,std::size_t n) {
    std::uint32_t c=0xffffffffU;
    while(n--) { c^=*p++; for(unsigned b=0;b<8;++b)c=(c>>1)^((0U-(c&1U))&0xedb88320U); }
    return ~c;
}
struct Config { Mac peer{}; Secret secret{}; bool armed=false; };
using ConfigBytes = std::array<std::uint8_t,48>;
inline bool validConfig(const Config &c) {
    bool same=true; for(auto b:c.secret) if(b!=c.secret[0])same=false;
    return validMac(c.peer) && !same;
}
inline ConfigBytes encodeConfig(const Config &c) {
    ConfigBytes b{}; b[0]='P';b[1]='X';b[2]=1;b[3]=c.armed?1:0;
    std::copy(c.peer.begin(),c.peer.end(),b.begin()+4);
    std::copy(c.secret.begin(),c.secret.end(),b.begin()+10);
    put32(b.data()+44,crc(b.data(),44)); return b;
}
inline bool decodeConfig(const std::uint8_t *p,std::size_t n,Config &out) {
    if(!p||n!=48||p[0]!='P'||p[1]!='X'||p[2]!=1||p[3]>1||p[42]||p[43]||get32(p+44)!=crc(p,44))return false;
    Config c; c.armed=p[3]!=0; std::copy(p+4,p+10,c.peer.begin()); std::copy(p+10,p+42,c.secret.begin());
    if(!validConfig(c))return false;
    out=c; return true;
}
class Crypto { public: virtual ~Crypto()=default; virtual bool mac(const std::uint8_t*,std::size_t,Secret&)=0; };
class Codec {
    Crypto &crypto_; Mac local_,peer_; Token pair_{};
public:
    Codec(Crypto &c,const Mac &local,const Mac &peer):crypto_(c),local_(local),peer_(peer) {}
    bool initialize() {
        std::array<std::uint8_t,28> context{};
        std::memcpy(context.data(),"LARP-PROX-PAIR/1",16);
        const auto &a=local_<peer_?local_:peer_, &b=local_<peer_?peer_:local_;
        std::copy(a.begin(),a.end(),context.begin()+16); std::copy(b.begin(),b.end(),context.begin()+22);
        Secret digest{}; if(!crypto_.mac(context.data(),context.size(),digest))return false;
        std::copy(digest.begin(),digest.begin()+16,pair_.begin()); return true;
    }
    bool encode(bool response,const Token &boot,std::uint32_t seq,const Token &nonce,const Token &echo,Packet &out) {
        out={}; std::memcpy(out.data(),"LPRX",4);out[4]=1;out[5]=response?2:1;
        std::copy(pair_.begin(),pair_.end(),out.begin()+8);
        std::copy(local_.begin(),local_.end(),out.begin()+24); std::copy(peer_.begin(),peer_.end(),out.begin()+30);
        std::copy(boot.begin(),boot.end(),out.begin()+36); put32(out.data()+52,seq);
        std::copy(nonce.begin(),nonce.end(),out.begin()+56);std::copy(echo.begin(),echo.end(),out.begin()+72);
        Secret digest{}; if(!crypto_.mac(out.data(),88,digest))return false;
        std::copy(digest.begin(),digest.end(),out.begin()+88);return true;
    }
    bool validate(const std::uint8_t *p,std::size_t size,const Mac &source,const Mac &destination) {
        if(!p||size!=120||source!=peer_||destination!=local_||std::memcmp(p,"LPRX",4)||p[4]!=1||
            (p[5]!=1&&p[5]!=2)||p[6]||p[7]||!std::equal(pair_.begin(),pair_.end(),p+8)||
            !std::equal(peer_.begin(),peer_.end(),p+24)||!std::equal(local_.begin(),local_.end(),p+30)||
            !nonzero(p+36,16)||!get32(p+52)||!nonzero(p+56,16)||
            (p[5]==1&&nonzero(p+72,16))||(p[5]==2&&!nonzero(p+72,16)))return false;
        Secret digest{}; if(!crypto_.mac(p,88,digest))return false;
        unsigned diff=0;for(unsigned i=0;i<32;++i)diff|=digest[i]^p[88+i]; return !diff;
    }
};
class Core {
    led::near::Scene scene_;
    Token boot_{},nonce_{},peerBoot_{};
    bool armed_=false,pending_=false,fresh_=false,fault_=false;
    std::uint32_t probeSequence_=0,sequence_=0,generation_=0,sentAt_=0,receivedAt_=0;
    int q_=0,raw_=0;
    std::uint32_t acceptedSequence_=0;
    ProximityCurve curve_;
    led::near::Parameters parameters_;
    bool apply(led::near::Operation op,std::uint32_t now) {
        scene_.service(now);
        if(sequence_==UINT32_MAX) { fault_=true;return false; }
        led::near::Command c; c.slot=0;c.generation=generation_;c.sequence=++sequence_;c.operation=op;c.parameters=parameters_;
        const auto *result=scene_.apply(c,now);if(std::strcmp(result,"applied")){fault_=true;return false;} return true;
    }
public:
    explicit Core(const Token &boot):boot_(boot) { parameters_.colour.red=24;parameters_.colour.green=8;parameters_.colour.blue=0;parameters_.gain=1000;parameters_.duration=1000;parameters_.curve=1; }
    bool arm(std::uint32_t now) {
        if(fault_||armed_)return false;
        if(generation_==UINT32_MAX||!nonzero(boot_.data(),boot_.size())){fail(now);return false;}
        scene_.reset(); ++generation_;sequence_=0;parameters_.valid=false;parameters_.level=0;
        pending_=fresh_=false;peerBoot_={};q_=0;armed_=true;
        return apply(led::near::Operation::Start,now);
    }
    void stop(std::uint32_t now) {
        pending_=fresh_=false;
        if(armed_) { armed_=false;apply(led::near::Operation::Stop,now); }
    }
    void fail(std::uint32_t now) { stop(now);fault_=true; }
    bool beginProbe(const Token &nonce,std::uint32_t now,Codec &codec,Packet &out) {
        if(!armed_||fault_||pending_)return false;
        if(!nonzero(nonce.data(),nonce.size())||probeSequence_==UINT32_MAX){fail(now);return false;}
        nonce_=nonce;sentAt_=now;pending_=true;++probeSequence_;
        if(!codec.encode(false,boot_,probeSequence_,nonce_,{},out)){fail(now);return false;}return true;
    }
    void rejectSend() { pending_=false; }
    bool response(const Packet &p,int rssi,std::uint32_t observedAt,std::uint32_t now) {
        if(!armed_||fault_||!pending_||p[5]!=2||get32(p.data()+52)!=probeSequence_||
            !std::equal(nonce_.begin(),nonce_.end(),p.begin()+56)||!std::equal(boot_.begin(),boot_.end(),p.begin()+72)||
            observedAt-sentAt_>=2000U||now-observedAt>=3000U||rssi< -127||rssi>=0)return false;
        pending_=false;
        Token peerBoot{};std::copy(p.begin()+36,p.begin()+52,peerBoot.begin());
        if(!fresh_||observedAt-receivedAt_>=3000U||peerBoot!=peerBoot_)q_=rssi*256;
        else q_+=(rssi*256-q_)/4;
        peerBoot_=peerBoot;fresh_=true;receivedAt_=observedAt;raw_=rssi;acceptedSequence_=probeSequence_;
        parameters_.valid=true;parameters_.level=curve_.level(q_/256);
        return apply(led::near::Operation::Update,now);
    }
    void service(std::uint32_t now) {
        scene_.service(now);
        if(pending_&&now-sentAt_>=2000U)pending_=false;
        if(armed_&&fresh_&&now-receivedAt_>=3000U) {
            fresh_=false;parameters_.valid=false;parameters_.level=0;apply(led::near::Operation::Update,now);
        }
    }
    bool setCurve(const ProximityCurve &curve,std::uint32_t now) {
        if(!curve.valid()||fault_)return false;
        service(now);curve_=curve;
        if(armed_&&fresh(now)){parameters_.level=curve_.level(q_/256);return apply(led::near::Operation::Update,now);}
        return true;
    }
    int rawRssi()const{return raw_;}
    std::uint32_t acceptedSequence()const{return acceptedSequence_;}
    std::uint32_t sampleAge(std::uint32_t now)const{return acceptedSequence_?now-receivedAt_:UINT32_MAX;}
    led::Pixel render(std::uint32_t now) { return scene_.render(now)[0]; }
    bool armed()const{return armed_;} bool fault()const{return fault_;} bool pending()const{return pending_;}
    bool fresh(std::uint32_t now)const{return fresh_&&now-receivedAt_<3000U;}
    std::uint32_t age(std::uint32_t now)const{return fresh_?now-receivedAt_:UINT32_MAX;}
    int filtered()const{return q_/256;} std::uint16_t level()const{return parameters_.level;}
    const Token &boot()const{return boot_;}
};
class TxLane {
public: enum class Kind { None,Request,Response };
private: Kind kind_=Kind::None;std::uint32_t at_=0;Mac peer_{};
public:
    bool begin(Kind kind,const Mac &peer,std::uint32_t now){if(kind==Kind::None||kind_!=Kind::None)return false;kind_=kind;peer_=peer;at_=now;return true;}
    Kind complete(const Mac &peer){if(peer!=peer_)return Kind::None;auto k=kind_;kind_=Kind::None;return k;}
    bool idle()const{return kind_==Kind::None;}bool expired(std::uint32_t now)const{return !idle()&&now-at_>=2000U;}
};
struct UsbCommand { enum class Kind { Invalid,Info,Pair,Arm,Stop,Reboot,BleAccess,BleInfo } kind=Kind::Invalid;Config config; };
inline int hex(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
inline UsbCommand parse(const char *s) {
    UsbCommand c;if(!s)return c;
    if(!std::strcmp(s,"INFO"))c.kind=UsbCommand::Kind::Info;
    else if(!std::strcmp(s,"ARM"))c.kind=UsbCommand::Kind::Arm;
    else if(!std::strcmp(s,"STOP"))c.kind=UsbCommand::Kind::Stop;
    else if(!std::strcmp(s,"REBOOT"))c.kind=UsbCommand::Kind::Reboot;
    else if(!std::strcmp(s,"BLE-ACCESS"))c.kind=UsbCommand::Kind::BleAccess;
    else if(!std::strcmp(s,"BLE-INFO"))c.kind=UsbCommand::Kind::BleInfo;
    else if(std::strlen(s)==87&&!std::strncmp(s,"PAIR ",5)&&s[22]==' ') {
        for(unsigned i=0;i<6;++i) {auto at=5+i*3;int a=hex(s[at]),b=hex(s[at+1]);if(a<0||b<0||(i<5&&s[at+2]!=':'))return {};c.config.peer[i]=(a<<4)|b;}
        for(unsigned i=0;i<32;++i){int a=hex(s[23+i*2]),b=hex(s[24+i*2]);if(a<0||b<0)return {};c.config.secret[i]=(a<<4)|b;}
        if(validConfig(c.config))c.kind=UsbCommand::Kind::Pair;
    }return c;
}
}}
