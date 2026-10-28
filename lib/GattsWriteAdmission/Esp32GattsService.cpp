#include "Esp32GattsService.h"
namespace larp {
namespace companion {
std::uint16_t Esp32GattsService::handles() const noexcept {
    return challenge_.output    ? GattsServiceDefinition::AuthOutputHandles
           : challenge_.auth    ? GattsServiceDefinition::AuthHandles
           : challenge_.issue   ? GattsServiceDefinition::IssueHandles
           : challenge_.ingress ? GattsServiceDefinition::ChallengeHandles
                                : GattsServiceDefinition::Handles;
}
void Esp32GattsService::phase(GattsServicePhase next) noexcept {
    phase_ = next;
    portENTER_CRITICAL(&mutex_);
    publication_.phase = next;
    publication_.setup = setup_;
    portEXIT_CRITICAL(&mutex_);
}
bool Esp32GattsService::begin() noexcept {
    const auto task = xTaskGetCurrentTaskHandle();
    if (!task || (worker_ && worker_ != task))
        return false;
    if (worker_)
        return queue_ != nullptr && phase_ != GattsServicePhase::Failed;
    worker_ = task;
    startedAt_ = clock_.nowMilliseconds();
    queue_ = xQueueCreateStatic(Depth, sizeof(Copied), storage_, &control_);
    if ((challenge_.auth && !challenge_.issue) || (challenge_.output && !challenge_.auth) ||
        !queue_ || (challenge_.ingress && (!challenge_.context || !challenge_.boot.valid())) ||
        (!challenge_.ingress &&
         (challenge_.context || challenge_.boot.valid() || challenge_.issue))) {
        phase(GattsServicePhase::Failed);
        return false;
    }
    return queue_ != nullptr;
}
GattsServiceCopy Esp32GattsService::copyEvent(const GattsServiceEvent &caller) noexcept {
    const auto event = caller;
    if (event.kind == GattsServiceEventKind::Other ||
        (event.kind == GattsServiceEventKind::Registered &&
         event.application != GattsServiceDefinition::Application))
        return GattsServiceCopy::Ignored;
    if (!queue_)
        return GattsServiceCopy::Unavailable;
    GattsSourceRegistration original;
    if (!source_.readRegistration(original)) {
        portENTER_CRITICAL(&mutex_);
        broken_ = true;
        portEXIT_CRITICAL(&mutex_);
        return GattsServiceCopy::Unavailable;
    }
    if (original.interface != event.interface)
        return GattsServiceCopy::Ignored;
    const auto task = xTaskGetCurrentTaskHandle();
    portENTER_CRITICAL(&mutex_);
    if (!producer_ && task)
        producer_ = task;
    const bool producer = task && producer_ == task;
    if (!producer)
        broken_ = true;
    portEXIT_CRITICAL(&mutex_);
    if (!producer)
        return GattsServiceCopy::Unavailable;
    Copied value;
    value.original = original;
    value.event = event;
    value.receivedAt = clock_.nowMilliseconds();
    if (xQueueSendToBack(queue_, &value, 0) == pdTRUE)
        return GattsServiceCopy::Copied;
    portENTER_CRITICAL(&mutex_);
    broken_ = true;
    portEXIT_CRITICAL(&mutex_);
    return GattsServiceCopy::Overflow;
}
bool Esp32GattsService::current(const Copied &value) const noexcept {
    GattsSourceRegistration original;
    return source_.readRegistration(original) && original == value.original &&
           value.event.interface == value.original.interface;
}
GattsServiceSnapshot Esp32GattsService::snapshot() const noexcept {
    portENTER_CRITICAL(&mutex_);
    const auto result = publication_;
    portEXIT_CRITICAL(&mutex_);
    return result;
}
bool Esp32GattsService::ready() const noexcept {
    portENTER_CRITICAL(&mutex_);
    const auto value = publication_;
    const bool broken = broken_;
    portEXIT_CRITICAL(&mutex_);
    return !broken && value.phase == GattsServicePhase::Ready &&
           source_.endpointSetupState(value.setup) == GattsEndpointSetupState::Installed;
}
void Esp32GattsService::service() noexcept {
    if (busy_ || !worker_ || worker_ != xTaskGetCurrentTaskHandle() || !queue_ ||
        phase_ == GattsServicePhase::Failed)
        return;
    busy_ = true;
    portENTER_CRITICAL(&mutex_);
    const bool broken = broken_;
    portEXIT_CRITICAL(&mutex_);
    if (broken || (phase_ != GattsServicePhase::Ready &&
                   clock_.nowMilliseconds() - startedAt_ >=
                       GattsServiceDefinition::SetupDeadlineMilliseconds)) {
        phase(GattsServicePhase::Failed);
        busy_ = false;
        return;
    }
    Copied value;
    const bool received = xQueueReceive(queue_, &value, 0) == pdTRUE;
    if (received && (!current(value) || clock_.nowMilliseconds() - value.receivedAt >=
                                            GattsServiceDefinition::SetupDeadlineMilliseconds)) {
        phase(GattsServicePhase::Failed);
        busy_ = false;
        return;
    }
    const bool duplicate = received && havePrevious_ && previousOriginal_ == value.original &&
                           previous_ == value.event;
    if (received && !duplicate) {
        const auto &event = value.event;
        bool accepted = false, posted = false;
        if (event.success && phase_ == GattsServicePhase::Idle &&
            event.kind == GattsServiceEventKind::Registered) {
            setup_.original = value.original;
            phase(GattsServicePhase::Creating);
            accepted = posted = true;
            if (!driver_.create(value.original.interface, challenge_.ingress != nullptr,
                                challenge_.issue != nullptr, challenge_.auth != nullptr,
                                challenge_.output))
                phase(GattsServicePhase::Failed);
        } else if (event.success && phase_ == GattsServicePhase::Creating &&
                   event.kind == GattsServiceEventKind::Created && event.uuid128 &&
                   event.uuid == GattsServiceDefinition::service() && event.primary &&
                   !event.instance && event.service &&
                   static_cast<std::uint32_t>(event.service) + handles() - 1 <= UINT16_MAX) {
            setup_.service = event.service;
            phase(GattsServicePhase::Command);
            accepted = posted = true;
            if (!driver_.addCommand(setup_.service))
                phase(GattsServicePhase::Failed);
        } else if (event.success && phase_ == GattsServicePhase::Command &&
                   event.kind == GattsServiceEventKind::Attribute && event.uuid128 &&
                   event.uuid == GattsServiceDefinition::command() &&
                   event.service == setup_.service && event.attribute > setup_.service &&
                   event.attribute <= setup_.service + handles() - 1) {
            setup_.command = event.attribute;
            phase(GattsServicePhase::Profile);
            accepted = posted = true;
            if (!driver_.addProfile(setup_.service))
                phase(GattsServicePhase::Failed);
        } else if (event.success && phase_ == GattsServicePhase::Profile &&
                   event.kind == GattsServiceEventKind::Attribute && event.uuid128 &&
                   event.uuid == GattsServiceDefinition::profile() &&
                   event.service == setup_.service && event.attribute > setup_.command &&
                   event.attribute <= setup_.service + handles() - 1) {
            setup_.profile = event.attribute;
            accepted = posted = true;
            if (challenge_.ingress) {
                phase(GattsServicePhase::Challenge);
                if (!driver_.addChallenge(setup_.service))
                    phase(GattsServicePhase::Failed);
            } else {
                setup_.receivedAt = value.receivedAt;
                phase(GattsServicePhase::Installing);
                if (source_.postEndpointSetup(setup_, responses_) != GattsEndpointSetupPost::Posted)
                    phase(GattsServicePhase::Failed);
            }
        } else if (event.success && phase_ == GattsServicePhase::Challenge &&
                   event.kind == GattsServiceEventKind::Attribute && event.uuid128 &&
                   event.uuid == GattsServiceDefinition::challenge() &&
                   event.service == setup_.service && event.attribute > setup_.profile &&
                   event.attribute <= setup_.service + handles() - 1) {
            setup_.challenge = event.attribute;
            accepted = posted = true;
            if (challenge_.issue) {
                phase(GattsServicePhase::Issue);
                if (!driver_.addIssue(setup_.service))
                    phase(GattsServicePhase::Failed);
            } else {
                setup_.receivedAt = value.receivedAt;
                phase(GattsServicePhase::Installing);
                if (source_.postEndpointSetup(setup_, responses_, challenge_) !=
                    GattsEndpointSetupPost::Posted)
                    phase(GattsServicePhase::Failed);
            }
        } else if (event.success && phase_ == GattsServicePhase::Issue &&
                   event.kind == GattsServiceEventKind::Attribute && event.uuid128 &&
                   event.uuid == GattsServiceDefinition::issue() &&
                   event.service == setup_.service && event.attribute > setup_.challenge &&
                   event.attribute <= setup_.service + handles() - 1) {
            setup_.issue = event.attribute;
            accepted = posted = true;
            if (challenge_.auth) {
                phase(GattsServicePhase::Proof);
                if (!driver_.addProof(setup_.service))
                    phase(GattsServicePhase::Failed);
            } else {
                setup_.receivedAt = value.receivedAt;
                phase(GattsServicePhase::Installing);
                if (source_.postEndpointSetup(setup_, responses_, challenge_) !=
                    GattsEndpointSetupPost::Posted)
                    phase(GattsServicePhase::Failed);
            }
        } else if (event.success && phase_ == GattsServicePhase::Proof &&
                   event.kind == GattsServiceEventKind::Attribute && event.uuid128 &&
                   event.uuid == GattsServiceDefinition::proof() &&
                   event.service == setup_.service && event.attribute > setup_.issue &&
                   event.attribute <= setup_.service + handles() - 1) {
            setup_.proof = event.attribute;
            accepted = posted = true;
            phase(GattsServicePhase::Result);
            if (!driver_.addResult(setup_.service))
                phase(GattsServicePhase::Failed);
        } else if (event.success && phase_ == GattsServicePhase::Result &&
                   event.kind == GattsServiceEventKind::Attribute && event.uuid128 &&
                   event.uuid == GattsServiceDefinition::result() &&
                   event.service == setup_.service && event.attribute > setup_.proof &&
                   event.attribute <= setup_.service + handles() - 1) {
            setup_.result = event.attribute;
            accepted = posted = true;
            if (challenge_.output) {
                phase(GattsServicePhase::Output);
                if (!driver_.addOutput(setup_.service))
                    phase(GattsServicePhase::Failed);
            } else {
                setup_.receivedAt = value.receivedAt;
                phase(GattsServicePhase::Installing);
                if (source_.postEndpointSetup(setup_, responses_, challenge_) !=
                    GattsEndpointSetupPost::Posted)
                    phase(GattsServicePhase::Failed);
            }
        } else if (event.success && phase_ == GattsServicePhase::Output &&
                   event.kind == GattsServiceEventKind::Attribute && event.uuid128 &&
                   event.uuid == GattsServiceDefinition::output() &&
                   event.service == setup_.service && event.attribute > setup_.result &&
                   event.attribute <= setup_.service + handles() - 1) {
            setup_.output = event.attribute;
            setup_.receivedAt = value.receivedAt;
            accepted = posted = true;
            phase(GattsServicePhase::Installing);
            if (source_.postEndpointSetup(setup_, responses_, challenge_) !=
                GattsEndpointSetupPost::Posted)
                phase(GattsServicePhase::Failed);
        } else if (event.success && phase_ == GattsServicePhase::Starting &&
                   event.kind == GattsServiceEventKind::Started &&
                   event.service == setup_.service &&
                   source_.endpointSetupState(setup_) == GattsEndpointSetupState::Installed) {
            phase(GattsServicePhase::Ready);
            accepted = true;
        }
        if (!accepted)
            phase(GattsServicePhase::Failed);
        else {
            previous_ = event;
            previousOriginal_ = value.original;
            havePrevious_ = true;
        }
        // No second configuration/SDK post in the same worker invocation.
        if (posted) {
            busy_ = false;
            return;
        }
    }
    if (phase_ == GattsServicePhase::Installing) {
        const auto state = source_.endpointSetupState(setup_);
        if (state == GattsEndpointSetupState::Installed) {
            phase(GattsServicePhase::Starting);
            if (!driver_.start(setup_.service))
                phase(GattsServicePhase::Failed);
        } else if (state == GattsEndpointSetupState::Rejected ||
                   state == GattsEndpointSetupState::Unavailable)
            phase(GattsServicePhase::Failed);
    }
    busy_ = false;
}
} // namespace companion
} // namespace larp
