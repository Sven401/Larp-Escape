#include "GameConfiguration/Profile.h"
#include "GameDomain/GameDefinitions.h"

namespace larp {
namespace configuration {
namespace {
const Palette kPurple = {{0x800080, 0x993399, 0xB266B2, 0xCC99CC, 0xE5CCE5, 0x660066, 0x4D004D,
                          0xA569BD, 0xD8BFD8, 0x990099, 0xB200B2, 0xCC00CC, 0xE500E5, 0xFF00FF,
                          0xFF33FF, 0xFF66FF}};

const Palette kRed = {{0xCC0000, 0x990000, 0x800020, 0xB22222, 0xFF5A36, 0xFF6666, 0xFF9999,
                       0xFFCCCC, 0xFFD700, 0xFFA07A, 0xE34234, 0xD72638, 0xC21807, 0xFF8C42,
                       0xFA8072, 0xFFC0CB}};

const Palette kOrange = {{0xFFA500, 0xFF8C00, 0xFF7F50, 0xFF6347, 0xFF4500, 0xFF4500, 0xFF6347,
                          0xFF7F50, 0xFF8C00, 0xFFA500, 0xFFB347, 0xFFCC66, 0xFFD700, 0xFFE4B5,
                          0xFFEFD5, 0xFFF8DC}};

const Palette kGreen = {{0x4CAF50, 0x388E3C, 0x2E7D32, 0x1B5E20, 0x0D3B15, 0x81C784, 0xA5D6A7,
                         0xC8E6C9, 0xE8F5E9, 0x66BB6A, 0x57A773, 0x5FAD56, 0x8BC34A, 0x9CCC65,
                         0xB2DFDB, 0xD0ECE7}};

const Palette kWhite = {{0xFFFFFF, 0xF8F8F0, 0xF0F0E8, 0xE8F0F8, 0xE0E0F0, 0xF0E8F8, 0xF8E0E0,
                         0xF8F0D0, 0xE8F8D0, 0xD0F8E8, 0xD0E8F8, 0xF0D0F8, 0xF8D0E0, 0xE0F8F0,
                         0xF0F8D0, 0xF8E8D0}};

const Palette kBlue = {{0x0000FF, 0x0000CC, 0x003366, 0x00509E, 0x1E6091, 0x0077B6, 0x0096C7,
                        0x00B4D8, 0x48CAE4, 0x90E0EF, 0x3399FF, 0x3366FF, 0x4A69BD, 0x6A5ACD,
                        0x5A84E3, 0x8EA8E6}};

const Palette kMutedAllColors = {{0xFFA97F, 0xFFE08C, 0xA8C89E, 0x85A88A, 0xD0E3A8, 0xB7D3C3,
                                  0x8DA6B1, 0xA0B6D3, 0xB7D3E8, 0xE3D0E3, 0xEFC5DE, 0xD98A9C,
                                  0xFFA08A, 0xFFB39A, 0xFFD98A, 0xFFC7D1}};

class Builder {
  public:
    explicit Builder(Profile &profile) : profile(profile), ok(true) {}
    ComponentId component(Component item, const char *name) {
        item.id = ComponentId(static_cast<std::uint16_t>(profile.components.size() + 1));
        ok = item.name.assign(name) && ok;
        ok = profile.components.push(item) && ok;
        return item.id;
    }
    EffectId effect(Effect item, const char *name) {
        item.id = EffectId(static_cast<std::uint16_t>(profile.effects.size() + 1));
        ok = item.name.assign(name) && ok;
        ok = profile.effects.push(item) && ok;
        return item.id;
    }
    ActionId action(ActionKind kind, ComponentId component = ComponentId(), std::uint32_t value = 0,
                    ComponentId source = ComponentId(), EffectId binding = EffectId()) {
        Action item;
        item.id = ActionId(static_cast<std::uint16_t>(profile.actions.size() + 1));
        item.kind = kind;
        item.component = component;
        item.source = source;
        item.effect = binding;
        item.value = value;
        ok = profile.actions.push(item) && ok;
        return item.id;
    }
    ActionId apply(EffectId effectId) {
        Action item;
        item.id = ActionId(static_cast<std::uint16_t>(profile.actions.size() + 1));
        item.kind = ActionKind::ApplyEffect;
        item.effect = effectId;
        ok = profile.actions.push(item) && ok;
        return item.id;
    }
    ConditionId predicate(Field field, Compare comparison, std::uint32_t value,
                          ComponentId component = ComponentId()) {
        Condition item;
        item.id = ConditionId(static_cast<std::uint16_t>(profile.conditions.size() + 1));
        item.field = field;
        item.comparison = comparison;
        item.operand = value;
        item.component = component;
        ok = profile.conditions.push(item) && ok;
        return item.id;
    }
    ConditionId wait(std::uint32_t milliseconds) {
        Condition item;
        item.id = ConditionId(static_cast<std::uint16_t>(profile.conditions.size() + 1));
        item.kind = ConditionKind::Wait;
        item.operand = milliseconds;
        ok = profile.conditions.push(item) && ok;
        return item.id;
    }
    ConditionId combine(ConditionKind kind, ConditionId first, ConditionId second) {
        Condition item;
        item.id = ConditionId(static_cast<std::uint16_t>(profile.conditions.size() + 1));
        item.kind = kind;
        item.first = first;
        item.second = second;
        ok = profile.conditions.push(item) && ok;
        return item.id;
    }
    void transition(StateId from, StateId to, ConditionId condition, std::uint16_t priority,
                    ActionId first = ActionId(), ActionId second = ActionId()) {
        Transition item;
        item.id = TransitionId(static_cast<std::uint16_t>(profile.transitions.size() + 1));
        item.source = from;
        item.target = to;
        item.condition = condition;
        item.priority = priority;
        if (first.value != 0)
            ok = item.actions.push(first) && ok;
        if (second.value != 0)
            ok = item.actions.push(second) && ok;
        ok = profile.transitions.push(item) && ok;
    }
    Profile &profile;
    bool ok;
};
Endpoint endpoint(ComponentId expander, std::uint8_t pin) {
    Endpoint result;
    result.expander = expander;
    result.pin = pin;
    return result;
}
ActionId twinkle(Builder &builder, ComponentId led, std::uint8_t density, bool speed = false) {
    Effect effect;
    effect.kind = EffectKind::Twinkle;
    effect.component = led;
    effect.density = density;
    effect.changesSpeed = speed;
    return builder.apply(builder.effect(effect, "twinkle"));
}
ActionId brightness(Builder &builder, ComponentId led, std::uint8_t target) {
    Effect effect;
    effect.kind = EffectKind::BrightnessRamp;
    effect.component = led;
    effect.brightness = target;
    effect.intervalMilliseconds = 150;
    return builder.apply(builder.effect(effect, "brightness"));
}
ActionId burst(Builder &builder, ComponentId led, std::uint8_t frames, bool green = false) {
    Effect effect;
    effect.kind = EffectKind::Burst;
    effect.component = led;
    effect.frames = frames;
    effect.colored = green;
    effect.colorRgb = green ? 0x00FF00U : 0;
    return builder.apply(builder.effect(effect, "burst"));
}
State state(std::uint16_t id, const char *name, Builder &builder) {
    State result;
    result.id = StateId(id);
    builder.ok = result.name.assign(name) && builder.ok;
    return result;
}
template <std::size_t N>
void add(List<ActionId, N> &destination, ActionId action, Builder &builder) {
    builder.ok = destination.push(action) && builder.ok;
}
} // namespace

bool makeStandardProfile(Profile &profile) {
    // Clear counts without constructing a full-capacity Profile temporary on the stack.
    profile.components.clear();
    profile.roles.clear();
    profile.states.clear();
    profile.transitions.clear();
    profile.conditions.clear();
    profile.actions.clear();
    profile.effects.clear();
    profile.schemaVersion = Limits::SchemaVersion;
    profile.revision = 3;
    profile.board = Board::Esp32DevkitV4;
    profile.initialState = StateId(1);
    profile.errorState = StateId(9);
    profile.inputs = InputPolicy();
    Builder builder(profile);
    builder.ok = profile.name.assign("Larp-Escape Standard");

    Component nfc;
    nfc.kind = ComponentKind::NfcSpi;
    nfc.pins = {{16, 17, 23, 4}};
    const ComponentId nfcId = builder.component(nfc, "PN532");
    Component expander;
    expander.kind = ComponentKind::IoExpander;
    expander.pins = {{21, 22, UnusedPin, UnusedPin}};
    expander.address = 0x20;
    for (std::size_t pin = 8; pin < 16; ++pin)
        expander.startupModes[pin] = ExpanderMode::InputPullup;
    const ComponentId expander0 = builder.component(expander, "MCP 0x20");
    expander.address = 0x21;
    expander.startupModes.fill(ExpanderMode::Unconfigured);
    expander.startupModes[8] = ExpanderMode::InputPullup;
    expander.startupModes[9] = ExpanderMode::Output;
    for (std::size_t pin = 0; pin < 8; ++pin)
        expander.startupModes[pin] = ExpanderMode::Output;
    const ComponentId expander1 = builder.component(expander, "MCP 0x21");
    Component matrix;
    matrix.kind = ComponentKind::Matrix;
    for (std::uint8_t row = 0; row < 8; ++row)
        builder.ok = matrix.rows.push(endpoint(expander1, static_cast<std::uint8_t>(15 - row))) &&
                     builder.ok;
    builder.ok = matrix.rows.push(endpoint(expander0, 8)) && builder.ok;
    builder.ok = matrix.columns.push(endpoint(expander0, 9)) && builder.ok;
    for (std::uint8_t pin = 0; pin < 7; ++pin)
        builder.ok = matrix.columns.push(endpoint(expander0, pin)) && builder.ok;
    builder.ok = matrix.columns.push(endpoint(expander0, 13)) && builder.ok;
    const std::uint8_t wakePins[] = {15, 14, 12, 11};
    for (std::uint8_t pin : wakePins)
        builder.ok = matrix.wakeInputs.push(endpoint(expander0, pin)) && builder.ok;
    const ComponentId matrixId = builder.component(matrix, "Crystal matrix");
    Component audio;
    audio.kind = ComponentKind::AudioUart;
    audio.bus = 1;
    audio.pins = {{25, 26, 14, 13}};
    const ComponentId audioId = builder.component(audio, "DFPlayer");
    Component led;
    led.kind = ComponentKind::LedStrip;
    led.pins[0] = 15;
    led.ledCount = 23;
    const ComponentId ledId = builder.component(led, "WS2811 GRB");
    Component network;
    network.kind = ComponentKind::EspNow;
    network.peer = {{0xB0, 0xA7, 0x32, 0xF1, 0x86, 0x54}};
    const ComponentId networkId = builder.component(network, "ESP-NOW receiver");

    const Palette *rolePalettes[] = {&kPurple, &kRed, &kOrange, &kGreen, &kWhite, &kBlue};
    const auto &definitions = domain::defaultRoleDefinitions();
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        const domain::RoleDefinition &definition = definitions[index];
        Role role;
        role.id = RoleId(static_cast<std::uint16_t>(index + 1));
        builder.ok = role.name.assign(definition.name) && builder.ok;
        role.uid = definition.uid;
        role.audioTrack = definition.audioTrack;
        for (const domain::CrystalDefinition &definitionCrystal : definition.crystals) {
            Crystal crystal;
            builder.ok = crystal.name.assign(definitionCrystal.symbol) && builder.ok;
            crystal.row = definitionCrystal.position.row;
            crystal.column = static_cast<std::uint8_t>(definitionCrystal.position.column);
            crystal.category = static_cast<CrystalCategory>(definitionCrystal.category);
            crystal.audioTrack = definitionCrystal.audioTrack;
            crystal.changesPalette = crystal.category == CrystalCategory::Color;
            if (crystal.changesPalette)
                crystal.palette = *rolePalettes[index];
            builder.ok = role.crystals.push(crystal) && builder.ok;
        }
        builder.ok = profile.roles.push(role) && builder.ok;
    }

    const ActionId resetRound = builder.action(ActionKind::ResetRound);
    const ActionId resetGame = builder.action(ActionKind::ResetGame);
    const ActionId resetRetry = builder.action(ActionKind::ResetCardRetry);
    const ActionId consumeCrystal = builder.action(ActionKind::ConsumeCrystal);
    const ActionId consumeError = builder.action(ActionKind::ConsumeError);
    const ActionId readRole = builder.action(ActionKind::ReadRole, nfcId, 1000);
    Effect crystalColors;
    crystalColors.component = ledId;
    crystalColors.kind = EffectKind::Palette;
    const EffectId crystalColorBinding = builder.effect(crystalColors, "crystal colors");
    const ActionId readCrystals =
        builder.action(ActionKind::ReadCrystals, matrixId, 100, ComponentId(), crystalColorBinding);
    const ActionId monitorKeystone = builder.action(ActionKind::ObserveKeystone, nfcId, 2000);
    const ActionId wakeAudio = builder.action(ActionKind::WakeAudio, audioId);
    const ActionId burst15 = burst(builder, ledId, 15);
    const ActionId burst30 = burst(builder, ledId, 30);
    const ActionId burstGreen = burst(builder, ledId, 120, true);
    Effect muted;
    muted.component = ledId;
    muted.kind = EffectKind::Palette;
    muted.palette = kMutedAllColors;
    const ActionId mutedPalette = builder.apply(builder.effect(muted, "muted palette"));

    State standby = state(1, "standby", builder);
    add(standby.entry, twinkle(builder, ledId, 0), builder);
    add(standby.entry, resetGame, builder);
    add(standby.entry, resetRound, builder);
    add(standby.tick, builder.action(ActionKind::AnimateWakeInputs, ledId, 0, matrixId), builder);
    builder.ok = profile.states.push(standby) && builder.ok;
    State idle = state(2, "idle", builder);
    add(idle.entry, burstGreen, builder);
    add(idle.entry, burst15, builder);
    add(idle.entry, twinkle(builder, ledId, 1), builder);
    add(idle.entry, mutedPalette, builder);
    add(idle.entry, resetRetry, builder);
    add(idle.tick, brightness(builder, ledId, 20), builder);
    add(idle.tick, readRole, builder);
    builder.ok = profile.states.push(idle) && builder.ok;
    State waiting = state(3, "waiting_for_crystals", builder);
    add(waiting.entry, wakeAudio, builder);
    add(waiting.entry, burst15, builder);
    add(waiting.entry, twinkle(builder, ledId, 2, true), builder);
    add(waiting.entry, resetRetry, builder);
    add(waiting.tick, brightness(builder, ledId, 50), builder);
    add(waiting.tick, readCrystals, builder);
    add(waiting.tick, monitorKeystone, builder);
    builder.ok = profile.states.push(waiting) && builder.ok;
    const char *crystalNames[] = {"first_crystal_placed", "second_crystal_placed",
                                  "third_crystal_placed"};
    for (std::uint8_t index = 0; index < 3; ++index) {
        State placed = state(static_cast<std::uint16_t>(4 + index), crystalNames[index], builder);
        add(placed.entry, burst15, builder);
        add(placed.entry, twinkle(builder, ledId, static_cast<std::uint8_t>(3 + index)), builder);
        add(placed.entry, builder.action(ActionKind::PlayCrystalTrack, audioId, index), builder);
        if (index < 2)
            add(placed.entry, resetRetry, builder);
        add(placed.tick, brightness(builder, ledId, static_cast<std::uint8_t>(100 + 50 * index)),
            builder);
        if (index < 2) {
            add(placed.tick, readCrystals, builder);
            add(placed.tick, monitorKeystone, builder);
        }
        builder.ok = profile.states.push(placed) && builder.ok;
    }
    State completed = state(7, "game_completed", builder);
    add(completed.entry, burst30, builder);
    add(completed.entry, builder.action(ActionKind::CompleteRound), builder);
    add(completed.entry, twinkle(builder, ledId, 6), builder);
    add(completed.entry, builder.action(ActionKind::PlayRoleTrack, audioId), builder);
    add(completed.tick, brightness(builder, ledId, 255), builder);
    builder.ok = profile.states.push(completed) && builder.ok;
    State bonus = state(8, "bonus", builder);
    add(bonus.entry, twinkle(builder, ledId, 7), builder);
    add(bonus.entry, builder.action(ActionKind::PlayTrack, audioId, 900), builder);
    builder.ok = profile.states.push(bonus) && builder.ok;
    State error = state(9, "error", builder);
    add(error.entry, twinkle(builder, ledId, 3), builder);
    add(error.entry, builder.action(ActionKind::PlayTrack, audioId, 998), builder);
    add(error.entry, resetGame, builder);
    builder.ok = profile.states.push(error) && builder.ok;

    const ConditionId audioReady = builder.predicate(Field::AudioBusy, Compare::Equal, 0, audioId);
    const ConditionId wrongCrystal =
        builder.predicate(Field::IncorrectCrystal, Compare::Equal, 1, matrixId);
    const ConditionId missingKeystone =
        builder.predicate(Field::KeystoneError, Compare::Equal, 1, nfcId);
    const ConditionId anyError = builder.combine(ConditionKind::Any, wrongCrystal, missingKeystone);
    const ConditionId errorReady = builder.combine(ConditionKind::All, audioReady, anyError);
    const ConditionId crystalReady =
        builder.predicate(Field::CrystalAvailable, Compare::Equal, 1, matrixId);
    const ConditionId crystalAudio =
        builder.predicate(Field::CrystalAudioPending, Compare::Equal, 1);
    const ConditionId crystalOrAudio =
        builder.combine(ConditionKind::Any, crystalReady, crystalAudio);
    const ConditionId nextCrystal = builder.combine(ConditionKind::All, audioReady, crystalOrAudio);
    const ConditionId timeout =
        builder.predicate(Field::StateElapsedMilliseconds, Compare::GreaterEqual, 600000);
    const ConditionId wake = builder.predicate(Field::AllWakeActive, Compare::Equal, 1, matrixId);
    const ConditionId role = builder.predicate(Field::RoleAccepted, Compare::Equal, 1, nfcId);
    const ConditionId delay = builder.wait(500);
    const ConditionId delayedReady = builder.combine(ConditionKind::All, delay, audioReady);
    const ConditionId puzzle = builder.predicate(Field::PuzzleComplete, Compare::Equal, 1);
    const ConditionId completedReady = builder.combine(ConditionKind::All, delayedReady, puzzle);
    const ConditionId allRoles = builder.predicate(
        Field::SeenRoleCount, Compare::Equal, static_cast<std::uint32_t>(profile.roles.size()));
    const ConditionId bonusReady = builder.combine(ConditionKind::All, delayedReady, allRoles);
    builder.transition(StateId(1), StateId(2), wake, 0);
    builder.transition(StateId(2), StateId(3), role, 0);
    builder.transition(StateId(2), StateId(9), errorReady, 1, consumeError);
    builder.transition(StateId(2), StateId(1), timeout, 2);
    for (std::uint16_t index = 3; index <= 5; ++index) {
        builder.transition(StateId(index), StateId(9), errorReady, 0, consumeError);
        builder.transition(StateId(index), StateId(static_cast<std::uint16_t>(index + 1)),
                           nextCrystal, 1, consumeCrystal);
        builder.transition(StateId(index), StateId(1), timeout, 2);
    }
    builder.transition(StateId(6), StateId(9), errorReady, 0, consumeError);
    builder.transition(StateId(6), StateId(7), completedReady, 1);
    builder.transition(StateId(7), StateId(8), bonusReady, 0);
    builder.transition(StateId(7), StateId(2), delayedReady, 1, resetRound);
    const ActionId activate = builder.action(ActionKind::SendActivation, networkId);
    builder.transition(StateId(8), StateId(2), delayedReady, 0, activate, resetGame);
    builder.transition(StateId(9), StateId(2), delayedReady, 0, resetRound);
    return builder.ok && static_cast<bool>(validateProfile(profile));
}
bool makeHintExampleProfile(Profile &profile) {
    if (!makeStandardProfile(profile))
        return false;
    profile.revision = 4;
    if (!profile.name.assign("Larp-Escape with hint"))
        return false;
    Effect effect;
    effect.id = EffectId(200);
    effect.name.assign("cyan hint");
    effect.component = ComponentId(6);
    effect.kind = EffectKind::Burst;
    effect.frames = 80;
    effect.colored = true;
    effect.colorRgb = 0x00FFFF;
    if (!profile.effects.push(effect))
        return false;
    Action action;
    action.id = ActionId(200);
    action.kind = ActionKind::ApplyEffect;
    action.effect = effect.id;
    if (!profile.actions.push(action))
        return false;
    State hint;
    hint.id = StateId(42); // Deliberately not a legacy GamePhase enum ordinal.
    hint.name.assign("hint");
    if (!hint.entry.push(action.id) || !profile.states.push(hint))
        return false;
    Condition timer;
    timer.id = ConditionId(200);
    timer.field = Field::StateElapsedMilliseconds;
    timer.comparison = Compare::GreaterEqual;
    timer.operand = 30000;
    if (!profile.conditions.push(timer))
        return false;
    Transition transition;
    transition.id = TransitionId(200);
    transition.source = StateId(2);
    transition.target = hint.id;
    transition.condition = timer.id;
    transition.priority = 3;
    if (!profile.transitions.push(transition))
        return false;
    timer.id = ConditionId(201);
    timer.operand = 5000;
    if (!profile.conditions.push(timer))
        return false;
    transition.id = TransitionId(201);
    transition.source = hint.id;
    transition.target = StateId(2);
    transition.condition = timer.id;
    transition.priority = 0;
    if (!profile.transitions.push(transition))
        return false;
    return static_cast<bool>(validateProfile(profile));
}
} // namespace configuration
} // namespace larp
