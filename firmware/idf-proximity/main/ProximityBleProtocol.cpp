#include "ProximityBleProtocol.h"
#include <cstring>
namespace larp { namespace proximity { namespace ble {
namespace {
void put16(std::uint8_t *p,std::uint16_t n){p[0]=n;p[1]=n>>8;}
std::uint16_t get16(const std::uint8_t *p){return std::uint16_t(p[0])|(std::uint16_t(p[1])<<8);}
bool equal(const std::uint8_t *a,const std::uint8_t *b,unsigned n){unsigned diff=0;while(n--)diff|=*a++^*b++;return !diff;}
}
void Session::header(Message &m,std::uint8_t type,std::uint16_t size){m={};m.size=size;m.bytes[0]=1;m.bytes[1]=type;put16(m.bytes.data()+2,size);}
bool Session::tag(const Secret &key,const char *domain,const std::uint8_t *p,std::size_t n,Secret &out){
    std::array<std::uint8_t,320> bytes{};const auto d=std::strlen(domain);if(d+n>bytes.size())return false;
    std::memcpy(bytes.data(),domain,d);std::memcpy(bytes.data()+d,p,n);return crypto_.hmac(key,bytes.data(),d+n,out);
}
bool Session::sign(Message &m,const Secret &key,const char *domain){Secret out{};if(m.size<36||!tag(key,domain,m.bytes.data(),m.size-32,out))return false;std::copy(out.begin(),out.end(),m.bytes.begin()+m.size-32);return true;}
bool Session::verify(const Message &m,const Secret &key,const char *domain){
    if(m.size<36||m.size>m.bytes.size()||m.bytes[0]!=1||get16(m.bytes.data()+2)!=m.size)return false;
    Secret out{};return tag(key,domain,m.bytes.data(),m.size-32,out)&&equal(out.data(),m.bytes.data()+m.size-32,32);
}
bool Session::same(const Message &a,const Message &b)const{return a.size==b.size&&std::equal(a.bytes.begin(),a.bytes.begin()+a.size,b.bytes.begin());}
bool Session::begin(const Credentials &access,const Token &boot,const Token &link,std::uint32_t now,const CurveView &view,Message &out){
    reset();bool constant=true;for(auto byte:access.secret)if(byte!=access.secret[0])constant=false;
    if(constant||!nonzero(access.device.data(),16)||!nonzero(boot.data(),16)||!nonzero(link.data(),16))return false;
    access_=access;boot_=boot;link_=link;at_=now;started_=true;header(out,0x81,90);
    std::copy(access.device.begin(),access.device.end(),out.bytes.begin()+4);std::copy(boot.begin(),boot.end(),out.bytes.begin()+20);std::copy(link.begin(),link.end(),out.bytes.begin()+36);
    put32(out.bytes.data()+52,view.revision);put16(out.bytes.data()+56,1);return sign(out,access.secret,"LARP-PROX-BLE/1/challenge");
}
bool Session::proof(const Message &m,std::uint32_t now,const CurveView &view,Message &out){
    if(!started_||authenticated_||now-at_>=5000U||m.size!=52||m.bytes[0]!=1||m.bytes[1]!=1||get16(m.bytes.data()+2)!=52||!nonzero(m.bytes.data()+4,16))return false;
    std::copy(m.bytes.begin()+4,m.bytes.begin()+20,client_.begin());std::array<std::uint8_t,64> context{};
    std::copy(access_.device.begin(),access_.device.end(),context.begin());std::copy(boot_.begin(),boot_.end(),context.begin()+16);std::copy(link_.begin(),link_.end(),context.begin()+32);std::copy(client_.begin(),client_.end(),context.begin()+48);
    Secret expected{},identity{};
    if(!tag(access_.secret,"LARP-PROX-BLE/1/proof",context.data(),context.size(),expected)||!equal(expected.data(),m.bytes.data()+20,32)||
       !tag(access_.secret,"LARP-PROX-BLE/1/session",context.data(),context.size(),key_)||!tag(access_.secret,"LARP-PROX-BLE/1/id",context.data(),context.size(),identity))return false;
    std::copy(identity.begin(),identity.begin()+16,id_.begin());authenticated_=true;
    header(out,0x82,88);std::copy(id_.begin(),id_.end(),out.bytes.begin()+4);put32(out.bytes.data()+20,view.revision);std::copy(view.digest.begin(),view.digest.end(),out.bytes.begin()+24);
    return sign(out,key_,"LARP-PROX-BLE/1/auth-result");
}
bool Session::result(std::uint32_t request,std::uint8_t code,const CurveView &view,Message &out){
    if(!authenticated_)return false;
    header(out,0x83,102);std::copy(id_.begin(),id_.end(),out.bytes.begin()+4);put32(out.bytes.data()+20,request);out.bytes[24]=code;put32(out.bytes.data()+25,view.revision);
    std::copy(view.digest.begin(),view.digest.end(),out.bytes.begin()+29);const auto points=view.curve.encode();std::copy(points.begin(),points.end(),out.bytes.begin()+61);out.bytes[69]=view.durable?1:0;
    if(!sign(out,key_,"LARP-PROX-BLE/1/curve-result"))return false;
    if(request==lastRequest_)cached_=out;
    return true;
}
bool Session::command(const Message &m,std::uint32_t now,const CurveView &view,bool &apply,ProximityCurve &curve,std::uint32_t &request,Message &out){
    apply=false;out={};if(!authenticated_||m.size<24)return false;
    const bool write=m.bytes[1]==3;
    if((m.bytes[1]!=2&&!write)||m.size!=(write?74:62)||!equal(id_.data(),m.bytes.data()+4,16)||!verify(m,key_,write?"LARP-PROX-BLE/1/set-curve":"LARP-PROX-BLE/1/get-curve"))return false;
    request=get32(m.bytes.data()+20);if(!request)return false;
    if(request==lastRequest_){
        if(same(m,lastCommand_)&&cached_.size){out=cached_;return true;}
        const auto saved=cached_;const bool ok=result(request,7,view,out);cached_=saved;return ok;
    }
    if(request<lastRequest_)return result(request,6,view,out);
    lastRequest_=request;lastCommand_=m;cached_={};
    const auto ttl=get16(m.bytes.data()+28);const auto age=now-get32(m.bytes.data()+24);
    if(!ttl||ttl>2000||age>ttl)return result(request,3,view,out);
    if(!write)return result(request,0,view,out);
    if(get32(m.bytes.data()+30)!=view.revision)return result(request,1,view,out);
    if(!ProximityCurve::decode(m.bytes.data()+34,curve))return result(request,2,view,out);
    if(!view.durable)return result(request,5,view,out);
    if(view.revision==UINT32_MAX)return result(request,8,view,out);
    apply=true;return true;
}
bool Session::telemetry(const Telemetry &s,const CurveView &view,Message &out){
    if(!authenticated_||telemetrySequence_==UINT32_MAX)return false;
    header(out,0x84,145);std::copy(id_.begin(),id_.end(),out.bytes.begin()+4);
    put32(out.bytes.data()+20,++telemetrySequence_);put32(out.bytes.data()+24,s.deviceMs);put16(out.bytes.data()+28,s.flags&0x7ff);put16(out.bytes.data()+30,s.fault);put16(out.bytes.data()+32,s.bleFault);
    out.bytes[34]=s.raw;out.bytes[35]=s.filtered;put16(out.bytes.data()+36,s.targetLevel);put32(out.bytes.data()+38,s.age);put32(out.bytes.data()+42,s.sampleSequence);std::copy(s.peer.begin(),s.peer.end(),out.bytes.begin()+46);
    const led::Pixel values[]={s.computed,s.submitted,s.confirmed};unsigned at=52;for(const auto &p:values){out.bytes[at++]=p.red;out.bytes[at++]=p.green;out.bytes[at++]=p.blue;}
    put32(out.bytes.data()+61,s.submittedAt);put32(out.bytes.data()+65,s.confirmedAt);put32(out.bytes.data()+69,view.revision);std::copy(view.digest.begin(),view.digest.end(),out.bytes.begin()+73);put32(out.bytes.data()+105,s.submittedCurveRevision);put32(out.bytes.data()+109,s.confirmedCurveRevision);
    return sign(out,key_,"LARP-PROX-BLE/1/telemetry");
}
void Assembler::reset() noexcept {
    current_ = {};
    transfer_ = completed_ = received_ = previousOffset_ = previousSize_ = 0;
    started_ = 0;
}
FrameResult Assembler::accept(const std::uint8_t *bytes, std::size_t size,
                              std::uint32_t now, Message &out) noexcept {
    out.size = 0;
    if (!bytes || size <= 8 || size > 20 || bytes[0] != 1 || bytes[1] != 0x4c)
        return FrameResult::Rejected;
    const auto transfer = get16(bytes + 2), total = get16(bytes + 4), offset = get16(bytes + 6);
    const auto count = static_cast<std::uint16_t>(size - 8);
    if (!transfer || !total || total > 256 || offset + count > total)
        return FrameResult::Rejected;
    if (!transfer_) {
        if (transfer <= completed_ || offset) return FrameResult::Rejected;
        transfer_ = transfer; current_.size = total; received_ = 0; started_ = now;
    }
    if (now - started_ >= 2000U || transfer != transfer_ || total != current_.size)
        return FrameResult::Rejected;
    if (received_ && offset == previousOffset_ && count == previousSize_ &&
        equal(bytes + 8, current_.bytes.data() + offset, count)) return FrameResult::Duplicate;
    if (offset != received_) return FrameResult::Rejected;
    std::copy_n(bytes + 8, count, current_.bytes.begin() + offset);
    previousOffset_ = offset; previousSize_ = count;
    received_ = static_cast<std::uint16_t>(received_ + count);
    if (received_ != total) return FrameResult::Receiving;
    out = current_; completed_ = transfer_; transfer_ = 0;
    previousOffset_ = previousSize_ = received_ = 0;
    return FrameResult::Complete;
}
bool encodeFrame(const Message &message, std::uint16_t transfer, std::uint16_t offset,
                  std::array<std::uint8_t, 20> &bytes, std::size_t &written) noexcept {
    written = 0;
    if (!transfer || !message.size || message.size > message.bytes.size() || offset >= message.size)
        return false;
    bytes.fill(0);
    bytes[0] = 1; bytes[1] = 0x4c;
    put16(bytes.data() + 2, transfer); put16(bytes.data() + 4, message.size);
    put16(bytes.data() + 6, offset);
    const auto count = std::min<std::size_t>(12, message.size - offset);
    std::copy_n(message.bytes.data() + offset, count, bytes.begin() + 8);
    written = 8 + count;
    return true;
}
}}}
