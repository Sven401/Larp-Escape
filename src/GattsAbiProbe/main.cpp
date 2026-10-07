// Compile-only SDK ABI/queue probe. SYNTHETIC verifier and responses.
// No Bluetooth initialization, advertising, radio, keys or device owners.
#include "Companion/CommandStream.h"
#include "Companion/IdentityAccess.h"
#include <Arduino.h>
#include <Esp32DeviceIdentityStore.h>
#include <Esp32GattsService.h>
#include <Esp32GattsSource.h>
#include <Esp32PeerProofCrypto.h>
#include <Esp32PeerProofNonce.h>
#include <FreeRtosCommandFrameQueue.h>
#include <FreeRtosGameControlChannel.h>
#include <FreeRtosGattsResponseQueue.h>
#include <FreeRtosPeerProofQueue.h>
#include <FreeRtosProfileFrameQueue.h>
#include <FreeRtosProfileResponseQueue.h>
#include <GattsWriteAdmission.h>
#include <SdkGattsLifecycle.h>
#include <algorithm>
namespace {
using namespace larp::companion;
FreeRtosGameControlChannel channel;
// SDK/NVS symbols link only; no begin/read/write/erase in this ABIprobe.
Esp32DeviceIdentityStore unopenedIdentityStore;
FreeRtosCommandFrameQueue queue(channel);
FreeRtosProfileFrameQueue profileQueue(channel);
class ProbeClock : public IProfileImportClock {
  public:
    std::uint32_t nowMilliseconds() const noexcept override {
        return millis();
    }
};
ProbeClock profileClock;
FreeRtosProfileResponseQueue profileResponses(channel, profileClock);
IdentityAccess identityPolicy(channel);
BoundConnection connection;
CommandStream stream(channel);
GattsWriteBinding original;
std::array<char, maxGameCommandJsonBytes> document = {};
std::array<char, 20> frame = {};
std::size_t length = 0, offset = 0;
bool ready = false, queued = false;
GattsEndpoints probeEndpoints() {
    GattsEndpoints endpoints;
    endpoints.interface = 1;
    endpoints.command = 0x42;
    endpoints.profile = 0x43;
    return endpoints;
}
FreeRtosGattsConnection responseContext(identityPolicy, profileClock, probeEndpoints());
FreeRtosGattsResponseQueue responses(responseContext);
class SyntheticResponseSubmitter : public IGattsResponseSubmitter {
  public:
    RoutedWriteResponse last;
    std::uint32_t copies = 0;
    bool submitOriginal(const RoutedWriteResponse &response) noexcept override {
        if (millis() - response.receivedAt >= GattsResponseLimits::MaxAgeMilliseconds ||
            !responseContext.originalLive(response.original))
            return false;
        last = response;
        if (copies != UINT32_MAX)
            ++copies;
        return true; // Synthetic posting; no esp_ble_gatts_send_response call.
    }
} responseSubmitter;
GattsResponseDispatcher responseWorker(responses, responseSubmitter);
Esp32GattsSource sourcePort(identityPolicy, profileClock, channel, queue, profileQueue);
FreeRtosGattsResponseQueue sourceResponses(sourcePort);
class UnopenedPeerContext : public IPeerProofContext {
  public:
    bool originalLive(const PeerProofBinding &) const noexcept override {
        return false;
    }
    bool credentialCurrent(const PeerCredential &) const noexcept override {
        return false;
    }
} unopenedPeerContext;
Esp32PeerProofCrypto peerCrypto;
Esp32PeerProofNonce peerNonce;
PeerProofOwner peerProofOwner(unopenedPeerContext, peerCrypto, peerNonce, profileClock);
PeerProofReceiver peerProofReceiver(peerProofOwner);
FreeRtosPeerProofQueue peerProofQueue(unopenedPeerContext, profileClock);
SdkGattsServiceDriver serviceDriver;
Esp32GattsService sourceService(sourcePort, sourceResponses, profileClock, serviceDriver);
} // namespace
void setup() {
    Serial.begin(115200);
    if (!channel.begin() || !queue.begin() || !profileQueue.begin() || !responses.begin())
        return;
    if (!sourceResponses.begin() || !armGattsSourceBeforeSdkStart(0x4c45, sourcePort) ||
        sourcePort.registeredSourceNow(1) ||
        sourcePort.configureEndpointsNow(0x42, 0x43, sourceResponses))
        return; // Actual uninitialized SDK branch; no BTU task/register/radio started.
    Serial.printf(
        "gatts_source_abi: network=false initialized=false source_registered=false port=%u\n",
        unsigned(sizeof(sourcePort)));
    if (!sourceService.begin())
        return;
    esp_ble_gatts_cb_param_t serviceParameter = {};
    serviceParameter.reg.app_id = GattsServiceDefinition::Application;
    const auto serviceEvent = fromSdkGattsService(ESP_GATTS_REG_EVT, 1, serviceParameter);
    if (sourceService.copyEvent(serviceEvent) != GattsServiceCopy::Unavailable)
        return;
    sourceService.service();
    if (sourceService.ready() || sourceService.snapshot().phase != GattsServicePhase::Failed)
        return;
    Serial.printf("gatts_service_abi: network=false initialized=false sdk_created=false owner=%u\n",
                  unsigned(sizeof(sourceService)));
    if (!peerProofQueue.begin() ||
        peerProofOwner.issue(PeerProofBinding{}, PeerCredential{}) !=
            PeerProofOutcome::InvalidContext ||
        dispatchPeerProofFrame(peerProofQueue, peerProofReceiver).received)
        return;
    Serial.printf("peer_proof_abi: initialized=false owner=%u receiver=%u queue=%u item=%u\n",
                  unsigned(sizeof(peerProofOwner)), unsigned(sizeof(peerProofReceiver)),
                  unsigned(sizeof(peerProofQueue)), unsigned(sizeof(CopiedPeerProofFrame)));
    PeerNonce unopenedNonce;
    unopenedNonce[0] = 99;
    if (peerNonce.make(unopenedNonce) || unopenedNonce[0] != 99 ||
        peerCrypto.validKey(PeerPublicKey{}) ||
        peerCrypto.verify(PeerPublicKey{}, PeerTranscript{}, PeerSignature{}))
        return;
    Serial.printf("peer_crypto_abi: initialized=false nonce_generated=false key_verified=false "
                  "crypto=%u nonce=%u\n",
                  unsigned(sizeof(peerCrypto)), unsigned(sizeof(peerNonce)));
    // Actual synchronized retirement port; no peer/security grant from SDK fields.
    TrustedAuthority firstClosed, replacementClosed, closed, observed;
    closed.epoch = 99;
    if (channel.advanceAuthority(0, false, firstClosed) != AuthorityUpdate::Published ||
        channel.advanceAuthority(0, false, replacementClosed) != AuthorityUpdate::Published ||
        channel.closeAuthorityIfCurrent(firstClosed, closed) != AuthorityUpdate::Conflict ||
        closed.epoch != 99 || !channel.readAuthority(observed) ||
        !(observed == replacementClosed) ||
        channel.closeAuthorityIfCurrent(replacementClosed, closed) != AuthorityUpdate::Published ||
        closed.gameMaster || closed.principal || closed.epoch != replacementClosed.epoch + 1)
        return;
    Serial.printf(
        "authority_retirement_probe: synthetic=true network=false replacement_preserved=true "
        "closed_epoch=%u\n",
        unsigned(closed.epoch));
    Identity device, peer;
    device.bytes[0] = 1;
    peer.bytes[0] = 2;
    VerifiedPeerEvidence evidence;
    evidence.peer = peer;
    evidence.encrypted = evidence.mitm = evidence.secureConnections = evidence.bonded = true;
    if (identityPolicy.begin(device) != IdentityAccessOutcome::Initialized)
        return;
    esp_ble_gatts_cb_param_t connectParameter = {};
    connectParameter.connect.conn_id = 0;
    connectParameter.connect.remote_bda[0] = 1;
    connectParameter.connect.link_role = 0; // No GM/peer rights from link role.
    connectParameter.connect.conn_handle = 99;
    const auto connectEvent = fromSdkGattsConnect(1, connectParameter);
    BoundConnection first;
    GattsWriteBinding retired;
    GattsWriteEvent selected;
    selected.interface = 1;
    selected.attribute = 0x42;
    selected.peerAddress[0] = 1;
    esp_ble_gatts_cb_param_t disconnectParameter = {};
    disconnectParameter.disconnect.conn_id = 0;
    disconnectParameter.disconnect.remote_bda[0] = 1;
    if (responseContext.connectNow(connectEvent, first) != GattsLifecycleOutcome::Opened ||
        !responseContext.captureWriteNow(selected, GattsEndpoint::Command, retired) ||
        retired.authority.gameMaster ||
        responseContext.disconnectNow(fromSdkGattsDisconnect(1, disconnectParameter)) !=
            GattsLifecycleOutcome::Closed ||
        responseContext.connectNow(connectEvent, connection) != GattsLifecycleOutcome::Opened ||
        connection.token <= first.token || responseContext.originalLive(retired) ||
        responseContext.retireOriginal(retired) != GattsLifecycleOutcome::Ignored)
        return;
    Serial.printf("gatts_lifecycle_probe: synthetic=true network=false original_token=%u "
                  "reconnected_token=%u sdk_ordering_proven=false\n",
                  unsigned(first.token), unsigned(connection.token));
    if (identityPolicy.authenticate(connection, evidence) != IdentityAccessOutcome::Verified ||
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
    DeviceIdentityRecord unopenedDestination;
    unopenedDestination.bytes[0] = 99;
    if (unopenedIdentityStore.read(unopenedDestination) != IdentityRecordRead::Failure ||
        unopenedDestination.bytes[0] != 99)
        return; // Actual adapter fail-closed branch, still no NVS initialization/IO.
    Serial.printf("identity_store_abi: network=false initialized=false bytes=%u\n",
                  unsigned(sizeof(unopenedIdentityStore)));
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
            rejected.status != AttWriteStatus::RequestUnsupported ||
            responseWorker.service() != GattsResponseDispatch::Submitted)
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
    ProfileControlFrame control;
    control.transfer = 1;
    control.request = 1;
    control.operation = ProfileOperation::Finish;
    if (!encodeProfileControl(control, profileValue, sizeof(profileValue), profileWritten) ||
        profileQueue.copyWrite(profileValue, profileWritten, original.authority.principal,
                               original.authority.epoch, millis()) != ProfileCopyOutcome::Copied ||
        !profileQueue.take(profileCopy) || profileCopy.count != ProfileControlLimits::CommandBytes)
        return;
    Serial.printf("profile_control_probe: synthetic=true network=false bytes=%u imported=false "
                  "applied=false\n",
                  unsigned(profileWritten));
    esp_ble_gatts_cb_param_t profileParameter = {};
    profileParameter.write.conn_id = original.connection.handle;
    profileParameter.write.trans_id = 18;
    profileParameter.write.handle = 0x43;
    profileParameter.write.len = profileWritten;
    profileParameter.write.value = reinterpret_cast<std::uint8_t *>(profileValue);
    profileParameter.write.need_rsp = true;
    for (std::size_t i = 0; i < original.peerAddress.size(); ++i)
        profileParameter.write.bda[i] = original.peerAddress[i];
    const auto profileEvent = fromSdkGattsWrite(original.interface, profileParameter);
    const auto profileAdmitted = admitOriginalGattsProfileWrite(
        profileEvent, responseContext, channel, profileQueue, responses, millis());
    if (profileAdmitted.outcome != GattsWriteOutcome::Copied ||
        profileAdmitted.status != AttWriteStatus::Ok || !profileQueue.take(profileCopy) ||
        responseWorker.service() != GattsResponseDispatch::Submitted)
        return;
    Serial.printf(
        "profile_raw_write_probe: synthetic=true network=false response_copied=%s applied=false\n",
        profileAdmitted.responseCopied ? "true" : "false");
    RoutedProfileResponse response;
    response.original = original.authority;
    response.producedAt = millis();
    response.response.command = control;
    response.response.code = ProfileResponseCode::DraftReady;
    response.response.detail = ProfileResponseDetail::None;
    response.response.draftRevision = response.response.configurationRevision = 1;
    RoutedProfileResponse copiedResponse;
    if (!profileResponses.begin() ||
        profileResponses.copyResponse(response) != ProfileResponseCopy::Copied ||
        !profileResponses.takeCurrent(copiedResponse))
        return;
    Serial.printf(
        "profile_response_queue_probe: synthetic=true network=false queue=%u item=%u saved=false\n",
        unsigned(sizeof(profileResponses)), unsigned(sizeof(copiedResponse)));
    Serial.printf("att_response_queue_probe: synthetic=true network=false queue=%u item=%u "
                  "age_ms=%u sdk_posted=false peer_delivered=false\n",
                  unsigned(sizeof(responses)), unsigned(sizeof(RoutedWriteResponse)),
                  unsigned(GattsResponseLimits::MaxAgeMilliseconds));
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
    const auto admitted =
        admitOriginalGattsWrite(event, responseContext, channel, queue, responses, now);
    if (admitted.outcome != GattsWriteOutcome::Copied) {
        ready = false;
        return;
    }
    if (responseWorker.service() != GattsResponseDispatch::Submitted) {
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
    DeviceIdentityRecord unopenedDestination;
    unopenedDestination.bytes[0] = 99;
    if (unopenedIdentityStore.read(unopenedDestination) != IdentityRecordRead::Failure ||
        unopenedDestination.bytes[0] != 99)
        return; // Actual adapter fail-closed branch, still no NVS initialization/IO.
    Serial.printf("identity_store_abi: network=false initialized=false bytes=%u\n",
                  unsigned(sizeof(unopenedIdentityStore)));
    Serial.printf("gatts_abi_probe: copied=%u local_response_copied=%s stream=%u "
                  "command_queued=%s executed=false heap=%u\n",
                  unsigned(offset), admitted.responseCopied ? "true" : "false",
                  unsigned(dispatch.stream.outcome), queued ? "true" : "false",
                  unsigned(ESP.getFreeHeap()));
    // There is deliberately NO application owner here. A queued command is
    // not executed; full application execution is covered by the actual Host path.
    delay(5);
}
