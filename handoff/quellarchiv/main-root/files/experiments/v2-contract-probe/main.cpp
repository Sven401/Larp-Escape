#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <esp_system.h>
#include <esp_bt.h>
#include <bootloader_random.h>
#include "ContractLab.h"
using namespace larp::companion;
static ContractLab lab;
static BLECharacteristic *outbound;
static QueueHandle_t outgoing;
static StaticQueue_t queueControl;
static std::uint8_t queueBytes[4*sizeof(RoutedReplyFrame)];
class Bearer : public IReplyFrameBearer {
    ReplyFrameCopy copyReplyFrame(const RoutedReplyFrame &frame) noexcept override {
        TrustedAuthority a;
        if (!lab.channel.readAuthority(a) || !a.gameMaster || a.principal!=frame.principal || a.epoch!=frame.epoch)
            return ReplyFrameCopy::Unavailable;
        return xQueueSend(outgoing,&frame,0)==pdTRUE ? ReplyFrameCopy::Copied : ReplyFrameCopy::Busy;
    }
} bearer;
static ReplyFrameSender replies(lab.channel,bearer);
static StatusFrameSender statuses(lab.channel,bearer);
class Lifecycle : public BLEServerCallbacks {
    void onConnect(BLEServer *) override {
        TrustedAuthority a;
        lab.channel.advanceAuthority(7,true,a); // Explicit lab injection, NOT authentication.
        Serial.printf("LAB CONNECT epoch=%lu synthetic_authority=1\n",(unsigned long)a.epoch);
    }
    void onDisconnect(BLEServer *server) override {
        TrustedAuthority a; lab.channel.advanceAuthority(0,false,a);
        Serial.printf("LAB DISCONNECT epoch=%lu\n",(unsigned long)a.epoch);
        server->startAdvertising();
    }
} lifecycle;
class Ingress : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *characteristic) override {
        auto bytes=characteristic->getValue();
        TrustedAuthority a;
        if (lab.channel.readAuthority(a) && a.gameMaster)
            lab.frames.copyWrite(bytes.data(),bytes.size(),a.principal,a.epoch,millis());
    }
} ingress;
void setup() {
    Serial.begin(115200);
    bootloader_random_enable();BootIdentity boot;boot.high=esp_random();boot.low=esp_random();bootloader_random_disable();
    if (!boot.valid() || !lab.begin(boot)) { Serial.println("LAB INIT FAILED");while(true)delay(1000); }
    for(unsigned i=0;i<256;++i) { lab.tick(millis());delay(1); }
    outgoing=xQueueCreateStatic(4,sizeof(RoutedReplyFrame),queueBytes,&queueControl);
    Serial.printf("LAB BEFORE BLE heap=%lu largest=%lu profile_bytes=%u\n",(unsigned long)ESP.getFreeHeap(),(unsigned long)ESP.getMaxAllocHeap(),unsigned(sizeof(larp::configuration::Profile)));
    // Arduino's default btStart starts BTDM. This BLE-only lab releases Classic
    // controller memory and explicitly starts BLE before BLEDevice::init.
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    esp_bt_controller_config_t btConfig=BT_CONTROLLER_INIT_CONFIG_DEFAULT();btConfig.mode=ESP_BT_MODE_BLE;
    if(esp_bt_controller_init(&btConfig)!=ESP_OK || esp_bt_controller_enable(ESP_BT_MODE_BLE)!=ESP_OK) {
        Serial.println("LAB BLE CONTROLLER FAILED");while(true)delay(1000);
    }
    BLEDevice::init("Larp v2 Vertragslabor");
    auto server=BLEDevice::createServer();server->setCallbacks(&lifecycle);
    auto service=server->createService("86ddc5b0-3878-4ff7-9f55-17c62aeb1001");
    auto marker=service->createCharacteristic("86ddc5b0-3878-4ff7-9f55-17c62aeb1002",BLECharacteristic::PROPERTY_READ);
    marker->setValue("V2 LAB; fake owners; injected authority; RAM journal");
    auto command=service->createCharacteristic("86ddc5b0-3878-4ff7-9f55-17c62aeb1003",BLECharacteristic::PROPERTY_WRITE);
    command->setCallbacks(&ingress);
    outbound=service->createCharacteristic("86ddc5b0-3878-4ff7-9f55-17c62aeb1004",BLECharacteristic::PROPERTY_NOTIFY);
    outbound->addDescriptor(new BLE2902());service->start();
    auto advertising=BLEDevice::getAdvertising();advertising->addServiceUUID(service->getUUID());advertising->setScanResponse(true);advertising->start();
    Serial.printf("V2 LAB READY boot=%08lX%08lX heap=%lu synthetic_devices=1 synthetic_authority=1\n",(unsigned long)boot.high,(unsigned long)boot.low,(unsigned long)ESP.getFreeHeap());
}
void loop() {
    const auto now=millis();lab.tick(now);
    if (!replies.pending()) {
        RoutedGameReply reply;
        if(lab.channel.takeCurrentReply(reply)) {
            replies.load(reply,now);
            Serial.printf("V2 REPLY lease=%lu request=%lu outcome=%s duplicate=%u consumed=%u next=%lu cfg=%lu state=%lu\n",(unsigned long)reply.reply.lease,(unsigned long)reply.reply.request,gameReplyOutcomeName(reply.reply.outcome),reply.reply.duplicate,reply.reply.consumed,(unsigned long)reply.reply.nextRequest,(unsigned long)reply.reply.configurationRevision,(unsigned long)reply.reply.stateRevision);
        }
    }
    if(!statuses.pending()) { RoutedConfiguredStatus status;if(lab.channel.readCurrentStatus(status))statuses.load(status,now); }
    static std::uint32_t sent=0;
    if(now-sent>=20) {
        sent=now;serviceConfiguredFrames(replies,statuses,now,20);
        RoutedReplyFrame frame;
        if(xQueueReceive(outgoing,&frame,0)==pdTRUE) {
            TrustedAuthority a;
            if(lab.channel.readAuthority(a) && a.gameMaster && a.principal==frame.principal && a.epoch==frame.epoch) {
                outbound->setValue(reinterpret_cast<std::uint8_t*>(frame.bytes.data()),frame.count);outbound->notify();
            }
        }
    }
    delay(1);
}
