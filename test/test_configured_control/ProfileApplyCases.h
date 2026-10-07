#pragma once
larp::integration::ProfileImportReceipt acceptedImport() {
    ImportBytes source;
    source.bytes = importExample();
    ImportClock clock;
    const auto result = importSource(source, importEnvelope(source.bytes), clock);
    importOutcome(larp::integration::ProfileImportOutcome::Ready, result);
    TEST_ASSERT_TRUE(result.receipt.issued());
    TEST_ASSERT_TRUE(result.receipt.belongsTo(f->application));
    return result.receipt;
}
void applyOutcome(larp::integration::ProfileApplyOutcome expected,
                  const larp::integration::ProfileApplyResult &result) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(result.outcome));
}
larp::integration::ProfileApplyResult
applyReceipt(const larp::integration::ProfileImportReceipt &receipt) {
    return larp::integration::applyImportedProfile(f->application, f->channel.actual, receipt,
                                                   f->now);
}
void pumpProfileApply() {
    for (unsigned step = 0; step < 256 && f->application.busy(); ++step) {
        f->now += 5;
        f->owners.pump(f->now);
        f->application.service(f->now);
    }
    TEST_ASSERT_FALSE(f->application.busy());
}
void test_profile_apply_actual_queue_confirmation_and_current_wire_status() {
    const auto receipt = acceptedImport();
    const auto oldRecord = f->store.record(0);
    const auto applied = applyReceipt(receipt);
    applyOutcome(larp::integration::ProfileApplyOutcome::Queued, applied);
    const auto pending = f->application.snapshot();
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ApplicationPhase::Applying),
                      static_cast<unsigned>(pending.phase));
    TEST_ASSERT_EQUAL_UINT32(1, f->application.activation().configurationRevision);
    TEST_ASSERT_FALSE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(1, pending.storedGeneration);
    pumpProfileApply();
    const auto confirmed = f->application.snapshot();
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ApplicationPhase::Ready),
                      static_cast<unsigned>(confirmed.phase));
    TEST_ASSERT_TRUE(confirmed.storedMatchesActive);
    TEST_ASSERT_FALSE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(2, confirmed.storedGeneration);
    TEST_ASSERT_EQUAL_UINT32(2, f->application.activation().configurationRevision);
    TEST_ASSERT_EQUAL_UINT32(4, f->application.storage().activeProfileRevision);
    TEST_ASSERT_EQUAL_STRING(oldRecord.c_str(), f->store.record(0).c_str());
    char observation[1024];
    const int count = std::snprintf(
        observation, sizeof(observation),
        "{\"scope\":\"host_profile_apply\",\"transfer\":%u,\"importedDraftRevision\":%lu,"
        "\"queued\":{\"phase\":\"%s\",\"configurationRevision\":1,\"storedGeneration\":%lu,"
        "\"running\":false},"
        "\"confirmed\":{\"phase\":\"%s\",\"configurationRevision\":%lu,\"profileRevision\":%lu,"
        "\"storedGeneration\":%lu,\"storedMatchesActive\":true,\"running\":false},"
        "\"storageScope\":\"host_model\",\"deviceAcceptanceMeasured\":false}",
        receipt.source().transfer, static_cast<unsigned long>(receipt.draftRevision()),
        applicationPhaseName(pending.phase), static_cast<unsigned long>(pending.storedGeneration),
        applicationPhaseName(confirmed.phase),
        static_cast<unsigned long>(f->application.activation().configurationRevision),
        static_cast<unsigned long>(f->application.storage().activeProfileRevision),
        static_cast<unsigned long>(confirmed.storedGeneration));
    TEST_ASSERT_TRUE(count > 0 && static_cast<std::size_t>(count) < sizeof(observation));
    profileSourceFixture("apply-observation.json", std::string(observation, count) + char(10));
    f->now += ControlLimits::PublishMilliseconds;
    f->control.service(f->now);
    RoutedConfiguredStatus status;
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(status));
    TEST_ASSERT_EQUAL_UINT32(receipt.original().epoch, status.connectionEpoch);
    char encoded[maxConfiguredStatusJsonBytes];
    std::size_t written = 0;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ConfiguredEncodeResult::Success),
                      static_cast<unsigned>(encodeConfiguredStatus(status.status, encoded,
                                                                   sizeof(encoded), written)));
    profileSourceFixture("applied-status-v2.json", std::string(encoded, written) + char(10));
}
void test_profile_apply_default_receipt_has_no_authority_or_application_effect() {
    larp::integration::ProfileImportReceipt receipt;
    const auto oldRevision = f->application.storage().draftRevision;
    const auto reads = f->channel.reads;
    applyOutcome(
        larp::integration::ProfileApplyOutcome::InvalidReceipt,
        larp::integration::applyImportedProfile(f->application, f->channel, receipt, f->now));
    TEST_ASSERT_EQUAL_UINT32(reads, f->channel.reads);
    TEST_ASSERT_EQUAL_UINT32(oldRevision, f->application.storage().draftRevision);
    TEST_ASSERT_FALSE(f->application.busy());
}
void test_profile_apply_wrong_application_rejected_even_with_same_revisions() {
    const auto receipt = acceptedImport();
    BootFixture other(f->store);
    other.ready();
    TEST_ASSERT_TRUE(
        other.app.createDraft(FactoryProfile::Hint, other.app.storage().draftRevision));
    TEST_ASSERT_EQUAL_UINT32(receipt.draftRevision(), other.app.storage().draftRevision);
    TEST_ASSERT_EQUAL_UINT32(receipt.source().expectedConfigurationRevision,
                             other.app.activation().configurationRevision);
    const auto reads = f->channel.reads;
    applyOutcome(larp::integration::ProfileApplyOutcome::WrongApplication,
                 larp::integration::applyImportedProfile(other.app, f->channel, receipt, f->now));
    TEST_ASSERT_EQUAL_UINT32(reads, f->channel.reads);
    TEST_ASSERT_FALSE(other.app.busy());
}
void test_profile_apply_original_epoch_cannot_be_reissued_after_revocation() {
    const auto receipt = acceptedImport();
    f->channel.actual.suspendAuthority();
    applyOutcome(larp::integration::ProfileApplyOutcome::Unauthorized, applyReceipt(receipt));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(f->channel.actual.updateAuthority(authority(3))));
    applyOutcome(larp::integration::ProfileApplyOutcome::Unauthorized, applyReceipt(receipt));
    TEST_ASSERT_EQUAL_UINT32(receipt.draftRevision(), f->application.storage().draftRevision);
    TEST_ASSERT_FALSE(f->application.busy());
}
void test_profile_apply_local_draft_edit_invalidates_old_receipt() {
    const auto receipt = acceptedImport();
    TEST_ASSERT_TRUE(f->application.createDraft(FactoryProfile::Standard, receipt.draftRevision()));
    applyOutcome(larp::integration::ProfileApplyOutcome::StaleRevision, applyReceipt(receipt));
    TEST_ASSERT_FALSE(f->application.busy());
    TEST_ASSERT_EQUAL_UINT32(1, f->application.activation().configurationRevision);
}
void test_profile_apply_game_start_after_import_denies_apply_without_stopping_game() {
    const auto receipt = acceptedImport();
    const auto started = f->execute(f->command());
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(started.reply.outcome));
    const auto stateRevision = f->application.execution().stateRevision;
    const auto request = f->control.snapshot().nextRequest;
    applyOutcome(larp::integration::ProfileApplyOutcome::GameRunning, applyReceipt(receipt));
    TEST_ASSERT_TRUE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(stateRevision, f->application.execution().stateRevision);
    TEST_ASSERT_EQUAL_UINT32(request, f->control.snapshot().nextRequest);
    TEST_ASSERT_EQUAL_UINT32(1, f->application.activation().configurationRevision);
}
void test_profile_apply_rechecks_original_authority_after_admission_selection() {
    const auto receipt = acceptedImport();
    f->channel.revokeBefore = f->channel.reads + 2;
    applyOutcome(
        larp::integration::ProfileApplyOutcome::Unauthorized,
        larp::integration::applyImportedProfile(f->application, f->channel, receipt, f->now));
    TEST_ASSERT_FALSE(f->application.busy());
    TEST_ASSERT_EQUAL_UINT32(receipt.draftRevision(), f->application.storage().draftRevision);
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_profile_apply_duplicate_is_busy_then_stale_without_repeating_save() {
    const auto receipt = acceptedImport();
    applyOutcome(larp::integration::ProfileApplyOutcome::Queued, applyReceipt(receipt));
    applyOutcome(larp::integration::ProfileApplyOutcome::Busy, applyReceipt(receipt));
    pumpProfileApply();
    const auto writes = f->store.writeCalls;
    applyOutcome(larp::integration::ProfileApplyOutcome::StaleRevision, applyReceipt(receipt));
    TEST_ASSERT_EQUAL_UINT32(writes, f->store.writeCalls);
    TEST_ASSERT_FALSE(f->application.busy());
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_profile_apply_later_revoke_preserves_already_selected_transaction() {
    const auto receipt = acceptedImport();
    applyOutcome(larp::integration::ProfileApplyOutcome::Queued, applyReceipt(receipt));
    f->channel.actual.suspendAuthority();
    pumpProfileApply();
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ApplicationPhase::Ready),
                      static_cast<unsigned>(f->application.snapshot().phase));
    TEST_ASSERT_TRUE(f->application.snapshot().storedMatchesActive);
    TEST_ASSERT_FALSE(f->application.gameRunning());
    applyOutcome(larp::integration::ProfileApplyOutcome::Unauthorized, applyReceipt(receipt));
    f->now += ControlLimits::PublishMilliseconds;
    f->control.service(f->now);
    RoutedConfiguredStatus status;
    TEST_ASSERT_FALSE(f->channel.actual.readCurrentStatus(status));
}
void test_profile_apply_uncertain_storage_is_fault_not_confirmed_or_started() {
    const auto receipt = acceptedImport();
    const auto oldRecord = f->store.record(0);
    f->store.uncertainFinish = true;
    applyOutcome(larp::integration::ProfileApplyOutcome::Queued, applyReceipt(receipt));
    pumpProfileApply();
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ApplicationPhase::Fault),
                      static_cast<unsigned>(f->application.snapshot().phase));
    TEST_ASSERT_FALSE(f->application.snapshot().storedMatchesActive);
    TEST_ASSERT_TRUE(f->application.persistence().writeUncertain);
    TEST_ASSERT_EQUAL_STRING(oldRecord.c_str(), f->store.record(0).c_str());
    TEST_ASSERT_FALSE(
        f->application.startGame(f->application.activation().configurationRevision, f->now));
    TEST_ASSERT_FALSE(f->application.gameRunning());
    applyOutcome(larp::integration::ProfileApplyOutcome::NotReady, applyReceipt(receipt));
    f->now += ControlLimits::PublishMilliseconds;
    f->control.service(f->now);
    RoutedConfiguredStatus status;
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(status));
    char encoded[maxConfiguredStatusJsonBytes];
    std::size_t written = 0;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ConfiguredEncodeResult::Success),
                      static_cast<unsigned>(encodeConfiguredStatus(status.status, encoded,
                                                                   sizeof(encoded), written)));
    profileSourceFixture("uncertain-status-v2.json", std::string(encoded, written) + char(10));
}
class MutatingReceiptChannel : public IGameControlChannel {
  public:
    larp::integration::ProfileImportReceipt *receipt = nullptr;
    bool readAuthority(TrustedAuthority &value) const noexcept override {
        if (receipt)
            *receipt = larp::integration::ProfileImportReceipt();
        return f->channel.actual.readAuthority(value);
    }
    bool receive(TrustedGameRequest &) noexcept override {
        return false;
    }
    bool send(const RoutedGameReply &) noexcept override {
        return false;
    }
    bool publish(const RoutedConfiguredStatus &) noexcept override {
        return false;
    }
};
void test_profile_apply_copies_original_receipt_before_external_observation() {
    auto receipt = acceptedImport();
    MutatingReceiptChannel channel;
    channel.receipt = &receipt;
    applyOutcome(larp::integration::ProfileApplyOutcome::Queued,
                 larp::integration::applyImportedProfile(f->application, channel, receipt, f->now));
    TEST_ASSERT_FALSE(receipt.issued());
    TEST_ASSERT_TRUE(f->application.busy());
}
void test_profile_apply_late_unaccepted_import_cannot_use_locally_parsed_draft() {
    ImportBytes source;
    source.bytes = importExample();
    source.revokeAfterEnd = true;
    ImportClock clock;
    const auto result = importSource(source, importEnvelope(source.bytes), clock);
    importOutcome(larp::integration::ProfileImportOutcome::Unauthorized, result);
    TEST_ASSERT_TRUE(result.draft);
    TEST_ASSERT_TRUE(f->application.storage().draftReady);
    TEST_ASSERT_FALSE(result.receipt.issued());
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(f->channel.actual.updateAuthority(authority(3))));
    applyOutcome(larp::integration::ProfileApplyOutcome::InvalidReceipt,
                 applyReceipt(result.receipt));
    TEST_ASSERT_FALSE(f->application.busy());
    TEST_ASSERT_EQUAL_UINT32(1, f->application.activation().configurationRevision);
}
