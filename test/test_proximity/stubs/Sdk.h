#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>
using esp_err_t=int;constexpr int ESP_OK=0,ESP_ERR_NVS_NOT_FOUND=1;
constexpr int pdTRUE=1,WIFI_MODE_STA=0,WIFI_STORAGE_RAM=0,WIFI_SECOND_CHAN_NONE=0,WIFI_PS_NONE=0,WIFI_IF_STA=0,WIFI_EVENT_STA_STOP=1;
constexpr int WIFI_EVENT=0;using esp_event_base_t=int;
#define pdMS_TO_TICKS(n) (n)
struct FakeQueue{unsigned width,capacity;std::deque<std::vector<std::uint8_t>> values;};
using QueueHandle_t=FakeQueue*;
inline QueueHandle_t xQueueCreate(unsigned capacity,unsigned width){return new FakeQueue{width,capacity,{}};}
inline int xQueueSend(QueueHandle_t q,const void *p,int){if(q->values.size()==q->capacity)return 0;auto b=static_cast<const std::uint8_t*>(p);q->values.emplace_back(b,b+q->width);return pdTRUE;}
inline int xQueueReceive(QueueHandle_t q,void *p,int){if(q->values.empty())return 0;std::memcpy(p,q->values.front().data(),q->width);q->values.pop_front();return pdTRUE;}
namespace fake {
static std::uint64_t now=0;
static std::vector<std::uint8_t> committed,pending;
static bool missing=true,initFail=false,openFail=false,setFail=false,commitFail=false,readFail=false,readCorrupt=false;
static unsigned writes=0,commits=0;
static std::string serial,input;
static std::function<void()> tick;
static bool restarted=false;
#ifndef LARP_PROXIMITY_REAL_PSA
static bool psaMacOk=false;
#endif
static unsigned sends=0;
static std::array<std::uint8_t,16> pmk{},lmk{};
static std::array<std::uint8_t,6> peerAddress{};
static bool peerEncrypted=false;
static int peerChannel=0,peerInterface=-1;
static unsigned pmkCalls=0;
static bool multipleNvs=false,incrementalRandom=false;
static std::uint32_t commitAdvanceMs=0;
#ifndef LARP_PROXIMITY_REAL_PSA
static bool psaHashOk=false,destroyFail=false;
static unsigned keyCounter=0,activeKeys=0,destroyCalls=0;
#endif
static std::map<std::string,std::vector<std::uint8_t>> records,staged;
static std::map<unsigned,std::string> handles;
}
using nvs_handle_t=unsigned;constexpr int NVS_READWRITE=0,NVS_READONLY=1;
inline int nvs_flash_init(){return fake::initFail?-1:0;}
inline int nvs_open(const char *space,int mode,nvs_handle_t *h){
    if(fake::openFail)return -1;
    if(!fake::multipleNvs){*h=1;return 0;}
    bool found=false;for(const auto &entry:fake::handles)if(entry.second==space){*h=entry.first;found=true;break;}
    if(!found){if(mode==NVS_READONLY)return ESP_ERR_NVS_NOT_FOUND;*h=fake::handles.size()+1;fake::handles[*h]=space;}
    return 0;
}
inline void nvs_close(nvs_handle_t){}
inline int nvs_get_blob(nvs_handle_t handle,const char *name,void *out,std::size_t *n){
    if(fake::multipleNvs){
        if(fake::readFail)return -1;
        const auto key=fake::handles[handle]+":"+name;const auto entry=fake::records.find(key);if(entry==fake::records.end())return ESP_ERR_NVS_NOT_FOUND;
        const auto &value=entry->second;if(!out){*n=value.size();return 0;}if(*n<value.size())return -1;*n=value.size();std::memcpy(out,value.data(),*n);
        if(fake::readCorrupt&&*n)static_cast<std::uint8_t*>(out)[0]^=1;
        return 0;
    }
    if(fake::readFail)return -1;
    if(fake::missing)return ESP_ERR_NVS_NOT_FOUND;
    if(!out){*n=fake::committed.size();return 0;}
    if(*n<fake::committed.size())return -1;
    *n=fake::committed.size();std::memcpy(out,fake::committed.data(),*n);
    if(fake::readCorrupt&&*n)static_cast<std::uint8_t*>(out)[0]^=1;
    return 0;
}
inline int nvs_set_blob(nvs_handle_t handle,const char *name,const void *p,std::size_t n){++fake::writes;if(fake::setFail)return -1;auto b=static_cast<const std::uint8_t*>(p);
    if(fake::multipleNvs)fake::staged[fake::handles[handle]+":"+name]=std::vector<std::uint8_t>(b,b+n);else fake::pending.assign(b,b+n);return 0;}
inline int nvs_commit(nvs_handle_t handle){++fake::commits;fake::now+=fake::commitAdvanceMs;if(fake::commitFail)return -1;
    if(fake::multipleNvs){const auto prefix=fake::handles[handle]+":";for(const auto &item:fake::staged)if(item.first.compare(0,prefix.size(),prefix)==0)fake::records[item.first]=item.second;fake::staged.clear();}
    else{fake::committed=fake::pending;fake::missing=false;}return 0;}
struct rmt_symbol_word_t{unsigned duration0=0,level0=0,duration1=0,level1=0;};
struct FakeRmt;using rmt_channel_handle_t=FakeRmt*;using rmt_encoder_handle_t=int*;
struct rmt_tx_done_event_data_t{};
using Done=bool(*)(rmt_channel_handle_t,const rmt_tx_done_event_data_t*,void*);
struct FakeRmt{Done callback=nullptr;void *user=nullptr;bool enabled=false,deleted=false;};
constexpr int GPIO_NUM_21=21,RMT_CLK_SRC_DEFAULT=0;
struct rmt_tx_channel_config_t{int clk_src=0,gpio_num=0;unsigned resolution_hz=0,mem_block_symbols=0,trans_queue_depth=0;};
struct rmt_copy_encoder_config_t{};struct rmt_tx_event_callbacks_t{Done on_trans_done=nullptr;};
struct rmt_transmit_config_t{struct{bool queue_nonblocking=false;}flags;};
namespace fake {
static std::vector<FakeRmt*> channels;
static bool rmtNewFail=false,rmtEnableFail=false,rmtTransmitFail=false;
static const rmt_symbol_word_t *retained=nullptr;
static std::vector<rmt_symbol_word_t> symbols;
static rmt_tx_channel_config_t rmtConfig{};
inline void complete(FakeRmt *channel){rmt_tx_done_event_data_t e;if(channel->callback)channel->callback(channel,&e,channel->user);}
}
inline int rmt_new_tx_channel(const rmt_tx_channel_config_t *cfg,rmt_channel_handle_t *h){if(fake::rmtNewFail)return -1;*h=new FakeRmt;fake::channels.push_back(*h);fake::rmtConfig=*cfg;return 0;}
inline int rmt_new_copy_encoder(const rmt_copy_encoder_config_t*,rmt_encoder_handle_t *h){*h=new int;return 0;}
inline int rmt_tx_register_event_callbacks(rmt_channel_handle_t h,const rmt_tx_event_callbacks_t *cb,void *user){h->callback=cb->on_trans_done;h->user=user;return 0;}
inline int rmt_enable(rmt_channel_handle_t h){if(fake::rmtEnableFail)return -1;h->enabled=true;return 0;}
inline int rmt_disable(rmt_channel_handle_t h){if(!h->enabled)return -1;h->enabled=false;return 0;}
inline int rmt_del_channel(rmt_channel_handle_t h){h->deleted=true;return 0;}
inline int rmt_del_encoder(rmt_encoder_handle_t h){delete h;return 0;}
inline int rmt_transmit(rmt_channel_handle_t,rmt_encoder_handle_t,const void *p,std::size_t size,const rmt_transmit_config_t*){
    if(fake::rmtTransmitFail)return -1;
    fake::retained=static_cast<const rmt_symbol_word_t*>(p);fake::symbols.assign(fake::retained,fake::retained+size/sizeof(rmt_symbol_word_t));return 0;
}
#ifndef LARP_PROXIMITY_REAL_PSA
using mbedtls_svc_key_id_t=unsigned;struct psa_key_attributes_t{};
#define PSA_KEY_ATTRIBUTES_INIT {}
constexpr int PSA_SUCCESS=0,PSA_KEY_TYPE_HMAC=0,PSA_KEY_USAGE_SIGN_MESSAGE=0,PSA_ALG_SHA_256=0;
#define PSA_ALG_HMAC(a) (a)
inline int psa_crypto_init(){return 0;}inline void psa_set_key_type(psa_key_attributes_t*,int){}inline void psa_set_key_bits(psa_key_attributes_t*,int){}
inline void psa_set_key_usage_flags(psa_key_attributes_t*,int){}inline void psa_set_key_algorithm(psa_key_attributes_t*,int){}
inline int psa_import_key(psa_key_attributes_t*,const void*,std::size_t,mbedtls_svc_key_id_t *id){*id=++fake::keyCounter;++fake::activeKeys;return 0;}
inline int psa_destroy_key(mbedtls_svc_key_id_t){++fake::destroyCalls;if(fake::destroyFail)return -1;if(fake::activeKeys)--fake::activeKeys;return 0;}
inline int psa_hash_compute(int,const void*,std::size_t,void *out,std::size_t capacity,std::size_t *length){if(!fake::psaHashOk||capacity<32)return -1;std::memset(out,7,32);*length=32;return 0;}
inline void psa_reset_key_attributes(psa_key_attributes_t*){}
inline int psa_mac_compute(unsigned,int,const void*,std::size_t,void *out,std::size_t size,std::size_t *length){if(!fake::psaMacOk||size<32)return -1;std::memset(out,7,32);*length=32;return 0;}
#endif
struct usb_serial_jtag_driver_config_t{unsigned rx_buffer_size=0,tx_buffer_size=0;};
inline int usb_serial_jtag_driver_install(const usb_serial_jtag_driver_config_t*){return 0;}
inline int usb_serial_jtag_write_bytes(const void *p,std::size_t n,int){fake::serial.append(static_cast<const char*>(p),n);return n;}
inline int usb_serial_jtag_read_bytes(void *out,std::size_t capacity,int){const auto n=std::min(capacity,fake::input.size());std::memcpy(out,fake::input.data(),n);fake::input.erase(0,n);return n;}
inline std::int64_t esp_timer_get_time(){return fake::now*1000;}
inline void esp_fill_random(void *p,std::size_t n){if(fake::incrementalRandom){for(std::size_t i=0;i<n;++i)static_cast<std::uint8_t*>(p)[i]=i+1;}else std::memset(p,7,n);}
inline void esp_restart(){fake::restarted=true;}
inline void vTaskDelay(int){++fake::now;if(fake::tick)fake::tick();}
constexpr int ESP_MAC_WIFI_STA=0;
inline int esp_read_mac(std::uint8_t *p,int){for(unsigned n=0;n<6;++n)p[n]=2*(n+1);return 0;}
struct wifi_pkt_rx_ctrl_t{int rssi=-40;};
struct esp_now_recv_info_t{std::uint8_t *src_addr=nullptr,*des_addr=nullptr;wifi_pkt_rx_ctrl_t *rx_ctrl=nullptr;};
struct esp_now_send_info_t{std::uint8_t *des_addr=nullptr;};
using esp_now_send_status_t=int;constexpr int ESP_NOW_SEND_SUCCESS=0;
struct wifi_init_config_t{int nvs_enable=0;};
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}
struct esp_now_peer_info_t{std::uint8_t peer_addr[6]{},lmk[16]{};int channel=0,ifidx=0;bool encrypt=false;};
inline int esp_netif_init(){return 0;}inline int esp_event_loop_create_default(){return 0;}
inline int esp_wifi_init(const wifi_init_config_t*){return 0;}
inline int esp_event_handler_register(int,int,void(*)(void*,esp_event_base_t,std::int32_t,void*),void*){return 0;}
inline int esp_wifi_set_storage(int){return 0;}inline int esp_wifi_set_mode(int){return 0;}inline int esp_wifi_start(){return 0;}
inline int esp_wifi_set_channel(int,int){return 0;}inline int esp_wifi_set_ps(int){return 0;}inline int esp_now_init(){return 0;}
inline int esp_now_set_pmk(const std::uint8_t *key){++fake::pmkCalls;std::copy(key,key+16,fake::pmk.begin());return 0;}
inline int esp_now_add_peer(const esp_now_peer_info_t *peer){std::copy(peer->lmk,peer->lmk+16,fake::lmk.begin());std::copy(peer->peer_addr,peer->peer_addr+6,fake::peerAddress.begin());fake::peerEncrypted=peer->encrypt;fake::peerChannel=peer->channel;fake::peerInterface=peer->ifidx;return 0;}
inline int esp_now_register_send_cb(void(*)(const esp_now_send_info_t*,esp_now_send_status_t)){return 0;}
inline int esp_now_register_recv_cb(void(*)(const esp_now_recv_info_t*,const std::uint8_t*,int)){return 0;}
inline int esp_now_send(const std::uint8_t*,const std::uint8_t*,std::size_t){++fake::sends;return 0;}
