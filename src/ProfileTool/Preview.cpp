#include "Preview.h"
#include "GameConfiguration/ProfileVocabulary.h"
#include "GameExecution/Engine.h"
#include <cstring>
#include <limits>
#include <memory>

namespace {
using namespace larp::configuration;
using namespace larp::execution;
class Preview : public IExecutionInputs, public IExecutionActions {
  public:
    explicit Preview(const Profile &profile) : engine(*this, *this), profile_(profile) {}
    bool read(Field field, ComponentId component, std::uint32_t &value) noexcept override {
        const std::size_t slot = sampleSlot(field, component);
        if (slot >= values_.size() || !available_[slot])
            return false;
        value = values_[slot];
        return true;
    }
    ActionOutcome execute(const Profile &profile, const Action &action,
                          const ActionContext &context) noexcept override {
        std::printf("{\"event\":\"action\",\"state\":%u,\"transition\":%u,\"phase\":%u,\"now\":%lu,"
                    "\"id\":%u,\"kind\":\"%s\",\"component\":%u,\"value\":%lu",
                    context.state.value, context.transition.value,
                    static_cast<unsigned>(context.phase),
                    static_cast<unsigned long>(context.nowMilliseconds), action.id.value,
                    vocabulary::ActionKindNames[static_cast<unsigned>(action.kind)],
                    action.component.value, static_cast<unsigned long>(action.value));
        if (action.kind == ActionKind::ApplyEffect) {
            for (std::size_t index = 0; index < profile.effects.size(); ++index) {
                const Effect &effect = *profile.effects.get(index);
                if (effect.id != action.effect)
                    continue;
                std::printf(",\"effect\":%u,\"effectKind\":\"%s\",\"effectComponent\":%u,\"rgb\":%"
                            "lu,\"frames\":%u",
                            effect.id.value,
                            vocabulary::EffectKindNames[static_cast<unsigned>(effect.kind)],
                            effect.component.value, static_cast<unsigned long>(effect.colorRgb),
                            effect.frames);
                break;
            }
        }
        std::puts("}");
        // Applied only to this trace: even read/reset/audio actions do no device or
        // puzzle work here. All predicate inputs remain explicitly script-owned.
        return ActionOutcome::Applied;
    }
    bool set(const char *name, std::uint32_t component, std::uint32_t value) {
        if (component > 65535)
            return false;
        for (std::size_t index = 0; index < Fields; ++index) {
            if (std::strcmp(name, vocabulary::FieldNames[index]) != 0)
                continue;
            const Field field = static_cast<Field>(index);
            if (field == Field::StateElapsedMilliseconds ||
                field == Field::TransitionElapsedMilliseconds ||
                (field <= Field::AllWakeActive && value > 1))
                return false;
            const std::size_t slot =
                sampleSlot(field, ComponentId(static_cast<std::uint16_t>(component)));
            if (slot == values_.size())
                return false;
            values_[slot] = value;
            available_[slot] = true;
            return true;
        }
        return false;
    }
    Engine engine;

  private:
    static const std::size_t Fields = static_cast<std::size_t>(Field::RoundCount) + 1;
    const Profile &profile_;
    std::array<std::uint32_t, (Limits::Components + 1) * Fields> values_ = {};
    std::array<bool, (Limits::Components + 1) * Fields> available_ = {};
    std::size_t sampleSlot(Field field, ComponentId component) const {
        if (component.value == 0)
            return static_cast<std::size_t>(field);
        for (std::size_t index = 0; index < profile_.components.size(); ++index)
            if (profile_.components.get(index)->id == component)
                return (index + 1) * Fields + static_cast<std::size_t>(field);
        return values_.size();
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
void report(const Engine &engine, const Result &result) {
    const ExecutionSnapshot state = engine.snapshot();
    std::printf(
        "{\"event\":\"snapshot\",\"result\":\"%s\",\"state\":%u,\"stateRevision\":%lu,"
        "\"profileRevision\":%lu,"
        "\"running\":%s,\"paused\":%s,\"entryPending\":%s,\"elapsed\":%lu,\"lastTransition\":%u,"
        "\"requests\":%lu,\"condition\":%u,\"field\":%u,\"component\":%u}\n",
        outcomeName(result.outcome), state.state.value,
        static_cast<unsigned long>(state.stateRevision),
        static_cast<unsigned long>(state.profileRevision), state.running ? "true" : "false",
        state.paused ? "true" : "false", state.entryPending ? "true" : "false",
        static_cast<unsigned long>(state.stateElapsedMilliseconds), state.lastTransition.value,
        static_cast<unsigned long>(state.actionRequests), result.condition.value,
        static_cast<unsigned>(result.field), result.component.value);
}
} // namespace
int runPreview(const larp::configuration::Profile &profile, std::FILE *input) {
    std::unique_ptr<Preview> preview(new Preview(profile));
    std::printf("{\"event\":\"preview\",\"engineBytes\":%lu,\"scriptedInputs\":true,"
                "\"deviceActions\":false}\n",
                static_cast<unsigned long>(sizeof(Engine)));
    char line[256];
    unsigned lineNumber = 0;
    bool failed = false;
    while (std::fgets(line, sizeof(line), input) != nullptr) {
        if (++lineNumber > 100000 || (std::strchr(line, '\n') == nullptr && !std::feof(input))) {
            std::fprintf(stderr, "Preview script limit at line %u\n", lineNumber);
            return 2;
        }
        char *tokens[6] = {};
        std::size_t count = 0;
        for (char *token = std::strtok(line, " \t\r\n"); token;
             token = std::strtok(nullptr, " \t\r\n")) {
            if (token[0] == '#')
                break;
            if (count == 6) {
                std::fprintf(stderr, "Too many arguments at line %u\n", lineNumber);
                return 2;
            }
            tokens[count++] = token;
        }
        if (count == 0)
            continue;
        std::uint32_t a = 0, b = 0, c = 0;
        Result result;
        bool valid = true;
        if (std::strcmp(tokens[0], "set") == 0) {
            valid = count == 4 && number(tokens[2], a) && number(tokens[3], b) &&
                    preview->set(tokens[1], a, b);
            if (valid)
                continue;
        } else if (std::strcmp(tokens[0], "start") == 0) {
            valid = count == 2 && number(tokens[1], a);
            if (valid)
                result = preview->engine.start(profile, a);
        } else if (std::strcmp(tokens[0], "tick") == 0) {
            valid = count == 2 && number(tokens[1], a);
            if (valid)
                result = preview->engine.tick(a);
        } else if (std::strcmp(tokens[0], "state") == 0 || std::strcmp(tokens[0], "edge") == 0) {
            valid = count == 4 && number(tokens[1], a) && a <= 65535 && number(tokens[2], b) &&
                    number(tokens[3], c);
            if (valid)
                result =
                    std::strcmp(tokens[0], "state") == 0
                        ? preview->engine.changeState(StateId(static_cast<std::uint16_t>(a)), b, c)
                        : preview->engine.takeTransition(
                              TransitionId(static_cast<std::uint16_t>(a)), b, c);
        } else if (std::strcmp(tokens[0], "pause") == 0 || std::strcmp(tokens[0], "resume") == 0 ||
                   std::strcmp(tokens[0], "stop") == 0) {
            valid = count == 3 && number(tokens[1], a) && number(tokens[2], b);
            if (valid)
                result = std::strcmp(tokens[0], "pause") == 0    ? preview->engine.pause(a, b)
                         : std::strcmp(tokens[0], "resume") == 0 ? preview->engine.resume(a, b)
                                                                 : preview->engine.stop(a, b);
        } else if (std::strcmp(tokens[0], "snapshot") == 0)
            valid = count == 1;
        else
            valid = false;
        if (!valid) {
            std::fprintf(stderr, "Invalid preview command at line %u\n", lineNumber);
            return 2;
        }
        report(preview->engine, result);
        failed = failed || !result;
    }
    return std::ferror(input) || std::fflush(stdout) != 0 || failed ? 1 : 0;
}
