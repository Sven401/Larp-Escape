#pragma once
#include "ProximityBleTransport.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
extern "C" {
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
}
namespace larp { namespace proximity { namespace ble {
struct Revocation { RevokeReason reason=RevokeReason::None;std::uint32_t epoch=0,at=0,age=0;std::uint16_t offset=0;std::uint8_t type=0;int sdk=0; };
struct ResourceStats { std::uint32_t epoch=0,accessCalls=0,accessOk=0,accessAt=0,notifyCalls=0,notifyOk=0,notifyBusy=0,notifyAllocFailures=0;std::uint16_t accessSize=0;int accessCode=0,minMsysFree=-1,minNotifyPoolFree=-1,lastNotifyPoolFree=-1; };
class Bearer final:public Transport {
    struct Link { std::uint32_t epoch=0;std::uint16_t handle=BLE_HS_CONN_HANDLE_NONE;bool connected=false,subscribed=false,abort=false,ready=false,fault=false; };
    struct Egress { std::uint32_t epoch=0,createdAt=0;Message message; };
    mutable portMUX_TYPE lock_=portMUX_INITIALIZER_UNLOCKED;
    Link link_;Revocation last_;ResourceStats stats_;
    void recordAccess(std::uint16_t,int);void recordNotify(int,int,int);
    void abort(std::uint32_t,RevokeReason,std::uint32_t age=0,std::uint16_t offset=0,std::uint8_t type=0,int sdk=0);
    QueueHandle_t incoming_=nullptr,outgoing_=nullptr;
    Token device_{};std::uint8_t addressType_=0;
    ble_gatt_chr_def characteristics_[3]{};ble_gatt_svc_def services_[2]{};
    std::uint16_t input_=0,output_=0,transfer_=1,offset_=0;Message sending_{};
    std::uint32_t sendingEpoch_=0,sendingAt_=0,busyAt_=0;bool busy_=false,attempted_=false;
    TaskHandle_t host_=nullptr;StaticTask_t hostStorage_{};
    alignas(portBYTE_ALIGNMENT) std::array<StackType_t,8192/sizeof(StackType_t)> hostStack_{};
    ble_npl_callout timer_{};
    static Bearer *selected_;
    Link snapshot()const;
    bool onHost()const;
    bool event(const Ingress&);
    void advertise();void poll();void fail();
    static int access(std::uint16_t,std::uint16_t,ble_gatt_access_ctxt*,void*);
    static int gap(ble_gap_event*,void*);
    static void sync();static void reset(int);static void run(void*);static void tick(ble_npl_event*);
public:
    bool begin(const Token&)override;
    bool receive(Ingress&)override;
    bool send(std::uint32_t,const Message&)override;
    bool current(std::uint32_t)const override;
    void revoke(std::uint32_t)override;
    void revokeReason(std::uint32_t,RevokeReason)override;
    Revocation diagnostic()const;
    ResourceStats resources()const;
    bool ready()const override;bool fault()const override;
};
}}}
