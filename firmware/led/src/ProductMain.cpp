#include <Arduino.h>
#include "LedWire.h"
#include "LedOutput.h"
#include "WakeInputs.h"
#ifdef LARP_ESPNOW_TEST
#include "EspNowRadio.h"
#if defined(LARP_NFC_TEST) || defined(LARP_MATRIX_TEST)
#error "ESP-NOW test uses its own hardware-test variant"
#endif
#endif
#if defined(LARP_NFC_TEST) && defined(LARP_MATRIX_TEST)
#error "NFC and matrix hardware tests require separate firmware variants"
#endif
#ifdef LARP_NFC_TEST
#include "NfcBox.h"
#endif
#include "AudioOutput.h"

#ifdef LARP_MATRIX_TEST
#include "MatrixInputs.h"
#endif
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <esp_system.h>
#include <mbedtls/md.h>
#include <freertos/queue.h>
#include <cstring>

// The Arduino loop owns the LED engine. Authenticated BLE is the only control
// ingress; USB only exports the per-box access credential on explicit request.
namespace {
namespace wire = larp::ledwire;
larp::led::LedOutput output;
larp::wake::WakeInputs wakeInputs(larp::wake::mcpWakeBus());
larp::audio::AudioOutput audioOutput;

#ifdef LARP_MATRIX_TEST
larp::matrix::MatrixInputs matrixInputs(larp::wake::mcpWakeBus());
#endif
class Control final : public wire::IControl {
 public:
#ifdef LARP_ESPNOW_TEST
  wire::EspNowSnapshot espNowSnapshot() const noexcept override { return larp::espnowradio::snapshot(); }
  bool submitEspNow(const std::array<uint8_t, 6> &peer, uint32_t now) noexcept override {
    return larp::espnowradio::submit(peer, now);
  }
#endif
  wire::Snapshot snapshot() const noexcept override {
    const auto s = output.snapshot();
    wire::Snapshot out;
    out.revision = s.revision; out.appliedRevision = s.appliedRevision;
    out.effect = static_cast<wire::Effect>(s.applied);
    // Off describes the reported applied effect; Ready additionally proves boot
    // black was transmitted. No command is accepted before that observation.
    out.flags = (s.ready ? wire::Ready : 0) | (s.pending ? wire::Pending : 0) |
      (s.applied == larp::led::Effect::Off ? wire::Off : 0) |
      (s.completed ? wire::Completed : 0) | (s.fault ? wire::Fault : 0);
    out.mode = wire::Mode::LedOnly;
    const auto wake = wakeInputs.snapshot();
#ifdef LARP_NFC_TEST
    out.nfcSupported = true;
#endif
    out.wakeSupported = true; out.wakeValid = wake.valid;
    out.wakeMask = wake.activeMask;
#ifdef LARP_MATRIX_TEST
    const auto matrix = matrixInputs.snapshot(millis());
    out.matrixSupported = true; out.matrixValid = matrix.valid;
    out.matrixAmbiguous = matrix.ambiguous; out.matrixBits = matrix.bits;
#endif
    return out;
  }
  bool submit(wire::Operation op, wire::Effect effect, uint32_t expected,
              uint32_t &revision) noexcept override {
#ifdef LARP_NFC_TEST
    if (larp::nfcble::busy()) return false;
#endif
    if (expected != output.snapshot().revision) return false;
    const bool ok = op == wire::Operation::Stop ? output.stop() :
      output.start(static_cast<larp::led::Effect>(effect));
    revision = output.snapshot().revision;
    return ok;
  }
  wire::AudioSnapshot audioSnapshot() const noexcept override {
    const auto s = audioOutput.snapshot();
    wire::AudioSnapshot out;
    out.supported = true; out.revision = s.revision; out.appliedRevision = s.appliedRevision;
    out.track = s.track; out.volume = s.volume; out.ready = s.ready;
    out.pending = s.pending; out.active = s.active; out.fault = s.fault; out.paused = s.paused;
    out.busyValid = s.busyValid; out.busyLow = s.busyLow;
    return out;
  }
  bool submitAudio(wire::Operation op, uint16_t track, uint8_t volume,
                   uint32_t expected, uint32_t &revision) noexcept override {
#ifdef LARP_NFC_TEST
    if (larp::nfcble::busy()) return false;
#endif
    if (expected != audioOutput.snapshot().revision) return false;
    const bool ok = op == wire::Operation::Stop ? audioOutput.stop() :
        op == wire::Operation::Pause ? audioOutput.pause() :
        op == wire::Operation::Resume ? audioOutput.resume() :
        op == wire::Operation::Volume ? audioOutput.volume(volume) : audioOutput.start(track, volume);
    revision = audioOutput.snapshot().revision;
    return ok;
  }
  void stopOnDisconnect() noexcept override { output.stop(); audioOutput.stop();
#ifdef LARP_ESPNOW_TEST
    larp::espnowradio::cancel();
#endif
#ifdef LARP_NFC_TEST
    larp::nfcble::box().cancel();
#endif
  }
} control;
class Hmac final : public wire::IHmac256 {
 public:
  bool calculate(const wire::Secret &key, const uint8_t *data, size_t size,
                  wire::Secret &result) noexcept override {
    const auto *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    return info && mbedtls_md_hmac(info, key.data(), key.size(), data, size,
                                 result.data()) == 0;
  }
} crypto;
wire::Session session(crypto, control);
#ifdef LARP_NFC_TEST
larp::nfcble::Session nfcSession(session, larp::nfcble::box());
#endif
wire::Assembler assembler;
wire::Nonce bootNonce;
struct Credentials { uint8_t version; wire::Nonce device; wire::Secret secret; } credentials{};
static_assert(sizeof(Credentials) == 49, "NVS credential format");
bool provisioned = false;
BLEServer *bleServer = nullptr;
BLECharacteristic *input = nullptr, *notifications = nullptr;
BLE2902 *subscription = nullptr;
QueueHandle_t received = nullptr;
portMUX_TYPE linkLock = portMUX_INITIALIZER_UNLOCKED;
struct Link {
  uint32_t epoch = 0;
  uint16_t connection = 0;
  esp_gatt_if_t interface = ESP_GATT_IF_NONE;
  bool connected = false, subscribed = false, abort = false, advertise = false;
} link;
struct Frame { uint32_t epoch; uint8_t size; uint8_t bytes[20]; };
Link linkSnapshot() {
  portENTER_CRITICAL(&linkLock); const auto value = link; portEXIT_CRITICAL(&linkLock);
  return value;
}
void abortLink(uint32_t epoch) {
  portENTER_CRITICAL(&linkLock);
  if (link.epoch == epoch && link.connected) link.abort = true;
  portEXIT_CRITICAL(&linkLock);
}
// Arduino handles ATT responses; its successful WRITE response is only transport
// admission. A separately authenticated Result proves application and output.
void gattEvent(esp_gatts_cb_event_t event, esp_gatt_if_t interface,
               esp_ble_gatts_cb_param_t *p) {
  if (event == ESP_GATTS_CONNECT_EVT) {
    portENTER_CRITICAL(&linkLock);
    const bool accept = !link.connected && link.epoch < UINT32_MAX;
    if (accept) {
      ++link.epoch; link.connection = p->connect.conn_id; link.interface = interface;
      link.connected = true; link.subscribed = false; link.abort = false;
    }
    portEXIT_CRITICAL(&linkLock);
    if (!accept) bleServer->disconnect(p->connect.conn_id);
    return;
  }
  if (event == ESP_GATTS_DISCONNECT_EVT) {
#ifdef LARP_NFC_TEST
    larp::nfcble::box().cancel();
#endif
    portENTER_CRITICAL(&linkLock);
    if (link.connected && link.connection == p->disconnect.conn_id && link.interface == interface) {
      link.connected = false; link.subscribed = false; link.abort = true;
      link.advertise = true;
      if (link.epoch < UINT32_MAX) ++link.epoch;
    }
    portEXIT_CRITICAL(&linkLock);
    return;
  }
  const auto current = linkSnapshot();
  if (!current.connected || current.interface != interface) return;
  if (event == ESP_GATTS_EXEC_WRITE_EVT) {
    if (p->exec_write.conn_id == current.connection) abortLink(current.epoch);
    return;
  }
  if (event != ESP_GATTS_WRITE_EVT || p->write.conn_id != current.connection) return;
  const auto &w = p->write;
  if (subscription && w.handle == subscription->getHandle()) {
    // Arduino creates CCCD with AUTO_RSP: Bluedroid already answers the write.
    // need_rsp is therefore false even for a regular ATT Write Request.
    const bool valid = !w.is_prep && !w.offset && w.len == 2 &&
                       w.value && w.value[0] == 1 && w.value[1] == 0;
    portENTER_CRITICAL(&linkLock);
    if (link.epoch == current.epoch) {
      link.subscribed = valid;
      if (!valid) link.abort = true;
    }
    portEXIT_CRITICAL(&linkLock);
    return;
  }
  if (!input || w.handle != input->getHandle()) return;
  if (current.abort || !current.subscribed || w.is_prep || !w.need_rsp || w.offset ||
      !w.value || w.len <= 8 || w.len > 20) { abortLink(current.epoch); return; }
  Frame frame{}; frame.epoch = current.epoch; frame.size = w.len;
  std::memcpy(frame.bytes, w.value, w.len);
  if (xQueueSendToBack(received, &frame, 0) != pdTRUE) abortLink(current.epoch);
}

// Only the Arduino owner touches protocol state, credentials or LED buffers.
uint32_t ownerEpoch = 0, lastStatus = 0, lastFrame = 0;
wire::Message tx[4];
uint8_t txHead = 0, txCount = 0;
uint16_t txOffset = 0, txTransfer = 1;
void clearTransport() {
#ifdef LARP_NFC_TEST
  nfcSession.reset();
#endif
  assembler.reset(); txHead = txCount = 0; txOffset = 0; txTransfer = 1;
}
bool enqueue(const wire::Message &message) {
  if (!message.size) return true;
  if (txCount == 4) { abortLink(ownerEpoch); return false; }
  tx[(txHead + txCount) % 4] = message; ++txCount;
  return true;
}
void serviceBle() {
  auto current = linkSnapshot();
  if (current.epoch != ownerEpoch || !current.connected || current.abort) {
    if (session.connected()) session.disconnect();
    clearTransport();
    if (current.epoch != ownerEpoch) {
      // Epoch is independent of recycled ATT conn_id and cannot lose disconnect.
      output.stop(); audioOutput.stop(); ownerEpoch = current.epoch;
    }
    if (current.connected && current.abort) {
      bleServer->disconnect(current.connection);
      return;
    }
    if (current.connected) {
      wire::Nonce nonce{}; esp_fill_random(nonce.data(), nonce.size());
      if (!session.beginConnection(bootNonce, nonce, millis())) abortLink(ownerEpoch);
    } else {
      bool advertise = false;
      portENTER_CRITICAL(&linkLock);
      if (!link.connected && link.advertise && link.epoch == current.epoch) {
        link.advertise = false; advertise = link.epoch < UINT32_MAX;
      }
      portEXIT_CRITICAL(&linkLock);
      if (advertise && provisioned) BLEDevice::startAdvertising();
      return;
    }
  }
  Frame frame{};
  // Bounded work leaves LED output and disconnect handling responsive under flooding.
  for (unsigned count = 0; count < 8 && xQueueReceive(received, &frame, 0) == pdTRUE; ++count) {
    current = linkSnapshot();
    if (!current.connected || current.abort || current.epoch != ownerEpoch) return;
    if (frame.epoch != ownerEpoch) continue;
    wire::Message message{}, reply{};
    const auto assembled = assembler.accept(frame.bytes, frame.size, millis(), message);
    if (assembled == wire::FrameResult::Rejected) { abortLink(ownerEpoch); return; }
    if (assembled == wire::FrameResult::Complete) {
#ifdef LARP_NFC_TEST
      if (message.bytes[1] == 5) {
        const auto led = output.snapshot();
        larp::nfcble::allow(led.ready && !led.fault && !led.pending && led.applied == larp::led::Effect::Off && led.requested == larp::led::Effect::Off);
        if (!nfcSession.receive(message, millis(), reply)) { abortLink(ownerEpoch); return; }
        if (!enqueue(reply)) return;
        continue;
      }
#endif
      if (session.receive(message, millis(), reply) == wire::SessionResult::Rejected) {
        abortLink(ownerEpoch); return;
      }
      if (!enqueue(reply)) return;
    }
  }
  wire::Message reply{};
  if (session.service(millis(), reply) && !enqueue(reply)) return;
#ifdef LARP_NFC_TEST
  if (!nfcSession.service(millis(), reply)) { abortLink(ownerEpoch); return; }
  if (!enqueue(reply)) return;
#endif
  if (!session.connected()) { abortLink(ownerEpoch); return; }
  if (session.authenticated() && txCount == 0 && millis() - lastStatus >= 250) {
    lastStatus = millis();
    if (!session.publishStatus(lastStatus, reply) || !enqueue(reply)) {
      abortLink(ownerEpoch); return;
    }
  }
  current = linkSnapshot();
  if (!txCount || !current.connected || !current.subscribed || current.abort ||
      current.epoch != ownerEpoch || millis() - lastFrame < 8) return;
  std::array<uint8_t, 20> bytes{}; size_t size = 0;
  if (!wire::encodeFrame(tx[txHead], txTransfer, txOffset, bytes, size) ||
      esp_ble_gatts_send_indicate(current.interface, current.connection,
        notifications->getHandle(), size, bytes.data(), false) != ESP_OK) {
    abortLink(ownerEpoch); return;
  }
  lastFrame = millis(); txOffset += size - 8;
  if (txOffset == tx[txHead].size) {
    txHead = (txHead + 1) % 4; --txCount; txOffset = 0;
    if (txTransfer == UINT16_MAX) abortLink(ownerEpoch);
    else ++txTransfer;
  }
}

bool nonzero(const uint8_t *bytes, size_t size) {
  uint8_t any = 0; while (size--) any |= *bytes++; return any != 0;
}
bool loadCredentials() {
  // Arduino may automatically erase its default NVS on migration/full-page
  // errors. Keep identity in a separate partition that we never auto-format.
  if (nvs_flash_init_partition("larpkeys") != ESP_OK) return false;
  nvs_handle_t storage;
  if (nvs_open_from_partition("larpkeys", "larp-led", NVS_READWRITE, &storage) != ESP_OK) return false;
  size_t size = 0;
  const auto found = nvs_get_blob(storage, "access-v1", nullptr, &size);
  bool ok = false;
  if (found == ESP_ERR_NVS_NOT_FOUND) {
    credentials.version = 1;
    esp_fill_random(credentials.device.data(), credentials.device.size());
    esp_fill_random(credentials.secret.data(), credentials.secret.size());
    if (nonzero(credentials.device.data(), 16) && nonzero(credentials.secret.data(), 32) &&
        nvs_set_blob(storage, "access-v1", &credentials, sizeof(credentials)) == ESP_OK &&
        nvs_commit(storage) == ESP_OK) {
      Credentials verify{}; size_t length = sizeof(verify);
      ok = nvs_get_blob(storage, "access-v1", &verify, &length) == ESP_OK &&
           length == sizeof(verify) && std::memcmp(&verify, &credentials, length) == 0;
    }
  } else if (found == ESP_OK && size == sizeof(credentials)) {
    ok = nvs_get_blob(storage, "access-v1", &credentials, &size) == ESP_OK &&
         size == sizeof(credentials) && credentials.version == 1 &&
         nonzero(credentials.device.data(), 16) && nonzero(credentials.secret.data(), 32);
  }
  // Read/type/storage errors never generate a replacement identity or erase NVS.
  nvs_close(storage);
  return ok && session.configure(credentials.device, credentials.secret);
}
void hexToSerial(const uint8_t *bytes, size_t size) {
  const char digits[] = "0123456789abcdef";
  while (size--) { const auto b = *bytes++; Serial.write(digits[b >> 4]); Serial.write(digits[b & 15]); }
}
void serviceUsb() {
  static char command[48]; static size_t used = 0; static bool overflow = false;
  for (unsigned count = 0; count < 64 && Serial.available(); ++count) {
    const char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      command[used] = 0;
      if (!overflow && std::strcmp(command, "LARP-PROVISION-V1") == 0) {
        if (provisioned) {
          // Explicit trusted USB request only; never print secrets at startup.
          Serial.print("{\"schemaVersion\":1,\"deviceId\":\"");
          hexToSerial(credentials.device.data(), 16);
          Serial.print("\",\"secret\":\""); hexToSerial(credentials.secret.data(), 32);
          Serial.println("\",\"name\":\"Larp LED\"}");
        } else Serial.println("LARP ERROR credentials unavailable");
      }
      used = 0; overflow = false;
    } else if (used < sizeof(command) - 1) command[used++] = c;
    else overflow = true;
  }
}
bool ready = false, outputInitialized = false;
} // namespace

void setup() {
  Serial.begin(115200);
#ifdef LARP_NFC_TEST
  if (!larp::nfcble::begin()) { Serial.println("LARP ERROR NFC worker"); return; }
#endif
  audioOutput.begin();
  wakeInputs.begin(millis());
#ifdef LARP_MATRIX_TEST
  matrixInputs.begin(millis());
#endif
  outputInitialized = output.begin();
  if (!outputInitialized) { Serial.println("LARP ERROR LED initialization"); return; }
  received = xQueueCreate(32, sizeof(Frame));
  if (!received) { Serial.println("LARP ERROR frame queue"); return; }
  BLEDevice::init("Larp LED"); // Active radio supplies entropy for hardware RNG.
#ifdef LARP_ESPNOW_TEST
  if (!larp::espnowradio::begin()) Serial.println("LARP ESP-NOW unavailable");
#endif
  esp_fill_random(bootNonce.data(), bootNonce.size());
  provisioned = loadCredentials();
  if (!provisioned) { Serial.println("LARP ERROR credentials; USB recovery required"); return; }
  bleServer = BLEDevice::createServer();
  auto *service = bleServer->createService("6c790001-2f50-4d5a-a7f4-35a478c4b101");
  input = service->createCharacteristic("6c790002-2f50-4d5a-a7f4-35a478c4b101", BLECharacteristic::PROPERTY_WRITE);
  notifications = service->createCharacteristic("6c790003-2f50-4d5a-a7f4-35a478c4b101", BLECharacteristic::PROPERTY_NOTIFY);
  subscription = new BLE2902(); notifications->addDescriptor(subscription);
  BLEDevice::setCustomGattsHandler(gattEvent);
  service->start();
  BLEDevice::getAdvertising()->addServiceUUID(service->getUUID());
  BLEDevice::getAdvertising()->setScanResponse(true);
  BLEDevice::startAdvertising();
  ready = true;
#ifdef LARP_MATRIX_TEST
  Serial.println("LARP matrix-test ready; historical column-LOW scan; B7 input unqualified; USB provisioning on explicit request");
#else
  Serial.println("LARP LED-only ready; USB provisioning on explicit request");
#endif
}
void loop() {
#ifdef LARP_ESPNOW_TEST
  larp::espnowradio::service(millis());
#endif
#ifdef LARP_MATRIX_TEST
  matrixInputs.service(millis());
#endif
  wakeInputs.service(millis());
  serviceUsb();
  if (ready) serviceBle();
  if (ready) {
    const auto current = linkSnapshot();
    if (current.epoch != ownerEpoch || !current.connected || current.abort) {
      session.disconnect();
      const auto state = output.snapshot();
      if (state.requested != larp::led::Effect::Off) output.stop();
    }
  }
  // Apply link/epoch aborts before a prepared UART Play can leave the device.
  // Boot/fault Stop still drains even when BLE initialization did not succeed.
  audioOutput.service();
  if (outputInitialized) {
    output.service();
  }
  delay(1);
}
