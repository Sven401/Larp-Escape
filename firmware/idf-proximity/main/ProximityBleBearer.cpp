#include "ProximityBleBearer.h"
#include <esp_timer.h>
#include "host/ble_hs_mbuf.h"
#include "host/ble_att.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
namespace larp { namespace proximity { namespace ble {
namespace {
std::uint32_t now(){return static_cast<std::uint32_t>(esp_timer_get_time()/1000);}
std::size_t frameBudget(std::uint16_t handle){const auto mtu=ble_att_mtu(handle);return std::min<std::size_t>(244,mtu>=23?mtu-3:20);}
// BLE_UUID128_INIT uses little-endian UUID bytes.
void increment(std::uint32_t &value){if(value!=UINT32_MAX)++value;}
const ble_uuid128_t serviceId=BLE_UUID128_INIT(0x01,0xb1,0xc4,0x78,0xa4,0x35,0xf4,0xa7,0x5a,0x4d,0x50,0x2f,0x01,0x01,0x79,0x6c);
const ble_uuid128_t inputId=BLE_UUID128_INIT(0x01,0xb1,0xc4,0x78,0xa4,0x35,0xf4,0xa7,0x5a,0x4d,0x50,0x2f,0x02,0x01,0x79,0x6c);
const ble_uuid128_t outputId=BLE_UUID128_INIT(0x01,0xb1,0xc4,0x78,0xa4,0x35,0xf4,0xa7,0x5a,0x4d,0x50,0x2f,0x03,0x01,0x79,0x6c);
}
Bearer *Bearer::selected_=nullptr;
Bearer::Link Bearer::snapshot()const{portENTER_CRITICAL(&lock_);const auto value=link_;portEXIT_CRITICAL(&lock_);return value;}
bool Bearer::onHost()const{return host_&&xTaskGetCurrentTaskHandle()==host_;}
bool Bearer::ready()const{const auto v=snapshot();return v.ready&&!v.fault;}
bool Bearer::fault()const{return snapshot().fault;}
bool Bearer::current(std::uint32_t epoch)const{const auto v=snapshot();return v.epoch==epoch&&v.connected&&v.subscribed&&!v.abort&&!v.fault;}
Revocation Bearer::diagnostic()const{portENTER_CRITICAL(&lock_);const auto value=last_;portEXIT_CRITICAL(&lock_);return value;}
void Bearer::abort(std::uint32_t epoch,RevokeReason reason,std::uint32_t age,std::uint16_t offset,std::uint8_t type,int sdk){
    const auto at=now();portENTER_CRITICAL(&lock_);
    if(link_.epoch==epoch){if(last_.reason==RevokeReason::None||last_.epoch!=epoch)last_={reason,epoch,at,age,offset,type,sdk};link_.abort=true;}
    portEXIT_CRITICAL(&lock_);
}
ResourceStats Bearer::resources()const{portENTER_CRITICAL(&lock_);const auto value=stats_;portEXIT_CRITICAL(&lock_);return value;}
void Bearer::recordAccess(std::uint16_t size,int code){
    const auto at=now();const auto free=os_msys_num_free();portENTER_CRITICAL(&lock_);
    stats_.epoch=link_.epoch;increment(stats_.accessCalls);if(!code)increment(stats_.accessOk);
    stats_.accessAt=at;stats_.accessSize=size;stats_.accessCode=code;
    if(stats_.minMsysFree<0||free<stats_.minMsysFree)stats_.minMsysFree=free;
    portEXIT_CRITICAL(&lock_);
}
void Bearer::recordNotify(int result,int poolFree,int globalFree,bool deferred,bool allocated){
    portENTER_CRITICAL(&lock_);stats_.epoch=link_.epoch;increment(stats_.notifyCalls);
    if(deferred)increment(stats_.notifyReserveDeferrals);else if(!result)increment(stats_.notifyOk);else if(result==BLE_HS_ENOMEM||result==BLE_HS_EBUSY)increment(stats_.notifyBusy);
    if(!allocated)increment(stats_.notifyAllocFailures);
    if(poolFree>=0){stats_.lastNotifyPoolFree=poolFree;if(stats_.minNotifyPoolFree<0||poolFree<stats_.minNotifyPoolFree)stats_.minNotifyPoolFree=poolFree;}
    if(stats_.minMsysFree<0||globalFree<stats_.minMsysFree)stats_.minMsysFree=globalFree;
    portEXIT_CRITICAL(&lock_);
}
void Bearer::revoke(std::uint32_t epoch){abort(epoch,RevokeReason::Owner);}
void Bearer::revokeReason(std::uint32_t epoch,RevokeReason reason){abort(epoch,reason);}
void Bearer::fail(){abort(snapshot().epoch,RevokeReason::HostFault);portENTER_CRITICAL(&lock_);link_.fault=true;link_.abort=true;link_.ready=false;portEXIT_CRITICAL(&lock_);}
bool Bearer::event(const Ingress &value){if(xQueueSend(incoming_,&value,0)==pdTRUE)return true;abort(value.epoch,RevokeReason::IngressFull);return false;}
bool Bearer::receive(Ingress &value){return incoming_&&xQueueReceive(incoming_,&value,0)==pdTRUE;}
bool Bearer::send(std::uint32_t epoch,const Message &message){
    if(!message.size||message.size>message.bytes.size()||!current(epoch))return false;
    Egress e;e.epoch=epoch;e.message=message;e.createdAt=message.bytes[1]==0x84&&message.size==145?get32(message.bytes.data()+24):now();
    if(xQueueSend(outgoing_,&e,0)==pdTRUE)return true;
    abort(epoch,RevokeReason::EgressFull);return false;
}
bool Bearer::begin(const Token &device){
    if(attempted_||selected_||!nonzero(device.data(),device.size()))return false;
    attempted_=true;selected_=this;device_=device;
    incoming_=xQueueCreate(16,sizeof(Ingress));outgoing_=xQueueCreate(4,sizeof(Egress));
    if(!incoming_||!outgoing_||nimble_port_init()!=0){fail();return false;}
    ble_hs_cfg.sync_cb=sync;ble_hs_cfg.reset_cb=reset;
    // This isolated Dev service authorizes its own HMAC protocol. It does not
    // weaken or instantiate the separate SC/TLS NimblePeerSource product path.
    ble_hs_cfg.sm_bonding=0;ble_hs_cfg.sm_mitm=0;
    ble_svc_gap_init();ble_svc_gatt_init();
    if(ble_svc_gap_device_name_set("Larp NG")){fail();return false;}
    characteristics_[0].uuid=&inputId.u;characteristics_[0].access_cb=access;characteristics_[0].arg=this;characteristics_[0].val_handle=&input_;characteristics_[0].flags=BLE_GATT_CHR_F_WRITE;
    characteristics_[1].uuid=&outputId.u;characteristics_[1].access_cb=access;characteristics_[1].arg=this;characteristics_[1].val_handle=&output_;characteristics_[1].flags=BLE_GATT_CHR_F_NOTIFY;
    services_[0].type=BLE_GATT_SVC_TYPE_PRIMARY;services_[0].uuid=&serviceId.u;services_[0].characteristics=characteristics_;
    if(ble_gatts_count_cfg(services_)||ble_gatts_add_svcs(services_)||ble_npl_callout_init(&timer_,nimble_port_get_dflt_eventq(),tick,this)){fail();return false;}
    const auto launched=xTaskCreateStaticPinnedToCore(run,"ng-ble",sizeof(hostStack_),this,5,hostStack_.data(),&hostStorage_,1);
    if(!launched){fail();return false;}return true;
}
void Bearer::run(void *arg){auto &self=*static_cast<Bearer*>(arg);self.host_=xTaskGetCurrentTaskHandle();nimble_port_run();self.fail();vTaskDelete(nullptr);}
void Bearer::sync(){auto *self=selected_;if(!self||!self->onHost()||!self->input_||!self->output_||ble_hs_id_infer_auto(0,&self->addressType_)){if(self)self->fail();return;}self->advertise();if(ble_npl_callout_reset(&self->timer_,ble_npl_time_ms_to_ticks32(8)))self->fail();}
void Bearer::reset(int){if(selected_)selected_->fail();}
void Bearer::advertise(){
    if(!onHost()||snapshot().fault)return;
    ble_hs_adv_fields fields{};fields.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;fields.uuids128=&serviceId;fields.num_uuids128=1;fields.uuids128_is_complete=1;
    std::array<std::uint8_t,18> identity{};identity[0]=identity[1]=0xff;std::copy(device_.begin(),device_.end(),identity.begin()+2);
    ble_hs_adv_fields scan{};const char name[]="Larp NG";scan.name=reinterpret_cast<const std::uint8_t*>(name);scan.name_len=sizeof(name)-1;scan.name_is_complete=1;scan.mfg_data=identity.data();scan.mfg_data_len=identity.size();
    ble_gap_adv_params params{};params.conn_mode=BLE_GAP_CONN_MODE_UND;params.disc_mode=BLE_GAP_DISC_MODE_GEN;
    if(ble_gap_adv_set_fields(&fields)||ble_gap_adv_rsp_set_fields(&scan)||ble_gap_adv_start(addressType_,nullptr,BLE_HS_FOREVER,&params,gap,this)){fail();return;}
    portENTER_CRITICAL(&lock_);link_.ready=true;portEXIT_CRITICAL(&lock_);
}
int Bearer::access(std::uint16_t connection,std::uint16_t attribute,ble_gatt_access_ctxt *context,void *arg){
    auto &self=*static_cast<Bearer*>(arg);const auto v=self.snapshot();
    if(!self.onHost()||!context||context->op!=BLE_GATT_ACCESS_OP_WRITE_CHR||attribute!=self.input_||v.handle!=connection||!self.current(v.epoch))return BLE_ATT_ERR_UNLIKELY;
    const auto size=OS_MBUF_PKTLEN(context->om);if(size<=8||size>frameBudget(connection)){self.recordAccess(size,BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN);return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;}
    Ingress e;e.kind=Ingress::Kind::Fragment;e.epoch=v.epoch;e.at=now();e.size=size;std::uint16_t copied=0;
    const int copiedStatus=ble_hs_mbuf_to_flat(context->om,e.bytes.data(),e.bytes.size(),&copied);
    if(copiedStatus||copied!=size){self.recordAccess(size,BLE_ATT_ERR_INSUFFICIENT_RES);self.abort(v.epoch,RevokeReason::AccessCopy,0,0,0,copiedStatus?copiedStatus:-1);return BLE_ATT_ERR_INSUFFICIENT_RES;}
    if(!self.event(e)){self.recordAccess(size,BLE_ATT_ERR_INSUFFICIENT_RES);return BLE_ATT_ERR_INSUFFICIENT_RES;}
    self.recordAccess(size,0);return 0;
}
int Bearer::gap(ble_gap_event *e,void *arg){
    auto &self=*static_cast<Bearer*>(arg);if(!e||!self.onHost())return 0;
    if(e->type==BLE_GAP_EVENT_CONNECT){
        if(e->connect.status){self.advertise();return 0;}
        auto v=self.snapshot();if(v.connected||v.epoch==UINT32_MAX){ble_gap_terminate(e->connect.conn_handle,BLE_ERR_REM_USER_CONN_TERM);if(v.epoch==UINT32_MAX)self.fail();return 0;}
        portENTER_CRITICAL(&self.lock_);++self.link_.epoch;self.link_.handle=e->connect.conn_handle;self.link_.connected=true;self.link_.subscribed=false;self.link_.abort=false;self.stats_={};self.stats_.epoch=self.link_.epoch;portEXIT_CRITICAL(&self.lock_);
        self.transfer_=1;self.offset_=0;self.sending_={};self.busy_=false;return 0;
    }
    if(e->type==BLE_GAP_EVENT_DISCONNECT){
        const auto v=self.snapshot();if(v.handle!=e->disconnect.conn.conn_handle)return 0;
        portENTER_CRITICAL(&self.lock_);self.link_.connected=false;self.link_.subscribed=false;self.link_.abort=false;portEXIT_CRITICAL(&self.lock_);
        Ingress in;in.kind=Ingress::Kind::Disconnected;in.epoch=v.epoch;self.event(in);self.sending_={};self.advertise();return 0;
    }
    if(e->type==BLE_GAP_EVENT_SUBSCRIBE&&e->subscribe.attr_handle==self.output_){
        const auto v=self.snapshot();if(v.handle!=e->subscribe.conn_handle)return 0;
        if(v.subscribed&&e->subscribe.cur_notify)return 0;
        portENTER_CRITICAL(&self.lock_);self.link_.subscribed=e->subscribe.cur_notify!=0;portEXIT_CRITICAL(&self.lock_);
        if(!e->subscribe.cur_notify)self.abort(v.epoch,RevokeReason::Unsubscribe);
        if(e->subscribe.cur_notify){Ingress in;in.kind=Ingress::Kind::Subscribed;in.epoch=v.epoch;self.event(in);}return 0;
    }
    if(e->type==BLE_GAP_EVENT_ADV_COMPLETE&&!self.snapshot().connected)self.advertise();
    return 0;
}
void Bearer::tick(ble_npl_event *event){auto *self=static_cast<Bearer*>(ble_npl_event_get_arg(event));if(self)self->poll();}
void Bearer::poll(){
    if(!onHost())return;
    const auto v=snapshot();
    if(v.connected&&(v.abort||v.fault)){ble_gap_terminate(v.handle,BLE_ERR_REM_USER_CONN_TERM);sending_={};}
    else if(current(v.epoch)){
        if(!sending_.size){Egress next;for(unsigned n=0;n<4&&xQueueReceive(outgoing_,&next,0)==pdTRUE;++n)if(next.epoch==v.epoch){sending_=next.message;sendingEpoch_=next.epoch;sendingAt_=next.createdAt;offset_=0;break;}}
        if(sending_.size){
            if(sendingEpoch_!=v.epoch){sending_={};}
            else if(now()-sendingAt_>=(sending_.bytes[1]==0x84?1000U:2000U)){abort(v.epoch,RevokeReason::EgressAge,now()-sendingAt_,offset_,sending_.bytes[1]);sending_={};}
            else{std::array<std::uint8_t,244> bytes{};std::size_t size=0;
                if(!encodeFrame(sending_,transfer_,offset_,bytes,size,frameBudget(v.handle))){abort(v.epoch,RevokeReason::FrameEncode,now()-sendingAt_,offset_,sending_.bytes[1]);}
                else{auto *buffer=ble_hs_mbuf_from_flat(bytes.data(),size);
                    // Sample the actual pool before notify_custom consumes this buffer.
                    const int poolFree=buffer&&buffer->om_omp&&buffer->om_omp->omp_pool?buffer->om_omp->omp_pool->mp_num_free:-1;
                    const int globalFree=os_msys_num_free();
                    // Keep this pool available for inbound ATT requests and their responses.
                    // This is cooperative backpressure, not a cross-core atomic reservation.
                    const bool allocated=buffer!=nullptr;
                    const bool deferred=allocated&&poolFree>=0&&poolFree<4;
                    int result=BLE_HS_ENOMEM;
                    if(allocated&&poolFree<0){os_mbuf_free_chain(buffer);result=BLE_HS_EINVAL;}
                    else if(deferred){os_mbuf_free_chain(buffer);result=BLE_HS_EBUSY;}
                    else if(buffer)result=ble_gatts_notify_custom(v.handle,output_,buffer);
                    recordNotify(result,poolFree,globalFree,deferred,allocated);
                    // notify_custom consumes the mbuf on every return, including errors.
                    if(!result){busy_=false;offset_+=size-8;if(offset_==sending_.size){sending_={};offset_=0;if(transfer_==UINT16_MAX)abort(v.epoch,RevokeReason::TransferExhausted);else ++transfer_;}}
                    else if(result==BLE_HS_ENOMEM||result==BLE_HS_EBUSY){if(!busy_){busy_=true;busyAt_=now();}else if(now()-busyAt_>=2000U)abort(v.epoch,RevokeReason::NotifyBusy,now()-sendingAt_,offset_,sending_.bytes[1],result);}
                    else abort(v.epoch,RevokeReason::NotifyError,now()-sendingAt_,offset_,sending_.bytes[1],result);
                }
            }
        }
    }
    if(ble_npl_callout_reset(&timer_,ble_npl_time_ms_to_ticks32(8)))fail();
}
}}}
