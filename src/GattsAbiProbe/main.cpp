// Compile-only SDK ABI/queue probe. SYNTHETIC verifier and responses.
// No Bluetooth initialization, advertising, radio, keys or device owners.
#include "Companion/CommandStream.h"
#include "Companion/IdentityAccess.h"
#include <Arduino.h>
#include <FreeRtosCommandFrameQueue.h>
#include <FreeRtosGameControlChannel.h>
#include <FreeRtosProfileFrameQueue.h>
#include <GattsWriteAdmission.h>
#include <SdkGattsWrite.h>
#include <algorithm>
namespace {
using namespace larp::companion;
FreeRtosGameControlChannel channel;
FreeRtosCommandFrameQueue queue(channel);
FreeRtosProfileFrameQueue profileQueue(channel);
IdentityAccess identityPolicy(channel);
BoundConnection connection;
CommandStream stream(channel);
GattsWriteBinding original;
std::array<char, maxGameCommandJsonBytes> document = {};
std::array<char, 20> frame = {};
std::size_t length = 0, offset = 0;
bool ready = false, queued = false;
class SyntheticResponse : public IWriteResponseBearer {
  public:
    RoutedWriteResponse last;
    std::uint32_t copies = 0;
    bool copyWriteResponse(const RoutedWriteResponse &response) noexcept override {
        if (!(response.original.connection == connection) ||
            response.original.interface != original.interface ||
            response.original.peerAddress != original.peerAddress)
            return false;
        last = response;
        if (copies != UINT32_MAX)
            ++copies;
        return true;
    }
} responses;
} // namespace
void setup() {
    Serial.begin(115200);
    if (!channel.begin() || !queue.begin() || !profileQueue.begin())
        return;
    Identity device, peer;
    device.bytes[0] = 1;
    peer.bytes[0] = 2;
    VerifiedPeerEvidence evidence;
    evidence.peer = peer;
    evidence.encrypted = evidence.mitm = evidence.secureConnections = evidence.bonded = true;
    if (identityPolicy.begin(device) != IdentityAccessOutcome::Initialized ||
        identityPolicy.connect(0, connection) != IdentityAccessOutcome::Connected ||
        identityPolicy.authenticate(connection, evidence) != IdentityAccessOutcome::Verified ||
        identityPolicy.assignGameMaster(peer, 7) != IdentityAccessOutcome::Assigned ||
        !identityPolicy.route(connection, original.authority))
        return;
    original.connection = connection;
    original.interface = 1; // Synthetic registration, not a live SDK application.
    original.attribute = 0x42;
    original.peerAddress[0] = 1; // Synthetic address; no real peer captured.
    GameCommand command;
    command.boot.high = 0x12345678;
    command.boot.low = 0xabcdef01;
    command.lease = command.request = command.expectedConfigurationRevision = 1;
    if (encodeGameCommand(command, document.data(), document.size(), length) !=
        ConfiguredEncodeResult::Success)
        return;
    Serial.printf("gatts_abi_probe: synthetic=true network=false SDK_write=%u binding=%u queue=%u "
                  "admission_only=true executed=false\n",
                  unsigned(sizeof(esp_ble_gatts_cb_param_t)), unsigned(sizeof(original)),
                  unsigned(sizeof(queue)));
    for (const auto flag : {ESP_GATT_PREP_WRITE_CANCEL, ESP_GATT_PREP_WRITE_EXEC}) {
        esp_ble_gatts_cb_param_t parameter = {};
        parameter.exec_write.conn_id = connection.handle;
        parameter.exec_write.trans_id = 99;
        parameter.exec_write.exec_write_flag = flag;
        for (std::size_t i = 0; i < original.peerAddress.size(); ++i)
            parameter.exec_write.bda[i] = original.peerAddress[i];
        const auto event = fromSdkGattsExecute(original.interface, parameter, original);
        const auto rejected = admitGattsWrite(event, original, channel, queue, responses, millis());
        if (rejected.outcome != GattsWriteOutcome::PreparedUnsupported ||
            rejected.status != AttWriteStatus::RequestUnsupported)
            return;
    }
    // Synthetic profile metadata copy only, no temporary store/radio/import.
    ProfileSourceEnvelope metadata;
    metadata.transfer = 1;
    metadata.bytes = metadata.expectedConfigurationRevision = 1;
    char profileValue[20];
    std::size_t profileWritten = 0;
    CopiedProfileFrame profileCopy;
    if (!encodeProfileSourceEnvelope(metadata, profileValue, sizeof(profileValue),
                                     profileWritten) ||
        profileQueue.copyWrite(profileValue, profileWritten, original.authority.principal,
                               original.authority.epoch, millis()) != ProfileCopyOutcome::Copied ||
        !profileQueue.take(profileCopy) || profileCopy.count != profileWritten)
        return;
    Serial.printf(
        "profile_queue_probe: synthetic=true network=false queue=%u frame=%u imported=false\n",
        unsigned(sizeof(profileQueue)), unsigned(sizeof(profileCopy)));
    ready = true;
}
void loop() {
    if (!ready || queued) {
        delay(100);
        return;
    }
    const auto now = millis();
    const auto count = std::min<std::size_t>(12, length - offset);
    std::size_t written = 0;
    if (!encodeCommandFrame(1, static_cast<std::uint16_t>(length),
                            static_cast<std::uint16_t>(offset), document.data() + offset, count,
                            frame.data(), frame.size(), written)) {
        ready = false;
        return;
    }
    esp_ble_gatts_cb_param_t parameter = {};
    parameter.write.conn_id = connection.handle;
    parameter.write.trans_id = static_cast<std::uint32_t>(offset + 1);
    parameter.write.handle = original.attribute;
    parameter.write.len = static_cast<std::uint16_t>(written);
    parameter.write.value = reinterpret_cast<std::uint8_t *>(frame.data());
    parameter.write.need_rsp = true;
    for (std::size_t i = 0; i < original.peerAddress.size(); ++i)
        parameter.write.bda[i] = original.peerAddress[i];
    const auto event = fromSdkGattsWrite(original.interface, parameter);
    const auto admitted = admitGattsWrite(event, original, channel, queue, responses, now);
    if (admitted.outcome != GattsWriteOutcome::Copied) {
        ready = false;
        return;
    }
    const auto dispatch = dispatchCommandFrame(queue, stream, now);
    if (!dispatch.received) {
        ready = false;
        return;
    }
    offset += count;
    queued = dispatch.stream.outcome == CommandStreamOutcome::Queued;
    Serial.printf("gatts_abi_probe: copied=%u local_response_copied=%s stream=%u "
                  "command_queued=%s executed=false heap=%u\n",
                  unsigned(offset), admitted.responseCopied ? "true" : "false",
                  unsigned(dispatch.stream.outcome), queued ? "true" : "false",
                  unsigned(ESP.getFreeHeap()));
    // There is deliberately NO application owner here. A queued command is
    // not executed; full application execution is covered by the actual Host path.
    delay(5);
}
