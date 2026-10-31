#pragma once
#include "ProximityBleTransport.h"
#include "ProximityBleCrypto.h"
#include <nvs.h>
namespace larp { namespace proximity { namespace ble {
class Settings {
    Crypto &crypto_;bool curveTrusted_=false;
    bool save(const char*,const std::uint8_t*,std::size_t);
public:
    explicit Settings(Crypto &c):crypto_(c){}
    bool load(Credentials&,bool &hasAccess,CurveView&,bool &hasCurve);
    bool provision(Credentials&,CurveView&);
    bool saveCurve(const CurveView&);
    bool curveTrusted()const{return curveTrusted_;}
};
class Runtime {
    NativeCrypto crypto_;Settings settings_{crypto_};Session session_{crypto_};Transport &bearer_;
    Credentials access_;CurveView view_;Assembler assembler_;
    bool loaded_=false,hasAccess_=false,hasCurve_=false,started_=false;
    std::uint32_t epoch_=0,sessionAt_=0,lastStatus_=0,lastApply_=0,appliedRevision_=0;
    bool applied_=false;std::uint16_t fault_=0;
    bool start();void revoke();
public:
    explicit Runtime(Transport &bearer):bearer_(bearer){}
    bool begin(Core&,bool radioReady,std::uint32_t now);
    bool provision(Core&,bool radioReady,std::uint32_t now);
    bool credential(Credentials &out)const{if(!loaded_||!hasAccess_||!view_.durable)return false;out=access_;return true;}
    void process(Core&);
    void publish(const Telemetry&);
    std::uint32_t revision()const{return appliedRevision_;}
    const CurveView &view()const{return view_;}
    std::uint16_t fault()const{return fault_;}
    bool ready()const{return started_&&bearer_.ready();}
};
}}}
