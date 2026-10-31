#include "ProximityBleBearer.h"
#include <deque>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdio>
using namespace larp::proximity;
unsigned assertions=0;void check(bool ok,const char*text){++assertions;if(!ok)throw std::runtime_error(text);}
namespace {
int critical=0;std::uint32_t clockValue=1000;TaskHandle_t task=reinterpret_cast<void*>(1),host=reinterpret_cast<void*>(2);
void(*entry)(void*)=nullptr;void*argument=nullptr;int(*gapFn)(ble_gap_event*,void*)=nullptr;void*gapArg=nullptr;
ble_npl_callout*timer=nullptr;const ble_gatt_svc_def*definitions=nullptr;
int notifyError=0,registrationError=0,timerError=0,terminated=-1,buffers=0,notifications=0;
bool allocationFailure=false,returnHost=false;
std::vector<std::vector<std::uint8_t>> sent;std::vector<unsigned> sentAt;
struct HostYield{};
struct Queue{unsigned capacity,width;std::deque<std::vector<std::uint8_t>> values;};
void outside(){check(!critical,"SDK/Queue call under snapshot spinlock");}
}
extern "C" {
ble_hs_config ble_hs_cfg{};
void fake_enter(){++critical;}void fake_exit(){--critical;}
QueueHandle_t xQueueCreate(unsigned capacity,unsigned width){outside();return new Queue{capacity,width,{}};}
int xQueueSend(QueueHandle_t handle,const void*p,int ticks){outside();check(!ticks,"blocking BLE queue write");auto&q=*static_cast<Queue*>(handle);if(q.values.size()>=q.capacity)return 0;auto*b=static_cast<const std::uint8_t*>(p);q.values.emplace_back(b,b+q.width);return pdTRUE;}
int xQueueReceive(QueueHandle_t handle,void*p,int ticks){outside();check(!ticks,"blocking BLE queue read");auto&q=*static_cast<Queue*>(handle);if(q.values.empty())return 0;std::memcpy(p,q.values.front().data(),q.width);q.values.pop_front();return pdTRUE;}
TaskHandle_t xTaskGetCurrentTaskHandle(){outside();return task;}
TaskHandle_t xTaskCreateStaticPinnedToCore(void(*fn)(void*),const char*,uint32_t bytes,void*arg,unsigned,StackType_t*stack,StaticTask_t*,int core){outside();check(bytes==8192&&core==1&&reinterpret_cast<std::uintptr_t>(stack)%16==0,"NimBLE stack bytes/core/alignment");entry=fn;argument=arg;return host;}
void vTaskDelete(void*){}
int64_t esp_timer_get_time(){return std::int64_t(clockValue)*1000;}
int nimble_port_init(){outside();return 0;}
void nimble_port_run(){ble_hs_cfg.sync_cb();if(!returnHost)throw HostYield{};}
void*nimble_port_get_dflt_eventq(){return nullptr;}
int ble_gap_terminate(uint16_t handle,uint8_t){outside();terminated=handle;return 0;}
int ble_gap_adv_set_fields(const ble_hs_adv_fields*f){outside();check(f->num_uuids128==1,"service not advertised");return 0;}
int ble_gap_adv_rsp_set_fields(const ble_hs_adv_fields*f){outside();check(f->mfg_data_len==18&&f->mfg_data[0]==255&&f->mfg_data[1]==255,"device id advertising format");return 0;}
int ble_gap_adv_start(uint8_t,const void*,int,const ble_gap_adv_params*,int(*fn)(ble_gap_event*,void*),void*arg){outside();gapFn=fn;gapArg=arg;return 0;}
int ble_hs_id_infer_auto(int,uint8_t*out){outside();*out=0;return 0;}
void ble_svc_gap_init(){}void ble_svc_gatt_init(){}int ble_svc_gap_device_name_set(const char*){return 0;}
int ble_gatts_count_cfg(const ble_gatt_svc_def*){outside();return registrationError;}
int ble_gatts_add_svcs(const ble_gatt_svc_def*value){outside();definitions=value;*value[0].characteristics[0].val_handle=101;*value[0].characteristics[1].val_handle=102;return 0;}
int ble_npl_callout_init(ble_npl_callout*out,void*,void(*fn)(ble_npl_event*),void*arg){outside();timer=out;out->callback=fn;out->event.arg=arg;return timerError;}
int ble_npl_callout_reset(ble_npl_callout*,uint32_t){outside();return timerError;}
uint32_t ble_npl_time_ms_to_ticks32(uint32_t n){return n;}
void*ble_npl_event_get_arg(ble_npl_event*e){return e->arg;}
int ble_hs_mbuf_to_flat(const os_mbuf*source,void*out,uint16_t capacity,uint16_t*size){outside();if(source->len>capacity)return 1;*size=source->len;std::memcpy(out,source->bytes,source->len);return 0;}
os_mbuf*ble_hs_mbuf_from_flat(const void*p,uint16_t size){outside();if(allocationFailure)return nullptr;auto*buffer=new os_mbuf;buffer->len=size;std::memcpy(buffer->bytes,p,size);++buffers;return buffer;}
int ble_gatts_notify_custom(uint16_t,uint16_t,os_mbuf*buffer){outside();++notifications;if(!notifyError){sent.emplace_back(buffer->bytes,buffer->bytes+buffer->len);sentAt.push_back(clockValue);}delete buffer;--buffers;return notifyError;}
}
struct Fixture {
    ble::Bearer bearer;std::uint32_t epoch=0;std::uint16_t connection=7;
    void begin(){Token device;device.fill(1);check(bearer.begin(device),"begin failed");task=host;try{entry(argument);}catch(const HostYield&){}task=reinterpret_cast<void*>(1);check(bearer.ready(),"sync not ready");}
    void event(ble_gap_event &e){task=host;check(gapFn(&e,gapArg)==0,"GAP event failed");task=reinterpret_cast<void*>(1);}
    void connect(){ble_gap_event e{};e.type=BLE_GAP_EVENT_CONNECT;e.connect.conn_handle=connection;event(e);e={};e.type=BLE_GAP_EVENT_SUBSCRIBE;e.subscribe.conn_handle=connection;e.subscribe.attr_handle=102;e.subscribe.cur_notify=1;event(e);ble::Ingress in;bool found=false;while(bearer.receive(in))if(in.kind==ble::Ingress::Kind::Subscribed){epoch=in.epoch;found=true;}check(found&&bearer.current(epoch),"no current subscribed link");}
    void tick(unsigned now){clockValue=now;task=host;timer->callback(&timer->event);task=reinterpret_cast<void*>(1);}
    int write(os_mbuf &m){auto &c=definitions[0].characteristics[0];ble_gatt_access_ctxt context{};context.op=BLE_GATT_ACCESS_OP_WRITE_CHR;context.om=&m;task=host;const auto code=c.access_cb(connection,101,&context,c.arg);task=reinterpret_cast<void*>(1);return code;}
    ble::Message telemetry(){ble::Message m;m.size=145;for(unsigned i=0;i<m.size;++i)m.bytes[i]=i;m.bytes[0]=1;m.bytes[1]=0x84;put32(m.bytes.data()+24,clockValue);return m;}
};
void roundtrip(unsigned step=8){Fixture f;f.begin();f.connect();const auto message=f.telemetry();check(f.bearer.send(f.epoch,message),"egress rejected");for(unsigned i=0;i<13;++i)f.tick(1000+i*step);check(sent.size()==13&&!buffers,"notification/mbuf count");ble::Assembler assembler;ble::Message result;for(unsigned i=0;i<sent.size();++i){const auto status=assembler.accept(sent[i].data(),sent[i].size(),sentAt[i],result);check(status==(i+1==sent.size()?ble::FrameResult::Complete:ble::FrameResult::Receiving),"notification framing");}check(result.size==145&&std::equal(result.bytes.begin(),result.bytes.begin()+145,message.bytes.begin()),"native bearer truncated145");}
void stale(bool partial){Fixture f;f.begin();f.connect();check(f.bearer.send(f.epoch,f.telemetry()),"stale setup send");if(partial){f.tick(1999);check(sent.size()==1,"partial setup");}f.tick(2000);check(!f.bearer.current(f.epoch)&&sent.size()==(partial?1U:0U),"expired telemetry delivered as fresh");const auto d=f.bearer.diagnostic();check(d.reason==ble::RevokeReason::EgressAge&&d.age==1000&&d.offset==(partial?12:0)&&d.type==0x84&&d.epoch==f.epoch,"expiry diagnostic");f.bearer.revoke(f.epoch);check(f.bearer.diagnostic().reason==d.reason,"first cause overwritten");f.tick(2008);check(terminated==7&&!buffers,"expired link not terminated/buffer leak");}
void busy(){Fixture f;f.begin();f.connect();check(f.bearer.send(f.epoch,f.telemetry()),"busy send");notifyError=BLE_HS_ENOMEM;f.tick(1100);check(!buffers&&!sent.size(),"error mbuf not consumed");notifyError=0;f.tick(1200);check(sent.size()==1,"busy retry missing");notifyError=BLE_HS_EBUSY;f.tick(1999);f.tick(2000);check(!f.bearer.current(f.epoch)&&!buffers&&sent.size()==1,"partial successes extended absolute budget");}
void overflow(bool outgoing){Fixture f;f.begin();f.connect();if(outgoing){auto m=f.telemetry();for(unsigned i=0;i<4;++i)check(f.bearer.send(f.epoch,m),"egress capacity");check(!f.bearer.send(f.epoch,m),"egress overflow hidden");}else{os_mbuf m{};m.len=9;for(unsigned i=0;i<16;++i)check(f.write(m)==0,"ingress capacity");check(f.write(m)==BLE_ATT_ERR_INSUFFICIENT_RES,"ingress overflow not rejected");}check(!f.bearer.current(f.epoch),"overflow did not fence BLE");check(f.bearer.diagnostic().reason==(outgoing?ble::RevokeReason::EgressFull:ble::RevokeReason::IngressFull),"overflow reason");f.tick(1008);check(terminated==7,"overflow not terminated");}
void notifyFailure(){Fixture f;f.begin();f.connect();check(f.bearer.send(f.epoch,f.telemetry()),"error setup");os_mbuf queued{};queued.len=9;for(unsigned i=0;i<16;++i)check(f.write(queued)==0,"diagnostic queue fill");notifyError=123;f.tick(1045);const auto d=f.bearer.diagnostic();check(d.reason==ble::RevokeReason::NotifyError&&d.sdk==123&&d.age==45&&d.offset==0&&d.type==0x84,"SDK error diagnostic");check(!buffers&&!f.bearer.current(f.epoch),"error not fenced");
    // A disconnect callback must not overwrite the cause when its own ingress event overflows.
    auto e=ble_gap_event{};e.type=BLE_GAP_EVENT_DISCONNECT;e.disconnect.conn.conn_handle=f.connection;
    f.event(e);
    check(f.bearer.diagnostic().reason==ble::RevokeReason::NotifyError&&f.bearer.diagnostic().sdk==123,"disconnect ingress overflow overwrote first cause");
    ble::Ingress discarded;while(f.bearer.receive(discarded)){}
    f.connection=8;f.connect();check(f.bearer.diagnostic().reason==ble::RevokeReason::NotifyError,"reconnect erased last cause");
}
void copy(){Fixture f;f.begin();f.connect();os_mbuf m{};m.len=9;m.bytes[8]=42;clockValue=1234;check(f.write(m)==0,"valid write");m.bytes[8]=7;ble::Ingress in;check(f.bearer.receive(in)&&in.at==1234&&in.bytes[8]==42&&in.size==9,"callback borrowed bytes/time");m.len=21;check(f.write(m)==BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN,"oversize write");}
void epochs(){Fixture f;f.begin();f.connect();check(f.bearer.send(f.epoch,f.telemetry()),"old queue setup");const auto old=f.epoch;ble_gap_event e{};e.type=BLE_GAP_EVENT_DISCONNECT;e.disconnect.conn.conn_handle=f.connection;f.event(e);f.connection=8;f.connect();check(f.epoch!=old&&!f.bearer.send(old,f.telemetry()),"old epoch accepted");auto next=f.telemetry();next.bytes[4]=99;check(f.bearer.send(f.epoch,next),"new epoch send");f.tick(1008);check(sent.size()==1&&sent[0][12]==99,"old egress reached new link");}
int main(int argc,char**argv){try{check(argc==2,"scenario missing");const std::string mode=argv[1];if(mode=="roundtrip")roundtrip();else if(mode=="interval45")roundtrip(45);else if(mode=="notify-error")notifyFailure();else if(mode=="stale-queued")stale(false);else if(mode=="stale-partial")stale(true);else if(mode=="busy")busy();else if(mode=="full-in")overflow(false);else if(mode=="full-out")overflow(true);else if(mode=="copy")copy();else if(mode=="epochs")epochs();else throw std::runtime_error("unknown scenario");std::printf("PASS BLE bearer %s: %u assertions\n",argv[1],assertions);return 0;}catch(const std::exception&e){std::fprintf(stderr,"FAIL BLE bearer %s\n",e.what());return 1;}}
