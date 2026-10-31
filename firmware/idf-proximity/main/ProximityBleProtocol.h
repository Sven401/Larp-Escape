#pragma once
#include "ProximityCore.h"
namespace larp { namespace proximity { namespace ble {
struct Message { std::array<std::uint8_t,256> bytes{};std::uint16_t size=0; };
enum class FrameResult : std::uint8_t { Receiving,Complete,Duplicate,Rejected };
class Assembler {
    Message current_;
    std::uint16_t transfer_=0,completed_=0,received_=0,previousOffset_=0,previousSize_=0;
    std::uint32_t started_=0;
public:
    void reset()noexcept;
    FrameResult accept(const std::uint8_t*,std::size_t,std::uint32_t,Message&)noexcept;
};
bool encodeFrame(const Message&,std::uint16_t,std::uint16_t,std::uint8_t*,std::size_t,std::size_t&,std::size_t frameLimit=20)noexcept;
template<std::size_t N> bool encodeFrame(const Message &message,std::uint16_t transfer,std::uint16_t offset,std::array<std::uint8_t,N> &bytes,std::size_t &written,std::size_t frameLimit=20)noexcept {
    return encodeFrame(message,transfer,offset,bytes.data(),bytes.size(),written,frameLimit);
}
struct Credentials { Token device{};Secret secret{}; };
class Crypto { public:virtual ~Crypto()=default;virtual bool hmac(const Secret&,const std::uint8_t*,std::size_t,Secret&)=0;virtual bool hash(const std::uint8_t*,std::size_t,Secret&)=0; };
struct CurveView { ProximityCurve curve;std::uint32_t revision=1;Secret digest{};bool durable=false; };
inline bool digest(Crypto &crypto,const ProximityCurve &curve,Secret &out) {
    std::array<std::uint8_t,10> bytes{{1,4}};const auto points=curve.encode();std::copy(points.begin(),points.end(),bytes.begin()+2);
    return crypto.hash(bytes.data(),bytes.size(),out);
}
struct Telemetry {
    std::uint32_t sequence=0,deviceMs=0,age=UINT32_MAX,sampleSequence=0,submittedAt=0,confirmedAt=0,submittedCurveRevision=0,confirmedCurveRevision=0;
    std::uint16_t flags=0,fault=0,bleFault=0,targetLevel=0;
    std::int8_t raw=0,filtered=0;Mac peer{};
    led::Pixel computed{},submitted{},confirmed{};
};
class Session {
    Crypto &crypto_;Credentials access_;Token boot_{},link_{},client_{},id_{};Secret key_{};
    bool started_=false,authenticated_=false;std::uint32_t at_=0,lastRequest_=0,telemetrySequence_=0;
    Message lastCommand_{},cached_{};
    bool tag(const Secret&,const char*,const std::uint8_t*,std::size_t,Secret&);
    bool sign(Message&,const Secret&,const char*);
    bool verify(const Message&,const Secret&,const char*);
    static void header(Message&,std::uint8_t,std::uint16_t);
    bool same(const Message&,const Message&) const;
public:
    explicit Session(Crypto &c):crypto_(c){}
    void reset(){started_=authenticated_=false;key_={};id_={};lastRequest_=telemetrySequence_=0;lastCommand_={};cached_={};}
    bool begin(const Credentials&,const Token &boot,const Token &link,std::uint32_t now,const CurveView&,Message&);
    bool proof(const Message&,std::uint32_t now,const CurveView&,Message&);
    bool authenticated()const{return authenticated_;}
    // false revokes only BLE. apply=true requests an owner-side durable CAS commit.
    bool command(const Message&,std::uint32_t now,const CurveView&,bool &apply,ProximityCurve &points,std::uint32_t &request,Message&reply);
    bool result(std::uint32_t request,std::uint8_t code,const CurveView&,Message&);
    bool telemetry(const Telemetry&,const CurveView&,Message&);
};
}}}
