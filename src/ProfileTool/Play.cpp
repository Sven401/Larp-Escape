#include "Play.h"
#include "GameConfiguration/ProfileVocabulary.h"
#include "GameExecution/Session.h"
#include <cstring>
#include <limits>
#include <memory>

namespace {
using namespace larp::configuration;
using namespace larp::execution;
class Play : public IProfileDevices {
  public:
    explicit Play(const Profile &profile) : session(*this), profile_(profile) {
        for (std::size_t index = 0; index < profile.components.size(); ++index)
            samples_[index].available = profile.components.get(index)->enabled;
    }
    bool sample(const Component &component, DeviceSample &value) noexcept override {
        const std::size_t slot = componentSlot(component.id);
        if (slot == Limits::Components)
            return false;
        value = samples_[slot];
        return true;
    }
    PollOutcome pollCard(const PollRequest &request, larp::domain::CardUid &uid) noexcept override {
        const std::size_t slot = componentSlot(request.component, ComponentKind::NfcSpi);
        if (slot == Limits::Components || !samples_[slot].available)
            return PollOutcome::Unavailable;
        uid = cards_[slot];
        return PollOutcome::Fresh;
    }
    PollOutcome pollCrystals(const PollRequest &request,
                             larp::ports::CrystalInputBatch &batch) noexcept override {
        const std::size_t slot = componentSlot(request.component, ComponentKind::Matrix);
        if (slot == Limits::Components || !samples_[slot].available)
            return PollOutcome::Unavailable;
        batch = batches_[slot];
        return PollOutcome::Fresh;
    }
    ActionOutcome playTrack(ComponentId component, std::uint16_t track,
                            const ActionContext &context) noexcept override {
        const std::size_t slot = componentSlot(component, ComponentKind::AudioUart);
        const ActionOutcome outcome = slot == Limits::Components || !samples_[slot].available
                                          ? ActionOutcome::Unavailable
                                          : ActionOutcome::Applied;
        if (outcome == ActionOutcome::Applied)
            samples_[slot].audioBusy = true;
        trace("play", component, context, outcome);
        std::printf(",\"track\":%u}\n", track);
        return outcome;
    }
    ActionOutcome wakeAudio(ComponentId component, const ActionContext &context) noexcept override {
        return simple("wake", component, ComponentKind::AudioUart, context);
    }
    ActionOutcome sendActivation(ComponentId component,
                                 const ActionContext &context) noexcept override {
        return simple("activation", component, ComponentKind::EspNow, context);
    }
    ActionOutcome applyEffect(const Effect &effect, ActionId binding,
                              const ActionContext &context) noexcept override {
        const std::size_t slot = componentSlot(effect.component, ComponentKind::LedStrip);
        const ActionOutcome outcome = slot == Limits::Components || !samples_[slot].available
                                          ? ActionOutcome::Unavailable
                                          : ActionOutcome::Applied;
        trace("effect", effect.component, context, outcome);
        std::printf(",\"binding\":%u,\"effect\":%u,\"kind\":\"%s\",\"rgb\":%lu,"
                    "\"frames\":%u,\"paletteFirst\":%lu}\n",
                    binding.value, effect.id.value,
                    vocabulary::EffectKindNames[static_cast<unsigned>(effect.kind)],
                    static_cast<unsigned long>(effect.colorRgb), effect.frames,
                    static_cast<unsigned long>(effect.palette[0]));
        return outcome;
    }
    bool card(std::uint32_t component, std::uint32_t role) {
        const std::size_t slot = componentSlot(component, ComponentKind::NfcSpi);
        if (slot == Limits::Components)
            return false;
        if (role == 0) {
            cards_[slot] = {};
            return true;
        }
        for (std::size_t index = 0; index < profile_.roles.size(); ++index)
            if (profile_.roles.get(index)->id.value == role) {
                cards_[slot] = profile_.roles.get(index)->uid;
                return true;
            }
        return false;
    }
    bool wakes(std::uint32_t component, std::uint32_t mask) {
        const std::size_t slot = componentSlot(component, ComponentKind::Matrix);
        if (slot == Limits::Components)
            return false;
        const std::size_t count = profile_.components.get(slot)->wakeInputs.size();
        if (mask >= (1U << count))
            return false;
        for (std::size_t index = 0; index < Limits::WakeInputs; ++index)
            samples_[slot].wakes[index] = (mask & (1U << index)) != 0;
        return true;
    }
    bool crystals(std::uint32_t component, const larp::ports::CrystalInputBatch &batch) {
        const std::size_t slot = componentSlot(component, ComponentKind::Matrix);
        if (slot == Limits::Components)
            return false;
        batches_[slot] = batch;
        return true;
    }
    bool finish(std::uint32_t component) {
        const std::size_t slot = componentSlot(component, ComponentKind::AudioUart);
        if (slot == Limits::Components)
            return false;
        samples_[slot].audioBusy = false;
        return true;
    }
    bool available(std::uint32_t component, std::uint32_t value) {
        const std::size_t slot = componentSlot(component);
        if (slot == Limits::Components || value > 1)
            return false;
        samples_[slot].available = value != 0;
        return true;
    }
    Session session;

  private:
    const Profile &profile_;
    std::array<DeviceSample, Limits::Components> samples_ = {};
    std::array<larp::domain::CardUid, Limits::Components> cards_ = {};
    std::array<larp::ports::CrystalInputBatch, Limits::Components> batches_ = {};
    std::size_t componentSlot(std::uint32_t id) const {
        if (id == 0 || id > 65535)
            return Limits::Components;
        for (std::size_t index = 0; index < profile_.components.size(); ++index)
            if (profile_.components.get(index)->id.value == id)
                return index;
        return Limits::Components;
    }
    std::size_t componentSlot(std::uint32_t id, ComponentKind kind) const {
        const std::size_t index = componentSlot(id);
        return index != Limits::Components && profile_.components.get(index)->kind == kind
                   ? index
                   : Limits::Components;
    }
    std::size_t componentSlot(ComponentId id) const {
        return componentSlot(id.value);
    }
    std::size_t componentSlot(ComponentId id, ComponentKind kind) const {
        return componentSlot(id.value, kind);
    }
    static void trace(const char *operation, ComponentId component, const ActionContext &context,
                      ActionOutcome outcome) {
        std::printf("{\"event\":\"device_request\",\"operation\":\"%s\",\"component\":%u,"
                    "\"state\":%u,\"transition\":%u,\"phase\":%u,\"now\":%lu,\"outcome\":\"%s\"",
                    operation, component.value, context.state.value, context.transition.value,
                    static_cast<unsigned>(context.phase),
                    static_cast<unsigned long>(context.nowMilliseconds),
                    actionOutcomeName(outcome));
    }
    ActionOutcome simple(const char *operation, ComponentId component, ComponentKind kind,
                         const ActionContext &context) {
        const std::size_t slot = componentSlot(component, kind);
        const ActionOutcome outcome = slot == Limits::Components || !samples_[slot].available
                                          ? ActionOutcome::Unavailable
                                          : ActionOutcome::Applied;
        trace(operation, component, context, outcome);
        std::puts("}");
        return outcome;
    }
};
bool number(const char *text, std::uint32_t &value) {
    if (text == nullptr || *text == '\0')
        return false;
    value = 0;
    for (const char *byte = text; *byte; ++byte) {
        if (*byte < '0' || *byte > '9')
            return false;
        const unsigned digit = static_cast<unsigned>(*byte - '0');
        if (value > (std::numeric_limits<std::uint32_t>::max() - digit) / 10)
            return false;
        value = value * 10 + digit;
    }
    return true;
}
bool position(char *text, larp::domain::GridPosition &value) {
    char *separator = std::strchr(text, ':');
    if (separator == nullptr)
        return false;
    *separator = '\0';
    std::uint32_t row = 0, column = 0;
    if (!number(text, row) || !number(separator + 1, column) || row < 1 || row > 9 || column > 8)
        return false;
    value.row = static_cast<std::uint8_t>(row);
    value.column = static_cast<larp::domain::GridColumn>(column);
    return true;
}
void report(const Session &session, const Result &result) {
    const SessionSnapshot value = session.snapshot();
    const ExecutionSnapshot &state = value.execution;
    std::printf(
        "{\"event\":\"session\",\"result\":\"%s\",\"state\":%u,\"stateRevision\":%lu,"
        "\"profileRevision\":%lu,\"running\":%s,\"paused\":%s,\"entryPending\":%s,"
        "\"elapsed\":%lu,\"lastTransition\":%u,\"rolePresent\":%s,\"role\":%u,"
        "\"crystals\":%u,\"requiredCrystals\":%u,\"seenRoles\":%u,\"rounds\":%lu,"
        "\"audioPending\":%s,\"incorrectCrystal\":%s,\"keystoneError\":%s,"
        "\"lastCardDecision\":%u,\"actionRequests\":%lu,\"queuedActions\":%lu,"
        "\"deferredActions\":%lu,\"rejectedActions\":%lu,\"unavailableActions\":%lu,\"devices\":[",
        outcomeName(result.outcome), state.state.value,
        static_cast<unsigned long>(state.stateRevision),
        static_cast<unsigned long>(state.profileRevision), state.running ? "true" : "false",
        state.paused ? "true" : "false", state.entryPending ? "true" : "false",
        static_cast<unsigned long>(state.stateElapsedMilliseconds), state.lastTransition.value,
        value.rolePresent ? "true" : "false", value.role.value, value.crystalCount,
        value.requiredCrystals, value.seenRoleCount, static_cast<unsigned long>(value.roundCount),
        value.crystalAudioPending ? "true" : "false", value.incorrectCrystal ? "true" : "false",
        value.keystoneError ? "true" : "false", static_cast<unsigned>(value.lastCardDecision),
        static_cast<unsigned long>(state.actionRequests),
        static_cast<unsigned long>(state.queuedActions),
        static_cast<unsigned long>(state.deferredActions),
        static_cast<unsigned long>(state.rejectedActions),
        static_cast<unsigned long>(state.unavailableActions));
    for (std::size_t index = 0; index < value.deviceCount; ++index) {
        const SessionDeviceSnapshot &device = value.devices[index];
        if (index)
            std::putchar(',');
        std::printf("{\"id\":%u,\"enabled\":%s,\"available\":%s,\"cardReadable\":%s,"
                    "\"matrixReadable\":%s,\"pollPending\":%s,\"outcome\":\"%s\"}",
                    device.id.value, device.enabled ? "true" : "false",
                    device.available ? "true" : "false", device.cardReadable ? "true" : "false",
                    device.matrixReadable ? "true" : "false", device.pollPending ? "true" : "false",
                    actionOutcomeName(device.lastOutcome));
    }
    std::puts("]}");
}
} // namespace
int runPlay(const larp::configuration::Profile &profile, std::FILE *input) {
    std::unique_ptr<Play> play(new Play(profile));
    std::printf("{\"event\":\"play\",\"sessionBytes\":%lu,\"syntheticDevices\":true,"
                "\"physicalHardware\":false}\n",
                static_cast<unsigned long>(sizeof(Session)));
    char line[1024];
    unsigned lineNumber = 0;
    bool failed = false;
    while (std::fgets(line, sizeof(line), input) != nullptr) {
        if (++lineNumber > 100000 || (std::strchr(line, '\n') == nullptr && !std::feof(input))) {
            std::fprintf(stderr, "Play script limit at line %u\n", lineNumber);
            return 2;
        }
        char *tokens[83] = {};
        std::size_t count = 0;
        for (char *token = std::strtok(line, " \t\r\n"); token;
             token = std::strtok(nullptr, " \t\r\n")) {
            if (token[0] == '#')
                break;
            if (count == 83) {
                std::fprintf(stderr, "Too many arguments at line %u\n", lineNumber);
                return 2;
            }
            tokens[count++] = token;
        }
        if (count == 0)
            continue;
        std::uint32_t a = 0, b = 0, c = 0;
        Result result;
        bool valid = true, inputOnly = false;
        if (std::strcmp(tokens[0], "card") == 0 || std::strcmp(tokens[0], "wakes") == 0 ||
            std::strcmp(tokens[0], "device") == 0) {
            valid = count == 3 && number(tokens[1], a) && number(tokens[2], b);
            if (valid)
                valid = std::strcmp(tokens[0], "card") == 0    ? play->card(a, b)
                        : std::strcmp(tokens[0], "wakes") == 0 ? play->wakes(a, b)
                                                               : play->available(a, b);
            inputOnly = true;
        } else if (std::strcmp(tokens[0], "crystals") == 0) {
            valid = count >= 2 && number(tokens[1], a);
            larp::ports::CrystalInputBatch batch;
            for (std::size_t index = 2; valid && index < count; ++index) {
                larp::domain::GridPosition value;
                valid = position(tokens[index], value) && batch.push_back(value);
            }
            if (valid)
                valid = play->crystals(a, batch);
            inputOnly = true;
        } else if (std::strcmp(tokens[0], "finish") == 0) {
            valid = count == 2 && number(tokens[1], a) && play->finish(a);
            inputOnly = true;
        } else if (std::strcmp(tokens[0], "start") == 0 || std::strcmp(tokens[0], "tick") == 0) {
            valid = count == 2 && number(tokens[1], a);
            if (valid)
                result = std::strcmp(tokens[0], "start") == 0 ? play->session.start(profile, a)
                                                              : play->session.tick(a);
        } else if (std::strcmp(tokens[0], "state") == 0 || std::strcmp(tokens[0], "edge") == 0) {
            valid = count == 4 && number(tokens[1], a) && a <= 65535 && number(tokens[2], b) &&
                    number(tokens[3], c);
            if (valid)
                result =
                    std::strcmp(tokens[0], "state") == 0
                        ? play->session.changeState(StateId(static_cast<std::uint16_t>(a)), b, c)
                        : play->session.takeTransition(TransitionId(static_cast<std::uint16_t>(a)),
                                                       b, c);
        } else if (std::strcmp(tokens[0], "pause") == 0 || std::strcmp(tokens[0], "resume") == 0 ||
                   std::strcmp(tokens[0], "stop") == 0) {
            valid = count == 3 && number(tokens[1], a) && number(tokens[2], b);
            if (valid)
                result = std::strcmp(tokens[0], "pause") == 0    ? play->session.pause(a, b)
                         : std::strcmp(tokens[0], "resume") == 0 ? play->session.resume(a, b)
                                                                 : play->session.stop(a, b);
        } else if (std::strcmp(tokens[0], "snapshot") == 0)
            valid = count == 1;
        else
            valid = false;
        if (!valid) {
            std::fprintf(stderr, "Invalid play command at line %u\n", lineNumber);
            return 2;
        }
        if (!inputOnly) {
            report(play->session, result);
            failed = failed || !result;
        }
    }
    return std::ferror(input) || std::fflush(stdout) != 0 || failed ? 1 : 0;
}
