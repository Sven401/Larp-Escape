// Independent transport diagnostic. No game commands, GPIO or v2 authority.
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <esp_system.h>
#include "DiagnosticProtocol.h"
namespace {
const char *serviceUuid = "86ddc5b0-3878-4ff7-9f55-17c62aeb0001";
diagnostic::Session session;
BLECharacteristic *replyCharacteristic = nullptr;
class Status : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *c) override {
        std::uint8_t bytes[16] = {1,0x49};
        diagnostic::put32(bytes+2,session.boot);
        diagnostic::put32(bytes+6,millis());
        diagnostic::put32(bytes+10,ESP.getFreeHeap());
        bytes[14]=1; // diagnostic-only flag, not game authority
        c->setValue(bytes,sizeof(bytes));
    }
} statusCallbacks;
class Command : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *c) override {
        const auto value=c->getValue();
        std::uint8_t reply[16] = {};
        session.receive(reinterpret_cast<const std::uint8_t *>(value.data()),value.size(),reply);
        replyCharacteristic->setValue(reply,sizeof(reply));
        Serial.printf("BLE_DIAG request=%lu status=%u reason=%u processed=%lu boot=%lu\n",
                      static_cast<unsigned long>(diagnostic::get32(reply+2)),reply[1],reply[14],
                      static_cast<unsigned long>(session.processed),static_cast<unsigned long>(session.boot));
    }
} commandCallbacks;
class Connections : public BLEServerCallbacks {
    void onConnect(BLEServer *) override { session.clearConnection(); Serial.println("BLE_DIAG connected"); }
    void onDisconnect(BLEServer *) override {
        session.clearConnection(); std::uint8_t empty[16] = {};
        replyCharacteristic->setValue(empty,sizeof(empty));
        BLEDevice::startAdvertising(); Serial.println("BLE_DIAG disconnected advertising restarted");
    }
} connections;
}
void setup() {
    Serial.begin(115200); session.boot=esp_random(); if(!session.boot) session.boot=1;
    BLEDevice::init("Larp-ESP-Diagnostic");
    auto *server=BLEDevice::createServer(); server->setCallbacks(&connections);
    auto *service=server->createService(serviceUuid);
    auto *status=service->createCharacteristic("86ddc5b0-3878-4ff7-9f55-17c62aeb0002",BLECharacteristic::PROPERTY_READ);
    status->setCallbacks(&statusCallbacks);
    auto *command=service->createCharacteristic("86ddc5b0-3878-4ff7-9f55-17c62aeb0003",BLECharacteristic::PROPERTY_WRITE);
    command->setCallbacks(&commandCallbacks);
    replyCharacteristic=service->createCharacteristic("86ddc5b0-3878-4ff7-9f55-17c62aeb0004",BLECharacteristic::PROPERTY_READ);
    std::uint8_t empty[16] = {}; replyCharacteristic->setValue(empty,sizeof(empty));
    service->start(); auto *advertising=BLEDevice::getAdvertising();
    advertising->addServiceUUID(serviceUuid); advertising->setScanResponse(true); advertising->start();
    Serial.printf("BLE_DIAG ready boot=%lu heap=%u service=%s diagnostic_only=true\n",
                  static_cast<unsigned long>(session.boot),ESP.getFreeHeap(),serviceUuid);
}
void loop() { delay(20); }
