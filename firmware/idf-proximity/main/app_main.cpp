#include "ProximityCore.h"
#ifdef LARP_PROXIMITY_BLE
#include "ProximityBleRuntime.h"
#include "ProximityBleBearer.h"
#endif
#include <atomic>
#include <cstdio>
#include <cstdarg>
#include <psa/crypto.h>
#include <driver/rmt_tx.h>
#include <driver/rmt_encoder.h>
#include <driver/usb_serial_jtag.h>
#include <esp_event.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_now.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <nvs.h>
#include <nvs_flash.h>

namespace {
using namespace larp::proximity;
#ifdef LARP_PROXIMITY_BLE
ble::Bearer bleBearer; // BSS: dedicated 8192-byte NimBLE host stack.
ble::Runtime bleControl(bleBearer);
#endif
std::uint32_t nowMs(){return static_cast<std::uint32_t>(esp_timer_get_time()/1000);}
void wipe(void *memory,std::size_t n){volatile auto *p=static_cast<volatile std::uint8_t*>(memory);while(n--)*p++=0;}
void report(const char *format,...) __attribute__((format(printf,1,2)));
void report(const char *format,...) {
    char line[512];va_list args;va_start(args,format);const int n=vsnprintf(line,sizeof(line),format,args);va_end(args);
    if(n>0&&n<static_cast<int>(sizeof(line)))usb_serial_jtag_write_bytes(line,n,pdMS_TO_TICKS(20));
}
class PsaCrypto final:public Crypto {
    mbedtls_svc_key_id_t key_=0;bool failed_=false;
public:
    bool load(const Secret &secret) {
        if(psa_crypto_init()!=PSA_SUCCESS)return false;
        psa_key_attributes_t attr=PSA_KEY_ATTRIBUTES_INIT;
        psa_set_key_type(&attr,PSA_KEY_TYPE_HMAC);psa_set_key_bits(&attr,256);
        psa_set_key_usage_flags(&attr,PSA_KEY_USAGE_SIGN_MESSAGE);
        psa_set_key_algorithm(&attr,PSA_ALG_HMAC(PSA_ALG_SHA_256));
        const auto result=psa_import_key(&attr,secret.data(),secret.size(),&key_);
        psa_reset_key_attributes(&attr);return result==PSA_SUCCESS;
    }
    bool mac(const std::uint8_t *data,std::size_t n,Secret &out)override {
        std::size_t length=0;
        const bool ok=key_&&psa_mac_compute(key_,PSA_ALG_HMAC(PSA_ALG_SHA_256),data,n,out.data(),out.size(),&length)==PSA_SUCCESS&&length==32;
        if(!ok)failed_=true;
        return ok;
    }
    bool failed()const{return failed_;}
};
class Storage {
    nvs_handle_t handle_=0;bool ready_=false;
public:
    // Missing configuration is a valid dark setup state; any other error faults.
    bool begin(Config &config,bool &paired) {
        if(nvs_flash_init()!=ESP_OK)return false; // never erase/reformat on failure
        if(nvs_open("prox_lab",NVS_READWRITE,&handle_)!=ESP_OK)return false;
        ready_=true;std::size_t size=0;auto result=nvs_get_blob(handle_,"pair-v1",nullptr,&size);
        if(result==ESP_ERR_NVS_NOT_FOUND){paired=false;return true;}
        if(result!=ESP_OK||size!=ConfigBytes{}.size())return false;
        ConfigBytes bytes{};result=nvs_get_blob(handle_,"pair-v1",bytes.data(),&size);
        const bool ok=result==ESP_OK&&decodeConfig(bytes.data(),size,config);wipe(bytes.data(),bytes.size());
        paired=ok;return ok;
    }
    bool save(const Config &config) {
        if(!ready_||!validConfig(config))return false;
        auto bytes=encodeConfig(config);ConfigBytes read{};
        bool ok=nvs_set_blob(handle_,"pair-v1",bytes.data(),bytes.size())==ESP_OK&&nvs_commit(handle_)==ESP_OK;
        std::size_t size=read.size();
        ok=ok&&nvs_get_blob(handle_,"pair-v1",read.data(),&size)==ESP_OK&&size==read.size()&&read==bytes;
        wipe(bytes.data(),bytes.size());wipe(read.data(),read.size());return ok;
    }
};
class Output {
    rmt_channel_handle_t channel_=nullptr;rmt_encoder_handle_t encoder_=nullptr;
    std::array<rmt_symbol_word_t,25> symbols_{};
    struct Completion {std::atomic<bool> done{false};std::atomic<std::uint32_t> at{0};};
    std::array<Completion,2> done_{};unsigned completionSlot_=0;
    bool pending_=false,known_=false,fault_=false,recovered_=false;
    std::uint32_t at_=0,confirmedAt_=0,submittedCurve_=0,confirmedCurve_=0;larp::led::Pixel submitted_{},confirmed_{};
    static bool done(rmt_channel_handle_t,const rmt_tx_done_event_data_t*,void *user) {
        auto &cell=*static_cast<Completion*>(user);cell.at.store(nowMs(),std::memory_order_relaxed);cell.done.store(true,std::memory_order_release);return false;
    }
    void error(){fault_=true;known_=false;pending_=false;}
    bool configure() {
        static_assert(ATOMIC_BOOL_LOCK_FREE == 2,"RMT ISR completion needs lock-free bool");
        rmt_tx_channel_config_t config{};config.clk_src=RMT_CLK_SRC_DEFAULT;config.gpio_num=GPIO_NUM_21;
        config.resolution_hz=40000000;config.mem_block_symbols=48;config.trans_queue_depth=1;
        rmt_copy_encoder_config_t encoder{};rmt_tx_event_callbacks_t callbacks{};callbacks.on_trans_done=done;
        if(rmt_new_tx_channel(&config,&channel_)!=ESP_OK||rmt_new_copy_encoder(&encoder,&encoder_)!=ESP_OK||
            rmt_tx_register_event_callbacks(channel_,&callbacks,&done_[completionSlot_])!=ESP_OK||rmt_enable(channel_)!=ESP_OK){error();return false;}
        return true;
    }
public:
    bool begin() { return configure()&&submit({},nowMs()); }
    bool submit(larp::led::Pixel pixel,std::uint32_t now,std::uint32_t curveRevision=0) {
        if(pending_||!channel_||!encoder_||pixel.red>32||pixel.green>32||pixel.blue>32)return false;
        const std::uint8_t bytes[]={pixel.red,pixel.green,pixel.blue};unsigned index=0;
        for(auto byte:bytes)for(int bit=7;bit>=0;--bit){auto &s=symbols_[index++];const bool one=(byte&(1U<<bit))!=0;s={};s.level0=1;s.duration0=one?24:10;s.level1=0;s.duration1=one?26:40;}
        auto &latch=symbols_[24];latch={};latch.duration0=6000;latch.duration1=6000;
        rmt_transmit_config_t tx{};tx.flags.queue_nonblocking=true;
        done_[completionSlot_].done.store(false,std::memory_order_release);submitted_=pixel;submittedCurve_=curveRevision;at_=now;
        if(rmt_transmit(channel_,encoder_,symbols_.data(),sizeof(symbols_),&tx)!=ESP_OK){error();return false;}
        pending_=true;return true;
    }
    void service(std::uint32_t now) {
        if(pending_&&done_[completionSlot_].done.exchange(false,std::memory_order_acquire)) {pending_=false;confirmed_=submitted_;confirmedAt_=done_[completionSlot_].at.load(std::memory_order_relaxed);confirmedCurve_=submittedCurve_;known_=true;}
        if(pending_&&now-at_>=2000U)error();
        if(fault_&&!recovered_&&channel_&&encoder_){
            recovered_=true;
            // A new channel uses a different permanent callback cell. A late
            // completion from the aborted channel cannot confirm recovery black.
            if(rmt_disable(channel_)==ESP_OK&&rmt_del_channel(channel_)==ESP_OK){
                channel_=nullptr;
                if(rmt_del_encoder(encoder_)==ESP_OK){encoder_=nullptr;++completionSlot_;if(configure())submit({},now);}
            }
        }
    }
    bool idle()const{return !pending_;}bool fault()const{return fault_;}
    bool black()const{return known_&&!pending_&&!confirmed_.red&&!confirmed_.green&&!confirmed_.blue;}
    bool known()const{return known_;}larp::led::Pixel confirmed()const{return confirmed_;}
    larp::led::Pixel submitted()const{return submitted_;}
    std::uint32_t submittedAt()const{return at_;}std::uint32_t confirmedAt()const{return confirmedAt_;}
    std::uint32_t submittedCurve()const{return submittedCurve_;}std::uint32_t confirmedCurve()const{return confirmedCurve_;}
};
struct Event { enum class Kind { Rx,Tx,WifiStop } kind=Kind::Rx;Mac source{},destination{};Packet packet{};int rssi=0;std::uint32_t at=0;bool ok=false; };
QueueHandle_t events=nullptr;
std::atomic<bool> dropped{false};
void rx(const esp_now_recv_info_t *info,const std::uint8_t *data,int size) {
    if(!info||!info->src_addr||!info->des_addr||!info->rx_ctrl||!data||size!=120)return;
    Event e;e.at=nowMs();std::copy(info->src_addr,info->src_addr+6,e.source.begin());std::copy(info->des_addr,info->des_addr+6,e.destination.begin());
    e.rssi=info->rx_ctrl->rssi;std::copy(data,data+size,e.packet.begin());
    if(xQueueSend(events,&e,0)!=pdTRUE)dropped.store(true,std::memory_order_release);
}
void tx(const esp_now_send_info_t *info,esp_now_send_status_t status) {
    if(!info||!info->des_addr){dropped.store(true,std::memory_order_release);return;}
    Event e;e.kind=Event::Kind::Tx;e.at=nowMs();e.ok=status==ESP_NOW_SEND_SUCCESS;
    std::copy(info->des_addr,info->des_addr+6,e.destination.begin());
    if(xQueueSend(events,&e,0)!=pdTRUE)dropped.store(true,std::memory_order_release);
}
void wifiEvent(void*,esp_event_base_t,std::int32_t id,void*) {
    if(id!=WIFI_EVENT_STA_STOP)return;
    Event e;e.kind=Event::Kind::WifiStop;
    if(xQueueSend(events,&e,0)!=pdTRUE)dropped.store(true,std::memory_order_release);
}
bool wifi(const Config &config,PsaCrypto &crypto) {
    if(esp_netif_init()!=ESP_OK||esp_event_loop_create_default()!=ESP_OK)return false;
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();cfg.nvs_enable=0;
    if(esp_wifi_init(&cfg)!=ESP_OK||esp_event_handler_register(WIFI_EVENT,WIFI_EVENT_STA_STOP,wifiEvent,nullptr)!=ESP_OK||
        esp_wifi_set_storage(WIFI_STORAGE_RAM)!=ESP_OK||esp_wifi_set_mode(WIFI_MODE_STA)!=ESP_OK||
        esp_wifi_start()!=ESP_OK||esp_wifi_set_channel(1,WIFI_SECOND_CHAN_NONE)!=ESP_OK||esp_wifi_set_ps(WIFI_PS_NONE)!=ESP_OK||
        esp_now_init()!=ESP_OK)return false;
    Secret pmk{},lmk{};
    const char pmkDomain[]="LARP-PROX-PMK/1",lmkDomain[]="LARP-PROX-LMK/1";
    bool ok=crypto.mac(reinterpret_cast<const std::uint8_t*>(pmkDomain),sizeof(pmkDomain)-1,pmk)&&
        crypto.mac(reinterpret_cast<const std::uint8_t*>(lmkDomain),sizeof(lmkDomain)-1,lmk);
    esp_now_peer_info_t peer{};std::copy(config.peer.begin(),config.peer.end(),peer.peer_addr);
    std::copy(lmk.begin(),lmk.begin()+16,peer.lmk);peer.channel=1;peer.ifidx=WIFI_IF_STA;peer.encrypt=true;
    ok=ok&&esp_now_set_pmk(pmk.data())==ESP_OK&&esp_now_add_peer(&peer)==ESP_OK&&
        esp_now_register_send_cb(tx)==ESP_OK&&esp_now_register_recv_cb(rx)==ESP_OK;
    wipe(pmk.data(),pmk.size());wipe(lmk.data(),lmk.size());wipe(peer.lmk,sizeof(peer.lmk));return ok;
}
}
extern "C" void app_main() {
    usb_serial_jtag_driver_config_t usb{};usb.rx_buffer_size=256;usb.tx_buffer_size=1024;
    if(usb_serial_jtag_driver_install(&usb)!=ESP_OK)return;
    Mac local{};Config config;bool paired=false,storageReady=false,radioReady=false,restartRequired=false,durableKnown=true;
    unsigned fault=0; // 1 storage,2 crypto,3 wifi,4 queue,5 missing TX callback,6 RMT,7 core,8 protocol crypto
    Storage storage;PsaCrypto crypto;Output output;
    Token boot{}; // random generated after radio start below, or hardware RNG in dark setup
    esp_fill_random(boot.data(),boot.size());Core core(boot);
    events=xQueueCreate(16,sizeof(Event));
    if(!output.begin())fault=6;
    storageReady=storage.begin(config,paired);if(!storageReady)fault=1;
    // Read base WiFi MAC without enabling unrelated radio or device peripherals.
    if(esp_read_mac(local.data(),ESP_MAC_WIFI_STA)!=ESP_OK||!validMac(local))fault=3;
    Codec codec(crypto,local,config.peer);
    if(!events)fault=4;
    if(!fault&&paired){
        if(config.peer==local)fault=1;
        else if(!crypto.load(config.secret)||!codec.initialize())fault=2;
        else if(!(radioReady=wifi(config,crypto)))fault=3;
    }
    // Boot entropy must be collected after WiFi enables the hardware entropy source.
    if(radioReady){esp_fill_random(boot.data(),boot.size());if(!nonzero(boot.data(),boot.size()))fault=2;core=Core(boot);}
#ifdef LARP_PROXIMITY_BLE
    bleControl.begin(core,radioReady,nowMs());
    if(core.fault()&&!fault)fault=7;
    larp::led::Pixel computed{};
#endif
    bool autoArm=paired&&config.armed&&!fault;
    char line[112]{};unsigned used=0;bool overflow=false;
    TxLane lane;Packet transmit{},response{};bool responsePending=false;std::uint32_t responseAt=0,lastPoll=0,lastFrame=0,rebootAt=0;bool reboot=false;
    auto fail=[&](unsigned reason,std::uint32_t now){if(!fault)fault=reason;core.fail(now);responsePending=false;};
    auto send=[&](TxLane::Kind kind,const Packet &packet,std::uint32_t now){
        if(!lane.begin(kind,config.peer,now))return false;
        transmit=packet;
        if(esp_now_send(config.peer.data(),transmit.data(),transmit.size())!=ESP_OK){lane.complete(config.peer);if(kind==TxLane::Kind::Request)core.rejectSend();fail(3,now);return false;}return true;
    };
    report("{\"type\":\"ready\",\"protocol\":1}\n");
    for(;;){
        auto now=nowMs();output.service(now);
        if(output.fault())fail(6,now);
        if(dropped.load(std::memory_order_acquire))fail(4,now);
        if(lane.expired(now))fail(5,now);
        if(autoArm&&output.black()){autoArm=false;if(!core.arm(now))fail(7,now);}
        // Bound input and erase command/secret buffers after every complete line.
        std::uint8_t input[64];int bytes=usb_serial_jtag_read_bytes(input,sizeof(input),0);
        for(int n=0;n<bytes;++n){const char ch=input[n];if(ch=='\r')continue;
            if(ch!='\n'&&(input[n]<32||input[n]>126)){overflow=true;continue;}
            if(ch!='\n'){if(used<sizeof(line)-1)line[used++]=ch;else overflow=true;continue;}
            now=nowMs();line[used]=0;auto command=overflow?UsbCommand{}:parse(line);wipe(line,sizeof(line));used=0;overflow=false;
            if(command.kind==UsbCommand::Kind::Info){
                const auto rgb=output.confirmed();
                auto boolean=[](bool b){return b?"true":"false";};
                char age[16],strength[16];
                if(core.age(now)==UINT32_MAX)std::strcpy(age,"null");else std::snprintf(age,sizeof(age),"%lu",static_cast<unsigned long>(core.age(now)));
                if(core.fresh(now))std::snprintf(strength,sizeof(strength),"%d",core.filtered());else std::strcpy(strength,"null");
                report("{\"type\":\"info\",\"protocol\":1,\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"peer\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"paired\":%s,\"armed\":%s,\"persistedArmed\":%s,\"durableKnown\":%s,\"fresh\":%s,\"ageMs\":%s,\"filteredRssi\":%s,\"level\":%u,\"fault\":%u,\"radio\":%s,\"txIdle\":%s,\"outputKnown\":%s,\"outputPending\":%s,\"black\":%s,\"rgb\":[%u,%u,%u],\"restartRequired\":%s}\n",
                    local[0],local[1],local[2],local[3],local[4],local[5],config.peer[0],config.peer[1],config.peer[2],config.peer[3],config.peer[4],config.peer[5],
                    boolean(paired),boolean(core.armed()),boolean(paired&&config.armed),boolean(durableKnown),boolean(core.fresh(now)),age,strength,core.level(),fault,boolean(radioReady),boolean(lane.idle()),boolean(output.known()),boolean(!output.idle()),boolean(output.black()),rgb.red,rgb.green,rgb.blue,boolean(restartRequired));
#ifdef LARP_PROXIMITY_BLE
            }else if(command.kind==UsbCommand::Kind::BleAccess){
                ble::Credentials credential;
                if(!bleControl.provision(core,radioReady,now)||!bleControl.credential(credential))report("{\"type\":\"error\",\"op\":\"BLE-ACCESS\",\"reason\":\"unavailable\"}\n");
                else{const char digits[]="0123456789abcdef";char device[33]{},secret[65]{};for(unsigned i=0;i<16;++i){device[i*2]=digits[credential.device[i]>>4];device[i*2+1]=digits[credential.device[i]&15];}for(unsigned i=0;i<32;++i){secret[i*2]=digits[credential.secret[i]>>4];secret[i*2+1]=digits[credential.secret[i]&15];}
                    report("{\"schemaVersion\":1,\"protocol\":\"LARP-PROX-BLE/1\",\"deviceId\":\"%s\",\"secret\":\"%s\",\"name\":\"Larp Naeheglut\",\"stationMac\":\"%02X:%02X:%02X:%02X:%02X:%02X\"}\n",device,secret,local[0],local[1],local[2],local[3],local[4],local[5]);wipe(device,sizeof(device));wipe(secret,sizeof(secret));}
                wipe(credential.secret.data(),credential.secret.size());
            }else if(command.kind==UsbCommand::Kind::BleInfo){
                const auto &view=bleControl.view();const auto p=view.curve.encode();
                report("{\"type\":\"ble-info\",\"protocol\":1,\"ready\":%s,\"fault\":%u,\"curveRevision\":%lu,\"appliedRevision\":%lu,\"curveDurableKnown\":%s,\"points\":[[%d,%u],[%d,%u],[%d,%u],[%d,%u]]}\n",bleControl.ready()?"true":"false",bleControl.fault(),static_cast<unsigned long>(view.revision),static_cast<unsigned long>(bleControl.revision()),view.durable?"true":"false",static_cast<std::int8_t>(p[0]),p[1],static_cast<std::int8_t>(p[2]),p[3],static_cast<std::int8_t>(p[4]),p[5],static_cast<std::int8_t>(p[6]),p[7]);
#endif
            }else if(command.kind==UsbCommand::Kind::Stop){
                autoArm=false;core.stop(now);responsePending=false;
                Config saved=config;saved.armed=false;
                if(storageReady&&(!paired||storage.save(saved))){config.armed=false;durableKnown=true;report("{\"type\":\"ack\",\"op\":\"STOP\",\"durable\":true,\"armed\":false}\n");}
                else{durableKnown=false;fail(1,now);report("{\"type\":\"error\",\"op\":\"STOP\",\"reason\":\"storage\",\"durable\":false,\"restartPermission\":\"unknown\"}\n");}
                wipe(saved.secret.data(),saved.secret.size());
            }else if(command.kind==UsbCommand::Kind::Pair){
                if(fault||core.armed()||autoArm||!output.black()||command.config.peer==local)report("{\"type\":\"error\",\"op\":\"PAIR\",\"reason\":\"state\"}\n");
                else if(!storage.save(command.config)){durableKnown=false;fail(1,now);report("{\"type\":\"error\",\"op\":\"PAIR\",\"reason\":\"storage\"}\n");}
                else{config=command.config;paired=true;responsePending=false;restartRequired=true;report("{\"type\":\"ack\",\"op\":\"PAIR\",\"durable\":true,\"armed\":false,\"restartRequired\":true}\n");}
            }else if(command.kind==UsbCommand::Kind::Arm){
                if(fault||!paired||restartRequired||!radioReady||core.armed()||autoArm||!output.black())report("{\"type\":\"error\",\"op\":\"ARM\",\"reason\":\"state\"}\n");
                else{Config saved=config;saved.armed=true;Core prepared=core;
                    if(!prepared.arm(nowMs())){fail(7,now);report("{\"type\":\"error\",\"op\":\"ARM\",\"reason\":\"core\"}\n");}
                    else if(!storage.save(saved)){durableKnown=false;fail(1,now);report("{\"type\":\"error\",\"op\":\"ARM\",\"reason\":\"storage\"}\n");}
                    else{config.armed=true;core=prepared;report("{\"type\":\"ack\",\"op\":\"ARM\",\"durable\":true,\"armed\":true}\n");}
                    wipe(saved.secret.data(),saved.secret.size());
                }
            }else if(command.kind==UsbCommand::Kind::Reboot){
                if(fault||!paired||config.armed||core.armed()||autoArm||!durableKnown||!output.black())
                    report("{\"type\":\"error\",\"op\":\"REBOOT\",\"reason\":\"state\"}\n");
                else{reboot=true;rebootAt=now;report("{\"type\":\"ack\",\"op\":\"REBOOT\",\"armed\":false}\n");}
            }else report("{\"type\":\"error\",\"op\":\"command\",\"reason\":\"invalid\"}\n");
            wipe(command.config.secret.data(),command.config.secret.size());
        }
        wipe(input,sizeof(input));
        // Queue items preserve arrival time; draining cannot make old RF fresh.
        Event event;
        for(unsigned n=0;events&&n<16&&xQueueReceive(events,&event,0)==pdTRUE;++n){
            now=nowMs();
            if(event.kind==Event::Kind::WifiStop){fail(3,now);continue;}
            if(event.kind==Event::Kind::Tx){const auto kind=lane.complete(event.destination);if(kind==TxLane::Kind::None){fail(5,now);continue;}if(!event.ok&&kind==TxLane::Kind::Request)core.rejectSend();continue;}
            if(fault||!paired||restartRequired||now-event.at>=3000U||!codec.validate(event.packet.data(),event.packet.size(),event.source,event.destination))continue;
            if(event.packet[5]==1){
                if(!responsePending){Token nonce{},echo{};std::copy(event.packet.begin()+56,event.packet.begin()+72,nonce.begin());std::copy(event.packet.begin()+36,event.packet.begin()+52,echo.begin());
                    if(!codec.encode(true,boot,get32(event.packet.data()+52),nonce,echo,response)){fail(8,now);continue;}responsePending=true;responseAt=event.at;}
            }else core.response(event.packet,event.rssi,event.at,now);
        }
        now=nowMs();
        if(crypto.failed())fail(8,now);
        core.service(now);if(core.fault())fail(7,now);
        if(responsePending&&now-responseAt>=2000U)responsePending=false;
        if(!fault&&!restartRequired&&radioReady&&lane.idle()){
            if(responsePending){send(TxLane::Kind::Response,response,now);responsePending=false;}
            else if(core.armed()&&!core.pending()&&now-lastPoll>=500U){
                lastPoll=now;Token nonce{};esp_fill_random(nonce.data(),nonce.size());Packet packet{};
                if(core.beginProbe(nonce,now,codec,packet))send(TxLane::Kind::Request,packet,now);else if(core.fault())fail(8,now);
            }
        }
#ifdef LARP_PROXIMITY_BLE
        bleControl.process(core);
        now=nowMs();core.service(now);output.service(now);if(core.fault())fail(7,now);if(output.fault())fail(6,now);
#endif
        if(output.idle()&&now-lastFrame>=33U){lastFrame=now;const auto pixel=fault?larp::led::Pixel{}:core.render(now);
#ifdef LARP_PROXIMITY_BLE
            computed=pixel;const auto curveRevision=bleControl.revision();
#else
            const std::uint32_t curveRevision=0;
#endif
            if(!output.fault()&&!output.submit(pixel,now,curveRevision))fail(6,now);
        }
#ifdef LARP_PROXIMITY_BLE
        ble::Telemetry telemetry;telemetry.deviceMs=now;telemetry.fault=fault;
        telemetry.flags=(paired?1:0)|(core.armed()?2:0)|(paired&&config.armed?4:0)|(durableKnown?8:0)|(core.fresh(now)?16:0)|(radioReady?32:0)|(lane.idle()?64:0)|(output.known()?128:0)|(!output.idle()?256:0)|(output.black()?512:0);
        telemetry.raw=core.rawRssi();telemetry.filtered=core.filtered();telemetry.targetLevel=core.fresh(now)?core.level():0;telemetry.age=core.sampleAge(now);telemetry.sampleSequence=core.acceptedSequence();telemetry.peer=config.peer;
        telemetry.computed=computed;telemetry.submitted=output.submitted();telemetry.confirmed=output.confirmed();telemetry.submittedAt=output.submittedAt();telemetry.confirmedAt=output.confirmedAt();telemetry.submittedCurveRevision=output.submittedCurve();telemetry.confirmedCurveRevision=output.confirmedCurve();bleControl.publish(telemetry);
#endif
        if(reboot&&now-rebootAt>=100U&&output.black())esp_restart();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
