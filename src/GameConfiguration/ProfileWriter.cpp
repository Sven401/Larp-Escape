#include "GameConfiguration/ProfileWriter.h"
#include "GameConfiguration/ProfileVocabulary.h"
#include <cstring>

namespace larp {
namespace configuration {
namespace {
class Json {
  public:
    explicit Json(IProfileWriter &sink) : sink(sink), ok(true) {}
    void literal(const char *text) {
        if (ok)
            ok = sink.write(text, std::strlen(text));
    }
    void number(std::uint32_t value) {
        char buffer[10];
        std::size_t count = 0;
        do {
            buffer[count++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value != 0);
        for (std::size_t index = 0; index < count / 2; ++index) {
            const char saved = buffer[index];
            buffer[index] = buffer[count - index - 1];
            buffer[count - index - 1] = saved;
        }
        if (ok)
            ok = sink.write(buffer, count);
    }
    void boolean(bool value) {
        literal(value ? "true" : "false");
    }
    void string(const char *text) {
        literal("\"");
        for (std::size_t index = 0; ok && text[index] != '\0'; ++index) {
            const unsigned char byte = static_cast<unsigned char>(text[index]);
            if (byte == '"' || byte == '\\')
                literal("\\");
            if (byte < 32) {
                static const char hex[] = "0123456789abcdef";
                char escaped[] = {'\\', 'u', '0', '0', hex[byte >> 4], hex[byte & 15]};
                if (ok)
                    ok = sink.write(escaped, sizeof(escaped));
            } else if (ok) {
                ok = sink.write(text + index, 1);
            }
        }
        literal("\"");
    }
    void key(const char *name) {
        string(name);
        literal(":");
    }
    void numeric(const char *name, std::uint32_t value) {
        key(name);
        number(value);
    }
    void flag(const char *name, bool value) {
        key(name);
        boolean(value);
    }
    void text(const char *name, const char *value) {
        key(name);
        string(value);
    }
    bool good() const {
        return ok;
    }

  private:
    IProfileWriter &sink;
    bool ok;
};
const char *componentName(ComponentKind kind) {
    return vocabulary::ComponentKindNames[static_cast<unsigned>(kind)];
}
const char *fieldName(Field field) {
    return vocabulary::FieldNames[static_cast<unsigned>(field)];
}
const char *actionName(ActionKind kind) {
    return vocabulary::ActionKindNames[static_cast<unsigned>(kind)];
}
void endpoints(Json &json, const List<Endpoint, Limits::MatrixLines> &items) {
    json.literal("[");
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index)
            json.literal(",");
        json.literal("{");
        json.numeric("expander", items.get(index)->expander.value);
        json.literal(",");
        json.numeric("pin", items.get(index)->pin);
        json.literal("}");
    }
    json.literal("]");
}
template <typename T, std::size_t N> void numbers(Json &json, const std::array<T, N> &values) {
    json.literal("[");
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index)
            json.literal(",");
        json.number(static_cast<std::uint32_t>(values[index]));
    }
    json.literal("]");
}
template <std::size_t N> void actionIds(Json &json, const List<ActionId, N> &values) {
    json.literal("[");
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index)
            json.literal(",");
        json.number(values.get(index)->value);
    }
    json.literal("]");
}
void component(Json &json, const Component &item) {
    json.literal("{");
    json.numeric("id", item.id.value);
    json.literal(",");
    json.text("name", item.name.c_str());
    json.literal(",");
    json.text("kind", componentName(item.kind));
    json.literal(",");
    json.flag("enabled", item.enabled);
    json.literal(",");
    json.key("pins");
    numbers(json, item.pins);
    json.literal(",");
    json.numeric("bus", item.bus);
    json.literal(",");
    json.numeric("address", item.address);
    json.literal(",");
    json.numeric("ledCount", item.ledCount);
    json.literal(",");
    json.numeric("powerLimitMilliamperes", item.powerLimitMilliamperes);
    json.literal(",");
    json.numeric("volume", item.volume);
    json.literal(",");
    json.numeric("baud", item.baud);
    json.literal(",");
    json.numeric("initialBrightness", item.initialBrightness);
    json.literal(",");
    json.numeric("initialTwinkleDensity", item.initialTwinkleDensity);
    json.literal(",");
    json.numeric("initialTwinkleSpeed", item.initialTwinkleSpeed);
    json.literal(",");
    json.key("startupModes");
    numbers(json, item.startupModes);
    json.literal(",");
    json.key("peer");
    numbers(json, item.peer);
    json.literal(",");
    json.numeric("channel", item.channel);
    json.literal(",");
    json.key("rows");
    endpoints(json, item.rows);
    json.literal(",");
    json.key("columns");
    endpoints(json, item.columns);
    json.literal(",");
    json.key("wakeInputs");
    json.literal("[");
    for (std::size_t index = 0; index < item.wakeInputs.size(); ++index) {
        if (index)
            json.literal(",");
        json.literal("{");
        json.numeric("expander", item.wakeInputs.get(index)->expander.value);
        json.literal(",");
        json.numeric("pin", item.wakeInputs.get(index)->pin);
        json.literal("}");
    }
    json.literal("]}");
}
void role(Json &json, const Role &item) {
    json.literal("{");
    json.numeric("id", item.id.value);
    json.literal(",");
    json.text("name", item.name.c_str());
    json.literal(",");
    json.key("uid");
    numbers(json, item.uid);
    json.literal(",");
    json.numeric("audioTrack", item.audioTrack);
    json.literal(",");
    json.key("crystals");
    json.literal("[");
    for (std::size_t index = 0; index < item.crystals.size(); ++index) {
        if (index)
            json.literal(",");
        const Crystal &crystal = *item.crystals.get(index);
        json.literal("{");
        json.text("name", crystal.name.c_str());
        json.literal(",");
        json.numeric("row", crystal.row);
        json.literal(",");
        json.numeric("column", crystal.column);
        json.literal(",");
        json.numeric("audioTrack", crystal.audioTrack);
        json.literal(",");
        json.text("category",
                  vocabulary::CrystalCategoryNames[static_cast<unsigned>(crystal.category)]);
        json.literal(",");
        json.flag("changesPalette", crystal.changesPalette);
        json.literal(",");
        json.key("palette");
        numbers(json, crystal.palette);
        json.literal("}");
    }
    json.literal("]}");
}
void state(Json &json, const State &item) {
    json.literal("{");
    json.numeric("id", item.id.value);
    json.literal(",");
    json.text("name", item.name.c_str());
    json.literal(",");
    json.flag("manualAllowed", item.manualAllowed);
    json.literal(",");
    json.key("entry");
    actionIds(json, item.entry);
    json.literal(",");
    json.key("tick");
    actionIds(json, item.tick);
    json.literal(",");
    json.key("exit");
    actionIds(json, item.exit);
    json.literal("}");
}
void transition(Json &json, const Transition &item) {
    json.literal("{");
    json.numeric("id", item.id.value);
    json.literal(",");
    json.numeric("source", item.source.value);
    json.literal(",");
    json.numeric("target", item.target.value);
    json.literal(",");
    json.numeric("condition", item.condition.value);
    json.literal(",");
    json.numeric("priority", item.priority);
    json.literal(",");
    json.flag("manualAllowed", item.manualAllowed);
    json.literal(",");
    json.key("actions");
    actionIds(json, item.actions);
    json.literal("}");
}
void condition(Json &json, const Condition &item) {
    json.literal("{");
    json.numeric("id", item.id.value);
    json.literal(",");
    json.text("kind", vocabulary::ConditionKindNames[static_cast<unsigned>(item.kind)]);
    if (item.kind == ConditionKind::Predicate) {
        json.literal(",");
        json.text("field", fieldName(item.field));
        json.literal(",");
        json.numeric("component", item.component.value);
        json.literal(",");
        json.text("comparison", vocabulary::CompareNames[static_cast<unsigned>(item.comparison)]);
        json.literal(",");
        json.numeric("operand", item.operand);
    } else {
        json.literal(",");
        json.numeric("first", item.first.value);
        if (item.kind != ConditionKind::Not) {
            json.literal(",");
            json.numeric("second", item.second.value);
        }
    }
    json.literal("}");
}
void action(Json &json, const Action &item) {
    json.literal("{");
    json.numeric("id", item.id.value);
    json.literal(",");
    json.text("kind", actionName(item.kind));
    json.literal(",");
    json.numeric("component", item.component.value);
    json.literal(",");
    json.numeric("source", item.source.value);
    json.literal(",");
    json.numeric("effect", item.effect.value);
    json.literal(",");
    json.numeric("value", item.value);
    json.literal("}");
}
void effect(Json &json, const Effect &item) {
    json.literal("{");
    json.numeric("id", item.id.value);
    json.literal(",");
    json.text("name", item.name.c_str());
    json.literal(",");
    json.numeric("component", item.component.value);
    json.literal(",");
    json.text("kind", vocabulary::EffectKindNames[static_cast<unsigned>(item.kind)]);
    json.literal(",");
    json.numeric("density", item.density);
    json.literal(",");
    json.flag("changesSpeed", item.changesSpeed);
    json.literal(",");
    json.numeric("speed", item.speed);
    json.literal(",");
    json.numeric("brightness", item.brightness);
    json.literal(",");
    json.numeric("intervalMilliseconds", item.intervalMilliseconds);
    json.literal(",");
    json.numeric("frames", item.frames);
    json.literal(",");
    json.flag("colored", item.colored);
    json.literal(",");
    json.numeric("colorRgb", item.colorRgb);
    json.literal(",");
    json.numeric("rippleIndex", item.rippleIndex);
    json.literal(",");
    json.numeric("origin", item.origin);
    json.literal(",");
    json.key("palette");
    numbers(json, item.palette);
    json.literal("}");
}
template <typename T, std::size_t N>
void objects(Json &json, const List<T, N> &items, void (*emit)(Json &, const T &)) {
    json.literal("[");
    for (std::size_t index = 0; index < items.size() && json.good(); ++index) {
        if (index)
            json.literal(",");
        emit(json, *items.get(index));
    }
    json.literal("]");
}
} // namespace
bool writeProfile(const Profile &profile, IProfileWriter &writer) {
    if (!validateProfile(profile))
        return false;
    Json json(writer);
    json.literal("{");
    json.numeric("schemaVersion", profile.schemaVersion);
    json.literal(",");
    json.numeric("revision", profile.revision);
    json.literal(",");
    json.text("name", profile.name.c_str());
    json.literal(",");
    json.text("board", vocabulary::BoardNames[static_cast<unsigned>(profile.board)]);
    json.literal(",");
    json.numeric("initialState", profile.initialState.value);
    json.literal(",");
    json.numeric("errorState", profile.errorState.value);
    json.literal(",\"inputs\":{");
    json.numeric("cardTimeoutMilliseconds", profile.inputs.cardTimeoutMilliseconds);
    json.literal(",");
    json.numeric("emptyReadsBeforeClear", profile.inputs.emptyReadsBeforeClear);
    json.literal(",");
    json.numeric("keystoneFailureThreshold", profile.inputs.keystoneFailureThreshold);
    json.literal(",");
    json.flag("consecutiveEmptyReads", profile.inputs.consecutiveEmptyReads);
    json.literal(",");
    json.key("wakeRippleOrigins");
    numbers(json, profile.inputs.wakeRippleOrigins);
    json.literal(",");
    json.numeric("wakeRippleFrames", profile.inputs.wakeRippleFrames);
    json.literal("}");
    json.literal(",");
    json.key("components");
    objects(json, profile.components, component);
    json.literal(",");
    json.key("roles");
    objects(json, profile.roles, role);
    json.literal(",");
    json.key("states");
    objects(json, profile.states, state);
    json.literal(",");
    json.key("transitions");
    objects(json, profile.transitions, transition);
    json.literal(",");
    json.key("conditions");
    objects(json, profile.conditions, condition);
    json.literal(",");
    json.key("actions");
    objects(json, profile.actions, action);
    json.literal(",");
    json.key("effects");
    objects(json, profile.effects, effect);
    json.literal("}");
    return json.good();
}
} // namespace configuration
} // namespace larp
