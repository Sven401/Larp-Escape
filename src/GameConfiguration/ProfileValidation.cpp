#include "GameConfiguration/Profile.h"

namespace larp {
namespace configuration {
namespace {
ValidationResult fail(Error error, Section section, std::uint16_t object = 0,
                      std::uint16_t detail = 0) {
    ValidationResult result;
    result.error = error;
    result.section = section;
    result.object = object;
    result.detail = detail;
    return result;
}
template <typename T, std::size_t N, typename Key> const T *find(const List<T, N> &items, Key id) {
    for (std::size_t index = 0; index < items.size(); ++index)
        if (items.get(index)->id == id)
            return items.get(index);
    return nullptr;
}
template <typename T, std::size_t N>
ValidationResult uniqueIds(const List<T, N> &items, Section section) {
    for (std::size_t index = 0; index < items.size(); ++index) {
        const std::uint16_t id = items.get(index)->id.value;
        if (id == 0)
            return fail(Error::InvalidId, section);
        for (std::size_t previous = 0; previous < index; ++previous)
            if (items.get(previous)->id.value == id)
                return fail(Error::DuplicateId, section, id);
    }
    return {};
}
ValidationResult componentReference(const Profile &profile, ComponentId id, ComponentKind kind,
                                    Section section, std::uint16_t owner) {
    const Component *component = find(profile.components, id);
    if (component == nullptr)
        return fail(Error::MissingReference, section, owner, id.value);
    if (!component->enabled)
        return fail(Error::DisabledComponent, section, owner, id.value);
    if (component->kind != kind)
        return fail(Error::WrongComponentKind, section, owner, id.value);
    return {};
}
bool boardPin(std::uint8_t pin, bool output) {
    // Classic ESP32 devkit: GPIO 6..11 are flash; 1/3 remain the USB diagnostics UART.
    const bool available = pin == 0 || pin == 2 || pin == 4 || pin == 5 ||
                           (pin >= 12 && pin <= 19) || (pin >= 21 && pin <= 23) ||
                           (pin >= 25 && pin <= 27) || (pin >= 32 && pin <= 39);
    return available && (!output || pin < 34);
}
std::size_t pinCount(ComponentKind kind) {
    switch (kind) {
    case ComponentKind::NfcSpi:
        return 4;
    case ComponentKind::IoExpander:
        return 2;
    case ComponentKind::AudioUart:
        return 4;
    case ComponentKind::LedStrip:
        return 1;
    default:
        return 0;
    }
}
bool outputPin(ComponentKind kind, std::size_t index) {
    if (kind == ComponentKind::NfcSpi)
        return index != 1;
    if (kind == ComponentKind::AudioUart)
        return index == 1 || index == 3;
    return true;
}
ValidationResult validateComponents(const Profile &profile) {
    for (std::size_t index = 0; index < profile.components.size(); ++index) {
        const Component &item = *profile.components.get(index);
        if (item.name.empty())
            return fail(Error::MissingName, Section::Components, item.id.value);
        if (static_cast<unsigned>(item.kind) > static_cast<unsigned>(ComponentKind::EspNow))
            return fail(Error::InvalidKind, Section::Components, item.id.value);
        for (ExpanderMode mode : item.startupModes)
            if (static_cast<unsigned>(mode) > static_cast<unsigned>(ExpanderMode::Input))
                return fail(Error::InvalidParameter, Section::Components, item.id.value);
        if (!item.enabled)
            continue;
        const std::size_t used = pinCount(item.kind);
        for (std::size_t pin = 0; pin < item.pins.size(); ++pin) {
            if (pin >= used) {
                if (item.pins[pin] != UnusedPin)
                    return fail(Error::InvalidPin, Section::Components, item.id.value,
                                item.pins[pin]);
                continue;
            }
            if (!boardPin(item.pins[pin], outputPin(item.kind, pin)))
                return fail(Error::InvalidPin, Section::Components, item.id.value, item.pins[pin]);
            for (std::size_t other = 0; other < pin; ++other)
                if (item.pins[other] == item.pins[pin])
                    return fail(Error::PinConflict, Section::Components, item.id.value,
                                item.pins[pin]);
            for (std::size_t previous = 0; previous < index; ++previous) {
                const Component &old = *profile.components.get(previous);
                if (!old.enabled)
                    continue;
                for (std::size_t oldPin = 0; oldPin < pinCount(old.kind); ++oldPin) {
                    if (old.pins[oldPin] != item.pins[pin])
                        continue;
                    const bool sharedI2c = item.kind == ComponentKind::IoExpander &&
                                           old.kind == item.kind && old.bus == item.bus &&
                                           oldPin == pin && old.pins[0] == item.pins[0] &&
                                           old.pins[1] == item.pins[1];
                    if (!sharedI2c)
                        return fail(Error::PinConflict, Section::Components, item.id.value,
                                    item.pins[pin]);
                }
                if (item.kind == ComponentKind::IoExpander && old.kind == item.kind &&
                    old.bus == item.bus) {
                    if (old.pins[0] != item.pins[0] || old.pins[1] != item.pins[1])
                        return fail(Error::PinConflict, Section::Components, item.id.value,
                                    old.id.value);
                    if (old.address == item.address)
                        return fail(Error::DuplicateAddress, Section::Components, item.id.value,
                                    old.id.value);
                }
                if (item.kind == ComponentKind::AudioUart && old.kind == item.kind &&
                    old.bus == item.bus)
                    return fail(Error::InvalidParameter, Section::Components, item.id.value,
                                old.id.value);
            }
        }
        if (item.kind == ComponentKind::IoExpander) {
            if (item.bus > 1 || item.address < 0x20 || item.address > 0x27)
                return fail(Error::InvalidParameter, Section::Components, item.id.value);
            for (ExpanderMode mode : item.startupModes)
                if (static_cast<unsigned>(mode) > static_cast<unsigned>(ExpanderMode::Input))
                    return fail(Error::InvalidParameter, Section::Components, item.id.value);
        } else if (item.kind == ComponentKind::AudioUart) {
            if (item.bus == 0 || item.bus > 2 || item.volume > 30 || item.baud != 9600)
                return fail(Error::InvalidParameter, Section::Components, item.id.value);
        } else if (item.kind == ComponentKind::LedStrip) {
            if (item.ledCount == 0 || item.ledCount > Limits::Leds ||
                item.powerLimitMilliamperes == 0 || item.initialTwinkleDensity > 8 ||
                item.initialTwinkleSpeed > 8)
                return fail(Error::InvalidParameter, Section::Components, item.id.value);
        } else if (item.kind == ComponentKind::EspNow) {
            bool nonzero = false;
            for (std::uint8_t byte : item.peer)
                nonzero = nonzero || byte != 0;
            if (!nonzero || (item.peer[0] & 1U) != 0 || item.channel > 14)
                return fail(Error::InvalidParameter, Section::Components, item.id.value);
        } else if (item.kind == ComponentKind::Matrix) {
            if (item.rows.size() == 0 || item.columns.size() == 0)
                return fail(Error::InvalidParameter, Section::Components, item.id.value);
            std::array<Endpoint, 2 * Limits::MatrixLines + Limits::WakeInputs> usedEndpoints = {};
            std::size_t endpointCount = 0;
            for (std::size_t group = 0; group < 3; ++group) {
                const std::size_t count = group == 0   ? item.rows.size()
                                          : group == 1 ? item.columns.size()
                                                       : item.wakeInputs.size();
                for (std::size_t position = 0; position < count; ++position) {
                    const Endpoint &endpoint = *(group == 0   ? item.rows.get(position)
                                                 : group == 1 ? item.columns.get(position)
                                                              : item.wakeInputs.get(position));
                    const ValidationResult reference =
                        componentReference(profile, endpoint.expander, ComponentKind::IoExpander,
                                           Section::Components, item.id.value);
                    if (!reference)
                        return reference;
                    if (endpoint.pin >= 16)
                        return fail(Error::InvalidPin, Section::Components, item.id.value,
                                    endpoint.pin);
                    for (std::size_t previous = 0; previous < endpointCount; ++previous)
                        if (usedEndpoints[previous].expander == endpoint.expander &&
                            usedEndpoints[previous].pin == endpoint.pin)
                            return fail(Error::MatrixPinConflict, Section::Components,
                                        item.id.value, endpoint.pin);
                    // Two enabled matrices may not independently write/read the same expander pin.
                    for (std::size_t previous = 0; previous < index; ++previous) {
                        const Component &old = *profile.components.get(previous);
                        if (!old.enabled || old.kind != ComponentKind::Matrix)
                            continue;
                        for (std::size_t oldGroup = 0; oldGroup < 3; ++oldGroup) {
                            const std::size_t oldCount = oldGroup == 0   ? old.rows.size()
                                                         : oldGroup == 1 ? old.columns.size()
                                                                         : old.wakeInputs.size();
                            for (std::size_t oldIndex = 0; oldIndex < oldCount; ++oldIndex) {
                                const Endpoint &other =
                                    *(oldGroup == 0   ? old.rows.get(oldIndex)
                                      : oldGroup == 1 ? old.columns.get(oldIndex)
                                                      : old.wakeInputs.get(oldIndex));
                                if (other.expander == endpoint.expander &&
                                    other.pin == endpoint.pin)
                                    return fail(Error::MatrixPinConflict, Section::Components,
                                                item.id.value, endpoint.pin);
                            }
                        }
                    }
                    usedEndpoints[endpointCount++] = endpoint;
                }
            }
        }
    }
    return {};
}
bool validPalette(const Palette &palette) {
    for (std::uint32_t rgb : palette)
        if (rgb > 0xFFFFFFU)
            return false;
    return true;
}
ValidationResult validateRoles(const Profile &profile) {
    for (std::size_t index = 0; index < profile.roles.size(); ++index) {
        const Role &role = *profile.roles.get(index);
        if (role.name.empty())
            return fail(Error::MissingName, Section::Roles, role.id.value);
        bool nonzero = false;
        for (std::uint8_t byte : role.uid)
            nonzero = nonzero || byte != 0;
        if (!nonzero)
            return fail(Error::InvalidUid, Section::Roles, role.id.value);
        for (std::size_t previous = 0; previous < index; ++previous)
            if (profile.roles.get(previous)->uid == role.uid)
                return fail(Error::DuplicateUid, Section::Roles, role.id.value);
        if (role.audioTrack == 0 || role.crystals.size() == 0)
            return fail(Error::InvalidParameter, Section::Roles, role.id.value);
        for (std::size_t position = 0; position < role.crystals.size(); ++position) {
            const Crystal &crystal = *role.crystals.get(position);
            if (crystal.name.empty() || crystal.row == 0 || crystal.row > 9 || crystal.column > 8 ||
                crystal.audioTrack == 0 || static_cast<unsigned>(crystal.category) > 2 ||
                !validPalette(crystal.palette))
                return fail(Error::InvalidCrystal, Section::Roles, role.id.value,
                            static_cast<std::uint16_t>(position));
            for (std::size_t previous = 0; previous < position; ++previous) {
                const Crystal &other = *role.crystals.get(previous);
                if (crystal.row == other.row && crystal.column == other.column)
                    return fail(Error::DuplicateCrystal, Section::Roles, role.id.value,
                                static_cast<std::uint16_t>(position));
            }
        }
    }
    return {};
}
ValidationResult expression(const Profile &profile, ConditionId id,
                            std::array<ConditionId, Limits::ConditionDepth> &path,
                            std::size_t depth) {
    const Condition *node = find(profile.conditions, id);
    if (node == nullptr)
        return fail(Error::MissingReference, Section::Conditions, depth ? path[depth - 1].value : 0,
                    id.value);
    for (std::size_t previous = 0; previous < depth; ++previous)
        if (path[previous] == id)
            return fail(Error::ConditionCycle, Section::Conditions, id.value);
    if (depth == path.size())
        return fail(Error::ConditionDepth, Section::Conditions, id.value);
    path[depth] = id;
    if (node->kind == ConditionKind::Predicate || node->kind == ConditionKind::Wait)
        return {};
    ValidationResult result = expression(profile, node->first, path, depth + 1);
    if (!result || node->kind == ConditionKind::Not)
        return result;
    return expression(profile, node->second, path, depth + 1);
}
ValidationResult validateConditions(const Profile &profile) {
    for (std::size_t index = 0; index < profile.conditions.size(); ++index) {
        const Condition &node = *profile.conditions.get(index);
        if (static_cast<unsigned>(node.kind) > static_cast<unsigned>(ConditionKind::Wait))
            return fail(Error::InvalidKind, Section::Conditions, node.id.value);
        if (node.kind == ConditionKind::Wait) {
            if (node.operand == 0 || node.operand > 0x7FFFFFFFU || node.first.value != 0 ||
                node.second.value != 0 || node.component.value != 0 ||
                node.field != Field::AudioBusy || node.comparison != Compare::Equal)
                return fail(Error::InvalidExpression, Section::Conditions, node.id.value);
        } else if (node.kind == ConditionKind::Predicate) {
            if (node.first.value != 0 || node.second.value != 0 ||
                static_cast<unsigned>(node.field) > static_cast<unsigned>(Field::RoundCount) ||
                static_cast<unsigned>(node.comparison) >
                    static_cast<unsigned>(Compare::GreaterEqual))
                return fail(Error::InvalidExpression, Section::Conditions, node.id.value);
            const bool boolean =
                static_cast<unsigned>(node.field) <= static_cast<unsigned>(Field::AllWakeActive);
            if (boolean && (node.operand > 1 || (node.comparison != Compare::Equal &&
                                                 node.comparison != Compare::NotEqual)))
                return fail(Error::InvalidExpression, Section::Conditions, node.id.value);
            if ((node.field == Field::StateElapsedMilliseconds ||
                 node.field == Field::TransitionElapsedMilliseconds) &&
                node.operand > 0x7FFFFFFFU)
                return fail(Error::InvalidParameter, Section::Conditions, node.id.value);
            bool requiresDevice = true;
            ComponentKind kind = ComponentKind::Matrix;
            switch (node.field) {
            case Field::AudioBusy:
                kind = ComponentKind::AudioUart;
                break;
            case Field::RoleAccepted:
            case Field::KeystoneError:
                kind = ComponentKind::NfcSpi;
                break;
            case Field::CrystalAvailable:
            case Field::IncorrectCrystal:
            case Field::AllWakeActive:
                break;
            default:
                requiresDevice = false;
                break;
            }
            if (requiresDevice) {
                const ValidationResult reference = componentReference(
                    profile, node.component, kind, Section::Conditions, node.id.value);
                if (!reference)
                    return reference;
                if (node.field == Field::AllWakeActive &&
                    find(profile.components, node.component)->wakeInputs.size() == 0)
                    return fail(Error::InvalidParameter, Section::Conditions, node.id.value);
                if (node.field == Field::RoleAccepted && profile.roles.size() == 0)
                    return fail(Error::InvalidParameter, Section::Conditions, node.id.value);
            } else if (node.component.value != 0) {
                return fail(Error::InvalidExpression, Section::Conditions, node.id.value);
            }
        } else if (node.first.value == 0 ||
                   (node.kind == ConditionKind::Not ? node.second.value != 0
                                                    : node.second.value == 0) ||
                   node.component.value != 0) {
            return fail(Error::InvalidExpression, Section::Conditions, node.id.value);
        }
        std::array<ConditionId, Limits::ConditionDepth> path = {};
        const ValidationResult result = expression(profile, node.id, path, 0);
        if (!result)
            return result;
    }
    return {};
}
ValidationResult validateEffects(const Profile &profile) {
    for (std::size_t index = 0; index < profile.effects.size(); ++index) {
        const Effect &effect = *profile.effects.get(index);
        if (effect.name.empty())
            return fail(Error::MissingName, Section::Effects, effect.id.value);
        if (static_cast<unsigned>(effect.kind) > static_cast<unsigned>(EffectKind::Palette))
            return fail(Error::InvalidKind, Section::Effects, effect.id.value);
        const ValidationResult reference = componentReference(
            profile, effect.component, ComponentKind::LedStrip, Section::Effects, effect.id.value);
        if (!reference)
            return reference;
        const Component &led = *find(profile.components, effect.component);
        if (effect.density > 8 || effect.speed > 8 || effect.colorRgb > 0xFFFFFFU ||
            !validPalette(effect.palette) ||
            (effect.kind == EffectKind::BrightnessRamp && effect.intervalMilliseconds == 0) ||
            ((effect.kind == EffectKind::Burst || effect.kind == EffectKind::Ripple) &&
             effect.frames == 0) ||
            (effect.kind == EffectKind::Ripple &&
             (effect.origin >= led.ledCount || effect.rippleIndex >= Limits::WakeInputs)))
            return fail(Error::InvalidParameter, Section::Effects, effect.id.value);
    }
    return {};
}
ValidationResult validateActions(const Profile &profile) {
    for (std::size_t index = 0; index < profile.actions.size(); ++index) {
        const Action &action = *profile.actions.get(index);
        if (static_cast<unsigned>(action.kind) >
            static_cast<unsigned>(ActionKind::AnimateWakeInputs))
            return fail(Error::InvalidKind, Section::Actions, action.id.value);
        if (action.kind == ActionKind::ApplyEffect) {
            if (find(profile.effects, action.effect) == nullptr)
                return fail(Error::MissingReference, Section::Actions, action.id.value,
                            action.effect.value);
            if (action.component.value != 0 || action.source.value != 0 || action.value != 0)
                return fail(Error::InvalidParameter, Section::Actions, action.id.value);
            continue;
        }
        if (action.effect.value != 0)
            return fail(Error::InvalidParameter, Section::Actions, action.id.value);
        bool device = true;
        ComponentKind kind = ComponentKind::AudioUart;
        switch (action.kind) {
        case ActionKind::ReadRole:
        case ActionKind::ObserveKeystone:
            kind = ComponentKind::NfcSpi;
            break;
        case ActionKind::ReadCrystals:
            kind = ComponentKind::Matrix;
            break;
        case ActionKind::SendActivation:
            kind = ComponentKind::EspNow;
            break;
        case ActionKind::AnimateWakeInputs:
            kind = ComponentKind::LedStrip;
            break;
        case ActionKind::PlayTrack:
        case ActionKind::PlayRoleTrack:
        case ActionKind::PlayCrystalTrack:
        case ActionKind::WakeAudio:
            break;
        default:
            device = false;
            break;
        }
        if (device) {
            const ValidationResult reference = componentReference(
                profile, action.component, kind, Section::Actions, action.id.value);
            if (!reference)
                return reference;
        } else if (action.component.value != 0) {
            return fail(Error::InvalidParameter, Section::Actions, action.id.value);
        }
        if (action.kind == ActionKind::AnimateWakeInputs) {
            const ValidationResult reference = componentReference(
                profile, action.source, ComponentKind::Matrix, Section::Actions, action.id.value);
            if (!reference)
                return reference;
            const Component &matrix = *find(profile.components, action.source);
            const Component &led = *find(profile.components, action.component);
            if (matrix.wakeInputs.size() == 0 || profile.inputs.wakeRippleFrames == 0)
                return fail(Error::InvalidParameter, Section::Actions, action.id.value);
            for (std::size_t wake = 0; wake < matrix.wakeInputs.size(); ++wake)
                if (profile.inputs.wakeRippleOrigins[wake] >= led.ledCount)
                    return fail(Error::InvalidParameter, Section::Actions, action.id.value);
        } else if (action.source.value != 0) {
            return fail(Error::InvalidParameter, Section::Actions, action.id.value);
        }
        const bool parameterized =
            action.kind == ActionKind::PlayTrack || action.kind == ActionKind::PlayCrystalTrack ||
            action.kind == ActionKind::ReadRole || action.kind == ActionKind::ReadCrystals ||
            action.kind == ActionKind::ObserveKeystone;
        if (!parameterized && action.value != 0)
            return fail(Error::InvalidParameter, Section::Actions, action.id.value);
        if (action.kind == ActionKind::ReadRole && profile.roles.size() == 0)
            return fail(Error::InvalidParameter, Section::Actions, action.id.value);
        if (action.kind == ActionKind::ReadCrystals) {
            const Component &matrix = *find(profile.components, action.component);
            for (std::size_t roleIndex = 0; roleIndex < profile.roles.size(); ++roleIndex) {
                const Role &role = *profile.roles.get(roleIndex);
                for (std::size_t crystalIndex = 0; crystalIndex < role.crystals.size();
                     ++crystalIndex) {
                    const Crystal &crystal = *role.crystals.get(crystalIndex);
                    if (crystal.row > matrix.rows.size() || crystal.column >= matrix.columns.size())
                        return fail(Error::InvalidCrystal, Section::Actions, action.id.value,
                                    role.id.value);
                }
            }
        }
        if (action.kind == ActionKind::PlayTrack && (action.value == 0 || action.value > 65535))
            return fail(Error::InvalidParameter, Section::Actions, action.id.value);
        if (action.kind == ActionKind::PlayCrystalTrack && action.value >= Limits::CrystalsPerRole)
            return fail(Error::InvalidParameter, Section::Actions, action.id.value);
        if ((action.kind == ActionKind::ReadRole || action.kind == ActionKind::ReadCrystals ||
             action.kind == ActionKind::ObserveKeystone) &&
            (action.value == 0 || action.value > 0x7FFFFFFFU))
            return fail(Error::InvalidParameter, Section::Actions, action.id.value);
    }
    return {};
}
template <std::size_t N>
ValidationResult actionReferences(const Profile &profile, const List<ActionId, N> &ids,
                                  Section section, std::uint16_t owner) {
    for (std::size_t index = 0; index < ids.size(); ++index)
        if (find(profile.actions, *ids.get(index)) == nullptr)
            return fail(Error::MissingReference, section, owner, ids.get(index)->value);
    return {};
}
// Conditions are already validated/acyclic. Sharing a Wait node is permitted,
// but two independent waits in one guard would require two clocks per transition.
ValidationResult singleWait(const Profile &profile, ConditionId id, ConditionId &found,
                            std::uint16_t transition, std::size_t depth = 0) {
    const Condition *node = find(profile.conditions, id);
    if (node == nullptr || depth == Limits::ConditionDepth)
        return fail(Error::InvalidExpression, Section::Transitions, transition, id.value);
    if (node->kind == ConditionKind::Wait) {
        if (found.value != 0 && found != id)
            return fail(Error::InvalidExpression, Section::Transitions, transition, id.value);
        found = id;
        return {};
    }
    if (node->kind == ConditionKind::Predicate)
        return {};
    ValidationResult result = singleWait(profile, node->first, found, transition, depth + 1);
    if (!result || node->kind == ConditionKind::Not)
        return result;
    return singleWait(profile, node->second, found, transition, depth + 1);
}
ValidationResult validateGraph(const Profile &profile) {
    if (profile.states.size() == 0)
        return fail(Error::EmptyGraph, Section::Profile);
    if (find(profile.states, profile.initialState) == nullptr ||
        find(profile.states, profile.errorState) == nullptr)
        return fail(Error::MissingReference, Section::Profile);
    for (std::size_t index = 0; index < profile.states.size(); ++index) {
        const State &state = *profile.states.get(index);
        if (state.name.empty())
            return fail(Error::MissingName, Section::States, state.id.value);
        ValidationResult result =
            actionReferences(profile, state.entry, Section::States, state.id.value);
        if (!result)
            return result;
        result = actionReferences(profile, state.tick, Section::States, state.id.value);
        if (!result)
            return result;
        result = actionReferences(profile, state.exit, Section::States, state.id.value);
        if (!result)
            return result;
    }
    for (std::size_t index = 0; index < profile.transitions.size(); ++index) {
        const Transition &transition = *profile.transitions.get(index);
        if (find(profile.states, transition.source) == nullptr ||
            find(profile.states, transition.target) == nullptr ||
            find(profile.conditions, transition.condition) == nullptr)
            return fail(Error::MissingReference, Section::Transitions, transition.id.value);
        ConditionId wait;
        const ValidationResult waitResult =
            singleWait(profile, transition.condition, wait, transition.id.value);
        if (!waitResult)
            return waitResult;
        for (std::size_t previous = 0; previous < index; ++previous) {
            const Transition &other = *profile.transitions.get(previous);
            if (other.source == transition.source && other.priority == transition.priority)
                return fail(Error::DuplicatePriority, Section::Transitions, transition.id.value,
                            other.id.value);
        }
        const ValidationResult result = actionReferences(profile, transition.actions,
                                                         Section::Transitions, transition.id.value);
        if (!result)
            return result;
    }
    return {};
}
} // namespace

ValidationResult validateProfile(const Profile &profile) {
    if (profile.schemaVersion != Limits::SchemaVersion)
        return fail(Error::UnsupportedSchema, Section::Profile);
    if (profile.board != Board::Esp32DevkitV4)
        return fail(Error::UnsupportedBoard, Section::Profile);
    if (profile.name.empty())
        return fail(Error::MissingName, Section::Profile);
    if (profile.inputs.cardTimeoutMilliseconds == 0 || profile.inputs.emptyReadsBeforeClear == 0 ||
        profile.inputs.keystoneFailureThreshold == 0)
        return fail(Error::InvalidParameter, Section::Profile);
    const ValidationResult ids[] = {uniqueIds(profile.components, Section::Components),
                                    uniqueIds(profile.roles, Section::Roles),
                                    uniqueIds(profile.states, Section::States),
                                    uniqueIds(profile.transitions, Section::Transitions),
                                    uniqueIds(profile.conditions, Section::Conditions),
                                    uniqueIds(profile.actions, Section::Actions),
                                    uniqueIds(profile.effects, Section::Effects)};
    for (const ValidationResult &result : ids)
        if (!result)
            return result;
    ValidationResult result = validateComponents(profile);
    if (!result)
        return result;
    result = validateRoles(profile);
    if (!result)
        return result;
    result = validateConditions(profile);
    if (!result)
        return result;
    result = validateEffects(profile);
    if (!result)
        return result;
    result = validateActions(profile);
    if (!result)
        return result;
    return validateGraph(profile);
}
} // namespace configuration
} // namespace larp
