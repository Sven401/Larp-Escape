#pragma once
#include "GameConfiguration/Profile.h"

namespace larp {
namespace configuration {
namespace vocabulary {
// Stable JSON spellings shared by reader and writer; enum order is part of v1.
static const char *const BoardNames[] = {"esp32_devkit_v4"};
static const char *const ComponentKindNames[] = {"nfc_spi",    "io_expander", "matrix",
                                                 "audio_uart", "led_strip",   "esp_now"};
static const char *const CrystalCategoryNames[] = {"animal", "item", "color"};
static const char *const FieldNames[] = {
    "audio_busy",      "role_accepted",     "crystal_available",     "crystal_audio_pending",
    "puzzle_complete", "incorrect_crystal", "keystone_error",        "all_wake_active",
    "seen_role_count", "state_elapsed_ms",  "transition_elapsed_ms", "round_count"};
static const char *const CompareNames[] = {"eq", "ne", "lt", "le", "gt", "ge"};
static const char *const ConditionKindNames[] = {"predicate", "all", "any", "not", "wait"};
static const char *const ActionKindNames[] = {
    "apply_effect",   "play_track",      "play_role_track",  "play_crystal_track",
    "wake_audio",     "reset_round",     "reset_game",       "reset_card_retry",
    "complete_round", "send_activation", "consume_crystal",  "consume_error",
    "read_role",      "read_crystals",   "observe_keystone", "animate_wake_inputs"};
static const char *const EffectKindNames[] = {"twinkle", "burst", "ripple", "brightness_ramp",
                                              "palette"};

// Extending an enum requires an explicit wire spelling before it can be serialized.
static_assert(sizeof(BoardNames) / sizeof(BoardNames[0]) ==
                  static_cast<unsigned>(Board::Esp32DevkitV4) + 1,
              "Board JSON vocabulary must match enum");
static_assert(sizeof(ComponentKindNames) / sizeof(ComponentKindNames[0]) ==
                  static_cast<unsigned>(ComponentKind::EspNow) + 1,
              "ComponentKind JSON vocabulary must match enum");
static_assert(sizeof(CrystalCategoryNames) / sizeof(CrystalCategoryNames[0]) ==
                  static_cast<unsigned>(CrystalCategory::Color) + 1,
              "CrystalCategory JSON vocabulary must match enum");
static_assert(sizeof(FieldNames) / sizeof(FieldNames[0]) ==
                  static_cast<unsigned>(Field::RoundCount) + 1,
              "Field JSON vocabulary must match enum");
static_assert(sizeof(CompareNames) / sizeof(CompareNames[0]) ==
                  static_cast<unsigned>(Compare::GreaterEqual) + 1,
              "Compare JSON vocabulary must match enum");
static_assert(sizeof(ConditionKindNames) / sizeof(ConditionKindNames[0]) ==
                  static_cast<unsigned>(ConditionKind::Wait) + 1,
              "ConditionKind JSON vocabulary must match enum");
static_assert(sizeof(ActionKindNames) / sizeof(ActionKindNames[0]) ==
                  static_cast<unsigned>(ActionKind::AnimateWakeInputs) + 1,
              "ActionKind JSON vocabulary must match enum");
static_assert(sizeof(EffectKindNames) / sizeof(EffectKindNames[0]) ==
                  static_cast<unsigned>(EffectKind::Palette) + 1,
              "EffectKind JSON vocabulary must match enum");
} // namespace vocabulary
} // namespace configuration
} // namespace larp
