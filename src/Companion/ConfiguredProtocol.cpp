#include "Companion/ConfiguredProtocol.h"
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
} // namespace companion
} // namespace larp
