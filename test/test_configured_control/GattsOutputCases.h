#pragma once
#include "PeerAuthCases.h"
struct OutputFixture : AuthFixture {
    explicit OutputFixture(bool gm = true, IPeerProofCrypto *crypto = nullptr,
                           std::uint32_t registration = 0)
        : AuthFixture(crypto, registration, gm, true) {}
    void granted() {
        issueNow();
        complete();
        confirm();
    }
    RoutedReplyFrame frame() {
        actor();
        TrustedAuthority route;
        TEST_ASSERT_TRUE(access.route(original.connection, route));
        RoutedReplyFrame value;
        value.principal = route.principal;
        value.epoch = route.epoch;
        value.count = 20;
        for (unsigned i = 0; i < value.count; ++i)
            value.bytes[i] = static_cast<char>(i + 1);
        worker();
        return value;
    }
    void readOutput(std::uint16_t offset = 0, bool isLong = false, bool need = true,
                    std::uint16_t packed = 257) {
        actor();
        sdk446::Request request = {};
        request.read = {0x48, offset, isLong, need};
        host_sdk_source::registeredCallbacks.request(packed, ++transaction, 1, &request);
        std::memset(&request, 0xaa, sizeof(request));
        worker();
    }
};
void outputCopy(ReplyFrameCopy expected, ReplyFrameCopy actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
void test_output_original_grant_whole_owned_value_busy_and_empty_poll() {
    OutputFixture fixture;
    RoutedReplyFrame denied;
    denied.principal = 7;
    denied.epoch = 1;
    denied.count = 20;
    outputCopy(ReplyFrameCopy::Unavailable, fixture.source.copyReplyFrame(denied));
    fixture.readOutput();
    TEST_ASSERT_EQUAL(8, host_sdk_source::readSent.back().status);
    TEST_ASSERT_EQUAL(0, host_sdk_source::readSent.back().response.value.length);
    fixture.granted();
    auto value = fixture.frame();
    const auto original = value;
    outputCopy(ReplyFrameCopy::Copied, fixture.source.copyReplyFrame(value));
    value.bytes.fill(0);
    outputCopy(ReplyFrameCopy::Busy, fixture.source.copyReplyFrame(value));
    fixture.readOutput();
    const auto &sent = host_sdk_source::readSent.back();
    TEST_ASSERT_EQUAL(0, sent.status);
    TEST_ASSERT_EQUAL(257, sent.connection);
    TEST_ASSERT_EQUAL(0x48, sent.response.value.handle);
    TEST_ASSERT_EQUAL(20, sent.response.value.length);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(original.bytes.data(), sent.response.value.value, 20);
    fixture.readOutput();
    TEST_ASSERT_EQUAL(0, host_sdk_source::readSent.back().status);
    TEST_ASSERT_EQUAL(0, host_sdk_source::readSent.back().response.value.length);
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
void test_output_no_automatic_role_from_authentication_or_unconfigured_endpoint() {
    OutputFixture observer(false);
    observer.granted();
    RoutedReplyFrame value;
    value.principal = 7;
    value.epoch = 1;
    value.count = 20;
    outputCopy(ReplyFrameCopy::Unavailable, observer.source.copyReplyFrame(value));
    observer.readOutput();
    TEST_ASSERT_EQUAL(8, host_sdk_source::readSent.back().status);
    AuthFixture oldVariant;
    oldVariant.issueNow();
    oldVariant.complete();
    oldVariant.confirm();
    oldVariant.actor();
    TEST_ASSERT_FALSE(oldVariant.source.configureOutputNow(0x48));
    oldVariant.worker();
    outputCopy(ReplyFrameCopy::Unavailable, oldVariant.source.copyReplyFrame(value));
}
void test_output_value_bounds_wrong_worker_route_and_blob_do_not_consume_slot() {
    OutputFixture fixture;
    fixture.granted();
    auto valid = fixture.frame();
    for (unsigned mode = 0; mode < 4; ++mode) {
        auto bad = valid;
        if (mode == 0)
            bad.count = 0;
        if (mode == 1)
            bad.count = 21;
        if (mode == 2)
            ++bad.epoch;
        if (mode == 3)
            ++bad.principal;
        outputCopy(ReplyFrameCopy::Unavailable, fixture.source.copyReplyFrame(bad));
    }
    fixture.actor();
    outputCopy(ReplyFrameCopy::Unavailable, fixture.source.copyReplyFrame(valid));
    fixture.worker();
    outputCopy(ReplyFrameCopy::Copied, fixture.source.copyReplyFrame(valid));
    host_sdk_source::currentTask = reinterpret_cast<TaskHandle_t>(4);
    outputCopy(ReplyFrameCopy::Unavailable, fixture.source.copyReplyFrame(valid));
    fixture.readOutput(1);
    TEST_ASSERT_EQUAL(7, host_sdk_source::readSent.back().status);
    fixture.readOutput(0, true);
    TEST_ASSERT_EQUAL(7, host_sdk_source::readSent.back().status);
    const auto before = host_sdk_source::readSent.size();
    fixture.readOutput(0, false, false);
    fixture.readOutput(0, false, true, 513);
    TEST_ASSERT_EQUAL(before, host_sdk_source::readSent.size());
    fixture.readOutput();
    TEST_ASSERT_EQUAL(20, host_sdk_source::readSent.back().response.value.length);
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
void test_output_absolute_expiry_busy_no_refresh_wrap_and_future() {
    for (unsigned mode = 0; mode < 4; ++mode) {
        OutputFixture fixture;
        fixture.granted();
        auto value = fixture.frame();
        f->now = mode == 2 ? UINT32_MAX - 10 : 100;
        const auto created = f->now;
        outputCopy(ReplyFrameCopy::Copied, fixture.source.copyReplyFrame(value));
        f->now = created + 999;
        outputCopy(ReplyFrameCopy::Busy, fixture.source.copyReplyFrame(value));
        f->now = mode == 0 ? created + 999 : mode == 3 ? created - 1 : created + 1000;
        fixture.readOutput();
        TEST_ASSERT_EQUAL(mode == 0 ? 0 : 14, host_sdk_source::readSent.back().status);
        TEST_ASSERT_EQUAL(mode == 0 ? 20 : 0,
                          host_sdk_source::readSent.back().response.value.length);
        fixture.readOutput();
        TEST_ASSERT_EQUAL(0, host_sdk_source::readSent.back().response.value.length);
    }
}
OutputFixture *outputFixture = nullptr;
RoutedReplyFrame *outputCaller = nullptr;
void outputRevoke() {
    outputFixture->actor();
    outputFixture->access.revokeGameMaster(testIdentity(2));
}
void outputReconnect() {
    outputFixture->actor();
    outputFixture->close();
    (void)outputFixture->open();
}
void outputConcurrentCopy() {
    outputFixture->worker();
    auto value = outputFixture->frame();
    outputCopy(ReplyFrameCopy::Copied, outputFixture->source.copyReplyFrame(value));
    outputFixture->actor();
}
void outputMutateCaller() {
    outputCaller->bytes.fill(0);
    outputCaller->count = 128;
    outputCaller->principal = 0;
}
void outputRecursiveRead() {
    outputFixture->readOutput();
    outputFixture->actor();
}
void test_output_recursive_read_cannot_send_the_same_slot_twice() {
    OutputFixture fixture;
    outputFixture = &fixture;
    fixture.granted();
    auto value = fixture.frame();
    outputCopy(ReplyFrameCopy::Copied, fixture.source.copyReplyFrame(value));
    const auto before = host_sdk_source::readSent.size();
    fixture.clock.before = outputRecursiveRead;
    fixture.readOutput();
    TEST_ASSERT_EQUAL(before + 1, host_sdk_source::readSent.size());
    TEST_ASSERT_EQUAL(20, host_sdk_source::readSent.back().response.value.length);
    outputFixture = nullptr;
}
unsigned outputSecurityReads = 0;
void outputExpireDuringFinalSecurity(std::uint8_t *) {
    if (++outputSecurityReads == 2)
        f->now += 1000;
    else
        host_sdk_security::beforeState = outputExpireDuringFinalSecurity;
}
void test_output_last_security_ports_cannot_extend_absolute_reply_deadline() {
    OutputFixture fixture;
    fixture.granted();
    auto value = fixture.frame();
    outputCopy(ReplyFrameCopy::Copied, fixture.source.copyReplyFrame(value));
    outputSecurityReads = 0;
    host_sdk_security::beforeState = outputExpireDuringFinalSecurity;
    fixture.readOutput();
    TEST_ASSERT_EQUAL(2, outputSecurityReads);
    TEST_ASSERT_EQUAL(14, host_sdk_source::readSent.back().status);
    TEST_ASSERT_EQUAL(0, host_sdk_source::readSent.back().response.value.length);
}
void test_output_rechecks_security_revoke_and_recycled_handle_before_actual_send() {
    for (unsigned mode = 0; mode < 5; ++mode) {
        OutputFixture fixture;
        outputFixture = &fixture;
        fixture.granted();
        auto value = fixture.frame();
        outputCopy(ReplyFrameCopy::Copied, fixture.source.copyReplyFrame(value));
        const auto before = host_sdk_source::readSent.size();
        if (mode == 0)
            host_sdk_security::flags = 0x12;
        if (mode == 1)
            host_sdk_security::keyBytes = 15;
        if (mode == 2)
            fixture.clock.before = outputRevoke;
        if (mode == 3)
            fixture.clock.before = outputReconnect;
        if (mode == 4)
            host_sdk_security::beforeConnection = outputRevoke;
        fixture.readOutput();
        if (host_sdk_source::readSent.size() != before) {
            TEST_ASSERT_EQUAL(8, host_sdk_source::readSent.back().status);
            TEST_ASSERT_EQUAL(0, host_sdk_source::readSent.back().response.value.length);
        }
        TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
    }
    outputFixture = nullptr;
}
void test_output_empty_poll_cannot_discard_concurrently_copied_value_and_caller_is_frozen() {
    OutputFixture fixture;
    outputFixture = &fixture;
    fixture.granted();
    fixture.clock.before = outputConcurrentCopy;
    fixture.readOutput();
    TEST_ASSERT_EQUAL(0, host_sdk_source::readSent.back().response.value.length);
    fixture.readOutput();
    TEST_ASSERT_EQUAL(20, host_sdk_source::readSent.back().response.value.length);
    auto value = fixture.frame();
    const auto original = value;
    outputCaller = &value;
    fixture.clock.before = outputMutateCaller;
    outputCopy(ReplyFrameCopy::Copied, fixture.source.copyReplyFrame(value));
    fixture.readOutput();
    TEST_ASSERT_EQUAL_HEX8_ARRAY(original.bytes.data(),
                                 host_sdk_source::readSent.back().response.value.value, 20);
    outputFixture = nullptr;
    outputCaller = nullptr;
}
void test_output_failed_sdk_submission_never_causes_automatic_replay() {
    OutputFixture fixture;
    fixture.granted();
    auto value = fixture.frame();
    outputCopy(ReplyFrameCopy::Copied, fixture.source.copyReplyFrame(value));
    host_sdk_source::sendResult = 1;
    fixture.readOutput();
    TEST_ASSERT_EQUAL(20, host_sdk_source::readSent.back().response.value.length);
    host_sdk_source::sendResult = 0;
    fixture.readOutput();
    TEST_ASSERT_EQUAL(0, host_sdk_source::readSent.back().response.value.length);
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
void test_output_service_optional_fifteen_handles_install_and_read_permission() {
    AuthSetupModel ports;
    PeerChallengeReadSetup setup;
    setup.boot = f->boot;
    setup.context = &ports;
    setup.ingress = &ports;
    setup.issue = &ports;
    setup.auth = &ports;
    setup.output = true;
    GattsServiceFixture sdk(setup);
    sdk.pending();
    const std::array<std::array<std::uint8_t, 16>, 5> uuids = {
        {GattsServiceDefinition::challenge(), GattsServiceDefinition::issue(),
         GattsServiceDefinition::proof(), GattsServiceDefinition::result(),
         GattsServiceDefinition::output()}};
    for (unsigned i = 0; i < uuids.size(); ++i) {
        auto p = sdk.attributeParameter(true);
        p.add_char.attr_handle = 0x26 + i * 2;
        std::memcpy(p.add_char.char_uuid.uuid.uuid128, uuids[i].data(), 16);
        sdk.copy(ESP_GATTS_ADD_CHAR_EVT, p);
        sdk.worker();
    }
    sdk.phase(GattsServicePhase::Installing);
    TEST_ASSERT_EQUAL(15, host_service_sdk::calls.front().handles);
    const auto &output = host_service_sdk::calls[7];
    TEST_ASSERT_EQUAL(4, output.permission);
    TEST_ASSERT_EQUAL(2, output.property);
    TEST_ASSERT_EQUAL(20, output.maximum);
    sdk.install();
    sdk.worker();
    sdk.started();
    TEST_ASSERT_TRUE(sdk.owner.ready());
    TEST_ASSERT_EQUAL(0x2e, sdk.owner.snapshot().setup.output);
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
void test_output_configuration_rejects_aliases_even_with_nonordered_auth_handles() {
    AuthSetupModel ports;
    SourceFixture fixture;
    TEST_ASSERT_TRUE(fixture.source.configurePeerProofGrantNow(f->boot, ports));
    TEST_ASSERT_TRUE(fixture.source.configureChallengeReadNow(0x44, ports));
    TEST_ASSERT_TRUE(fixture.source.configureChallengeIssueNow(0x45, ports));
    TEST_ASSERT_TRUE(fixture.source.configurePeerAuthNow(0x46, 0x20, ports));
    for (std::uint16_t attribute : {0x20, 0x42, 0x43, 0x44, 0x45, 0x46})
        TEST_ASSERT_FALSE(fixture.source.configureOutputNow(attribute));
    TEST_ASSERT_TRUE(fixture.source.configureOutputNow(0x48));
    TEST_ASSERT_FALSE(fixture.source.configureOutputNow(0x49));
}
#if defined(LARP_REAL_PEER_CRYPTO)
void outputActualP256(bool gm) {
    Esp32PeerProofCrypto crypto;
    OutputFixture fixture(gm, &crypto, 0x01020304);
    fixture.granted(); // Actual Mbed TLS verifies the fixed independent transcript/signature.
    ProfileResponseFrame response;
    response.command.transfer = 1;
    response.command.request = 2;
    response.command.operation = ProfileOperation::Apply;
    response.code = ProfileResponseCode::Saved;
    response.detail = ProfileResponseDetail::None;
    response.draftRevision = 4;
    response.configurationRevision = 2;
    RoutedReplyFrame value;
    TrustedAuthority observed;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(observed));
    value.epoch = observed.epoch;
    value.principal = 7;
    std::size_t count = 0;
    TEST_ASSERT_TRUE(
        encodeProfileResponse(response, value.bytes.data(), value.bytes.size(), count));
    value.count = static_cast<std::uint16_t>(count);
    outputCopy(gm ? ReplyFrameCopy::Copied : ReplyFrameCopy::Unavailable,
               fixture.source.copyReplyFrame(value));
    fixture.readOutput();
    const auto &sent = host_sdk_source::readSent.back();
    TEST_ASSERT_EQUAL(gm ? 0 : 8, sent.status);
    TEST_ASSERT_EQUAL(gm ? 20 : 0, sent.response.value.length);
    if (gm) {
        TEST_ASSERT_EQUAL_HEX8_ARRAY(value.bytes.data(), sent.response.value.value, 20);
        outputCopy(ReplyFrameCopy::Copied, fixture.source.copyReplyFrame(value));
        fixture.actor();
        accessOutcome(IdentityAccessOutcome::Revoked,
                      fixture.access.revokeGameMaster(testIdentity(2)));
        fixture.readOutput();
        TEST_ASSERT_EQUAL(8, host_sdk_source::readSent.back().status);
        TEST_ASSERT_EQUAL(0, host_sdk_source::readSent.back().response.value.length);
    }
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
void test_output_actual_p256_gm_response_and_revoke() {
    outputActualP256(true);
}
void test_output_actual_p256_authenticated_peer_gets_no_implicit_gm() {
    outputActualP256(false);
}
#endif
