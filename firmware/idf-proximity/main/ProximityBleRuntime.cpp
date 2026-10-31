#include "ProximityBleRuntime.h"
#include <psa/crypto.h>
#include <esp_random.h>
#include <esp_timer.h>
namespace larp { namespace proximity { namespace ble {
namespace {
void clear(void *memory,std::size_t n){auto *p=static_cast<volatile std::uint8_t*>(memory);while(n--)*p++=0;}
std::uint32_t nowMs(){return static_cast<std::uint32_t>(esp_timer_get_time()/1000);}
bool valid(const Credentials &c){bool constant=true;for(auto b:c.secret)if(b!=c.secret[0])constant=false;return !constant&&nonzero(c.device.data(),16);}
using AccessBytes=std::array<std::uint8_t,56>;
using CurveBytes=std::array<std::uint8_t,20>;
AccessBytes encode(const Credentials &c){AccessBytes b{};b[0]='P';b[1]='B';b[2]=1;std::copy(c.device.begin(),c.device.end(),b.begin()+4);std::copy(c.secret.begin(),c.secret.end(),b.begin()+20);put32(b.data()+52,crc(b.data(),52));return b;}
CurveBytes encode(const CurveView &v){CurveBytes b{};b[0]='P';b[1]='C';b[2]=1;b[3]=4;put32(b.data()+4,v.revision);const auto points=v.curve.encode();std::copy(points.begin(),points.end(),b.begin()+8);put32(b.data()+16,crc(b.data(),16));return b;}
}
bool Settings::load(Credentials &access,bool &hasAccess,CurveView &view,bool &hasCurve){
    curveTrusted_=false;hasAccess=hasCurve=false;view=CurveView{};access={};nvs_handle_t store=0;
    const auto opened=nvs_open("prox_ble",NVS_READONLY,&store);
    if(opened==ESP_ERR_NVS_NOT_FOUND){curveTrusted_=digest(crypto_,view.curve,view.digest);return curveTrusted_;}
    if(opened!=ESP_OK)return false;
    AccessBytes a{};CurveBytes c{};std::size_t as=a.size(),cs=c.size();
    const auto ar=nvs_get_blob(store,"access-v1",a.data(),&as),cr=nvs_get_blob(store,"curve-v1",c.data(),&cs);nvs_close(store);
    bool ok=true;
    if(ar==ESP_OK){hasAccess=true;ok=as==a.size()&&a[0]=='P'&&a[1]=='B'&&a[2]==1&&!a[3]&&get32(a.data()+52)==crc(a.data(),52);if(ok){std::copy(a.begin()+4,a.begin()+20,access.device.begin());std::copy(a.begin()+20,a.begin()+52,access.secret.begin());ok=valid(access);}}
    else if(ar!=ESP_ERR_NVS_NOT_FOUND)ok=false;
    if(cr==ESP_OK){hasCurve=true;const bool shape=cs==c.size()&&c[0]=='P'&&c[1]=='C'&&c[2]==1&&c[3]==4&&get32(c.data()+4)&&get32(c.data()+16)==crc(c.data(),16);
        if(!shape||!ProximityCurve::decode(c.data()+8,view.curve))ok=false;else{view.revision=get32(c.data()+4);view.durable=true;curveTrusted_=true;}}
    else if(cr!=ESP_ERR_NVS_NOT_FOUND)ok=false;
    else curveTrusted_=true;
    const bool hashed=digest(crypto_,view.curve,view.digest);curveTrusted_=curveTrusted_&&hashed;
    clear(a.data(),a.size());return ok&&hashed;
}
bool Settings::save(const char *name,const std::uint8_t *bytes,std::size_t size){
    if(size>56)return false;
    nvs_handle_t store=0;if(nvs_open("prox_ble",NVS_READWRITE,&store)!=ESP_OK)return false;
    std::array<std::uint8_t,56> check{};std::size_t got=size;
    const bool ok=nvs_set_blob(store,name,bytes,size)==ESP_OK&&nvs_commit(store)==ESP_OK&&nvs_get_blob(store,name,check.data(),&got)==ESP_OK&&got==size&&std::equal(bytes,bytes+size,check.begin());
    nvs_close(store);clear(check.data(),check.size());return ok;
}
bool Settings::saveCurve(const CurveView &view){if(!view.revision||!view.curve.valid())return false;const auto b=encode(view);return save("curve-v1",b.data(),b.size());}
bool Settings::provision(Credentials &access,CurveView &view){
    bool hasAccess=false,hasCurve=false;if(!load(access,hasAccess,view,hasCurve))return false;
    // All existing records have been validated before the first possible write.
    if(!hasAccess){esp_fill_random(access.device.data(),access.device.size());esp_fill_random(access.secret.data(),access.secret.size());if(!valid(access))return false;auto b=encode(access);const bool ok=save("access-v1",b.data(),b.size());clear(b.data(),b.size());if(!ok)return false;}
    if(!hasCurve&&!saveCurve(view))return false;
    bool reAccess=false,reCurve=false;
    if(!load(access,reAccess,view,reCurve)||!reAccess||!reCurve)return false;
    return view.durable;
}
bool Runtime::start(){if(started_)return true;if(!hasAccess_||!view_.durable)return false;started_=bearer_.begin(access_.device);if(!started_)fault_=3;return started_;}
bool Runtime::begin(Core &core,bool radioReady,std::uint32_t now){
    loaded_=settings_.load(access_,hasAccess_,view_,hasCurve_);
    if(!loaded_){fault_=1;if(settings_.curveTrusted()){if(core.setCurve(view_.curve,now))appliedRevision_=view_.revision;}else core.fail(now);return false;}
    if(!core.setCurve(view_.curve,now)){fault_=2;return false;}
    appliedRevision_=view_.revision;
    if(radioReady&&hasAccess_&&view_.durable)return start();
    return true;
}
bool Runtime::provision(Core &core,bool radioReady,std::uint32_t now){
    if(!radioReady||!loaded_||core.fault())return false;
    const auto oldDigest=view_.digest;
    if(!settings_.provision(access_,view_)){fault_=1;return false;}
    hasAccess_=hasCurve_=true;
    if((appliedRevision_!=view_.revision||oldDigest!=view_.digest)&&!core.setCurve(view_.curve,nowMs())){appliedRevision_=0;fault_=2;return false;}
    appliedRevision_=view_.revision;
    (void)now;return start();
}
void Runtime::revoke(RevokeReason reason){bearer_.revokeReason(epoch_,reason);session_.reset();assembler_.reset();epoch_=0;}
void Runtime::process(Core &core){
    if(!started_)return;
    if(bearer_.fault()){fault_=3;session_.reset();return;}
    if(epoch_&&!bearer_.current(epoch_)){session_.reset();assembler_.reset();epoch_=0;}
    Ingress event;
    for(unsigned n=0;n<16&&bearer_.receive(event);++n){
        const auto now=nowMs();
        if(event.kind==Ingress::Kind::Disconnected){if(event.epoch==epoch_){session_.reset();assembler_.reset();epoch_=0;}continue;}
        if(!bearer_.current(event.epoch))continue;
        Message reply{};
        if(event.kind==Ingress::Kind::Subscribed){
            epoch_=event.epoch;assembler_.reset();Token link{};esp_fill_random(link.data(),link.size());sessionAt_=now;fault_=0;
            if(!session_.begin(access_,core.boot(),link,now,view_,reply)||!bearer_.send(epoch_,reply)){fault_=4;revoke(RevokeReason::Challenge);}continue;
        }
        if(event.epoch!=epoch_)continue;
        if(now-event.at>=2000U){fault_=4;revoke(RevokeReason::IngressAge);continue;}
        Message message{};const auto status=assembler_.accept(event.bytes.data(),event.size,event.at,message);
        if(status==FrameResult::Rejected){fault_=4;revoke(RevokeReason::Assembly);continue;}if(status!=FrameResult::Complete)continue;
        if(!session_.authenticated()){
            if(!session_.proof(message,now,view_,reply)||!bearer_.send(epoch_,reply)){fault_=4;revoke(RevokeReason::Proof);}continue;
        }
        bool apply=false;ProximityCurve curve;std::uint32_t request=0;
        if(!session_.command(message,now,view_,apply,curve,request,reply)){fault_=4;revoke(RevokeReason::Command);continue;}
        if(apply){
            if(core.fault()||(applied_&&now-lastApply_<250U)){if(!session_.result(request,4,view_,reply)){fault_=4;revoke();continue;}}
            else{CurveView next=view_;next.curve=curve;++next.revision;
                if(!digest(crypto_,next.curve,next.digest)){fault_=4;revoke();continue;}
                if(!settings_.saveCurve(next)){fault_=2;view_.durable=false;if(!session_.result(request,5,view_,reply)){revoke();continue;}}
                else{view_=next;view_.durable=true;lastApply_=nowMs();applied_=true;
                    const bool accepted=core.setCurve(view_.curve,lastApply_);appliedRevision_=accepted?view_.revision:0;if(!accepted)fault_=2;
                    if(!session_.result(request,accepted?0:9,view_,reply)){fault_=4;revoke();continue;}
                }
            }
        }
        if(reply.size&&!bearer_.send(epoch_,reply)){fault_=4;revoke();}
    }
    const auto now=nowMs();
    if(epoch_&&!session_.authenticated()&&now-sessionAt_>=5000U){fault_=4;revoke(RevokeReason::ProofTimeout);}
 }
void Runtime::publish(const Telemetry &base){
    const auto now=base.deviceMs;
    if(epoch_&&session_.authenticated()&&bearer_.current(epoch_)&&now-lastStatus_>=500U){
        lastStatus_=now;auto state=base;state.bleFault=fault_;if(view_.durable)state.flags|=1U<<10;
        Message out;if(!session_.telemetry(state,view_,out)||!bearer_.send(epoch_,out)){fault_=4;revoke(RevokeReason::Telemetry);}
    }
}
}}}
