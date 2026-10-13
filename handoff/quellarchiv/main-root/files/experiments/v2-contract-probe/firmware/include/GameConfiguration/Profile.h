#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace larp {
namespace configuration {
// Document limits are compiled capabilities, not implicit defaults in an app.
// Storage is caller-owned. Do not put a Profile on a small FreeRTOS task stack.
struct Limits {
    static const std::size_t Components = 16;
    static const std::size_t States = 32;
    static const std::size_t Transitions = 64;
    static const std::size_t Conditions = 96;
    static const std::size_t Actions = 96;
    static const std::size_t Effects = 32;
    static const std::size_t Roles = 16;
    static const std::size_t CrystalsPerRole = 8;
    static const std::size_t StateActions = 8;
    static const std::size_t TransitionActions = 4;
    static const std::size_t ConditionDepth = 8;
    static const std::size_t MatrixLines = 9;
    static const std::size_t WakeInputs = 4;
    static const std::uint16_t Leds = 256;
    static const std::uint16_t SchemaVersion = 1;
};

template <typename T, std::size_t Capacity> class List {
  public:
    List() : values_(), count_(0) {}
    bool push(const T &value) {
        if (count_ == Capacity)
            return false;
        values_[count_++] = value;
        return true;
    }
    std::size_t size() const {
        return count_;
    }
    const T *get(std::size_t index) const {
        return index < count_ ? &values_[index] : nullptr;
    }
    T *get(std::size_t index) {
        return index < count_ ? &values_[index] : nullptr;
    }
    void clear() {
        count_ = 0;
    }

  private:
    std::array<T, Capacity> values_;
    std::size_t count_;
};

inline bool validUtf8(const char *text, std::size_t length) {
    for (std::size_t index = 0; index < length;) {
        const unsigned char first = static_cast<unsigned char>(text[index]);
        if (first < 128) {
            ++index;
            continue;
        }
        std::size_t width = first >= 0xC2 && first <= 0xDF   ? 2
                            : first >= 0xE0 && first <= 0xEF ? 3
                            : first >= 0xF0 && first <= 0xF4 ? 4
                                                             : 0;
        if (width == 0 || width > length - index)
            return false;
        for (std::size_t offset = 1; offset < width; ++offset) {
            const unsigned char next = static_cast<unsigned char>(text[index + offset]);
            if (next < 0x80 || next > 0xBF)
                return false;
        }
        const unsigned char second = static_cast<unsigned char>(text[index + 1]);
        if ((first == 0xE0 && second < 0xA0) || (first == 0xED && second >= 0xA0) ||
            (first == 0xF0 && second < 0x90) || (first == 0xF4 && second >= 0x90))
            return false;
        index += width;
    }
    return true;
}
template <std::size_t Capacity> class Text {
  public:
    Text() : value_() {}
    // Rejection leaves the previous value intact. Capacity excludes the terminator.
    bool assign(const char *text) {
        if (text == nullptr)
            return false;
        std::size_t length = 0;
        while (length <= Capacity && text[length] != '\0') {
            if (static_cast<unsigned char>(text[length]) < 32U)
                return false;
            ++length;
        }
        if (length > Capacity || !validUtf8(text, length))
            return false;
        std::array<char, Capacity + 1> copy = {};
        for (std::size_t index = 0; index < length; ++index)
            copy[index] = text[index];
        value_ = copy;
        return true;
    }
    bool empty() const {
        return value_[0] == '\0';
    }
    const char *c_str() const {
        return value_.data();
    }

  private:
    std::array<char, Capacity + 1> value_;
};

template <typename Tag> struct Id {
    std::uint16_t value;
    Id() : value(0) {}
    explicit Id(std::uint16_t number) : value(number) {}
    bool operator==(Id other) const {
        return value == other.value;
    }
    bool operator!=(Id other) const {
        return !(*this == other);
    }
};
struct ComponentTag {};
struct StateTag {};
struct ConditionTag {};
struct ActionTag {};
struct EffectTag {};
struct RoleTag {};
struct TransitionTag {};
using ComponentId = Id<ComponentTag>;
using StateId = Id<StateTag>;
using ConditionId = Id<ConditionTag>;
using ActionId = Id<ActionTag>;
using EffectId = Id<EffectTag>;
using RoleId = Id<RoleTag>;
using TransitionId = Id<TransitionTag>;
using Name = Text<32>;
static const std::uint8_t UnusedPin = 255;

enum class ExpanderMode : std::uint8_t { Unconfigured, InputPullup, Output, Input };
enum class Board : std::uint8_t { Esp32DevkitV4 };
enum class ComponentKind : std::uint8_t { NfcSpi, IoExpander, Matrix, AudioUart, LedStrip, EspNow };
struct Endpoint {
    ComponentId expander;
    std::uint8_t pin = UnusedPin;
};
struct Component {
    ComponentId id;
    Name name;
    ComponentKind kind = ComponentKind::NfcSpi;
    bool enabled = true;
    // SPI: SCK/MISO/MOSI/SS; I2C: SDA/SCL; audio: RX/TX/BUSY/WAKE; LED: DATA.
    std::array<std::uint8_t, 4> pins = {{UnusedPin, UnusedPin, UnusedPin, UnusedPin}};
    std::uint8_t bus = 0;
    std::uint8_t address = 0;
    std::uint16_t ledCount = 0;
    std::uint16_t powerLimitMilliamperes = 1000;
    std::uint8_t volume = 27;
    std::uint32_t baud = 9600;
    std::uint8_t initialBrightness = 100;
    std::uint8_t initialTwinkleDensity = 1;
    std::uint8_t initialTwinkleSpeed = 1;
    std::array<ExpanderMode, 16> startupModes = {};
    std::array<std::uint8_t, 6> peer = {};
    std::uint8_t channel = 0;
    List<Endpoint, Limits::MatrixLines> rows;
    List<Endpoint, Limits::MatrixLines> columns;
    List<Endpoint, Limits::WakeInputs> wakeInputs;
};

enum class CrystalCategory : std::uint8_t { Animal, Item, Color };
using Palette = std::array<std::uint32_t, 16>;
struct Crystal {
    Name name;
    std::uint8_t row = 0;
    std::uint8_t column = 0; // 0..8, logical A..I
    std::uint16_t audioTrack = 0;
    CrystalCategory category = CrystalCategory::Animal;
    bool changesPalette = false;
    Palette palette = {};
};
struct Role {
    RoleId id;
    Name name;
    std::array<std::uint8_t, 7> uid = {};
    std::uint16_t audioTrack = 0;
    List<Crystal, Limits::CrystalsPerRole> crystals;
};

// Inputs are sampled by the owning application, never inside a predicate.
// Event latches and consumption are an explicit part of the execution contract.
enum class Field : std::uint8_t {
    AudioBusy,
    RoleAccepted,
    CrystalAvailable,
    CrystalAudioPending,
    PuzzleComplete,
    IncorrectCrystal,
    KeystoneError,
    AllWakeActive,
    SeenRoleCount,
    StateElapsedMilliseconds,
    TransitionElapsedMilliseconds,
    RoundCount
};
enum class Compare : std::uint8_t { Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual };
// Wait emits one match after its interval and restarts on the next observation.
// It has independent timing per transition; at most one distinct Wait per guard.
enum class ConditionKind : std::uint8_t { Predicate, All, Any, Not, Wait };
struct Condition {
    ConditionId id;
    ComponentId component; // Source for a hardware-dependent field.
    ConditionKind kind = ConditionKind::Predicate;
    Field field = Field::AudioBusy;
    Compare comparison = Compare::Equal;
    std::uint32_t operand = 0;
    ConditionId first;
    ConditionId second;
};

enum class EffectKind : std::uint8_t { Twinkle, Burst, Ripple, BrightnessRamp, Palette };
struct Effect {
    EffectId id;
    Name name;
    ComponentId component;
    EffectKind kind = EffectKind::Twinkle;
    std::uint8_t density = 0;
    bool changesSpeed = false;
    std::uint8_t speed = 4;
    std::uint8_t brightness = 0;
    std::uint16_t intervalMilliseconds = 20;
    std::uint8_t frames = 15;
    bool colored = false;
    std::uint32_t colorRgb = 0;
    std::uint8_t rippleIndex = 0;
    std::uint16_t origin = 0;
    Palette palette = {};
};
enum class ActionKind : std::uint8_t {
    ApplyEffect,
    PlayTrack,
    PlayRoleTrack,
    PlayCrystalTrack,
    WakeAudio,
    ResetRound,
    ResetGame,
    ResetCardRetry,
    CompleteRound,
    SendActivation,
    ConsumeCrystal,
    ConsumeError,
    ReadRole,
    ReadCrystals,
    ObserveKeystone,
    AnimateWakeInputs
};
struct Action {
    ActionId id;
    ActionKind kind = ActionKind::ResetRound;
    ComponentId source; // Matrix source for AnimateWakeInputs.
    ComponentId component;
    EffectId effect;         // ApplyEffect, or optional Palette binding for ReadCrystals.
    std::uint32_t value = 0; // track number, crystal index, or polling interval
};
struct State {
    StateId id;
    Name name;
    bool manualAllowed = true;
    List<ActionId, Limits::StateActions> entry;
    List<ActionId, Limits::StateActions> tick;
    List<ActionId, Limits::StateActions> exit;
};
struct Transition {
    TransitionId id;
    StateId source;
    StateId target;
    ConditionId condition;
    std::uint16_t priority = 0; // Smaller first; unique for each source state.
    bool manualAllowed = false;
    List<ActionId, Limits::TransitionActions> actions;
};
struct InputPolicy {
    std::uint16_t cardTimeoutMilliseconds = 100;
    std::uint8_t emptyReadsBeforeClear = 5;
    std::uint8_t keystoneFailureThreshold = 5;
    // Preserve the characterized standard game until its product contract changes.
    bool consecutiveEmptyReads = false;
    std::array<std::uint16_t, Limits::WakeInputs> wakeRippleOrigins = {{22, 2, 16, 10}};
    std::uint8_t wakeRippleFrames = 60;
};
struct Profile {
    std::uint16_t schemaVersion = Limits::SchemaVersion;
    std::uint32_t revision = 0;
    Name name;
    Board board = Board::Esp32DevkitV4;
    StateId initialState;
    StateId errorState;
    InputPolicy inputs;
    List<Component, Limits::Components> components;
    List<Role, Limits::Roles> roles;
    List<State, Limits::States> states;
    List<Transition, Limits::Transitions> transitions;
    List<Condition, Limits::Conditions> conditions;
    List<Action, Limits::Actions> actions;
    List<Effect, Limits::Effects> effects;
};

enum class Section : std::uint8_t {
    Profile,
    Components,
    Roles,
    States,
    Transitions,
    Conditions,
    Actions,
    Effects
};
enum class Error : std::uint8_t {
    None,
    UnsupportedSchema,
    UnsupportedBoard,
    MissingName,
    EmptyGraph,
    InvalidId,
    DuplicateId,
    InvalidKind,
    InvalidPin,
    PinConflict,
    InvalidParameter,
    MissingReference,
    DisabledComponent,
    WrongComponentKind,
    DuplicateAddress,
    MatrixPinConflict,
    InvalidUid,
    DuplicateUid,
    InvalidCrystal,
    DuplicateCrystal,
    InvalidExpression,
    ConditionCycle,
    ConditionDepth,
    DuplicatePriority
};
struct ValidationResult {
    Error error = Error::None;
    Section section = Section::Profile;
    std::uint16_t object = 0;
    std::uint16_t detail = 0;
    explicit operator bool() const {
        return error == Error::None;
    }
};
ValidationResult validateProfile(const Profile &profile);
// Creates a data representation, not a live controller or hardware setup.
// Returns false if the compiled capacities cannot hold the complete standard profile.
bool makeStandardProfile(Profile &destination);
// Demonstrates an added state, edited time condition and colored effect as data.
bool makeHintExampleProfile(Profile &destination);
} // namespace configuration
} // namespace larp
