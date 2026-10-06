#include "Companion/ConfiguredProtocol.h"
#include "Companion/CommandFrame.h"
#include "Companion/ProfileCapabilities.h"
#include "GameConfiguration/ProfileReader.h"
#include "GameConfiguration/ProfileVocabulary.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
namespace larp {
namespace companion {
namespace {
// Overestimates include all fixed keys, ten digits per 32-bit counter,
// six digits per ID, longest booleans and 2x escaping of bounded texts.
static_assert(2000U + configuration::Limits::Components * 220U <= maxConfiguredStatusJsonBytes,
              "Configured status capacity must cover every component and escaped text");
static_assert(400U <= maxGameReplyJsonBytes, "Receipt capacity must cover the bounded fields");
const char *boolean(bool value) {
    return value ? "true" : "false";
}
// No temporary document, allocation or unescaped text. Text values reject
// controls and invalid UTF-8 at assignment; quote/backslash still need escaping.
class Json {
  public:
    Json(char *buffer, std::size_t capacity) : buffer_(buffer), capacity_(capacity) {
        if (buffer_ && capacity_)
            buffer_[0] = '\0';
        else
            good_ = false;
    }
    void append(const char *format, ...) {
        if (!good_)
            return;
        va_list args;
        va_start(args, format);
        const int result = std::vsnprintf(buffer_ + used_, capacity_ - used_, format, args);
        va_end(args);
        if (result < 0 || static_cast<std::size_t>(result) >= capacity_ - used_)
            good_ = false;
        else
            used_ += static_cast<std::size_t>(result);
    }
    void text(const char *value) {
        append("\"");
        for (const char *cursor = value; *cursor && good_; ++cursor) {
            if (*cursor == '"' || *cursor == '\\')
                append("\\%c", *cursor);
            else
                append("%c", *cursor);
        }
        append("\"");
    }
    ConfiguredEncodeResult finish(std::size_t &written) {
        if (!good_) {
            if (buffer_ && capacity_)
                buffer_[0] = '\0';
            written = 0;
            return ConfiguredEncodeResult::BufferTooSmall;
        }
        written = used_;
        return ConfiguredEncodeResult::Success;
    }

  private:
    char *buffer_;
    std::size_t capacity_, used_ = 0;
    bool good_ = true;
};
void clear(char *buffer, std::size_t capacity, std::size_t &written) {
    written = 0;
    if (buffer && capacity)
        buffer[0] = '\0';
}
} // namespace
const char *configuredPhaseName(ConfiguredPhase value) {
    switch (value) {
    case ConfiguredPhase::Empty:
        return "empty";
    case ConfiguredPhase::PendingActivation:
        return "pending_activation";
    case ConfiguredPhase::Applying:
        return "applying";
    case ConfiguredPhase::Saving:
        return "saving";
    case ConfiguredPhase::Ready:
        return "ready";
    case ConfiguredPhase::Fault:
        return "fault";
    }
    return nullptr;
}
const char *gameReplyOutcomeName(GameReplyOutcome value) {
    switch (value) {
    case GameReplyOutcome::Executed:
        return "executed";
    case GameReplyOutcome::ExecutionRejected:
        return "execution_rejected";
    case GameReplyOutcome::InvalidRequest:
        return "invalid_request";
    case GameReplyOutcome::WrongBoot:
        return "wrong_boot";
    case GameReplyOutcome::Unauthorized:
        return "unauthorized";
    case GameReplyOutcome::StaleConfiguration:
        return "stale_configuration";
    case GameReplyOutcome::StaleState:
        return "stale_state";
    case GameReplyOutcome::OutOfOrder:
        return "out_of_order";
    case GameReplyOutcome::ReceiptExpired:
        return "receipt_expired";
    case GameReplyOutcome::RequestConflict:
        return "request_conflict";
    case GameReplyOutcome::NotReady:
        return "not_ready";
    }
    return nullptr;
}
ConfiguredEncodeResult encodeConfiguredStatus(const ConfiguredStatus &s, char *buffer,
                                              std::size_t capacity, std::size_t &written) {
    clear(buffer, capacity, written);
    const char *phase = configuredPhaseName(s.phase);
    if (!s.boot.valid() || s.sample == 0 || !phase ||
        s.deviceCount > configuration::Limits::Components ||
        s.crystalCount > configuration::Limits::CrystalsPerRole ||
        s.requiredCrystals > configuration::Limits::CrystalsPerRole ||
        s.seenRoleCount > configuration::Limits::Roles || (s.running && s.state == 0) ||
        (s.paused && !s.running) || (s.rolePresent && s.role == 0))
        return ConfiguredEncodeResult::InvalidValue;
    for (std::size_t i = 0; i < s.deviceCount; ++i) {
        if (!s.devices[i].id)
            return ConfiguredEncodeResult::InvalidValue;
        for (std::size_t j = 0; j < i; ++j)
            if (s.devices[i].id == s.devices[j].id)
                return ConfiguredEncodeResult::InvalidValue;
    }
    Json json(buffer, std::min(capacity, maxConfiguredStatusJsonBytes));
    json.append(
        "{\"schemaVersion\":2,\"capabilities\":[\"status.configured.read\",\"game.control.core\"],"
        "\"boot\":\"%08lX%08lX\",\"sample\":%lu,\"sampledMilliseconds\":%lu,\"application\":{"
        "\"phase\":\"%s\","
        "\"configurationRevision\":%lu,\"profileRevision\":%lu,\"draftRevision\":%lu,"
        "\"deviceGeneration\":%lu,\"storedGeneration\":%lu,\"storedProfileRevision\":%lu,"
        "\"hasActive\":%s,\"draftReady\":%s,\"hardwareReady\":%s,\"storedMatchesActive\":%s},"
        "\"control\":{\"lease\":%lu,\"nextRequest\":%lu},\"game\":{\"running\":%s,\"paused\":%s,"
        "\"entryPending\":%s,\"state\":",
        (unsigned long)s.boot.high, (unsigned long)s.boot.low, (unsigned long)s.sample,
        (unsigned long)s.sampledMilliseconds, phase, (unsigned long)s.configurationRevision,
        (unsigned long)s.profileRevision, (unsigned long)s.draftRevision,
        (unsigned long)s.deviceGeneration, (unsigned long)s.storedGeneration,
        (unsigned long)s.storedProfileRevision, boolean(s.hasActive), boolean(s.draftReady),
        boolean(s.hardwareReady), boolean(s.storedMatchesActive), (unsigned long)s.controlLease,
        (unsigned long)s.nextRequest, boolean(s.running), boolean(s.paused),
        boolean(s.entryPending));
    if (s.running)
        json.append("%u", unsigned(s.state));
    else
        json.append("null");
    json.append(
        ",\"lastTransition\":%u,\"stateRevision\":%lu,\"stateElapsedMilliseconds\":%lu,\"role\":",
        unsigned(s.lastTransition), (unsigned long)s.stateRevision,
        (unsigned long)s.stateElapsedMilliseconds);
    if (s.rolePresent)
        json.append("%u", unsigned(s.role));
    else
        json.append("null");
    json.append(
        ",\"crystalCount\":%u,\"requiredCrystals\":%u,\"seenRoleCount\":%u,\"roundCount\":%lu,"
        "\"audioPending\":%s,\"incorrectCrystal\":%s,\"keystoneError\":%s,\"rejectedActions\":%lu,"
        "\"unavailableActions\":%lu,\"lastTickOutcome\":",
        unsigned(s.crystalCount), unsigned(s.requiredCrystals), unsigned(s.seenRoleCount),
        (unsigned long)s.roundCount, boolean(s.audioPending), boolean(s.incorrectCrystal),
        boolean(s.keystoneError), (unsigned long)s.rejectedActions,
        (unsigned long)s.unavailableActions);
    json.text(s.lastTickOutcome.c_str());
    json.append("},\"devices\":[");
    for (std::size_t i = 0; i < s.deviceCount; ++i) {
        const auto &d = s.devices[i];
        json.append("%s{\"id\":%u,\"enabled\":%s,\"available\":%s,\"cardReadable\":%s,"
                    "\"matrixReadable\":%s,"
                    "\"pollPending\":%s,\"lastOutcome\":",
                    i ? "," : "", unsigned(d.id), boolean(d.enabled), boolean(d.available),
                    boolean(d.cardReadable), boolean(d.matrixReadable), boolean(d.pollPending));
        json.text(d.lastOutcome.c_str());
        json.append("}");
    }
    json.append("]}");
    return json.finish(written);
}
const char *gameCommandKindName(GameCommandKind value) {
    switch (value) {
    case GameCommandKind::Start:
        return "start";
    case GameCommandKind::Stop:
        return "stop";
    case GameCommandKind::Pause:
        return "pause";
    case GameCommandKind::Resume:
        return "resume";
    case GameCommandKind::ChangeState:
        return "change_state";
    case GameCommandKind::TakeTransition:
        return "take_transition";
    }
    return nullptr;
}
ConfiguredEncodeResult encodeGameCommand(const GameCommand &c, char *buffer, std::size_t capacity,
                                         std::size_t &written) {
    clear(buffer, capacity, written);
    const char *kind = gameCommandKindName(c.kind);
    const bool target =
        c.kind == GameCommandKind::ChangeState || c.kind == GameCommandKind::TakeTransition;
    if (!kind || !c.boot.valid() || !c.lease || !c.request ||
        (target ? c.target == 0 : c.target != 0))
        return ConfiguredEncodeResult::InvalidValue;
    static_assert(400U <= maxGameCommandJsonBytes, "Canonical command fields must fit their cap");
    Json json(buffer, std::min(capacity, maxGameCommandJsonBytes));
    json.append("{\"schemaVersion\":2,\"boot\":\"%08lX%08lX\",\"lease\":%lu,\"request\":%lu,"
                "\"expectedConfigurationRevision\":%lu,\"expectedStateRevision\":%lu,\"kind\":\"%"
                "s\",\"target\":%u}",
                (unsigned long)c.boot.high, (unsigned long)c.boot.low, (unsigned long)c.lease,
                (unsigned long)c.request, (unsigned long)c.expectedConfigurationRevision,
                (unsigned long)c.expectedStateRevision, kind, unsigned(c.target));
    return json.finish(written);
}
ConfiguredEncodeResult encodeGameReply(const GameReply &r, char *buffer, std::size_t capacity,
                                       std::size_t &written) {
    clear(buffer, capacity, written);
    const char *outcome = gameReplyOutcomeName(r.outcome);
    if (!r.boot.valid() || !outcome)
        return ConfiguredEncodeResult::InvalidValue;
    Json json(buffer, std::min(capacity, maxGameReplyJsonBytes));
    json.append("{\"schemaVersion\":2,\"boot\":\"%08lX%08lX\",\"lease\":%lu,\"request\":%lu,"
                "\"nextRequest\":%lu,\"outcome\":\"%s\",\"duplicate\":%s,\"consumed\":%s,"
                "\"configurationRevision\":%lu,\"stateRevision\":%lu,\"executionOutcome\":",
                (unsigned long)r.boot.high, (unsigned long)r.boot.low, (unsigned long)r.lease,
                (unsigned long)r.request, (unsigned long)r.nextRequest, outcome,
                boolean(r.duplicate), boolean(r.consumed), (unsigned long)r.configurationRevision,
                (unsigned long)r.stateRevision);
    json.text(r.executionOutcome.c_str());
    json.append("}");
    return json.finish(written);
}
ConfiguredEncodeResult encodeProfileCapabilities(char *buffer, std::size_t capacity,
                                                 std::size_t &written) {
    using configuration::ImportLimits;
    using configuration::Limits;
    namespace words = configuration::vocabulary;
    static_assert(sizeof(configuration::Name) == 33, "Name byte capacity must be explicit");
    clear(buffer, capacity, written);
    Json json(buffer, std::min(capacity, maxProfileCapabilitiesJsonBytes));
    json.append("{\"schemaVersion\":1,\"scope\":\"profile_language\",\"profileSchemaVersion\":%u,"
                "\"gameSchemaVersion\":2,\"commandFrameVersion\":%u,\"limits\":{",
                static_cast<unsigned>(Limits::SchemaVersion),
                static_cast<unsigned>(CommandFrameLimits::Version));
    static const struct {
        const char *name;
        std::uint32_t value;
    } limits[] = {
        {"components", Limits::Components},
        {"states", Limits::States},
        {"transitions", Limits::Transitions},
        {"conditions", Limits::Conditions},
        {"actions", Limits::Actions},
        {"effects", Limits::Effects},
        {"roles", Limits::Roles},
        {"crystalsPerRole", Limits::CrystalsPerRole},
        {"stateActions", Limits::StateActions},
        {"transitionActions", Limits::TransitionActions},
        {"conditionDepth", Limits::ConditionDepth},
        {"matrixLines", Limits::MatrixLines},
        {"wakeInputs", Limits::WakeInputs},
        {"leds", Limits::Leds},
        {"paletteEntries", std::tuple_size<configuration::Palette>::value},
        {"roleUidBytes", std::tuple_size<decltype(configuration::Role::uid)>::value},
        {"componentPinSlots", std::tuple_size<decltype(configuration::Component::pins)>::value},
        {"expanderPinModes",
         std::tuple_size<decltype(configuration::Component::startupModes)>::value},
        {"peerAddressBytes", std::tuple_size<decltype(configuration::Component::peer)>::value},
        {"nameUtf8Bytes", sizeof(configuration::Name) - 1},
        {"profileDocumentBytes", ImportLimits::DocumentBytes},
        {"profileContainerDepth", ImportLimits::ContainerDepth},
        {"commandDocumentBytes", maxGameCommandDocumentBytes},
        {"commandFrameBytes", CommandFrameLimits::Frame},
        {"commandFramePayloadBytes", CommandFrameLimits::Payload},
        {"commandAssemblyMilliseconds", CommandFrameLimits::TimeoutMilliseconds},
        {"gameCommandQueue", ControlLimits::Queue},
        {"gameReceipts", ControlLimits::Receipts}};
    for (std::size_t i = 0; i < sizeof(limits) / sizeof(limits[0]); ++i) {
        if (i)
            json.append(",");
        json.text(limits[i].name);
        json.append(":%lu", static_cast<unsigned long>(limits[i].value));
    }
    json.append("},");
    static const struct {
        const char *name;
        const char *const *words;
        std::size_t count;
    } lists[] = {
        {"boards", words::BoardNames, sizeof(words::BoardNames) / sizeof(words::BoardNames[0])},
        {"componentKinds", words::ComponentKindNames,
         sizeof(words::ComponentKindNames) / sizeof(words::ComponentKindNames[0])},
        {"crystalCategories", words::CrystalCategoryNames,
         sizeof(words::CrystalCategoryNames) / sizeof(words::CrystalCategoryNames[0])},
        {"fields", words::FieldNames, sizeof(words::FieldNames) / sizeof(words::FieldNames[0])},
        {"comparisons", words::CompareNames,
         sizeof(words::CompareNames) / sizeof(words::CompareNames[0])},
        {"conditionKinds", words::ConditionKindNames,
         sizeof(words::ConditionKindNames) / sizeof(words::ConditionKindNames[0])},
        {"actionKinds", words::ActionKindNames,
         sizeof(words::ActionKindNames) / sizeof(words::ActionKindNames[0])},
        {"effectKinds", words::EffectKindNames,
         sizeof(words::EffectKindNames) / sizeof(words::EffectKindNames[0])}};
    for (std::size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); ++i) {
        if (i)
            json.append(",");
        json.text(lists[i].name);
        json.append(":[");
        for (std::size_t j = 0; j < lists[i].count; ++j) {
            if (j)
                json.append(",");
            json.text(lists[i].words[j]);
        }
        json.append("]");
    }
    json.append(",\"gameCommands\":[");
    for (unsigned i = 0; i <= static_cast<unsigned>(GameCommandKind::TakeTransition); ++i) {
        if (i)
            json.append(",");
        const char *name = gameCommandKindName(static_cast<GameCommandKind>(i));
        if (!name) {
            clear(buffer, capacity, written);
            return ConfiguredEncodeResult::InvalidValue;
        }
        json.text(name);
    }
    json.append("],\"constraints\":{\"configurationRequiresStoppedGame\":true,"
                "\"startRequiresStoredProfile\":true,\"singleGameMasterConnection\":true}}");
    return json.finish(written);
}
} // namespace companion
} // namespace larp
